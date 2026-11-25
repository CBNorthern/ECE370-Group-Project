//*********************** MAIN CODE ****************************//
#include "msp430fr6989.h"
#include "Grlib/grlib/grlib.h"          // Graphics library (grlib)
#include "LcdDriver/lcd_driver.h"       // LCD driver
#include <stdio.h>

#define FLAGS UCA1IFG // Contains the transmit & receive flags
#define RXFLAG UCRXIFG // Receive flag
#define TXFLAG UCTXIFG // Transmit flag
#define TXBUFFER UCA1TXBUF // Transmit buffer
#define RXBUFFER UCA1RXBUF // Receive buffer

#define redLED BIT0
#define greenLED BIT7
#define BUT1 BIT1
#define BUT2 BIT2

// AUTO is default
// AUTO = 1, MANUAL = 0
#define AUTO 1
#define MANUAL 0

unsigned int mode = AUTO;
unsigned int ledOn = 0; // initialize MANUAL LED off

// defining address and registers
// Manufacturer ID Addr = 1000100b, Reg = 0x7E
// Device ID Addr = 1000100b, Reg = 0x7F
#define sensor_addr 0b1000100
#define mfgID_reg 0x7E // Manufacturer ID Register
#define devID_reg 0x7F // Device ID Register
#define config_reg 0x01 // Configure Register
#define result_reg 0x00 // Result Register
#define data_config 0x7604 // RN = 0111b, CT = 0, M = 11b, ME = 1
unsigned int data_mfg;
unsigned int data_dev;
unsigned int result;
int pause;
int write_out;

//***************************************************** INITIALIZE FUNCTIONS ********************************************************************//
// Function to configure ACLK to 32 KHz crystal
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

// Configure eUSCI in I2C master mode
void Initialize_I2C(void) {
    // Enter reset state before the configuration starts...
    UCB1CTLW0 |= UCSWRST;
    // Divert pins to I2C functionality
    P4SEL1 |= (BIT1|BIT0);
    P4SEL0 &= ~(BIT1|BIT0);
    // Keep all the default values except the fields below...
    // (UCMode 3:I2C) (Master Mode) (UCSSEL 1:ACLK, 2,3:SMCLK)
    UCB1CTLW0 |= UCMODE_3 | UCMST | UCSSEL_3;
    // Clock divider = 8 (SMCLK @ 1 MHz / 8 = 125 KHz)
    UCB1BRW = 64;
    // Exit the reset mode
    UCB1CTLW0 &= ~UCSWRST;
}

////////////////////////////////////////////////////////////////////
// Write a word (2 bytes) to I2C (address, register)
int i2c_write_word(unsigned char i2c_address, unsigned char i2c_reg, unsigned int data) {
    unsigned char byte1, byte2;
    byte1 = (data >> 8) & 0xFF; // MSByte
    byte2 = data & 0xFF; // LSByte
    UCB1I2CSA = i2c_address; // Set I2C address
    UCB1CTLW0 |= UCTR; // Master writes (R/W bit = Write)
    UCB1CTLW0 |= UCTXSTT; // Initiate the Start Signal
    while ((UCB1IFG & UCTXIFG0) ==0) {}
    UCB1TXBUF = i2c_reg; // Byte = register address
    while((UCB1CTLW0 & UCTXSTT)!=0) {}
    //********** Write Byte #1 ***************************
    UCB1TXBUF = byte1;
    while ( (UCB1IFG & UCTXIFG0) == 0) {}
    //********** Write Byte #2 ***************************
    UCB1TXBUF = byte2;
    while ( (UCB1IFG & UCTXIFG0) == 0) {}
    UCB1CTLW0 |= UCTXSTP;
    while ( (UCB1CTLW0 & UCTXSTP) != 0) {}
    return 0;
}

int i2c_read_word(unsigned char i2c_address, unsigned char i2c_reg) {
    unsigned char byte1, byte2;
    unsigned int data2;
    // Initialize the bytes to make sure data is received every time
    byte1 = 111;
    byte2 = 111;
    //********** Write Frame #1 ***************************
    UCB1I2CSA = i2c_address; // Set I2C address
    UCB1IFG &= ~UCTXIFG0;
    UCB1CTLW0 |= UCTR; // Master writes (R/W bit = Write)
    UCB1CTLW0 |= UCTXSTT; // Initiate the Start Signal
    while ((UCB1IFG & UCTXIFG0) ==0) {}
    UCB1TXBUF = i2c_reg; // Byte = register address
    while((UCB1CTLW0 & UCTXSTT)!=0) {}
    if(( UCB1IFG & UCNACKIFG )!=0) return -1;
    UCB1CTLW0 &= ~UCTR; // Master reads (R/W bit = Read)
    UCB1CTLW0 |= UCTXSTT; // Initiate a repeated Start Signal
    //****************************************************
    //********** Read Frame #1 ***************************
    while ( (UCB1IFG & UCRXIFG0) == 0) {}
    byte1 = UCB1RXBUF;
    //****************************************************
    //********** Read Frame #2 ***************************
    while((UCB1CTLW0 & UCTXSTT)!=0) {}
    UCB1CTLW0 |= UCTXSTP; // Setup the Stop Signal
    while ( (UCB1IFG & UCRXIFG0) == 0) {}
    byte2 = UCB1RXBUF;
    while ( (UCB1CTLW0 & UCTXSTP) != 0) {}
    //****************************************************
    // Merge the two received bytes
    data2 = ( (byte1 << 8) | (byte2 & 0xFF) );
    return data2;
}
//*********************************************************************************************************************************************//


Graphics_Context g_sContext;    // Declare a graphic library context

//***************** Initialize LCD Display (setup and clear) ******************//
void InitLCDdisp(void){

    // Configure SMCLK to 8 MHz (used as SPI clock)
    CSCTL0 = CSKEY;                 // Unlock CS registers
    CSCTL3 &= ~(BIT4|BIT5|BIT6);    // DIVS=0
    CSCTL0_H = 0;                   // Relock the CS registers

    ////////////////////////////////////////////////////////////////////////////////////////////
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

    // Default AUTO
    Graphics_drawStringCentered(&g_sContext, "  Auto Mode ", AUTO_STRING_LENGTH, 64, 25, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, "Level: 0", AUTO_STRING_LENGTH, 64, 35, OPAQUE_TEXT);
}
//*********************************************************************//

//***************** LCD Display *****************//
void LCDdisp(unsigned char *level){

    if (mode == 0){ // MANUAL
        Graphics_drawStringCentered(&g_sContext, "Manual Mode", AUTO_STRING_LENGTH, 64, 25, OPAQUE_TEXT);
        if (ledOn == 1){
            Graphics_drawStringCentered(&g_sContext, " LED On ", AUTO_STRING_LENGTH, 64, 35, OPAQUE_TEXT);
        } else {
            Graphics_drawStringCentered(&g_sContext, " LED Off", AUTO_STRING_LENGTH, 64, 35, OPAQUE_TEXT);
        }
    } else if (mode == 1){ // AUTO
        Graphics_drawStringCentered(&g_sContext, "  Auto Mode ", AUTO_STRING_LENGTH, 64, 25, OPAQUE_TEXT);
        Graphics_drawStringCentered(&g_sContext, level, AUTO_STRING_LENGTH, 64, 35, OPAQUE_TEXT);
    } else {
        Graphics_drawStringCentered(&g_sContext, "    ???    ", AUTO_STRING_LENGTH, 64, 25, OPAQUE_TEXT);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*********************************************** MAIN FUNCT *************************************************************//
void main(void){
    WDTCTL = WDTPW | WDTHOLD; // Stop the Watchdog timer
    PM5CTL0 &= ~LOCKLPM5; // Disable GPIO power-on default high impedance mode

    // Initialize configuration functions
    config_ACLK_to_32KHz_crystal();
    InitLCDdisp();
    Initialize_I2C();

    // initialize LEDs
    P1DIR |= redLED; // Divert pin to TA0.1 (for PWM)
    P1SEL1 &= ~redLED;
    P1SEL0 |= redLED;
    P9DIR |= greenLED;
    P9OUT |= greenLED;

    // initialize pushbuttons
    // Configuring buttons with interrupt
    P1DIR &= ~(BUT1 | BUT2); // 0: input
    P1REN |= (BUT1 | BUT2); // 1: enable built-in resistors
    P1OUT |= (BUT1 | BUT2); // 1: built-in resistor is pulled up to Vcc
    P1IE |= (BUT1 | BUT2); // 1: enable interrupts
    P1IES |= (BUT1 | BUT2); // 1: interrupt on falling edge
    P1IFG &= ~(BUT1 | BUT2); // 0: clear the interrupt flags

    // Configure I2C
    write_out = i2c_write_word(sensor_addr, config_reg, data_config); // write to config register

    // Configure Timer 0
    TA0CCR0 = 120 - 1; // set number of cycles for PWM
    TA0CTL = TASSEL_1 | ID_0 | MC_1 | TACLR; // Crystal ACLK, Div = 0, Up Mode
    // Channel 1
    TA0CCR1 = 0; // start with LED off
    TA0CCTL1 |= OUTMOD_7; // Reset/Set Output Mode

    // Configure Timer 1
    // Channel 0
    TA1CCR0 = 32768 - 1; // T = 1 second
    TA1CCTL0 &= ~CCIFG;
    TA1CCTL0 |= CCIE;
    TA1CTL = TASSEL_1 | ID_0 | MC_1 | TACLR; // Crystal ACLK, Div = 0, Up Mode

    // enable interrupts
    _low_power_mode_0();

    // Infinite loop to read lux value
    while(1){}


}


#define result_reg 0x00 // Result Register
unsigned int result = 0;
unsigned int iteration = 0;
char *ledLevel = "Level: 0";

//******************** LED ISR **********************//
#pragma vector = TIMER1_A0_VECTOR
__interrupt void TA1_A0_ISR(){
    iteration++;

    result += i2c_read_word(sensor_addr, result_reg); // read lux value from result register

    // iterate every 4 seconds
    if (iteration == 4){
        result /= 4; // find average lux result

        // set AUTO
        if (result < 100){
            TA0CCR1 = 80;
            ledLevel = "Level: 4";
        } else if ((100 <= result)&&(result < 200)){
            TA0CCR1 = 60;
            ledLevel = "Level: 3";
        } else if ((200 <= result)&&(result < 300)){
            TA0CCR1 = 40;
            ledLevel = "Level: 2";
        } else if ((300 <= result)&&(result < 400)){
            TA0CCR1 = 20;
            ledLevel = "Level: 1";
        } else if (result > 400){
            TA0CCR1 = 0;
            ledLevel = "Level: 0";
        }
        LCDdisp(ledLevel);
        iteration = 0; // reset iterations
        result = 0; // reset result
    }

}

//******* PUSHBUTTON ISR *******
#pragma vector = PORT1_VECTOR // Link the ISR to the vector__interrupt
__interrupt void Port1_ISR() {
    // Detect button 1 (toggle modes)
    if ((P1IFG & BUT1) != 0){
        mode ^= 1; // toggle modes
        P1SEL0 ^= redLED;
        P9OUT ^= greenLED; // toggle mode indicator light
        P1OUT &= ~redLED; // red LED starts off
        ledOn = 0;
        if (mode == AUTO){ // reset to 0
            iteration = 0;
            result = 0;
        }
    }
    // Detect button 2 (only for MANUAL; toggle red LED)
    if (mode == 0){ // make sure in MANUAL
        if ((P1IFG & BUT2) != 0){
            ledOn ^= 1; // toggle level/on/off
            P1OUT ^= redLED;
        }
    }
    // display on LCD
    LCDdisp(ledLevel);

    // clear flags
    P1IFG &= ~BUT1;
    P1IFG &= ~BUT2;
}
