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

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include <ws2812.h>
#include <math.h>
#include <glcd_font.h>

#include "app.h"
#include "dc_meters.h"
#include "dc_knob.h"
#include "dc_fader.h"
#include "dc_buttons.h"
#include "dc_lcd.h"
#include "dc_sysex.h"

/////////////////////////////////////////////////////////////////////////////
// Globals
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
    MIOS32_BOARD_Init(0);
    MIOS32_BOARD_LED_Init(0xffffffff);

    // Init WS2812 RGB LEDs
    s32 success;
    success = WS2812_Init(0);
    if (success < 0)
        MIOS32_MIDI_SendDebugString("Failed to init WS2812!");

    // Init modules.
    DC_BUTTONS_Init();
    DC_LCD_Init();
    DC_KNOB_Init();
    DC_SYSEX_Init();
    MIOS32_LCD_Init(0);
    
    if (success < 0)
    {
        // Red LED on CPU board to show a problem occurred at Init.
        MIOS32_BOARD_LED_Set(0x0004, 0x0004);
    }
    else
    {
        // Blue LED on CPU board to show Init was completed successfully.
        MIOS32_BOARD_LED_Set(0x0008, 0x0008);
    }
    
    // Test mode - hold SHIFT at boot to switch on all LEDs.
    // TODO: Getting J10 pin value causes LCD reset to fail! Not sure why...
    if (false)  // (MIOS32_BOARD_J10_PinGet(0) == 0)
    {
        DC_BUTTONS_Test();
        DC_METERS_Test();
        DC_KNOB_Test();
    }
}

/////////////////////////////////////////////////////////////////////////////
// This task is running in the background
/////////////////////////////////////////////////////////////////////////////
void APP_Background(void)
{

}

/////////////////////////////////////////////////////////////////////////////
// This hook is called each mS from the main task
/////////////////////////////////////////////////////////////////////////////
void APP_Tick(void)
{
    DC_LCD_Tick();
    DC_BUTTONS_Tick();
}

/////////////////////////////////////////////////////////////////////////////
// This hook is called each mS from the MIDI task
/////////////////////////////////////////////////////////////////////////////
void APP_MIDI_Tick(void)
{

}

/////////////////////////////////////////////////////////////////////////////
// This hook is called when a MIDI package has been received
/////////////////////////////////////////////////////////////////////////////
void APP_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{
    // Toggle Status LED on MIDI received.
    MIOS32_BOARD_LED_Set(0x0001, ~MIOS32_BOARD_LED_Get());

    // Forward USB3<->UART0 (J11E M0). This port is used for calibrating/updating the MF board.
    if (port == MF_CALIBRATION_USB_PORT)    
        MIOS32_MIDI_SendPackage(UART0, midi_package);    
    else if (port == UART0)    
        MIOS32_MIDI_SendPackage(MF_CALIBRATION_USB_PORT, midi_package);   

    // Pass MIDI package on to other modules.
    DC_BUTTONS_MIDI_NotifyPackage(port, midi_package);
    DC_KNOB_MIDI_NotifyPackage(port, midi_package);
    DC_FADER_MIDI_NotifyPackage(port, midi_package);
    DC_METERS_MIDI_NotifyPackage(port, midi_package);
}

/////////////////////////////////////////////////////////////////////////////
// This hook is called before the shift register chain is scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServicePrepare(void)
{
    u16 state = MIOS32_BOARD_J5_Get();
    MIOS32_ENC_StateSet(0, (state >> 2) & 3);
}

/////////////////////////////////////////////////////////////////////////////
// This hook is called after the shift register chain has been scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServiceFinish(void)
{
}

/////////////////////////////////////////////////////////////////////////////
// This hook is called when a button has been toggled.
// pin_value is 1 when button released, and 0 when button pressed
/////////////////////////////////////////////////////////////////////////////
void APP_DIN_NotifyToggle(u32 pin, u32 pin_value)
{
    DC_BUTTONS_DIN_NotifyToggle(pin, pin_value);
}

/////////////////////////////////////////////////////////////////////////////
// This hook is called when an encoder has been moved
// incrementer is positive when encoder has been turned clockwise, else
// it is negative
/////////////////////////////////////////////////////////////////////////////
void APP_ENC_NotifyChange(u32 encoder, s32 incrementer)
{
    DC_KNOB_NotifyChange(encoder, incrementer);
}

 void APP_AIN_NotifyChange(u32 pin, u32 pin_value)
 {
 }
