
/* Copyright (c) 2012-2012, YEP Telecom. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
//#include <mach/clk.h>

#include <mach/hardware_info.h>

static struct platform_device *hardware_info_pdev = NULL;
static DEFINE_MUTEX(registration_lock);

// modified by yangze for sensor hardware devices info (x825) 2013-10-12 begin
#define HARDWARE_INFO_ATTR(name)				\
	static ssize_t name##_show(struct device *dev, struct device_attribute *attr, char *buf)\
	{\
		struct hardware_info_pdata *hardware_info = dev_get_drvdata(dev);\
		if(hardware_info->name==NULL)\
			snprintf(hardware_info->name,125,"sorry,not detected");\
		return sprintf(buf,  "%s\n", hardware_info->name);\
	}\
	static ssize_t name##_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)\
	{\
		struct hardware_info_pdata *hardware_info = dev_get_drvdata(dev);\
		snprintf(hardware_info->name,125, buf);\
		return count;\
	}\
	static DEVICE_ATTR(name, 0664, name##_show, name##_store)
// modified by yangze for sensor hardware devices info (x825) 2013-10-12 end
	
 HARDWARE_INFO_ATTR(lcm);
 HARDWARE_INFO_ATTR(ctp);
 //add wifi and bluetooth hardinfo for ql701  20150124 start by shihuijun
 HARDWARE_INFO_ATTR(wifi);
 HARDWARE_INFO_ATTR(bluetooth);
 //add wifi and bluetooth hardinfo for ql701  20150124 start by shihuijun
 HARDWARE_INFO_ATTR(main_cam);
 HARDWARE_INFO_ATTR(sub_cam);
 HARDWARE_INFO_ATTR(flash);
 HARDWARE_INFO_ATTR(gsensor);
 HARDWARE_INFO_ATTR(msensor);
 HARDWARE_INFO_ATTR(als_ps);
 HARDWARE_INFO_ATTR(gyro);
 HARDWARE_INFO_ATTR(emcp);
 HARDWARE_INFO_ATTR(rom);
 HARDWARE_INFO_ATTR(ram);


static struct hardware_info_pdata hardware_info_data = 
{
    .lcm = "lcm",
    .ctp = "ctp",
    .main_cam = "main_cam",
    .sub_cam = "sub_cam",
    .flash = "flash",
    .gsensor = "gsensor",
    .msensor = "msensor",
    .als_ps = "als_ps",
    .gyro = "gyro",
    .emcp = "emcp",
    .rom = "rom",
    .ram = "ram",
    //add wifi and bluetooth hardinfo for ql701  20150124 start by shihuijun
    .wifi="wifi",
    .bluetooth="bluetooth",
    //add wifi and bluetooth hardinfo for ql701  20150124 start by shihuijun
    
};

static int hardware_info_probe(struct platform_device *pdev)
{
	
	struct hardware_info_pdata *hardware_pdata;

	hardware_pdata = pdev->dev.platform_data;

	platform_set_drvdata(pdev, hardware_pdata);
	hardware_info_pdev = pdev;
	return 0;
}

static int hardware_info_remove(struct platform_device *pdev)
{
	return 0;
}

// modified by yangze for sensor hardware devices info (x825) 2013-10-12 begin
int register_hardware_info(HARDWARE_ID id, char* name)
{
	int ret=0;
	
	struct hardware_info_pdata *hardware_info;
	// modified by yangze for sensor hardware devices info (x825) 2013-10-12 begin
	if(NULL == hardware_info_pdev){
		return -1;
	}
	// modified by yangze for sensor hardware devices info (x825) 2013-10-12 end
	printk("%s:hardware id=%d, name=%s\n", __func__, id, name);
	hardware_info = platform_get_drvdata(hardware_info_pdev);
	mutex_lock(&registration_lock);
	switch(id){
	case LCM:
		snprintf(hardware_info->lcm,125, name);
		ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_lcm);
		if (ret) {
			printk("%s:error on device_create_file %s\n", __func__, name);
		}
		break;
	case CTP:
		snprintf(hardware_info->ctp,125,name);
		ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_ctp);
		if (ret) {
			printk("%s:error on device_create_file %s\n", __func__, name);
		}
		break;
	case MAIN_CAM:
		snprintf(hardware_info->main_cam,125, name);
		ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_main_cam);
		if (ret) {
			printk("%s:error on device_create_file %s\n", __func__, name);
		}
		break;
	case SUB_CAM:
		snprintf(hardware_info->sub_cam, 125,name);
		ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_sub_cam);
		if (ret) {
			printk("%s:error on device_create_file %s\n", __func__, name);
		}
		break;
	case FLASH:
		snprintf(hardware_info->flash,125,name);
		ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_flash);
		if (ret) {
			printk("%s:error on device_create_file %s\n", __func__, name);
		}
		break;
	case GSENSOR:
		snprintf(hardware_info->gsensor, 125,name);
		ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_gsensor);
		if (ret) {
			printk("%s:error on device_create_file %s\n", __func__, name);
		}
		break;
	case MSENSOR:
		snprintf(hardware_info->msensor,125,name);
		ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_msensor);
		if (ret) {
			printk("%s:error on device_create_file %s\n", __func__, name);
		}
		break;
	case ALS_PS:
		snprintf(hardware_info->als_ps, 125,name);
		ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_als_ps);
		if (ret) {
			printk("%s:error on device_create_file %s\n", __func__, name);
		}
		break;
	case GYRO:
		snprintf(hardware_info->gyro, 125,name);
		ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_gyro);
		if (ret) {
			printk("%s:error on device_create_file %s\n", __func__, name);
		}
		break;
    case EMCP:
        snprintf(hardware_info->emcp,125, name);
        ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_emcp);
        if (ret) {
            printk("%s:error on device_create_file %s\n", __func__, name);
        }
        break;
    case ROM:
        snprintf(hardware_info->rom, 125,name);
        ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_rom);
        if (ret) {
            printk("%s:error on device_create_file %s\n", __func__, name);
        }
        break;
    case RAM:
        snprintf(hardware_info->ram, 125,name);
        ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_ram);
        if (ret) {
            printk("%s:error on device_create_file %s\n", __func__, name);
        }
        break;
    //add wifi and bluetooth hardinfo for ql701  20150124 start by shihuijun
    case WIFI:
        snprintf(hardware_info->wifi, 125,name);
        ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_wifi);
        if (ret) {
            printk("%s:error on device_create_file %s\n", __func__, name);
        }
        break;
	case BLUETOOTH:
        snprintf(hardware_info->bluetooth, 125,name);
        ret = device_create_file(&hardware_info_pdev->dev, &dev_attr_bluetooth);
        if (ret) {
            printk("%s:error on device_create_file %s\n", __func__, name);
        }
        break;
	//add wifi and bluetooth hardinfo for ql701  20150124 end by shihuijun
	default:
		ret = -1;
		break;

	}
	
	mutex_unlock(&registration_lock);

	return ret;
}
// modified by yangze for sensor hardware devices info (x825) 2013-10-12 end

EXPORT_SYMBOL(register_hardware_info);

static struct of_device_id hardware_info_table[] = {
    { .compatible ="yep,hardware_info"},
    { },
};
static struct platform_device hardware_info_device = {
    .name = "hardware_info",
    .id = -1,
    .dev = {
        .platform_data = &hardware_info_data,
    },
};
static struct platform_driver hardware_info_driver = {
	.probe = hardware_info_probe,
	.remove = hardware_info_remove,
	.shutdown = NULL,
	.driver = {
		   .name = "hardware_info",
           .owner = THIS_MODULE,
           .of_match_table = hardware_info_table,
		   },
};
static int __init hardware_info_driver_init(void)
{
	// modified by yangze for sensor hardware devices info (x825) 2013-10-12 begin
	int rc = 0;
	
	platform_device_register(&hardware_info_device);
	
    rc = platform_driver_register(&hardware_info_driver);
	
	if(rc == 0){
//deleted by litao 20140725 begin
#if 0
		register_hardware_info(GSENSOR, "sorry,not detected");
		register_hardware_info(MSENSOR, "sorry,not detected");
		register_hardware_info(ALS_PS, "sorry,not detected");
//added by litao to register the hardware info (ql1000) 2014-02-11
		register_hardware_info(GYRO, "sorry,not detected");
#endif
//deleted by litao 20140725 end
//added by yangze to register the camera led-flash hardware info (ql1700) 2014-07-16		
		register_hardware_info(FLASH, "SGM3780-2MHz-1.5A-led-flash-driver");
//add wifi and bluetooth hardinfo for ql701  20150124 start by shihuijun
        register_hardware_info(WIFI, "Host SW:3.2.3.172, FW:00083, HW:WCN v2.0");
        register_hardware_info(BLUETOOTH, "HW:WCN3620 v4.0");
//add wifi and bluetooth hardinfo for ql701  20150124 end by shihuijun

	}
	
	return rc;
	// modified by yangze for sensor hardware devices info (x825) 2013-10-12 end
}

postcore_initcall(hardware_info_driver_init);
/*
MODULE_AUTHOR("yep");
MODULE_DESCRIPTION("get all hardware device info driver");
MODULE_LICENSE("GPL");

module_init(hardware_info_driver_init);
module_exit(hardware_info_driver_exit);
*/

