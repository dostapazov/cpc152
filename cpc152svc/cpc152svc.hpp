#ifndef cpc152svc_HPP_INCLUDED
#define cpc152svc_HPP_INCLUDED

#define BOOST_THREAD_PLATFORM_PTHREAD
#define LOCKED_QUEUE 1


#include "pch.h"
#include "io_devices.hpp"
#include "read_options.hpp"
#include <gklib/otd.h>
#include <gklib/modem_proto.h>
#include <gklib/cpc152proto.h>
#include <gklib/otd_storage.hpp>


#define DISCRETE_DLEN CPC152_DATA_CALC_SIZE(sizeof(dic120_input::data)+sizeof(dic120_input::changes_mask)+sizeof(WORD))
#define ANALOG_DLEN CPC152_DATA_CALC_SIZE(sizeof(aic124_input::data)+sizeof(WORD))


using namespace std;
using namespace boost;
namespace       as = boost::asio;

#define POLL_VARIANT 2

namespace Fastwell
{



void print_otd_data (const char * pref,lpotd_data data);
void print_otd_proto(sotd_proto & sop);
void print_bytes    (LPBYTE ptr,int len);
void printf_error   (system::error_code & ec);


QWORD timeval2qword   (timeval & tv);
QWORD get_current_time();
void  filetime2timeval(QWORD ft,timeval &tv);


struct seq_checker
{
    int prev_num;
    seq_checker():prev_num(-1){}
    bool check_0_8(int n,const char * text = "")
    {
      if(prev_num>-1)
      {
        if(8!=(n+prev_num))
           {
            printf("error_sequence %s prev = %d cur = %d\n",text,prev_num,n);
            return false;
           }

      }
      prev_num = n;
     return true;
    }

};


#define ALARM_SUFFIX    ".alarm"
#define ALARM_EXTENSION ".arch"

class traw_cpc152_server;

class totd_data
{

    totd_data(const totd_data & ) { }
    totd_data() {};
public:
    typedef boost::recursive_mutex           mutex_type;
    typedef boost::unique_lock<mutex_type>   unique_locker;
    typedef boost::shared_lock<mutex_type>   shared_locker;
    typedef boost::shared_ptr<totd_data>     shared_ptr;
    typedef vector<QWORD>                    alarms_list;
protected:
    mutable mutex_type    mut_data;
    talarms_def*          alarms_def;
    talarms               alarms;
    mutable mutex_type    mut_alarm;
    tfast_queue           pre_alarm_queue;
 #ifdef ALARM_WRITE_TWO_STEP
    tfast_queue           alarm_queue;
 #endif


    QWORD                 alarm_start;
    QWORD                 alarm_end;
    int                   alarm_file;
    ssize_t               alarm_wrb;
    DWORD                 max_alarm_data_size;
    traw_cpc152_server    * raw_server;

    void  alarm_write_thread();
    void  alarm_check_start (const lpcpc152scan_data sd);
    void  alarm_get_filename(string& fname,QWORD timestamp);
    int   alarm_open_file   (QWORD timestamp);
    int   alarm_close_file  ();
    bool  alarm_write       (lpcpc152scan_data sd, WORD sz);
    bool  alarm_check_end   (QWORD timestamp);
    bool  alarm_check_active();
    void  release_storage   ();
    void  release_pre_alarm ();
    int   get_alarms_range(WORD dev_num, WORD grp_num,WORD param_num, talarms::iterator & lo_cond, talarms::iterator & hi_cond);
    void  read_alarms       (const string &alarms_path, talarms * alarms );
public:
    totd_data(talarms_def & _alarms_def);
    virtual ~totd_data();
    mutex_type  & get_mutex()       {
        return mut_data ;
    }

    bool   prepare_queues(DWORD min_scan_period, DWORD a_count, DWORD d_count);
    DWORD  handle(lpcpc152scan_data sd,DWORD sz);

    int    get_alarms_list(alarms_list & alist);
    string alarm_get_filename(QWORD _timestamp);
    void   print_alarms_info();
    void   read_alarms       ();
    void   write_alarms_config(const string &alarms_path, talarms * alarms );
    void   set_raw_server(traw_cpc152_server * srv) { raw_server = srv; }
    bool   handle_alarm_command(talarm_condition &ac, bool erase);
};

class tserver;

/*Серверная часть проэкта*/

class tnet_session:public boost::enable_shared_from_this<tnet_session>
{
public:
    typedef boost::shared_ptr<tnet_session> shared_ptr;
    typedef boost::recursive_mutex          mutex_type;
    typedef boost::unique_lock<mutex_type>  locker;


protected:
    mutex_type              mut;
    boost::asio::strand     m_strand;
    int                     session_id;
    tserver               * owner;
    as::ip::tcp::socket     socket_;
    totd_data::shared_ptr   data_;
    as::deadline_timer      timer;


    BYTE   read_buffer[4096];
    int    rd_len;

    virtual void  handle_read  (system::error_code  ec,int rd_bytes) ;
    void  start_timer  (as::deadline_timer * t,int msec);
    virtual void  handle_timer (as::deadline_timer * t,int msec,system::error_code ec) = 0;
    virtual void  handle_write (system::error_code  ec,int wr_bytes);
    virtual void  start_read();

public:
    tnet_session(tserver * own, totd_data::shared_ptr data);
    virtual        ~tnet_session();

    virtual         void   start_session();
    virtual         void   stop_session ();
    virtual         int    send         (void * ptr,int len) ;
    virtual         int    send_async   (void * ptr,int len) ;

    as::ip::tcp::socket & get_socket();
    as::io_service &      get_service();
    mutex_type &          get_mutext () {
        return mut;
    }


};

class tserver
{
public:
    typedef boost::recursive_mutex mutex_type;
    typedef boost::unique_lock<mutex_type> unique_locker;


protected:
    as::io_service        svc_;
    as::ip::tcp::acceptor acceptor_;
    as::ip::tcp::endpoint ep_;
    totd_data::shared_ptr data_;
    int        session_counter;
    mutex_type mut;
    typedef std::map<int,tnet_session::shared_ptr> session_list;
    session_list       sessions;

    virtual tnet_session * create_session() = 0;

    virtual void           handle_accept(tnet_session::shared_ptr ns, system::error_code ec);
    virtual void           init_acceptor();
    virtual int            get_max_connection() {
        return as::ip::tcp::acceptor::max_connections;
    }

    tserver(totd_data::shared_ptr data);
public:
    virtual void start_server(int port);
    void start_accept();
    as::io_service & get_io_service() {
        return svc_;
    }
    int  add_session    (tnet_session * ns);
    void remove_session (int key);

};

class tdev_poller;

class traw_cpc152_session:public tnet_session
{
   protected:
    typedef boost::recursive_mutex mutex_type;
    typedef boost::unique_lock<mutex_type> locker;
    atomic_bool         send_busy;
    atomic<int>         packet_num;
    mutex_type          mut;
    __int64             connection_time;
    int                 timer_count;
    int                 timer_request_tmsync;
    modem_proto_buffer  mpb;
    rx_buffer           rxb;
    tfast_queue         send_queue;
    int                 frame_size;
    LPBYTE              pre_send_buffer;
    LPBYTE              tmp_send_buffer;
    LPMPROTO_HEADER     send_mph;
    lpcpc152proto_hdr   cpc152hdr;
    int                 analog_scan;
    __int64             total_write;
    __int64             total_read;



    virtual void handle_read   (system::error_code  ec,int rd_bytes) override;
    virtual void handle_timer  (as::deadline_timer * t,int msec,system::error_code  ec) override;
    virtual void start_read    () override;
            void sync          ();
            void handle_modem_proto    ();
            void handle_modem_proto    (LPMPROTO_HEADER mph);
            void init_session          ();
            void handle_write          (system::error_code  ec,int wr_bytes) override;
            void request_time_sync     ();
            void handle_alarm_condition(lpcpc152proto_hdr phdr, bool is_erase, bool);
            void handle_time_sync      (lpcpc152proto_hdr phdr,bool is_respond,bool);
            void handle_get_alarms_list();
            void do_send_alarms_list   ();
            void request_discrete      ();

            void handle_get_alarm_file(lpcpc152proto_hdr phdr,bool is_respond, bool is_erase, bool is_end);
            void do_send_alarm_files(int count, boost::shared_ptr<QWORD> qptr, bool send, bool erase);
            void do_send_alarm_file(QWORD alarm_id, bool send, bool erase);

   public:
    traw_cpc152_session(tserver * own,totd_data::shared_ptr data,int _frame_size):tnet_session(own,data),frame_size(_frame_size)
    {
       init_session();
    }
    virtual ~traw_cpc152_session ();
    virtual void start_session   () override;
    virtual void stop_session    () override;
            void do_send         (lpcpc152scan_data sd);
            bool do_send         (lpcpc152proto_hdr hdr, int sz);
            void do_send         (LPMPROTO_HEADER mph);
            void new_alarm_notify(QWORD alarm_id     );



};


class traw_cpc152_server :public tserver
{
protected:
    tdev_poller * dev_poller;
    int frame_size;
    virtual tnet_session * create_session() override;
    int            get_max_connection() {
        return 1;
    }
    void           handle_accept(tnet_session::shared_ptr ns, system::error_code ec) override;
public:
    traw_cpc152_server(totd_data::shared_ptr data,int _frame_size)
        :tserver(data),dev_poller(nullptr),frame_size(_frame_size) {}
    typedef boost::shared_ptr<traw_cpc152_server> shared_ptr;
    void send(cpc152scan_data * sd);
    void new_alarm_notify(QWORD alarm_id);
    void set_dev_poller(tdev_poller * dp  ){dev_poller = dp;}
 tdev_poller* get_dev_poller() {return dev_poller;}

};



class tdev_poller
{
public:
protected:
    totd_data::shared_ptr            data_;
    traw_cpc152_server::shared_ptr   server_;
    typedef std::vector<std::pair<int,int>> tdevices_list;
    atomic<int>        terminate_request;
    tfast_queue        queue;
    tdevices_list      d_input; // pair::first   - device file descriptor
    tdevices_list      a_input; // painr::second - device scan frequency
    int                watch_dog_dev;

    void handle_data();
    void scan_data_protect(lpcpc152scan_data sd);
    int  prepare_fdset(bool discrete,fd_set * fds);
    void dev_start  (bool discrete, bool start);
    void read_devices (bool discrete);
    void do_read      (bool discrete,int fd,int dev_num,char * buf,int bsz);
    void handle_read_discrete(int dev_num, char * buf, int blen);
    void handle_read_analog  (int dev_num,char * buf,int blen);

public:
    tdev_poller(totd_data::shared_ptr _data,traw_cpc152_server::shared_ptr _server,DWORD queue_size,int wdg_port);

    void      add_device(int fd,int scan_freq,bool discrete)
    {
        if(discrete) d_input.push_back(std::pair<int,int>(fd,scan_freq));
        else a_input.push_back(std::pair<int,int>(fd,scan_freq));
    }
    int       get_device_count() {
        return (int)d_input.size()+a_input.size();
    }
    void      terminate()      {
        terminate_request.fetch_add(1);
    }
    bool      terminate_check() {
        return terminate_request.fetch_add(0) !=0 ? true : false;
    }
    void      polling();
    void      refresh_discrete    ();

//static void      do_delay(clock::time_point & tm_beg, chrono::nanoseconds &delay_time);
//static void      do_delay(chrono::nanoseconds &delay_time);
};





#define CHILD_NEED_WORK       -1
#define CHILD_NEED_TERMINATE   0


class tapplication
{
protected:

    static app_config  config;


    totd_data::shared_ptr             data    ;
    traw_cpc152_server::shared_ptr    server  ;
    boost::shared_ptr<tdev_poller>    poller  ;
    int ro_result;
    boost::thread                   *server_thread;
    boost::thread                   *poller_thread;

    struct sigaction sigact;
    sigset_t  sigset;
    int       signo;


    int  daemon_start();
    void daemon_control_proc();
    void daemon_work_init();
    int  init_application();
    int  open_device(char * dev_name);
    int  init_devices();
    int  init_dic120(tdic_param *beg, tdic_param *end, int * _count);
    int  init_aic124(taic_param *beg, taic_param *end, int * _count, int *_min_scan_period);

    int  do_run();


public:
    tapplication(int argc,char **argv);
    int run();
    static int get_log_level() {
        return config.log_level;
    }
    static const char * pid_file_name;
    static void pid_file(bool create);
    static tapplication * app;
    void   stop_threads  ();
    static std::string config_file;
    static void write_syslog(int pri,const char * fmt,...);
    static int  iopl_init  (bool release = false);
    static void clear_tmp_files();
    static const char * rdopts_lib;
    static const app_config & get_config(){return config;}
};



} // end namespace Fastwell

#endif // cpc152svc_HPP_INCLUDED
