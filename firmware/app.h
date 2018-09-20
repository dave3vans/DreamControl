/*
 *              ~|  DreamControl |~
 * 
 *             Studio MIDI controller
 *
 * ==========================================================================
 *
 *  Copyright (C) 2018 Dave Evans (dave@propertech.co.uk)
 *  Licensed for personal non-commercial use only. All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _APP_H
#define _APP_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

#define DAW_USB_PORT USB0                               // USB port our DAW is connected to.
                                                        // This is currently Cubase, via a Generic Remote mapping.
#define PLUGIN_USB_PORT USB1                            // USB port our plugin is directly connected to.
                                                        // We use this for controlling plugin-specific parameters directly, and sending metering sysex.
#define MF_CALIBRATION_USB_PORT USB2                    // USB2 is only used for MF board calibration.

#define LED_BRIGHTNESS 0.02f                            // **** DANGER ** DO NOT ALTER THIS. MAY EXCEED CURRENT RATING OF POWER SUPPLY *******
                                                        // Also, values below 0.02 cause incorrect colours to be displayed.


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern void APP_Init(void);
extern void APP_Background(void);
extern void APP_Tick(void);
extern void APP_MIDI_Tick(void);
extern void APP_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package);
extern void APP_SRIO_ServicePrepare(void);
extern void APP_SRIO_ServiceFinish(void);
extern void APP_DIN_NotifyToggle(u32 pin, u32 pin_value);
extern void APP_ENC_NotifyChange(u32 encoder, s32 incrementer);
extern void APP_AIN_NotifyChange(u32 pin, u32 pin_value);

/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

#endif /* _APP_H */
