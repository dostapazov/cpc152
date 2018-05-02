
#include "dic120.h"
#include <dic120ioctl.h>
#include <linux/uaccess.h>
#include <linux/wait.h>




int dic120_open (struct inode * pinode,struct file* pfile)
{
    int result = 0;
    lpdic120dev  dev = NULL;
    DBG_TRACE(KERN_DEBUG, "Open device number %d\n",(int)MINOR(pinode->i_cdev->dev));
    dev = container_of(pinode->i_cdev, struct _dic120dev, _cdev);
    if(dev && dev->isize == sizeof(*dev) )
    {
        if(dev->resource && (0!=atomic_read(&dev->irq_handler_installed) || dev->irq_num<DIC120_FIRST_IRQ))
        {
            pfile->private_data = dev;
            dqueue_reset(&dev->queue);
            DBG_TRACE(KERN_DEBUG, "DIC120:sizeof dev struct = %d\n",(int)sizeof(*dev));
        }
        else
            result = -ENODEV;
    }
    else
    {

        result = -EINVAL ;
        if(dev) TRACE(KERN_DEBUG,"Real size %d  pointer size %d\n",(int)sizeof(*dev),dev->isize);

    }
    return result;
}

int dic120_close(struct inode * pinode,struct file* pfile)
{
    int result = 0;
    lpdic120dev  dev = (lpdic120dev)pfile->private_data;
    DBG_TRACE(KERN_DEBUG ,"DIC120:Close dic120 number %d\n",(int)MINOR(pinode->i_cdev->dev));
    if(dev && dev->isize == sizeof(*dev))
    {
        dic120dev_start(dev,0,0);
        DBG_TRACE(KERN_DEBUG,"dic120dev::isize %d\n",dev->isize);
    }
    else
        printk(KERN_ALERT "file::private_data is invalid\n");


    return result;
}


ssize_t dic120_read  (struct file * pfile, char __user * buf, size_t buf_len, loff_t * poff)
{
    ssize_t ret = 0;
    lpdic120_input idata;
    s32   data_len;
    lpdic120dev  dev = (lpdic120dev)pfile->private_data;
    if(dic120dev_is_device(dev))
    {
        if(dqueue_is_empty(&dev->queue))
        {
            if(pfile->f_flags & O_NONBLOCK)
                return -EAGAIN;
            else
                wait_event_interruptible(dev->_wait_queue,dev->_wait_flag!=0);
        }

        dev->_wait_flag = 0;
        if(dqueue_is_empty(&dev->queue)) return -EAGAIN;

        dev->queue_reads++;
        DBG_TRACE(KERN_DEBUG,"Read begin  buf_len  %d\n",(int)buf_len);
        do {
            idata = (lpdic120_input)dqueue_get_ptr(&dev->queue,&data_len) ;
            //data_len = dqueue_get(&dev->queue,(u8*)&idata,sizeof(idata),0);
            if(idata && data_len && data_len<=buf_len)
            {
                copy_to_user(buf,(void*)idata,data_len);
                dqueue_remove_first(&dev->queue);
                ret    += data_len;
                buf    += data_len;
                buf_len-= data_len;
                *poff   += data_len;
                ++dev->queue_entry_reads;
            }
        } while(data_len && data_len<=buf_len);
        DBG_TRACE(KERN_DEBUG,"Read end  rd_count  %d ,qtail %d,qhead %d\n",(int)ret,dev->queue.q_tail,dev->queue.q_head);
    }
    return ret;

}

unsigned int dic120_poll (struct file * pfile, struct poll_table_struct *pts)
{
    unsigned int ret = 0;
    lpdic120dev  dev = (lpdic120dev)pfile->private_data;
    if(dev && dev->isize == sizeof(*dev))
    {
        poll_wait(pfile,&dev->_wait_queue,pts);
        if(!dqueue_is_empty(&dev->queue))
            ret |=  (POLLIN | POLLRDNORM);
    }

    return ret;
}

ssize_t dic120_write (struct file *pfile, const char __user *buf, size_t buf_len, loff_t *poff)
{
    ssize_t ret = buf_len;
    dic120_input _ib;
    lpdic120dev  dev = (lpdic120dev)pfile->private_data;
    if(dev && dev->isize == sizeof(*dev))
    {
        DBG_TRACE(KERN_DEBUG,"Write begin buf_len %d\n",(int)buf_len);
        copy_from_user(&_ib,buf,MIN(sizeof(_ib),buf_len));
        dqueue_add(&dev->queue,(u8*)&_ib,sizeof(_ib));
        DBG_TRACE(KERN_DEBUG,"Write end\n");
    }
    return ret;
}




long dic120_compat_ioctl  (struct file *filp, unsigned int cmd,unsigned long arg)
{
    long ret = 0;
    int ioc_size;
    lpdic120dev  dev = (lpdic120dev)filp->private_data;
    DBG_TRACE(KERN_DEBUG, "Begin IOCTL\n");
    if(dic120dev_is_device(dev))
    {

        if (_IOC_TYPE(cmd) != DIC120_IOC_MAGIC)   {
            return -ENOTTY;
        }
        if (_IOC_NR  (cmd)  > DIC120_IOC_MAXNR)   {
            return -ENOTTY;
        }

        /*
         * the direction is a bitmask, and VERIFY_WRITE catches R/W
         * transfers. `Type' is user-oriented, while
         * access_ok is kernel-oriented, so the concept of "read" and
         * "write" is reversed
         */
        ioc_size =  _IOC_SIZE(cmd);
        if (_IOC_DIR(cmd) & _IOC_READ)
            ret = !access_ok(VERIFY_WRITE, (void __user *)arg, ioc_size);
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
            ret =  !access_ok(VERIFY_READ, (void __user *)arg, ioc_size);
        if (ret) {
            DBG_TRACE(KERN_DEBUG,"Invalid access check  IOC %d\n",_IOC_NR(cmd));
            return -EFAULT;
        }
        DBG_TRACE(KERN_DEBUG,"IOCTL CMD %u size %d\n",_IOC_NR(cmd),ioc_size);
        switch(cmd)
        {
        case DIC120_IOC_WORKSTART: {
            int scan_freq = 0;
            get_user(scan_freq,(int __user *)arg);
            if(!scan_freq) scan_freq = HZ;
            ret = dic120dev_start(dev,1,scan_freq);
        }
        break;
        case DIC120_IOC_WORKSTOP :
            ret = dic120dev_start(dev,0,0);
            break;
        case DIC120_IOC_CLEAR_PGA :
            ret = dic120dev_clear_pga(dev);
            break;
        case DIC120_IOCW_ADD_PGA:
        {
            p55_pga_param pp;
            copy_from_user(&pp,(void __user*)arg,sizeof(pp));
            ret = dic120dev_add_pga(dev,&pp);
        }
        break;
        case DIC120_IOC_TEST:
#ifndef _REAL_DEVICE
        {
            int mode;
            get_user(mode,(int __user *)arg);
            dic120dev_test(dev,mode);
            ret = 0;
        }
#else
        ret = -ENOSYS;
#endif
        break;
        case DIC120_IOCR_GET_VERSION:
        {
            int ver = _DIC120MOD_VER;
            put_user(ver,(int __user *)arg);
        }
        break;

        default :
            ret = -ENOSYS;
            break;
        }

    }
    else
    {
        ret = -EBADHANDLE;
        TRACE(KERN_ALERT,"IOCTL filp::private data is invalid\n");
    }

    return ret;
}


int dic120_ioctl (struct inode *inode, struct file *filp, unsigned int cmd,unsigned long arg)
{

    return dic120_compat_ioctl(filp,cmd,arg);
}

// proc_fs procedure


lpdic120dev get_pde_data(struct inode * pinode)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
    return (lpdic120dev)PDE_DATA(pinode);
#else
    struct proc_dir_entry * e = PDE(pinode);
    return (lpdic120dev)e->data;
#endif
}

int     dic120_proc_open    (struct inode *  pinode,struct file* pfile)
{
    int ret = 0;
    lpdic120dev  dev = get_pde_data(pinode);
    if(dev && dev->isize == sizeof(*dev))
    {
        pfile->private_data = dev;
        //DBG_TRACE(KERN_DEBUG,"!!! proc file dic120dev::isize = %d\n",dev->isize);
        if(sizeof(*dev) == dev->isize )
        {
            if(NULL == dev->info)
            {
                dev->info_step = 0;
                dev->info_len  = 0;
                dev->info_size = PAGE_SIZE;
                dev->info      = kmalloc(dev->info_size,GFP_KERNEL);
                if(!dev->info)
                    ret = -ENOMEM;
            }
            else
                ret = -EBUSY;
        }
        else
            ret = -ENODEV;
    }
    else
        ret = -ENODEV;

    return ret;
}

int     dic120_proc_close   (struct inode * pinode,struct file* pfile)
{
    int ret = 0;
    lpdic120dev dev = (lpdic120dev)pfile->private_data;
    if(dev && sizeof(*dev)==dev->isize)
    {
        if(dev->info)
            kfree(dev->info);
        dev->info = NULL;
        DBG_TRACE(KERN_DEBUG,"Close proc file port 0x%X\n",dev->base_port);
    }
    else
        TRACE(KERN_ALERT,"Invalid device data\n");
    return ret;

}


ssize_t dic120_proc_read    (struct file *pfile, char __user *buf, size_t buf_len, loff_t *poff)
{
    ssize_t ret = 0;
    ssize_t len = 0;
    int     ioffset = 0;
    lpdic120dev  dev = (lpdic120dev)pfile->private_data;
    if(dev && dev->isize == sizeof(*dev) && (dev->info_step>=0 || dev->info_len))
    {
        do {
            len = dev->info_len;
            if(!len)
                len = dic120dev_make_info(dev);
            if(len)
            {
                if(dev->info_len>(int)buf_len)
                {
                    len = buf_len;
                    ioffset = len;

                }
                copy_to_user(buf,dev->info,len);
                ret+=len;
                buf+=len;
                buf_len-=len;
                *poff+=len;
                if(ioffset)
                {
                    dev->info_len-=len;
                    memmove(dev->info,dev->info+ioffset,dev->info_len);
                    return ret;
                }
                else
                    dev->info_len = 0;
            }
        } while(len);
    }
    return ret;
}

int dic120dev_make_info(volatile lpdic120dev dev)
{
#ifndef _REAL_DEVICE
    static const char * dbg_str = "-DEBUG SIMULATE VERSION";
#else
    static const char * dbg_str = "";
#endif

    dev->info_len = 0;
    if(dev->info_step<0) return 0;
    if(dev->pga_found)
    {

        switch(dev->info_step)
        {
        case 0:
            dev->info_len  = snprintf(dev->info,dev->info_size,"Ostapenko D.V. dostapazov@gmail.com 2016\n"
                                      "Fastwell DIC120-P55 Module Version %d%s\n"
                                      "Device base address %04X h IRQ %d %s : %s\n"
                                      ,_DIC120MOD_VER,dbg_str
                                      ,dev->base_port-DIC120_BASE_ADDR_OFFSET
                                      ,(int)dev->irq_num,dev->irq_num >= DIC120_FIRST_IRQ ? "":"[scan-mode]"
                                      ,atomic_read(&dev->active) ? "worked" : "stopped"
                                     );
            break;
        case 1:
            dev->info_len =snprintf(dev->info,dev->info_size,"Timer count:\t%Lu"
                                    ", Work time:\t%Lu, Scan period:\t%d ms"
                                    "\nIRQ: count\t%Lu, handled\t%Lu"
                                    "\nINTR REG value\t%02X"
                                    "\n"
                                    ,dev->dev_timer_count
                                    ,(dev->dev_timer_count*dev->tmr_jiff_add)/HZ
                                    ,(int)(dev->tmr_jiff_add*HZ)/1000
                                    ,dev->irq_count,dev->irq_handled
                                    ,(unsigned)dev->reg_intr
                                   );


            break;
        case 2:
        {
            int i;
            dev->info_len =snprintf(dev->info,dev->info_size,"Enabled pga count %d\n",dev->pga_count);
            for(i = 0; i<dev->pga_count; i++)
            {
                dev->info_len += snprintf(dev->info+dev->info_len,dev->info_size-dev->info_len
                                          ,"Number %d port:%4X h, debounce:%02X, fronts:A%02X-B%02X-C%02X, irqs:A%02X-B%02X-C%02X\n"
                                          ,(int)dev->pga_params[i].number
                                          ,(unsigned)dev->pga_base_port[dev->pga_params[i].number]
                                          ,(unsigned)dev->pga_params[i].debounce
                                          ,(unsigned)dev->pga_params[i].fronts[0],(unsigned)dev->pga_params[i].fronts[1],(unsigned)dev->pga_params[i].fronts[2]
                                          ,(unsigned)dev->pga_params[i].irqs  [0],(unsigned)dev->pga_params[i].irqs  [2],(unsigned)dev->pga_params[i].irqs  [2]
                                         );

            }
            i = MIN(79, dev->info_size-dev->info_len);
            if(i>0)
            {
                memset(dev->info+dev->info_len,'-',i);
                dev->info_len += i;
                dev->info[dev->info_len-1] = '\n';
            }

        }
        break;
        case 3 :
        case 4 :
        case 5:
        case 6:
        {

            int   i;
            int   enabled = 0;
            u16   code;
            char * ptr;
            int   pga_num   = dev->info_step-3;
            code          = dev->pga_id[pga_num];
            ptr           = (char*)&code;

            dev->info_len = snprintf(dev->info,dev->info_size
                                     ,"PGA N %d - ID:%04X %c-%d [%s]"
                                     ,pga_num
                                     ,(unsigned)(dev->pga_base_port [pga_num])
                                     ,ptr[0],(int)ptr[1]
                                     ,code == dev->current_pga_id[pga_num] ? "good" : "bad"
                                    );
            for(i = 0 ; i<dev->pga_count && !enabled; i++)
                enabled = ((pga_num == (int)dev->pga_params[i].number) ? 1 : 0);
            if(enabled)
            {
                dev->info_len += snprintf(dev->info+dev->info_len,dev->info_size-dev->info_len
                                          ,": scan count %Lu"
                                          ,dev->scan_count[pga_num]
                                         );

            }
            else
            {
                dev->info_len += snprintf(dev->info+dev->info_len,dev->info_size-dev->info_len,": disabled");
            }
            dev->info_len += snprintf(dev->info+dev->info_len,dev->info_size-dev->info_len,"\n");

        }
        break;
        case 7:
            dev->info_len = snprintf(dev->info,dev->info_size,
                                     "Queue : count\t%u, total\t%Lu, lost\t%u, overflow\t%u\n"
                                     "Reads : count\t%Lu, entryes\t%Lu\n"
                                     ,dqueue_get_qcount(&dev->queue)
                                     ,dev->queue_total
                                     ,dev->queue_lost
                                     ,dev->queue_overflow
                                     ,dev->queue_reads
                                     ,dev->queue_entry_reads
                                    );
            break;
        default:
        {
            dev->info_len  = 0;
            dev->info_step = -2;
        }
        break;
        }
        ++dev->info_step;
    }
    else
    {

        if(dev->info_step>=0)
        {
            dev->info_len  = snprintf(dev->info,dev->info_size,"No PGA found at port 0x%03X\n",dev->base_port);
            dev->info_step = -1;
        }
    }

    return dev->info_len;
}

