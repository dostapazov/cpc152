#include "aic124.h"
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/timex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <aic124ioctl.h>
#include <linux/time.h>

#define DELAY_ON_CHANNEL ndelay(430)



void aic124dev_dac_prepare(lpaic124dev dev)
{
    //prepare data for DAC output;
    u16     dac_ch = 0;
    lpaic124_dac_output_t odac,eodac;
    if(spin_trylock(&dev->irq_locker))
    {
        odac  = dev->dac_output;
        eodac = odac + 2;
        while(odac<eodac)
        {
            if(odac->data_pos<odac->data_count  )
            {
                odac->value = odac->data[odac->data_pos++]&AIC124_DAC_VALUE_MASK;
                odac->value |=dac_ch;
                if(odac->data_count>1 && odac->data_pos>=odac->data_count)
                    odac->data_pos = 0;
            }
            ++odac;
            dac_ch = AIC124_DAC_CHANEEL1;
        }
        spin_unlock(&dev->irq_locker);
    }
}

u16 __aimux2channel(u8 mux_num)
{
    /*Convert aimux number to aic124 channel*/
    static u16 ch_nums[AIC124_SINGLE_CHANNELS_COUNT] = {0,1,2,3,8,9,10,11,4,5,6,7,12,13,14,15};
    mux_num&=AIC124_MUX_MASK;
    if(mux_num<AIC124_MUX_COUNT)  return ch_nums[mux_num];
    return 0;
}

#define __channel2aimux(ch_num) __aimux2channel(ch_num)


void __aic124dev_start_scan(lpaic124dev dev,int no_real_start)
{
    //  Start scan
    if(dqueue_is_full(&dev->_queue_internal))
        ++dev->_internal_lost;
    dev->curr_data_buff =(lpaic124_input) __dqueue_alloc_input(&dev->_queue_internal,sizeof(aic124_input));

    if(dev->channels_count)
    {
        dev->channel_idx = 0;
        if(dev->aib_use)
        {
            dev->aib_channel_idx = 0;
            dev->aib_current     = dev->aib_channels;
            dev->aib_channel_idx = 0;
            outb_p(dev->aib_current->channels[0],dev->base_port+AIC124_WR_REGB_DISCRETE_OUT);
        }
        outb_p(dev->channel_numbers[0],dev->base_port+AIC124_W_REGB_ADC_CHANNEL);

        if(dev->scan_over_time)  ++dev->scan_over_time_count ;
        dev->scan_over_time = 0;
        atomic_set(&dev->scan_state,__AIC124_DEV_SCAN_CNANNELS);
        dev->regb0wr |=  AIC124_W_CTRL0_ADC_START;
        if(0==no_real_start)
        {       
          do_gettimeofday(&dev->scan_start_time);
          tasklet_schedule(&dev->irq_tasklet1);

        }
    }
}



static __always_inline __inline void aic124dev_start_timer(lpaic124dev  dev)
{

    dev->dev_timer.expires = jiffies+dev->tmr_jiff_add;
    add_timer(&dev->dev_timer);

}

void aic124dev_timer  (unsigned long arg)
{
    lpaic124dev  dev = (lpaic124dev) arg;
    if(dev && sizeof(*dev) == dev->isize )
    {
        if(0!=atomic_read(&dev->active))
        {
            ++dev->dev_timer_count;
            aic124dev_dac_prepare(dev)  ;// Prepare data do output on the DAC
            if(__AIC124_DEV_SCAN_NONE == atomic_read(&dev->scan_state)) //if scanning process is inactive - start him
            {
                __aic124dev_start_scan(dev,0);
                DBG_TRACE(KERN_DEBUG,"START SCAN PROCESS\n");
            }
            else
            {
                ++dev->scan_over_time;
                DBG_TRACE(KERN_ALERT,"Device do not end scanning when the timer is expired %d. port 0x%03X\n",dev->scan_over_time,dev->base_port);
            }
            aic124dev_start_timer(dev);
        }
        else
        {
            DBG_TRACE(KERN_DEBUG,"device stopped not rescheduling timer\n");
            return;
        }
    }
    else
    {
        TRACE(KERN_ALERT,"dev_timer invalid device pointer" );
        return;
    }
}


void  aic124dev_hwreset (lpaic124dev dev)
{
    //Reset aic124 device
    lpaic124dev bdev,edev;
    int    i;
    bdev = get_first_device();
    edev = get_last_device ();


    //call soft reset
    DBG_TRACE(KERN_DEBUG,"Soft reset port %X\n",dev->base_port);
    outb_p(0                          ,dev->base_port + AIC124_W_REGB_EX_CTRL_ENABLE);
    outb_p(AIC124_W_EXTREGB_SOFT_RESET,dev->base_port + AIC124_W_REGB_EX_CTRL);

    //Setup SHARED IRQ for device
    dev->regb1wr&=~AIC124_W_CTRL1_IRQ_SHARE;
    while(bdev<edev)
    {
        if(bdev!=dev && bdev->irq_num == dev->irq_num)
        {
            dev->regb1wr|=AIC124_W_CTRL1_IRQ_SHARE;
            break;
        }
        ++bdev;
    }

    mdelay(100);

    dev->regb0wr &= AIC124_W_CTRL0_FREQ_100KHZ;
    outb_p(dev->regb0wr,dev->base_port);
    wmb();
    outb_p(dev->regb1wr,dev->base_port+AIC124_RW_REGB_CONTROL1);
    DBG_TRACE(KERN_DEBUG,"Write control register reg0 0x%02X,reg1 0x%02X. port %X\n",(u32)dev->regb0wr,(u32)dev->regb1wr,dev->base_port);

    //setup channels gain
    {
        int port = dev->base_port+AIC124_W_REGB_GAIN0;
        for(i = 0; i<4; i++)
        {
            DBG_TRACE(KERN_DEBUG,"Set gain-%i=%02X\n",i,(unsigned int)dev->gains[i]);
            outb_p(dev->gains[i],port);
            ++port;
            udelay(100);
        }
    }
    DBG_TRACE(KERN_DEBUG,"Write gain values 0x%02x-0x%02x-0x%02x-0x%02x. port %X\n",(u32)dev->gains[0],(u32)dev->gains[1],(u32)dev->gains[2],(u32)dev->gains[3],dev->base_port);
}

int aic124dev_start(volatile lpaic124dev dev, int start,int scan_period)
{
    int ret = 0;

    if(start)
    {
        if(dev->id[0] == 'A')
        {
            if(0==atomic_read(&dev->active))
            {
                // device id is correct

                aic124dev_hwreset(dev);
                dev->dev_timer_count     = 0;
                dev->irq_count           = dev->irq_handled = 0;

                atomic_set(&dev->scan_state,0);
                dev->seq_number     = 0;
                dev->queue_lost     = 0;
                dev->scan_count     = 0;
                dev->scan_over_time = 0;
                dev->scan_over_time_count = 0;
                dev->_internal_lost = 0;
                dev->queue_overflow = 0;
                dev->queue_reads    = 0;
                dev->queue_entry_reads = 0;
                dqueue_reset(&dev->queue);
                dqueue_reset(&dev->_queue_internal);

                //prepare dac output into interrupt handler;
                dev->dac_output[0].value    = dev->dac_output[1].value    = -1;
                dev->dac_output[0].data_pos = dev->dac_output[1].data_pos =  0;
                DBG_TRACE(KERN_DEBUG,"Start deviced : scan frequency = %d\n",scan_period);

                //Setup  timer schedule parameters
                dev->tmr_jiff_add = 1;
                if(scan_period>0)
                    dev->tmr_jiff_add = MAX(1,(scan_period*HZ)/1000);
                //enables interrupt and start ADC
#ifdef _REAL_DEVICE
                dev->regb0wr |= AIC124_W_CTRL0_ENABLE_IRQ;
#endif
                if(dev->channels_count)
                {
                    atomic_set(&dev->active,1);
                    aic124dev_start_timer(dev);
                    DBG_TRACE(KERN_DEBUG,"*!start device at port 0x%X!*\n",dev->base_port);
                }
                else
                {
                    TRACE(KERN_INFO,"Start error: no channels defined. Nothing to do. port %X )\n",dev->base_port);
                    ret = -EBADSLT;
                }
            }
            else
                ret = - EPERM;
        }
        else
            ret = -ENODEV;
    }
    else
    {
        if(0!=atomic_read(&dev->active))
        {
            atomic_set(&dev->active,0);
            udelay(100);
            //Disbale irq;
            dev->regb0wr&=~(AIC124_W_CTRL0_ENABLE_IRQ|AIC124_W_CTRL0_ADC_START);
            outb_p(dev->regb0wr,dev->base_port+AIC124_RW_REGB_CONTROL0);
            DBG_TRACE(KERN_DEBUG,"!*stop device at port 0x%X*!\n",dev->base_port);
            DBG_TRACE(KERN_DEBUG,"delete timer\n");
            del_timer_sync(&dev->dev_timer);
            //try_to_del_timer_sync(&dev->dev_timer);
        }
        else ret = -ENOEXEC;
    }
    return ret;
}



void __aic124dev_dac_output(lpaic124dev dev)
{
    lpaic124_dac_output_t odac,eodac;
    //Output to DAC
    if(dev->regb0rd & AIC124_R_CTRL0_DAC_RDY)
    {
        odac   = dev->dac_output;
        eodac  = odac+2;
        while(odac<eodac)
        {
            if(!(odac->value&AIC124_DAC_VALUE_OUTPUT_DISABLED))
            {
                DBG_TRACE(KERN_DEBUG,"DAC output %X\n",odac->value);
             #ifdef _REAL_DEVICE
                outw_p(odac->value,dev->base_port+AIC124_W_REGW_DAC);
             #endif
                odac->value |= AIC124_DAC_VALUE_OUTPUT_DISABLED;
                return;
            }
            ++odac;
        }
    }
}


void __always_inline __aic124dev_next_input(lpaic124dev dev)
{
    dev->curr_data_buff->seq_number    = ++dev->seq_number;
    dev->curr_data_buff->tmstamp_start = dev->scan_start_time;
    do_gettimeofday(&dev->scan_start_time);
    dev->curr_data_buff->tmstamp_end = dev->scan_start_time;
    __dqueue_commit(&dev->_queue_internal);
    if(dqueue_is_full(&dev->_queue_internal))
        ++dev->_internal_lost;
    dev->curr_data_buff = (lpaic124_input)__dqueue_alloc_input(&dev->_queue_internal,sizeof(aic124_input));
}

int __always_inline  __aic124dev_select_next_channel(lpaic124dev dev)
{
    int ret = __AIC124_DEV_SCAN_CNANNELS;
    if(++dev->channel_idx < dev->channels_count)
    {
        outb_p(dev->channel_numbers[dev->channel_idx],dev->base_port+AIC124_W_REGB_ADC_CHANNEL);
        //outb  (dev->channel_numbers[dev->channel_idx],dev->base_port+AIC124_W_REGB_ADC_CHANNEL);
        //wmb();
    }
    else
    {
        dev->curr_data_buff->number = 0;
        dev->curr_data_buff->data_len = dev->channels_count;
        __aic124dev_next_input (dev);
        ret = __AIC124_DEV_SCAN_NONE;
    }
    return ret;
}

int __always_inline __aic124dev_select_next_mux_channel(lpaic124dev dev)
{
    int ret = __AIC124_DEV_SCAN_NONE;

    if(++dev->aib_channel_idx < dev->aib_current->count)
    {
        u8 mch = dev->aib_current->channels[dev->aib_channel_idx];
        outb_p(mch,dev->base_port+AIC124_WR_REGB_DISCRETE_OUT);
        //outb  (mch,dev->base_port+AIC124_WR_REGB_DISCRETE_OUT);
        //wmb();
        //DBG_TRACE(KERN_DEBUG,"cur_mux_channel = %u : mux_chan_limit = %d\n",(unsigned)mch&AIC124_MUX_MASK,dev->aib_current->count);
        ret = __AIC124_DEV_SCAN_CNANNELS;
    }
    else
    {
        u8 ch = dev->channel_numbers[dev->channel_idx];
        dev->curr_data_buff->number  =  __aimux2channel( ch );
        dev->curr_data_buff->data_len = dev->aib_current->count;

        if(++dev->channel_idx < dev->channels_count)
        {
            ch = dev->channel_numbers[dev->channel_idx];
            outb_p(ch,dev->base_port+AIC124_W_REGB_ADC_CHANNEL);
            //outb  (ch,dev->base_port+AIC124_W_REGB_ADC_CHANNEL);
            //wmb();
            ret = __AIC124_DEV_SCAN_CNANNELS;
            dev->aib_channel_idx = 0;
            ++dev->aib_current;
            outb_p(*dev->aib_current->channels,dev->base_port+AIC124_WR_REGB_DISCRETE_OUT);
            //outb  (*dev->aib_current->channels,dev->base_port+AIC124_WR_REGB_DISCRETE_OUT);
            //wmb();

        }
        else
            ret = __AIC124_DEV_SCAN_NONE;
        __aic124dev_next_input (dev);
    }
    return ret;
}


int __always_inline __aic124dev_prepare_next_scan(lpaic124dev dev)
{
    /*Prepare hardware to next scan*/
    if(aic124dev_is_device(dev))
    {
        int ret  = atomic_read(&dev->scan_state);
        DBG_TRACE(KERN_DEBUG,"Prepare next scan begin state %d channel %u:%02X \n",ret,(unsigned)dev->channel_idx,(unsigned)dev->channel_numbers[dev->channel_idx]);
        if(__AIC124_DEV_SCAN_CNANNELS == ret)
        {

            if(dev->aib_use)
                ret = __aic124dev_select_next_mux_channel(dev);
            else
                ret = __aic124dev_select_next_channel   (dev);

            if(!ret)
                ret =   __AIC124_DEV_SCAN_TEST_SIGNAL_1;
        }

        switch(ret )
        {
            //nothing to scan; scan test_signals;
        case __AIC124_DEV_SCAN_TEST_SIGNAL_1:
            //DBG_TRACE(KERN_DEBUG,"Select scan AGND\n");
            outb_p(AIC124_CHANNEL_2p5V,dev->base_port+AIC124_W_REGB_ADC_CHANNEL);
            break;
        case __AIC124_DEV_SCAN_TEST_SIGNAL_2:
            //DBG_TRACE(KERN_DEBUG,"Select scan 2.5v\n");
            outb_p(AIC124_CHANNEL_AGND,dev->base_port+AIC124_W_REGB_ADC_CHANNEL);
            break;
        }
        atomic_set(&dev->scan_state,ret);
        
        DBG_TRACE(KERN_DEBUG,"Prepare next scan end   state %d  ch=%X aib_ch=%X\n"
                  ,ret
                  ,(unsigned)dev->channel_numbers[dev->channel_idx]
                  ,(unsigned)(dev->aib_use  ? (unsigned)dev->aib_current->channels[dev->aib_channel_idx] : (unsigned)-1)
                 );
        return ret;
    }
    else
        return __AIC124_DEV_SCAN_NONE;
}


//AIC124 Device interrupt real handler
irqreturn_t __aic124dev_irq_handler(int irq_num, void * arg)
{

    irqreturn_t ret  = IRQ_NONE;
    lpaic124dev dev = (lpaic124dev) arg;
    if(aic124dev_is_device(dev) && dev->irq_num == irq_num)
    {
        rmb();
        dev->regb0rd = inb(dev->base_port+AIC124_RW_REGB_CONTROL0);
        if(dev->regb0rd&AIC124_R_CTRL0_ADC_RDY)
        {
            u16  value;
            u16 data_idx;
            int scan_state;
            data_idx = dev->aib_use ? dev->aib_channel_idx : dev->channel_idx;
            scan_state = atomic_read(&dev->scan_state);
            __aic124dev_prepare_next_scan(dev);
            ret  = IRQ_HANDLED;
#ifdef _REAL_DEVICE
            value =  inw(dev->base_port+AIC124_R_REGW_ADC_DATA);
            rmb();
#else
//              value = dev->channel_numbers[dev->channel_idx];
             {
                  int idx = (u16)dev->dev_timer_count%(u16)dev->dac_output[0].data_count;
                  value = dev->dac_output[0].data[idx];
             }

#endif
            switch(scan_state)
            {
            case __AIC124_DEV_SCAN_TEST_SIGNAL_1:
#ifndef _REAL_DEVICE
                value = 0x7D0;
#endif
                atomic_set(&dev->test_signals[0],(int)value);
                atomic_dec(&dev->scan_state);
                break;
            case __AIC124_DEV_SCAN_TEST_SIGNAL_2:
#ifndef _REAL_DEVICE
                value = 0x005;
#endif
                atomic_set(&dev->test_signals[1],(int)value);
                atomic_set(&dev->scan_state, __AIC124_DEV_SCAN_NONE);
                break;
            case __AIC124_DEV_SCAN_CNANNELS:

                dev->curr_data_buff->data[data_idx] = value;
                break;
            default:
                atomic_set(&dev->scan_state, __AIC124_DEV_SCAN_NONE);//End of scan;
                break;
            }
            ++dev->irq_handled;
            if( __AIC124_DEV_SCAN_NONE < atomic_read(&dev->scan_state))
                aic124dev_irq_tasklet1((unsigned long)dev);
                else
               tasklet_schedule(&dev->irq_tasklet1);

             if(dev->regb0rd&AIC124_R_CTRL0_DAC_RDY)
            {
                spin_lock_irq(&dev->irq_locker);
                __aic124dev_dac_output(dev);
                spin_unlock_irq(&dev->irq_locker);
            }

        }
        ++dev->irq_count;
    }
    return ret;
}



#ifndef _REAL_DEVICE

void aic124dev_irq_emul_tasklet (unsigned long arg)
{
    //Simulate call irqhandler
    lpaic124dev dev = (lpaic124dev) arg;
    if(aic124dev_is_device(dev))
    {
        __aic124dev_irq_handler(dev->irq_num,dev);
    }
}


#endif


void aic124dev_irq_tasklet1       (unsigned long arg)
{
    //Second half of IRQ handler
    lpaic124dev dev = (lpaic124dev) arg;
    if(aic124dev_is_device(dev))
    {
        if(atomic_read(&dev->scan_state)>__AIC124_DEV_SCAN_NONE)
         {
          // if device in scan_channels state
          // initiate begin ADC work
          DELAY_ON_CHANNEL;
          #ifdef _REAL_DEVICE
            outb_p(dev->regb0wr,dev->base_port);
          #else
           tasklet_schedule(&dev->irq_emul_tasklet);
          #endif
         }
        else
        {
         //done scan put data into queue
         ++dev->scan_count;
         aic124dev_data_to_queue(dev);
        }
    }
}



int  aic124dev_resource_release (volatile lpaic124dev dev)
{

    if(dev && sizeof(*dev) == dev->isize)
    {

        if(dev->resource)
        {

            outb_p(0,dev->base_port+AIC124_RW_REGB_CONTROL0);
            atomic_set(&dev->active,0);
            if(0!=atomic_read(&dev->irq_handler_installed))
            {
                udelay(200);
                DBG_TRACE(KERN_DEBUG,"release irq %d\n",dev->irq_num);
                synchronize_irq(dev->irq_num);
                free_irq   (dev->irq_num,dev);
                atomic_set(&dev->irq_handler_installed ,0);
            }

            DBG_TRACE(KERN_DEBUG,"release IO resource 0x%X\n",dev->base_port);
            release_region(dev->base_port,AIC124_PORT_COUNT);
            dev->resource = NULL;

        }

        return 0;
    }
    return -ENODEV;

}

int  aic124dev_hwread_id (lpaic124dev dev)
{
    // read id-string
    // Reset aic124 device
    // setup timers and chunnel gains

    int ret = 0;
    int i = 0;
    
#ifndef _REAL_DEVICE
    if(dev->id[0] != 'A')
    {
        TRACE(KERN_DEBUG,"Not found AIC124 device at port %X. Simulate ;-(\n",dev->base_port);
        dev->id[0] = 'A';
    }
#else
    dev->id[0] = inb_p(dev->base_port+AIC124_R_REGB_ID);
#endif
    if(dev->id[0] == 'A')
    {
        //read dac mode
        dev->id[1]  = inb_p(dev->base_port+AIC124_R_REGB_DAC_MODE);
        //read version and revision
        dev->ver[0] = inb_p(dev->base_port+AIC124_R_REGB_VERNUM);
        dev->ver[1] = inb_p(dev->base_port+AIC124_R_REGB_REVNUM);
        //Read serial number
        
        outb_p(0                         ,dev->base_port+AIC124_W_REGB_EX_CTRL_ENABLE);
        outb_p(AIC124_W_EXTREGB_RD_SERIAL,dev->base_port+AIC124_W_REGB_EX_CTRL);
        
        mdelay(100);
        for(i = 0; i<5; i++)
        {
            
            outb_p(0x10+i,dev->base_port+AIC124_W_REGB_EX_CTRL_ENABLE);
            
            dev->serial_num[i] = inb_p(dev->base_port+AIC124_W_REGB_EX_CTRL);
        }
    }
    else
    {
        TRACE(KERN_ALERT,"No device found at port %04X\n",dev->base_port);
        ret = -ENODEV;
    }
    return ret;
}




int  aic124dev_resource_init (volatile lpaic124dev dev)
{
    int ret = 0;
    if(!dev->resource)
        dev->resource =  request_region(dev->base_port, AIC124_PORT_COUNT,name_dev);
    if(dev->resource)
    {
        ret = aic124dev_hwread_id(dev);
        if(!ret)
        {
            if(0==atomic_read(&dev->irq_handler_installed))
            {
                ret = request_irq(dev->irq_num,__aic124dev_irq_handler,SA_SHIRQ,name_dev,dev);
                atomic_set(&dev->irq_handler_installed , ret == 0 ? 1 : 0);
                if(ret) TRACE(KERN_ALERT,"Error request_irq %d ret code %d\n",dev->irq_num,ret);
            }
        }
        if(ret)
            aic124dev_resource_release(dev);
        DBG_TRACE(KERN_DEBUG, "aic124dev_hwinit result %d\n",ret);
    }
    else
    {
        TRACE(KERN_ALERT ,"error request region base port = %X\n",dev->base_port);
        ret = -EFAULT;
    }
    return ret;
}


int  aic124dev_clear_channels    (volatile lpaic124dev dev)
{
    if(aic124dev_is_device(dev))
    {
        dev->aib_use     = 0;
        dev->aib_current = 0;
        dev->auto_scan   = 0;
        dev->channels_count = 0;
        dev->channel_idx   = 0;
        bzero(dev->channel_numbers,sizeof(dev->channel_numbers));
        bzero(dev->gains,sizeof(dev->gains));
        bzero(dev->aib_channels,sizeof(dev->aib_channels));
        dev->aib_channel_idx = 0;
        return 0;
    }

    return -ENODEV;
}

int aic124dev_add_aib      (volatile lpaic124dev dev, aib_param * ap)
{
    int ret = 0;
    if(aic124dev_is_device(dev))
    {

        aic124_channel_param cp;
        cp.auto_scan    = ap->auto_scan;
        cp.avg          = ap->avg;
        cp.channel_gain = ap->ch_gain;
        cp.channel_mode = 1;
        cp.channel_number = __aimux2channel(ap->aib_number);
        ret = aic124dev_add_channel(dev,&cp);
        if(!ret)
        {
            int i   ;
            int idx = dev->channels_count-1;
            lpaib_channels_t  ach = dev->aib_channels+idx;
            dev->aib_use = 1;
            ach->aib_number       = ap->aib_number;
            ach->count   = MIN(AIC124_MUX_SINGLE_CHANNELS_COUNT,ap->count);
            for(i = 0; i<ach->count; i++)
            {
                ach->channels[i]    =  ap->mux_numbers[i]&AIC124_MUX_MASK;
                if(ap->mux_modes[i])
                    ach->channels[i]|= AIC124_MUX_SINGLE;
                switch(ap->mux_gains[i])
                {
                case 2 :
                case 10   :
                    ach->channels[i] |= AIC124_AIB_GAIN2_10;
                    break;
                case 4 :
                case 100  :
                    ach->channels[i] |= AIC124_AIB_GAIN4_100;
                    break;
                case 8 :
                case 1000 :
                    ach->channels[i] |= AIC124_AIB_GAIN8_1000;
                    break;
                }
            }
        }

    }
    else
        ret = -ENODEV;
    return ret;
}

u8  aic124dev_get_channel_gain(lpaic124dev dev,u8 channel_number)
{
    int ret;
    int goffs = channel_number/(u8)4;
    int gpos  = channel_number%(u8)4;
    u8  gain  = dev->gains[goffs];

    if(gpos) gain>>=(gpos<<1);



    switch(gain&3)
    {
     case 0x03: ret = 8;break;
     case 0x02: ret = 4;break;
     case 0x01: ret = 2;break;
       default: ret = 1;break;
    }
   DBG_TRACE(KERN_DEBUG,"get_channel_gain  %u, goffs = %d,gpos = %d dev->gain %u gain %u result %d\n",(unsigned) channel_number,goffs,gpos,(unsigned)dev->gains[goffs],(unsigned)gain,ret);
   return ret;
}

void aic124dev_set_channel_gain(lpaic124dev dev,u8 channel_number,u8 _gain)
{
    int goffs = channel_number/(u8)4;
    int gpos  = channel_number%(u8)4;
    u8  gmask = 0x03;
    u8  gval  = 0x00;
    switch(_gain)
    {
    case 2 :
        gval = 0x01;
        break;
    case 4 :
        gval = 0x02;
        break;
    case 8 :
        gval = 0x03;
        break;

    }
    if(gpos)
    {
        gmask<<=(gpos<<1);
        gval <<=(gpos<<1);
    }
    dev->gains[goffs]&=~gmask;
    dev->gains[goffs]|= (gval&gmask);

    DBG_TRACE(KERN_DEBUG,"set_channel_gain  %u:%u goffs = %d,gpos = %d dev->gain %u ,gmask %u gval %u \n",
              (unsigned) channel_number,(unsigned)_gain,goffs,gpos,(unsigned)dev->gains[goffs],(unsigned)gmask,(unsigned)gval);

}

int aic124dev_add_channel(volatile lpaic124dev dev, aic124_channel_param * cp)
{
    if(aic124dev_is_device(dev))
    {
        if(dev->channels_count<AIC124_SINGLE_CHANNELS_COUNT)
        {
            u8  cval;
            cp->channel_number&=AIC124_CHANNEL_MASK;
            cval  = (cp->channel_number);
            if(cp->channel_mode) cval|= AIC124_CHANNEL_SINGLE;
            dev->channel_numbers[dev->channels_count++] = cval;
            dev->auto_scan = dev->auto_scan;
            dev->regb1wr  &= ~AIC124_W_CTRL1_AVG16;

            switch (cp->avg)
            {
            case 2:
                dev->regb1wr |= AIC124_W_CTRL1_AVG2;
                break;
            case 4:
                dev->regb1wr |= AIC124_W_CTRL1_AVG4;
                break;
            case 8:
                dev->regb1wr |= AIC124_W_CTRL1_AVG8;
                break;
            case 16:
                dev->regb1wr |= AIC124_W_CTRL1_AVG16;
                break;
            }

            aic124dev_set_channel_gain(dev,cp->channel_number,cp->channel_gain);

            return 0;
        }
        return -ENOSPC;
    }
    return -ENODEV;
//    if(!channels_mode)
//      {
//       dev->channels_mode = 0;
//       dev->channel_limit  = AIC124_DIFF_CHANNEL_MAXNUM;
//      }
//      else
//      {
//       dev->channels_mode = 0xFFFF;
//       dev->channel_limit  = AIC124_SINGLE_CHANNEL_MAXNUM;
//      }

//   if(gains)
//    {
//     //Setup gains for all channels
//     int gnum;
//     int chnum = 0;
//     u8 * dest_gain = dev->gains;
//     bzero(dev->gains,sizeof(dev->gains));
//     for(gnum = 0;gnum<sizeof(dev->gains)/sizeof(dev->gains[0]);gnum++)
//     {

//      for(chnum = 0;chnum<4;chnum++)
//      {
//       //Setp 4 channel
//       u8 gval = 0xC0;
//       switch(*gains++)
//       {
//        case 0 :  gval = 0;break;
//        case 2 :  gval = 0x40;break;
//        case 4 :  gval = 0x80;break;
//       }
//       if(chnum) gval>>=(chnum<<1);
//       *dest_gain |=gval;
//      }
//      ++dest_gain;
//     }
//    }
    return 0;
}



void aic124dev_data_to_queue(volatile lpaic124dev dev)
{
    //Put ready data to queue
    //Ready data placed in other half work buffer

    s32    len;
    lpaic124_input ptr ;
    do {
        ptr = (lpaic124_input)dqueue_get_ptr(&dev->_queue_internal,&len);
        if(ptr)
        {

            if(dqueue_is_full(&dev->queue))
            {
                ++dev->queue_lost;
                ++dev->queue_overflow;
            }
            else
            {
                ptr->test_signals[0] = atomic_read(&dev->test_signals[0]);
                ptr->test_signals[1] = atomic_read(&dev->test_signals[1]);
                dqueue_add(&dev->queue,(u8*)ptr,len);
                ++dev->queue_total;
            }
            dqueue_remove_first(&dev->_queue_internal);
            dev->_wait_flag = 1;
            wake_up_interruptible(&dev->_wait_queue);
        }
    } while(ptr);


}

#ifndef _REAL_DEVICE

void aic124dev_test_scan(lpaic124dev dev)
{
    int i     = 128;
    DBG_TRACE(KERN_DEBUG,"TEST SCAN BEGIN\n");
    __aic124dev_start_scan(dev,1);
    for(i = 0; i<128 && __AIC124_DEV_SCAN_NONE != atomic_read(&dev->scan_state); i++)
    {
        DBG_TRACE(KERN_DEBUG,"step %d state %d ",i,atomic_read(&dev->scan_state));
        DBG_TRACE("","channel idx:number %u:%02X",(unsigned)dev->channel_idx,(unsigned)dev->channel_numbers[dev->channel_idx]);

        if(dev->aib_use)
        {
            DBG_TRACE(KERN_DEBUG,"aib channel %u:%02X  \n | aib_current %s\n"
                      ,(unsigned)dev->aib_channel_idx
                      ,(unsigned)dev->aib_channels[dev->channel_idx].channels[dev->aib_channel_idx]
                      ,dev->aib_current ? "not null" : "IS NULL!"
                     );
        }
        DBG_TRACE(KERN_DEBUG,"***Step %d next_scan return %d\n\n",i, __aic124dev_prepare_next_scan(dev));


    }

    DBG_TRACE(KERN_DEBUG,"Scan state %d "
              "channel idx:number %u:%02X "
              "aib channel %u:%02X "
              "| aib_current %s\n"
              ,atomic_read(&dev->scan_state)
              ,(unsigned)dev->channel_idx
              ,(unsigned)dev->channel_numbers[dev->channel_idx]
              ,(unsigned)dev->aib_channel_idx
              ,(unsigned)dev->aib_channels[dev->channel_idx].channels[dev->aib_channel_idx]
              ,dev->aib_current ? "not null" : "IS NULL!"
             );


    DBG_TRACE(KERN_DEBUG,"TEST SCAN END\n");
}

int aic124dev_test(lpaic124dev dev,int mode)
{
    if(aic124dev_is_device(dev))
    {
        aic124dev_test_scan(dev);
        return 0;
    }
    return -ENODEV;
}
#endif

