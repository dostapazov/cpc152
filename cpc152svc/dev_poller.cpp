#include "cpc152svc.hpp"
#include "io_devices.hpp"
#include <time.h>
#include <linux/watchdog.h>






namespace Fastwell
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"


tdev_poller::tdev_poller(totd_data::shared_ptr _data, traw_cpc152_server::shared_ptr _server, DWORD queue_size, int wdg_dev)
    :data_(_data),server_(_server),watch_dog_dev(wdg_dev)
{

    queue.setup_queue(queue_size,ANALOG_DLEN);
    data_->set_raw_server(_server.get());

}
#pragma GCC diagnostic pop


void tdev_poller::handle_data()
{
    
//    sched_param sp;
//    int policy;
//   if(!pthread_getschedparam(pthread_self(),&policy,&sp))
//   {
//
//      policy = sched_policy;
//      int maxpri = sched_get_priority_max(policy);
//      sp.__sched_priority = min(maxpri,sched_prio);
//      pthread_setschedparam(pthread_self(),policy,&sp);
//
//     //pthread_setschedprio(pthread_self(),15);
//    }

    if(watch_dog_dev>0)
      {
        int opt = WDIOS_ENABLECARD;
        ioctl(watch_dog_dev,WDIOC_SETOPTIONS,&opt);
        opt = 60;
        ioctl(watch_dog_dev,WDIOC_SETTIMEOUT,&opt);
      }

    int bsz = queue.get_entry_size();
    tqentry* qentry = (tqentry*)new BYTE [ bsz ];
    int wc_count = 0;
 try{
    while(!terminate_check())
    {
        if(queue.wait(1000))
        {
            int data_sz;
            do
            {
              data_sz = queue.get_entry(qentry,bsz);

             if(data_sz)
              {
                 server_->send((lpcpc152scan_data)qentry->data);
                 data_->handle((lpcpc152scan_data)qentry->data,qentry->size);
              }
            if(watch_dog_dev>0 && (++wc_count) > 1000 )
               {
                ioctl(watch_dog_dev,WDIOC_KEEPALIVE,0);
                wc_count=0;
               } 
              
             }while(data_sz);
        }

        if(watch_dog_dev>0)
           ioctl(watch_dog_dev,WDIOC_KEEPALIVE,0);

    }
   }
   
  catch(...)
   {
     tapplication::write_syslog(LOG_ALERT,"Exception in dev_poller::handle_data\n");
     exit(CHILD_NEED_WORK);
   }
    delete [] (LPBYTE)qentry;
    if(watch_dog_dev>0)
      {
        int opt = WDIOS_DISABLECARD;
        ioctl(watch_dog_dev,WDIOC_SETOPTIONS,&opt);
      }

}


void tdev_poller::scan_data_protect(lpcpc152scan_data sd)
{
 LPBYTE ptr = sd->data;
 ptr+=sd->dlen;
 *((LPWORD)ptr) = calc_kpk(sd,cpc152scan_data_get_size(*sd),OTD_DEF_POLINOM);
}



void tdev_poller::handle_read_discrete(int dev_num, char * buf,int blen)
{
  try
  { 

    BYTE tmp_buf[DISCRETE_DLEN];
    lpcpc152scan_data sd = (lpcpc152scan_data)tmp_buf;

    sd->dev_num  = dev_num&CPC152SCAN_DATA_DEVNUM_MASK;
    sd->dev_num |= CPC152SCAN_DATA_DISCRETE;


    lpdic120_input ptr = (lpdic120_input)buf;
    int count = blen/sizeof(*ptr);
    lpdic120_input eptr = ptr+count;


    while(ptr<eptr)
    {
        sd->group_num   = ptr->number;
        sd->seq_number  = ptr->seq_number;
        sd->tm_beg      = timeval2qword(ptr->tmstamp_start);
        sd->tm_end      = timeval2qword(ptr->tmstamp_end);
        sd->dlen        = ptr->data_len;
        memcpy(sd->data,ptr->data,sd->dlen);

        bool worked  = ptr->test_signals[0] == P55ID ? true:false;
        #ifdef _DEBUG
        worked = true;
        #endif
        if(!worked)
            sd->dev_num|=CPC152SCAN_DATA_NOTWORKED;
        scan_data_protect(sd);
        if(!queue.alloc_entry(DISCRETE_DLEN,(LPBYTE)sd))
           {
            if(tapplication::get_log_level()>0)
                tapplication::write_syslog(LOG_ALERT,"Discrete data: input queue full. force add .sequence %Lu\n",(u_int64_t)ptr->seq_number);
            queue.alloc_entry(DISCRETE_DLEN,(LPBYTE)sd,true);
           }
        ++ptr;
    }
  }
  catch(...)
   {
     tapplication::write_syslog(LOG_ALERT,"Exception in dev_poller::handle_read_discrete\n");
     exit(CHILD_NEED_WORK);
   }

}

void      tdev_poller::refresh_discrete    ()
{
  tdevices_list::iterator ptr = d_input.begin();
  tdevices_list::iterator end = d_input.end();
  while(ptr<end)
  {
      ioctl(ptr->first,DIC120_IOC_REREAD);
      ++ptr;
  }
}




void tdev_poller::handle_read_analog(int dev_num, char * buf, int blen)
{
 try
 {


    BYTE tmp_buf[ANALOG_DLEN];
    lpcpc152scan_data sd = (lpcpc152scan_data)tmp_buf;
    sd->dev_num    = dev_num&CPC152SCAN_DATA_DEVNUM_MASK;

    lpaic124_input ptr = (lpaic124_input)buf;
    int count = blen/sizeof(*ptr);
    lpaic124_input eptr = ptr+count;
    u_int64_t prev_seq = 0;

    while(ptr<eptr)
    {

        int test_s0 = ptr->test_signals[0];
        //int test_s1 = ptr->test_signals[1];
        int gain = tapplication::get_config().dev_cfg.aic[dev_num].gain;
        if(gain)
           test_s0/=gain;
        bool worked  = test_s0 > 0x100   ;
        #ifdef _DEBUG
        worked = true;
        #endif

        if(prev_seq)
        {
            int delta = ptr->seq_number - prev_seq;
            if(delta > 1 && tapplication::get_log_level()>0)
              tapplication::write_syslog(LOG_ALERT,"Lost sequence count %d\n",delta);
        }
        sd->seq_number = ptr->seq_number;
        if(!worked)
           sd->dev_num|= CPC152SCAN_DATA_NOTWORKED;
        sd->dlen       = (ptr->data_len<<1);
        sd->group_num  = ptr->number;
        sd->tm_beg     = timeval2qword(ptr->tmstamp_start) ;
        sd->tm_end     = timeval2qword(ptr->tmstamp_end  ) ;
        memcpy(sd->data,ptr->data,sd->dlen);
        prev_seq = ptr->seq_number;
        scan_data_protect(sd);
        if(!queue.alloc_entry(ANALOG_DLEN,(LPBYTE)sd,false))
        {

            if(tapplication::get_log_level()>0)
               tapplication::write_syslog(LOG_ALERT,"Read queue full analog sequence %Lu\n",(u_int64_t)ptr->seq_number);

        }
        ++ptr;
    }
  }
 
   catch(...)
   {
     tapplication::write_syslog(LOG_ALERT,"Exception in dev_poller::handle_read_analog\n");
     exit(CHILD_NEED_WORK);
   }

}


void tdev_poller::dev_start(bool discrete,bool start)
{
  int command ;
  if(start)
    command = discrete ? DIC120_IOC_WORKSTART : AIC124_IOC_WORKSTART;
    else
    command = discrete ? DIC120_IOC_WORKSTOP : AIC124_IOC_WORKSTOP;

  tdevices_list * dev_list = discrete ? &d_input : &a_input;
  tdevices_list::iterator beg = dev_list->begin();
  tdevices_list::iterator end = dev_list->end();
  while(beg<end)
  {
      if(ioctl(beg->first,command,&beg->second))
         {
           tapplication::write_syslog(LOG_ALERT,"Error %s %s device fd = %d\n"
                                      ,start    ? "start"    : "stop"
                                      ,discrete ? "discrete" : "analog",beg->first
                                      );
         }
      ++beg;
  }

}


void tdev_poller::do_read      (bool discrete,int fd,int dev_num,char * buf,int bsz)
{
    int rd_len;
    do{
       rd_len = read(fd,buf,bsz);
       if(rd_len>0)
       {
          if(discrete)
           handle_read_discrete(dev_num,buf,rd_len);
          else
           handle_read_analog(dev_num,buf,rd_len);
           //printf("read %d bytes from %s device number %d\n",rd_len, discrete ? "discrete" : "analog",dev_num);
           //fflush(stdout);
       }
      }while(rd_len>0);

}

void tdev_poller::read_devices(bool discrete)
{
 try
 {
//    sched_param sp;
//    sp.__sched_priority = sched_get_priority_max(SCHED_FIFO);
//    if(!discrete)
//    {
//        sp.__sched_priority = 5;
//        pthread_setschedparam(pthread_self(),SCHED_FIFO,&sp);
//        pthread_setschedprio (pthread_self(),sp.__sched_priority);
//    }

    tdevices_list * dev_list = discrete ? &d_input : &a_input;


    //prepare fd_set
  if(dev_list->size())
  {

    int   rdb_buf_size = discrete ? 2048:65536;
    char* rd_buf      = new char[rdb_buf_size];
    dev_start(discrete,true);

    tdevices_list::iterator beg = dev_list->begin();
    tdevices_list::iterator end = dev_list->end();


    while(rd_buf && !this->terminate_check())
    {
        tdevices_list::iterator ptr = beg;
        int dev_num = 0;
        while(ptr<end)
        {
         do_read(discrete,ptr->first,dev_num++,rd_buf,rdb_buf_size);
         ++ptr;
          usleep( 1000 );
        }
    }
    dev_start(discrete,false);
    if(rd_buf) delete [] rd_buf;
    if(terminate_check())
        tapplication::write_syslog(LOG_INFO,"Terminate request fired\n" );
  }
    tapplication::write_syslog(LOG_INFO,"End polling %s devices\n",discrete ? "discrete":"analog" );
  }
    catch(...)
   {
     tapplication::write_syslog(LOG_ALERT,"Exception in dev_poller::read_devices\n");
     exit(CHILD_NEED_WORK);
   }

}


void tdev_poller::polling()
{
    //Start data handler
    terminate_request.store(0);
    this->server_->set_dev_poller(this);
    boost::thread   ard (boost::bind(&tdev_poller::read_devices,this,false));
    boost::thread   drd (boost::bind(&tdev_poller::read_devices,this,true ));
    handle_data();

    //Для остановки нити обработки

    //sotd.join();
    ard.join();
    drd.join();
    queue.drop_all();
    queue.get_event().fire(1);

}


}// end of namespace Fastwell
