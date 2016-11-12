#ifndef __FOCALTECH_INFO_H__
#define __FOCALTECH_INFO_H__

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include "ft5x36_vendor_id.h"


typedef struct{  
u8 vendor_id;  
char ven_name[16] ;
} ft5x36_ven_info;

/*add vendor name in vendor name array*/
static ft5x36_ven_info ft5x36_name[]=
{
{FT5X36_VENDOR_BYD,"BYD"},
{FT5X36_VENDOR_CHAOSHENG,"ChaoSheng"},
{FT5X36_VENDOR_TOPTOUCH,"Toptouch"},
{FT5X36_VENDOR_JTOUCH,"J-Touch"},
{FT5X36_VENDOR_DAWOSI,"Dawos"},
{FT5X36_VENDOR_TRULY,"Truly"},
{FT6X06_VENDOR0x86_EKEY,"Ekey"},
{FT6X06_VENDOR0x11_HONGZHAN,"HongZhan"},
{FT6X06_VENDOR0xD2_HONGZHAN,"HongZhan"},
{FT6X06_VENDOR0x80_EACH,"EACH"},
{FT6X06_VENDOR0xD7_TONGHAO,"TongHao"},
{FT6X06_VENDOR_JINCHEN,"JinChen"},
};
#endif
