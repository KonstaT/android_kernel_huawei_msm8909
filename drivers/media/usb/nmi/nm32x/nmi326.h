/*****************************************************************************
 Copyright(c) 2012 NMI Inc. All Rights Reserved

 File name : nmi326.h

 Description : NM326 host interface

 History :
 ----------------------------------------------------------------------
 2012/11/27 	ssw		initial
*******************************************************************************/

#ifndef __NMI_HW_H__
#define __NMI_HW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
//#include "cust_gpio_usage.h"
//#include "cust_eint.h"

//#define NM_DEBUG_ON /*Modify by lichuangchuang for QL702 DTV GPIO state (QL702) SW00000000 2015-04-25*/

#ifdef NM_DEBUG_ON
//#define NM_KMSG(x...) printk(KERN_DEBUG ">ISDBT< " x)
#define NM_KMSG(x...) printk(x)
#else
#define NM_KMSG(x...) /* null */
#endif

#define ISDBT_DEV_NAME		"isdbt"  //isdbt
#define ISDBT_DEV_MAJOR	227
#define ISDBT_DEV_MINOR	0

#define DTV_IRQ_DEINIT			0
#define DTV_IRQ_INIT			1
#define DTV_IRQ_SET				2

void isdbt_gpio_power_off(void); //Added by lichuangchuang for DTV gpio (8909) SW00000000 2015/02/14

typedef struct {
	long		index;
	void		**hInit;
	void		*hI2C;
	spinlock_t	isr_lock;
	struct fasync_struct *async_queue;
	unsigned char* rwBuf;
} ISDBT_OPEN_INFO_T;

#define MAX_OPEN_NUM		8

#define IOCTL_MAGIC	't'

#define IOCTL_MAXNR				9

#define IOCTL_ISDBT_POWER_ON		_IO( IOCTL_MAGIC, 0 )
#define IOCTL_ISDBT_POWER_OFF		_IO( IOCTL_MAGIC, 1 )
#define IOCTL_ISDBT_RST_DN			_IO( IOCTL_MAGIC, 2 )
#define IOCTL_ISDBT_RST_UP			_IO( IOCTL_MAGIC, 3 )

#define IOCTL_ISDBT_INTERRUPT_REGISTER		_IO(IOCTL_MAGIC, 4)
#define IOCTL_ISDBT_INTERRUPT_UNREGISTER	_IO(IOCTL_MAGIC, 5)
#define IOCTL_ISDBT_INTERRUPT_ENABLE		_IO(IOCTL_MAGIC, 6)
#define IOCTL_ISDBT_INTERRUPT_DISABLE		_IO(IOCTL_MAGIC, 7)
#define IOCTL_ISDBT_INTERRUPT_DONE		_IO(IOCTL_MAGIC, 8)

#ifdef __cplusplus
}
#endif

#endif // __NMI_HW_H__

