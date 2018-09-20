/*
 * MIDI System Exclusive (SysEx) functions for DreamControl
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
#include <string.h>

#include "app.h"
#include "dc_sysex.h"
#include "dc_meters.h"

/////////////////////////////////////////////////////////////////////////////
// local variables
/////////////////////////////////////////////////////////////////////////////

// SysEx start byte and our manufacturer ID.
static const u8 sysex_header[4] = { 0xf0, 0x00, 0x21, 0x69 };

// Various variable set when receiving SysEx.
static u8 is_receiving_sysex = 0;
static u8 sysex_counter = 0;
static dc_sysex_command_t sysex_command = 0;
char sysex_data[64];


void DC_SYSEX_Init()
{
    // Install SysEx callback.
    MIOS32_MIDI_SysExCallback_Init(DC_SYSEX_Parser);
}

s32 DC_SYSEX_Parser(mios32_midi_port_t port, u8 midi_in)
{
    // Ignore 'realtime' messages.
    if (midi_in >= 0xf8)
        return 0;

    if (!is_receiving_sysex)
    {
        // Are we receiving the sysex header?
        if (midi_in == sysex_header[sysex_counter])
        {
            sysex_counter++;

            if (sysex_counter == sizeof(sysex_header)) 
            {
                sysex_counter = 0;
                is_receiving_sysex = 1;
            }
        }
        else
        {    
            // Invalid header byte received - ignore this message until the next start byte.    
            sysex_counter = 0;
        }
    }
    else
    {
        // Receive actual data.
        if (midi_in != 0xF7 && sysex_counter < sizeof(sysex_data))
        {
            if (sysex_counter == 0)
            {
                sysex_command = midi_in;
            }
            else
            {
                sysex_data[sysex_counter - 1] = (char)midi_in;
            }

            sysex_counter++;
        }
        else
        {
            // End byte received or our input buffer is full, so stop receiving and execute command.
            is_receiving_sysex = 0;
            sysex_counter = 0;

            DC_SYSEX_ReceiveCommand();
        }
    }

    return 0;
}

void DC_SYSEX_ReceiveCommand()
{
    switch (sysex_command)
    {
        case SYSEX_COMMAND_METER_DATA:
            DC_METERS_Update(sysex_data);
            break;
    }
}

void DC_SYSEX_SendCommand(mios32_midi_port_t port, u8 sysex_command_code, const char *sysex_data)
{
    // Build our sysex message to send.
    char data_to_send[sizeof(sysex_data) + sizeof(sysex_header) + 2];
    memcpy(&data_to_send[0], &sysex_header[0], sizeof(sysex_header));
    data_to_send[sizeof(sysex_header)] = sysex_command_code;
    memcpy(&data_to_send[sizeof(sysex_header) + 1], &sysex_data[0], sizeof(sysex_data));
    data_to_send[sizeof(data_to_send) - 1] = 0xF7;      // Sysex end byte

    MIOS32_MIDI_SendSysEx(port, data_to_send, sizeof(data_to_send));
}