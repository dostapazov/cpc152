#ifndef __DIC120_H__
#define __DIC120_H__
/*
 Ostapenko D.V. doostapazov@gmail.com
 Fastwell DIC120 device driver
 common defines
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/version.h>
#include "dic120hw.h"


#define LICENSE_TYPE "GPL"
#define AUTHOR "Ostapenko D.V. dostapazov@gmail.com Azov 20016"
#define DESCR  "Fastwell dic120 Device driver"


#define DIC120_MAX_DEVICES  16


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

#define DBG_TRACE(t,...) do{ if(DEBUG_ACTIVE){printk(t "DIC120:" __VA_ARGS__);}}while(0)
#define TRACE(t,...) do{printk(t "DIC120:" __VA_ARGS__);}while(0)


struct dic120_dev_params
{
    int params[3];
};


extern char *  name_dev;
extern int     dic120_open         (struct inode * pinode,struct file* pfile);
extern int     dic120_close        (struct inode * pinode,struct file* pfile);
extern int     dic120_ioctl        (struct inode *inode, struct file *filp, unsigned int cmd,unsigned long arg);
extern long    dic120_compat_ioctl (struct file *filp, unsigned int cmd,unsigned long arg);
extern ssize_t dic120_read         (struct file *pfile, char __user *buf, size_t buf_len, loff_t *poff);
extern ssize_t dic120_write        (struct file *pfile, const char __user *buf, size_t buf_len, loff_t *poff);
extern unsigned int dic120_poll    (struct file * pfile, struct poll_table_struct *pts);

extern int     dic120_proc_open    (struct inode * pinode,struct file* pfile);
extern int     dic120_proc_close   (struct inode * pinode,struct file* pfile);
extern ssize_t dic120_proc_read    (struct file *pfile, char __user *buf, size_t buf_len, loff_t *poff);



#endif
