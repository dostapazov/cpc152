#ifndef _READ_OPTIONS_INC
#define _READ_OPTIONS_INC

#include <string>
#include <vector>
#include <map>
#include <gklib/otd.h>
#include <aic124ioctl.h>
#include <dic120ioctl.h>
#include <gklib/cpc152proto.h>

namespace Fastwell
{

using namespace std;



struct  server_config
{
    int server_port;
    int frame_size;
    server_config() {
        server_port  = 45777;
        frame_size   = 4096;
    }
};

struct tdic_param
{
    char   dev_name[128];
    int    scan_freq;
    int    debounce;
    int    pga     [DIC120_PGA_COUNT];
    tdic_param() {
        bzero(this,sizeof(*this));
        debounce = 4;
    }
};

struct taic_param
{
    char       dev_name[128];
    int        gain;
    int        scan_period;
    int        channels[AIC124_SINGLE_CHANNELS_COUNT];
    int        modes   [AIC124_SINGLE_CHANNELS_COUNT];
    taic_param() {
        bzero(this,sizeof(*this));
    }
};

struct device_config
{

    int  dic_count;
    tdic_param dic[CPC152_MAX_SLOTS];
    int  aic_count;
    taic_param aic[CPC152_MAX_SLOTS];
    device_config() {
        dic_count = aic_count = 0;
    }
};



inline int  alarm_condition_get_text(const talarm_condition & ac, char * text,int sz)
{
    return snprintf(text,sz,"%d-%d-%d%c%u"
                    ,ac.dev_num
                    ,ac.grp_num
                    ,ac.param_num
                    ,ac.more ? '>':'<',ac.alarm_value);
}

typedef std::vector<talarm_condition> talarms;

talarms &  operator << (talarms & alarms,talarm_condition & ac);

struct talarms_def
{
    char     storage_path[1024];
    bool     use_interpol_data;
    DWORD    pre_alarm;
    DWORD    post_alarm;
    talarms_def() {
        pre_alarm = 5000;
        post_alarm = 100;
        use_interpol_data = false;
    }
};

void parse_alarm_start(string value ,talarms & alarms);

struct tdac_param
{
   int freq;
   int ampl;
   bool inv_neg;
   bool is_sinus;
   tdac_param(){freq = 50;ampl=0x1FF;is_sinus = true;inv_neg = false;}
};

struct app_config
{
    device_config dev_cfg;
    server_config srv_cfg;
    talarms_def   alarms;
    tdac_param    dac_param[2];
    int           log_level;
    int           daemon;
    int           devp_qsize;
    int           watch_dog_enabled;
    app_config() {
        log_level       = 0;
        daemon          = 0;
        devp_qsize      = 4096;
        watch_dog_enabled  = 0;
    }
};


class tread_options
{
protected:

    std::map<string,int> cmd_map;

    typedef std::pair<string,string> key_value;
    void init_cmd_map(const char ** ptr);
    void get_cmd_key (const char * ptr,key_value & kv);
    void print_help  (const char ** ptr);
    static const char *commands [];

public:
    tread_options();
    int  operator ()(int argc,char **  argv,app_config & cfg);
    int  read_config(const string & cfg_file,app_config & cfg);

};

DWORD  hexchar2digit(char ch);
bool   is_hex_digit (char val);
DWORD  hextoi       (const string str);
int    str2int      (const string _str);
int    str2int_list (const string str,int * values,int count);

}// end of namespace Fastwell;

typedef int (*pread_options)(int,char**,void *);
typedef int (*pread_config )(const char *    ,void *) ;

extern "C"
{
    extern int read_options(int argc, char **  argv, void *arg);
    extern int read_config (const char * cfg_file,void * arg);

}

#endif
