/*
 * Fader functions for DreamControl
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

#include "app.h"
#include "dc_fader.h"

/////////////////////////////////////////////////////////////////////////////
// local variables
/////////////////////////////////////////////////////////////////////////////

#define MF_VALUE_CC_MSB 0                               // MIDI messages that our Motorfader (MF) should transmit.
#define MF_VALUE_CC_LSB 32                              // MF board needs to be in Emulated Motormix mode.
#define MF_TOUCH_CC 47
#define MF_VALUE_NRPN_LSB 1                             // DAW selected channel fader set to NRPN #1.

u8 current_mf_value_msb = -1;                           // if -1, we're waiting for a new value's MSB, 
                                                        // otherwise we're waiting for the corresponding LSB.
u8 current_daw_channel_value_msb = -1;
u8 daw_channel_nrpn_is_next = -1;


void DC_FADER_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{  
    ///////////////////  FADER POSITION TO/FROM DAW

    // MF board sends MIDI CC0 and CC32 for 14bit value, but our DAW only accepts NRPN values, so we convert.
    if (port == UART0 && midi_package.chn == Chn1 && midi_package.type == CC)
    {
        if (midi_package.cc_number == MF_VALUE_CC_MSB)
        {
            current_mf_value_msb = midi_package.value;
        }
        else if (midi_package.cc_number == MF_VALUE_CC_LSB && current_mf_value_msb > -1)
        {
            // Convert to NRPN for DAW
            MIOS32_MIDI_SendCC(DAW_USB_PORT, Chn1, 99, 0);
            MIOS32_MIDI_SendCC(DAW_USB_PORT, Chn1, 98, MF_VALUE_NRPN_LSB);
            MIOS32_MIDI_SendCC(DAW_USB_PORT, Chn1, 6, current_mf_value_msb);
            MIOS32_MIDI_SendCC(DAW_USB_PORT, Chn1, 38, midi_package.value);        
            current_mf_value_msb = -1;
        }
    }
    if (port == DAW_USB_PORT && midi_package.chn == Chn1 && midi_package.type == CC)
    {
        if (midi_package.cc_number == 98 && midi_package.value == MF_VALUE_NRPN_LSB)       // 98 = NRPN LSB
        {
            daw_channel_nrpn_is_next = 1;
        }
        else if (midi_package.cc_number == 6 && daw_channel_nrpn_is_next == 1)             // 6 = Data Entry MSB
        {
            current_daw_channel_value_msb = midi_package.value;           
        }
        else if (midi_package.cc_number == 38 && current_daw_channel_value_msb > -1)       // 38 = Data Entry LSB
        {
            // Convert to CC for MF board
            MIOS32_MIDI_SendCC(UART0, Chn1, MF_VALUE_CC_MSB, current_daw_channel_value_msb);
            MIOS32_MIDI_SendCC(UART0, Chn1, MF_VALUE_CC_LSB, midi_package.value);
            current_daw_channel_value_msb = -1;
            daw_channel_nrpn_is_next = -1;
        }
    }
}