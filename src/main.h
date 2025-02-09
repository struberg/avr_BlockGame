/*
 * Copyright 2018-2025 Mark Struberg
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __MAIN_H__
    #define __MAIN_H__

// Debugging
#define DEBUG_ENABLED



#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stddef.h>

#include "avr_common/strub_common.h"
#include "avr_common/max7219.h"
#include "avr_common/gfx/font_proportional.h"
#include "avr_common/gfx/tile_8x8.h"
#include "avr_common/button.h"


/* BUTTONS START */
#define BUTTON_LEFT_PORT VPORTA
#define BUTTON_LEFT_PIN PIN5_bm
#define BUTTON_LEFT_PINCTRL PORTA.PIN5CTRL
#define BUTTON_LEFT_PRESSED 0x01

#define BUTTON_RIGHT_PORT VPORTA
#define BUTTON_RIGHT_PIN PIN4_bm
#define BUTTON_RIGHT_PINCTRL PORTA.PIN4CTRL
#define BUTTON_RIGHT_PRESSED 0x02

#define BUTTON_UP_PORT VPORTA
#define BUTTON_UP_PIN PIN6_bm
#define BUTTON_UP_PINCTRL PORTA.PIN6CTRL
#define BUTTON_UP_PRESSED 0x04

#define BUTTON_DOWN_PORT VPORTA
#define BUTTON_DOWN_PIN PIN7_bm
#define BUTTON_DOWN_PINCTRL PORTA.PIN7CTRL
#define BUTTON_DOWN_PRESSED 0x08
/* BUTTONS END */



/* DISPLAY START  */

#define MAX7219_MODULE_COUNT 4


// maps directly to the display ram
extern FrameBuffer frameBuffer;
extern uint8_t frameBufferMem[MAX7219_MODULE_COUNT*8]; 


// bigger than the frameBuffer, allows for scrolling
extern FrameBuffer backBuffer;
extern uint8_t backBufferMem[(MAX7219_MODULE_COUNT+1)*8]; 

/* DISPLAY END  */

/**
 * @brief initialise the block game
 * 
 */
void startBlockGame(void);

/**
 * @brief permanent task for the block game
 * 
 */
void task_BlockGame(void);

void buttonPressed_BlockGame(uint8_t buttons);

#endif