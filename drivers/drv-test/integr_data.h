#ifndef _integr_data_inc_
#define _integr_data_inc_
#include <sys/types.h>
#include <sys/time.h>



class Tintegr_data
{
 //protected:
  int64_t        prev_time;
  int            count_time;
  int            count_scan;
  int            vcount;
  int32_t   *    values;
  int32_t   *    max_val;
  int32_t   *    p_res ;
  int32_t   *    n_res;

public:
  Tintegr_data(int _size = 32);
  ~Tintegr_data()
  {if(values) delete [] values;  }
static u_int64_t timeval2filetime(timeval & tv)  ;
       int       add_scan(u_int64_t time,int16_t * in_data,size_t in_count);
       void      reset();
       int       get_result (int num, int * _p_res = nullptr, int * _n_res = nullptr);
       void      get_results(int32_t * results, int32_t* p_result, int32_t* n_results , int count, bool mk_div, int32_t mul = 1) ;
       int       get_vcount(){return vcount;}
       int       get_count_time(){return count_time;}
       int       get_count_scan(){return count_scan;}
};

/*
class Tintegr_simpson
{
 protected:
    int     step_count;
    int64_t prev_time;
    int64_t count_time;
    int     vcount;
    int32_t * values;
    int32_t * odd_sum;
    int32_t * even_sum;
    int32_t * results;
 public:
    Tintegr_simpson(int _size);
   ~Tintegr_simpson();
    int add_scan   (timeval & tv,int16_t * in_data,size_t in_count);
    int do_result  (timeval & tv,int16_t * in_data,size_t in_count);
};
*/

#endif
