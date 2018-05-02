#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <iostream>
#ifndef RDOPTS_SHARED_LIB
#include "cpc152svc.hpp"
#else
#include "read_options.hpp"
#endif


using namespace std;
using namespace boost::algorithm;


namespace Fastwell
{

#ifndef RDOPTS_SHARED_LIB
void parse_alarm_start(string value ,talarms & alarms)
{
//Разбирается строка вида 0-1-1>0 || 0-1-2<1

    talarm_condition ac (0,0,0,false,0);

    int err = 0;
    vector<string> s;
    string str;
    split(s,value,is_any_of(">"));

    if(s.size()>1)
        ac.more = true;
    else
    {
        s.clear();
        split(s,value,is_any_of("<"));
        if(s.size()<2)
            err++;
    }

    if(!err)
    {
        ac.alarm_value =str2int(s.at(1));
        str = s.at(0);
        s.clear();
        split(s,str,is_any_of("-"));
        if(s.size()==3)
        {

            ac.dev_num   = str2int(s.at(0));
            ac.grp_num   = str2int(s.at(1));
            ac.param_num = str2int(s.at(2));

        }
        else
            err++;
    }

    if(tapplication::get_log_level()>2)
    {
    if(err)
        tapplication::write_syslog(LOG_ERR,"Wrong alarm start definition %s\n",value.c_str());

     alarms<<ac;
    }


}

#endif

talarms &  operator << ( talarms & alarms,talarm_condition & ac)
{
#ifndef RDOPTS_SHARED_LIB
    char text[512];
    int len;
#endif
    talarms::iterator ptr;
    ptr = std::lower_bound(alarms.begin(),alarms.end(),ac);
    if(ptr < alarms.end() && (*ptr) == ac )
    {
        *ptr = ac;
#ifndef RDOPTS_SHARED_LIB
        len = sprintf(text,"Update:" );
#endif
    }
    else
    {
        alarms.insert(ptr,ac);
#ifndef RDOPTS_SHARED_LIB
        len = sprintf(text,"Insert:");
#endif
    }
#ifndef RDOPTS_SHARED_LIB
    alarm_condition_get_text(ac,text+len,(int)sizeof(text)-len);
    tapplication::tapplication::write_syslog(LOG_INFO,"%s\n",text);
#endif
    return alarms;
}





}//Fastwell
