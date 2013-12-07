/* 
 * File:   main.c
 * Author: Jared
 *
 * Created on 5 July 2013, 12:48 PM
 *
 * Zeitgebers are events that keep our circadian rhythms regulated.
 * An alarm clock is an example of an artificial zeitgeber
 *
 * Code is organised into the following layer model:
 * Hardware Layer       Hardware specific definitions (hardware.h)
 * Peripheral Layer     PIC peripherals; connects the driver layer to the hardware layer (peripherals/...)
 * Driver Layer         Drivers; provides ways of communicating with attached sensors/devices (drivers/...)
 * API Layer            API; to provide an easy to use interface of the drivers to the user-mode application (api/...)
 * Application Layer    User-mode applications (applications/...)
 *
 * In addition there is also the core/... directory, which contains
 * the system kernel and other system related code.
 * 
 * And finally, the main.c initializes all the peripherals and apis
 * (drivers are initialized through the appropriate api)
 * and is also in charge of running the kernel.
 *
 * Peripherals/drivers may define their own interrupts.
 *
 * User-mode applications should only need access to the API code, nothing else.
 *
 * ERRATA (xxxDAxxx) http://ww1.microchip.com/downloads/en/DeviceDoc/80000505g.pdf
 */

#include <stdlib.h>
#include <system.h>
#include <Rtcc.h>
#include "hardware.h"

// Core
#include "core/systick.h"
#include "core/scheduler.h"
#include "core/os.h"

// Peripherals
#include "peripherals/adc.h"
#include "peripherals/pwm.h"
#include "peripherals/gpio.h"
#include "peripherals/rtc.h"

// API
#include "api/power_monitor.h"
#include "api/oled.h"

// User-mode applications
#include "api/app.h"
#include "applications/main/appmain.h"
#include "drivers/ssd1351.h"

#include "api/graphics/gfx.h"
#include "util/util.h"

//#include "gui/icons/bat.h"

//#include "tools/DSC09748.h"
//const image_t img = {DSC09748_bytes, DSC09748_WIDTH, DSC09748_HEIGHT};

//#include "tools/fluffy.h"
//const image_t img = {fluffy_bytes, FLUFFY_WIDTH,  FLUFFY_HEIGHT};

#include "gui/Wallpapers/wallpaper.h"
#define wallpaper img_wallpaper

#include <gui/statusbar.h>

#include <gui/icons/battery_50.h>

#include <gui/icons/bat.h>


//#include "tools/wolf.h"
//const image_t img = {wolf_bytes, WOLF_WIDTH,  WOLF_HEIGHT};

#define RCON_RESET (_RCON_POR_MASK | _RCON_BOR_MASK | _RCON_WDTO_MASK | _RCON_SWR_MASK | _RCON_EXTR_MASK | _RCON_CM_MASK | _RCON_IOPUWR_MASK | _RCON_TRAPR_MASK)
#define UNEXPECTED_RESET (_RCON_POR_MASK| _RCON_BOR_MASK | _RCON_WDTO_MASK | _RCON_CM_MASK | _RCON_IOPUWR_MASK | _RCON_TRAPR_MASK)


// 0x0000, 0xFBFF, 0xFFFF, 0xF2DF
// 0xFFFF, 0xFBFF, 0xFFFF, 0xF2DF

// 0xFFFF, 0xFFFF, 0xFFF3, 0x7BFF

#ifndef HID_BOOTLOADER
    _CONFIG1(FWDTEN_OFF & ICS_PGx2 & GWRP_OFF & GCP_OFF & JTAGEN_OFF)
    _CONFIG2(POSCMOD_HS & IOL1WAY_ON & OSCIOFNC_OFF & FCKSM_CSDCMD & FNOSC_PRIPLL & PLL96MHZ_ON & PLLDIV_DIV8 & IESO_OFF) // For 32MHz OSC
    _CONFIG3(0xFFFF);
#endif

void InitializeIO() {
    // Initialize all the IO pins immediately into a valid state

    /// Analog ///
//    _ANS(AN_VBAT) = ANALOG;
//    _ANS(AN_LIGHT) = ANALOG;
    _TRIS(ANR_VBAT) = INPUT;
    _TRIS(ANR_LIGHT) = INPUT;

    /// OLED ///
    _TRIS(OL_E) = OUTPUT;
    _TRIS(OL_RW) = OUTPUT;
    _TRIS(OL_DC) = OUTPUT;
    _TRIS(OL_CS) = OUTPUT;
    _TRIS(OL_RESET) = OUTPUT;
    _TRIS(OL_POWER) = OUTPUT;
    OL_DATA_TRIS &= ~OL_DATA_MASK; // D0..D7 output
    _LAT(OL_POWER) = 0;          // OLED supply off
    _LAT(OL_RESET) = 1;          // Disable OLED
    
    /// Buttons ///
    _TRIS(BTN1) = INPUT;
    _TRIS(BTN2) = INPUT;
    _TRIS(BTN3) = INPUT;
    _TRIS(BTN4) = INPUT;

    /// Status LEDs ///
    _TRIS(LED1) = OUTPUT;
    _TRIS(LED2) = OUTPUT;
    _LAT(LED1) = 0;
    _LAT(LED2) = 0;

    /// Misc GPIO ///
    _TRIS(VMOTOR) = OUTPUT;
    _TRIS(PEIZO) = OUTPUT;
//    _TRIS(VBUS_SENSE) = INPUT;
    _LAT(VMOTOR) = 0;
    _LAT(PEIZO) = 0;

    /// Sensors ///
    _TRIS(SDA) = INPUT; // Bi-directional
    _TRIS(SCL) = OUTPUT;
    _TRIS(INTM) = INPUT;
    _TRIS(INTA) = INPUT;

    /// Power Supply ///
    _TRIS(PW_STAT1) = INPUT;
    _TRIS(PW_STAT2) = INPUT;
    _TRIS(PW_CE) = OUTPUT;
    _LAT(PW_CE) = 0;        // Enable charging

    /// Bluetooth ///
    _TRIS(BT_MISO) = INPUT;
    _TRIS(BT_MOSI) = OUTPUT;
    _TRIS(BT_REQN) = OUTPUT;
    _TRIS(BT_SCK) = OUTPUT;
    _TRIS(BT_RDYN) = INPUT; 
    _TRIS(BT_RESET) = OUTPUT;
    _LAT(BT_RESET) = 1;     // Keep BT in reset

    /// Pin-Change Interrupts ///
    _CNIEn(INTM_CN) = 1;
    _CNIEn(INTA_CN) = 1;

    /// Pin Pull Ups ///
    /*_CNPUE(BTN1_CN) = 1;
    _CNPUE(BTN2_CN) = 1;
    _CNPUE(BTN3_CN) = 1;
    _CNPUE(BTN4_CN) = 1;*/
    _CNPUE(SDA_CN) = 1;
    _CNPUE(SCL_CN) = 1;

    /// USB ///
    _TRIS(USB_DPLUS) = INPUT;
    _TRIS(USB_DMINUS) = INPUT;
    _TRIS(USB_VBUS) = INPUT;
    //_CNPUE(USB_DPLUS_CN) = 1; // Required when USB is enabled
    _CNPUE(USB_DPLUS_CN) = 0;

    /// Peripheral Pin Select ///
    // BT_MISO : SDI
    // BT_MOSI : SDO
    // BT_SCK : SCK
    // Chip select?

    /// Disable Unused Peripherals ///
    // Setting the PMD bit on a peripheral will cut the clock source to it.
    // You will not be able to write to any of its registers.

    // Disable ALL peripherals, let the system modules explicitly enable them if needed
    PMD1 = 0xFFFF;
    PMD2 = 0xFFFF;
    PMD3 = 0xFFFF;
    PMD4 = 0xFFFF;
    PMD5 = 0xFFFF;
    PMD6 = 0xFFFF;
}

BOOL displayOn = TRUE;

void WatchSleep() {
    ssd1351_DisplayOff();
    _LAT(BT_RESET) = 1;     // Turn off Bluetooth
    _LAT(VMOTOR) = 0;
    _LAT(PEIZO) = 0;
    _LAT(LED1) = 0;
    _LAT(LED2) = 0;
    _LAT(OL_POWER) = 0;     // Turn off OLED supply
    _LAT(OL_RESET) = 1;  

    PMD1 = 0xFFFF;
    PMD2 = 0xFFFF;
    PMD3 = 0xFFFF;
    PMD4 = 0xFFFF;
    PMD5 = 0xFFFF;
    PMD6 = 0xFFFF;

    Sleep();
}

void CheckButtons() {
    UINT32 i;
#ifdef HID_BOOTLOADER
     // Execute bootloader if USB cable is plugged in and a button is pressed
    if (_PORT(USB_VBUS) && ( _PORT(BTN2) || _PORT(BTN3) || _PORT(BTN4))) {
        _LAT(LED1) = 0; _LAT(LED2) = 0;
        for (i=0; i<100000; i++);
        while ( _PORT(BTN2) || _PORT(BTN3) || _PORT(BTN4));
        asm("reset");
    }
#endif

    if (_PORT(BTN2)) {
        displayOn = !displayOn;
        if (displayOn)
            ssd1351_DisplayOn();
        else {
            ssd1351_DisplayOff();
            WatchSleep();
        }

        //for (i=0; i<100000; i++);
    }
}

extern const char* chgstat[];

extern const uint8 bitreverse[256];
#include "api/graphics/font.h"
#include "core/systick.h"
void Initialize() {
    InitializeIO();

    UINT32 i;
    UINT8 x = 8;
    UINT8 y = 8;
    char s[10];

    //NOTE: VBUS doesn't do what I want it to do.
    // Since it's used to charge the li-ion, the charger chip keeps voltage on VBUS for a few seconds.
    // PW_STAT1 goes LOW when the USB is connected, but it probably can't be used as the VBUS status signal.
    // Maybe a pull down resistor on VBUS would help?

    _CPDIV = 0b00; //CPU prescaler

    //Sleep();


    //for (i=0; i<100000; i++);

    _LAT(LED1) = 1;
    _LAT(LED2) = 1;

    /*while (1) {
        UINT32 i;
        for (i=0; i<100000; i++);
        _TOGGLE(LED2);

        CheckButtons();
     }*/

    //_LAT(PW_CE) = 1;

    InitializeOled();
    ssd1351_DisplayOn();

    ClearImage();

    DrawString("OLED Watch v1.0", 8, y, WHITE); y += 10;
    //DrawString("Booting...", 8, y, WHITE); y += 10;
    UpdateDisplay();

    // Check the reset status
    // Software resets are the only type of reset that should occur normally
    if (RCON & UNEXPECTED_RESET) {
        //TODO: Draw an error icon or something

        if (RCONbits.BOR)
            DrawString("RST: Brown-out", 8,y,WHITE);
        else if (RCONbits.CM)
            DrawString("RST: Conf Mismatch", 8,y,WHITE);
        else if (RCONbits.IOPUWR)
            DrawString("RST: Invalid Opcode", 8,y,WHITE);
        else if (RCONbits.EXTR)
            DrawString("RST: MCLR", 8,y,WHITE);
        else if (RCONbits.POR)
            DrawString("RST: Power-on", 8,y,WHITE);
        else if (RCONbits.WDTO)
            DrawString("RST: Watchdog Timeout", 8,y,WHITE);
        else if (RCONbits.TRAPR)
            DrawString("RST: Trap Error", 8,y,WHITE);
        //else if (RCONbits.SWR)
        //    DrawString("RST: Software", 8,y,WHITE);
        else {
            utoa(s, RCON & RCON_RESET, 16);
            x = 8;
            x = DrawString("RST: Unknown - ", x,y,WHITE);
            DrawString(s, x,y,WHITE);
        }
        y += 10;

        UpdateDisplay();
        for (i=0; i<3000000; i++);
    }
    RCON &= ~RCON_RESET;


    adc_init();
    adc_enable();

    rtc_init();

    //systick_init();

    _LAT(LED2) = 0;
    
    //
    //BitBlit(&img_statusbar, NULL, 0,0, 0,0, 0,0, ADD,0);
    
    SetFontSize(1);
    //SetFont(fonts.f5x5);
    SetFont(fonts.Stellaris);
    //DrawString("OLED Watch", 8,56, WHITE);

    //BYTE i = 0;
    UINT v = 0;
    //UINT x = 0;
    while (1) {
        if (displayOn) {
            

            ClearImage();

            global_drawop = SRCCOPY;
            SetFontSize(1);

            DrawImage(0,0,wallpaper);
            // BitBlit(&img_bat, NULL, i,40, 0,0, 0,0, ADD,1);

            //utoa(s, i, 16);
            //DrawString(s, 8,70, WHITE);
            //utoa(s, bitreverse[i], 16);
            //DrawString(s, 8,90, WHITE);

            i++;

            ProcessPowerMonitor();

           
            //DrawString(chargeStatusMessage[GetChargeStatus()], 8,8, WHITE);

            //v = adc_Read(0);
            //utoa(s, v, 10);
            //x = 8;
            //x = DrawString("VBAT: ", x,24,WHITE);
            //x = DrawString(s,        x,24, WHITE);


            //TODO: Reading two ADC samples next to each other produces invalid results
            // (eg. vbat reports 3600mV instead of 4200mV)
            //uint vbat = adc_Read(AN_VBAT) * 2;
            //uint vcore = adc_Read(AN_VCAP);
            //unsigned long vdd = 1200UL * 1024UL / (unsigned long)anbg;
            //unsigned long vcore = (unsigned long)vdd * (unsigned long)ancap / 1024;
            //unsigned long vbat = (unsigned long)vdd * (unsigned long)anbat * 2 / 1024;


            
//            utoa(s, systick, 10);
//            x = 8;
//            x = DrawString("systick: ", x,24,WHITE);
//            x = DrawString(s,        x,24, WHITE);

            utoa(s, vdd, 10);
            x = 8;
            x = DrawString("VDD: ", x,38,WHITE);
            x = DrawString(s,        x,38, WHITE);
            x = DrawString("mV", x,38,WHITE);



            utoa(s, battery_voltage, 10);
            x = 8;
            x = DrawString("VBAT: ", x,54,WHITE);
            x = DrawString(s,        x,54, WHITE);
            x = DrawString("mV", x,54,WHITE);


            
            //VBAT = 503:an = ???V
            //VCAP = 541:an = 1.8V
            //VBG = 356:an = 1.2V
            //VREF = 1024:an = ???V
            

//            global_drawop = SUBTRACT;
//
//            uint8 w = mLerp(0,100, 4,DISPLAY_WIDTH-8, battery_level);
//            DrawRoundedBox(4,4,w,10,SHADE(220),SHADE(128));
//
//            global_drawop = ADD;
//
//            utoa(s, battery_level, 10);
//            x = 6;
//            x = DrawString(s,   x,5,SILVER);
//            x = DrawString("%", x,5,SILVER);
//
//
//            SetFontSize(2);

            //RtcTimeToStr(s);
            //DrawString(s, 8,100,WHITE);

            _LAT(LED1) = 0;

            UpdateDisplay();

            _LAT(LED1) = 1;
        }

        CheckButtons();
    }

    /*while(1) {
        UINT32 i;
        for (i=0; i<100000; i++);
        //_TOGGLE(LED1);
        //_TOGGLE(LED2);

        // Display charging status
        //                  VBUS    STAT1   STAT2
        // Precharge        1       0       0
        // Fast charge      1       0       1
        // Charge done      1       1       0
        // Fault            1       1       1
        // Sleep            0       1       1
        _LAT(LED1) = !_PORT(PW_STAT1);
        _LAT(LED2) = !_PORT(PW_STAT2);

        // isCharging = (VBUS && !STAT1)

        CheckButtons();
    }*/

    // Core

    // Peripherals
    adc_init();
    pwm_init();
    gpio_init();
    rtc_init();
    
    // API
    InitializePowerMonitor(); // Battery charging/power supply monitor

    if (!InitializeOled()) {
        // Error initializing the OLED display
        // Since the display is not initialized,
        // we could revert to showing debug information through the USB port?
    }

    // Display loading message on OLED
    

    /*if (!InitializeUsb()) {
        // Error initializing USB
        // Display error message on screen
    }

    if (!InitializeSensors()) {
        // Error initializing sensors
        // Display error message on screen/usb status
    }

    if (!InitializeBluetooth()) {
        // Error initializing bluetooth
        // Display error message on screen/usb status
    }*/

    _LAT(LED1) = 0;

    //NOTE: The above initialization may not actually put the peripheral into
    // a working state, it only makes sure the device is properly configured
    // and responding. This keeps the Initialization block short.
}

int main() {
    Initialize();

    InitializeOS();

    RegisterUserApplication(&appmain);

    //ProcessGpio(); //How fast does GPIO need to be processed?
    //ProcessUsb();

    while(1) {
        ProcessTasks();
    }

    return 0;
}
