#ifndef IO_DEVICES_HPP
#define IO_DEVICES_HPP
#include "pch.h"
#include <lin_ke_defs.h>


namespace Fastwell
{

class tevent
{
protected:
    boost::mutex               mut;
    boost::condition_variable  cv;
    int    ev_value;
    int    ev_count;
public:
    tevent()   {
        ev_value = ev_count = 0;
    }
    void fire (int v,bool all = true);
    void reset();
    bool wait ();
    bool wait (int msec);
    int get_value();

    struct count_changed
    {
        tevent * ev ;
        int     count;

        count_changed(tevent & _ev,int _count):ev(&_ev),count(_count) {};
        count_changed(const count_changed & c) {
            *this = c;
        }
        count_changed & operator = (const count_changed & src) {
            ev= src.ev;
            count = src.count;
            return * this;
        }
        bool operator()()
        {
            return  ev->ev_value && ev->ev_count!=count ? true : false;
        }

    };
};

#pragma pack (push,1)
struct tqentry
{
    WORD size;
    BYTE data[ANYSIZE_ARRAY];
};
#pragma pack(pop)

class tfast_queue
{
protected:
    typedef boost::recursive_mutex             mutex_type;
    typedef boost::unique_lock<mutex_type>     locker;
    mutex_type mut;
    DWORD  wr_pos;
    DWORD  rd_pos;
    DWORD  max_size_;
    DWORD  entry_size_;
    DWORD  queue_count;
    DWORD  qsize;
    DWORD  max_queue_count;
    DWORD  need_mem;
    int    mem_mapped;
    int    fd;
    LPBYTE data_;
    mutable tevent event;

    LPBYTE alloc_memory_map(DWORD bytes, const char *file_name);
    void   free_memory_map ();

public :
    tfast_queue();
    ~tfast_queue() {
        release_queue();
    }
    bool       alloc_entry      (WORD sz,LPBYTE dptr,bool force = false);
    bool       alloc_alarm_entry(WORD sz, LPBYTE dptr);
    void       release_entry();
    tqentry *  get_entry();
    int        get_entry(LPVOID ptr,int sz);

    DWORD      get_queue_count() {
        locker l(mut);
        return queue_count;
    }

    DWORD      get_queue_size() {

        return qsize;
    }

    bool       is_queue_full () {
        locker l(mut);
        return queue_count == qsize ? true:false;
    }
    void       fire_event()    {
        event.fire(1);
    }
    bool       wait(int timeout) {
        return event.wait(timeout);
    }
    bool       wait() {
        return event.wait();
    }
    DWORD get_entry_size(){
        return entry_size_;
    }

    void       release_queue();
    bool setup_queue(DWORD count, DWORD size, bool in_mmap = false ,const char * file = nullptr);
    tevent &   get_event() {
        return event;
    }
    void       drop_all ();

};


}

#endif // IO_DEVICES_HPP
