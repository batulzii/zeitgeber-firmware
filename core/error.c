/* 
 * File:   error.c
 * Author: Jared
 *
 * Created on 18 July 2014, 8:12 PM
 */

////////// Includes ////////////////////////////////////////////////////////////

#include "system.h"
#include "core/error.h"
#include "core/os.h"

#include "hardware.h"
#include "api/graphics/gfx.h"

////////// Code ////////////////////////////////////////////////////////////////

static bool in_error = false;

//TODO: What happens with the stack when the stack trap is called???

void CriticalError(const char* msg) {

    // Disable the RTOS
    T1CONbits.TON = 0;
    RCONbits.SWDTEN = 0;

    // Display an error message
    ClearImage();
    DrawString("CRITICAL ERROR", 8,8, RED);
    DrawString(msg, 8,18, WHITE);
    UpdateDisplay();

    while (!_PORT(BTN1) && !_PORT(BTN2) && !_PORT(BTN3) && !_PORT(BTN4));
    while (_PORT(BTN1) || _PORT(BTN2) || _PORT(BTN3) || _PORT(BTN4));
    //while (!_PORT(BTN1));

    int i;
    //for (i=0; i<1000000; i++) ;

    Reset();
}


////////// Traps ///////////////////////////////////////////////////////////////

void isr _OscillatorFail() {
    CriticalError("Trap: OSC Failed");
}

void isr _AddressError() {
    CriticalError("Trap: Address Error");
}

void isr _StackError() {
    CriticalError("Trap: Stack Error");
}

void isr _MathError() {
    CriticalError("Trap: Math Error");
}