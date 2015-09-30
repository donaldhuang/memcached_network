
#include "cq.h"
#include<stdlib.h>
#include<stdio.h>


static pthread_mutex_t cqi_freelist_lock=PTHREAD_MUTEX_INITIALIZER;
static CQ_ITEM *cqi_freelist;
void cqi_free(CQ_ITEM *item)
{
	pthread_mutex_lock(&cqi_freelist_lock);
	item->next=cqi_freelist;
	cqi_freelist=item;
	pthread_mutex_unlock(&cqi_freelist_lock);
}

CQ_ITEM *cqi_new(void)
{
	CQ_ITEM *item=NULL;
	pthread_mutex_lock(&cqi_freelist_lock);
	if(cqi_freelist)
	{
		item=cqi_freelist;
		cqi_freelist=item->next;
	}
	pthread_mutex_unlock(&cqi_freelist_lock);

	if(NULL==item)
	{
		int i;
		item =(CQ_ITEM*) malloc(sizeof(CQ_ITEM)*ITEMS_PER_ALLOC);
		if(NULL ==item)
			return NULL;

		for(i=2;i<ITEMS_PER_ALLOC;i++)
			item[i-1].next=&item[i];
	
		pthread_mutex_lock(&cqi_freelist_lock);
		item[ITEMS_PER_ALLOC-1].next=cqi_freelist;
		cqi_freelist=&item[1];
		pthread_mutex_unlock(&cqi_freelist_lock);
	}
	return item;
}



void cq_init(conn_queue *cq)
{
	pthread_mutex_init(&cq->lock,NULL);
	pthread_cond_init(&cq->cond,NULL);
	cq->head=NULL;
	cq->tail=NULL;
}
CQ_ITEM *cq_pop(conn_queue *cq)
{
	CQ_ITEM *item;
	pthread_mutex_lock(&cq->lock);
	item=cq->head;
	if(NULL!=item)
	{
		cq->head=item->next;
		if(NULL==cq->head)
			cq->tail=NULL;
	}
	pthread_mutex_unlock(&cq->lock);
	return item;
}
void cq_push(conn_queue *cq,CQ_ITEM *item)
{
/*
	printf("be\n");
	
	if(cq==NULL)
		printf("cq null\n");
	if(item==NULL)
		printf("item null\n");
*/
	item->next=NULL;
	pthread_mutex_lock(&cq->lock);
	if(NULL==cq->tail)
	{
		cq->head=item;
	}
	else
		cq->tail->next=item;
	cq->tail=item;
	pthread_cond_signal(&cq->cond);
	pthread_mutex_unlock(&cq->lock);

}
