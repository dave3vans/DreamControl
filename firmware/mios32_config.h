/*
 * MIOS32 Configuration file for DreamControl
 *
 * ==========================================================================
 *
 *  Copyright (C) 2018 Dave Evans (dave@propertech.co.uk)
 *  Licensed for personal non-commercial use only. All other rights reserved.
 * 
 * ==========================================================================
 */


#ifndef _MIOS32_CONFIG_H
#define _MIOS32_CONFIG_H

// The boot message which is returned on a SysEx query.
#define MIOS32_LCD_BOOT_MSG_DELAY 0 
#define MIOS32_LCD_BOOT_MSG_LINE1 "DreamControl"
#define MIOS32_LCD_BOOT_MSG_LINE2 "(C) 2018 Dave Evans"

#define MIOS32_USB_VENDOR_ID    0x16c0                  // sponsored by voti.nl! see http://www.voti.nl/pids
#define MIOS32_USB_VENDOR_STR   "ProperTech Systems"    // For the USB device description.
#define MIOS32_USB_PRODUCT_STR  "DreamControl"          // For the MIDI device list.
#define MIOS32_USB_PRODUCT_ID   0x03f1                  
#define MIOS32_USB_VERSION_ID   0x0100                  // v1.00

#define MIOS32_USB_MIDI_NUM_PORTS 4

#endif /* _MIOS32_CONFIG_H */
