#include "dic120.h"
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include "../common/confparse.h"



static int    major = 0;
static int    minor = 0;

char * name_dev = "dic120";
static int    dev_count;
lpdic120dev  mod_devs = NULL;
static struct proc_dir_entry * dir_entry = NULL;

static char * dev_config   = NULL ;

module_param(major, int, S_IRUGO);
MODULE_PARM_DESC(major, "DIC120 major device number");

module_param_named(config, dev_config, charp , S_IRUGO);
MODULE_PARM_DESC(dev_config, "DIC120 device config file");



lpdic120dev get_first_device(void) {
    return mod_devs;
}
lpdic120dev get_last_device (void) {
    return mod_devs+dev_count;
}


uint64_t __udivdi3(uint64_t num,uint64_t den)
{
    return div64_u64(num,den);
}


static struct file_operations fops = {
    .owner          =    THIS_MODULE,
    .read           =    dic120_read,
    .write          =    dic120_write,
    .unlocked_ioctl =    dic120_compat_ioctl,
    .open           =    dic120_open,
    .release        =    dic120_close,

};


static struct file_operations proc_fops =
{
    .owner          = THIS_MODULE,
    .read           = dic120_proc_read,
    .open           = dic120_proc_open,
    .release        = dic120_proc_close,
};


static int __init dic120_reg_device(lpdic120dev dcur,int number)
{
    int ret = 0;
    dev_t dev;
    char  proc_name[32];
    dcur->isize = sizeof(*dcur);
    dcur->dev_number = number;
    spin_lock_init(&dcur->irq_locker);
    init_waitqueue_head(&dcur->_wait_queue);
    tasklet_init       (&dcur->irq_tasklet1 ,dic120dev_irq_tasklet1,(unsigned long)dcur);
#ifndef _REAL_DEVICE
    tasklet_init       (&dcur->irq_emul_tasklet,dic120dev_irq_emul_tasklet,(unsigned long)dcur);
#endif

    init_timer(&dcur->dev_timer);
    dcur->dev_timer.data     =(unsigned long) dcur;
    dcur->dev_timer.function = dic120dev_timer;
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
        snprintf(proc_name,sizeof(proc_name),"%s_%d",name_dev,number);
        dcur->proc_entry = proc_create_data(proc_name,S_IFREG | S_IRUGO,dir_entry,&proc_fops,dcur);
        if(!dcur->proc_entry)
        {
            DBG_TRACE(KERN_DEBUG,"Error create proc entry for device %d\n",number);
            ret = -ENOMEM;
        }
        else
        {
            if(dcur->base_port && !dic120dev_resource_init(dcur))
            {
                dqueue_init(&dcur->queue,sizeof(dic120_input),dcur->queue.q_size);
                dqueue_init(&dcur->_queue_internal,sizeof(dic120_input),8);
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


static int __init dic120_read_config(char * conf_name,struct dic120_dev_params * dp,int dp_count)
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


static int __init dic120_reg_module(void)
{
    int result;
    dev_t dev;
    lpdic120dev  dcur,dend;
    int    ndev;
    size_t nsize;

    struct dic120_dev_params *dev_params = 0;
    struct dic120_dev_params *dp         = 0;

    nsize = DIC120_MAX_DEVICES*sizeof(struct dic120_dev_params);
    if(!(dev_config && 0==*dev_config))
        dev_config = "/etc/dic120.conf";

    dev_params = (struct dic120_dev_params*)kmalloc(nsize,GFP_KERNEL);
    dev_count  = dic120_read_config (dev_config,dev_params,DIC120_MAX_DEVICES);

    if(major)
    {
        dev    = MKDEV(major,minor);
        result = register_chrdev_region(dev,dev_count,name_dev);

    }
    else
    {
        result = alloc_chrdev_region(&dev,minor,dev_count,name_dev);
        major = MAJOR(dev);
    }
    if(result)
        TRACE(KERN_ALERT,"can't get major %d",major);
    else
    {
        DBG_TRACE(KERN_INFO, "register success major %d\n",major) ;
        dir_entry  = proc_mkdir(name_dev,NULL);
        if(dev_count)
        {
            nsize = sizeof(struct _dic120dev)*dev_count;
            mod_devs = kmalloc(nsize,GFP_KERNEL);
            DBG_TRACE(KERN_DEBUG,"alloc space for mod_devs size %d at 0x%p\n",(int)nsize,(void*)mod_devs);
            bzero(mod_devs,nsize);

            dcur = mod_devs;
            dend = dcur+dev_count;
            dp = dev_params;

            while(dcur<dend)
            {

                dcur->base_port    = dp->params[CONF_PORT];
                dcur->base_port   +=DIC120_BASE_ADDR_OFFSET;
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
                result = dic120_reg_device(dcur++,ndev++);
            }
        }
        else
        {
            TRACE(KERN_ALERT,"Devices is not defined. Check the configuration file /etc/dic120.conf\n");
            result = -1;
        }
    }
    if(dev_params) kfree(dev_params);
    return result;
}


static int __init dic120_init(void)
{
    int result = 0;

    TRACE(KERN_INFO,"Initialize module\n");
    result = dic120_reg_module();
    TRACE(KERN_DEBUG, "dic120_init return code %d\n",result);
    return result;
}

static void __exit dic120_release_device(lpdic120dev  dcur)
{
    char proc_name[64];
    DBG_TRACE(KERN_DEBUG"\t","stop device \n");
    dic120dev_start(dcur,0,0);
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
        snprintf(proc_name,sizeof(proc_name),"%s_%d",name_dev,dcur->dev_number);
        remove_proc_entry(proc_name,dir_entry);

    }

    dic120dev_resource_release(dcur);
    DBG_TRACE(KERN_DEBUG,"Delete cdev major %d minor %d\n",MAJOR(dcur->_cdev.dev),MINOR(dcur->_cdev.dev));
    cdev_del(&dcur->_cdev);

}

static void __exit dic120_exit(void)
{
    dev_t dev;
    lpdic120dev  dcur,dend;
    int dev_no = 0;

    dev = MKDEV(major,minor);
    DBG_TRACE(KERN_DEBUG,"Unregister chrdev region %d.%d count %d \n",major,minor,dev_count);
    unregister_chrdev_region(dev,dev_count);

    dcur = mod_devs;
    dend = dcur +dev_count;
    while(dcur<dend)
    {
        TRACE(KERN_DEBUG ,"EXIT : Release device %d\n",dev_no++);
        dic120_release_device(dcur);
        ++dcur;
    }
    //if(dir_entry) proc_remove(dir_entry);
    if(dir_entry) remove_proc_entry(name_dev,NULL);
    if(mod_devs ) kfree(mod_devs);


    TRACE(KERN_INFO, "module exit\n");
}

module_init(dic120_init);
module_exit(dic120_exit);

MODULE_LICENSE(LICENSE_TYPE);
MODULE_AUTHOR (AUTHOR);
MODULE_DESCRIPTION(DESCR);

