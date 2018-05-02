
#include "aic124.h"
#include <aic124ioctl.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/delay.h>




int aic124_open (struct inode * pinode,struct file* pfile)
{
    int result = 0;
    lpaic124dev  dev = NULL;
    DBG_TRACE(KERN_DEBUG, "Open device number %d\n",(int)MINOR(pinode->i_cdev->dev));
    dev = container_of(pinode->i_cdev, struct _aic124dev, _cdev);
    if(dev && dev->isize == sizeof(*dev) )
    {
        if(dev->resource && 0!=atomic_read(&dev->irq_handler_installed))
        {
            pfile->private_data = dev;
            dqueue_reset(&dev->queue);
            DBG_TRACE(KERN_DEBUG, "AIC124:sizeof dev struct = %d\n",(int)sizeof(*dev));
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

int aic124_close(struct inode * pinode,struct file* pfile)
{
    int result = 0;
    lpaic124dev  dev = (lpaic124dev)pfile->private_data;
    DBG_TRACE(KERN_DEBUG ,"AIC124:Close aic124 number %d\n",(int)MINOR(pinode->i_cdev->dev));
    if(dev && dev->isize == sizeof(*dev))
    {
        aic124dev_start(dev,0,0);
        DBG_TRACE(KERN_DEBUG,"aic124dev::isize %d\n",dev->isize);
    }
    else
        printk(KERN_ALERT "file::private_data is invalid\n");


    return result;
}


ssize_t aic124_read  (struct file * pfile, char __user * buf, size_t buf_len, loff_t * poff)
{
    ssize_t ret = 0;
    lpaic124_input idata;
    s32   data_len;
    lpaic124dev  dev = (lpaic124dev)pfile->private_data;
    if(dev && dev->isize == sizeof(*dev))
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
            idata = (lpaic124_input)dqueue_get_ptr(&dev->queue,&data_len) ;
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

unsigned int aic124_poll (struct file * pfile, struct poll_table_struct *pts)
{
    unsigned int ret = 0;
    lpaic124dev  dev = (lpaic124dev)pfile->private_data;
    if(dev && dev->isize == sizeof(*dev))
    {
        poll_wait(pfile,&dev->_wait_queue,pts);
        if(!dqueue_is_empty(&dev->queue))
            ret |=  (POLLIN | POLLRDNORM);
    }

    return ret;
}

ssize_t aic124_write (struct file *pfile, const char __user *buf, size_t buf_len, loff_t *poff)
{
    ssize_t ret = buf_len;
    aic124_input _ib;
    lpaic124dev  dev = (lpaic124dev)pfile->private_data;
    if(dev && dev->isize == sizeof(*dev))
    {
        DBG_TRACE(KERN_DEBUG,"Write begin buf_len %d\n",(int)buf_len);
        copy_from_user(&_ib,buf,MIN(sizeof(_ib),buf_len));
        dqueue_add(&dev->queue,(u8*)&_ib,sizeof(_ib));
        DBG_TRACE(KERN_DEBUG,"Write end\n");
    }
    return ret;
}


int aic124_add_aib(lpaic124dev dev,void __user *buf,int ioc_size)
{

    int ret = -EBADTYPE;
    aib_param ap;
    if(ioc_size == sizeof(ap))
    {
        ret = -ENODEV;
        if(aic124dev_is_device(dev))
        {
            copy_from_user(&ap,buf,ioc_size);
            ret = aic124dev_add_aib(dev,&ap);

        }
        else
            TRACE(KERN_DEBUG,"no device \n");
    }
    else
        TRACE(KERN_DEBUG,"add_aib bad size of data %d instead of %d\n",ioc_size,(int) sizeof(ap));
    return ret;

}

int aic124_add_channel(lpaic124dev dev,void __user *buf,int ioc_size)
{

    int ret = -EBADTYPE;
    aic124_channel_param cp;
    if(ioc_size == sizeof(cp))
    {
        ret = -ENODEV;
        if(aic124dev_is_device(dev))
        {
            copy_from_user(&cp,buf,ioc_size);
            ret = aic124dev_add_channel(dev,&cp);
        }
        else
            TRACE(KERN_DEBUG,"no device \n");
    }
    else
        TRACE(KERN_DEBUG,"set_base_param bad size of data %d instead of %d\n",ioc_size,(int) sizeof(cp));
    return ret;

}

int  aic124_set_dac_values(lpaic124dev dev,void __user *buf,int ioc_size)
{
    int ret = -EBADTYPE;
    aic124_dacvalues dv;
    lpaic124_dac_output_t odac;
    if(aic124dev_is_device(dev) && sizeof(dv) == ioc_size)
    {
        if(0==atomic_read(&dev->active))
        {
            copy_from_user(&dv,buf,ioc_size);
            odac = dev->dac_output;
            if(dv.dac_num)  ++odac;
            odac->data_count = dv.dac_values_count;
            memcpy(odac->data,dv.dac_values,sizeof(odac->data[0])*odac->data_count);
#ifdef _DEBUG
            {
                char * otext = (char*)kmalloc(16+5*odac->data_count,GFP_KERNEL);
                int i = 0,len =0;
                for(; i<odac->data_count; i++)
                {
                    len += sprintf(otext+len,"%hX ",odac->data[i]);
                }
                DBG_TRACE(KERN_DEBUG,"DAC%d - %s\n",dv.dac_num,otext);
                kfree(otext);
            }
#endif

            ret = 0;
        }
        else
            ret = -EBUSY;
    }
    return ret;

}

long aic124_compat_ioctl  (struct file *filp, unsigned int cmd,unsigned long arg)
{
    long ret = 0;
    int ioc_size;
    lpaic124dev  dev = (lpaic124dev)filp->private_data;
    DBG_TRACE(KERN_DEBUG, "Begin IOCTL\n");
    if(aic124dev_is_device(dev))
    {

        if (_IOC_TYPE(cmd) != AIC124_IOC_MAGIC)   {
            return -ENOTTY;
        }
        if (_IOC_NR  (cmd)  > AIC124_IOC_MAXNR)   {
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
        case AIC124_IOCW_ADD_CHANNEL :
            ret = aic124_add_channel(dev,(void __user *)arg,ioc_size);
            break;
        case AIC124_IOCW_ADD_AIB:
            ret = aic124_add_aib(dev,(void __user *)arg,ioc_size);
            break;
        case AIC124_IOCW_SET_DAC_VALUES:
            ret = aic124_set_dac_values(dev,(void __user *)arg,ioc_size);
            break;
        case AIC124_IOC_WORKSTART: {
            int scan_freq = 0;
            get_user(scan_freq,(int __user *)arg);
            if(!scan_freq) scan_freq = HZ;
            ret = aic124dev_start(dev,1,scan_freq);
        }
        break;
        case AIC124_IOC_WORKSTOP :
            ret = aic124dev_start(dev,0,0);
            break;
        case AIC124_IOC_CLEAR_CHANNELS :
            ret = aic124dev_clear_channels(dev);
            break;
        case AIC124_IOC_TEST:
#ifndef _REAL_DEVICE
        {
            int mode;
            get_user(mode,(int __user *)arg);
            aic124dev_test(dev,mode);
            ret = 0;
        }
#else
        ret = -ENOSYS;
#endif
        break;
        case AIC124_IOCR_GET_VERSION:
        {
            int ver = _AIC124MOD_VER;
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


int aic124_ioctl (struct inode *inode, struct file *filp, unsigned int cmd,unsigned long arg)
{

    return aic124_compat_ioctl(filp,cmd,arg);
}

// proc_fs procedure


lpaic124dev get_pde_data(struct inode * pinode)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
    return (lpaic124dev)PDE_DATA(pinode);
#else
    struct proc_dir_entry * e = PDE(pinode);
    return (lpaic124dev)e->data;
#endif
}

int     aic124_proc_open    (struct inode *  pinode,struct file* pfile)
{
    int ret = 0;
    lpaic124dev  dev = get_pde_data(pinode);
    if(dev && dev->isize == sizeof(*dev))
    {
        pfile->private_data = dev;
        //DBG_TRACE(KERN_DEBUG,"!!! proc file aic124dev::isize = %d\n",dev->isize);
        if(sizeof(*dev) == dev->isize )
        {
            if(NULL == dev->info)
            {
                dev->info_step = 0;
                dev->info_len  = 0;
                dev->info_size = PAGE_SIZE;
                dev->info = kmalloc(dev->info_size,GFP_KERNEL);
                if(NULL == dev->info)
                    ret = -ENOMEM;
            }
            else
                ret =-EBUSY;
        }
        else
            ret = -ENODEV;
    }
    else
        ret = -ENODEV;

    return ret;
}

int     aic124_proc_close   (struct inode * pinode,struct file* pfile)
{
    int ret = 0;
    lpaic124dev dev = (lpaic124dev)pfile->private_data;
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


ssize_t aic124_proc_read    (struct file *pfile, char __user *buf, size_t buf_len, loff_t *poff)
{
    ssize_t ret = 0;
    ssize_t len = 0;
    int     ioffset = 0;
    lpaic124dev  dev = (lpaic124dev)pfile->private_data;
    if(dev && dev->isize == sizeof(*dev) && (dev->info_step>=0 || dev->info_len))
    {
        do {
            len = dev->info_len;
            if(!len)
                len = aic124dev_make_info(dev);
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

int aic124dev_make_info(volatile lpaic124dev dev)
{
#ifndef _REAL_DEVICE
    static const char * dbg_str = " - DEBUG SIMULATE VERSION";
#else
    static const char * dbg_str = "";
#endif

    dev->info_len = 0;
    if(dev->info_step<0) return 0;
    if(dev->id[0] == 'A')
    {
        switch(dev->info_step)
        {
        case 0:
            dev->info_len  = snprintf(dev->info,dev->info_size,"Ostapenko D.V. dostapazov@gmail.com 2016\n"
                                      "Module Version %d%s\nFastwell AIC124 v %d.%d DAC mode %X\n"
                                      ,_AIC124MOD_VER,dbg_str
                                      ,(u16)dev->ver[0]
                                      ,(u16)dev->ver[1]
                                      ,(u16)dev->id[1]);
            break;
        case 1 :
            dev->info_len  = snprintf(dev->info,dev->info_size,"Serial number %02X-%02X-%02X-%02X-[%02X]\n"
                                      ,(u16)dev->serial_num[0],
                                      (u16)dev->serial_num[1],
                                      (u16)dev->serial_num[2],
                                      (u16)dev->serial_num[3],
                                      (u16)dev->serial_num[4]
                                     );
            break;
        case 2 :
            dev->info_len = snprintf(dev->info,dev->info_size
                                     ,"Base port 0x%04X, irq number %u %s active\n"
                                     "Test signals [ %03X ] [%03X ]\n"
                                     ,dev->base_port,dev->irq_num,atomic_read(&dev->active)==0 ?  "not ":""
                                     ,atomic_read(&dev->test_signals[0]),atomic_read(&dev->test_signals[1])
                                    );
            break;
        case 3:
            dev->info_len = snprintf(dev->info,dev->info_size,"DAC0 value\t 0x%X  number %d from %d\nDAC1 value\t 0x%X  number %d from %d\n"
                                     ,dev->dac_output[0].value&AIC124_DAC_VALUE_MASK,dev->dac_output[0].data_pos,dev->dac_output[0].data_count
                                     ,dev->dac_output[1].value&AIC124_DAC_VALUE_MASK,dev->dac_output[1].data_pos,dev->dac_output[1].data_count
                                    );
            break;

        case 4 :
        {
            u32 p_int= 0,p_point = 0;
            u64 percent = 10000;
            int irq_per_sec = 0;
            int tm_int = dev->dev_timer_count*dev->tmr_jiff_add/HZ;
            if(dev->scan_count)
            {
                percent *= (u64)dev->scan_over_time_count;
                percent /= dev->scan_count;
                p_int   = (u32) (percent/(u64)100);
                p_point = (u32) ((u32)percent%(u32)10000);
            }

            if(tm_int)
                irq_per_sec =  div_u64(dev->irq_handled,tm_int);
            dev->info_len = snprintf(dev->info,dev->info_size,
                                     "Interrupt: total\t%Lu,  handled\t%Lu, per second\t%u\n"
                                     "Scan : count\t%Lu overtime\t%u (%02u.%02u %%), scan-period:\t%d ms\n"

                                     ,dev->irq_count,dev->irq_handled,irq_per_sec
                                     ,dev->scan_count,dev->scan_over_time_count,p_int,p_point,dev->tmr_jiff_add*(1000/HZ)

                                    );
        }
        break;
        case 5 :
            dev->info_len = snprintf(dev->info,dev->info_size,"timer_count\t%Lu, work time\t%Lu sec\nHZ\t%d,LPG %ld\n"
                                     ,dev->dev_timer_count,(dev->dev_timer_count*dev->tmr_jiff_add)/HZ,HZ,loops_per_jiffy);
            break;
        case 6 :
            dev->info_len = snprintf(dev->info,dev->info_size,"Queue count:\t%d, total count:\t%Lu, max count:\t%d, qsize:\t%d\n"
                                     "total lost:\t%u, overflow\t%u, reads:\t%Lu,entry reads:\t%Lu\n"
                                     "internal: queue count:t%u, lost:\t%u  \n"
                                     ,dqueue_get_qcount(&dev->queue)
                                     ,dev->queue_total
                                     ,dev->queue.q_count_max
                                     ,dev->queue.q_size
                                     ,dev->queue_lost
                                     ,dev->queue_overflow
                                     ,dev->queue_reads
                                     ,dev->queue_entry_reads
                                     ,dqueue_get_qcount(&dev->_queue_internal)
                                     ,dev->_internal_lost

                                    );
            break;
        case 7:
            if(dev->channels_count)
            {
                int i;
                dev->info_len = snprintf(dev->info,dev->info_size,"channels count %d auto scan %s\n", dev->channels_count,dev->auto_scan ? "on":"off");
                for( i = 0; i<dev->channels_count && dev->info_len<dev->info_size; i++)
                {
                    dev->info_len += snprintf(dev->info+dev->info_len,dev->info_size-dev->info_len,
                                              "%d : value %02X [ number %d mode = %c ] gain-%u\n"
                                              ,i
                                              ,(unsigned)dev->channel_numbers[i]
                                              ,(unsigned)dev->channel_numbers[i]&AIC124_MUX_MASK
                                              ,(dev->channel_numbers[i]&AIC124_MUX_SINGLE) ?'S':'D'
                                              ,(unsigned)aic124dev_get_channel_gain(dev,(dev->channel_numbers[i]&AIC124_MUX_MASK))
                                             );


                }
                if(dev->aib_use)
                    dev->info_len+= snprintf(dev->info+dev->info_len,dev->info_size-dev->info_len,"AIBS : auto scan %s \n",dev->auto_scan_aibs ? "on":"off");
            }
            break;
        case 8 :
        case 9 :
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
            if(dev->aib_use)
            {
                int i = dev->info_step-8;
                if(i<dev->channels_count)
                {
                    int j;
                    dev->info_len = snprintf(dev->info,dev->info_size,
                                             "%d - number %d chanels count %d\n"
                                             ,i
                                             ,(unsigned)dev->aib_channels[i].aib_number
                                             ,(unsigned)dev->aib_channels[i].count
                                            );

                    for(j = 0; j<dev->aib_channels[i].count && dev->info_len<dev->info_size; j++)
                    {
                        dev->info_len += snprintf(dev->info+dev->info_len,dev->info_size-dev->info_len,
                                                  "%c%02X "
                                                  ,(dev->aib_channels[i].channels[j]&AIC124_MUX_SINGLE) ? 'S':'D'
                                                  ,(unsigned)(dev->aib_channels[i].channels[j]&AIC124_MUX_MASK)
                                                 );
                    }

                    if(dev->info_len<dev->info_size)
                        dev->info[dev->info_len++] = '\n';
                }
            }
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
            dev->info_len  = snprintf(dev->info,dev->info_size,"Module Version %d%s\n",_AIC124MOD_VER,dbg_str);
            if(dev->id[0])
                dev->info_len += snprintf(dev->info+dev->info_len,dev->info_size-dev->info_len,"AIC124 device not found at port %X\n",dev->base_port);
            else
                dev->info_len += snprintf(dev->info+dev->info_len,dev->info_size-dev->info_len,"%s","Not initialized\n");
            dev->info_step = -1;
        }
    }

    return dev->info_len;
}

