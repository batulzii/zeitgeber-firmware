/*
 * File:   oled/ssd1351.c
 * Author: Jared
 *
 * Created on 9 July 2013, 12:51 PM
 */

////////// Includes ////////////////////////////////////////////////////////////

#include <system.h>
#include "hardware.h"
#include "ssd1351.h"
#include "peripherals/ssd1351p.h"

////////// Defines /////////////////////////////////////////////////////////////

#define CMD_SET_COLUMN_ADDR         0x15
#define CMD_SET_ROW_ADDR            0x75

#define CMD_WRITE_RAM               0x5C
#define CMD_READ_RAM                0x5D

#define CMD_COLORDEPTH              0xA0    // Numerous functions include increment direction ... see DS
                                            // A0[0] = Address Increment Mode (0 = horizontal, 1 = vertical)
                                            // A0[1] = Column Address Remap (0 = left to right, 1 = right to left)
                                            // A0[2] = Color Remap (0 = ABC, 1 = CBA) - HW RGB/BGR switch
                                            // A0[4] = COM Scan Direction (0 = top to bottom, 1 = bottom to top)
                                            // A0[5] = Odd/Even Paid Split
                                            // A0[7:6] = Display Color Mode (00 = 8-bit, 01 = 65K, 10/11 = 262K, 8/16-bit interface only)

#define CMD_SET_DISPLAY_START_LINE  0xA1
#define CMD_SET_DISPLAY_OFFSET      0xA2    // (locked)
#define CMD_SET_DISPLAY_MODE        0xA4
    #define DISPLAY_MODE_ALLOFF         0   // Force entire display area to grayscale GS0
    #define DISPLAY_MODE_ALLON          1   // Force entire display area to grayscale GS63
    #define DISPLAY_MODE_RESET          2   // Resets the display area relative to the above two commands
    #define DISPLAY_MODE_INVERT         3   // Inverts the display contents (GS0 -> GS63, GS63 -> GS0, etc.)

#define CMD_FUNCTION_SELECTION      0xAB    // Enable/Disable the internal VDD regulator

#define CMD_DISPLAY_OFF             0xAE    // Sleep mode on (display off)
#define CMD_DISPLAY_ON              0xAF    // Sleep mode off (display on)

#define CMD_SET_PHASE_LENGTH        0xB1    // (locked) Larger capacitance may require larger delay to discharge previous pixel state
#define CMD_ENHANCE_DRIVING_SCHEME  0xB2    // Not documented in SSD1351 datasheet.
#define CMD_FRONT_CLOCK_DIV         0xB3    // (locked) DCLK divide ration fro CLK (from 1 to 16)
#define CMD_SET_VSL                 0xB4
#define CMD_SET_GPIO                0xB5
#define CMD_SET_SECOND_PRECHARGE    0xB6    // Second pre-charge period

#define CMD_GRAYSCALE_LUT           0xB8    // 63 data bytes
#define CMD_SET_LUT_LINEAR          0xB9    // Use internal linear lookup table for above

#define CMD_SET_PRECHARGE           0xBB    // Pre-charge voltage

#define CMD_SET_VCOMH               0xBE
#define CMD_SET_CONTRAST            0xC1    // Set R,G,B contrast (3 bytes, 0-255)
#define CMD_MASTER_CONTRAST         0xC7    // Master contrast (0 to 15)
#define CMD_SET_MUX_RATIO           0xCA
#define CMD_SET_COMMAND_LOCK        0xFD    // 

#define CMD_HORIZONTAL_SCROLL       0x96    // 5 bytes, see datasheet for values
#define CMD_STOP_MOVING             0x9E    // Stop horizontal scroll
#define CMD_START_MOVING            0x9F    // Start horizontal scroll

const uint8 gamma_lut[] = {
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
    0x12, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F,
    0x21, 0x23, 0x25, 0x27, 0x2A, 0x2D, 0x30, 0x33,
    0x36, 0x39, 0x3C, 0x3F, 0x42, 0x45, 0x48, 0x4C,
    0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C,
    0x70, 0x74, 0x78, 0x7D, 0x82, 0x87, 0x8C, 0x91,
    0x96, 0x9B, 0xA0, 0xA5, 0xAA, 0xAF, 0xB4
};

////////// Methods /////////////////////////////////////////////////////////////

bool ssd1351_Test() {
    // Check that we can communicate with the display

    _LAT(OL_RESET) = 0;

    //Delay?

    // Master contrast is 0b1111 on reset
    ssd1351_command(CMD_MASTER_CONTRAST);
    uint8 value = ssd1351_read();

    _LAT(OL_RESET) = 1;

    return value ? true : false;
}

void ssd1351_PowerOn() {
    // Power on initialization sequence

    //NOTE: ssd1351_command(x) is equivalent to ssd1351_sendv(x, 0)

    _LAT(OL_RESET) = 0;

    // Unlock locked commands
    ssd1351_sendv(CMD_SET_COMMAND_LOCK,         1, 0x12);
    ssd1351_sendv(CMD_SET_COMMAND_LOCK,         1, 0xB1);
    
    // Turn off display
    ssd1351_command(CMD_DISPLAY_OFF);
    
    // Configure display
    ssd1351_sendv(CMD_FRONT_CLOCK_DIV,          1, 0xF1);
    ssd1351_sendv(CMD_SET_MUX_RATIO,            1, 0x7F);   // Display row configuration (interlaced)
    ssd1351_sendv(CMD_SET_DISPLAY_OFFSET,       1, 0x00);
    ssd1351_sendv(CMD_SET_DISPLAY_START_LINE,   1, 0x00);
    ssd1351_sendv(CMD_COLORDEPTH,               1, 0xB4); //0x74
    ssd1351_sendv(CMD_SET_GPIO,                 1, 0x00);                   // Disable GPIO
    ssd1351_sendv(CMD_FUNCTION_SELECTION,       1, 0x01);
    ssd1351_sendv(CMD_SET_VSL,                  3, 0xA0, 0xB5, 0x55);       // External VSL

    // Contrast/gamma display settings
    ssd1351_sendv(CMD_SET_CONTRAST,             3, 0xC8, 0x80, 0xC8);   // R,G,B contrast values
    ssd1351_sendv(CMD_MASTER_CONTRAST,          1, 0x0F);            // Full master contrast
    ssd1351_sendbuf(CMD_GRAYSCALE_LUT,          (uint8*)gamma_lut, sizeof(gamma_lut));

    ssd1351_sendv(CMD_SET_PHASE_LENGTH,         1, 0x32);
    ssd1351_sendv(CMD_ENHANCE_DRIVING_SCHEME,   3, 0xA4, 0x00, 0x00);
    
    ssd1351_sendv(CMD_SET_PRECHARGE,            1, 0x17);
    ssd1351_sendv(CMD_SET_SECOND_PRECHARGE,     1, 0x01);
    ssd1351_sendv(CMD_SET_VCOMH,                1, 0x05);

    ssd1351_command(CMD_SET_DISPLAY_MODE | DISPLAY_MODE_RESET);
    
    // Clear Screen
    ssd1351_ClearScreen();

    // Turn on VCC
    _LAT(OL_POWER) = 1;
    
    ssd1351_DisplayOn();
}

void ssd1351_DisplayOn() {
    ssd1351_command(CMD_DISPLAY_ON);
}

void ssd1351_PowerOff() {
    // Power off sequence
    ssd1351_DisplayOff();

    _LAT(OL_RESET) = 1;
    _LAT(OL_POWER) = 0;
}

void ssd1351_DisplayOff() {
    ssd1351_command(CMD_DISPLAY_OFF);
}

void ssd1351_SetContrast(uint8 contrast) {
    if (contrast > 0x0F) contrast = 0x0F;
    ssd1351_sendv(CMD_MASTER_CONTRAST, 1, contrast);
}

void ssd1351_ClearScreen() {
    //TODO: How do we clear the screen?
}

//TODO: How to write pixels to the screen?
// Can we implement efficient modes of just writing a windowed region of the display?
// Can we do a Read-Modify-Write on existing pixels?