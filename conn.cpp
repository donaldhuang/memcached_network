
//#include<pthread.h>
//#include<event.h>
#include"conn.h"

#include<unistd.h>

static pthread_mutex_t conn_lock=PTHREAD_MUTEX_INITIALIZER;
static int freecurr;
static int freetotal;



static conn **freeconns;

//static                    //why can not use static 
void conn_init(void) {														//每个Worker的
	freetotal = 200;
	freecurr = 0;
	if ((freeconns =(conn**)calloc(freetotal, sizeof(conn *))) == NULL) {
		fprintf(stderr, "Failed to allocate connection structures\n");
	}
	return;
}

conn *conn_new(const int sfd,enum conn_states init_state,const int event_flags,
			   const int read_buffer_size,enum network_transport transport,
struct event_base *base)
{
	conn *c=conn_from_freelist();
	if(NULL==c)
	{
		if(!(c=(conn*)calloc(1,sizeof(conn))))
		{
			fprintf(stderr,"calloc()\n");
			return NULL;
		}
		//initiate c
/*
{
	MEMCACHED_CONN_CREATE(c);

        c->rbuf = c->wbuf = 0;
        c->ilist = 0;
        c->suffixlist = 0;
        c->iov = 0;
        c->msglist = 0;
        c->hdrbuf = 0;

        c->rsize = read_buffer_size;
        c->wsize = DATA_BUFFER_SIZE;
        c->isize = ITEM_LIST_INITIAL;
        c->suffixsize = SUFFIX_LIST_INITIAL;
        c->iovsize = IOV_LIST_INITIAL;
        c->msgsize = MSG_LIST_INITIAL;
        c->hdrsize = 0;

        c->rbuf = (char *)malloc((size_t)c->rsize);
        c->wbuf = (char *)malloc((size_t)c->wsize);
        c->ilist = (item **)malloc(sizeof(item *) * c->isize);
        c->suffixlist = (char **)malloc(sizeof(char *) * c->suffixsize);
        c->iov = (struct iovec *)malloc(sizeof(struct iovec) * c->iovsize);
        c->msglist = (struct msghdr *)malloc(sizeof(struct msghdr) * c->msgsize);

		//根据配置大小来分配，可能会分配失败
        if (c->rbuf == 0 || c->wbuf == 0 || c->ilist == 0 || c->iov == 0 ||
                c->msglist == 0 || c->suffixlist == 0) {
            conn_free(c);
            fprintf(stderr, "malloc()\n");
            return NULL;
        }
		//end initiate c
}	
*/
	}
	//initiate c
/*
{
    c->transport = transport;                 //传输方式
    c->protocol = settings.binding_protocol;  //传输协议

    // unix socket mode doesn't need this, so zeroed out.  but why
    //  is this done for every command?  presumably for UDP
    //  mode.  
    if (!settings.socketpath) {
        c->request_addr_size = sizeof(c->request_addr);
    } else {
        c->request_addr_size = 0;
    }

    if (settings.verbose > 1) {
        if (init_state == conn_listening) {
            fprintf(stderr, "<%d server listening (%s)\n", sfd,
                prot_text(c->protocol));
        } else if (IS_UDP(transport)) {
            fprintf(stderr, "<%d server listening (udp)\n", sfd);
        } else if (c->protocol == negotiating_prot) {
            fprintf(stderr, "<%d new auto-negotiating client connection\n",
                    sfd);
        } else if (c->protocol == ascii_prot) {
            fprintf(stderr, "<%d new ascii client connection.\n", sfd);
        } else if (c->protocol == binary_prot) {
            fprintf(stderr, "<%d new binary client connection.\n", sfd);
        } else {
            fprintf(stderr, "<%d new unknown (%d) client connection\n",
                sfd, c->protocol);
            assert(false);
        }
    }

//信息赋值给conn结构，conn的构造把item全用上了
    c->sfd = sfd;
    c->state = init_state;
    c->rlbytes = 0;
    c->cmd = -1;
    c->rbytes = c->wbytes = 0;
    c->wcurr = c->wbuf;
    c->rcurr = c->rbuf;
    c->ritem = 0;
    c->icurr = c->ilist;
    c->suffixcurr = c->suffixlist;
    c->ileft = 0;
    c->suffixleft = 0;
    c->iovused = 0;
    c->msgcurr = 0;
    c->msgused = 0;

    c->write_and_go = init_state;
    c->write_and_free = 0;
    c->item = 0;

    c->noreply = false;
}
*/
	//end initiate c
	
	c->sfd=sfd;
	event_set(&c->event,sfd,event_flags,event_handler,(void*)c);
	event_base_set(base,&c->event);
	c->ev_flags=event_flags;

	if(event_add(&c->event,0)==-1)
	{
		if( conn_add_to_freelist(c))
			conn_free(c);
		perror("event_add");
		return NULL;
	}

	//MEMCACHED_CONN_ALLOCATE(c->sfd);
	
	return c;
}

conn *conn_from_freelist()
{
	conn *c;
	pthread_mutex_lock(&conn_lock);

	if(freecurr>0)
		c=freeconns[--freecurr];
	else
		c=NULL;

	pthread_mutex_unlock(&conn_lock);
}

void event_handler(const int fd,const short which,void *arg)
{
	//real handler
	char buf[128];
    printf("handle fd:%d\n",fd);
	int rc=read(fd,buf,sizeof(buf));
	buf[rc]='\0';
	printf("conn event %s\n",buf);
    string back="hello from server\n";
    write(fd,back.c_str(),back.length()+1);

}

bool conn_add_to_freelist(conn *c) 
{
	bool ret = true;
	pthread_mutex_lock(&conn_lock);

	if (freecurr < freetotal) 
	{
		freeconns[freecurr++] = c;
		ret = false;
	}
	else 
	{
		// try to enlarge free connections array 
		size_t newsize = freetotal * 2;
		conn **new_freeconns = (conn**)realloc(freeconns, sizeof(conn *) * newsize);
		if (new_freeconns) 
		{				
			freetotal = newsize;
			freeconns = new_freeconns;
			freeconns[freecurr++] = c;		
			ret = false;
		}
	}

	pthread_mutex_unlock(&conn_lock);
	return ret;
}

void conn_free(conn *c) {
	if (c) 
	{
		/*
		MEMCACHED_CONN_DESTROY(c);
		if (c->hdrbuf)		      
		free(c->hdrbuf);
		if (c->msglist)
		free(c->msglist);		
		if (c->rbuf)		
		free(c->rbuf);		
		if (c->wbuf)		
		free(c->wbuf);			
		if (c->ilist)		
		free(c->ilist);		
		if (c->suffixlist)		
		free(c->suffixlist);		
		if (c->iov)
		free(c->iov);	
		*/
		free(c);	
	}
}


