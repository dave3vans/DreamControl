/*
 * Virtual knob functions for DreamControl
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
#include <stdbool.h>

#include "app.h"
#include "dc_knob.h"
#include "dc_lcd.h"

/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#define ENCODER_COUNT 1                                 // The number of physical knobs. This could be expanded in future.
#define ENCODER_VOLUME_RANGE 96                         // The range of the encoder when controlling volume (corresponds to dB).
#define ENCODER_CC_RANGE 128                            // The range of the encoder when controlling other parameters (7 bit MIDI standard).

#define LED_RING_SIZE 16                                // Number of LEDs in our encoder ring.
#define LED_RING_START_INDEX 45                         // The start index of the first LED.
#define LED_RING_OFFSET -5                              // Depending on how the led ring board is mounted, the first LED may not be at the '-45' position.
#define LED_RING_BRIGHTNESS_OFFSET -0.01                // The LED ring might have slightly different brightness, so we can adjust that here.

/////////////////////////////////////////////////////////////////////////////
// local variables
/////////////////////////////////////////////////////////////////////////////

const u8 virtual_knob_CCs[2] = {0x07, 0x01};            // We have 2 virtual knobs, VOLUME and MODULATION. This could be expanded in future.
u8 virtual_knob_pos[2];                                 // MIDI CC's for each virtual knob.
u8 current_virtual_knob = 0;                            // Index of the currently selected virtual knob.
s16 led_hue_override = -1;                              // The LED ring's colour can be overridden by certain functions, e.g. monitor mute.

void DC_KNOB_Init()
{
    // Init all pins of J5A, J5B and J5C as inputs with internal Pull-Up.
    int pin;
    for (pin = 0; pin < 12; ++pin)
    {
        MIOS32_BOARD_J5_PinInit(pin, MIOS32_BOARD_PIN_MODE_INPUT_PU);
    }

    // Init rotary encoders of the same type
    int enc;
    for (enc = 0; enc < ENCODER_COUNT; ++enc)
    {
        mios32_enc_config_t enc_config = MIOS32_ENC_ConfigGet(enc);
        enc_config.cfg.type = NON_DETENTED; // see mios32_enc.h for available types
        enc_config.cfg.sr = 0;              
        enc_config.cfg.pos = 0;             
        enc_config.cfg.speed = FAST;
        enc_config.cfg.speed_par = 1;

        MIOS32_ENC_ConfigSet(enc, enc_config);

        // Reset virtual positions
        virtual_knob_pos[enc] = 0;
    }

    DC_KNOB_UpdateLedRing();   
}

void DC_KNOB_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{
    // MIDI received, update our virtual knob value if necessary.
    int i;
    for (i = 0; i < sizeof(virtual_knob_CCs); i++) 
    {
        if (port == DAW_USB_PORT && midi_package.chn == Chn1 && midi_package.type == CC && midi_package.cc_number == virtual_knob_CCs[i])
        {
            int max = virtual_knob_CCs[current_virtual_knob] == 7 ? ENCODER_VOLUME_RANGE : ENCODER_CC_RANGE;
            int value = midi_package.value - (128 - max);

            if (value < 0)
                value = 0;
            else if (value >= max)
                value = max - 1;

            virtual_knob_pos[i] = value;

            if (current_virtual_knob == i) DC_KNOB_UpdateLedRing();
        }
    }
}

void DC_KNOB_NotifyChange(u32 encoder, s32 incrementer)
{
    // Knob moved - increment to virtual position and ensure that the value is in correct range
    bool is_volume = virtual_knob_CCs[current_virtual_knob] == 7;
    int max = is_volume ? ENCODER_VOLUME_RANGE : ENCODER_CC_RANGE;
    int value = virtual_knob_pos[current_virtual_knob] + incrementer;

    if (value < 0)
        value = 0;
    else if (value >= max)
        value = max - 1;

    // Only send if value has changed.
    if (virtual_knob_pos[current_virtual_knob] != value)
    {
        // Store new value and send MIDI.
        virtual_knob_pos[current_virtual_knob] = value;
        MIOS32_MIDI_SendCC(DAW_USB_PORT, Chn1, virtual_knob_CCs[current_virtual_knob], (value == 0) ? 0 : value + (128 - max));
      
        // Display dB value on LCD.
        if (is_volume) value = (int)((float)(-max + value) / 2.0f);
        char label[11];
        sprintf(label, "MIDI CC %d", virtual_knob_CCs[current_virtual_knob]);

        DC_LCD_PopupBigValue(value, is_volume ? "MONITOR LEVEL    dBFS" : label, 1);
    }

    // update LED ring
    DC_KNOB_UpdateLedRing();
}


void DC_KNOB_UpdateLedRing()
{
    // Update LED ring. We display a nice colour spectrum from blue through green to red, unless the
    // colour is overridden. Also, values 'between' individual LEDs are shown by varying brightness.
    int value = virtual_knob_pos[current_virtual_knob];
    float values_per_led = (virtual_knob_CCs[current_virtual_knob] == 7 ? ENCODER_VOLUME_RANGE : ENCODER_CC_RANGE) / LED_RING_SIZE;
    int led;
    float h, v;

    for (led = 0; led < LED_RING_SIZE; led++)
    {
        h = (led_hue_override > -1) ? led_hue_override : 240.0 - round((((float)led + 1.0) / 16.0) * 240.0);

        if (value >= (led + 1) * values_per_led)
        {
            v = LED_BRIGHTNESS + LED_RING_BRIGHTNESS_OFFSET;
        }
        else if (value > led * values_per_led)
        {
            v = ((float)(value % (int)values_per_led) / values_per_led) * (LED_BRIGHTNESS + LED_RING_BRIGHTNESS_OFFSET);
        }
        else
        {
            v = 0.0;

            if (led == 0 && value == 0)
                v = 0.007f;             // If value is 0, leave one LED on very dimly.
        }

        if (led_hue_override > -1 && v > 0.007f)
            v = v / 2.0f;

        int led_index = LED_RING_START_INDEX + LED_RING_OFFSET + LED_RING_SIZE - 1 - led;
        if (led_index < LED_RING_START_INDEX)
            led_index += LED_RING_SIZE;

        WS2812_LED_SetHSV(led_index, h, current_virtual_knob == 0 ? 1.0 : 0.0, v);      
    }
}

void DC_KNOB_SetCurrentKnob(u8 knob_index)
{
    if (knob_index < sizeof(virtual_knob_pos))
    {
        current_virtual_knob = knob_index;
        DC_KNOB_UpdateLedRing();
    }
}

s8 DC_KNOB_GetKnobValue(u8 knob_index)
{
    if (knob_index < sizeof(virtual_knob_pos))
    {    
        bool is_volume = virtual_knob_CCs[current_virtual_knob] == 7;
        int max = is_volume ? ENCODER_VOLUME_RANGE : ENCODER_CC_RANGE;
        int value = virtual_knob_pos[current_virtual_knob];
        if (is_volume) value = (int)((float)(-max + value) / 2.0f);

        return value;
    }

    return 0;
}

void DC_KNOB_SetRingColour(s16 hue)
{
    led_hue_override = hue;
    DC_KNOB_UpdateLedRing();
}

void DC_KNOB_Test()
{
    // Test mode - make all meter LEDs white.
    int i;
    for (i = LED_RING_START_INDEX; i < LED_RING_START_INDEX + 2; i++)
        WS2812_LED_SetHSV(i, 0, 0, LED_BRIGHTNESS);
}
