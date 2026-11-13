// Code to print to the LCD pixel display on the Educational BoosterPack

#include "msp430fr6989.h"
#include "Grlib/grlib/grlib.h"          // Graphics library (grlib)
#include "LcdDriver/lcd_driver.h"       // LCD driver
#include <stdio.h>

// Write on BoosterPack LCD Display //
void LCDdisp(void){

    WDTCTL = WDTPW | WDTHOLD;     // Stop the Watchdog timer
    PM5CTL0 &= ~LOCKLPM5;         // Disable GPIO power-on default high-impedance mode

    // Configure SMCLK to 8 MHz (used as SPI clock)
    CSCTL0 = CSKEY;                 // Unlock CS registers
    CSCTL3 &= ~(BIT4|BIT5|BIT6);    // DIVS=0
    CSCTL0_H = 0;                   // Relock the CS registers

    ////////////////////////////////////////////////////////////////////////////////////////////
    Graphics_Context g_sContext;        // Declare a graphic library context
    Crystalfontz128x128_Init();         // Initialize the display

    // Set the screen orientation
    Crystalfontz128x128_SetOrientation(0);

    // Initialize the context
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);

    // Set background and foreground colors
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);

    // Set the default font for strings
    GrContextFontSet(&g_sContext, &g_sFontFixed6x8);

    // Clear the screen
    Graphics_clearDisplay(&g_sContext);
    ////////////////////////////////////////////////////////////////////////////////////////////


    // infinite loop
    while(1){}
}
////////////////////////////////////////////////////////////////////////////////////////////////

#define redLED BIT0 // Red LED at P1.0
#define greenLED BIT7 // Green LED at P9.7
#define BUT1 BIT1 // Button S1 at P1.1
#define BUT2 BIT2 // Button S2 at P1.2

// main function
void main (void){
    WDTCTL = WDTPW | WDTHOLD; // Stop the Watchdog timer
    PM5CTL0 &= ~LOCKLPM5; // Enable the GPIO pins

    // Configure LEDs
    P1DIR |= redLED; // Direct pin as output (for red LED)
    P1SEL1 &= ~redLED;
    P1SEL0 |= redLED;
    P9DIR |= greenLED; // Direct pin as output (for green LED)
    P9OUT &= ~greenLED; // Turn green LED Off

    PM5CTL0 &= ~LOCKLPM5;

    TA0CTL = TASSEL_2 | ID_0 | MC_1 | TACLR; // SMCLK, Div = 0, Up Mode

    TA0CCR0 = 10000 - 1;
    TA0CCTL1 |= OUTMOD_7; // Reset/Set Output Mode

    // Configure buttons
    P1DIR &= ~(BUT1 | BUT2); // Direct pin as input
    P1REN |= (BUT1 | BUT2); // Enable built-in resistor
    P1OUT |= (BUT1 | BUT2); // Set resistor as pull-up

    while(1){
        if ((P1IN & (BUT1 | BUT2)) == BUT2){ // Check S1 is pressed
            TA0CCR1 = TA0CCR0; // LED fully on
            P9OUT |= redLED;
        } else if ((P1IN & (BUT1 | BUT2)) == BUT1){ // Check S2 is pressed
            TA0CCR1 = TA0CCR0/8; // LED on
            P9OUT |= redLED;
        } else {
            TA0CCR1 = 0*TA0CCR0; // LED off
            P9OUT |= redLED;
        }

        if ((TA0CTL & TAIFG) !=0){
            TA0CTL &= ~TAIFG; // clear flag
        }
    }
}

