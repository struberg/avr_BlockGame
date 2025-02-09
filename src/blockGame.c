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
#include "main.h"

#include <avr/pgmspace.h>

#include "avr_common/gfx/tile_8x8.h"
#include "avr_common/strub_common.h"

#define SET_LED PORTB.OUTSET = PIN3_bm;
#define CLR_LED PORTB.OUTCLR = PIN3_bm;

/**
 * @brief a brick game, similar to the tetris game.
 * Note that the display is now rotated 90 degrees.
 * 0/0 is the upper right corner
 * 
 */

PROGMEM const Tile spriteL []= {
    {0x13,{0x80,0x80,0x80,0xC0,0x0,0x0,0x0,0x0}},
    {0x31,{0x10,0xF0,0x0,0x0,0x0,0x0,0x0,0x0}},
    {0x13,{0xC0,0x40,0x40,0x40,0x0,0x0,0x0,0x0}},
    {0x31,{0xF0,0x80,0x0,0x0,0x0,0x0,0x0,0x0}},
};

PROGMEM const Tile spriteZ []= {
    {0x12,{0x40,0xC0,0x80,0x0,0x0,0x0,0x0,0x0}},
    {0x21,{0xC0,0x60,0x0,0x0,0x0,0x0,0x0,0x0}},
    {0x12,{0x40,0xC0,0x80,0x0,0x0,0x0,0x0,0x0}},
    {0x21,{0xC0,0x60,0x0,0x0,0x0,0x0,0x0,0x0}},
};

PROGMEM const Tile spriteI []= {
    {0x3,{0x80,0x80,0x80,0x80,0x0,0x0,0x0,0x0}},
    {0x30,{0xF0,0x0,0x0,0x0,0x0,0x0,0x0,0x0}},
    {0x3,{0x80,0x80,0x80,0x80,0x0,0x0,0x0,0x0}},
    {0x30,{0xF0,0x0,0x0,0x0,0x0,0x0,0x0,0x0}},
};

PROGMEM const Tile spriteS []= {
    {0x11,{0xC0,0xC0,0x0,0x0,0x0,0x0,0x0,0x0}},
    {0x11,{0xC0,0xC0,0x0,0x0,0x0,0x0,0x0,0x0}},
    {0x11,{0xC0,0xC0,0x0,0x0,0x0,0x0,0x0,0x0}},
    {0x11,{0xC0,0xC0,0x0,0x0,0x0,0x0,0x0,0x0}},
};

PROGMEM const Tile spriteT []= {
    {0x21,{0xE0,0x40,0x0,0x0,0x0,0x0,0x0,0x0}},
    {0x12,{0x80,0xC0,0x80,0x0,0x0,0x0,0x0,0x0}},
    {0x21,{0x40,0xE0,0x0,0x0,0x0,0x0,0x0,0x0}},
    {0x12,{0x40,0xC0,0x40,0x0,0x0,0x0,0x0,0x0}},
};

const Tile* blocks[] = {spriteL, spriteZ, spriteI, spriteS, spriteT}; 

FrameBuffer landed;
uint8_t landedMem[MAX7219_MODULE_COUNT*8] = {0,}; 


struct Blockgame {
    uint8_t block;
    Tile currentSprite;
    Tile oldSprite;
    uint8_t posX;
    uint8_t posY;
    uint8_t rotation;
    uint8_t oldPosX;
    uint8_t oldPosY;
    uint8_t oldRotation;

    uint8_t blockType;

    uint8_t time;
    uint8_t speed;
    uint8_t speedStep;

    uint16_t points;
} blockgame;

void bg_select_new_block(void) {
    blockgame.block = nextRandom() % 5;
    blockgame.points++;
    if (blockgame.speed > 5 && (blockgame.points % 32) == 0) {

        // increase speed
        blockgame.speed--;
    }
}

void bg_load_block(void) {
    const Tile* sp = blocks[blockgame.block] + (blockgame.rotation) % 4;

    tile_loadFromProgMem(sp, &blockgame.currentSprite);
} 

/**
 * @brief initialise the block game
 * 
 */
void startBlockGame(void) {
    // first we clear the FrameBuffer
    for (uint8_t i = 0; i < frameBuffer.bufferLen; i++) {
        frameBuffer.buffer[i] = 0;
    }

    landed.buffer = landedMem;
    landed.bufferLen = sizeof(landedMem);
    landed.heigth = 8;
    landed.width = 32;
    landed.widthBytes = 4;

    blockgame.posX = 0;
    blockgame.posY = 0;
    blockgame.rotation = 0;
    blockgame.time = 0;
    blockgame.speed = 40;
    blockgame.speedStep = 0;

    blockgame.points = 0;

    // then load the first sprite
    bg_select_new_block();
    bg_load_block();

    tile_place(&frameBuffer, blockgame.posX, blockgame.posY, &blockgame.currentSprite, true);

    blockgame.oldSprite = blockgame.currentSprite;

    max7219_renderData(&frameBuffer);

}

void buttonPressed_BlockGame(uint8_t buttons) {
    switch (buttons) {
        case BUTTON_LEFT_PRESSED:
            if (blockgame.posY < 7) {
                blockgame.posY++;
                uint8_t maxY = frameBuffer.heigth - tile_getHeigth(&blockgame.currentSprite);
                blockgame.posY = blockgame.posY > maxY ? maxY : blockgame.posY;
            }
            break;
        case BUTTON_RIGHT_PRESSED:
            if (blockgame.posY > 0) {
                blockgame.posY--;
            }
            break;
        case BUTTON_UP_PRESSED:
            blockgame.rotation++;
            bg_load_block();
            uint8_t maxY = frameBuffer.heigth - tile_getHeigth(&blockgame.currentSprite);
            blockgame.posY = blockgame.posY > maxY ? maxY : blockgame.posY;
            break;
        case BUTTON_DOWN_PRESSED:
            break;
    }

}

/**
 * @brief check whether the currentSprite has 'contact' with the already landed sprites
 */
bool bg_collide(void) {
    uint8_t spriteWidth = tile_getWidth(&blockgame.currentSprite);
    if (blockgame.posX  + spriteWidth >= landed.width) {
        // we reached the bottom
        return true;
    }

    uint8_t cols = tile_getWidth(&blockgame.currentSprite);
    uint8_t rows = tile_getHeigth(&blockgame.currentSprite);
    for (uint8_t row = 0; row < rows; row++) {
        for (uint8_t col = cols-1; col >= 0; col--) {
            if (blockgame.currentSprite.bytes[row] & (0x80>>col)) {
                // col+1 because we need to check for closeby pixels
                if (framebuffer_getPixel(&landed, blockgame.posX + col + 1, blockgame.posY + row)) {
                    return true;
                }
                else {
                    break; // finish inner loop. no need to check further pixels in this column
                }
            }
        }
    }
    return false;
}

/**
 * @brief transfer the current sprite to the landed ones
 */
void bg_update_landed(void) {
    tile_place(&landed, blockgame.posX, blockgame.posY, &blockgame.currentSprite, false);
}

/**
 * @brief remove every full line
 */
void bg_remove_completed(void) {
    for (uint8_t col = landed.width-1; col >0; col--) {
        bool colFull=true;
        for (uint8_t row = 0; row < landed.heigth; row++) {
            if (!framebuffer_getPixel(&landed, col, row)) {
                colFull = false;
            }
        }
        
        if (colFull) {
            // remove the line and shift the pixels left of it to the right
            for (uint8_t rowShift = 0; rowShift < landed.heigth; rowShift++) {
                for (uint8_t colShift = col; colShift >0; colShift--) {
                    bool pixelVal = framebuffer_getPixel(&landed, colShift-1, rowShift);
                    framebuffer_setPixel(&landed, colShift, rowShift, pixelVal);

                    // also update the display frameBuffer
                    framebuffer_setPixel(&frameBuffer, colShift, rowShift, pixelVal);
                }
            }
            //X max7219_renderData(&frameBuffer);

            // We removed this very row and shifted the bits above. 
            // Thus we need to handle the very row again next round.
            col++;
        }
    }
}

/**
 * @brief permanent task for the block game
 * 
 * Get's called once every timer tick
 */
void task_BlockGame(void){
    blockgame.time++;

    if (blockgame.time >= 15) {
        blockgame.time = 0;
        blockgame.speedStep++;
        
        bool eraseSprite = true;

        if (blockgame.speedStep == blockgame.speed) {
            if (bg_collide()) {
                
                bg_update_landed();

                bg_remove_completed();

                eraseSprite = false;

                if (blockgame.posX <= 1) {
                    // game over!
                    //X TODO 
                    return;
                }

                blockgame.posX = 0;
                blockgame.posY = 4;
                
                bg_select_new_block();
                bg_load_block();
            }

            blockgame.posX++;
            blockgame.speedStep = 0;
        }

        if (blockgame.oldPosX != blockgame.posX || blockgame.oldPosY != blockgame.posY 
            || blockgame.oldRotation != blockgame.rotation) {

            if (eraseSprite) {
                // delete old sprite
                tile_erase(&frameBuffer, blockgame.oldPosX, blockgame.oldPosY, &blockgame.oldSprite);
            }

            // paint new sprite
            tile_place(&frameBuffer, blockgame.posX, blockgame.posY, &blockgame.currentSprite, false);

            blockgame.oldPosX = blockgame.posX;
            blockgame.oldPosY = blockgame.posY;
            blockgame.oldRotation = blockgame.rotation;
            blockgame.oldSprite = blockgame.currentSprite;

            max7219_renderData(&frameBuffer);
        }
    } 
}
