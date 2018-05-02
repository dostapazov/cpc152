#include <lin_ke_defs.h>
#include <dic120ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>



void read_from_dic120(int fd,int rdc)
{
  char  rd_buf[8192];
  for(int i = 0;i<rdc || rdc<0;i++)
  {
     int rd_len = read(fd,rd_buf,sizeof(rd_buf));
     if(rd_len>0)
     {
        int bcnt = rd_len/sizeof(dic120_input);
        lpdic120_input bptr = (lpdic120_input)rd_buf;
        lpdic120_input eptr = bptr+bcnt;
        while(bptr<eptr)
        {
         char tmstr[4096];
         struct tm *_tm = localtime(&bptr->tmstamp.tv_sec);
         strftime(tmstr,sizeof(tmstr),"%Y-%m-%d %H:%M:%S",_tm);
         printf("%s %Lu pga-%hu port %04X dlen=%u\n",tmstr, bptr->seq_number,bptr->number,(unsigned)(u_int16_t)(bptr->test_signals[1]),(unsigned)bptr->data_len);
         printf("%02X %02X %02X\n",bptr->data[0],bptr->data[1],bptr->data[2]);
         printf("%02X %02X %02X\n",bptr->changes_mask[0],bptr->changes_mask[1],bptr->changes_mask[2]);
         bptr++;
        }
     }
     else
     {

     }
  }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

const char * get_argument(const char * arg ,int argc,char ** argv)
{
  char * ret = 0;
  for(int i = 1;i<argc && !ret;i++)
  {
    ret = strstr(argv[i],arg);
    if(ret) ret+=strlen(arg);
  }
  return ret;
}



int main (int argc,char ** argv)
{
 printf("DIC120 driver test\n");
 const char * dev_name;
 dev_name = get_argument("dev=",argc,argv);
 if(!dev_name || !dev_name[0])
     dev_name = "/dev/dic120_0";
 int rdc = 1000;
 const char * arg = get_argument("rdc=",argc,argv );
 if(arg) rdc = atoi(arg);

 printf("open device %s \n",dev_name);
 int fd = open(dev_name,O_RDWR);
 if(fd>0)
 {
   p55_pga_param pp;
   pp.debounce = 2;
   memset(pp.fronts,3,sizeof(pp.fronts));
   memset(pp.irqs,1,sizeof(pp.irqs));

   long ioc_ret;
   ioc_ret = ioctl(fd,DIC120_IOC_CLEAR_PGA);
   pp.number = 0;
   ioc_ret = ioctl(fd,DIC120_IOCW_ADD_PGA,&pp);
   pp.number = 2;
   ioc_ret = ioctl(fd,DIC120_IOCW_ADD_PGA,&pp);
   pp.number = 3;
   ioc_ret = ioctl(fd,DIC120_IOCW_ADD_PGA,&pp);
   pp.number = 1;
   ioc_ret = ioctl(fd,DIC120_IOCW_ADD_PGA,&pp);
   if(!ioc_ret)
    {
      int scan_freq = 2000;
      arg = get_argument("sf=",argc,argv );
      if(arg) scan_freq = atoi(arg);
      ioc_ret = ioctl(fd,DIC120_IOC_WORKSTART,&scan_freq);
      read_from_dic120(fd,rdc);
      ioc_ret = ioctl(fd,DIC120_IOC_WORKSTOP);
    }

   close(fd);

 }
 else
 {
   printf("open error %d\n",errno);
 }

 return 0;
}

#pragma GCC diagnostic pop
