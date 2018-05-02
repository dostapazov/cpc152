#include <lin_ke_defs.h>
#include <aic124ioctl.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <algorithm>
#include <errno.h>
#include "integr_data.h"
#include <time.h>
#include <fftw3.h>
#include <complex.h>


int syn_generate(int16_t * buf,int buf_count,bool is_sin,double ampl,bool positive,int discr = 1000,double Freq    = 50)
{
    double PI2     = M_PI*double(2.0);
    double period  = double(discr)/Freq;
    double delta_angle   = PI2/period;
    double angle = 0;
    double val;
    for(int i = 0;i<buf_count;i++)
    {
       if(is_sin)
        val = sin(angle);
        else
           val = cos(angle);
       if(positive)
            val+=1.0;
       val*=ampl;
       *buf = (int16_t)val;
     ++buf;
     angle+=delta_angle;
     if(angle>PI2) angle-=PI2;
    }
  return (int) period;
}

void read_from_aic124(int fd,int count,int aib,int param,int pcount,int period)
{
  int fft_size = 1024;
  u_int64_t time_len = 0;
  int val_count = 0;

  fftw_complex *out;
  double * in,*res;
  fftw_plan p,pb;

  in  = new double [fft_size]; //(fftw_complex*)fftw_malloc(sizeof(*in)*fft_size);
  res = new double [fft_size];//;(fftw_complex*)fftw_malloc(sizeof(*res)*fft_size);
  out = (fftw_complex*)fftw_malloc(sizeof(*out)*fft_size);
  bzero(in,sizeof(*in)*fft_size);
  bzero(res,sizeof(*res)*fft_size);
  bzero(out,sizeof(*out)*fft_size);


  p  = fftw_plan_dft_r2c_1d(pcount,in, out,FFTW_ESTIMATE|FFTW_PRESERVE_INPUT);
  pb = fftw_plan_dft_c2r_1d(pcount,out,res,FFTW_ESTIMATE|FFTW_PRESERVE_INPUT);
  fftw_print_plan(p);
  double ssum = 0;

  for(int i = 0;i<count || count<0;i++)
  {
    
    BYTE buffer[16384*2];

    aic124_input ibuf;
    int rd = read(fd,buffer,sizeof(buffer));
    if(rd>0)
     {
      int cnt = rd/sizeof(ibuf);
      //printf("read %d bytes, %d ibuf count\n",rd,cnt);
      volatile lpaic124_input ibptr = (lpaic124_input)buffer;
      lpaic124_input ibptr_end = ibptr+cnt;
      while(ibptr < ibptr_end)
      {
        if(ibptr->number == aib)
        {
         u_int64_t time_end   = Tintegr_data::timeval2filetime(ibptr->tmstamp_end);
         u_int64_t time_start = Tintegr_data::timeval2filetime(ibptr->tmstamp_start);

         time_len   += time_end - time_start;
         if(val_count<pcount)
         {
          double v = ibptr->data[0];
          in [val_count] = v;
          //in  [val_count][0] = v;     //real part
          //in  [val_count][1] = .0;     //real part
          ssum+= v*v;

          ++val_count;
         }
         else
         {
           fftw_execute_dft_r2c(p,in,out);
           fftw_execute_dft_c2r(pb,out,res);
           //fftw_execute(p);
           //fftw_execute(pb);

           double vsum = 0;
           for(int i = 0;i<pcount;i++)
           {
             double real  = out[i][0];
             double img   = out[i][1];
             real/=pcount;
             img/=pcount;
             double val   = sqrt(real*real+img*img);

             double angle = atan2(img,real);
               if(int((val*100.0))>1)
                printf("num %d real %.2f  img = %.2f val = %.2f angle = %.2f\n",i,real,img,val,angle);
            vsum+=val;
           }
           printf("vsum = %.2f %.2f sqrt(%.2f) = %.2f\n",vsum,vsum/(double)pcount,ssum,sqrt(ssum));
           for(int j = 0;j<pcount;j++)
               printf("%.2f ",res[j]/(double)pcount);
           printf("%s","\n---------------------------------\n");
           val_count = 0;
           ssum = 0;

         }

        }
        ++ibptr;
      }

     }
      else
    {
       rd = errno;
       printf("error - %d\n",rd);
       if(rd!=EAGAIN)
           return;
    }
  }
  fftw_print_plan(p);
  fftw_destroy_plan(p);
  fftw_free(out);
  delete [] in;
  delete [] res;
}


int init_module(int fd,int gains,int avg,int scan_period,int ampl)
{
 int ver = -1;

 if(!ioctl(fd,AIC124_IOCR_GET_VERSION,&ver))
 {
    printf("Init module aic124 version %d\n",ver);

    printf("setup DAC output\n");
    aic124_dacvalues dv;
    bzero(&dv,sizeof(dv));
    dv.dac_values_count = 1000/(50*scan_period);
    syn_generate(dv.dac_values,dv.dac_values_count+1,true,ampl,false,1000/scan_period,50);
    ioctl(fd,AIC124_IOCW_SET_DAC_VALUES,&dv);
    syn_generate(dv.dac_values,dv.dac_values_count+1,false,ampl,true,1000/scan_period,50);

    dv.dac_num = 1;
    dv.dac_values[0]=0x2FF;
    dv.dac_values_count = 1;

    ioctl(fd,AIC124_IOCW_SET_DAC_VALUES,&dv);


    printf("Reset channels\n");
    ioctl(fd,AIC124_IOC_CLEAR_CHANNELS);
    aib_param ap;
    bzero(&ap,sizeof(ap));

    ap.avg        = avg;
    ap.ch_gain    = gains;
    ap.count      = 32;

    memset(ap.mux_modes,1,sizeof(ap.mux_modes));
    for(int i = 0;i<(int)(sizeof(ap.mux_numbers)/sizeof(ap.mux_numbers[0]));i++)
        ap.mux_numbers[i] = i;
    ap.aib_number = 0;
    printf("Add aibs \n");
    if(!ioctl(fd,AIC124_IOCW_ADD_AIB,&ap))
    {
     ap.aib_number = 8;
     if(!ioctl(fd,AIC124_IOCW_ADD_AIB,&ap))
         return 0;
    }

 }
  return errno;
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

int test(int discr, int var);
int main(int argc ,char ** argv)
{

    const char * dev_name = get_argument("dev=",argc,argv);
    int rd_count = 10000;
    int scan_freq = 2;
    int avg       = 0;
    int aib       = 0;
    int param     = 0;
    int pcount    = 1;
    int gains     = 0;

    if(!dev_name) dev_name = "/dev/aic124_0";
    const char * a_str;
    a_str = get_argument("rdc=",argc,argv);
    if(a_str) rd_count=atoi(a_str);

    a_str = get_argument("avg=",argc,argv);
    if(a_str) avg=atoi(a_str);

    a_str = get_argument("sf=",argc,argv);
    if(a_str) scan_freq=atoi(a_str);

    a_str = get_argument("aib=",argc,argv);
    if(a_str) aib=atoi(a_str);

    a_str = get_argument("param=",argc,argv);
    if(a_str) param=atoi(a_str);

    a_str = get_argument("pcount=",argc,argv);
    if(a_str) pcount=atoi(a_str);

    a_str = get_argument("gains=",argc,argv);
    if(a_str) gains=atoi(a_str);

    return test(scan_freq,0);
    //sched_param sp;
    //sp.__sched_priority = sched_get_priority_max(SCHED_FIFO);
    //sched_setscheduler(0, SCHED_FIFO, &sp);

    int fd  = open(dev_name,O_RDWR);
    int period = 1000/(50*scan_freq);
    //period*=10;
    if(fd>0)
    {
     init_module(fd,gains,avg,scan_freq,1000);
     long ioc_ret;
     ioc_ret = ioctl(fd,AIC124_IOC_WORKSTART,&scan_freq);
     printf("%s","device start ");
     if(!ioc_ret)
     {
      printf("%s","success \n");
      //read_from_aic124_integ(fd,rd_count,period);
      read_from_aic124(fd,rd_count,aib,param,pcount,period);
      //usleep(3000000);
      ioc_ret = ioctl(fd,AIC124_IOC_WORKSTOP);
      printf("%s","device stop \n");
     }
     else
       printf("error %d\n",errno) ;
     close(fd);

    }
    else
     printf("error open device errno %d\n",errno);
     return 0;

    return 0;
}

#pragma GCC diagnostic pop

void dft_var0(int16_t * array,int cnt,int discr,double * Fr,double * Ph)
{

  int _cnt = 2048;
  double discr_freq  = 1000/discr;
  double dF  = discr_freq;
         dF /= double(_cnt);

  int Niguist = discr_freq/2;


  printf("discr_freq = %.2f, dF %.2f\n",discr_freq,dF);

  double * input  = new double[_cnt];//(fftw_complex*)fftw_malloc(sizeof(*input)*cnt);
  fftw_complex * output = (fftw_complex*)fftw_malloc(sizeof(*output)*_cnt);
  bzero(input ,sizeof(*input )*_cnt);
  bzero(output,sizeof(*output)*_cnt);
  double *mag   = new double[_cnt];
  double *phase = new double[_cnt];
  bzero(mag  ,sizeof(double)*_cnt);
  bzero(phase,sizeof(double)*_cnt);

  fftw_plan   p;
  p = fftw_plan_dft_r2c_1d(_cnt,input,output,FFTW_ESTIMATE);

  for(int i = 0;i<cnt;i++)
      input[i] = (double)array[i];
  fftw_execute(p);
  int from  = 30.0/dF;
  int limit = 70.0/dF;

  std::complex<double>* cbeg = (std::complex<double>*)output;
  std::complex<double>* ptr  = cbeg;
  std::complex<double>* cend = cbeg+limit;


  double * _mag    = mag;
  double * _phase  = phase;
  double  mag_sum  = 0;
  double normal = _cnt;
  normal/=2;

  while(ptr<(cend+1))
  {
   (*ptr)/=normal;
    //if(ptr>cbeg && ptr<(cend))
       // (*ptr)*=2;
     *_mag   = sqrt(ptr->real()*ptr->real()+ptr->imag()*ptr->imag());
     *_phase = atan2(ptr->imag(),ptr->real());
      mag_sum+= (*_mag)*(*_mag);
     ++_mag;
     ++_phase;
      ++ptr;
  }


  double F = 0;
  double MaxAmpl = .0;
  F = dF*from;
  for(int i = from;i<limit+1;i++)
  {
   double re     = output[i][0];
   double im     = output[i][1];
   double ampl   = mag[i];
   double phs   = phase[i];
   printf("idx=%03d F=%5.2f in = %05d  transfrom re=%10.2f im=%10.2f ampl=%10.2f phase=%10.2f\n",i,F,array[i],re,im,ampl,phs);
   if(ampl>MaxAmpl)
      {
        MaxAmpl = ampl;
        *Fr  = F;
        *Ph = phs;
      }

   F+=dF;
  }
  printf("ampl=%.2f\n",sqrt(mag_sum));
  printf("------------------------------------------------------\n");
  delete input;
  delete phase;
  delete mag;
  fftw_destroy_plan(p);
  fftw_free(output);

}

typedef std::pair<double,double> ampl_phase_t;



int test(int discr,int var)
{

    int16_t sin_data  [64000];
    int16_t cos_data  [64000];

 double freq     = 50.0;

 syn_generate(sin_data,sizeof(sin_data)/sizeof(sin_data[0]),true,1000,false,1000/discr,freq);
 syn_generate(cos_data,sizeof(cos_data)/sizeof(cos_data[0]),false,1000,false,1000/discr,freq);
 int cnt = 1024;
 double F1,Ph1;
 double F2,Ph2;

 switch(var)
 {
    case 0:
     dft_var0(sin_data,cnt,discr,&F1,&Ph1);
     dft_var0(cos_data,cnt,discr,&F2,&Ph2);
     break;
 }
 double grad = 180.0/M_PI;


 printf("F1 = %5.2f ph1 = %.2f gr1 =%.2f\n"
        "F2 = %5.2f ph2 = %.2f gr2 =%.2f\n"
        "delta %.2f\n"
        ,F1,Ph1,grad*Ph1,F2,Ph2,grad*Ph2,fabs(grad*Ph1)-fabs(grad*Ph2));
 return 0;
}
