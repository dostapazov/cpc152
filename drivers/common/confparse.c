#include "confparse.h"

int __init atoi_radix(const char * __str,int radix)
{

 int ret = 0;
 int len = 0;
 char * in = (char*)__str;
 int    i = 0;
 int    pow = 1;
 int    ch;
 while(radix && radix<=64 && (((*in>='0' && *in<='9') || (*in>='A' && *in<= 'z')) || *in=='~'))
     ++in,++len;
 if(len)
 {
  for(i = 0;i<len;i++)
  {
   --in;
      ch = radix>35? (int)*in:conf_toupper(*in);
  if(ch>=(char)'0' && ch<=(char)'9')
      ch -=(char)'0';
  else
  {
   if(ch == (char)'-')
     {ret = -ret;continue;}
   if(ch<(char)'a')
   {
    if(ch == (char)'^')
      ch = (char)36;
    else
    {
     ch-=(char)'A';
     ch+=(char)10;
    }
   }
   else
   {
    if(ch == '~')
      ch = 63;
      else{
        ch-='a';
        ch+=37;
      }
   }
  }
   ret+= ch*pow;
   pow*=radix;
  }
 }
 return ret;
}

int __init conf_atoi(const char * str)
{
 int ret = 0;
 int radix;
 if(str )
  {
   while(*str == '0')
       ++str;
   switch(*str)
   {
       case 'x':
       case 'X':
           radix = 16;
           ++str;
       break;
       case 'o':
           radix = 8;
           ++str;
       break;
       case 'b':
           radix = 2;
           ++str;
       break;
    default:
           radix = 10;
       break;

   }
   ret = atoi_radix(str,radix);
  }
 return ret;
}

int __init config_file_get_line     (struct file * f,char * buf,int bsz,loff_t * pos)
{

  int ret = -1;
  int rdb = 0;
  int ch;
  char * pbuf = buf;
  do{
      rdb = kernel_read(f,*pos,pbuf,1);
      if(rdb)
       {
         ++(*pos);
          ch = *pbuf;
         if(ch && ch!='\n' && ch!='\r' )
           {
             if(*pbuf!=' ' && *pbuf!='\t')
             {
              //skip spaces
              ++pbuf;
              ++ret;
             }
           }
           else
           {
            if(ret<1){ret = 1;*pbuf++ = '#';}
            break;
           }
       }
    }while(rdb>0 && ret<(bsz-1));
  *pbuf = 0;
   //printk(KERN_DEBUG"Get line done: ret = %d\n",ret);
   if(ret>0)
     {
      *pbuf = 0;
       //printk(KERN_DEBUG"%s",buf);
       pbuf = strchr(buf,'#');
       if(pbuf) *pbuf = 0;
     }
  return ret;
}


int __init config_line_get_param(const char * buf,int vcount,const char ** vnames, int * results)
{
  int ret = 0;
  char * ptr;
  int i;
  //printk(KERN_DEBUG"get param from line %s\n,vcount %d\n",buf,vcount);
   for(i = 0;i<vcount;i++)
   {
     ptr = strstr(buf,vnames[i]);
     if(ptr)
     {
       ptr+=strlen(vnames[i]);
       results[i] = conf_atoi(ptr);
      ++ret;
     }
   }
  return ret;
}


