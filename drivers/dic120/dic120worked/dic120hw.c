#include "dic120.h"
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/timex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <dic120ioctl.h>
#include <linux/time.h>
#include <linux/random.h>


int __dic120dev_read_p55_data (lpdic120dev dev, int number, int force);

static __always_inline __inline void
dic120dev_start_timer (lpdic120dev dev)
{

    if (!dev->tmr_jiff_add)
        dev->tmr_jiff_add = HZ;
    dev->dev_timer.expires = jiffies + dev->tmr_jiff_add;
    add_timer (&dev->dev_timer);
}

void
dic120dev_timer (unsigned long arg)
{

    lpdic120dev dev = (lpdic120dev) arg;
    ++dev->dev_timer_count;

    if (dev && sizeof (*dev) == dev->isize)
    {
        if (0 != atomic_read (&dev->active))
        {
            int changes = 0;
            if (spin_trylock (&dev->irq_locker))
            {
                int i;
                u16 pga_code;
                for (i = 0; i < dev->pga_count; i++)
                {
                    pga_code = inw_p (dev->pga_base_port[i] + DIC120_ID);
                    if (pga_code != dev->current_pga_id[i])
                    {

                        char *new_id = (char *) &pga_code;
                        char *old_id = (char *) &dev->current_pga_id[i];
                        TRACE (KERN_ALERT,
                               "Device number %d changed PGA ID old : %c-%d new : %c-%d\n",
                               dev->dev_number, old_id[0], (int) old_id[1],
                               new_id[0], (int) new_id[1]);
                        dev->current_pga_id[i] = pga_code;
                        changes += __dic120dev_read_p55_data (dev, i, 1);
                    }
                    else
                    {
                        if (dev->irq_num < DIC120_FIRST_IRQ)
                        {
                            changes += __dic120dev_read_p55_data (dev, i, 0);
                        }
                    }
                }
                spin_unlock (&dev->irq_locker);
            }

            dic120dev_start_timer (dev);
            if (changes)
                tasklet_schedule (&dev->irq_tasklet1);
#ifndef _REAL_DEVICE
            if (dev->irq_num >= DIC120_FIRST_IRQ)
                tasklet_schedule (&dev->irq_emul_tasklet);
#endif

        }
        else
            DBG_TRACE (KERN_DEBUG, "device stopped not rescheduling timer\n");
    }
    else
        TRACE (KERN_ALERT, "dev_timer invalid device pointer");
}

void __always_inline
write_data (int port, u8 * src, int src_len)
{
    do
    {
        if (src_len > sizeof (u8))
        {

            outw (*(u16 *) src, port);
            ++port;
            ++port;
            ++src;
            ++src;
            --src_len;
            --src_len;
        }
        else
        {
            outb (*src, port);
            --src_len;
        }
    }
    while (src_len > 0);

}

int __always_inline
read_data (int port, u8 * dst, int dst_len)
{
    int ret = 0;
    do
    {
        if (dst_len > sizeof (u8))
        {
#ifdef _REAL_DEVICE
            *(u16 *) dst = inw (port);
#else
            prandom_bytes (dst, sizeof (u16));
#endif
            if (*(u16 *) dst)
                ++ret;
            ++port;
            ++port;
            ++dst;
            ++dst;
            --dst_len;
            --dst_len;
        }
        else
        {
#ifdef _REAL_DEVICE
            *dst = inb (port);
#else
            prandom_bytes (dst, 1);
#endif
            if (*dst)
                ++ret;
            --dst_len;
        }
    }
    while (dst_len > 0);
    return ret;
}

int
__dic120dev_read_p55_data (lpdic120dev dev, int number, int force)
{

    int changes = 0;
    lpdic120_input dinp;
    u8 *changes_mask;
    u8 *data;
    int port;
    lpp55_pga_param pp = dev->pga_params + number;
    number = pp->number;
    port = dev->pga_base_port[number];

    if (dqueue_is_full (&dev->_queue_internal))
        ++dev->queue_lost;

    dinp =
        (lpdic120_input) __dqueue_alloc_input (&dev->_queue_internal,
                sizeof (*dinp));
    data = dinp->data;
    changes_mask = dinp->changes_mask;
    changes = read_data (port + P55_EVENT_A, changes_mask, P55_DATA_PORT_COUNT);
    if (changes || force)
    {
        read_data (port + P55_PORT_A, data, P55_DATA_PORT_COUNT);
        if (changes)
            write_data (port + P55_EVENT_A, changes_mask, P55_DATA_PORT_COUNT);
    }

    if (changes || force)
    {
        ++dev->scan_count[number];
        dinp->number = number;
        do_gettimeofday (&dinp->tmstamp);
        dinp->seq_number = ++dev->seq_number;
        dinp->data_len = 6;
        dinp->test_signals[0] = dev->current_pga_id[number];
        dinp->test_signals[1] = port;

        __dqueue_commit (&dev->_queue_internal);
    }
    return changes;
}

void
dic120dev_hwreset (lpdic120dev dev)
{
    //Reset dic120 device
    if (dic120dev_is_device (dev))
    {
        int i;
        int j;
        lpp55_pga_param pp = dev->pga_params;
        dev->reg_intr = (dev->irq_num & DIC120_IRQ_NUM_MASK);
        //reset events and disable interrputs on all PGA's
        for (i = 0; i < DIC120_PGA_COUNT; i++)
        {
            int port = dev->pga_base_port[i];
            outw_p (0xFFFF, port + P55_EVENT_A);
            wmb ();
            outb_p (0xFF, port + P55_EVENT_C);
            wmb ();
            outb_p (0, port + P55_INT_EVENT);
        }

        for (i = 0; i < dev->pga_count; i++)
        {
            u8 front_bounce = pp->debounce & 3;
            u8 event_intr = 0;
            int port = dev->pga_base_port[pp->number];

            __dic120dev_read_p55_data (dev, i, 1);
//          if(pp->number)
//            {
//             u8 pga_int_mask = 0x10<<(pp->number-1);
//             dev->reg_intr |=pga_int_mask;
//             pga_int_mask<<=1;
//            }

            outb_p (0x1B, port + P55_CONTROL);	//All ports - input
            for (j = 0; j < P55_DATA_PORT_COUNT; j++)
            {
                front_bounce |= (pp->fronts[j] << (2 * (j + 1)));
                event_intr |= (1 << j);
            }


            DBG_TRACE (KERN_DEBUG, "Out port %X value %X\n",
                       (unsigned) port + P55_TIME_BOUNCE, front_bounce);
            outb_p (front_bounce, port + P55_TIME_BOUNCE);	//Both front and  4ms debounce
            DBG_TRACE (KERN_DEBUG, "Out port %X value %X\n",
                       (unsigned) port + P55_INT_EVENT, event_intr);
            outb_p (event_intr, port + P55_INT_EVENT);	//Enable interrupts by events
            ++pp;
        }
    }

}

int
dic120dev_start (volatile lpdic120dev dev, int start, int scan_period)
{
    int ret = 0;


    if (start)
    {
        if (dev->pga_found)
        {
            if (0 == atomic_read (&dev->active))
            {
                // device id is correct


                dev->dev_timer_count = 0;
                dev->irq_count = 0;
                dev->irq_handled = 0;

                memset (dev->scan_count, 0, sizeof (dev->scan_count));
                dev->seq_number = 0;
                dev->queue_lost = 0;
                dev->queue_overflow = 0;
                dev->queue_reads = 0;
                dev->queue_entry_reads = 0;
                dqueue_reset (&dev->queue);
                dqueue_reset (&dev->_queue_internal);
                dev->tmr_jiff_add = 1;
                if (scan_period > 0)
                    dev->tmr_jiff_add = MAX (1, (scan_period * HZ) / 1000);

                DBG_TRACE (KERN_DEBUG, "Start device\n");
                dic120dev_hwreset (dev);
                tasklet_schedule (&dev->irq_tasklet1);
                atomic_set (&dev->active, 1);

                if (dev->irq_num >= DIC120_FIRST_IRQ)
                {
                    TRACE (KERN_DEBUG, "Out port %X value %X\n",
                           (unsigned) dev->base_port + DIC120_INT_REG,
                           dev->reg_intr);
                    outb_p (dev->reg_intr, dev->base_port + DIC120_INT_REG);
                }
                dic120dev_start_timer (dev);
            }
            else
                ret = -EPERM;
        }
        else
            ret = -ENODEV;
    }
    else
    {
        if (0 != atomic_read (&dev->active))
        {
            //int i;
            //Disable interrupts by event mask
            outb_p (0, dev->base_port + DIC120_INT_REG);
            atomic_set (&dev->active, 0);
            DBG_TRACE (KERN_DEBUG, "!*stop device at port 0x%X*!\n",
                       dev->base_port);
            DBG_TRACE (KERN_DEBUG, "delete timer\n");
            del_timer_sync (&dev->dev_timer);
        }
        else
            ret = -ENOEXEC;
    }
    return ret;
}



//DIC120 Device interrupt real handler
irqreturn_t
__dic120dev_irq_handler (int irq_num, void *arg)
{

    irqreturn_t ret = IRQ_NONE;
    lpdic120dev dev = (lpdic120dev) arg;
    ++dev->irq_count;
    if (dic120dev_is_device (dev) && dev->irq_num == irq_num)
    {
        int i;
        int changes = 0;
#ifdef _REAL_DEVICE
        spin_lock_irq (&dev->irq_locker);
#else
        spin_lock (&dev->irq_locker);
#endif
        for (i = 0; i < dev->pga_count; i++)
        {
            changes += __dic120dev_read_p55_data (dev, i, 0);
        }
#ifdef _REAL_DEVICE
        spin_unlock_irq (&dev->irq_locker);
#else
        spin_unlock (&dev->irq_locker);
#endif
        if (changes)
        {
            if (0 != atomic_read (&dev->active))
            {
                tasklet_schedule (&dev->irq_tasklet1);
                ++dev->irq_handled;
            }
            ret = IRQ_HANDLED;
        }
    }
    return ret;
}

#ifndef _REAL_DEVICE

void
dic120dev_irq_emul_tasklet (unsigned long arg)
{
    //Simulate call irqhandler
    lpdic120dev dev = (lpdic120dev) arg;
    if (dev && dev->isize == sizeof (*dev))
    {
        __dic120dev_irq_handler (dev->irq_num, dev);
    }
}


#endif


void
dic120dev_irq_tasklet1 (unsigned long arg)
{
    //Second half of IRQ handler
    lpdic120dev dev = (lpdic120dev) arg;
    if (dev && dev->isize == sizeof (*dev))
    {
        dic120dev_data_to_queue (dev);
    }
}



int
dic120dev_resource_release (volatile lpdic120dev dev)
{

    if (dev && sizeof (*dev) == dev->isize)
    {

        if (dev->resource_requested)
        {
            int i = 0;
            atomic_set (&dev->active, 0);
            udelay (100);
            if (0 != atomic_read (&dev->irq_handler_installed))
            {
                DBG_TRACE (KERN_DEBUG, "release irq %d\n", dev->irq_num);
                synchronize_irq (dev->irq_num);
                free_irq (dev->irq_num, dev);
                atomic_set (&dev->irq_handler_installed, 0);
            }

            DBG_TRACE (KERN_DEBUG, "release IO resource 0x%X\n",
                       dev->base_port);
            for (i = 0; i < DIC120_PGA_COUNT; i++)
            {
                if (dev->resource[i])
                {
                    release_region (dev->pga_base_port[i],
                                    DIC120_DEV_PORT_COUNT);
                    dev->resource[i] = NULL;
                }
            }
            dev->resource_requested = 0;

        }
        return 0;
    }
    return -ENODEV;
}

int
dic120dev_hwread_id (lpdic120dev dev)
{
    // read id-string
    // Reset dic120 device

    int ret = 0;
    int i;
    u16 id_code;
    for (i = 0; i < DIC120_PGA_COUNT; i++)
    {
        id_code = inw_p (dev->pga_base_port[i] + DIC120_ID);
#ifndef _REAL_DEVICE
        id_code = P55ID;
#endif
        dev->pga_id[i] = id_code;
        dev->current_pga_id[i] = id_code;

        if (id_code && id_code != 0xFFFF)
        {
            ++dev->pga_found;
        }
    }
    return ret;
}




int
dic120dev_resource_init (volatile lpdic120dev dev)
{
    int ret = 0;
    if (!dev->resource_requested)
    {
        int i;
        for (i = 0; i < DIC120_PGA_COUNT; i++)
        {
            dev->pga_base_port[i] +=
                dev->base_port + (i * DIC120_BASE_ADDR_PGA_OFFSET);
            dev->resource[i] =
                request_region (dev->pga_base_port[i], DIC120_DEV_PORT_COUNT,
                                dev_name);
            if (dev->resource[i])
                ++dev->resource_requested;
        }
    }

    if (dev->resource_requested)
    {
        ret = dic120dev_hwread_id (dev);
        if (!ret)
        {
            if (0 == atomic_read (&dev->irq_handler_installed))
            {
                if (dev->irq_num >= DIC120_FIRST_IRQ)
                {
                    //if dev->irq_num == 0 than pure scan mode;
                    ret =
                        request_irq (dev->irq_num, __dic120dev_irq_handler,
                                     SA_SHIRQ, dev_name, dev);
                    atomic_set (&dev->irq_handler_installed, ret == 0 ? 1 : 0);
                    if (ret)
                        TRACE (KERN_ALERT, "Error request_irq %d ret code %d\n",
                               dev->irq_num, ret);
                }
            }
        }
        if (ret)
            dic120dev_resource_release (dev);
        DBG_TRACE (KERN_DEBUG, "dic120dev_hwinit result %d\n", ret);
    }
    else
    {
        TRACE (KERN_ALERT, "error request region base port = %X\n",
               dev->base_port);
        ret = -EFAULT;
    }
    return ret;
}






void
dic120dev_data_to_queue (volatile lpdic120dev dev)
{
    //Put ready data to queue
    //Ready data placed in other half work buffer

    s32 len;
    lpdic120_input ptr;
    do
    {
        ptr = (lpdic120_input) dqueue_get_ptr (&dev->_queue_internal, &len);
        if (ptr)
        {

            if (dqueue_is_full (&dev->queue))
            {
                ++dev->queue_lost;
                ++dev->queue_overflow;
            }
            else
            {
                dqueue_add (&dev->queue, (u8 *) ptr, len);
                ++dev->queue_total;
            }
            dqueue_remove_first (&dev->_queue_internal);
            dev->_wait_flag = 1;
            wake_up_interruptible (&dev->_wait_queue);
        }
    }
    while (ptr);


}

int
dic120dev_add_pga (volatile lpdic120dev dev, lpp55_pga_param pp)
{
    int ret = 0;
    if (dic120dev_is_device (dev))
    {
        if (0 == atomic_read (&dev->active))
        {
            if (dev->pga_count < DIC120_PGA_COUNT)
            {
                memcpy (dev->pga_params + dev->pga_count, pp, sizeof (*pp));
                ++dev->pga_count;
            }
            else
                ret = -ENOSPC;
        }
        else
            ret = -EBUSY;
    }
    else
        ret = -ENODEV;
    return ret;
}

int
dic120dev_clear_pga (volatile lpdic120dev dev)
{
    int ret = 0;
    if (dic120dev_is_device (dev))
    {
        if (0 == atomic_read (&dev->active))
            dev->pga_count = 0;
        else
            ret = -EBUSY;
    }
    else
        ret = -ENODEV;
    return ret;
}


#ifndef _REAL_DEVICE
int
dic120dev_test (lpdic120dev dev, int mode)
{
    if (dic120dev_is_device (dev))
    {

        return 0;
    }
    return -ENODEV;
}
#endif
