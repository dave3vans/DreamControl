/*
 * LCD functions for DreamControl
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
#include <glcd_font.h>
#include <stdbool.h>

#include "app.h"
#include "dc_fonts.h"
#include "dc_lcd.h"

/////////////////////////////////////////////////////////////////////////////
// local variables
/////////////////////////////////////////////////////////////////////////////

#define POPUP_TIMEOUT_MS 2000            // Time in milliseconds that the 'popup' mode text is displayed.
#define INIT_TIME_MS 1500                // Time in milliseconds that the LCD reset pin is held low.

static unsigned int popup_timeout_value = -1;
static bool init_complete = false;
static bool splash_complete = false;
static unsigned int init_timer_value = 0;

void DC_LCD_Init()
{
    // LCD reset pin is connected to J10A pin 8.
    MIOS32_BOARD_J10_PinInit(7, MIOS32_BOARD_PIN_MODE_OUTPUT_OD);
    MIOS32_BOARD_J10_PinSet(7, 0);

    MIOS32_TIMER_Init(0, 1000, DC_LCD_Init_Timer, MIOS32_IRQ_PRIO_MID);
}

static void DC_LCD_Init_Timer(void)
{
    if (!init_complete)
    {
        if (init_timer_value >= INIT_TIME_MS)
        {
            // Set LCD reset pin high to complete reset cycle.
            init_complete = true;
            MIOS32_BOARD_J10_PinSet(7, 1);
            MIOS32_LCD_Init(0);
            MIOS32_MIDI_SendDebugString("Init display complete.");

            MIOS32_BOARD_LED_Set(0x0002, 0x0002);
        }
        else
        {
            init_timer_value += 1;
        }
    }
    else if (!splash_complete)
    {
        if (init_timer_value >= INIT_TIME_MS * 2)
        {
            // After another INIT_TIME_MS period, display the splash screen.
            splash_complete = true;
            MIOS32_LCD_Clear();
            DC_LCD_Print(0, 0, 4, " Dream");
            DC_LCD_Print(4, 0, 4, "Control");
        }
        else
        {
            init_timer_value += 1;
        }      
    }
}

void DC_LCD_Tick()
{
    // Popup timer
    if (popup_timeout_value > 0)
        popup_timeout_value += 1;

    if (popup_timeout_value >= POPUP_TIMEOUT_MS)
    {
        popup_timeout_value = -1;
        MIOS32_LCD_Clear();
    }
}

// Pop up a message on the LCD for a few seconds.
void DC_LCD_PopupBigValue(const int value, const char *value_label, const char is_negative)
{
    if (popup_timeout_value <= 0)
        MIOS32_LCD_Clear();

    popup_timeout_value = 1;

    MIOS32_LCD_FontInit((u8 *)GLCD_FONT_MEGA);
    MIOS32_LCD_GCursorSet(0, 0);
    MIOS32_LCD_PrintFormattedString("%d  ", value);

    MIOS32_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);

    MIOS32_LCD_GCursorSet(0, 6 * 8);
    MIOS32_LCD_PrintFormattedString("                     ");

    MIOS32_LCD_GCursorSet(0, 7 * 8);
    MIOS32_LCD_PrintString(value_label);
}

void DC_LCD_Clear()
{
    if (popup_timeout_value <= 0)
        MIOS32_LCD_Clear();
}

void DC_LCD_Print(const int row, const int X, const int font_size, const char *text)
{
    if (popup_timeout_value <= 0)
        MIOS32_LCD_FontInit(font_size == 0
                                ? (u8 *)GLCD_FONT_TINY
                                : font_size == 1
                                      ? (u8 *)GLCD_FONT_SMALL
                                      : font_size == 2
                                            ? (u8 *)GLCD_FONT_NORMAL
                                            : font_size == 3
                                                  ? (u8 *)GLCD_FONT_BIG
                                                  : font_size == 4
                                                        ? (u8 *)GLCD_FONT_HUGE
                                                        : font_size == 5
                                                              ? (u8 *)GLCD_FONT_MEGA
                                                              : (u8 *)GLCD_FONT_NORMAL);

    if (popup_timeout_value <= 0)
        MIOS32_LCD_GCursorSet(X, row * 8);
    if (popup_timeout_value <= 0)
        MIOS32_LCD_PrintString(text);
}
