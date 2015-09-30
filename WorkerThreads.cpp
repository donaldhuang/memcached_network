
/*
#include"libevent_thread.h"
#include<event.h>
#include<pthread.h>
#include"conn.h"
#include"cq.h"
#include<unistd.h>
#include<string.h>
*/
#include"WorkerThreads.h"
#include<stdio.h>
/*
WorkerThreads::WorkerThreads(int threadCount=1):nthreads(threadCount),threads(NULL)
{
    init_count=0;
    pthread_mutex_init(&init_lock,NULL);
    pthread_cond_init(&init_cond,NULL);
}
*/

void WorkerThreads::initiate()
{
    int i;

    threads=(LIBEVENT_THREAD*)calloc(nthreads,sizeof(LIBEVENT_THREAD));		//LIBEVENT_THREAD,加入libevent元素的thread结构 “数组”
    if(!threads)
    {
        perror("can't allocate thread des");
        exit(1);
    }

    for(i=0;i<nthreads;i++)													//设置thread和thread中的libevent所需属性
    {
        int fds[2];
        if(pipe(fds))														//thread和主线程的通信pipe
        {
            perror("can't create notify pipe");
            exit(1);
        }

        threads[i].notify_receive_fd=fds[0];
        threads[i].notify_send_fd=fds[1];

        setup_event_thread(&threads[i]);									//设置thread和thread中的libevent所需属性
        printf("init thread:%d\n",i);
    }

    for(i=0;i<nthreads;i++)
    {
        create_worker(worker_libevent,&threads[i]);							//启动thread
    }

    pthread_mutex_lock(&init_lock);
    while( init_count < nthreads)
    {
        pthread_cond_wait(&init_cond,&init_lock);
    }
    pthread_mutex_unlock(&init_lock);

    printf("finish\n");
}

void WorkerThreads::setup_event_thread(LIBEVENT_THREAD *me)
{
    me->base=(event_base*)event_init();//every thread has its own event_base

    //2.0 has event_config
    /*
       struct event_config *cfg=event_config_new();
       event_config_avoid_method(cfg,"epoll");
       me->base=event_base_new_with_config(cfg);
       event_config_free(cfg);
       */
    //in order to use libevent on file,use this method

    if(!me->base)
    {
        fprintf(stderr,"can't allocate event base\n");
        exit(1);
    }

    event_set(&me->notify_event,me->notify_receive_fd,						//设置 监听事件 和 处理函数
            EV_READ|EV_PERSIST, thread_libevent_process,me);	
    event_base_set(me->base,&me->notify_event);

    if(event_add(&me->notify_event,0)==-1)
    {
        fprintf(stderr,"can't monitor libevent notify pipe\n");
        exit(1);
    }

    //why initiate conn_queue here?
    me->new_conn_queue=(conn_queue*)malloc(sizeof(struct conn_queue));		//内部的conn_queue
    if(me->new_conn_queue==NULL)
    {
        perror("Failed to allocate memory for connection queue");
        exit(EXIT_FAILURE);
    }

    cq_init(me->new_conn_queue);
}

void thread_libevent_process(int fd,short which,void *arg)					//处理函数，即当主线程通知workerThread时，主线程会插入一个item到某个thread的queue中，queue是一个工具类
    //workerThread将item pop出来并封装为conn，封装期间就建立了和item所指向的对象的联系，也使用libevent完成
{
    LIBEVENT_THREAD *me=(LIBEVENT_THREAD*)arg;
    CQ_ITEM *item;

    char buf[1];

    if(read(fd,buf,1)!=1)
        fprintf(stderr,"can't read from libevent pipe\n");

    item=cq_pop(me->new_conn_queue);
    printf("process item fd is:%d\n",item->sfd);
    if(NULL!=item)
    {
        conn *c= conn_new (item->sfd,item->init_state,item->event_flags,
                item->read_buffer_size,item->transport,me->base);

        if(NULL==c)
        {
            if( IS_UDP(item->transport))
            {
                fprintf(stderr,"can't listen for events on UDP\n");
                exit(1);
            }
            else
            {
                fprintf(stderr,"can't listen for events on fd %d\n",item->sfd);
                close(item->sfd);
            }
        }
        else
        {
            printf("%lu get content\n",pthread_self());
            printf("sfd:%d,read_size:%d\n",item->sfd,item->read_buffer_size);
            c->thread=me;
            //test
            write(item->read_buffer_size,"abc",3);
        }
        cqi_free(item);
    }

}

void WorkerThreads::create_worker(void *(*func)(void*),void *arg)
{
    pthread_t thread;
    pthread_attr_t attr;
    int ret;

    pthread_attr_init(&attr);

    if((ret=pthread_create(&thread,&attr,func,arg))!=0)				
    {
        fprintf(stderr,"can't create thread: %s\n",strerror(ret));
        exit(1);
    }
}

void* worker_libevent(void *arg)
{
    LIBEVENT_THREAD *me = (LIBEVENT_THREAD*)arg;
    pthread_mutex_lock(&init_lock);
    init_count++;
    pthread_cond_signal(&init_cond);
    pthread_mutex_unlock(&init_lock);

    event_base_loop(me->base,0);
    return NULL;
}






