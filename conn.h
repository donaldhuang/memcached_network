#ifndef CONN
#define CONN
#include<pthread.h>
#include<event.h>
#include"libevent_thread.h"
#include <stdlib.h>
#include<stdio.h>
#include<string>
using namespace std;
//static pthread_mutex_t conn_lock=PTHREAD_MUTEX_INITIALIZER;

//static int freecurr;
//static int freetotal;

typedef struct conn conn;
struct conn
{
	int sfd;
	LIBEVENT_THREAD *thread;
	struct event event;
	short ev_flags;
    int state;
    short which;
	//other attr;
};

//static conn **freeconns;

//static 
void conn_init(void);

void event_handler(const int fd,const short which,void *arg);

conn *conn_from_freelist();
bool conn_add_to_freelist(conn *c);
void conn_free(conn *c);

conn *conn_new(const int sfd,enum conn_states init_state,const int event_flags,
			   const int read_buffer_size,enum network_transport transport,
struct event_base *base);
#endif
