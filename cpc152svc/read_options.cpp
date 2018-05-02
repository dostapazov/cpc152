
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "read_options.hpp"

using namespace boost::algorithm;

namespace Fastwell
{


tread_options::tread_options()
{

}

void tread_options::print_help(const char ** ptr)
{
    const char * cmd_ptr;
    const char * descr;
    do
    {
        cmd_ptr = *ptr;
        ++ptr;
        descr   = *ptr;
        ++ptr;
        if(*cmd_ptr == '-')
            cout<<setw(16)<<cmd_ptr<< ' ' <<descr<<endl;
        else
            cout<<"\t"<<cmd_ptr<<setw(12)<<"\t"<<setw(18)<<descr<<setw(0)<<endl;
    } while(*cmd_ptr);

}

#define CMD_HELP              1
#define CMD_CONFIG            2
#define CMD_SRV_PORT          3
#define CMD_SRV_FRAME_SIZE    4

#define CMD_DIC120            5
#define CMD_AIC124            6
#define CMD_ALARM_STORAGE     7
#define CMD_ALARM_TIMES       8
#define CMD_ALARM_START       9
#define CMD_LOG_LEVEL         10
#define CMD_RUNAS_DAEMON      11
#define CMD_DEV_POLLER        12
#define CMD_WATCH_DOG_PORT    13
#define CMD_DAC_PARAM         14





const char *tread_options::commands []
{
    /*index  0*/  "usage otdserver [opt] --config=device_config_file",""
    /*       1*/  ,"--help"       ,"\tshow this help"
    /*       2*/  ,"--config"     ,"\tpath to device config file"
    /*       3*/  ,"--srv-port"   ,"\tserver port   [default 45777 ]"
    /*       4*/  ,"--srv-frame-size","\tframe size   [default 4096 bytes ]"
                   "\n\tdevices_list\n"
    /*       5*/  ,"--dic120","\tdic_120 device name,debounce value  and pga for example --dic120=/dev/dic120_0:3:1500-0-1-2"
                   "\n\t\t/dev/dic120_0 device with 2ms debounce,1500 ms timer period and pga NN 0,1,2 "
    /*       6*/  ,"--aic124","\taic124 device and channels for example --aic124=/dev/aic124_0:2:2-0:1-1:0"
                   "\n\t\t/dev/aic124_0 device with gain=2 and scan period 2 ms \n\tand enable channel 0 mode SINGLE  and channel 2 mode DIFFERENTIAL"

                   "\n\nalarm store system"
    /*       7*/ ,"--alarm-storage","\t place where system was write alarm data\n\t\t if not defined than alarm system not active"
    /*       8*/ ,"--alarm-times","\t[=5:100] first value time before alarm, second value - after alarm"
    /*       9*/ ,"--alarm-start","\td-0-1>1(discrete switch on),a-1-23>0x1FF0 (analog value more than 0x1FF0)"
    /*       10*/ ,"--log-level","\tsetup log level"
    /*       11*/ ,"--daemon","\trun as daemon . config file setup /etc/otdserv.conf"
    /*       12*/ ,"--dev-poller","\tqueue size default 4096"

    /*       13*/ ,"--watchdog-enable","\tenable watchdog timer. default disabled"
    /*       14*/ ,"--dac-param","\t dac parameters  number,freq,amplitude,sinus or cosinus and inverse negative [0-50-0x1FF-1-1] "
    ,"\nComment : timer period in mSec depends on HZ value of Linux kernel","\nWritten by Ostapenko D.V. Azov 2016 dostapazov@gmail.com under contract with ComPa(St.Peterburg)"
    ,"",""
};




void tread_options::init_cmd_map(const char ** ptr)
{
    if(!cmd_map.size())
    {
        const char * cmd_ptr;
        int idx = 0;
        do
        {
            cmd_ptr = *ptr;
            ++ptr;
            ++ptr;
            if(*cmd_ptr == '-')
                cmd_map[string(cmd_ptr)] = idx;
            ++idx;
        } while(*cmd_ptr);
    }
}


void tread_options::get_cmd_key(const char * ptr,key_value & kv)
{
    kv.first = string();
    kv.second = kv.first;

    static string cmd_prefix ("--");
    static string cmd_suffix ("=" );
    string src(ptr);
    int endp  = src.find(cmd_suffix);
    int begp = src.find(cmd_prefix);
    if(begp>=0)
    {

        if(endp>0)
        {
            if(begp>=0 && begp<endp)
            {
                kv.first  = string(src,begp,endp-begp);
                trim(kv.first);
                kv.second = string(src,endp+1,src.length()-endp-1);
                trim(kv.second);
            }
        }
        else
            kv.first = string(src,begp);
    }

}


void parse_dac_param(app_config & cfg,string str)
{

    vector<string> values;
    split(values,str,is_any_of("-"));
    if(values.size()>0)
     {
       int dac_number = str2int(values[0]);
       tdac_param * dp = cfg.dac_param;
       if(dac_number) ++dp;
       if(values.size()>1)
          dp->freq = str2int(values[1]);
       if(values.size()>2)
          dp->ampl = str2int(values[2]);
       if(values.size()>3)
          dp->is_sinus = str2int(values[3]) ? true: false;
       if(values.size()>4)
          dp->inv_neg = str2int(values[4]) ? true: false;
     }
}

// --dic120=/dev/dic120_0:3-1-2-3-4

bool parse_dic_options(string value,device_config & dev_cfg)
{
    vector<string> dics;
    vector<string> vals;
    vector<string> name_bounce;
    split(dics,value,is_any_of(","));
    int  dic_cnt = dics.size();
    for(int i = 0; i<dic_cnt && dev_cfg.dic_count<CPC152_MAX_SLOTS; i++)
    {
        int  idx = dev_cfg.dic_count++;

        split(vals,dics[i],is_any_of("-"));
        split(name_bounce,vals[0],is_any_of(":"));
        strcpy(dev_cfg.dic[idx].dev_name,name_bounce[0].c_str());
        if(name_bounce.size()>1)
            dev_cfg.dic[idx].debounce = str2int(name_bounce[1]);
        else
            dev_cfg.dic[idx].debounce = P55_DEBOUNCE_4MS;
        if(name_bounce.size()>2)
            dev_cfg.dic[idx].scan_freq =  str2int(name_bounce[2]);
        else
            dev_cfg.dic[idx].scan_freq = 1000;
        memset(dev_cfg.dic[idx].pga,0,sizeof(dev_cfg.dic[idx].pga));
        for(int pga = 1; pga<(int)vals.size(); pga++)
        {
            int pga_num = str2int(vals[pga]);
            if(pga_num<DIC120_PGA_COUNT)
                dev_cfg.dic[idx].pga[pga_num] = 1;
        }

    }

    return dic_cnt? true : false;
}

/*--aic124=0x210:2:500-1:1-2:1-3:0-4:0*/
bool parse_aic_options(string value,device_config & dev_cfg)
{
    vector<string> aics;
    split(aics,value,is_any_of(","));
    int  aic_cnt = aics.size();

    for(int i = 0; i<aic_cnt && dev_cfg.aic_count<CPC152_MAX_SLOTS; i++)
    {
        int aic_idx = dev_cfg.aic_count++;
        vector<string> params;
        vector<string> port_gain_freq;
        split(params,aics[i],is_any_of("-"));
        split(port_gain_freq,params[0],is_any_of(":"));
        strcpy(dev_cfg.aic[aic_idx].dev_name , port_gain_freq[0].c_str());
        dev_cfg.aic[aic_idx].gain = 0;
        dev_cfg.aic[aic_idx].scan_period = 500;

        if(port_gain_freq.size()>1) //Setup gain
            dev_cfg.aic[aic_idx].gain      = str2int(port_gain_freq[1]);
        if(port_gain_freq.size()>2) //Setup scan frequency
            dev_cfg.aic[aic_idx].scan_period = str2int(port_gain_freq[2]);

        for(int j = 1; j<(int)params.size(); j++)
        {
            vector<string> vals;
            split(vals,params[j],is_any_of(":"));
            int range = str2int(vals[0]);
            if(range<AIC124_MUX_SINGLE_CHANNELS_COUNT)
            {
                dev_cfg.aic[aic_idx].channels[range]=1;
                dev_cfg.aic[aic_idx].modes[range] = vals.size()>1 ? str2int(vals[1]) : 1;
            }

        }

    }

    return aic_cnt ? true:false;
}


void parse_alarm_times(string value ,talarms_def & alarms)
{
    vector<string>   s;
    split(s,value,is_any_of(":"));
    for(int i = 0; i<2 && i<(int)s.size(); i++)
    {
        switch(i)
        {
        case 0:
            alarms.pre_alarm  = str2int(s.at(i));
            break;
        case 1:
            alarms.post_alarm = str2int(s.at(i));
            break;
        }
    }
}



int tread_options::operator()(int ac, char **av, app_config &cfg )
{
    init_cmd_map(commands);
    int ret = -1;
    for(int i = 1; i<ac; i++)
    {
        char * arg = av[i];
        key_value kv;
        get_cmd_key(arg,kv);
        std::map<string,int>::iterator cmd_curr = cmd_map.find(kv.first);

        if(cmd_curr!=cmd_map.end())
        {
            switch(cmd_curr->second)
            {
            case CMD_HELP         :
                print_help(commands);
                return EFAULT;
                break;
            case CMD_CONFIG       :
                return read_config(kv.second,cfg);
                return -1;
                break;
            case CMD_SRV_PORT     : {
                cfg.srv_cfg.server_port    = str2int(kv.second);
                if(cfg.srv_cfg.server_port<1)
                     cfg.srv_cfg.server_port = 45777;
                ret = 0;
            }
            break;
            case CMD_SRV_FRAME_SIZE:
                cfg.srv_cfg.frame_size  = str2int(kv.second);
                if(cfg.srv_cfg.frame_size<1024)
                    cfg.srv_cfg.frame_size = 1024;
                break;
            case CMD_DIC120       :
                if(parse_dic_options(kv.second,cfg.dev_cfg))
                    ret = 0;
                break;
            case CMD_AIC124       :
                if( parse_aic_options(kv.second,cfg.dev_cfg))
                    ret = 0;
                break;
            case CMD_ALARM_STORAGE:
            {
                strncpy(cfg.alarms.storage_path , kv.second.c_str(),sizeof(cfg.alarms.storage_path));
                if(strlen(cfg.alarms.storage_path))
                    strcat(cfg.alarms.storage_path,"/");
            }
            break;
            case CMD_ALARM_TIMES  :
                parse_alarm_times(kv.second,cfg.alarms);
                break;
            case CMD_LOG_LEVEL    :
                cfg.log_level = str2int(kv.second);
                break;
            case CMD_RUNAS_DAEMON :
                if(!cfg.daemon)
                {
                    cfg.daemon = 1; //read_config(tapplication::config_file,cfg);
                }
                return 0;
            case CMD_DEV_POLLER        : cfg.devp_qsize = std::max(4096,str2int(kv.second));
            break;

            case CMD_WATCH_DOG_PORT   : cfg.watch_dog_enabled  = str2int(kv.second);
            break;

            case CMD_DAC_PARAM : parse_dac_param(cfg,kv.second);
                break;
            default:
            break;


            }
        }
        else
            printf("warning : unknown command argument %s\n",*av);


    }

    if(ret)
    {
        print_help(commands);
    }
    cmd_map.clear();
    return ret;
}




int file2argv(const string & cfg_file,std::vector<char*> &argv)
{
    argv.push_back(NULL);
    int argc = 1;
    std::ifstream src(cfg_file);
    if(src.is_open())
    {
        char buffer[1024];
        while(argc<80 && !src.eof())
        {
            src.getline(buffer,sizeof(buffer));
            string str(buffer);
            trim(str);
            if(str.length() && *str.c_str()!='#')
            {
                char * ptr = new char[str.length()+1];
                strcpy(ptr,str.c_str());
                argv.push_back(ptr);
                ++argc;
            }
        }
        src.close();
    }

    return argc;

}



int tread_options::read_config(const string & _cfg_file
                               , app_config &cfg
                              )
{

    std::vector<char*> argv;
    string cfg_file = _cfg_file;
    file2argv(cfg_file,argv);
    int ret = (*this)((int)argv.size(),(char**)argv.begin().base(),cfg);
    for_each(argv.begin(),argv.end(),[](char * & ptr) {
        if(ptr) delete[] ptr;
    });
    argv.clear();
    return ret;
}

}// end of namespace Fastwell


extern "C" int read_options(int argc,char **  argv,void  * arg)
{

    Fastwell::app_config * cfg = (Fastwell::app_config*)arg;
    Fastwell::tread_options ro;
    return ro(argc,argv,*cfg);
}

extern "C" int read_config (const char * _cfg_file, void *arg)
{
    Fastwell::app_config * cfg = (Fastwell::app_config*)arg;
    Fastwell::tread_options ro;
    std::string cfg_name(_cfg_file);
    return ro.read_config(cfg_name,*cfg);
}


#ifdef RDOPTS_SHARED_LIB
int main()
{
    return 0;
}

#endif
