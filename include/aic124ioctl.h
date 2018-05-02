#ifndef _AIC124IOCTL_H_
#define _AIC124IOCTL_H_

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

#define AIC124_SINGLE_CHANNELS_COUNT      16 // AIC 124 max channels in single_mode
#define AIC124_SINGLE_CHANNEL_MAXNUM     (AIC124_SINGLE_CHANNELS_COUNT-1)
#define AIC124_DIFF_CHANNELS_COUNT        8  // AIC 124 max channels in differential mode
#define AIC124_DIFF_CHANNEL_MAXNUM        (AIC124_DIFF_CHANNELS_COUNT-1)


#define AIC124_MUX_COUNT                  16 // AIC 124 max mux like AIB920
#define AIC124_MUX_SINGLE_CHANNELS_COUNT  32 // AIB 920 max channels in single mode
#define AIC124_MUX_DIFF_CHANNELS_COUNT    16 // AIB 920 max channels in differential mode
#define AIC124_MUX_SINGLE_CHANNEL_MAXNUM  (AIC124_MUX_SINGLE_CHANNELS_COUNT-1) //
#define AIC124_MUX_DIFF_CHANNEL_MAXNUM    (AIC124_MUX_DIFF_CHANNELS_COUNT-1)

#define AIC124_CAPACITY                   (AIC124_MUX_COUNT*AIC124_MUX_SINGLE_CHANNELS_COUNT)

#define AIC124_AIB_MUXMASK       0x01F
#define AIC124_AIB_SINGLE_MODE   0x020
#define AIC124_AIB_GAIN1         0x000
#define AIC124_AIB_GAIN2_10      0x040
#define AIC124_AIB_GAIN4_100     0x080
#define AIC124_AIB_GAIN8_1000    0x0C0




#pragma pack(push,1)



typedef struct _aic124_channel_param
{
 u_int8_t    avg;        //avg-value
 u_int8_t    auto_scan;  //enable auto scan;
 u_int8_t    channel_number ;
 u_int8_t    channel_gain  ;
 u_int8_t    channel_mode  ;
}aic124_channel_param,*lpaic124_channel_param;

typedef struct _aib_param
{
  u_int8_t  avg;
  u_int8_t  auto_scan;
  u_int8_t  aib_number;
  u_int8_t  count;
  u_int8_t  ch_gain;  //Gain for aic124 channel ;
  u_int8_t  mux_numbers[AIC124_MUX_SINGLE_CHANNELS_COUNT];
  u_int8_t  mux_modes  [AIC124_MUX_SINGLE_CHANNELS_COUNT];
  u_int16_t mux_gains  [AIC124_MUX_SINGLE_CHANNELS_COUNT];
}aib_param,lpaib_param;


typedef struct _aic124_dacvalues
{
 u_int8_t  dac_num;
 u_int8_t  dac_values_count;
 int16_t   dac_values[50];
}aic124_dacvalues, *lpaic124_dacvalues;



typedef struct _aic124_input
{
          u_int64_t     seq_number;
  struct  timeval       tmstamp_start;
  struct  timeval       tmstamp_end;
          u_int16_t     number;//Number of AIB or 0 if aib_mask = 0;
          int16_t       test_signals[2];
          u_int16_t     data_len;
          int16_t       data[AIC124_MUX_SINGLE_CHANNELS_COUNT];
}aic124_input, *lpaic124_input;




#pragma pack(pop)

#define AIC124_IOC_MAGIC                'A'
#define AIC124_IOCR_GET_VERSION         _IOR(AIC124_IOC_MAGIC, 0,int)
#define AIC124_IOC_WORKSTOP             _IO( AIC124_IOC_MAGIC, 1)
#define AIC124_IOC_WORKSTART            _IOW(AIC124_IOC_MAGIC, 2,int)
#define AIC124_IOCW_SET_DAC_VALUES      _IOW(AIC124_IOC_MAGIC, 3,aic124_dacvalues)
#define AIC124_IOCW_ADD_CHANNEL         _IOW(AIC124_IOC_MAGIC, 4,aic124_channel_param)
#define AIC124_IOCW_ADD_AIB             _IOW(AIC124_IOC_MAGIC, 5,aib_param)
#define AIC124_IOC_CLEAR_CHANNELS       _IO(AIC124_IOC_MAGIC,  6)
#define AIC124_IOC_TEST                 _IOW(AIC124_IOC_MAGIC, 7,int)

#define AIC124_IOC_MAXNR    7

#endif
