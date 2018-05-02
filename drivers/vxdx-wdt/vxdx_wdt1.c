/*
 *	VortexDX internal Watchdog Timer 1 driver
 *
 *      Release 0.20
 *
 *	(c) Copyright 2010 Fastwel Co Ltd.
 *	Author: Konstantin Chaplaev <chaplaev@fastwel.ru>
 *	Date: 19/02/10
 *
 *	Based on w83627hf_wdt.c which is based on advantechwdt.c
 *	which is based on wdt.c.
 *
 *	Original copyright messages:
 *
 *	(c) Copyright 2006 Samuel Tardieu <sam@rfc1149.net>
 *	(c) Copyright 2006 Marcus Junker <junker@anduras.de>
 *
 *	(c) Copyright 2003 Pádraig Brady <P@draigBrady.com>
 *
 *	(c) Copyright 2000-2001 Marek Michalkiewicz <marekm@linux.org.pl>
 *
 *	(c) Copyright 1996 Alan Cox <alan@lxorguk.ukuu.org.uk>,
 *						All Rights Reserved.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Marcus Junker nor ANDURAS AG admit liability nor provide
 *	warranty for any of this software. This material is provided
 *	"AS-IS" and at no charge.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include <asm/system.h>

#define WATCHDOG_NAME "VortexDX WDT1"
#define PFX WATCHDOG_NAME ": "
#define WATCHDOG_TIMEOUT 60		/* 60 sec default timeout */
#define WATCHDOG_EARLY_DISABLE 1	/* Disable until userland kicks in */

/* WDT1 registers */
#define WDT_RESET    0x67
#define WDT_CONTROL  0x68
#define WDT_SIG_SEL  0x69
#define WDT_COUNT    0x6A
#define WDT_STATUS   0x6D
#define WDT_REG_LEN   7

/* Start WDT I/O address */
const int wdt_io = WDT_RESET;

static unsigned long wdt_is_open;
static char expect_close;
static DEFINE_SPINLOCK(io_lock);

static int timeout = WATCHDOG_TIMEOUT;	/* in seconds */
module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout,
	"Watchdog timeout in seconds. 1<= timeout <=255 (default="
				__MODULE_STRING(WATCHDOG_TIMEOUT) ")");

static int nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, int, 0);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static int early_disable = WATCHDOG_EARLY_DISABLE;
module_param(early_disable, int, 0);
MODULE_PARM_DESC(early_disable,
	"Watchdog gets disabled at boot time (default="
				__MODULE_STRING(WATCHDOG_EARLY_DISABLE) ")");


/*
 *	Low level hardware specific functions.
 */

static void vxdx_init(void)
{
	spin_lock(&io_lock);
	/* Set the system reset as watchdog signal (p.220) */
	outb(0xD0, WDT_SIG_SEL);
	spin_unlock(&io_lock);
}

static void wdt_ping(void)
{
	spin_lock(&io_lock);
	/* Reset second watchdog */
	outb(0, WDT_RESET);
	spin_unlock(&io_lock);
}

static void wdt_enable(void)
{
	spin_lock(&io_lock);
	/* Set the 6 bit */
	outb(inb(WDT_CONTROL) | 0x40, WDT_CONTROL);
	spin_unlock(&io_lock);
}

static void wdt_disable(void)
{
	spin_lock(&io_lock);
	/* Clear the 6 bit */
	outb(inb(WDT_CONTROL) & ~0x40, WDT_CONTROL);
	spin_unlock(&io_lock);
}

static uint8_t wdt_running(void)
{
	uint8_t t;

	spin_lock(&io_lock);
	/* Read the 6 bit */
	t = inb(WDT_CONTROL) >> 6;
	spin_unlock(&io_lock);

	return t;
}

static int wdt_set_heartbeat(int t)
{
	uint32_t val;

	if (t < 1 || t > 511)
		return -EINVAL;

	timeout = t;
	/*
	 * This WDT uses 24-bit register at 32.768kHz, so time range is ~30.5us
	 * to 512s with resolution ~30.5us. Therefore, we get 32768 "ticks" per second.
	 * If you want to use a millisecond interval, then the calculating of the
	 * register value will be
	 *   val = (t / 1000) * 32768
	 * or after reducing
	 *   val = (t * 4096) / 125.
	 */
	val = timeout * 32768;

	spin_lock(&io_lock);
	/* Disable the WDT */
	outb(inb(WDT_CONTROL) & ~0x40, WDT_CONTROL);
	/* Load timeout to register */
	outb(val & 0xFF, WDT_COUNT);
	outb((val >> 8) & 0xFF, WDT_COUNT + 1);
	outb((val >> 16) & 0xFF, WDT_COUNT + 2);
	/* Enable the WDT */
	outb(inb(WDT_CONTROL) | 0x40, WDT_CONTROL);
	spin_unlock(&io_lock);

	return 0;
}

/*
 *	Kernel methods.
 */

static ssize_t wdt_write(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	if (count) {
		if (!nowayout) {
			size_t i;

			expect_close = 0;

			for (i = 0; i != count; i++) {
				char c;
				if (get_user(c, buf + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 42;
			}
		}
		wdt_ping();
	}
	return count;
}

static long wdt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	int new_timeout;
	static const struct watchdog_info ident = {
		.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT
							| WDIOF_MAGICCLOSE,
		.firmware_version = 1,
		.identity = "VortexDX WDT1",
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user(argp, &ident, sizeof(ident)))
			return -EFAULT;
		break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);

	case WDIOC_SETOPTIONS:
	{
		int options, retval = -EINVAL;

		if (get_user(options, p))
			return -EFAULT;

		if (options & WDIOS_DISABLECARD) {
			wdt_disable();
			retval = 0;
		}

		if (options & WDIOS_ENABLECARD) {
			wdt_enable();
			retval = 0;
		}

		return retval;
	}

	case WDIOC_KEEPALIVE:
		wdt_ping();
		break;

	case WDIOC_SETTIMEOUT:
		if (get_user(new_timeout, p))
			return -EFAULT;
		if (wdt_set_heartbeat(new_timeout))
			return -EINVAL;
		wdt_ping();
		/* Fall */

	case WDIOC_GETTIMEOUT:
		return put_user(timeout, p);

	default:
		return -ENOTTY;
	}
	return 0;
}

static int wdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &wdt_is_open))
		return -EBUSY;
	/*
	 *	Activate
	 */

	wdt_enable();
	return nonseekable_open(inode, file);
}

static int wdt_close(struct inode *inode, struct file *file)
{
	if (expect_close == 42)
		wdt_disable();
	else {
		printk(KERN_CRIT PFX
			"Unexpected close, not stopping watchdog!\n");
		wdt_ping();
	}
	expect_close = 0;
	clear_bit(0, &wdt_is_open);
	return 0;
}

/*
 *	Notifier for system down
 */

static int wdt_notify_sys(struct notifier_block *this, unsigned long code,
	void *unused)
{
	if (code == SYS_DOWN || code == SYS_HALT)
		wdt_disable();	/* Turn the WDT off */

	return NOTIFY_DONE;
}

/*
 *	Kernel Interfaces
 */

static const struct file_operations wdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= wdt_write,
	.unlocked_ioctl	= wdt_ioctl,
	.open		= wdt_open,
	.release	= wdt_close,
};

static struct miscdevice wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &wdt_fops,
};

/*
 *	The WDT needs to learn about soft shutdowns in order to
 *	turn the timebomb registers off.
 */

static struct notifier_block wdt_notifier = {
	.notifier_call = wdt_notify_sys,
};

static int vxdx_check_wdt(void)
{
	/* TODO: Here we must probe for VortexDX processor */

	/* Claim the I/O region */
	if (!request_region(wdt_io, WDT_REG_LEN, WATCHDOG_NAME)) {
		printk(KERN_ERR PFX
			"I/O address 0x%x already in use\n", wdt_io);
		return -EBUSY;
	}

	return 0;
}

static int __init wdt_init(void)
{
	int ret = -EBUSY;

	printk(KERN_INFO PFX "WDT1 driver for VortexDX initializing\n");

	ret = vxdx_check_wdt();
	if (ret < 0) {
		printk(KERN_ERR PFX "No VortexDX watchdogs could be found\n");
		goto out;
	}

	vxdx_init();

	if (early_disable) {
		if (wdt_running())
			printk(KERN_WARNING PFX "Stopping previously enabled "
					"watchdog until userland kicks in\n");
		wdt_disable();
	}

	if (wdt_set_heartbeat(timeout)) {
		wdt_set_heartbeat(WATCHDOG_TIMEOUT);
		printk(KERN_INFO PFX
		     "timeout value must be 1 <= timeout <= 511, using %d\n",
							WATCHDOG_TIMEOUT);
	}

	/* wdt_set_heartbeat() will start the wdt */
	wdt_disable();

	ret = register_reboot_notifier(&wdt_notifier);
	if (ret != 0) {
		printk(KERN_ERR PFX
			"cannot register reboot notifier (err=%d)\n", ret);
		goto unreg_regions;
	}

	ret = misc_register(&wdt_miscdev);
	if (ret != 0) {
		printk(KERN_ERR PFX
			"cannot register miscdev on minor=%d (err=%d)\n",
						WATCHDOG_MINOR, ret);
		goto unreg_reboot;
	}

	printk(KERN_INFO PFX "initialized. timeout=%d sec (nowayout=%d)\n",
		timeout, nowayout);

unreg_reboot:
	unregister_reboot_notifier(&wdt_notifier);
unreg_regions:
	release_region(wdt_io, WDT_REG_LEN);
out:
	return ret;
}

static void __exit wdt_exit(void)
{
	misc_deregister(&wdt_miscdev);
	unregister_reboot_notifier(&wdt_notifier);
	release_region(wdt_io, WDT_REG_LEN);
}

module_init(wdt_init);
module_exit(wdt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Konstantin Chaplaev <chaplaev@fastwel.ru>");
MODULE_DESCRIPTION("VortexDX internal WDT1 driver");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
