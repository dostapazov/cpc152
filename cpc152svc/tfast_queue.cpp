#include "cpc152svc.hpp"
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/queue.h>


namespace Fastwell
{


tfast_queue::tfast_queue()
    : wr_pos(0)
    ,rd_pos(0)
    ,max_size_(0)
    ,entry_size_(0)
    ,queue_count(0)
    ,qsize(0)
    ,mem_mapped(0)
    ,data_(nullptr)

{  }

void           tfast_queue::release_queue()
{
    locker l(mut);
    if(mem_mapped)
       free_memory_map();
    else
        if(nullptr!=data_) delete [] data_;
    data_ = nullptr;
    rd_pos = wr_pos = max_size_ = queue_count = qsize = entry_size_ = 0;

}

LPBYTE tfast_queue::alloc_memory_map(DWORD bytes, const char * file_name)
{
    LPBYTE  ret = NULL;
    if(bytes)
    {
        bytes += ( _SC_PAGESIZE-bytes% _SC_PAGESIZE);
        int flags = MAP_PRIVATE|MAP_32BIT;

        if(file_name)
           {
            fd  = open(file_name,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR);
            if(fd<0) return ret;
             if(posix_fallocate(fd,0,bytes))
             {
               close(fd);
               fd = -1;
               unlink(file_name);
               return ret;
             }
           }
        else
        {
            fd = -1;
            flags |= MAP_LOCKED|MAP_ANONYMOUS;
        }



        ret =(LPBYTE) mmap(NULL,bytes
                          ,PROT_READ|PROT_WRITE
                          ,flags
                          ,fd,0);
        if(ret != MAP_FAILED)
        {
            mem_mapped = 1;
            need_mem   = bytes;
        }
        else
         {
          tapplication::write_syslog(LOG_ALERT,"error mmap for size %d errno %d\n",bytes,errno);
          ret = NULL;
         }
    }
    return ret;
}

void   tfast_queue::free_memory_map ()
{
        if(this->data_)
        {
            munmap(data_,need_mem);
            if(fd>0)
            close(fd);
            fd = -1;
        }

}


bool           tfast_queue::setup_queue  (DWORD count, DWORD size, bool in_mmap,const char * fname)
{
    locker l (mut);

    max_queue_count = 0;
    max_size_ = size;
    entry_size_ = max_size_+sizeof(tqentry);
    entry_size_-= sizeof(tqentry::data[0]);
    wr_pos = rd_pos = 0;
    qsize = count;
    need_mem = qsize*entry_size_;
    if(in_mmap)
    {
        data_ = alloc_memory_map(need_mem,fname);
        if(data_) return true;
        if(fname) return false;
    }
    data_ = new BYTE [need_mem];
    return data_ != nullptr ? true : false;

}


bool   tfast_queue::alloc_alarm_entry(WORD sz,LPBYTE dptr )
{
  return alloc_entry(sz,dptr,true);
}

bool tfast_queue::alloc_entry(WORD sz,LPBYTE dptr, bool force)
{

    if(sz<=max_size_)
    {
        locker l(mut);
        if(data_ && queue_count<qsize)
        {
            LPBYTE ptr = data_;
            ptr+= wr_pos*entry_size_;
            ++wr_pos;
            ++queue_count;
            if(wr_pos>=qsize) wr_pos=0;
            tqentry * hdr = (tqentry *)ptr;
            hdr->size = sz;
            memcpy(hdr->data,dptr,sz);
            if(this->queue_count%10 ==1)
                event.fire(1);
#ifdef _DEBUG
            max_queue_count = std::max(max_queue_count,queue_count);
#endif
            return true;
        }

        else
        {
         event.fire(1);
         if(force)
         {
            release_entry();
            return alloc_entry(sz,dptr,force);
         }
        }
    }
    return false;
}

void   tfast_queue::drop_all()
{
    locker l(mut);
    queue_count = 0;
    rd_pos = wr_pos = 0;
    event.reset();

}

void   tfast_queue::release_entry()
{
    locker l(mut);

    if(queue_count)
    {
        ++rd_pos;
        if(rd_pos>=qsize) rd_pos = 0;
        if((--queue_count)<=0)
        {
            queue_count = 0;
            event.reset();
        }
    }

}

tqentry *   tfast_queue::get_entry()
{
    locker l(mut);
    if(this->queue_count)
    {
        LPBYTE bptr = data_;
        bptr+=rd_pos*entry_size_;
        return (tqentry*)bptr;
    }
    return nullptr;
}

int       tfast_queue::get_entry(LPVOID ptr,int sz)
{
  int ret = 0;
  locker l(mut);
  tqentry * qe = get_entry();
  if(qe)
  {
    int esz = qe->size+sizeof(*qe)-sizeof(qe->data[0]);
    if(esz<=sz)
    {
     ret = esz;
     memcpy(ptr,qe,esz);
     release_entry();
    }
  }
  return ret;
}

}

//#include "../drivers/common/drvqueue.h"
void test_queue()
{

//       BYTE d[3];
//      Fastwell::tfast_queue q;
//      q.setup_queue(3,3);
//      q.release_entry();
//      for(int i = 1;i<256;i++)
//      {
//         memset(d,i,sizeof(d));
//         q.alloc_entry(1,1,1,sizeof(d),d,0);
//         if(i>2)
//               q.release_entry();
//      }

//     BYTE v[33];
//     BYTE r[33];

//    struct dqueue dq;
//    dqueue_init(&dq,33,3);
//    for(int i = 0;i<1000;i++)
//    {
//     memset(v,i+1,sizeof(v));
//     dqueue_add(&dq,v,11*(1+i%3));
//     dqueue_get(&dq,r,sizeof(r),i>1);

//    }
//    dqueue_release(&dq);
}
