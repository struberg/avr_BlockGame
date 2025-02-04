/**
 * @file main.c
 * @author Mark Struberg (struberg@apache.org)
 * @brief Mini AVR block game and scrolling light with MAX7219x4
 * @version 0.1
 * @date 2024-12-23
 * 
 * @copyright Copyright (c) 2024 Mark Struberg
 * @license Apache License v2.0
 * 
 * 
 */

#define F_CPU 10000000UL


#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stddef.h>

#include "avr_common/strub_common.h"
#include "avr_common/max7219.h"
#include "avr_common/gfx/font_proportional.h"
#include "avr_common/gfx/tile_8x8.h"


#define TASK_LED_bm 0x01

#define SET_LED PORTB.OUTSET = PIN3_bm;
#define CLR_LED PORTB.OUTCLR = PIN3_bm;

volatile uint8_t taskTriggered = 0; 

/** DISPLAY START  **/

#define MAX7219_MODULE_COUNT 4


// maps directly to the display ram
FrameBuffer frameBuffer;
uint8_t frameBufferMem[MAX7219_MODULE_COUNT*8]; 

// bigger than the frameBuffer, allows for scrolling
FrameBuffer backBuffer;
uint8_t backBufferMem[(MAX7219_MODULE_COUNT+1)*8]; 

/** DISPLAY END  **/


/** BUTTONS START **/

/** BUTTONS END **/

void setup_cpu(void) {
    // auf prescaler /2 stellen
    // damit rennt die CPU dann effektiv auf 10 MHz
    // attention, this needs the CCP (Configuration Change Protection) procedure!

    CCP = CCP_IOREG_gc;
    CLKCTRL.MCLKCTRLB = 0x01;

    CLKCTRL.MCLKCTRLA = 0x00;
}

void setup_task_timer(void) {
    TASK_TIMER.CCMP = TASK_TIMER_OVERFLOW; /* Compare or Capture */

    TASK_TIMER.CTRLB = 0 | TCB_CNTMODE_INT_gc; // stick at periodic interrupt mode

    TASK_TIMER.CTRLA = TCB_CLKSEL_CLKDIV1_gc  /* CLK_PER (without Prescaler) */
                    | 1 << TCB_ENABLE_bp   /* Enable: enabled */
                    | 0 << TCB_RUNSTDBY_bp /* Run Standby: disabled */
                    | 0 << TCB_SYNCUPD_bp; /* Synchronize Update: disabled */


    // enable Overflow Interrupt. TOP is CCMP
    TASK_TIMER.INTCTRL = TCB_CAPT_bm;
}

volatile uint16_t timers[3] = {0,};

/**
 * @brief TimerB0 overflow
 * 
 * Setzt die 'taskTriggered' bitmask auf FF.
 * Jeder Task kann dann selber checken ob er schon dran war 
 * indem er ein bit darin repräsentiert.
 */
ISR (TCB0_INT_vect) {
    for (uint8_t i=0; i< 3; i++) {
        if (timers[i] > 0) {
            timers[i]--;
        }
    }
    taskTriggered = 0xFF;

    // special handling in the new tinys. one needs to reset the int flags manually 
    TCB0.INTFLAGS = TCB_CAPT_bm;
}


void setup_anzeige(void) {
    frameBuffer.widthBytes = MAX7219_MODULE_COUNT;
    frameBuffer.width=frameBuffer.widthBytes*8;
    frameBuffer.heigth=8;
    frameBuffer.buffer=frameBufferMem;
    frameBuffer.bufferLen = sizeof(frameBufferMem);

    backBuffer.widthBytes = MAX7219_MODULE_COUNT+1;
    backBuffer.width=backBuffer.widthBytes*8;
    backBuffer.heigth=8;
    backBuffer.buffer=backBufferMem;
    backBuffer.bufferLen =  sizeof(backBufferMem);
}

static uint16_t counter = 0;
static uint16_t pos = 0;

/**
 * @brief draw the next character to the framebuffer
 * 
 * @param pFrameBuffer 
 * @param character the character to print
 * @param startXPos the x start position where the font tile of the character should be placed
 * @param pPreviousChar the previously printed character or empty or NULL
 * @return int the new last x pixel position of the printed character
 */
int drawNextChar(FrameBuffer* pFrameBuffer, char character, uint8_t startXPos, Tile* pPreviousChar) {
    Tile currentChar;
    fontp_loadCharTile(character, &currentChar);
    if (pPreviousChar != NULL && pPreviousChar->size != 0) {
        if (fontp_collide(pPreviousChar, &currentChar)) {
            // draw an empty line between the 2 font characters and increase the x pos
            SET_LED
            framebuffer_vline(pFrameBuffer, startXPos++, 0, 7, false);
            CLR_LED
        }
    }

    tile_place(pFrameBuffer, startXPos, 0, &currentChar);
    startXPos += tile_getWidth(&currentChar);

    if (startXPos < backBuffer.width) {
        *pPreviousChar = currentChar;
    }

    return startXPos; 
}

char* message = "*****  This is a scrolling text!  *****";
uint8_t msgPos = 0;
Tile previousChar = {0,};

// 0..7 used for shifting the bitmap.
// Once we shifted a whole byte (8 pixel == one matrix), 
// we continue to draw the next missing characters 
uint8_t shiftPos = 0;
uint8_t lastStartXPos = 0;

void task_anzeige(void) {
    if (taskTriggered & TASK_LED_bm) {
        // only once per timer interrupt
        taskTriggered &= ~TASK_LED_bm;
        counter++;
        if (counter == 150) {
            counter = 0;
            if (shiftPos == 0) {
                // we shifted out 8 pixels, now we need to draw again
                uint8_t startXPos = lastStartXPos;
                do {
                    lastStartXPos = startXPos;
                    startXPos = drawNextChar(&backBuffer, message[msgPos], startXPos, &previousChar);

                    if (startXPos < backBuffer.width) {
                        // otherwise we have to draw that character again next time
                        msgPos++;
                    }

                    if (message[msgPos] == 0) {
                        msgPos = 0;
                    }
                } while (startXPos < backBuffer.width);

                lastStartXPos -= 8; // we will shift this out
            }

            // are we in shift mode?
            for (uint8_t row = 0; row < 8; row++) {
                uint8_t rowStart = row*backBuffer.widthBytes;
                for (int col=0; col < backBuffer.widthBytes; col++) {
                    backBuffer.buffer[rowStart+col] <<= 1;
                    if (col < backBuffer.widthBytes-1) {
                        // for all but the last byte we have to carry over the MSB from the next byte
                        backBuffer.buffer[rowStart+col] |= (backBuffer.buffer[rowStart+col+1] >> 7);
                    }
                }
            }

            shiftPos++;
            if (shiftPos == 8) {
                shiftPos = 0;
            }

            // now copy the backBuffer to the frameBuffer
            for (uint8_t row = 0; row < 8; row++) {
                uint8_t fbRowStart = row*frameBuffer.widthBytes;
                uint8_t bbRowStart = row*backBuffer.widthBytes;
                for (int col=0; col < frameBuffer.widthBytes; col++) {
                    frameBuffer.buffer[fbRowStart+col] = backBuffer.buffer[bbRowStart+col];
                }
            }
            

            max7219_renderData(&frameBuffer, 4);
            pos++;
        }
    }
}

void setup_led(void) {
    PORTB.DIRSET = PIN3_bm;
}

int main(void) {
    setup_cpu();
    setup_task_timer();

    setup_led();
    setup_anzeige();

    max7219_init(4);
    
    sei();

    max7219_startDataFrame();
    for (uint8_t i=0; i < 4; i++) {
        max7219_sendData(MAX7219_CMD_INTENSITY, 0x00);
    }
    max7219_endDataFrame();
    
    while(1) {
        task_anzeige();
    }

    return 1;
}