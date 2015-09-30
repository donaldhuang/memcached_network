#include<unistd.h> //getopt
#include<stdlib.h> //atoi
#include<event.h> //libevent
#include<stdio.h>
#include<pthread.h>
#include<string.h> //strerror
#include<errno.h>
#include<fcntl.h> //open
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string>
#include "WorkerThreads.h"
#include "cq.h"
#define RECVBUF_LEN 10240
#define MFP_FLAG_READ 0x1
#define MFP_FLAG_WRITE 0x2
#define MFP_FLAG_ERROR 0x4
#define MAX_EVENTS 16
#define DATA_BUFFER_SIZE 2048
using namespace std;
struct Settings
{
    int num_threads;	
};
struct Settings settings;

static struct event_base *main_base;					//main thread dispatch event_base
static int last_thread=-1;
WorkerThreads *workerThreads;


static void drive_machine(conn *c);
void dispatch_conn_new(int sfd, enum conn_states init_state, int event_flags,int read_buffer_size, enum network_transport transport);
static void drive_machine(conn *c) {
    bool stop = false;
    int sfd, flags = 1;
    socklen_t addrlen;
    struct sockaddr_storage addr;
    int res;
    const char *str;


    while (!stop) {
        switch(c->state) {
            case conn_listening:
                addrlen = sizeof(addr);
                if ((sfd = accept(c->sfd, (struct sockaddr *)&addr, &addrlen)) == -1) {
                    printf("accept error 12:%d\n",sfd);
                    stop = true;
                    break;
                }
                if ((flags = fcntl(sfd, F_GETFL, 0)) < 0 ||
                        fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
                    printf("setting O_NONBLOCK");
                    close(sfd);
                    break;
                }
                printf("dispatch:%d\n",sfd);
                dispatch_conn_new(sfd, conn_new_cmd, EV_READ | EV_PERSIST,DATA_BUFFER_SIZE,tcp_transport); 
                stop = true;
        }
    }
}

void base_event_handler(const int fd, const short which,void* arg)
{
    conn *c;
    c = (conn *)arg;
    c->which = which;
    /* sanity */
    if (fd != c->sfd) {
        printf("Catastrophic: event fd doesn't match conn fd!\n");
    }
    printf("get new conn\n");
    drive_machine(c);
}
conn *myconn_new(const int sfd, enum conn_states init_state,
        const int event_flags,
        const int read_buffer_size, enum network_transport transport,
        struct event_base *base) {
    conn *c=NULL;
    if (!(c = (conn *)calloc(1, sizeof(conn)))) {
        printf("calloc()\n");
        return NULL;
    }

    c->sfd = sfd;
    c->state = init_state;
    event_set(&c->event, sfd, event_flags, base_event_handler, (void *)c);
    event_base_set(base, &c->event);
    c->ev_flags = event_flags;

    if (event_add(&c->event, 0) == -1) {
        if (conn_add_to_freelist(c)) {
            conn_free(c);
        }
        printf("event_add\n");
        return NULL;
    }
    return c;
}

static inline void MfpSetEpollEv(struct epoll_event *pev, int iFd, int iFlag)
{
    uint32_t epoll_events = 0;

    if((iFlag & MFP_FLAG_READ) != 0)
    {
        epoll_events |= EPOLLIN;
    }
    if((iFlag & MFP_FLAG_WRITE) != 0)
    {
        epoll_events |= EPOLLOUT;
    }
    if((iFlag & MFP_FLAG_ERROR) != 0)
    {
        epoll_events |= EPOLLHUP | EPOLLERR;
    }

    pev->data.fd = iFd;
    pev->events = epoll_events;
}
static int CreateTCPListenSocket(struct in_addr *pstListenAddr, unsigned short ushPort)
{
    struct sockaddr_in stAddr;
    int s = 0;
    int flags = 1;              //nonblock reusaddr

    s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == -1)
    {
        printf("Error opening socket\n");
        return -1;
    }
    if(ioctl(s, FIONBIO, &flags) && ((flags = fcntl(s, F_GETFL, 0)) < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0))
    {
        close(s);
        return -1;
    }

    if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) == -1)
    {
        printf("setsockopt failed SO_REUSEADDR\n");
    }

    memset(&stAddr, 0, sizeof(stAddr));
    stAddr.sin_family = AF_INET;
    stAddr.sin_addr = *pstListenAddr;
    stAddr.sin_port = htons(ushPort);
    if(bind(s, (struct sockaddr *) &stAddr, sizeof(stAddr)) == -1)
    {
        printf("bind %s:%d failed\n", inet_ntoa(stAddr.sin_addr), ntohs(stAddr.sin_port));
        return -2;
    }
    if(listen(s, 1024) == -1)
    {
        printf("listen failed\n");
        return -3;
    }
    int iRcvBufLen = RECVBUF_LEN + 200;
    if(setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *) &iRcvBufLen, sizeof(iRcvBufLen)) == -1)
    {
        printf("setsockopt SO_RCVBUF failed\n");
    }
    return s;
}



void thread_init(int nthreads)	
{	
    workerThreads=new WorkerThreads(nthreads);

    workerThreads->initiate();
}

void dispatch_conn_new(int sfd, enum conn_states init_state, int event_flags,int read_buffer_size, enum network_transport transport) {

    CQ_ITEM *item=cqi_new();							
    int tid=(last_thread+1)%settings.num_threads;			//轮询选出workerThread（数组）

    LIBEVENT_THREAD *thread=workerThreads->threads+tid;

    last_thread=tid;

    item->sfd=sfd;											//封装必要的信息到item结构，后面会利用item封装为conn
    item->init_state=init_state;
    item->event_flags=event_flags;
    item->read_buffer_size=read_buffer_size;
    item->transport=transport;
    cq_push(thread->new_conn_queue,item);					//item需要插入到被选中的thread的全局queue里面

    printf("get item fd is:%d\n",item->sfd);
    int wc=write(thread->notify_send_fd," ",1);				//主线程和workerThread的通信方式，写入到notify_send_fd告诉辅助线程item准备好了，可以处理
    last_thread++;
}

int main(int argc, char **argv)
{
    int c;
    while(-1!=(c=getopt(argc,argv,"t:")))		//从命令行读取workerThread数
    {
        switch (c)
        {
            case 't':
                settings.num_threads=atoi(optarg);
                break;
        }
    }


    struct in_addr stListenAddr;
    string sServerIP="192.168.1.33";
    int port=10025;

    if (inet_aton(sServerIP.c_str(), &(stListenAddr)) == 0)
    {
        printf("FATAL: Invalid IP %s!\n",sServerIP.c_str());
        return -1;
    }
    int iSocket = CreateTCPListenSocket(&(stListenAddr),port);
    if(iSocket < 0){
        printf("socket create error\n");
        return -1;
    }
    conn_init();
    thread_init(settings.num_threads);
    main_base=(event_base*)event_init();
    conn *listen_conn = NULL;  
    if (!(listen_conn = myconn_new(iSocket, conn_listening,EV_READ | EV_PERSIST, 1,tcp_transport,main_base)))
    {

        printf("failed to create listening connection\n");
        return -1;
    }
    event_base_loop(main_base,0);
}
