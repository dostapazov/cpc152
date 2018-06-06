#ifndef __AIC124_H__
#define __AIC124_H__
/*
 Ostapenko D.V. doostapazov@gmail.com
 Fastwell AIC124 device driver
 common defines
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/version.h>
#include "aic124hw.h"


#define LICENSE_TYPE "GPL"
#define AUTHOR "Ostapenko D.V. dostapazov@gmail.com Azov 20016"
#define DESCR  "Fastwell aic124 Device driver"


#define AIC124_MAX_DEVICES  16


#define LOBYTE(w)  (w&0xFF)
#define HIBYTE(w)  ((w>>8)&0xFF)
#define LOWORD(dw) (w&0xFFFF)
#define HIWORD(dw) ((w>>16)&0xFFFF)

#define MIN(a,b)   ((a)<(b) ? (a) : (b))
#define MAX(a,b)   ((a)>(b) ? (a) : (b))

#ifndef bzero
#define bzero(mem,sz) memset(mem,0,sz)
#endif

#ifdef _DEBUG
#define DEBUG_ACTIVE 1
#else
#define DEBUG_ACTIVE 0
#endif

#define DBG_TRACE(t,...) do{ if(DEBUG_ACTIVE){printk(t "AIC124:" __VA_ARGS__);}}while(0)
#define TRACE(t,...) do{printk(t "AIC124:" __VA_ARGS__);}while(0)


struct aic124_dev_params
{
    int params[3];
};


extern char *  name_dev;
extern int     aic124_open         (struct inode * pinode,struct file* pfile);
extern int     aic124_close        (struct inode * pinode,struct file* pfile);
extern int     aic124_ioctl        (struct inode *inode, struct file *filp, unsigned int cmd,unsigned long arg);
extern long    aic124_compat_ioctl (struct file *filp, unsigned int cmd,unsigned long arg);
extern ssize_t aic124_read         (struct file *pfile, char __user *buf, size_t buf_len, loff_t *poff);
extern ssize_t aic124_write        (struct file *pfile, const char __user *buf, size_t buf_len, loff_t *poff);
extern unsigned int aic124_poll    (struct file * pfile, struct poll_table_struct *pts);

extern int     aic124_proc_open    (struct inode * pinode,struct file* pfile);
extern int     aic124_proc_close   (struct inode * pinode,struct file* pfile);
extern ssize_t aic124_proc_read    (struct file *pfile, char __user *buf, size_t buf_len, loff_t *poff);



#endif
