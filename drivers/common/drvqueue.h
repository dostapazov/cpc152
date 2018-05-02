#ifndef __DRVQUEUE__
#define __DRVQUEUE__


#include <linux/types.h>
#include <linux/atomic.h>

struct dqueue
{

  int               q_size;
  u32               q_entry_size;
  u8 *              q_data;
  volatile u32      q_head;
  volatile u32      q_tail;
           atomic_t q_count;
           atomic_t q_uncommited;
  volatile int      q_count_max;
};


typedef struct  dqueue * lpdqueue;

static inline int  dqueue_get_qcount(lpdqueue pq)
{
  return atomic_read(&pq->q_count);
}

static inline int  dqueue_is_full (lpdqueue pq)
{
  return atomic_read(&pq->q_count) == pq->q_size ? 1 : 0;
}

static inline int  dqueue_is_empty(lpdqueue pq)
{
  return atomic_read(&pq->q_count) == 0 ? 1 : 0;
}

static inline void dqueue_reset(lpdqueue pq){ pq->q_tail = pq->q_head = 0;atomic_set(&pq->q_count,0);pq->q_count_max = 0;}


#ifdef __cplusplus
extern "C"{
#endif


extern int   dqueue_init    (lpdqueue pq, u32 q_entry_size,u32 qsize);
extern void  dqueue_release (lpdqueue pq);
extern u32   dqueue_add     (lpdqueue pq, u8* data        , u32 dlen);
extern s32   dqueue_get     (lpdqueue pq, u8* data        , u32 dsz, u32 remove);
extern void* dqueue_get_ptr (lpdqueue pq,s32 * len);
extern void  dqueue_remove_first(lpdqueue pq);
extern void* __dqueue_alloc_input (lpdqueue pq,u32 dlen);
extern u32   __dqueue_commit(lpdqueue pq);


#ifdef __cplusplus
}
#endif


#endif
