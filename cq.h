#ifndef CQ
#define CQ
#include<pthread.h>

#define ITEMS_PER_ALLOC 64

enum conn_states
{
	conn_listening,
	conn_new_cmd,
	conn_waiting,
	conn_read,
	conn_parse_cmd,
	conn_write,
	conn_nread,
	conn_swallow,
	conn_closing,
	conn_mwrite,
	conn_max_state
};

enum network_transport
{
	local_transport,
	tcp_transport,
	udp_transport
};

typedef struct conn_queue_item CQ_ITEM;
struct conn_queue_item
{
	int sfd;
	enum conn_states init_state;
	int event_flags;
	int read_buffer_size;
	enum network_transport transport;
	CQ_ITEM *next;
};

//typedef struct conn_queue CQ;
struct conn_queue
{
	CQ_ITEM *head;
	CQ_ITEM *tail;
	pthread_mutex_t lock;
	pthread_cond_t cond;
}CQ;


//static pthread_mutex_t cqi_freelist_lock=PTHREAD_MUTEX_INITIALIZER;
//static CQ_ITEM *cqi_freelist;

CQ_ITEM *cq_pop(conn_queue *cq);
void cqi_free(CQ_ITEM *item);
void cq_init(conn_queue *cq);
CQ_ITEM  *cqi_new(void);
void cq_push(conn_queue *cq,CQ_ITEM *item);

#endif
