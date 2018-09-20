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

#ifndef _DC_METERS_H
#define _DC_METERS_H

#include <stdbool.h>

/////////////////////////////////////////////////////////////////////////////
// global definitions
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////

typedef enum {
    LUFS_S = 0,
    LUFS_M = 2,
    LUFS_I = 4,
    LUFS_MIN = 6,
    LUFS_MAX = 8,
    LUFS_RANGE = 10,
    LUFS_TARGET = 12,
    PEAK_L = 14,
    PEAK_R = 16,
    MAX_L = 18,
    MAX_R = 20,
    MAX_TOTAL = 22,
    CLIP_L = 24,
    CLIP_R = 26
} dc_meter_data_index_t;

/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern void DC_METERS_Init();
extern void DC_METERS_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package);
extern void DC_METERS_Update(char* meter_data);
extern void DC_METERS_SetMeterMode(bool isLufs);
extern void DC_METERS_SetPeak3rdMeterMode(bool isMomentary);
extern void DC_METERS_SetRelativeMode(bool isRelative);
extern void DC_METERS_Set1dBScaleMode(bool isActive);

void DC_METERS_GetLevelStringFromMeterData(char* output_string, char *meter_data, dc_meter_data_index_t index, bool is_true_peak);
void DC_METERS_UpdateLedMeter(char meter_index, char *meter_data, dc_meter_data_index_t index, bool is_true_peak);

/////////////////////////////////////////////////////////////////////////////
// Exported variables
/////////////////////////////////////////////////////////////////////////////

#endif
