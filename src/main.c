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


#include "main.h"

#define TASK_LED_bm 0x01
#define TASK_BUTTON_bm 0x02

#define SET_LED PORTB.OUTSET = PIN3_bm;
#define CLR_LED PORTB.OUTCLR = PIN3_bm;



/** Flags for the multitasking */
volatile uint8_t taskTriggered = 0; 


// maps directly to the display ram
FrameBuffer frameBuffer;
uint8_t frameBufferMem[MAX7219_MODULE_COUNT*8]; 


// bigger than the frameBuffer, allows for scrolling
FrameBuffer backBuffer;
uint8_t backBufferMem[(MAX7219_MODULE_COUNT+1)*8]; 



/* Menu mode START */

/**
 * @brief modus for the display
 * 0: show scrolling text
 * 1: edit scrolling text
 * 2: tetris
 * 
 */
#define SCREEN_MODE_SCROLL 0
#define SCREEN_MODE_TETRIS 1
uint8_t screenMode = SCREEN_MODE_SCROLL;

/* Menu mode END */


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

    tile_place(pFrameBuffer, startXPos, 0, &currentChar, true);
    startXPos += tile_getWidth(&currentChar);

    if (startXPos < backBuffer.width) {
        *pPreviousChar = currentChar;
    }

    return startXPos; 
}

char* message = "**  Press the 'Down' button to start the falling block game!  **";
uint8_t msgPos = 0;
Tile previousChar = {0,};

// 0..7 used for shifting the bitmap.
// Once we shifted a whole byte (8 pixel == one matrix), 
// we continue to draw the next missing characters 
uint8_t shiftPos = 0;
uint8_t lastStartXPos = 0;

void do_laufschrift(void) {
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

        max7219_renderData(&frameBuffer);
        pos++;
    }

} 


void task_anzeige(void) {
    if (taskTriggered & TASK_LED_bm) {
        // only once per timer interrupt
        taskTriggered &= ~TASK_LED_bm;

        switch (screenMode) {
            case 0:
                do_laufschrift();
                break;
            case 1:
                task_BlockGame();
                break;
        }
     }
}

void setup_led(void) {
    PORTB.DIRSET = PIN3_bm;
}

void print(char* pText) {
    uint8_t startX = 0;
    Tile prevChar={0,};
    for (uint8_t i = 0; pText[i] != 0; i++) {
        startX = drawNextChar(&backBuffer, pText[i], startX, &prevChar);
    }
}

/**
 * @brief This function will get called whenever a button got pressed
 * 
 * @param buttons 
 */
void buttonPressed(uint8_t buttons) {
    if (screenMode == SCREEN_MODE_TETRIS) {
        buttonPressed_BlockGame(buttons);
        return;
    }
 
    switch (buttons) {
        case BUTTON_LEFT_PRESSED:
            break;
        case BUTTON_RIGHT_PRESSED:
            break;
        case BUTTON_UP_PRESSED:
            break;
        case BUTTON_DOWN_PRESSED:
            if (screenMode == SCREEN_MODE_SCROLL) {
                startBlockGame();
                screenMode = SCREEN_MODE_TETRIS;
            }
            break;
    }
}

void setup_buttons(void (*callback)(uint8_t)) {
    // input switches
    BUTTON_LEFT_PORT.DIR  &= ~BUTTON_LEFT_PIN;
    BUTTON_RIGHT_PORT.DIR &= ~BUTTON_RIGHT_PIN;
    BUTTON_UP_PORT.DIR    &= ~BUTTON_UP_PIN;
    BUTTON_DOWN_PORT.DIR  &= ~BUTTON_DOWN_PIN;


    // sadly enabling pullup is not available via bitmask
    BUTTON_LEFT_PINCTRL   = PORT_PULLUPEN_bm;
    BUTTON_RIGHT_PINCTRL = PORT_PULLUPEN_bm;
    BUTTON_UP_PINCTRL = PORT_PULLUPEN_bm;
    BUTTON_DOWN_PINCTRL = PORT_PULLUPEN_bm;
    setButtonCallback(callback, 0x08);
}

void task_buttons(void) {
    if (taskTriggered & TASK_BUTTON_bm) {
        // only once per timer interrupt
        taskTriggered &= ~TASK_BUTTON_bm;

        uint8_t currentButtons = 0;
        if (!(BUTTON_LEFT_PORT.IN & BUTTON_LEFT_PIN)) {
            currentButtons |= BUTTON_LEFT_PRESSED;
        }
        if (!(BUTTON_RIGHT_PORT.IN & BUTTON_RIGHT_PIN)) {
            currentButtons |= BUTTON_RIGHT_PRESSED;
        }
        if (!(BUTTON_UP_PORT.IN & BUTTON_UP_PIN)) {
            currentButtons |= BUTTON_UP_PRESSED;
        }
        if (!(BUTTON_DOWN_PORT.IN & BUTTON_DOWN_PIN)) {
            currentButtons |= BUTTON_DOWN_PRESSED;
        }

        buttonsCheck(currentButtons);
    }
}

int main(void) {
    setup_cpu();
    setup_task_timer();

    setup_led();
    setup_anzeige();

    setup_buttons((* buttonPressed));

    max7219_init(4);

    sei();

    max7219_startDataFrame();
    for (uint8_t i=0; i < 4; i++) {
        max7219_sendData(MAX7219_CMD_INTENSITY, 0x00);
    }
    max7219_endDataFrame();
    
    while(1) {
        task_anzeige();
        task_buttons();
    }

    return 1;
}