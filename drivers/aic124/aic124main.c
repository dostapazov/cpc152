#include "aic124.h"
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include "../common/confparse.h"



static int    major = 0;
static int    minor = 0;

char * dev_name = "aic124";
static int    dev_count;
lpaic124dev  mod_devs = NULL;
static struct proc_dir_entry * dir_entry = NULL;

static char * dev_config   = NULL ;

module_param(major, int, S_IRUGO);
MODULE_PARM_DESC(major, "AIC124 major device number");

module_param_named(config, dev_config, charp , S_IRUGO);
MODULE_PARM_DESC(dev_config, "AIC124 device config file");



lpaic124dev get_first_device(void) {
    return mod_devs;
}
lpaic124dev get_last_device (void) {
    return mod_devs+dev_count;
}


uint64_t __udivdi3(uint64_t num,uint64_t den)
{
    return div64_u64(num,den);
}


static struct file_operations fops = {
    .owner          =    THIS_MODULE,
    .read           =    aic124_read,
    .write          =    aic124_write,
    .unlocked_ioctl =    aic124_compat_ioctl,
    .open           =    aic124_open,
    .release        =    aic124_close,

};


static struct file_operations proc_fops =
{
    .owner          = THIS_MODULE,
    .read           = aic124_proc_read,
    .open           = aic124_proc_open,
    .release        = aic124_proc_close,
};


static int __init aic124_reg_device(lpaic124dev dcur,int number)
{
    int ret = 0;
    dev_t dev;
    char  proc_name[32];
    dcur->isize = sizeof(*dcur);
    dcur->dev_number = number;
    spin_lock_init(&dcur->irq_locker);
    init_waitqueue_head(&dcur->_wait_queue);
    tasklet_init       (&dcur->irq_tasklet1 ,aic124dev_irq_tasklet1,(unsigned long)dcur);
#ifndef _REAL_DEVICE
    tasklet_init       (&dcur->irq_emul_tasklet,aic124dev_irq_emul_tasklet,(unsigned long)dcur);
#endif

    init_timer(&dcur->dev_timer);
    dcur->dev_timer.data     =(unsigned long) dcur;
    dcur->dev_timer.function = aic124dev_timer;
    cdev_init(&dcur->_cdev,&fops);
    dcur->_cdev.owner = THIS_MODULE;
    dcur->_cdev.ops   = &fops;

    dev    = MKDEV(major,number);
    ret = cdev_add(&dcur->_cdev,dev,1);
    if(ret)
        TRACE(KERN_ALERT, "error cdev_add minor %d\n",number);
    else
    {

        DBG_TRACE(KERN_DEBUG, "add cdev major %d minor %d isize = %d\n",(int)MAJOR(dcur->_cdev.dev),MINOR(dcur->_cdev.dev),dcur->isize);
        snprintf(proc_name,sizeof(proc_name),"%s_%d",dev_name,number);
        dcur->proc_entry = proc_create_data(proc_name,S_IFREG | S_IRUGO,dir_entry,&proc_fops,dcur);
        if(!dcur->proc_entry)
        {
            DBG_TRACE(KERN_DEBUG,"Error create proc entry for device %d\n",number);
            ret = -ENOMEM;
        }
        else
        {
            if(dcur->base_port && !aic124dev_resource_init(dcur))
            {
                dqueue_init(&dcur->queue,sizeof(aic124_input),dcur->queue.q_size);
                dqueue_init(&dcur->_queue_internal,sizeof(aic124_input),32);
            }

        }
    }
    return ret;
}


static const char *var_names[] __initdata =
{
    "port=" ,
    "irq="  ,
    "qsize=",
    ""
};

#define CONF_PORT       0
#define CONF_IRQ        1
#define CONF_QSZ        2


static int __init aic124_read_config(char * conf_name,struct aic124_dev_params * dp,int dp_count)
{
    int    ret = 0;
    if(dp)
    {
        char   buf[256];
        int    lno = 0;
        int    len;
        struct file * f = filp_open(conf_name,O_RDONLY,0);
        if(IS_ERR(f))
            TRACE(KERN_ALERT,"Error open config file %s",conf_name);
        else
        {
            loff_t pos = 0;
            do {
                ++lno;
                len = config_file_get_line(f,buf,sizeof(buf)-1,&pos);
                if(len>0 && '#'!=buf[0] && buf[0])
                {
                    bzero(dp,sizeof(*dp));
                    buf[len+1] = 0;

                    if(config_line_get_param(buf,3,var_names,dp->params) == 3)
                    {
                        ++dp;
                        ++ret;
                    }
                    else
                        TRACE(KERN_ALERT,"Not enough device param line %d - %s\n",lno,buf);
                }
            } while(len>=0 && ret<dp_count);
        }
        DBG_TRACE(KERN_DEBUG,"Close config file\n");
        filp_close(f,NULL);

    }
    else
        TRACE(KERN_ALERT,"Read config:Zero pointer for device params\n");


    return ret;

}

/*
static int __init __aic124_read_config(char * conf_name)
{
  char   buf[256];
  int    var_params[3];
  int    dev_num;
  int    lno = 0;
  int    len;
  struct file * f = filp_open(conf_name,O_RDONLY,0);
  lpaic124dev  cur_dev;
  if(IS_ERR(f))
  {
   TRACE(KERN_ALERT,"Error open config file %s",conf_name);
  }
  else
  {
    loff_t pos = 0;
    do{
      ++lno;
       len = config_file_get_line(f,buf,sizeof(buf)-1,&pos);
     if(len>0 && '#'!=buf[0] && buf[0])
       {
         bzero(var_params,sizeof(var_params));
         buf[len+1] = 0;
         dev_num = config_line_get_param(buf,3,var_names,var_params);
         if(dev_num >= 0 && dev_num<dev_count)
           {
              cur_dev    = mod_devs;
              cur_dev   += dev_num;
              cur_dev->base_port  = var_params[CONF_PORT];
              cur_dev->irq_num    = var_params[CONF_IRQ];
              cur_dev->queue.q_size = var_params[CONF_QSZ];
              if(cur_dev->queue.q_size<32)
                  cur_dev->queue.q_size = 32;
              cur_dev->tmr_jiff_add = HZ;
              TRACE(KERN_INFO,"Setup device %d port = 0x%03X, irq = %d,qsize = %d\n",dev_num,cur_dev->base_port,cur_dev->irq_num,cur_dev->queue.q_size);
              aic124dev_resource_init(cur_dev);
           }
           else
           TRACE(KERN_ALERT,"line %d Error dev number in config_line:\n%s",lno,buf);
       }
    }while(len>=0);

    DBG_TRACE(KERN_DEBUG,"Close config file\n");

    filp_close(f,NULL);
    return 0;
  }

    return -EBADF;

}
*/




static int __init aic124_reg_module(void)
{
    int result;
    dev_t dev;
    lpaic124dev  dcur,dend;
    int    ndev;
    size_t nsize;

    struct aic124_dev_params *dev_params = 0;
    struct aic124_dev_params *dp         = 0;

    nsize = AIC124_MAX_DEVICES*sizeof(struct aic124_dev_params);
    if(!(dev_config && 0==*dev_config))
        dev_config = "/etc/aic124.conf";

    dev_params = (struct aic124_dev_params*)kmalloc(nsize,GFP_KERNEL);
    dev_count  = aic124_read_config (dev_config,dev_params,AIC124_MAX_DEVICES);

    if(major)
    {
        dev    = MKDEV(major,minor);
        result = register_chrdev_region(dev,dev_count,dev_name);

    }
    else
    {
        result = alloc_chrdev_region(&dev,minor,dev_count,dev_name);
        major = MAJOR(dev);
    }
    if(result)
        TRACE(KERN_ALERT,"can't get major %d",major);
    else
    {
        DBG_TRACE(KERN_INFO, "register success major %d\n",major) ;
        dir_entry  = proc_mkdir(dev_name,NULL);
        if(dev_count)
        {
            nsize = sizeof(struct _aic124dev)*dev_count;
            mod_devs = kmalloc(nsize,GFP_KERNEL);
            DBG_TRACE(KERN_DEBUG,"alloc space for mod_devs size %d at 0x%p\n",(int)nsize,(void*)mod_devs);
            bzero(mod_devs,nsize);

            dcur = mod_devs;
            dend = dcur+dev_count;
            dp = dev_params;

            while(dcur<dend)
            {
                dcur->base_port    = dp->params[CONF_PORT];
                dcur->irq_num      = dp->params[CONF_IRQ ];
                dcur->queue.q_size = MAX(dp->params[CONF_QSZ ],32);
                dcur->tmr_jiff_add = HZ;
                ++dp;
                ++dcur;
            }
            kfree(dev_params);
            dev_params = 0;
            ndev = minor;
            dcur = mod_devs;
            dend = dcur+dev_count;

            while(!result && dcur<dend)
            {
                DBG_TRACE(KERN_DEBUG,"Device %d base port 0x%X irq %d qsize %d\n",ndev,dcur->base_port,dcur->irq_num,dcur->queue.q_size);
                result = aic124_reg_device(dcur++,ndev++);
            }
        }
        else
        {
            TRACE(KERN_ALERT,"Devices is not defined. Check the configuration file /etc/aic124.conf\n");
            result = -1;
        }
    }
    if(dev_params) kfree(dev_params);
    return result;
}


static int __init aic124_init(void)
{
    int result = 0;

    TRACE(KERN_INFO,"Initialize module\n");
    result = aic124_reg_module();
    TRACE(KERN_DEBUG, "aic124_init return code %d\n",result);
    return result;
}

static void __exit aic124_release_device(lpaic124dev  dcur)
{
    char proc_name[64];
    DBG_TRACE(KERN_DEBUG"\t","stop device \n");
    aic124dev_start(dcur,0,0);
    udelay(1000);

    if(dcur->dev_timer.data)
    {
        DBG_TRACE(KERN_DEBUG"\t","del timer \n");
        del_timer_sync(&dcur->dev_timer);
    }

    DBG_TRACE(KERN_DEBUG"\t","dqueue release \n");
    dqueue_release(&dcur->queue);
    dqueue_release(&dcur->_queue_internal);
    if(dcur->proc_entry)
    {
        DBG_TRACE(KERN_DEBUG"\t","remove proc entry \n");
        snprintf(proc_name,sizeof(proc_name),"%s_%d",dev_name,dcur->dev_number);
        remove_proc_entry(proc_name,dir_entry);

    }

    aic124dev_resource_release(dcur);
    DBG_TRACE(KERN_DEBUG,"Delete cdev major %d minor %d\n",MAJOR(dcur->_cdev.dev),MINOR(dcur->_cdev.dev));
    cdev_del(&dcur->_cdev);

}

static void __exit aic124_exit(void)
{
    dev_t dev;
    lpaic124dev  dcur,dend;
    int dev_no = 0;

    dev = MKDEV(major,minor);
    DBG_TRACE(KERN_DEBUG,"Unregister chrdev region %d.%d count %d \n",major,minor,dev_count);
    unregister_chrdev_region(dev,dev_count);

    dcur = mod_devs;
    dend = dcur +dev_count;
    while(dcur<dend)
    {
        TRACE(KERN_DEBUG ,"EXIT : Release device %d\n",dev_no++);
        aic124_release_device(dcur);
        ++dcur;
    }
    //if(dir_entry) proc_remove(dir_entry);
    if(dir_entry) remove_proc_entry(dev_name,NULL);
    if(mod_devs ) kfree(mod_devs);


    TRACE(KERN_INFO, "module exit\n");
}

module_init(aic124_init);
module_exit(aic124_exit);

MODULE_LICENSE(LICENSE_TYPE);
MODULE_AUTHOR (AUTHOR);
MODULE_DESCRIPTION(DESCR);

