#include "cpc152svc.hpp"
#include <iostream>
//#include <gklib/otd_arch_proto.h>
#include <boost/exception/all.hpp>




namespace Fastwell
{

void printf_error(boost::system::error_code & ec)
{
    if(ec)
        tapplication::write_syslog(LOG_DEBUG,"Error %s\n",ec.message().c_str());
}


tnet_session::tnet_session(tserver *own, totd_data::shared_ptr data)
    :m_strand(own->get_io_service())
    ,session_id(-1)
    ,owner(own)
    ,socket_(own->get_io_service())
    ,data_(data)
    ,timer(own->get_io_service())
{

    rd_len = 0;
    bzero(read_buffer,sizeof(read_buffer));

}

tnet_session::~tnet_session()
{
    //printf("release net session\n") ;
}





int tnet_session::send(void * ptr,int len)
{
    if(len)
    {
        try {
            system::error_code ec;
            LPBYTE bptr = (LPBYTE)ptr;
            int wr_bytes = 0;
            do {
                locker l(mut);
                wr_bytes = socket_.write_some(as::buffer(bptr,len),ec);
                //wr_bytes = socket_.send(as::buffer(bptr,len),ec);
                if(wr_bytes>0)
                {
                    bptr+=wr_bytes;
                    len-= wr_bytes;
                }
            } while(wr_bytes>0 && !ec && len>0);

            if(wr_bytes<1 || ec)
            {
                printf_error(ec);
                stop_session();

            }
            else
                return wr_bytes;

        }
        catch(...)
        {
            tapplication::write_syslog(LOG_DEBUG,"Exception in send procedure\n");
            stop_session();
        }
    }
    return 0;
}

int    tnet_session::send_async   (void * ptr,int len)
{
    socket_.async_write_some(as::buffer(ptr,len),
                            m_strand.wrap(bind(&tnet_session::handle_write,shared_from_this(),as::placeholders::error,as::placeholders::bytes_transferred)));
    return 0;
}

void tnet_session::start_read()
{

    socket_.async_read_some(as::buffer(read_buffer+rd_len,sizeof(read_buffer)-rd_len),
                            m_strand.wrap(bind(&tnet_session::handle_read,shared_from_this(),as::placeholders::error,as::placeholders::bytes_transferred)));

}

void tnet_session::handle_read(system::error_code ec,int rd_bytes)
{
    if(ec || !rd_bytes)
    {
        printf_error(ec);
        stop_session();
    }
    else
        start_read();
}

void  tnet_session::handle_write (system::error_code  ec,int wr_bytes)
{
    if(ec || !wr_bytes)
    {
        printf_error(ec);
        stop_session();
    }

}

void tnet_session::start_session()
{
    socket_.set_option(as::ip::tcp::socket::reuse_address(true));
    socket_.set_option(as::ip::tcp::socket::keep_alive(true));
    start_read() ;
}

void tnet_session::stop_session()
{
    system::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both,ec);
    socket_.close ();

}

void  tnet_session::start_timer  (as::deadline_timer * t,int msec)
{
    t->expires_from_now(posix_time::milliseconds(msec));
    t->async_wait(m_strand.wrap(bind(&tnet_session::handle_timer,this->shared_from_this(),t,msec,as::placeholders::error)));
}

as::ip::tcp::socket & tnet_session::get_socket()
{
    return socket_;
}

as::io_service &      tnet_session::get_service()
{
    return socket_.get_io_service();
}

tserver::tserver(totd_data::shared_ptr data)
    :acceptor_(svc_)
    ,data_    (data)
{
}


void  tserver::init_acceptor()
{
    acceptor_.open(ep_.protocol());
    acceptor_.set_option(as::ip::tcp::acceptor::reuse_address(true));
    acceptor_.set_option(as::ip::tcp::acceptor::receive_buffer_size(4096));
    acceptor_.bind(ep_);

}

void tserver::start_server(int port)
{

    if(port)
    {

        bool need_work = 1;
        int exception_count = 0;
        do {
            session_counter = 0;
            tapplication::write_syslog(LOG_INFO,"Starting server on port %d\n",port);
            ep_ = as::ip::tcp::endpoint(as::ip::tcp::v4(),port);
            if(!acceptor_.is_open()) init_acceptor();
            start_accept();
            try {
                svc_.run();
                need_work = 0;
            }
            catch(boost::exception & ex)
            {
                tapplication::write_syslog(LOG_DEBUG,"exception in io_service::run()\n%s\n",boost::diagnostic_information(ex).c_str());
                ++exception_count;
            }

            catch(...)
            {
                tapplication::write_syslog(LOG_DEBUG,"Common exception in io_service::run()\n");
                ++exception_count;
            }
            tapplication::write_syslog(LOG_DEBUG,"clear sessions %d\n",sessions.size());
            sessions.clear();
            tapplication::write_syslog(LOG_DEBUG,"close acceptor\n");
            acceptor_.close();

        }
        while(need_work!=0 && exception_count<3);

        if(need_work && exception_count) exit(CHILD_NEED_WORK);

    }
}

void tserver::start_accept()
{
    acceptor_.listen(get_max_connection());
    tnet_session::shared_ptr ns ( create_session() );
    acceptor_.async_accept( ns->get_socket()
                            ,bind(&tserver::handle_accept,this,ns,as::placeholders::error));

}

void           tserver::handle_accept(tnet_session::shared_ptr ns, system::error_code ec)
{
    if(!ec)
    {
        tapplication::write_syslog(LOG_INFO,"Connect %s\n"
                                   ,ns->get_socket().remote_endpoint().address().to_string().c_str()
                                  );
        ns->start_session();
    }

}

int  tserver::add_session    (tnet_session * ns)
{
    unique_locker l(mut);
    sessions[++session_counter] = ns->shared_from_this();
    return session_counter;
}

void tserver::remove_session (int key)
{
    unique_locker l(mut);
    session_list::iterator ptr = sessions.find(key);
    if(ptr != sessions.end())
    {


        ptr->second.reset();

        sessions.erase(ptr);
    }
}



}//end of namespace Fastwell



