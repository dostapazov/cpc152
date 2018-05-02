#ifndef _DIC120IOCTL_H_
#define _DIC120IOCTL_H_

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>

typedef  u8  u_int8_t   ;
typedef  u16 u_int16_t  ;
typedef  u32 u_int32_t  ;
typedef  u64 u_int64_t  ;
typedef  s16 int16_t    ;

#else
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#endif



#pragma pack(push,1)

#define  DIC120_PGA_COUNT     4
#define  P55_DATA_PORT_COUNT  3
#define  P55ID                0x3770

typedef struct _dic120_input
{
          u_int64_t  seq_number;
  struct  timeval    tmstamp_start;
  struct  timeval    tmstamp_end;
          u_int16_t  number;//Number of PGA
          int16_t    test_signals[2];
          u_int8_t   data_len;
          u_int8_t   data        [P55_DATA_PORT_COUNT];
          u_int8_t   changes_mask[P55_DATA_PORT_COUNT];

}dic120_input, *lpdic120_input;

#define P55_DEBOUNCE_100NS  0
#define P55_DEBOUNCE_1_6MS  1
#define P55_DEBOUNCE_4MS    2
#define P55_DEBOUNCE_120MS  4

#define P55_NO_FRONTS       0
#define P55_FORWARD_FRONT   1
#define P55_BACK_FRONT      2
#define P55_BOTH_FRONTS     3

typedef struct _p55_pga_param
{
  u_int8_t    number;
  u_int8_t    debounce; // debounce value;  0 - 100ns, 1 - 1.6ms, 2 - 4ms,  4-120ms
  u_int8_t    fronts[P55_DATA_PORT_COUNT]; //use fronts:  0- none, 1 - forward front, 2 - back front,3 - both fronts
  u_int8_t    irqs  [P55_DATA_PORT_COUNT]; //0 - disabled 1 - enabled;
} p55_pga_param,*lpp55_pga_param;


#pragma pack(pop)

#define DIC120_IOC_MAGIC           'D'
#define DIC120_IOCR_GET_VERSION         _IOR(DIC120_IOC_MAGIC, 0,int)
#define DIC120_IOC_WORKSTOP             _IO( DIC120_IOC_MAGIC, 1)
#define DIC120_IOC_WORKSTART            _IOW(DIC120_IOC_MAGIC, 2,int)
#define DIC120_IOCW_ADD_PGA             _IOW(DIC120_IOC_MAGIC, 3,p55_pga_param)
#define DIC120_IOC_CLEAR_PGA            _IO (DIC120_IOC_MAGIC, 4)
#define DIC120_IOC_TEST                 _IOW(DIC120_IOC_MAGIC, 5,int)
#define DIC120_IOC_REREAD              _IO( DIC120_IOC_MAGIC, 6)

#define DIC120_IOC_MAXNR    6
//#endif


#endif
