/**
 * @file main.c
 * @author Mark Struberg (struberg@apache.org)
 * @brief Mini AVR block game and scrolling light with MAX7219x4
 * @version 0.1
 * @date 2024-12-23
 * 
 * @copyright Copyright (c) 2024 Mark Struberg
 * @license Apache License v2.0
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

#define DISPLAY_BUFFER_LEN (4*8)
uint8_t displayBuffer[DISPLAY_BUFFER_LEN]; 
FrameBuffer frameBuffer;

/** DISPLAY END  **/


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
    frameBuffer.width=32;
    frameBuffer.widthBytes = 4;
    frameBuffer.heigth=8;
    frameBuffer.buffer=displayBuffer;
    frameBuffer.bufferLen = DISPLAY_BUFFER_LEN;
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
            framebuffer_vline(&frameBuffer, startXPos++, 0, 7, false);
        }
    }

    tile_place(pFrameBuffer, startXPos, 0, &currentChar);
    startXPos += tile_getWidth(&currentChar);
    *pPreviousChar = currentChar;
    return startXPos; 
}

char* message = "  Tag! Hallo Strubi! ";
uint8_t msgPos = 0;
Tile previousChar = {0,};

void task_anzeige(void) {
    if (taskTriggered & TASK_LED_bm) {
        // only once per timer interrupt
        taskTriggered &= ~TASK_LED_bm;
        counter++;
        uint8_t charPos = msgPos;
        if (counter == 1500) {
            counter = 0;
            SET_LED
            uint8_t startXPos = 0;
            do {
                if (message[charPos] == 0) {
                    charPos = 0;
                }
                startXPos = drawNextChar(&frameBuffer, message[charPos++], startXPos, &previousChar);
            } while(startXPos < frameBuffer.width);
            
            msgPos++;
            if (message[msgPos] == 0) {
                msgPos = 0;
            }
            max7219_renderData(&frameBuffer, 4);
            pos++;
            CLR_LED
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