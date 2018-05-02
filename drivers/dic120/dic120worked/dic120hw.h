#ifndef __DIC120HW__
#define __DIC120HW__

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
#include <dic120ioctl.h>
#include "../common/drvqueue.h"

#define DIC120_FIRST_IRQ             3
#define DIC120_LAST_IRQ              7

#define DIC120_BASE_ADDR_OFFSET      0xA000
#define DIC120_BASE_ADDR_PGA_OFFSET  0x0400
#define DIC120_DEV_PORT_COUNT        0x0010
#define DIC120_IRQ_NUM_MASK          0x0007

#define DIC120_INT_REG      0x0D
#define DIC120_ID           0x0E



/*Порты*/

#define  P55_PORT_A      0
#define  P55_PORT_B      1
#define  P55_PORT_C      2
#define  P55_CONTROL     3

#define  P55_TIME_BOUNCE 4
#define  P55_INT_EVENT   5

#define  P55_EVENT_A     6
#define  P55_EVENT_B     7
#define  P55_EVENT_C     8
#define  P55_RESERV1     9
#define  P55_RESERV2     0x0A
#define  P55_RESERV3     0x0B
#define  P55_RESERV4     0x0C

#define  P55_ID_REG      DIC120_ID

#define  P55_PORT_A_INPUT  0x10
#define  P55_PORT_B_INPUT  0x02
#define  P55_PORT_C0_INPUT 0x01
#define  P55_PORT_C1_INPUT 0x08
#define  P55_PORT_MASK (P55_PORT_A_INPUT|P55_PORT_B_INPUT|P55_PORT_C0_INPUT|P55_PORT_C1_INPUT)

#define  P55TM_100NS       0
#define  P55TM_1_6MKS      1
#define  P55TM_4MS         2
#define  P55TM_120MS       3

#define  P55BNC_A_FR0      0x04
#define  P55BNC_A_FR1      0x08
#define  P55BNC_A_FR_BOTH  0x0C

#define  P55BNC_B_FR0      0x10
#define  P55BNC_B_FR1      0x20
#define  P55BNC_B_FR_BOTH  0x30

#define  P55BNC_C_FR0      0x40
#define  P55BNC_C_FR1      0x80
#define  P55BNC_C_FR_BOTH  0xC0
#define  P55BNC_FR_ALL     0xFC



typedef struct _dic120dev
{
    int                    isize;
    char                   *info;
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

    u64             scan_count[DIC120_PGA_COUNT];
    u16             resource_requested;
    void *          resource[DIC120_PGA_COUNT];
    atomic_t        irq_handler_installed;
    struct tasklet_struct  irq_tasklet1;
#ifndef _REAL_DEVICE
    struct tasklet_struct  irq_emul_tasklet;
#endif
    u64             irq_count  ;
    u64             irq_handled;


    int             irq_num;
    u32             tmr_jiff_add;
    u64             seq_number;
    /*end of common fields*/
    struct dqueue         _queue_internal;
    int             base_port;
    u8              reg_intr;
    int             pga_found;
    int             pga_base_port  [DIC120_PGA_COUNT];
    int             pga_count;
    p55_pga_param   pga_params    [DIC120_PGA_COUNT];



//device info data
    u16            pga_id         [DIC120_PGA_COUNT];
    u16            current_pga_id [DIC120_PGA_COUNT];
    struct         cdev _cdev;
} dic120dev,* lpdic120dev;

extern lpdic120dev get_first_device(void);
extern lpdic120dev get_last_device (void);

#ifndef SA_SHIRQ
#define SA_SHIRQ IRQF_SHARED
#endif

extern int  dic120dev_setup             (volatile lpdic120dev dev, int base_port, int irq, int avg, u8 gains[4], u16 channels_mode);
extern int  dic120dev_resource_init     (volatile lpdic120dev dev);

extern int  dic120dev_resource_release  (volatile lpdic120dev dev);
extern int  dic120dev_make_info         (volatile lpdic120dev dev);
extern int  dic120dev_add_pga        (volatile lpdic120dev dev, lpp55_pga_param pp);
extern int  dic120dev_clear_pga         (volatile lpdic120dev dev);

extern int  dic120dev_start             (volatile lpdic120dev dev, int start, int scan_freq);
extern void dic120dev_data_to_queue     (volatile lpdic120dev dev);
extern void dic120dev_irq_tasklet1      (unsigned long arg);
extern void dic120dev_timer             (unsigned long arg);

#ifndef _REAL_DEVICE
extern void dic120dev_irq_emul_tasklet  (unsigned long arg);
extern int  dic120dev_test              (lpdic120dev dev,int mode);
#endif





static __always_inline  int dic120dev_is_device(volatile lpdic120dev dev)
{
    if(dev && dev->isize == sizeof(*dev)) return 1;
    return 0;
}




#endif
