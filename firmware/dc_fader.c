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
u8 current_daw_channel_value_lsb = -1;
u8 daw_channel_nrpn_is_next = -1;


void DC_FADER_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{  
    ///////////////////  FADER POSITION TO/FROM DAW

    // MF board sends MIDI CC0 and CC32 for 14bit value, but our DAW only accepts NRPN values, so we convert.
    // Also, we invert values because our fader is upside down!

    // Receive from MF board, transmit to DAW.
    if (port == UART0 && midi_package.chn == Chn1 && midi_package.type == CC)
    {
        if (midi_package.cc_number == MF_VALUE_CC_MSB)
        {
            current_mf_value_msb = midi_package.value;
        }
        else if (midi_package.cc_number == MF_VALUE_CC_LSB && current_mf_value_msb > -1)
        {
            // Convert to NRPN for DAW and plugin.
            MIOS32_MIDI_SendCC(DAW_USB_PORT, Chn1, 99, 0);
            MIOS32_MIDI_SendCC(DAW_USB_PORT, Chn1, 98, MF_VALUE_NRPN_LSB);
            MIOS32_MIDI_SendCC(DAW_USB_PORT, Chn1, 38, 127 - midi_package.value);
            MIOS32_MIDI_SendCC(DAW_USB_PORT, Chn1, 6, 127 - current_mf_value_msb);

            MIOS32_MIDI_SendCC(PLUGIN_USB_PORT, Chn1, 99, 0);
            MIOS32_MIDI_SendCC(PLUGIN_USB_PORT, Chn1, 98, MF_VALUE_NRPN_LSB);
            MIOS32_MIDI_SendCC(PLUGIN_USB_PORT, Chn1, 38, 127 - midi_package.value);        
            MIOS32_MIDI_SendCC(PLUGIN_USB_PORT, Chn1, 6, 127 - current_mf_value_msb);

            current_mf_value_msb = -1;
        }
        else if (midi_package.cc_number == MF_TOUCH_CC)
        {
            // Send fader touch sense to DAW and plugin.
            MIOS32_MIDI_SendCC(DAW_USB_PORT, Chn1, MF_TOUCH_CC, midi_package.value);
            MIOS32_MIDI_SendCC(PLUGIN_USB_PORT, Chn1, MF_TOUCH_CC, midi_package.value);
        }
    }
 
    // Receive from DAW, transmit to MF board.
    if ((port == PLUGIN_USB_PORT || port == DAW_USB_PORT) && midi_package.chn == Chn1 && midi_package.type == CC)
    {
        if (midi_package.cc_number == 98 && midi_package.value == MF_VALUE_NRPN_LSB)       // 98 = NRPN LSB
        {
            daw_channel_nrpn_is_next = 1;
        }
        else if (midi_package.cc_number == 38 && daw_channel_nrpn_is_next == 1)            // 38 = Data Entry LSB
        {
            current_daw_channel_value_lsb = midi_package.value;           
        }
        else if (midi_package.cc_number == 6 && current_daw_channel_value_lsb > -1)        // 6 = Data Entry MSB
        {
            // Convert to CC for MF board
            MIOS32_MIDI_SendCC(UART0, Chn1, MF_VALUE_CC_MSB, 127 - midi_package.value);
            MIOS32_MIDI_SendCC(UART0, Chn1, MF_VALUE_CC_LSB, 127 - current_daw_channel_value_lsb);
            current_daw_channel_value_lsb = -1;
            daw_channel_nrpn_is_next = -1;
        }
    }
}