#include "integr_data.h"
#include <algorithm>
#include <memory.h>


#define bzero(ptr,sz) memset(ptr,0,sz)


Tintegr_data::Tintegr_data(int _size):prev_time(0),count_time(0),count_scan(-1)
{
  vcount = std::max(1,_size);
  values = new int32_t[4*vcount];
  bzero(values ,4*sizeof (*values)*vcount);
  max_val = values+vcount;
  p_res   = max_val+vcount;
  n_res   = p_res+vcount;
}


u_int64_t Tintegr_data::timeval2filetime(timeval & tv)
{
    u_int64_t ret = 11644473600;
    ret+=tv.tv_sec;
    ret*= 10000000;
    ret+= tv.tv_usec*10;
    //ret-= ret%10000;
    return ret;
}



void   Tintegr_data::reset()
{
    bzero(max_val,sizeof(*max_val)*vcount*3);
    count_scan    = 0;
    count_time    = 0;

}

int64_t __abs64(int64_t val)
{
    if(val<0) return -val;
    return val;
}

int   Tintegr_data::add_scan(u_int64_t time, int16_t * in_data, size_t in_count)
{
  //Добовление новых данных сканирования


  int32_t * ptr_beg  = values;
  int32_t * ptr_end  = ptr_beg+std::min((int)in_count,vcount);
  int32_t * _max_val = max_val;

  if(++count_scan)
  {
    int32_t   dT  = (time-prev_time)/100;
    count_time+=dT;
    int32_t * _p_res = p_res;
    int32_t * _n_res = n_res;

    while(ptr_beg<ptr_end)
     {
      int32_t _val  = *ptr_beg;
      int32_t _dval = (int64_t)(*in_data++) - _val;
      _val*=dT;
      _val+=((_dval*dT)>>1); // div 2
      //_val+=(_dval*dT);
      if(_val>0)
      *_p_res += _val;
      else
       *_n_res+= _val;

      *_max_val = std::max(*_max_val,abs(_val));

      ++_max_val;
      *ptr_beg+=_dval;
      ++_p_res;++_n_res;
      ++ptr_beg;
     }
  }
  else
  {
    while(ptr_beg<ptr_end)
    {
      *ptr_beg = (int32_t)*in_data++;
      ++ptr_beg;
    }
  }

  prev_time = time;
  return count_scan;
}

int       Tintegr_data::get_result(int num,int * _p_res ,int * _n_res )
{
  int ret = 0;
  if(num <vcount)
  {
    ret = p_res[num] - n_res[num];
    if(_p_res) *_p_res = p_res[num];
    if(_n_res) *_n_res = n_res[num];

  }
  return ret;
}

void    Tintegr_data::get_results(int32_t * results,int32_t* p_results,int32_t* n_results ,int count,bool mk_div,int32_t mul )
{
 int32_t * end_res = results+count;
 int32_t * p_beg   = this->p_res;
 int32_t * n_beg   = this->n_res;
 while(results<end_res)
  {
    if(p_results) *p_results++ = *p_beg;
    if(n_results) *p_results++ = *n_beg;
    *results = *p_beg - *n_beg;
    if(mk_div)
    {
        int64_t i = *results;
        i*=mul;
        i/=this->count_time;
        (*results) = (int32_t)i;
    }
    ++results;++p_beg;++n_beg;
  }
}

/*
Tintegr_simpson::Tintegr_simpson(int _size):step_count(-1),prev_time(0),count_time(0)
{
  vcount   = std::max(1,abs(_size));
  values   = new int32_t[4*vcount];
  results  = values+vcount;
  odd_sum  = results+vcount;
  even_sum = odd_sum+vcount;
  bzero(values,vcount*4*sizeof(*values));

}

Tintegr_simpson::~Tintegr_simpson()
{
  if(values)
      delete [] values;
  values = results = odd_sum = even_sum;
}

int Tintegr_simpson::add_scan   (timeval & tv,int16_t * in_data,size_t in_count)
{
 in_count = std::min(vcount,(int) in_count);

 int32_t * vbeg = values;
 int32_t * vend = vbeg+in_count;
 int64_t   curr_time = Tintegr_data::timeval2filetime(tv);
 if(++this->step_count)
 {
     int32_t shift;
     if(step_count&1)
       {
         vbeg  = odd_sum;
         shift = 2;
       }
     else
       {
         vbeg  = even_sum;
         shift = 1;
       }
   vend = vbeg+in_count;
   while(vbeg<vend)
   {
    int32_t v = (int32_t)*in_data;
    if(v<0) v = -v;
    v<<=shift;
    (*vbeg)+= v;
     ++vbeg;
     ++in_data;
   }
   count_time+= curr_time-prev_time;
 }
 else
 {
   while(vbeg<vend)
   {
     *vbeg =  (int32_t)  *in_data;
     ++vbeg;
     ++in_data;
   }
 }
 prev_time = curr_time;
 return step_count;
}

int Tintegr_simpson::do_result  (timeval & tv,int16_t * in_data,size_t in_count)
{
  int64_t   curr_time = Tintegr_data::timeval2filetime(tv);
  in_count = std::min(vcount,(int) in_count);

  count_time+= curr_time-prev_time;
  int64_t H = count_time/(++step_count);
  int64_t I;

  int32_t * vbeg  = values;
  int32_t * vend  = vbeg+in_count;
  int32_t * podd  = odd_sum;
  int32_t * peven = even_sum;
  int32_t * res   = results;

  while(vbeg<vend)
  {
    I = *vbeg+(int32_t)*in_data+*podd+*peven;
   *vbeg = *in_data;
    I*=H;
    *res = (int32_t)I/3;
    ++vbeg;
    ++podd;
    ++peven;
    ++in_data;
    ++res;
  }

  step_count = 0;
  count_time = 0;
  prev_time = curr_time;
  bzero(this->odd_sum,vcount*2*sizeof(*odd_sum));
  return in_count;
}
*/

