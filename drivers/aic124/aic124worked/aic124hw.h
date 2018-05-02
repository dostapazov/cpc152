#ifndef __AIC124HW__
#define __AIC124HW__

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/preempt.h>
#include <linux/atomic.h>
#include <linux/jiffies.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <aic124ioctl.h>
#include "../common/drvqueue.h"

#define AIC124_PORT_COUNT          0x10

#define AIC124_RW_REGB_CONTROL0       0

#define AIC124_W_CTRL0_AUTOSCAN    0x01
#define AIC124_W_CTRL0_ENABLE_IRQ  0x02
#define AIC124_W_CTRL0_ENABLE_DMA  0x04
#define AIC124_W_CTRL0_FREQ_100KHZ 0x08
#define AIC124_W_CTRL0_ADC_TIMER   0x10
#define AIC124_W_CTRL0_SEL_DRQ3    0x40
#define AIC124_W_CTRL0_ADC_START   0x80

#define AIC124_R_CTRL0_EOC         0x20 // End of conversion
#define AIC124_R_CTRL0_DAC_RDY     0x40 // DAC Ready
#define AIC124_R_CTRL0_ADC_RDY     0x80 // ADC Ready


#define AIC124_RW_REGB_CONTROL1       1

#define AIC124_W_CTRL1_NOAVG       0x00
#define AIC124_W_CTRL1_AVG2        0x04
#define AIC124_W_CTRL1_AVG4        0x05
#define AIC124_W_CTRL1_AVG8        0x06
#define AIC124_W_CTRL1_AVG16       0x07



#define AIC124_W_CTRL1_ENABLE_FIFO 0x08
#define AIC124_W_CTRL1_IRQ_SHARE   0x20

//ADC data read register
#define AIC124_R_REGW_ADC_DATA         2

#define AIC124_W_REGB_ADC_CHANNEL      2
#define AIC124_CHANNEL_MASK      0x0F
#define AIC124_CHANNEL_SINGLE    0x20
#define AIC124_CHANNEL_2p5V      0x10  // TEST SIGNAL 2,5 V
#define AIC124_CHANNEL_AGND      0x14  // TEST SIGNAL GROUND


//Discrete output register
#define AIC124_WR_REGB_DISCRETE_OUT     3
#define AIC124_MUX_MASK                 0x1F
#define AIC124_MUX_SINGLE               0x20


#define AIC124_REGW_RW_TIMER           4

#define AIC124_W_REGB_GAIN0            6
#define AIC124_W_REGB_GAIN1            7
#define AIC124_W_REGB_GAIN2            8
#define AIC124_W_REGB_GAIN3            9

#define AIC124_R_REGB_REVNUM           6
#define AIC124_R_REGB_VERNUM           7
#define AIC124_W_REGB_EX_CTRL_ENABLE        12
#define AIC124_W_REGB_EX_CTRL          10
#define AIC124_W_EXTREGB_RD_SERIAL    0x01
#define AIC124_W_EXTREGB_ADC_BITS16   0x08
#define AIC124_W_EXTREGB_O_PLUS_PLUS  0x20
#define AIC124_W_EXTREGB_SOFT_RESET   0x80

#define AIC124_W_REGW_DAC                 14
#define AIC124_DAC_VALUE_MASK             0x0FFF
#define AIC124_DAC_CHANEEL1               0x1000
#define AIC124_DAC_VALUE_OUTPUT_DISABLED  0x8000



#define AIC124_R_REGB_ID               14
#define AIC124_R_REGB_DAC_MODE         15
#define AIC124_DAC_MODE_VOLTAGE        0x11
#define AIC124_DAC_MODE_CURRENT        0x75
#define AIC124_DAC_MODE_MIXED          0xD9


#define __AIC124_DEV_SCAN_CNANNELS           3
#define __AIC124_DEV_SCAN_TEST_SIGNAL_1      2
#define __AIC124_DEV_SCAN_TEST_SIGNAL_2      1
#define __AIC124_DEV_SCAN_NONE               0


typedef struct _aic124_dac_output_r
{
    s16           data[100];
    s16           data_count;
    s16           data_pos;
    s16           value;
} aic124_dac_output_t,*lpaic124_dac_output_t;


typedef struct _aib_channels_t
{
    u8 aib_number;
    u8 count;
    u8 channels[AIC124_MUX_SINGLE_CHANNELS_COUNT];
} aib_channels_t,* lpaib_channels_t;

typedef struct _aic124dev
{
    int                    isize;
    char                  *info;
    int                    info_size;
    int                    info_len;
    int                    info_step;
    int                    dev_number;
    atomic_t               active;

    struct proc_dir_entry *proc_entry;
    spinlock_t      irq_locker;


    struct dqueue          queue;
    u64             queue_total;   //  Total data  queue
    u32             queue_lost ;   //  Lost data;
    u32             queue_overflow; // current overflow;
    u64             queue_reads;
    u64             queue_entry_reads;


    wait_queue_head_t     _wait_queue;
    int            _wait_flag;

    struct timer_list      dev_timer;
    u64             dev_timer_count;

    u64             scan_count;
    u32             scan_over_time;     //count of jiffies how mach scan overtime
    u32             scan_over_time_count;//
    void *          resource;
    atomic_t        irq_handler_installed;
    struct tasklet_struct  irq_tasklet1;
#ifndef _REAL_DEVICE
    struct tasklet_struct  irq_emul_tasklet;
#endif
    u64             irq_count;
    u64             irq_handled;
    int             base_port;
    int             irq_num;
    /*end of common fields*/

    u8              avg;
    u8              gains[4];
    u16             aic124_timer;

    u32             tmr_jiff_add;
    atomic_t        scan_state;

    u8              auto_scan;
    u8              auto_scan_aibs;

    u8              channels_count;
    u8              channel_numbers[AIC124_SINGLE_CHANNELS_COUNT];// Values to select chunnels ch_number,
    u8              channel_idx;

    u8               aib_use;
    aib_channels_t   aib_channels[AIC124_MUX_COUNT];
    lpaib_channels_t aib_current;
    u8               aib_channel_idx;

    volatile u8      regb0rd;                //device's state bits
    volatile u8      regb0wr;                //device's control reg 0
    volatile u8      regb1wr;                //device's control reg 1
    volatile u8      regexb_wr;              //device's ex control reg
    volatile u16     reg_timer;

    struct timeval   scan_start_time;
    u64              seq_number;
    u32             _internal_lost;
    struct   dqueue _queue_internal;
    aic124_input    _curr_data_buff;
    lpaic124_input  curr_data_buff;
    atomic_t        test_signals[2]; // Test signals 0 - 2.5 V, 1 -  AGND
//DAC 0,1     output data
    aic124_dac_output_t dac_output[2];
//device info data
    u8            id [2];
    u8            ver[2];
    u8            serial_num[8];
    struct        cdev _cdev;
} aic124dev,* lpaic124dev;

extern lpaic124dev get_first_device(void);
extern lpaic124dev get_last_device (void);

#ifndef SA_SHIRQ
#define SA_SHIRQ IRQF_SHARED
#endif

extern int  aic124dev_setup             (volatile lpaic124dev dev, int base_port, int irq, int avg, u8 gains[4], u16 channels_mode);
extern int  aic124dev_resource_init     (volatile lpaic124dev dev);
extern int  aic124dev_add_channel       (volatile lpaic124dev dev, aic124_channel_param *cp);
extern int  aic124dev_add_aib           (volatile lpaic124dev dev, aib_param * ap);
extern int  aic124dev_clear_channels    (volatile lpaic124dev dev);

extern int  aic124dev_resource_release  (volatile lpaic124dev dev);
extern int  aic124dev_make_info         (volatile lpaic124dev dev);

extern int  aic124dev_start             (volatile lpaic124dev dev, int start, int scan_period);
extern void aic124dev_data_to_queue     (volatile lpaic124dev dev);
extern void aic124dev_irq_tasklet1      (unsigned long arg);
extern void aic124dev_timer             (unsigned long arg);
extern u8   aic124dev_get_channel_gain  (lpaic124dev dev, u8 channel_number);

#ifndef _REAL_DEVICE
extern void aic124dev_irq_emul_tasklet  (unsigned long arg);
extern int  aic124dev_test              (lpaic124dev dev,int mode);
#endif





static inline  int aic124dev_is_device(volatile lpaic124dev dev)
{
    if(dev && dev->isize == sizeof(*dev) && dev->id[0] == 'A') return 1;
    return 0;
}




#endif
