#include "cpc152svc.hpp"

namespace Fastwell
{

 tnet_session * traw_cpc152_server::create_session()
 {
    return new traw_cpc152_session(this,data_,frame_size);
 }

 void           traw_cpc152_server::handle_accept(tnet_session::shared_ptr ns, system::error_code ec)
 {
     tserver::handle_accept(ns,ec);
     //start_accept();
 }

 void traw_cpc152_server::send(cpc152scan_data * sd)
 {
     unique_locker l(mut);
     session_list::iterator ptr = sessions.begin(),end = sessions.end();
     while(ptr!=end)
     {
         traw_cpc152_session * s = dynamic_cast<traw_cpc152_session *>(ptr->second.get());
         if(s)
           s->do_send(sd);
         ++ptr;
     }
 }

void traw_cpc152_server::new_alarm_notify(QWORD alarm_id)
{
    unique_locker l(mut);
    session_list::iterator ptr = sessions.begin(),end = sessions.end();
    while(ptr!=end)
    {
        traw_cpc152_session * s = dynamic_cast<traw_cpc152_session *>(ptr->second.get());
        if(s)
           s->new_alarm_notify(alarm_id);
        ++ptr;
    }
}

 traw_cpc152_session::~traw_cpc152_session()
 {
     tapplication::write_syslog(LOG_INFO,"Destructor traw_cpc152_session\n");
     delete [] pre_send_buffer;
     delete [] tmp_send_buffer;
     send_queue.release_queue();
 }

 void traw_cpc152_session::init_session()
 {

   send_queue.setup_queue(64,frame_size);
   pre_send_buffer = new BYTE[frame_size];
   tmp_send_buffer = new BYTE[frame_size];
   send_mph = (LPMPROTO_HEADER)pre_send_buffer;
   memset(send_mph,0,sizeof(*send_mph));
   send_mph->fa       = FA_DEVICE_RAW_CPC152;
   send_mph->internal = MP_INTERNAL_DEFAULT;
   cpc152hdr = (lpcpc152proto_hdr)send_mph->data;
   cpc152hdr->fa       = CPC152PROTO_FA_SCAN_DATA;
   cpc152hdr->count    = 0;
   analog_scan         = 0;
   send_mph->data_size = sizeof(*cpc152hdr) - sizeof(cpc152hdr->data[0]);
   packet_num.store(0);
   send_busy.store(false);
 }



 void traw_cpc152_session::start_session  ()
 {
   session_id = owner->add_session(this);
   if(tapplication::get_log_level()>2)
       tapplication::write_syslog(LOG_INFO,"start CPC152 RAW SERVER session id %d\n",session_id);
    tnet_session::start_session();
    connection_time      = 0;
    timer_count          = 0;
    timer_request_tmsync = 0;
    total_write          = 0;
    total_read           = 0;
    analog_scan          = 0;
    start_timer(&timer,1000);
 }

 void traw_cpc152_session::stop_session   ()
 {
   if(session_id)
   {
     if(tapplication::get_log_level()>2)
         tapplication::write_syslog(LOG_INFO,"stop CPC152 RAW SERVER session id %d connection time %ld sec.\n",session_id,connection_time);
     owner->remove_session(session_id);
     timer.cancel();
     tnet_session::stop_session();
     session_id = 0;
     owner->start_accept();
     //usleep(100000);
    }
 }

 void traw_cpc152_session::handle_modem_proto()
 {
  sync();
  DWORD mp_len = mpb.get_mproto_len();
  do {
      if(mp_len)
      {
          rxb.reserve(mp_len);
          mpb.peek_mproto(rxb.begin(),mp_len,true);
          LPMPROTO_HEADER mph = (LPMPROTO_HEADER)rxb.begin();
          handle_modem_proto(mph);
          rxb.clear();
      }
      sync();
      mp_len = mpb.get_mproto_len();
  } while(mp_len);

 }

 void traw_cpc152_session::handle_modem_proto(LPMPROTO_HEADER mph)
 {
   if(mproto_check(mph))
   {
     if(mph->fa == FA_DEVICE_RAW_CPC152)
    {
      lpcpc152proto_hdr phdr = (lpcpc152proto_hdr)mph->data;
      bool is_respond   = (phdr->fa&CPC152PROTO_FA_FLAG_RESPOND) ? true : false;
      bool is_end       = (phdr->fa&CPC152PROTO_FA_FLAG_END    ) ? true : false;
      bool is_erase     = (phdr->fa&CPC152PROTO_FA_FLAG_ERASE  ) ? true : false;
      switch(phdr->fa&CPC152PROTO_FA_MASK)
      {
        case CPC152PROTO_FA_ADD_CONDITION :
         handle_alarm_condition(phdr,is_erase,is_end);
         break;
       case CPC152PROTO_FA_SYNCTIME:
          handle_time_sync(phdr,is_respond,is_end);
       break;
       case CPC152PROTO_FA_GET_ALARMS_LIST:
          handle_get_alarms_list();
       break;
      case CPC152PROTO_FA_ERASE_ALARM_FILE:
          is_erase = true;
      case CPC152PROTO_FA_GET_ALARM_FILE:
         handle_get_alarm_file(phdr,is_respond,is_erase,is_end);
      break;
       case CPC152PROTO_FA_REQUEST_DISCRETE:
         request_discrete();
       break;
      }
    }
   timer_count = 0;
   }
    else
   {
     if(tapplication::get_log_level()>5)
           tapplication::write_syslog(LOG_ERR,"check summ errot MPROTO_HEADER\n");
   }
 }

 void traw_cpc152_session::handle_alarm_condition(lpcpc152proto_hdr phdr,bool is_erase,bool )
 {
   talarm_condition * ac = (talarm_condition *)phdr->data;
   talarm_condition * ac_end = ac+phdr->count;
   while(ac<ac_end)
   {
     data_->handle_alarm_command(*ac,is_erase);
       ++ac;
   }
 }

 void traw_cpc152_session::handle_time_sync(lpcpc152proto_hdr phdr, bool is_respond, bool)
 {
   if(is_respond && phdr->count == 2)
   {
     LPQWORD  qptr      = (LPQWORD)phdr->data;
     QWORD    curr_time = get_current_time();
     int      time_delta     = NS100_MSEC(abs((int)(qptr[0] - qptr[1])));
    if(tapplication::get_log_level()>5)
      {
         char txt[128];
         sprintf(txt,"Time delta %d\n",time_delta);
         tapplication::write_syslog(LOG_DEBUG,txt);
      }
     if(abs(time_delta)>1000)
     {
        QWORD     deliver_time = curr_time - qptr[0];
        QWORD     new_time     = qptr[1]+(deliver_time>>1);
        timeval tv;
        filetime2timeval(new_time,tv);
        if(settimeofday(&tv,NULL))
         {
            if(tapplication::get_log_level()>5)
              tapplication::write_syslog(LOG_DEBUG,"Error settimeofday\n");
         }
        else
         {
            if(tapplication::get_log_level()>5)
              tapplication::write_syslog(LOG_INFO,"Time synchronized. Difference was %d\n",time_delta);
         }

     }
   }
 }

 void traw_cpc152_session::sync()
 {
     DWORD offs = mpb.sync_internal(MP_INTERNAL_DEFAULT);
     while(offs)
     {
         if(tapplication::get_log_level()>2) tapplication::write_syslog(LOG_ERR,"OTD synchronozation %ud\n",offs);
         offs = mpb.sync_internal(MP_INTERNAL_DEFAULT);
     }
 }



 void traw_cpc152_session::handle_read   (system::error_code  ec,int rd_bytes)
 {
     if(!ec && rd_bytes)
     {
         total_read+=rd_bytes;
         do {
             mpb.add(read_buffer,rd_bytes);
             rd_bytes = socket_.available();
             if(rd_bytes)
                 socket_.read_some(as::buffer(read_buffer,sizeof(rd_bytes)),ec);
         } while(!ec && rd_bytes);
         rd_len = 0;
         if(!ec)
         {
             handle_modem_proto();
             timer_count = 0;
         }
         start_read();
     }
     else
         tnet_session::handle_read(ec,rd_bytes);

 }

 void traw_cpc152_session::handle_timer  (as::deadline_timer * t,int msec,system::error_code  ec)
 {
     if(!ec)
     {
         ++connection_time;
         if(++timer_count>5)
         {
             if(tapplication::get_log_level()>2)
                 tapplication::write_syslog(LOG_INFO,"Modbus activity timer expired\n");
             stop_session();
             timer_count = 0;
         }
         else
         {
             start_timer(t,msec);
             if(0>--timer_request_tmsync)
             {
                request_time_sync();
                timer_request_tmsync = 60;
             }
         }
      }
 }

 void traw_cpc152_session::start_read    ()
 {
   tnet_session::start_read();
 }

 void  traw_cpc152_session::handle_write (system::error_code  ec,int wr_bytes)
 {
   if(!ec)
   {
    total_write+=wr_bytes;
    if(wr_bytes)
    send_queue.release_entry();

    if(send_queue.get_queue_count())
     {
       send_busy.store(true);
       tqentry * qe = send_queue.get_entry();
       LPMPROTO_HEADER mph = (LPMPROTO_HEADER)qe->data;
       mph->internal = MP_INTERNAL_DEFAULT;
       mph->pkt_num = atomic_fetch_add(&packet_num,1);
       mproto_protect(mph);
       wr_bytes = qe->size;
       send_async(mph,mproto_size(mph));
     }
     else
        send_busy.store(false);
   }
   else
   {
     //transfer error. Disconnect;
     stop_session();
   }

 }

 bool traw_cpc152_session::do_send        (lpcpc152proto_hdr hdr, int sz)
 {
     LPMPROTO_HEADER mph = (LPMPROTO_HEADER)tmp_send_buffer;
     int need_sz = sizeof(*mph)-sizeof(mph->data[0])+sz;
     if(need_sz<=frame_size)
     {
       mproto_init(mph,0,FA_DEVICE_RAW_CPC152,sz);
       memcpy(mph->data,hdr,sz);
       do_send(mph);
       return true;
     }
     return false;
 }

 void traw_cpc152_session::do_send(LPMPROTO_HEADER mph)
 {
     if(!send_queue.alloc_entry(mproto_size(mph),(LPBYTE)mph))
     {
       if(tapplication::get_log_level()>5)
           tapplication::write_syslog(LOG_ERR,"overflow cpc152 server queue\n");
     }
     else
     {
         bool cmp = false;
         if(send_busy.compare_exchange_strong(cmp,true))
          {
            system::error_code  ec;
            handle_write(ec,0);
          }
     }
 }


 void traw_cpc152_session::do_send(lpcpc152scan_data sd)
 {
   int dlen = cpc152scan_data_get_size(*sd);
   dlen+=sizeof(WORD); // kpk
   if(int(mproto_size(send_mph)+dlen)<=frame_size && analog_scan<128)
    {
       LPBYTE ptr = (LPBYTE)send_mph->data;
       ptr+=send_mph->data_size;
       memcpy(ptr,sd,dlen);
       send_mph->data_size+=dlen;
       ++cpc152hdr->count;
       if(!(sd->dev_num&CPC152SCAN_DATA_DISCRETE))
           ++analog_scan ;
    }
    else
    {

     do_send(send_mph);
     cpc152hdr->count    = 0;
     send_mph->data_size = sizeof(*cpc152hdr) - sizeof(cpc152hdr->data[0]);
     analog_scan = 0;
     do_send(sd);
    }
 }

 void traw_cpc152_session::request_time_sync()
 {
   //Request for time synchronization
   BYTE    buf[128];
   lpcpc152proto_hdr ph = (lpcpc152proto_hdr) buf;
   ph->count = 2;
   ph->fa = CPC152PROTO_FA_SYNCTIME;
   LPQWORD pqw =  (LPQWORD)ph->data;
          *pqw = get_current_time();
         ++pqw;
         *pqw = 0;
   do_send(ph,cpc152proto_hdr_calc_size(2*sizeof(*pqw)));

 }

 void traw_cpc152_session::new_alarm_notify(QWORD alarm_id )
 {
   // Notify new alarm
     BYTE    buf[128];
     lpcpc152proto_hdr ph = (lpcpc152proto_hdr) buf;
     ph->count = 1;
     ph->fa = CPC152PROTO_FA_NEW_ALARM | CPC152PROTO_FA_FLAG_END;
     LPQWORD pqw =  (LPQWORD)ph->data;
            *pqw = alarm_id;
     do_send(ph,cpc152proto_hdr_calc_size(sizeof(*pqw)));
 }


 void traw_cpc152_session::do_send_alarm_file(QWORD alarm_id,bool send,bool  erase)
 {
     std::string alarm_file_name = data_->alarm_get_filename(alarm_id);
     int alarm_fd = open(alarm_file_name.c_str(),O_RDWR);
     if(alarm_fd>0)
     {
      as::deadline_timer timer(get_service());
      posix_time::millisec dt = posix_time::millisec(1);
      system::error_code ec;
      DWORD queue_limit = send_queue.get_queue_size()-16;

      std::unique_ptr<BYTE>  bptr(new BYTE[frame_size]);
      LPMPROTO_HEADER   mph  = (LPMPROTO_HEADER)bptr.get();
      lpcpc152proto_hdr phdr = (lpcpc152proto_hdr)mph->data;
      phdr->count = 0;
      phdr->fa    = CPC152PROTO_FA_FLAG_RESPOND;
      if(send)
         phdr->fa |=CPC152PROTO_FA_GET_ALARM_FILE;
      if(erase)
          phdr->fa |= CPC152PROTO_FA_ERASE_ALARM_FILE;

      lpcpc152alarm_file_content file_content = (lpcpc152alarm_file_content)phdr->data;
      file_content->alarm_id = alarm_id;
      int  beg_size = sizeof(*phdr) - sizeof(phdr->data[0])+sizeof(*file_content)-sizeof(file_content->data[0]);
      mproto_init(mph,0,FA_DEVICE_RAW_CPC152,beg_size);
      int chunk_size   = frame_size - sizeof(WORD)  -  mproto_size(mph);
      int curr_size    = 0;
      LPBYTE chunk_ptr = file_content->data;
      WORD rd_bytes = 0;
      WORD sd_size  = 0;
      if(send)
      {
      rd_bytes =  read(alarm_fd,&sd_size,sizeof(sd_size));

      do{

           if(rd_bytes && (curr_size+sd_size)<chunk_size)
           {
             lpcpc152scan_data sd = (lpcpc152scan_data)chunk_ptr;
             rd_bytes = read(alarm_fd,sd,sd_size);
             chunk_ptr+= rd_bytes;
             curr_size+= rd_bytes;
             mph->data_size+=rd_bytes;
             ++phdr->count;
             sd_size  = 0;
             rd_bytes =  read(alarm_fd,&sd_size,sizeof(sd_size));
           }
           else
           {
            //Send chunk

            *((LPWORD)chunk_ptr)= calc_kpk(mph->data,mph->data_size,OTD_DEF_POLINOM);
             mph->data_size+=sizeof(WORD);

             while(session_id && send_queue.get_queue_count() > queue_limit )
             {
               timer.expires_from_now(dt);
               timer.wait(ec);
             }
             if(session_id)
             {
              do_send(mph);
              chunk_ptr   = file_content->data;
              phdr->count = 0;
              mph->data_size = beg_size;
              curr_size      = 0;
             }
           }


        }while(session_id && rd_bytes);
      }

      close(alarm_fd);
      if(session_id)
      {
        phdr->fa |= CPC152PROTO_FA_FLAG_END;
        *((LPWORD)chunk_ptr)= calc_kpk(mph->data,mph->data_size,OTD_DEF_POLINOM);
         mph->data_size+=sizeof(WORD);
         do_send(mph);

      if(erase)
          unlink(alarm_file_name.c_str());
      }
     }

 }

 //863 2108080 nissan to

 void traw_cpc152_session::do_send_alarm_files(int count,boost::shared_ptr<QWORD> qptr,bool send,bool erase)
 {
   LPQWORD alarms_id = qptr.get();
   for(int i = 0;i<count;i++)
      {
       do_send_alarm_file(alarms_id[i],send,erase);
      }
 }

 void traw_cpc152_session::handle_get_alarm_file(lpcpc152proto_hdr phdr,bool is_respond, bool is_erase, bool /*is_end*/)
 {
   if(!is_respond)
   {
      boost::shared_ptr<QWORD> qptr(new QWORD[phdr->count]);
      memcpy(qptr.get(),phdr->data,sizeof(QWORD)*phdr->count);
      bool send = (phdr->fa &CPC152PROTO_FA_GET_ALARM_FILE) ? true : false;
      boost::thread(boost::bind(&traw_cpc152_session::do_send_alarm_files,this,phdr->count,qptr,send,is_erase)).detach();
   }
 }

 void traw_cpc152_session::do_send_alarms_list()
 {
     as::deadline_timer timer(get_service());
     posix_time::millisec dt = posix_time::millisec(1);
     system::error_code ec;
     DWORD queue_limit = send_queue.get_queue_size()-16;

     std::unique_ptr<BYTE>  bptr(new BYTE[frame_size]);
     LPMPROTO_HEADER   mph  = (LPMPROTO_HEADER)bptr.get();
     lpcpc152proto_hdr phdr = (lpcpc152proto_hdr)mph->data;
     phdr->fa               = CPC152PROTO_FA_GET_ALARMS_LIST|CPC152PROTO_FA_FLAG_RESPOND;
     int beg_size      = sizeof(*phdr)-sizeof(phdr->data[0]);
     mproto_init(mph,0,FA_DEVICE_RAW_CPC152,beg_size);
     std::unique_ptr<totd_data::alarms_list>  al ( new totd_data::alarms_list);

     if(data_->get_alarms_list(*al))
     {
        totd_data::alarms_list::iterator abeg = al->begin(),aend = al->end();
        LPQWORD alarms_ptr = (LPQWORD)phdr->data;
        phdr->count = 0;
        while(abeg<aend && session_id)
        {
         if(int(mproto_size(mph)+sizeof(QWORD)) <= frame_size)
         {
           *alarms_ptr  = *abeg;
           mph->data_size+=sizeof(*alarms_ptr);
           ++phdr->count;
           ++alarms_ptr;
         }
         else
         {
             while(session_id && send_queue.get_queue_count()>queue_limit)
             {
                 timer.expires_from_now(dt);
                 timer.wait(ec);
             }
             do_send(mph);
             alarms_ptr     = (LPQWORD)phdr->data;
             mph->data_size = beg_size;
             phdr->count    = 0;

         }
         ++abeg;
        }
        phdr->fa|=CPC152PROTO_FA_FLAG_END;
        if(session_id)
           do_send(mph);
        al->clear();
     }
 }

 void traw_cpc152_session::handle_get_alarms_list()
 {
   boost::thread(boost::bind(&traw_cpc152_session::do_send_alarms_list,this)).detach();
 }

  void traw_cpc152_session::request_discrete()
  {

      traw_cpc152_server * srv = dynamic_cast<traw_cpc152_server *>(owner);
      if(srv)
       {
          tdev_poller * dp = nullptr;
          dp = srv->get_dev_poller() ;
          if(dp) dp->refresh_discrete();
       }
  }




}// end of namespace Fastwell
