/* TinyI2C v2.0.1

   David Johnson-Davies - www.technoblogy.com - 5th June 2022

   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license:
   http://creativecommons.org/licenses/by/4.0/
*/

#include "TinyI2CMaster.h"

TinyI2CMaster::TinyI2CMaster() {}

/* *********************************************************************************************************************

   Minimal Tiny I2C Routines for the new 0-series and 1-series ATtiny and ATmega microcontrollers,
   such as the ATtiny402 or ATmega4809, and the AVR Dx series microcontrollers, such as the AVR128DA48
   and AVR32DB28.

********************************************************************************************************************* */

#define TWI0_BAUD(F_SCL, T_RISE)                                                                   \
    ((((((float)F_CLK_PER / (float)F_SCL)) - 10 - ((float)F_CLK_PER * T_RISE / 1000000))) / 2)

void TinyI2CMaster::init() {
    uint32_t baud = TWI0_BAUD(100000UL, 2UL);
    TWI0.MBAUD = (uint8_t)baud;
    TWI0.MCTRLA = TWI_ENABLE_bm; // Enable as master, no interrupts
    TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;
}

uint8_t TinyI2CMaster::read(void) {
    if (I2Ccount != 0)
        I2Ccount--;
    while (!(TWI0.MSTATUS & TWI_RIF_bm))
        ; // Wait for read interrupt flag
    uint8_t data = TWI0.MDATA;
    // Check slave sent ACK?
    if (I2Ccount != 0)
        TWI0.MCTRLB = TWI_MCMD_RECVTRANS_gc; // ACK = more bytes to read
    else
        TWI0.MCTRLB = TWI_ACKACT_NACK_gc; // Send NAK
    return data;
}

uint8_t TinyI2CMaster::readLast(void) {
    I2Ccount = 0;
    return TinyI2CMaster::read();
}

bool TinyI2CMaster::write(uint8_t data) {
    TWI0.MCTRLB = TWI_MCMD_RECVTRANS_gc; // Prime transaction
    TWI0.MDATA = data;                   // Send data
    while (!(TWI0.MSTATUS & TWI_WIF_bm))
        ; // Wait for write to complete
    if (TWI0.MSTATUS & (TWI_ARBLOST_bm | TWI_BUSERR_bm))
        return false;                      // Fails if bus error or arblost
    return !(TWI0.MSTATUS & TWI_RXACK_bm); // Returns true if slave gave an ACK
}

// Start transmission by sending address
bool TinyI2CMaster::start(uint8_t address, int32_t readcount) {
    bool read;
    if (readcount == 0)
        read = 0; // Write
    else {
        I2Ccount = readcount;
        read = 1;
    }                                 // Read
    TWI0.MADDR = address << 1 | read; // Send START condition
    while (!(TWI0.MSTATUS & (TWI_WIF_bm | TWI_RIF_bm)))
        ;                                // Wait for write or read interrupt flag
    if (TWI0.MSTATUS & TWI_ARBLOST_bm) { // Arbitration lost or bus error
        while (!(TWI0.MSTATUS & TWI_BUSSTATE_IDLE_gc))
            ; // Wait for bus to return to idle state
        return false;
    } else if (TWI0.MSTATUS & TWI_RXACK_bm) { // Address not acknowledged by client
        TWI0.MCTRLB |= TWI_MCMD_STOP_gc;      // Send stop condition
        while (!(TWI0.MSTATUS & TWI_BUSSTATE_IDLE_gc))
            ; // Wait for bus to return to idle state
        return false;
    }
    return true; // Return true if slave gave an ACK
}

bool TinyI2CMaster::restart(uint8_t address, int32_t readcount) {
    return TinyI2CMaster::start(address, readcount);
}

void TinyI2CMaster::stop(void) {
    TWI0.MCTRLB |= TWI_MCMD_STOP_gc; // Send STOP
    while (!(TWI0.MSTATUS & TWI_BUSSTATE_IDLE_gc))
        ; // Wait for bus to return to idle state
}

TinyI2CMaster TinyI2C = TinyI2CMaster(); // Instantiate a TinyI2C object
