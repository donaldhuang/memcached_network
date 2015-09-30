#ifndef WORKER_THREADS
#define WORKER_THREADS

#include"libevent_thread.h"
#include<event.h>
#include<pthread.h>
#include"conn.h"
#include"cq.h"
#include<unistd.h>
#include<string.h>

#define IS_UDP(x) (x==udp_transport)

void thread_libevent_process(int fd,short which,void *arg);
void* worker_libevent(void *arg);
static pthread_cond_t init_cond;
static pthread_mutex_t init_lock;
static int init_count;

class WorkerThreads
{
private:


//	LIBEVENT_THREAD* threads;
	int nthreads;


public:
	LIBEVENT_THREAD* threads;
	WorkerThreads(int threadCount=1):nthreads(threadCount),threads(NULL)
	{
		init_count=0;
		pthread_mutex_init(&init_lock,NULL);
		pthread_cond_init(&init_cond,NULL);
	}
	void initiate();
private:
//	void *worker_libevent(void *arg);
	void create_worker(void *(*func)(void*),void *arg);
	void setup_event_thread(LIBEVENT_THREAD *me);

//	void thread_libevent_process(int fd,short which,void *arg);

};

#endif
