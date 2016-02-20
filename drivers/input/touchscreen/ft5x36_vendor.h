/*
 focaltelech tp firmware update infomations
 
 Date           Author       Module    vendor id    Old_ver    New_ver
 2015.04.13     pangle       jinlong     0x11         null         0x10
 2015.04.13     pangle       jinlong     0x9b         0x10         0x11
 2015.05.18     pangle       jinlong     0x9b         0x11         0x13

 */
#ifndef __FOCALTECH_VENDOR_H__
#define __FOCALTECH_VENDOR_H__

#include "ft5x36_vendor_id.h"

//added by pangle for 860 fw upgrade at 20150413 begin
#if defined(CONFIG_QL860_BASE)
static unsigned char FT6X36_FIRMWARE0x9b_JINLONG[] = {
#if defined(CONFIG_QL860_BASE)
	//#include "ft5x36_firmware/QL860_IBD_Ft6336_JINLONG0x9b_Ver0x11_20150413.h"
	#include "ft5x36_firmware/QL860_IBD_Ft6336_JINLONG0x9b_Ver0x13_20150515.h"
#endif
}; 
#endif
//added by pangle for 860 fw upgrade at 20150413 begin



#endif
