/*
 * Button functions for DreamControl
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
#include <stdbool.h>

#include "app.h"
#include "dc_buttons.h"
#include "dc_sysex.h"
#include "dc_meters.h"
#include "dc_knob.h"

/////////////////////////////////////////////////////////////////////////////
// local variables
/////////////////////////////////////////////////////////////////////////////

#define DOUT_PINS_COUNT 32              // The number of DOUT pins that we are using for LEDs.
#define J10_PINS_USED 6                 // The number of GPIO pins on J10A that we are using for buttons.

/////////////// BUTTONS
// * DO NOT CHANGE ORDER * index corresponds with DIN pin (0-31) or J10A pin (32-37, pins D0-D5)

typedef enum {
    /////// DIN pins 0-31
    BUTTON_LOUD = 0,
    BUTTON_MONO = 1,
    BUTTON_SIDE = 2,
    BUTTON_LOW = 3,
    BUTTON_LOMID = 4,
    BUTTON_HIMID = 5,
    BUTTON_HIGH = 6,
    // 7 not used
    
    BUTTON_MIX = 8,
    BUTTON_CUE1 = 9,
    BUTTON_CUE2 = 10,
    BUTTON_CUE3 = 11,
    BUTTON_CUE4 = 12,
    BUTTON_EXT1 = 13,
    BUTTON_EXT2 = 14,
    // 15 not used

    BUTTON_MAIN = 16,
    BUTTON_ALT1 = 17,
    BUTTON_ALT2 = 18,
    BUTTON_ALT3 = 19,
    BUTTON_MONMUTE = 20,
    BUTTON_DIM = 21,
    BUTTON_REF = 22,
    // 23 not used

    BUTTON_RETURN = 24,
    BUTTON_LOOP = 25,
    BUTTON_BACK = 26,
    BUTTON_FORWARD = 27,
    BUTTON_PLAY = 28,
    BUTTON_STOP = 29,
    BUTTON_CLICK = 30,
    BUTTON_RECORD = 31,

    /////// J10A pins D0-D5
    BUTTON_SHIFT = 32,
    BUTTON_MUTE = 33,
    BUTTON_SOLO = 34,
    BUTTON_READ = 35,
    BUTTON_WRITE = 36,
    BUTTON_TALK = 37
} button_t;

typedef enum {
    // 'Shifted' buttons - add 1000 to button_t value.
    BUTTON_RESET_METER = 1000,
    BUTTON_PEAK_LUFS = 1001,
    BUTTON_ABS_REL = 1002,
    BUTTON_1DB_PEAK_SCALE = 1003,
    BUTTON_3RD_METER_IS_MOMENTARY = 1004,

    BUTTON_SAVE = 1008,

    BUTTON_LOOP1 = 1024,
    BUTTON_LOOP2 = 1025,
    BUTTON_LOOP8 = 1026,
    BUTTON_LOOP16 = 1027,

    BUTTON_MUTE_CLEAR = 1033,
    BUTTON_SOLO_CLEAR = 1034,
    BUTTON_READ_ALL = 1035,
    BUTTON_WRITE_ALL = 1036,
    BUTTON_VOL_MOD = 1037
} button_shifted_t;

typedef enum {
    // 'Unassigned' buttons are actions we haven't assigned to physical buttons (yet)
    // but can be set in the plugin, e.g. some meter mode settings. Starts at 10000.
    NONE = 10001
} button_unassigned_t;

/////////////// LEDs
// * DO NOT CHANGE ORDER * index corresponds with DOUT pin (0-31)

typedef enum {
    /////// DOUT pins 0-31
    LED_LOUD = 7,
    LED_MONO = 6,
    LED_SIDE = 5,
    LED_LOW = 4,
    LED_LOMID = 3,
    LED_HIMID = 2,
    LED_HIGH = 1,
    LED_MIX = 0,

    LED_CUE1 = 15,
    LED_CUE2 = 14,
    LED_CUE3 = 13,
    LED_CUE4 = 12,
    LED_EXT1 = 11,
    LED_EXT2 = 10,
    LED_MAIN = 9,
    LED_ALT1 = 8,

    LED_ALT2 = 23,
    LED_ALT3 = 22,
    LED_MONMUTE = 21,
    LED_DIM = 20,
    LED_REF = 19,
    LED_RETURN = 18,
    LED_LOOP = 17,
    LED_PLAY = 16,

    LED_STOP = 31,
    LED_CLICK = 30,
    LED_RECORD = 29,
    LED_MUTE = 28,
    LED_SOLO = 27,
    LED_READ = 26,
    LED_WRITE = 25,
    LED_TALK = 24
} led_t;

typedef enum {
    PORT_NONE = 0,
    PORT_PLUGIN = 1,
    PORT_DAW = 2,
} port_t;

/////////////// BUTTON MIDI NOTE MAPPINGS

// Some functions are only handled by the plugin, others only by the DAW, and lots are handled by both
// (this is to allow directly controlling the DAW via MIDI, and for the plugin to translate e.g. OSC for Reaper integration).

// Array index: 0 = Port to send/receive (bitfield: 0=none, 1=plugin, 2=daw, 3=both), 1 = Button number, 2 = LED number... etc.
// -1 means no LED for that button. >=35 are shifted functions.

const int midi_button_led_map[171] = {                      // MIDI note number
    PORT_PLUGIN, BUTTON_LOUD, LED_LOUD,                     // 0
    PORT_PLUGIN, BUTTON_MONO, LED_MONO,                     // 1
    PORT_PLUGIN, BUTTON_SIDE, LED_SIDE,                     // 2
    PORT_PLUGIN, BUTTON_LOW, LED_LOW,                       // 3
    PORT_PLUGIN, BUTTON_LOMID, LED_LOMID,                   // 4
    PORT_PLUGIN, BUTTON_HIMID, LED_HIMID,                   // 5
    PORT_PLUGIN, BUTTON_HIGH, LED_HIGH,                     // 6
    PORT_DAW | PORT_PLUGIN, BUTTON_MIX, LED_MIX,            // 7
    PORT_DAW | PORT_PLUGIN, BUTTON_CUE1, LED_CUE1,          // 8
    PORT_DAW | PORT_PLUGIN, BUTTON_CUE2, LED_CUE2,          // 9
    PORT_DAW | PORT_PLUGIN, BUTTON_CUE3, LED_CUE3,          // 10
    PORT_DAW | PORT_PLUGIN, BUTTON_CUE4, LED_CUE4,          // 11
    PORT_DAW | PORT_PLUGIN, BUTTON_EXT1, LED_EXT1,          // 12
    PORT_DAW | PORT_PLUGIN, BUTTON_EXT2, LED_EXT2,          // 13
    PORT_DAW | PORT_PLUGIN, BUTTON_MAIN, LED_MAIN,          // 14
    PORT_DAW | PORT_PLUGIN, BUTTON_ALT1, LED_ALT1,          // 15
    PORT_DAW | PORT_PLUGIN, BUTTON_ALT2, LED_ALT2,          // 16
    PORT_DAW | PORT_PLUGIN, BUTTON_ALT3, LED_ALT3,          // 17
    PORT_PLUGIN, BUTTON_MONMUTE, LED_MONMUTE,               // 18
    PORT_PLUGIN, BUTTON_DIM, LED_DIM,                       // 19
    PORT_PLUGIN, BUTTON_REF, LED_REF,                       // 20
    PORT_DAW | PORT_PLUGIN, BUTTON_RETURN, LED_RETURN,      // 21
    PORT_DAW | PORT_PLUGIN, BUTTON_LOOP, LED_LOOP,          // 22
    PORT_DAW | PORT_PLUGIN, BUTTON_BACK, -1,                // 23
    PORT_DAW | PORT_PLUGIN, BUTTON_FORWARD, -1,             // 24
    PORT_DAW | PORT_PLUGIN, BUTTON_PLAY, LED_PLAY,          // 25
    PORT_DAW | PORT_PLUGIN, BUTTON_STOP, LED_STOP,          // 26
    PORT_DAW | PORT_PLUGIN, BUTTON_CLICK, LED_CLICK,        // 27
    PORT_DAW | PORT_PLUGIN, BUTTON_RECORD, LED_RECORD,      // 28
    PORT_NONE, BUTTON_SHIFT, -1,                            // 29
    PORT_DAW | PORT_PLUGIN, BUTTON_MUTE, LED_MUTE,          // 30
    PORT_DAW | PORT_PLUGIN, BUTTON_SOLO, LED_SOLO,          // 31
    PORT_DAW | PORT_PLUGIN, BUTTON_READ, LED_READ,          // 32
    PORT_DAW | PORT_PLUGIN, BUTTON_WRITE, LED_WRITE,        // 33
    PORT_DAW | PORT_PLUGIN, BUTTON_TALK, LED_TALK,          // 34

    PORT_PLUGIN, BUTTON_RESET_METER, -1,                    // 35     Shifted
    PORT_PLUGIN, BUTTON_PEAK_LUFS, -1,                      // 36
    PORT_PLUGIN, BUTTON_ABS_REL, -1,                        // 37
    PORT_DAW | PORT_PLUGIN, BUTTON_SAVE, -1,                // 38
    PORT_DAW | PORT_PLUGIN, BUTTON_LOOP1, -1,               // 39
    PORT_DAW | PORT_PLUGIN, BUTTON_LOOP2, -1,               // 40
    PORT_DAW | PORT_PLUGIN, BUTTON_LOOP8, -1,               // 41
    PORT_DAW | PORT_PLUGIN, BUTTON_LOOP16, -1,              // 42
    PORT_DAW | PORT_PLUGIN, BUTTON_MUTE_CLEAR, -1,          // 43
    PORT_DAW | PORT_PLUGIN, BUTTON_SOLO_CLEAR, -1,          // 44
    PORT_PLUGIN, BUTTON_VOL_MOD, -1,                        // 45
    PORT_DAW | PORT_PLUGIN, BUTTON_READ_ALL, -1,            // 46
    PORT_DAW | PORT_PLUGIN, BUTTON_WRITE_ALL, -1,           // 47

    PORT_PLUGIN, BUTTON_3RD_METER_IS_MOMENTARY, -1,         // 48   
    PORT_PLUGIN, BUTTON_1DB_PEAK_SCALE, -1,                 // 49

    PORT_DAW, BUTTON_MIX, -1,                               // 50   Send additional MIDI notes for MIX/CUE buttons.
    PORT_DAW, BUTTON_CUE1, -1,                              // 51   (this is for also switching the headphone output in Cubase Control Room).
    PORT_DAW, BUTTON_CUE2, -1,                              // 52
    PORT_DAW, BUTTON_CUE3, -1,                              // 53
    PORT_DAW, BUTTON_CUE4, -1,                              // 54
    PORT_DAW, BUTTON_EXT1, -1,                              // 55
    PORT_DAW, BUTTON_EXT2, -1,                              // 56
};

u16 current_j10_state;
bool is_shifted = false;

void DC_BUTTONS_Init()
{
    // Init first six J10A pins to be inputs with internal pull-up.
    // Our 6 channel strip buttons are connected to J10A.
    int i;
    for (i = 0; i < J10_PINS_USED; i++)
    {
        MIOS32_BOARD_J10_PinInit(i, MIOS32_BOARD_PIN_MODE_INPUT_PU);
    }

    for (i = 0; i < DOUT_PINS_COUNT; i++)
        MIOS32_DOUT_PinSet(i, 0);
    
    // Request all plugin button states from plugin.
    DC_SYSEX_SendCommand(PLUGIN_USB_PORT, SYSEX_COMMAND_SYNC_BUTTONS, "");
}

void DC_BUTTONS_Tick()
{
    // If the state of a J10A pin has changed, send corresponding MIDI note message (or set Shift).
    // The pin is 0 (0V) when pressed, as it is pulled up, so normally high.
    u16 new_j10_state = MIOS32_BOARD_J10_Get();

    int i;
    for (i = 0; i < J10_PINS_USED; i++)
    {
        int button = i + 32;       // J10 buttons start at 32.

        if ((new_j10_state & (1 << i)) == (1 << i) && (current_j10_state & (1 << i)) == 0)
        {
            MIOS32_MIDI_SendDebugMessage("J10  pin %d   value 0", i);

            if (button == BUTTON_SHIFT) 
                is_shifted = false;           
            else          
                DC_BUTTONS_ButtonChanged(button, 0);         
        }
        else if ((new_j10_state & (1 << i)) == 0 && (current_j10_state & (1 << i)) == (1 << i))
        {
            MIOS32_MIDI_SendDebugMessage("J10  pin %d   value 1", i);

            if (button == BUTTON_SHIFT) 
                is_shifted = true;           
            else          
                DC_BUTTONS_ButtonChanged(button, 1);         
        }
    }

    current_j10_state = new_j10_state;
}

void DC_BUTTONS_DIN_NotifyToggle(u32 pin, u32 pin_value)
{
    // If a DIN pin has changed (which our buttons are connected to), send corresponding MIDI note message.
    MIOS32_MIDI_SendDebugMessage("DIN  pin %d   value %d", pin, pin_value);

    // The pin is 0 (0V) when pressed, as it is pulled up, so normally high.
    if (pin < 32)
    {
        if (pin == BUTTON_SHIFT) 
            is_shifted = pin_value;           
        else          
            DC_BUTTONS_ButtonChanged(pin, 1 - pin_value);   
    } 
}

void DC_BUTTONS_ButtonChanged(u32 button, u8 state)
{
    MIOS32_MIDI_SendDebugMessage("BUTTON   button %d  state %d", button, state);

    // Send a NoteOn message when button state changes. Velocity=0 means off.
    // Also turn on/off corresponding LED, if applicable.
    int i;
    u8 index = 0;

    for (i = 0; i < sizeof(midi_button_led_map) / 4; i += 3)
    {
        if (midi_button_led_map[i + 1] == button + (is_shifted ? 1000 : 0))
        {
            MIOS32_MIDI_SendDebugMessage("MIDI OUT   port %d   val1 %d   val2 %d", midi_button_led_map[i], index, state);

            if (midi_button_led_map[i] & PORT_PLUGIN > 0)
                MIOS32_MIDI_SendNoteOn(PLUGIN_USB_PORT, Chn1, index, state == 1 ? 127 : 0);

            if (midi_button_led_map[i] & PORT_DAW > 0)
                MIOS32_MIDI_SendNoteOn(DAW_USB_PORT, Chn1, index, state == 1 ? 127 : 0);

            //if (midi_button_led_map[i + 1] == BUTTON_RETURN)
            //    MIOS32_DOUT_PinSet(midi_button_led_map[i + 2], state);
            //if (state == 1 && midi_button_led_map[i + 2] > -1) 
            //    MIOS32_DOUT_PinSet(midi_button_led_map[i + 2], 1 - MIOS32_DOUT_PinGet(midi_button_led_map[i + 2]));
        }
        
        index++;
    }  
}



void DC_BUTTONS_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{  
    // Turn on/off an LED if corresponding NoteOn value received.
    if (midi_package.chn == Chn1 && midi_package.type == NoteOn)
    {
        if (midi_package.note < (sizeof(midi_button_led_map) / 4 / 3) && 
        ((port == PLUGIN_USB_PORT && midi_button_led_map[midi_package.note * 3] & PORT_PLUGIN > 0) || 
        (port == DAW_USB_PORT && midi_button_led_map[midi_package.note * 3] & PORT_DAW > 0)))
        {
            int led_pin = midi_button_led_map[(midi_package.note * 3) + 2];
            MIOS32_MIDI_SendDebugMessage("LED PIN  %d   = %d", led_pin, midi_package.velocity > 0 ? 1 : 0);

            if (led_pin > -1) 
                MIOS32_DOUT_PinSet(led_pin, midi_package.velocity > 0 ? 1 : 0);

            int button = midi_button_led_map[(midi_package.note * 3) + 1];
            DC_BUTTONS_HandleCommand(button, midi_package.velocity > 0);
        }
    }
}

void DC_BUTTONS_HandleCommand(int button, bool state)
{
    // Perform an action when a button is pressed. Some buttons simply send MIDI messages,
    // others may change options or require feedback on the hardware.
    MIOS32_MIDI_SendDebugMessage("HANDLECOMMAND %d  =  %d", button, state);

    if (button == BUTTON_PEAK_LUFS)    
        DC_METERS_SetMeterMode(state);

    else if (button == BUTTON_ABS_REL)
        DC_METERS_SetRelativeMode(state);

    else if (button == BUTTON_3RD_METER_IS_MOMENTARY)
        DC_METERS_SetPeak3rdMeterMode(state);

    else if (button == BUTTON_1DB_PEAK_SCALE)
        DC_METERS_Set1dBScaleMode(state);

    else if (button == BUTTON_VOL_MOD)
        DC_KNOB_SetCurrentKnob(state);

    else if (button == BUTTON_MONMUTE)
        DC_KNOB_SetRingColour(state ? 0 : -1);

    else if (button == BUTTON_DIM)
        DC_KNOB_SetRingColour(state ? 60 : -1);

    else if (button == BUTTON_REF)
        DC_KNOB_SetRingColour(state ? 299 : -1);
        
    else
        return;
}

void DC_BUTTONS_Test()
{
    // Switch on all button LEDs.
    int i;
    for (i = 0; i < DOUT_PINS_COUNT; i++)
        MIOS32_DOUT_PinSet(i, 1);
}