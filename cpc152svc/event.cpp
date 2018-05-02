#include "cpc152svc.hpp"

namespace Fastwell {

void tevent::fire (int v,bool all )
{
    {
        boost::unique_lock<boost::mutex> l(mut);
        ev_value = v;
        ++ev_count;
    }
    if(all)
        cv.notify_all();
    else
        cv.notify_one();

}

void tevent::reset()
{
    boost::unique_lock<boost::mutex> l(mut);
    //cout<<"reset event "<<boost::this_thread::get_id()<<std::endl;
    ev_value = 0;
}


bool tevent::wait()
{
    boost::unique_lock<boost::mutex> l(mut);
    cv.wait(l,count_changed(*this,ev_count));
    return ev_value ? true:false;
}

bool tevent::wait(int msec)
{
    boost::unique_lock<boost::mutex> l(mut);
    return cv.wait_for(l,boost::chrono::milliseconds(msec),count_changed(*this,ev_count));
}


int tevent::get_value()
{
    boost::lock_guard<boost::mutex> l(mut);
    return ev_value;
}


}
