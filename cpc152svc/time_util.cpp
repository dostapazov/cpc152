#include "cpc152svc.hpp"
#include <boost/chrono.hpp>


namespace Fastwell {


void  filetime2timeval(QWORD ft,timeval &tv)
{
    ft/=10;
    ft-=11644473600000000LL;
    tv.tv_usec = ft%1000000;
    tv.tv_sec  = ft/1000000;

}


QWORD timeval2qword(timeval & tv)
{
    QWORD ret = 11644473600;
    ret+=tv.tv_sec;
    ret*= 10000000;
    ret+= tv.tv_usec*10;
    //ret-= ret%10000;
    return ret;
}

QWORD get_current_time()
{

    /*
    chrono::system_clock::time_point t = chrono::system_clock::now();
    chrono::nanoseconds  ns = t.time_since_epoch();
    return ns.count();
    */
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return timeval2qword(tv);

}

}
