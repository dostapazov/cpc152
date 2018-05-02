
#include "drvqueue.h"

#include <linux/gfp.h>
#include <linux/slab.h>



int dqueue_loop_head(lpdqueue pq)
{
 int ret;
 if(++pq->q_head>=pq->q_size)
     pq->q_head = 0;
 ret = atomic_dec_return(&pq->q_count);
 return ret;

}

int dqueue_loop_tail(lpdqueue pq)
{
 int ret;
 if(++pq->q_tail>=pq->q_size)
     pq->q_tail = 0;
 ret = atomic_inc_return(&pq->q_count);
 if(ret>pq->q_count_max)
     pq->q_count_max = ret;
 return ret;

}


#pragma pack(push,1)

struct qentry
{
   u16 dlen;
   u8  data[1];
};
#pragma pack(pop)

typedef struct qentry* lpqentry;

lpqentry dqueue_get_entry(lpdqueue pq, u32 idx)
{
  lpqentry res = NULL;
  u8 * ptr = pq->q_data;
  if(idx<pq->q_size)
  {
     idx *=pq->q_entry_size;
     ptr+=idx;
     res =(lpqentry)ptr;
  }
  return res;
}

int dqueue_init(lpdqueue pq,u32 q_entry_size,u32 qsize)
{
 //Initialize queue
 lpqentry qe;
 u32  need_size;

 dqueue_release(pq);
 if(qsize && q_entry_size)
 {
 q_entry_size += sizeof(qe->dlen);
 need_size   = q_entry_size*qsize;

 pq->q_data  = (u8*)kmalloc(need_size,GFP_KERNEL);
 #ifdef _DEBUG
  printk(KERN_DEBUG "QUEUE size %d. Alloc GFP_KERNEL memory size %d PAGE_SIZE %lu  :  %p\n",(int)pq->q_size,(int)need_size,PAGE_SIZE,pq->q_data);
 #endif
 if(pq->q_data)
  {
    pq->q_entry_size = q_entry_size;
    pq->q_size       = qsize;
  }
 }
 return pq->q_data == NULL ? -ENOMEM : 0;
}

void dqueue_release(lpdqueue pq)
{

 atomic_set(&pq->q_count,0);
 pq->q_head  = pq->q_tail = 0;
 pq->q_entry_size = 0;
 pq->q_size = 0;

 if( pq->q_data)
      {
        #ifdef _DEBUG
          printk(KERN_DEBUG "Free GFP_KERNEL memory %p\n",pq->q_data);
        #endif
        kfree(pq->q_data);
      }

  pq->q_data = NULL;
}


void * __dqueue_alloc_input(lpdqueue pq,u32 dlen)
{
 lpqentry e;
 if(dlen && dlen<=(pq->q_entry_size-sizeof(e->dlen)))
 {
   atomic_set(&pq->q_uncommited,1);
   e = dqueue_get_entry(pq,pq->q_tail);
   e->dlen = dlen;
   return e->data;
 }
 return NULL;
}

u32 __dqueue_commit(lpdqueue pq)
{
 atomic_set(&pq->q_uncommited,0);
 return dqueue_loop_tail(pq);
}

u32 dqueue_add  (lpdqueue pq,u8* data,u32 dlen)
{
  u32 ret = 0;
  if(data)
   {
    void * dest_ptr = __dqueue_alloc_input(pq,dlen);
    if(dest_ptr)
      {
       memcpy(dest_ptr,data,dlen);
       ret = __dqueue_commit(pq);
      }
   }
#ifdef _DEBUG
  else
  printk("invalid data %lu %u\n",(long unsigned int)data,dlen);
#endif
  return ret;

}


void dqueue_remove_first(lpdqueue pq)
{
 if(!dqueue_is_empty(pq))
     dqueue_loop_head(pq);
}

s32  dqueue_get  (lpdqueue pq, u8* data, u32 dsz,u32 remove )
{

  s32 ret = 0;
  void * ptr = dqueue_get_ptr(pq,&ret);
  if(ptr && data && (s32)dsz>=ret)
  {
    memcpy(data,ptr,ret);
    if(remove) dqueue_loop_head(pq);
  }

  /*
  lpqentry e;
  if(!dqueue_is_empty(pq))
  {
    e = dqueue_get_entry(pq,pq->q_head);
    ret = e->dlen;
    if(dsz < e->dlen) ret = -ret;
       memcpy(data,e->data, dsz < e->dlen ? dsz:e->dlen);
   if(remove)
     dqueue_loop_head(pq);
  }
  */
  return ret;
}

void * dqueue_get_ptr(lpdqueue pq,s32 * len)
{
  s32    __len = 0;
  void * ret = NULL;
  if(!dqueue_is_empty(pq))
  {
    lpqentry e = dqueue_get_entry(pq,pq->q_head);
    __len = (s32)e->dlen;
    ret = e->data;
  }

  if(len) *len = __len;
  return ret;
}


