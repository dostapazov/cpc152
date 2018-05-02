#include "read_options.hpp"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace boost::algorithm;

namespace Fastwell
{
DWORD  hexchar2digit(char ch)
{

    ch = toupper(ch);
    if(ch>='0' && ch<='9')
        return DWORD(ch-'0');
    ch&=~0x20;
    if(ch>='A' && ch<='F')
        return DWORD(ch-'A'+10);
    return -1;
}

bool is_hex_digit(char val)
{
    val =(char) toupper(val);
    return ((val >='0' && val <='9') || (val>='A' && val<='F')) ? true:false;
}


DWORD hextoi(const string str)
{
    DWORD ret = 0;
    string::const_iterator beg = str.cbegin();
    string::const_iterator end = str.cend();
    while(beg<end &&  is_hex_digit(*beg))
    {
        ret<<=4;
        ret|=hexchar2digit(*beg);
        ++beg;
    }
    return ret;
}

int str2int(const string _str)
{
    string str = _str;
    str = to_upper_copy(str);
    int ret;
    int offs = str.find("0X");

    if(offs<0)
        ret = boost::lexical_cast<int>(str);
    else
    {
        str.erase(str.begin());
        str.erase(str.begin());
        ret = hextoi(str);
    }

    return ret;
}

int str2int_list(const string str,int * values,int count)
{
    int ret = 0;
    string::const_iterator beg = str.cbegin();
    string::const_iterator end = str.cend();
    string sub_str;
    while(ret<count && beg<end)
    {
        switch(*beg)
        {
        case 0:
        case ' ':
            beg = end;
        case ',':
            values[ret++] = str2int(sub_str);
            sub_str.clear();
            break;
        default:
            sub_str+=*beg;
            break;
        }
        ++beg;
    }
    if(sub_str.length() && ret<count)
        values[ret++] = str2int(sub_str);
    return ret;
}


}
