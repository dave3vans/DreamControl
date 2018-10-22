/*
 * Metering functions for DreamControl
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "dc_meters.h"
#include "dc_lcd.h"
#include "dc_knob.h"

/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#define LED_METER_SIZE 15               // The number of LEDs in each of our meters.
#define LED_METER_START_INDEX 0         // The start index of the first LED of the first meter.
#define LED_METER_COUNT 3               // The number of meters we have.

// Arrays of hue (colour) values and dB values for each LED for the different meter types. Values start at bottom of meter.
// Because of the way things work, we need LED_METER_SIZE+1 elements in these arrays.

const int peak_3db_meter_hue[15] = {248, 247, 246, 245, 244, 243, 242, 229, 228, 227, 226, 47, 46, 34, 0};
const int peak_3db_meter_vals[16] = {-60, -48, -42, -36, -30, -27, -24, -21, -18, -15, -12, -9, -6, -3, 0, 3};

const int peak_1db_meter_hue[15] = {198, 197, 196, 195, 194, 193, 192, 47, 46, 45, 44, 34, 33, 32, 0};
const int peak_1db_meter_vals[16] = {-60, -48, -36, -24, -20, -16, -12, -10, -8, -6, -4, -3, -2, -1, 0, 1};

const int lufs_abs_meter_hue[15] = {112, 111, 110, 109, 108, 89, 88, 87, 86, 67, 65, 64, 63, 42, 41};
const int lufs_abs_meter_vals[16] = {-35, -31, -27, -25, -23, -21, -19, -17, -15, -13, -11, -9, -7, -5, -3, -1};

const int lufs_rel_meter_hue[15] = {130, 130, 130, 130, 130, 130, 130, 130, 130, 114, 30, 0, 0, 0, 0};
const int lufs_rel_meter_vals[16] = {-18, -16, -14, -12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12};

/////////////////////////////////////////////////////////////////////////////
// local variables
/////////////////////////////////////////////////////////////////////////////

bool is_lufs_mode = false;                      // true = LUFS mode, false = True Peak mode
bool is_peak_with_momentary = false;            // true = True Peak mode also shows LUFS Momentary, false = LUFS Short
bool is_1db_scale = false;                      // true = True Peak scale is 1dB, false = 3dB
bool is_lufs_relative = false;                  // true = LUFS meter is relative mode, false = absolute mode
float current_lufs_target = -16.0f;             // Our current LUFS target value (set in plugin).

void DC_METERS_Init()
{
}

void DC_METERS_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{
}

void DC_METERS_Update(char *meter_data)
{
    // We are passed a pointer to an array of meter values, which were received as MIDI sysex data.
    // Each value is split into integral and fractional, i.e. 0=integral, 1=fractional, except CLIP_L and CLIP_R which are boolean.
    // The integral is always a negative number. True peak values have 3dB subtracted, as they can go up to +3dB.
    // This makes the total range -99.99 to 0.00.

    current_lufs_target = -((float)meter_data[LUFS_TARGET] + (float)(meter_data[LUFS_TARGET + 1] / 100.0f));
    float max_peak = -((float)meter_data[MAX_TOTAL] + ((float)(meter_data[MAX_TOTAL + 1] / 100.0f)));
    max_peak = max_peak + 3.0f;

    ///////////////   LCD
    // Our display layout shows:
    // Top line (small)    - Max peak left, max peak right, current monitor level (or 'CLIP!')
    // Middle line (large) - LUFS Integrated
    // Bottom line (small) - LUFS range, LUFS target, LUFS Short

    char max_l[6];
    DC_METERS_GetLevelStringFromMeterData(max_l, meter_data, MAX_L, true);
    char max_r[6];
    DC_METERS_GetLevelStringFromMeterData(max_r, meter_data, MAX_R, true);
    char level[6];
    sprintf(level, " %d", DC_KNOB_GetKnobValue(0));

    char top_line[19];
    sprintf(top_line, "%s %s %s ", max_l, max_r, meter_data[CLIP_L] || meter_data[CLIP_R] ? " CLIP" : level);

    char lufs_i[6];
    DC_METERS_GetLevelStringFromMeterData(lufs_i, meter_data, LUFS_I, false);

    char lufs_range[6];
    DC_METERS_GetLevelStringFromMeterData(lufs_range, meter_data, LUFS_RANGE, false);
    char lufs_target[6];
    DC_METERS_GetLevelStringFromMeterData(lufs_target, meter_data, LUFS_TARGET, false);
    char lufs_s[6];
    DC_METERS_GetLevelStringFromMeterData(lufs_s, meter_data, LUFS_S, false);

    char bottom_line[19];
    sprintf(bottom_line, "%s %s %s ", lufs_range, lufs_target, lufs_s);

    DC_LCD_Print(0, 0, 2, top_line);
    DC_LCD_Print(0, 108, 0, " dBTP");
    DC_LCD_Print(1, 0, 5, lufs_i);
    DC_LCD_Print(7, 0, 2, bottom_line);
    DC_LCD_Print(7, 108, 0, " LUFS");

    ////////////////  LED METERS
    // in LUFS mode:
    // LUFS Momentary, LUFS Short, LUFS Integrated
    // in True Peak mode:
    // True Peak Left, True Peak Right, LUFS momentary or LUFS Integrated

    if (is_lufs_mode)
    {
        DC_METERS_UpdateLedMeter(0, meter_data, LUFS_M, false);
        DC_METERS_UpdateLedMeter(1, meter_data, LUFS_S, false);
        DC_METERS_UpdateLedMeter(2, meter_data, LUFS_I, false);
    }
    else
    {
        DC_METERS_UpdateLedMeter(0, meter_data, PEAK_L, true);
        DC_METERS_UpdateLedMeter(1, meter_data, PEAK_R, true);
        DC_METERS_UpdateLedMeter(2, meter_data, is_peak_with_momentary ? LUFS_M : LUFS_I, false);
    }
}

void DC_METERS_GetLevelStringFromMeterData(char *output_string, char *meter_data, dc_meter_data_index_t index, bool is_peak_meter)
{
    int integral = -meter_data[index];
    int fractional = meter_data[index + 1];

    if (integral == -99 && fractional == 0)
    {
        sprintf(output_string, "     ");
        return;
    }

    // Add 3dB for true peak values, as they are supplied to us with 3dB subtracted so we don't have to worry about +/-.
    // TODO: This is a fairly naff way of doing this and could be improved.
    if (is_peak_meter)
        integral = integral + 3;
    if (is_peak_meter && integral > 0 && fractional > 0)
    {
        integral = integral - 1;
        fractional = 99 - fractional;
    }

    sprintf(output_string, "%s%d.%01d", (integral <= -10 ? "" : integral >= 0 ? " +" : " "), integral, fractional);
    if (strlen(output_string) >= 6)
        output_string[strlen(output_string) - 1] = 0;
}

void DC_METERS_UpdateLedMeter(char meter_index, char *meter_data, dc_meter_data_index_t data_index, bool is_peak_meter)
{
    int led, values_per_led;
    int *led_val_list;
    int *led_hue_list;
    float h, v;

    float value = -((float)meter_data[data_index] + (float)(meter_data[data_index + 1] / 100.0f));

    float max_l = -((float)meter_data[MAX_L] + (float)(meter_data[MAX_L + 1] / 100.0f)) + 3.0f;
    float max_r = -((float)meter_data[MAX_R] + (float)(meter_data[MAX_R + 1] / 100.0f)) + 3.0f;

    if (!is_peak_meter && is_lufs_relative)
        value = value - current_lufs_target;

    //MIOS32_MIDI_SendDebugMessage("METER %d = %d.%03d", meter_index, (int)value, abs((int)(1000*value)%1000));

    if (is_peak_meter)
        value = value + 3.0f;

    if (is_peak_meter && !is_1db_scale)
    {
        led_hue_list = peak_3db_meter_hue;
        led_val_list = peak_3db_meter_vals;
    }
    else if (is_peak_meter && is_1db_scale)
    {
        led_hue_list = peak_1db_meter_hue;
        led_val_list = peak_1db_meter_vals;
    }
    else if (!is_peak_meter && !is_lufs_relative)
    {
        led_hue_list = lufs_abs_meter_hue;
        led_val_list = lufs_abs_meter_vals;
    }
    else if (!is_peak_meter && is_lufs_relative)
    {
        led_hue_list = lufs_rel_meter_hue;
        led_val_list = lufs_rel_meter_vals;
    }

    // We calculate LED values from the bottom of the meter (0 is bottom LED)
    for (led = 0; led < LED_METER_SIZE; led++)
    {
        //if ((data_index == PEAK_L && max_l > 0.0f) || (data_index == PEAK_R && max_r > 0.0f))
        //    h = 0.0f; // If max above 0, make entire meter red.
        //else
            h = (float)led_hue_list[led];

        if (value >= led_val_list[led + 1])
            v = LED_BRIGHTNESS;
        else if (value > led_val_list[led])
            v = (1.0f - ((float)(led_val_list[led + 1] - value) / (float)(led_val_list[led + 1] - led_val_list[led]))) * LED_BRIGHTNESS;
        else
            v = 0.0;

        if (!is_peak_meter && is_lufs_relative && led < 8)
            v = v / 2.75f; // Lower area of relative meter darker.

        WS2812_LED_SetHSV(LED_METER_START_INDEX + (LED_METER_SIZE * meter_index) + LED_METER_SIZE - 1 - led, h, 1.0, v);
    }

    if (is_peak_meter)
    {
        for (led = 0; led < LED_METER_SIZE; led++)
        {
            float max = data_index == PEAK_L ? max_l : data_index == PEAK_R ? max_r : max_l;

            if (led > 0 && max > led_val_list[led - 1] && max <= led_val_list[led])
                WS2812_LED_SetHSV(LED_METER_START_INDEX + (LED_METER_SIZE * meter_index) + LED_METER_SIZE - 1 - led, 0, 1.0, LED_BRIGHTNESS / 2.0f);
        }
    }
}

void DC_METERS_SetMeterMode(bool isLufs)
{
    is_lufs_mode = isLufs;
}

void DC_METERS_SetPeak3rdMeterMode(bool isMomentary)
{
    is_peak_with_momentary = isMomentary;
}

void DC_METERS_SetRelativeMode(bool isRelative)
{
    is_lufs_relative = isRelative;
}

void DC_METERS_Set1dBScaleMode(bool isActive)
{
    is_1db_scale = isActive;
}

void DC_METERS_Test()
{
    // Test mode - make all meter LEDs white.
    int i;
    for (i = LED_METER_START_INDEX; i < (LED_METER_SIZE * LED_METER_COUNT); i++)
        WS2812_LED_SetHSV(i, 0, 0, LED_BRIGHTNESS);
}