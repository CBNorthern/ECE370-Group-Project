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


// Function to configure ACLK to 32 KHz crystal //
void config_ACLK_to_32KHz_crystal() {
    // By default, ACLK runs on LFMODCLK at 5MHz/128 = 39 KHz
    // Reroute pins to LFXIN/LFXOUT functionality
    PJSEL1 &= ~(BIT4|BIT5);
    PJSEL0 |= (BIT4|BIT5);

    // Wait until the oscillator fault flags remain cleared
    CSCTL0 = CSKEY; // Unlock CS registers
    do {
        CSCTL5 &= ~LFXTOFFG; // Local fault flag
        SFRIFG1 &= ~OFIFG; // Global fault flag
    } while((CSCTL5 & LFXTOFFG) != 0);

        CSCTL0_H = 0; // Lock CS registers

    return;
}
//////////////////////////////////////////////////

#define redLED BIT0 // Red LED at P1.0
#define greenLED BIT7 // Green LED at P9.7
#define BUT1 BIT1 // Button S1 at P1.1
#define BUT2 BIT2 // Button S2 at P1.2

// main function
void main (void){
    WDTCTL = WDTPW | WDTHOLD; // Stop the Watchdog timer
    PM5CTL0 &= ~LOCKLPM5; // Enable the GPIO pins
    config_ACLK_to_32KHz_crystal(); // 32 kHz clock function call

    // Configure LEDs
    P1DIR |= redLED; // Divert pin to TA0.1 (for PWM)
    P1SEL1 &= ~redLED;
    P1SEL0 |= redLED;
    P9DIR |= greenLED; // Direct pin as output (for green LED)
    P9OUT &= ~greenLED; // Turn green LED Off

    // Configure buttons
    P1DIR &= ~(BUT1 | BUT2); // Direct pin as input
    P1REN |= (BUT1 | BUT2); // Enable built-in resistor
    P1OUT |= (BUT1 | BUT2); // Set resistor as pull-up

    // Configure Timer 0
    TA0CCR0 = 120 - 1; // set number of cycles for PWM
    TA0CTL = TASSEL_1 | ID_0 | MC_1 | TACLR; // Crystal ACLK, Div = 0, Up Mode
    // Channel 1
    TA0CCR1 = 20 - 1; // start at lowest brightness level
    TA0CCTL1 |= OUTMOD_7; // Reset/Set Output Mode

    // Configure Timer 1
    TA1CCR0 = 32768 - 1; // T = 1 second
    TA1CCTL0 &= ~CCIFG;
    TA1CCTL0 |= CCIE;
    TA1CTL = TASSEL_1 | ID_0 | MC_1 | TACLR; // Crystal ACLK, Div = 0, Up Mode

    // Low Power Mode / Enable Interrupts
    _low_power_mode_3();

}


// LED ISR
#pragma vector = TIMER1_A0_VECTOR
__interrupt void TA1_A0_ISR(){

    TA0CCR1 += 20; // increase brightness
    if (TA0CCR1 >= 80){
        TA0CCR1 = 0; // roll back to zero
    } else if (TA0CCR1 == 20){
        TA0CCR1 -= 1; // reset to (20 - 1) from 0
    }

}
