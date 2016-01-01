////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006-2012 MStar Semiconductor, Inc.
// All rights reserved.
//
// Unless otherwise stipulated in writing, any and all information contained
// herein regardless in any format shall remain the sole proprietary of
// MStar Semiconductor Inc. and be kept in strict confidence
// (??MStar Confidential Information??) by the recipient.
// Any unauthorized act including without limitation unauthorized disclosure,
// copying, use, reproduction, sale, distribution, modification, disassembling,
// reverse engineering and compiling of the contents of MStar Confidential
// Information is unlawful and strictly prohibited. MStar hereby reserves the
// rights to any and all damages, losses, costs and expenses resulting therefrom.
//
////////////////////////////////////////////////////////////////////////////////

/**
 *
 * @file    mstar_drv_qcom.c
 *
 * @brief   This file defines the interface of touch screen
 *
 * @version v2.2.0.0
 *
 */
 
/*=============================================================*/
// INCLUDE FILE
/*=============================================================*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include <linux/i2c.h>
#include <linux/kobject.h>
#include <asm/irq.h>
#include <asm/io.h>


#include "mstar_drv_platform_interface.h"

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
#include <linux/regulator/consumer.h>
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

/*=============================================================*/
// CONSTANT VALUE DEFINITION
/*=============================================================*/

#define MSG_TP_IC_NAME "msg2xxx" //"msg21xxA" or "msg22xx" or "msg26xxM" /* Please define the mstar touch ic name based on the mutual-capacitive ic or self capacitive ic that you are using */

/*=============================================================*/
// VARIABLE DEFINITION
/*=============================================================*/

struct i2c_client *g_I2cClient = NULL;

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
struct regulator *g_ReguVdd = NULL;
struct regulator *g_ReguVcc_i2c= NULL;
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

struct pinctrl *ts_pinctrl;
struct pinctrl_state *gpio_state_active;
struct pinctrl_state *gpio_state_suspend;

int mstar_reset_gpio;
int mstar_irq_gpio;
/*=============================================================*/
// FUNCTION DEFINITION
/*=============================================================*/

static int mstar_ts_pinctrl_init(struct i2c_client *client)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	ts_pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(ts_pinctrl)) {
		dev_dbg(&client->dev,
			"Target does not use pinctrl\n");
		retval = PTR_ERR(ts_pinctrl);
		ts_pinctrl = NULL;
		return retval;
	}

	gpio_state_active
		= pinctrl_lookup_state(ts_pinctrl,
			"pmx_ts_active");
	if (IS_ERR_OR_NULL(gpio_state_active)) {
		dev_dbg(&client->dev,
			"Can not get ts default pinstate\n");
		retval = PTR_ERR(gpio_state_active);
		ts_pinctrl = NULL;
		return retval;
	}

	gpio_state_suspend
		= pinctrl_lookup_state(ts_pinctrl,
			"pmx_ts_suspend");
	if (IS_ERR_OR_NULL(gpio_state_suspend)) {
		dev_err(&client->dev,
			"Can not get ts sleep pinstate\n");
		retval = PTR_ERR(gpio_state_suspend);
		ts_pinctrl = NULL;
		return retval;
	}

	return 0;
}

static int msg22xx_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	/* reset, irq gpio info */
	
	mstar_reset_gpio = of_get_named_gpio(np,
			"mstar,reset-gpio", 0);
	mstar_irq_gpio = of_get_named_gpio(np,
			"mstar,irq-gpio", 0);

	//printk(KERN_INFO"zg-----------msg21xx_parse_dt,reset=%d\n",msg21xx_pdata->reset_gpio);
	//printk(KERN_INFO"zg-----------msg21xx_parse_dt,irq=%d\n",msg21xx_pdata->irq_gpio);
	return 0;
}

/* probe function is used for matching and initializing input device */
static int touch_driver_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    int err = 0;
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
    const char *vdd_name = "vdd";
    const char *vcc_i2c_name = "vcc_i2c";
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

    DBG("*** %s ***\n", __FUNCTION__);
    
    if (client == NULL)
    {
        DBG("i2c client is NULL\n");
        return -1;
    }
    g_I2cClient = client;

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
    g_ReguVdd = regulator_get(&g_I2cClient->dev, vdd_name);
    g_ReguVcc_i2c = regulator_get(&g_I2cClient->dev, vcc_i2c_name);
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

    msg22xx_parse_dt(&client->dev);

	err = mstar_ts_pinctrl_init(client);
	if (err) {
		dev_err(&client->dev, "Can't initialize pinctrl\n");
	}
	err = pinctrl_select_state(ts_pinctrl, gpio_state_active);
	if (err) {
		dev_err(&client->dev,
			"Can't select pinctrl default state\n");
	}

    return MsDrvInterfaceTouchDeviceProbe(g_I2cClient, id);
}

/* remove function is triggered when the input device is removed from input sub-system */
static int touch_driver_remove(struct i2c_client *client)
{
    DBG("*** %s ***\n", __FUNCTION__);

    return MsDrvInterfaceTouchDeviceRemove(client);
}

/* The I2C device list is used for matching I2C device and I2C device driver. */
static const struct i2c_device_id touch_device_id[] =
{
    {MSG_TP_IC_NAME, 0},
    {}, /* should not omitted */ 
};

MODULE_DEVICE_TABLE(i2c, touch_device_id);

static struct of_device_id mstar_match_table[] = {
	{ .compatible = "mstar,msg2xxx", },
	{ },
};

static struct i2c_driver touch_device_driver =
{
    .driver = {
        .name = MSG_TP_IC_NAME,
        .owner = THIS_MODULE,
        .of_match_table = mstar_match_table,
    },
    .probe = touch_driver_probe,
    .remove = touch_driver_remove,
    .id_table = touch_device_id,
};

static int __init touch_driver_init(void)
{
    int ret;

    /* register driver */
    ret = i2c_add_driver(&touch_device_driver);
    if (ret < 0)
    {
        DBG("add touch device driver i2c driver failed.\n");
        return -ENODEV;
    }
    DBG("add touch device driver i2c driver.\n");

    return ret;
}

static void __exit touch_driver_exit(void)
{
    DBG("remove touch device driver i2c driver.\n");

    i2c_del_driver(&touch_device_driver);
}

module_init(touch_driver_init);
module_exit(touch_driver_exit);
MODULE_LICENSE("GPL");
