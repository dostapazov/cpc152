#include "cpc152svc.hpp"
#include <gklib/otd_arch_proto.h>
#include <dirent.h>
#include <boost/algorithm/string.hpp>
#include <iomanip>
#include <fstream>
#include <limits.h>


namespace Fastwell
{



totd_data::totd_data(talarms_def & _alarms_def )
{
    alarms_def  = &_alarms_def;
    alarm_start = 0;
    alarm_end   = 0;
    alarm_file  = 0;
    max_alarm_data_size = ANALOG_DLEN;
    raw_server  =  nullptr;
}

totd_data::~totd_data()
{
    unique_locker l(this->get_mutex());
    release_pre_alarm();
}

void  totd_data::release_pre_alarm ()
{
    unique_locker l(this->get_mutex());
    pre_alarm_queue.drop_all();
    pre_alarm_queue.release_queue();
}

bool  totd_data::prepare_queues(DWORD min_scan_period,DWORD a_count,DWORD d_count)
{
    bool ret = true;

    if(alarms_def->pre_alarm || alarms_def->post_alarm)
       {

        int qsize = (alarms_def->pre_alarm+alarms_def->post_alarm);
        qsize/=min_scan_period;
        qsize*=a_count;
        qsize+=d_count*100;
        if(!pre_alarm_queue.setup_queue(qsize,max_alarm_data_size,true))
        {
            tapplication::write_syslog(LOG_ERR,"Erro create pre_alarm_queue qsize %d data-size %d\n",qsize,max_alarm_data_size);
            ret = false;
        }
       }

 return ret;
}




#ifdef _DEBUG
void print_bytes    (LPBYTE ptr,int len)
{
    auto ptr_end =    ptr+len;
    while(ptr<ptr_end)
    {
        printf("%02X ",(DWORD)*ptr);
        ++ptr;
    }
    printf("\n");
}

#endif







//void  totd_data::put_into_alarm   (sotd_proto & src,DWORD parts,QWORD timestamp)
//{

//    if(parts && src.op.addr)
//    {
//       //pre_alarm_queue.alloc_entry(0,0,0,src.op.proto_size,(LPBYTE)src.op.addr,timestamp,true);
//    }
//}

//bool otd_is_changed(otd_proto & op,int number)
//{
//    if(op.personal_chmask && OTD_CHECK_NUMBER(&op.personal_chmask->numbers,(WORD)number))
//    {
//        WORD val = 0;
//        otd_get_value(op.personal_chmask,number,&val,sizeof(val));
//        return val ? true:false;
//    }
//    return false;
//}

void totd_data::alarm_get_filename(string& fname,QWORD timestamp)
{
    fname = alarms_def->storage_path;
    char str[128];
    sprintf(str,"%Lu",timestamp);
    fname+=str;
}

string totd_data::alarm_get_filename(QWORD timestamp)
{
    string ret;
    alarm_get_filename(ret,timestamp);
    ret+=ALARM_SUFFIX;
    ret+=ALARM_EXTENSION;
    return ret;
}



bool totd_data::alarm_write(lpcpc152scan_data sd,WORD sz)
{
    if(sz)
    {
       alarm_wrb+= write(alarm_file,&sz,sizeof(sz));
       alarm_wrb+= write(alarm_file,sd,sz);
       return true;
    }
  return false;
}


bool  totd_data::alarm_check_end   (QWORD timestamp)
{
    unique_locker l(this->mut_alarm);
    if(alarm_file && timestamp<alarm_end)
    {
      return false;
    }
  return true;
}


bool is_alarm_fired(const lpcpc152scan_data sd,const talarm_condition & ac)
{
  //Check the alarm condition fired
  div_t dv = div(ac.param_num,8);
  BYTE  mask = (1<<dv.rem);
  LPBYTE data = sd->data;
  LPBYTE changes = data + P55_DATA_PORT_COUNT + dv.quot;

  if((*changes) & mask)
  {
    data+=dv.quot;
    int val = ((*data)&mask) ? 1:0;
    if(ac.more)
      return (val > ac.alarm_value ) ? true:false;
    else
      return (val < ac.alarm_value ) ? true:false;

  }
  return false;
}


void totd_data::alarm_check_start(const lpcpc152scan_data sd)
{
    bool need_activate = false;
    talarms::iterator lo_cond,hi_cond;
    unique_locker l(mut_data);
    int range_count = get_alarms_range(sd->dev_num&CPC152SCAN_DATA_DEVNUM_MASK,sd->group_num,USHRT_MAX,lo_cond,hi_cond);
    while(range_count && !need_activate && (lo_cond<hi_cond))
     {
       talarm_condition & ac = *lo_cond;
       need_activate = is_alarm_fired(sd,ac);
      ++lo_cond;
     }


    if(alarm_check_active())
    {
#ifndef _DEBUG

        if(need_activate)
        {
            unique_locker l(mut_alarm);
            alarm_end    = sd->tm_beg + (QWORD)MSEC_NS100(alarms_def->post_alarm);
        }
#endif
    }
    else
    {
        if(need_activate)
        {
            if(alarm_open_file(sd->tm_beg)>0)
            {
                boost::thread(boost::bind(&totd_data::alarm_write_thread,this)).detach();
            }
            else
              if(tapplication::get_log_level()>1)
                  tapplication::tapplication::write_syslog(LOG_ERR,"error create alarm_file\n");
        }
    }
}

bool totd_data::alarm_check_active()
{
  //unique_locker l(mut_alarm);
  return this->alarm_file>0 ? true : false;
}

void totd_data::alarm_write_thread()
{
 // Write alarm_data to file


    BYTE      buffer[250];
    tqentry * qe = (tqentry*)buffer;
    bool  end_alarm = false;
    QWORD alarm_beg = alarm_start - MSEC_NS100(alarms_def->pre_alarm);
    

    //First step write all data from prealarm_queue
    do{
        if(pre_alarm_queue.get_entry(qe,sizeof(buffer)))
        {
          lpcpc152scan_data sd = (lpcpc152scan_data)qe->data;

          if(sd->tm_beg > alarm_beg)
          {
            end_alarm = alarm_check_end(sd->tm_beg);
            if(!end_alarm)
               alarm_write(sd,qe->size);
          }
        }
        else
        {
          if(pre_alarm_queue.wait(1000))
              pre_alarm_queue.get_event().reset();
          else
              end_alarm = alarm_check_end(get_current_time());
        }
      }while(!end_alarm);


    alarm_close_file();


}


int totd_data::alarm_open_file(QWORD timestamp)
{
    string file_name;
    alarm_get_filename(file_name,timestamp);
    file_name+= ALARM_SUFFIX;
    if(tapplication::get_log_level()>1) tapplication::tapplication::write_syslog(LOG_INFO,"alarm activate \t %s\n",file_name.data());
    int res = open(file_name.c_str(),O_CREAT|O_RDWR,S_IRWXU|S_IRUSR);
    if(res>0)
       {
        unique_locker l(this->mut_alarm);
        alarm_file   = res;
        alarm_start  = timestamp;
        alarm_end    = alarm_start+(QWORD)MSEC_NS100(alarms_def->post_alarm);
       }

    alarm_wrb = 0;
    return res ;
}


int totd_data::alarm_close_file()
{
    //*Close alarm file and rename it to *.alarm.arch
    unique_locker l(this->mut_alarm);

    if(alarm_file)
    {
        QWORD save_alarm_start = alarm_start;
        DWORD save_alarm_wrb   = alarm_wrb;
        syncfs(alarm_file);
        close(alarm_file);

        alarm_start  = 0;
        alarm_end    = 0;
        alarm_file   = 0;
        alarm_wrb    = 0;

        string old_name;
        string new_name;
        alarm_get_filename(old_name,save_alarm_start);

        l.unlock();
        old_name+= ALARM_SUFFIX;
        new_name = old_name;
        new_name+= ALARM_EXTENSION;
        rename(old_name.c_str(),new_name.c_str());

        if(tapplication::get_log_level()>1) tapplication::tapplication::write_syslog(LOG_INFO,"done store alarm %d\n",(int)save_alarm_wrb);
        // Send notify about new alarm file
        if(raw_server)
            raw_server->new_alarm_notify(save_alarm_start);

        return 0;
    }
    return -1;
}

int   arch_filter(const dirent * de)
{
    if(de && de->d_type == DT_REG)
    {
        const char * ptr =  strstr(de->d_name,ALARM_EXTENSION);
        return ptr != NULL ? 1:0;
    }
    return 0;
}



int   totd_data::get_alarms_list(alarms_list & alist)
{

    dirent ** files = NULL;
    int count = scandir(alarms_def->storage_path,&files,arch_filter,alphasort);
   if(files && count)
   {
    for(int i = 0; i<count ; i++)
    {
        dirent *de = files[i] ;
        if(de)
        {
        char * end_ptr = NULL;
        alist.push_back(strtoull(de->d_name,&end_ptr,10));
        free(de);
       }
    }
    free(files);
   }
    return alist.size();
}


bool  totd_data::handle_alarm_command(talarm_condition & ac,bool erase)
{
    bool ret = false;
    char text[128];
    string log_str;
    if(erase)
    {
        unique_locker l(mut_data);
        talarms::iterator lo,hi,cptr;
        if(get_alarms_range(ac.dev_num,ac.grp_num,ac.param_num,lo,hi))
         {
            ret = true;
            log_str = "remove\n";
            cptr = lo;

            while(cptr<hi)
            {
               alarm_condition_get_text(*cptr,text,sizeof(text));
               log_str+=text;
               log_str+="\n";
               ++cptr;
            }
            alarms.erase(lo,hi);

         }

    }
    else
    {
      if(ac.param_num!=USHRT_MAX)
        {
          unique_locker l(mut_data);
          alarms<<ac;
          alarm_condition_get_text(ac,text,sizeof(text));
          log_str=text;
          ret = true;
        }
    }
    write_alarms_config(alarms_def->storage_path,&alarms);
    if(log_str.length())
        tapplication::tapplication::write_syslog(LOG_INFO,"%s\n",log_str.c_str());
    return ret;
}


int totd_data::get_alarms_range(WORD dev_num, WORD grp_num, WORD param_num, talarms::iterator & lo_cond, talarms::iterator & hi_cond)
{
    talarm_condition ac(dev_num,grp_num,param_num,false,0);
    if(dev_num   == USHRT_MAX)  ac.dev_num   = 0;
    if(grp_num   == USHRT_MAX)  ac.grp_num   = 0;
    if(param_num == USHRT_MAX)  ac.param_num = 0;

    lo_cond = alarms.begin();
    hi_cond = alarms.end();
    lo_cond = std::lower_bound(lo_cond,hi_cond,ac);
    ac.more = true;
    ac.dev_num   = dev_num;
    ac.grp_num   = grp_num;
    ac.param_num = param_num;
    hi_cond = std::upper_bound(lo_cond,hi_cond,ac);
    int ret = (int)std::distance(lo_cond,hi_cond);
    return ret;
}


void totd_data::print_alarms_info()
{
  if(tapplication::get_log_level()>2)
  {
    int asz = alarms.size();
    if(asz)
    {
        tapplication::write_syslog(LOG_INFO,"Count of alarms %d\n",asz);
        talarms::iterator lo = alarms.begin();
        talarms::iterator hi = alarms.end  ();
        char text[128];
        while(lo<hi)
        {
            alarm_condition_get_text(*lo,text,sizeof(text));
            tapplication::write_syslog(LOG_INFO,"%s\n",text);
            ++lo;
        }
    } else
        tapplication::write_syslog(LOG_INFO,"No alarms defined\n");
  }
}

void totd_data::read_alarms()
{
    unique_locker l(get_mutex());
    read_alarms  (alarms_def->storage_path,&alarms);
    //std::for_each(alarms.begin(),alarms.end(),[this ](talarm_condition & ac) {
    //    ac.param.addr.cp = cp_number;
    //});
}


#define  alarm_conf "alarms.conf"
void totd_data::read_alarms(const string & alarms_path, talarms * alarms )
{
    string alarms_file = alarms_path;
    alarms_file+= alarm_conf;
    std::ifstream src(alarms_file);
    if(src.is_open())
    {
        char buffer[1024];
        while(!src.eof())
        {
            src.getline(buffer,sizeof(buffer));
            string str(buffer);
            trim(str);
            if(str.length() && *str.c_str()!='#')
            {
                int pos = str.find('=');
                if(pos>0)
                {
                    string alarm_start = str.substr(pos+1);
                    parse_alarm_start(alarm_start,*alarms);
                }
            }
        }
        src.close();
    }
    else
    tapplication::write_syslog(LOG_ALERT,"Error open alarms defines %s\n",alarms_file.c_str());

}


void totd_data::write_alarms_config(const string & alarms_path, talarms *alarms)
{
    string alarms_file = alarms_path;
    alarms_file+= alarm_conf;
    std::ofstream dst;
    dst.open(alarms_file.c_str());
    if(dst.is_open())
    {
        dst<<"# defines variable alarms configuration"<<endl;
        char text[80];
        const char * cmd = "--alarm-start";
        talarms::iterator beg =  alarms->begin(),end = alarms->end();
        while(beg<end)
        {
            talarm_condition & ac = *beg;
            snprintf(text,sizeof(text),"%s=%d-%d-%d%c0x%X"
                      ,cmd,ac.dev_num,ac.grp_num,ac.param_num
                      ,ac.more ? '>':'<',ac.alarm_value);
#ifdef _DEBUG
            tapplication::write_syslog(LOG_INFO,"write %s\n",text);
#endif
            dst<<text<<endl;
            ++beg;
        }
    }
}

DWORD totd_data::handle(lpcpc152scan_data sd,DWORD sz)
{
    pre_alarm_queue.alloc_alarm_entry(sz,(LPBYTE)sd);
    if(sd->dev_num&CPC152SCAN_DATA_DISCRETE)
    {
      alarm_check_start(sd);
    }
    return 0;
}


}//end of namespace


