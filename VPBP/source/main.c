#include "gpio.h"
#include "uart.h"
#include "fb.h"

//#include <stdio.h>
//#include <unistd.h>

// include images
// #include "dk_image.h"
#include "dk_ladder1.h"
#include "dk_ladder2.h"
#include "dk_left0.h"
#include "dk_left1.h"
#include "dk_left2.h"
#include "dk_right0.h"
#include "dk_right1.h"
#include "dk_right2.h"

#include "mario_left1.h"
#include "mario_left2.h"
#include "mario_right1.h"
#include "mario_right2.h"

#include "bird_left1.h"
#include "bird_left2.h"
#include "bird_left3.h"
#include "bird_right1.h"
#include "bird_right2.h"
#include "bird_right3.h"

#include "bananarang.h"
#include "bananarang2.h"
#include "bananarang3.h"
#include "bananarangpack.h"
#include "emptypack.h"

#include "enemy.h"
#include "coin.h"
#include "health.h"
#include "black.h"
#include "heartpack.h"
#include "coinpack.h"
#include "teleporter.h"

#include "ladder.h"
#include "platform.h"

#include "structures.c"

#define MAXOBJECTS 30
#define SCREENWIDTH 1888
#define SCREENHEIGHT 1000
#define JUMPHEIGHT 100

#define LEFTEND (SCREENWIDTH - SCREENHEIGHT) / 2 // Left end of "game box" in pixels.
#define RIGHTEND LEFTEND + SCREENHEIGHT          // box has side length of SCREENHEIGHT.

#define FONT_WIDTH 8
#define FONT_HEIGHT 8

// GPIO macros

#define GPIO_BASE 0xFE200000
#define CLO_REG 0xFE003004
#define GPSET0_s 7
#define GPCLR0_s 10
#define GPLEV0_s 13

#define CLK 11
#define LAT 9
#define DAT 10

#define INP_GPIO(p) *(gpio + ((p) / 10)) &= ~(7 << (((p) % 10) * 3))
#define OUT_GPIO(p) *(gpio + ((p) / 10)) |= (1 << (((p) % 10) * 3))

unsigned int *gpio = (unsigned *)GPIO_BASE;
unsigned int *clo = (unsigned *)CLO_REG;

// Some method signatures...
void erase_state(struct gamestate *state);
int is_valid_cell(int x, int y, struct gamestate *state);
// void myDrawImage(unsigned char * img, int width, int height, int offx, int offy);

///////////////////////////////
// Init GPIO Code from Dylan //
///////////////////////////////

#define INPUT 0b000
#define OUTPUT 0b001

void init_gpio(int pin, int func)
{
    // find the index / corresponding function select register for the pin
    // -> hint: 10 pins per rgister
    // 16 / 10 -> 1.6 -> 1
    // 23 / 10 -> 2
    int index = pin / 10; // tell us which GPFSEL register to use

    int bit_shift = (pin % 10) * 3; // bit shift we want to mask out / replace

    // gpio[index] = gpio[index] & ~(0b111 << bit_shift);
    // right now: we have the gpio function select register, with
    // the bits for our function cleared.
    // the last thing to do is actually set these bits to funciton
    gpio[index] = (gpio[index] & ~(0b111 << bit_shift)) | (func << bit_shift);
}

// val is either 0 (for clearing) or 1 (for setting)
void write_gpio(int pin_number, int val)
{
    // find which register we want to modify

    // so if val is 0 -> we want to set a bit in clear registers
    // if val is 1 -> we want to set a bit in the set registers

    // it's kind of like a latch where we have to send a 1 to reset to clear it, and a 1 to data to write to it.

    if (val == 1)
    {
        // if val is 1 -> we want to set a bit in the set registers
        // you would want to check if the pin >= 32
        // if this was the case, target_addr = set register 1
        (*GPSET0) = 1 << pin_number;
    }
    else
    {
        (*GPCLR0) = 1 << pin_number;
    }
}

int read_gpio(int pin_number)
{
    // GPLEV0 encodes the first 0-31 GPIO pin values
    // to read pin x, we shift right by x so GPIO pin value x is at bit 0.
    // then, we can and with 1 to delete the rest of the data.

    return ((*GPLEV0) >> pin_number) & 1;
}

void init_snes_lines()
{
    // initialize GPIO lines for CLK, DAT, LAT
    // set CLK and LAT to outputs (we write to these)
    init_gpio(CLK, OUTPUT);
    init_gpio(LAT, OUTPUT);
    // set DATA as an input (we read the SNES buttons from)
    init_gpio(DAT, INPUT);
}

void wait(int dur)
{
    unsigned c = *clo + dur;
    while (c > *clo)
        ;
}

// Read SNES using method from lecture.
// Returns 1 if any button has been pressed, 0 if not.
int read_SNES(int *array)
{
    int data;

    write_gpio(CLK, 1);
    write_gpio(LAT, 1);
    wait(12);

    write_gpio(LAT, 0);

    // flag will be turned on if we read 0 from any button.
    int flag = 0;

    int i = 1;
    while (i <= 16)
    {
        wait(6);
        write_gpio(CLK, 0);
        wait(6);
        data = read_gpio(DAT);
        array[i - 1] = data;
        if (data == 0)
        {
            flag = 1;
        }
        write_gpio(CLK, 1);
        ++i;
    }

    return flag;
}

// Array to track which buttons have been pressed;
int buttons[16];

int map1[625] = {
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,
   0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,
   0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
   0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,
   0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    };

int map2[625] = {
    0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,
    0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,1,1,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,
    0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
};

int map3[625] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,
    0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,
    0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
    0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,
    0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
    0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,
    0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,
};

///////////////////////
// DRAWING FUNCTIONS //
///////////////////////

// Convert grid coords -> pixel coords
int grid_to_pixel_x(int x, int width)
{
    return LEFTEND + x * ((RIGHTEND - LEFTEND) / width);
}

int grid_to_pixel_y(int y, int height)
{
    return y * (SCREENHEIGHT / height);
}

// Draws an object at its current grid coordinates.
void draw_grid(struct object *o, int width, int height)
{
    if (!(*o).trampled) myDrawImage((*o).sprite.img, (*o).sprite.width, (*o).sprite.height, grid_to_pixel_x((*o).loc.x, width), grid_to_pixel_y((*o).loc.y, height));
}

// Draws an int at specified pixel offsets (right end of number at offx)
void draw_int(unsigned int n, int offx, int offy, unsigned char attr)
{
    while (n > 0)
    {
        // Print the smallest digit of n.
        char digit = (n % 10) + 48;
        drawChar(digit, offx, offy, attr);
        offx -= FONT_WIDTH;
        n = n / 10;
    }
}

// Draws an image structure at specified pixel offsets
void draw_image(struct image myimg, int offx, int offy)
{
    myDrawImage(myimg.img, myimg.width, myimg.height, offx, offy);
}

// Main drawing method - draws a game state.
// Coordinates of all objects are in grid coords, so need to convert these to pixel
// coords in order to draw.
void draw_state(struct gamestate * state, unsigned int init_time) {
    // Draw DK...
    // draw_grid(state->dk, state->width, state->height);
    // Drawing of DK moved to DKmove.

    // Draw each enemy...
    for (int i = 0; i < state->num_enemies; ++i)
    {
        // Print state->enemies[i] with grid coords (x, y) at location (x * SCREENWIDTH/state->width, y * SCREENHEIGHT/state->height)
        if (state->enemies[i].exists) draw_grid(&(state->enemies[i]), state->width, state->height);
    }

    // Draw each pack...
    for (int i = 0; i < state->num_packs; ++i)
    {
        // Print state->packs[i] with grid coords (x, y) at location (x * SCREENWIDTH/state->width, y * SCREENHEIGHT/state->height)
        if (state->packs[i].exists && !(state->packs[i].trampled))
            draw_grid(&(state->packs[i]), state->width, state->height);
    }

    // Draw each vehicle...
    for (int i = 0; i < state->num_vehicles; ++i)
    {
        // Draw start...
        if (!(state->vehicles[i].start.trampled)) draw_grid(&(state->vehicles[i].start), state->width, state->height);
        // Draw finish...
        if (!(state->vehicles[i].finish.trampled))draw_grid(&(state->vehicles[i].finish), state->width, state->height);
    }

    // Draw the exit...
    if (!(state->exit.trampled))draw_grid(&(state->exit), state->width, state->height);

    // Update and print score...
    state->score = state->time + (250000 * state->lives) + (250000 * state->dk.num_coins_grabbed);
    draw_int(state->score, SCREENWIDTH, FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, FONT_HEIGHT, "SCORE:", 0xF);

    // Update and print time remaining...

    // First calculate time elapsed by this iteration of loop.
    init_time = (*clo - init_time) / 1000; // init_time is time elapsed in thousandths of a second.
    state->time -= init_time;

    // If state.time is now leq 0, set loseflag.
    if (state->time <= 0)
        state->loseflag = 1;

    draw_int(state->time, SCREENWIDTH, 2 * FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, 2 * FONT_HEIGHT, "TIME:", 0xF);

    // Print lives remaining... (replace with hearts later)
    draw_int(state->lives, SCREENWIDTH, 3 * FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, 3 * FONT_HEIGHT, "LIVES:", 0xF);

    // End of drawing gamestate.
}


// Print black at every cell of the gamestate.
void black_screen(struct gamestate *state)
{
    for (int i = 0; i < state->width; ++i)
    {
        for (int j = 0; j < state->height; ++j)
        {
            myDrawImage(black_image.pixel_data, black_image.width, black_image.height, grid_to_pixel_x(i, state->width), grid_to_pixel_y(j, state->height));
        }
    }
}


// Print platforms and ladders at required position in game state....
// This method will only be called at the very beginning of level. After that, platforms and ladders will be re-printed
// only in necessary cells after DK/enemies move.
void set_screen(struct gamestate *state)
{

    for(int i = 0; i < (state->width * state->height); ++i){
        if(state->map_tiles[i] == 0){
            draw_image(state->background, grid_to_pixel_x((i % 25), state->width), grid_to_pixel_y((i / 25), state->height));
        }
        else if(state->map_tiles[i] == 1){        //Draw Platform
            draw_image(state->platform, grid_to_pixel_x((i % 25), state->width), grid_to_pixel_y((i / 25), state->height));
        }
        else if(state->map_tiles[i] == 2){        //Draw Ladder
            draw_image(state->ladder, grid_to_pixel_x((i % 25), state->width), grid_to_pixel_y((i / 25), state->height));
        }
    }
}


// Erases every object in the gamestate.
void erase_state(struct gamestate *state) {
    // Erase DK...
    draw_image(state->background, grid_to_pixel_x(state->dk.loc.x, state->width), grid_to_pixel_y(state->dk.loc.y, state->height));

    // Erase each enemy...
    for (int i = 0; i < state->num_enemies; ++i)
    {
        draw_image(state->background, grid_to_pixel_x(state->enemies[i].loc.x, state->width), grid_to_pixel_y(state->enemies[i].loc.y, state->height));
    }

    // Erase each pack...
    for (int i = 0; i < state->num_packs; ++i)
    {
        draw_image(state->background, grid_to_pixel_x(state->packs[i].loc.x, state->width), grid_to_pixel_y(state->packs[i].loc.y, state->height));
    }

    // Erase each vehicle...
    for (int i = 0; i < state->num_vehicles; ++i)
    {
        draw_image(state->background, grid_to_pixel_x(state->vehicles[i].start.loc.x, state->width), grid_to_pixel_y(state->vehicles[i].start.loc.y, state->height));
        draw_image(state->background, grid_to_pixel_x(state->vehicles[i].finish.loc.x, state->width), grid_to_pixel_y(state->vehicles[i].finish.loc.y, state->height));
    }

    // Erase exit...
    draw_image(state->background, grid_to_pixel_x(state->exit.loc.x, state->width), grid_to_pixel_y(state->exit.loc.y, state->height));

    // Erase time, score, lives...
    drawRect(SCREENWIDTH - 200, 0, SCREENWIDTH + 50, 3*FONT_HEIGHT + 50, 0x0, 1);

    // Erase all ladders and platforms...
    for (int i = 0; i < state->width*state->height; ++i) {
        if (state->map_tiles[i] > 0) draw_image(state->background, grid_to_pixel_x(i % state->width, state->width), grid_to_pixel_y(i / state->width, state->height));
    }

}


// Draws background at specified coordinates. In particular, draws platform/ladder if specified in mapTiles,
// else draw black.
void draw_background(int x, int y, struct gamestate *state) {
    if (state->map_tiles[(state->width)*y + x] == 0) draw_image(state->background, grid_to_pixel_x(x, state->width), grid_to_pixel_y(y, state->height));
    else if (state->map_tiles[(state->width)*y + x] == 1) draw_image(state->platform, grid_to_pixel_x(x, state->width), grid_to_pixel_y(y, state->height));
    else draw_image(state->ladder, grid_to_pixel_x(x, state->width), grid_to_pixel_y(y, state->height));
}


//////////////////////////////////////////
// START MENU / LEVEL SELECTION METHODS //
//////////////////////////////////////////

/*
 * Start Menu option selection logic. Can either select "Start" or "Quit" in initial menu.
 * Returns 1 if start is selected, -1 if quit is selected, and 0 if nothing has been selected.
 */
int startMenuSelectOption(int *buttons, struct startMenu *start)
{
    if (buttons[4] == 0)
    {
        // JP Up pressed - move to "start" and erase current quit game string...
        (*start).startGameSelected = 1;
        (*start).quitGameSelected = 0;
        // Hover feature on "Start" Button
        drawString(300, 350, "             ", 0xF); // Erase "-> QUIT GAME"
    }
    else if (buttons[5] == 0)
    {
        // JP Down pressed - move to "quit" button and erase current start game string...
        (*start).startGameSelected = 0;
        (*start).quitGameSelected = 1;
        // Hover feature on "Quit" Button
        drawString(300, 300, "              ", 0xF); // Erase "-> START GAME"
    }

    if (buttons[8] == 0)
    {
        // 'A' is pressed
        if ((*start).startGameSelected)
        {
            // Begin game.
            return 1;
        }
        else if ((*start).quitGameSelected)
        {
            // Quit Game
            return -1;
        }
    }
    return 0;
}


// Draws start menu, runs start menu selection process.
// Returns start_flag - equal to 1 if start was selected, -1 if quit was selected.
int start_menu(struct startMenu *sm, int *buttons) {
    sm->startGameSelected = 1;
    sm->quitGameSelected = 0;
    int start_flag = 0;

    // Draw rectangle border...
    drawRect(SCREENWIDTH / 2 - 100, SCREENHEIGHT / 2 - 100, SCREENWIDTH / 2 + 100, SCREENHEIGHT / 2 + 100, 0xF, 0);

    while (start_flag == 0)
    {
        // Display selection options with currently selected option being pointed to...
        if (sm->startGameSelected)
        {
            drawString(SCREENWIDTH / 2 - 50, SCREENHEIGHT / 2 - 25, "-> START GAME", 0xF);
            drawString(SCREENWIDTH / 2 - 50, SCREENHEIGHT / 2 + 25, "   QUIT GAME", 0xF);
        }
        else if (sm->quitGameSelected)
        {
            drawString(SCREENWIDTH / 2 - 50, SCREENHEIGHT / 2 - 25, "   START GAME", 0xF);
            drawString(SCREENWIDTH / 2 - 50, SCREENHEIGHT / 2 + 25, "-> QUIT GAME", 0xF);
        }
        // Loop while startMenuSelectOption returns 0 - so breaks when player presses
        // A on either start or quit option.
        read_SNES(buttons);
        // Pass address of sm so that flag attributes can be modified by function.
        // Function will automatically erase text when selection moves up/down.
        start_flag = startMenuSelectOption(buttons, sm);
    }

    // Erase selection menu from screen...
    drawRect(SCREENWIDTH / 2 - 100, SCREENHEIGHT / 2 - 100, SCREENWIDTH / 2 + 100, SCREENHEIGHT / 2 + 100, 0x0, 1);

    return start_flag;
}


// Returns 0 to return to gameplay, 1 to exit game, 2 to restart.
int pause_menu(int *buttons, struct gamestate *state) {
    int restart_pressed = 1;
    int pressed_a = 0;
    int exit_game = 0;
    wait(500000); // Wait for a bit to stop menu from immediately closing.

    // Draw white rectangle border and black rectangle fill...
    drawRect(SCREENWIDTH / 2 - 75, SCREENHEIGHT / 2 - 50, SCREENWIDTH / 2 + 200, SCREENHEIGHT / 2 + 100, 0x0, 1);
    drawRect(SCREENWIDTH / 2 - 75, SCREENHEIGHT / 2 - 50, SCREENWIDTH / 2 + 200, SCREENHEIGHT / 2 + 100, 0xF, 0);

    while (!pressed_a)
    {
        // First, display pause menu...

        if (restart_pressed) {
            drawString(SCREENWIDTH / 2, SCREENHEIGHT / 2, "-> RESTART GAME", 0xF);
            drawString(SCREENWIDTH / 2, SCREENHEIGHT / 2 + 50, "   QUIT GAME", 0xF);
        } else {
                    drawString(SCREENWIDTH / 2, SCREENHEIGHT / 2, "   RESTART GAME", 0xF);
                    drawString(SCREENWIDTH / 2, SCREENHEIGHT / 2 + 50, "-> QUIT GAME", 0xF);
                }

        // Read controller...
        read_SNES(buttons);

        // If start is pressed again, wait for a bit and then break (to prevent pause
        // menu from immediately opening again).
        if (buttons[4 - 1] == 0)
        {
            wait(500000);
            break;
        }

        else if (buttons[5 - 1] == 0)
        {
            // Up is pressed.
            restart_pressed = 1;
        }

        else if (buttons[6 - 1] == 0)
        {
            // Down is pressed.
            restart_pressed = 0;
        }

        else if (buttons[9 - 1] == 0)
            {
                // A is pressed, execute current selection.
                pressed_a = 1;
            }
    }

    // Erase pause menu...
    drawRect(SCREENWIDTH / 2 - 75, SCREENHEIGHT / 2 - 50, SCREENWIDTH / 2 + 200, SCREENHEIGHT / 2 + 100, 0x0, 1);
    drawString(SCREENWIDTH / 2, SCREENHEIGHT / 2, "                ", 0xF);
    drawString(SCREENWIDTH / 2, SCREENHEIGHT / 2 + 50, "              ", 0xF);

    if (pressed_a) {
        // Execute current selection. Otherwise, start was pressed to exit game menu and
        // we continue playing the game.

        // No matter what was selected, we should erase the game state before either restarting or exiting.

        erase_state(state);

        if (restart_pressed) exit_game = 2; // Restart first level.
        else {
            drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT / 2, "Exiting...", 0xF);
            wait(2000000);
            drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT / 2, "          ", 0xF);
            exit_game = 1;
        }
    }
    return exit_game;
}


////////////////////////////////
// MOVEMENT/GAMPLAY FUNCTIONS //
////////////////////////////////

// Moves DK based on buttons pressed in passed array.
void DKmove(int *buttons, struct gamestate *state)
{

    int pressed = 0;

    // Record old coordinates so that background can be drawn there.
    int oldx = (*state).dk.loc.x;
    int oldy = (*state).dk.loc.y;

    // Record hypothetical "moved to" location, we'll check that this cell is actually valid
    // before moving...
    int newx = oldx;
    int newy = oldy;

    if (buttons[7] == 0)
    { // Right
        if (oldx + (*state).dk.speed <= (*state).width - 1)
        {
            // Ensure that DK does not step outside of screen
            // uart_puts("Right\n");
            pressed = 7;
            newx = (*state).dk.loc.x + (*state).dk.speed;
            (*state).dk.enemy_direction = 1;
        }
    }

    else if (buttons[6] == 0)
    { // Left
        if (oldx - (*state).dk.speed >= 0)
        {
            // Ensure that DK does not step outside of screen
            // uart_puts("Left\n");
            pressed = 6;
            newx = (*state).dk.loc.x - (*state).dk.speed;
            (*state).dk.enemy_direction = 0;
        }
    }

    else if (buttons[4] == 0)
    { // Up
        if (oldy - (*state).dk.speed >= 0)
        {
            // Ensure that DK does not step outside of screen
            // uart_puts("Up\n");
            pressed = 4;
            newy = (*state).dk.loc.y - (*state).dk.speed;
            (*state).dk.enemy_direction = 2;
        }
    }

    else if (buttons[5] == 0)
    { // Down
        if (oldy + (*state).dk.speed <= (*state).height - 1)
        {
            // Ensure that DK does not step outside of screen
            // uart_puts("Down\n");
            pressed = 5;
            newy = (*state).dk.loc.y + (*state).dk.speed;
            (*state).dk.enemy_direction = 2;
        }
    }

    // If the cell DK wants to move to (may be current cell) is valid, update drawing and position of DK.
    if (is_valid_cell(newx, newy, state)) {

        // uart_puts("Valid\n");

        // Move DK to new valid cell...
        state->dk.loc.x = newx;
        state->dk.loc.y = newy;

        // If DK moved, he loses his immunity and we redraw the background at his old location.
        // If DK hasn't moved we don't redraw the background - this is to prevent DK from fading
        // in and out of black.
        if (pressed > 0)
        {
            (*state).dk.dk_immunity = 0;

            // Redraw background at old location... will depend on whether cell is occupied by a ladder, platform, or nothing.
            draw_background(oldx, oldy, state);
            // draw_image((*state).background, grid_to_pixel_x(oldx, (*state).width), grid_to_pixel_y(oldy, (*state).height));
        }

        // Regardless of whether DK moved, draw him at his new location...
        draw_grid(&((*state).dk), (*state).width, (*state).height);
        // draw_image((*state).dk.sprite, (*state).dk.loc.x * (SCREENWIDTH / (*state).width), (*state).dk.loc.y * (SCREENHEIGHT / (*state).height));

    }

    // Else, cell is invalid - do not move DK.
    // I think that I've set this up so that DKs sprite will still update when he tries to move to a non-valid cell,
    // which I think is what we want.
    else {
        // Draw DK at his old (and current) location....
        draw_grid(&(state->dk), state->width, state->height);
    }
}


// Updates DKs sprite to be consistent with the direction he's facing.
// Flag indicates whether to use the first or second sprite.
void updateDKdirection(struct gamestate *state, int flag) {
    if (state->dk.enemy_direction == 1) {
        // Facing right...
        if (flag) state->dk.sprite.img = (unsigned char*) dk_right1.pixel_data;
        else state->dk.sprite.img = (unsigned char*) dk_right2.pixel_data;
    } else if (state->dk.enemy_direction == 0) {
        // Facing left...
        if (flag) state->dk.sprite.img = (unsigned char*) dk_left1.pixel_data;
        else state->dk.sprite.img = (unsigned char*) dk_left2.pixel_data;
    } else if (state->dk.enemy_direction == 2) {
        if (flag) state->dk.sprite.img = (unsigned char*) dk_ladder1.pixel_data;
        else state->dk.sprite.img = (unsigned char*) dk_ladder2.pixel_data;
    }
}


// Updates enemy sprite to be consistent with direction being faced.
// Flag indicates whether to use first or second sprite.
void updateEnemyDirection(struct object *enemy, int flag) {
    if (!enemy->flying) {
        // Non-flying enemy sprites...
        if (enemy->enemy_direction == 1) {
            // Facing right
            // First right sprite
            if (flag) enemy->sprite.img = (unsigned char*) mario_right1.pixel_data;
            // Second right sprite
            else enemy->sprite.img = (unsigned char*) mario_right2.pixel_data;

        } else if (enemy->enemy_direction == 0) {
            // Facing left
            // First left sprite
            if (flag) enemy->sprite.img = (unsigned char*) mario_left1.pixel_data;
            // Second left sprite
            else enemy->sprite.img = (unsigned char*) mario_left2.pixel_data;
        }
    } else {
        // Flying enemy sprites...
        if (enemy->enemy_direction == 1) {
            // Facing right
            // First right sprite
            if (flag) enemy->sprite.img = (unsigned char*) bird_right1.pixel_data;
            // Second right sprite
            else enemy->sprite.img = (unsigned char*) bird_right3.pixel_data;

        } else if (enemy->enemy_direction == 0) {
            // Facing left
            // First left sprite
            if (flag) enemy->sprite.img = (unsigned char*) bird_left1.pixel_data;
            // Second left sprite
            else enemy->sprite.img = (unsigned char*) bird_left2.pixel_data;
        }
    }
}



/*
 * This method manages a boomerang object once it has been created.
 * It manages the direction, location and object interaction between
 * boomerang and enemies.
 */
void updateBoomerang(struct gamestate *state)
{
    // Store old location of boomerang for drawing purposes
    int oldx = (*state).boomerang.loc.x;
    int oldy = (*state).boomerang.loc.y;

    // Move boomerang in specified direction.
    if ((*state).boomerang.direction == 0 && (*state).boomerang.loc.x != 0)
    {
        (*state).boomerang.loc.x--;
            // If boomerang hits either edge of the map, reverse direction
        if ((*state).boomerang.loc.x <= 0)
        {
            (*state).boomerang.direction = 1;

        }
    }
    else if ((*state).boomerang.direction == 1 && (*state).boomerang.loc.x != (*state).width)
    {
        (*state).boomerang.loc.x++;

        if ((*state).boomerang.loc.x >= (*state).width)
        {
            (*state).boomerang.direction = 0;
        }

    }
    // See if boomerang has hit any enemy, if so, kills enemy and reverses direction
    for (int i = 0; i < (*state).num_enemies; ++i)
    {
        if ((*state).boomerang.loc.x == (*state).enemies[i].loc.x && (*state).boomerang.loc.y == (*state).enemies[i].loc.y)
        {
            (*state).boomerang.register_hit = 1;
            (*state).enemies[i].exists = 0;
            if ((*state).boomerang.direction == 1)
            {
                (*state).boomerang.direction = 0;
            }
            else if ((*state).boomerang.direction == 0)
            {
                (*state).boomerang.direction = 1;
            }
        }
    }

    // Check if boomerang has hit dk. If so, dk now "has Boomerang", boomerang object does not exist.
    if ((*state).boomerang.loc.x == (*state).dk.loc.x && (*state).boomerang.loc.y == (*state).dk.loc.y)
    {
        (*state).boomerang.exists = 0;
        (*state).dk.has_boomerang = 1;
    }

    if ((*state).boomerang.sprite.img == bananarang.pixel_data)
    {
        (*state).boomerang.sprite.img = (unsigned char*) bananarang2.pixel_data;
    }
    else if ((*state).boomerang.sprite.img == bananarang2.pixel_data)
    {
        (*state).boomerang.sprite.img = (unsigned char*) bananarang3.pixel_data;
    }
    else if ((*state).boomerang.sprite.img == bananarang3.pixel_data)
    {
        (*state).boomerang.sprite.img = (unsigned char*) bananarang.pixel_data;
    }

    // Draw boomerang if current position is not dk's position.
    if ((*state).boomerang.loc.x != (*state).dk.loc.x)
    {
        draw_image((*state).boomerang.sprite, grid_to_pixel_x((*state).boomerang.loc.x, (*state).width), grid_to_pixel_y((*state).boomerang.loc.y, (*state).height));
    }
    // Draw background if previous position is not dk's position (as to not erase dk)
    if (oldx != (*state).dk.loc.x)
    {
        draw_background(oldx, oldy, state);
    }
}


// Checks for collisions with DK in the gamestate. Updates gamestate accordingly.
void checkDKCollisions(struct gamestate *state) {
    // Check to see if DK has collided with an enemy. DK can only be hurt if his immunity is turned off.
    if (!(state->dk.dk_immunity))
    {
        for (int i = 0; i < state->num_enemies; ++i)
        {
            if(state->enemies[i].exists){
                if (state->dk.loc.x == state->enemies[i].loc.x && state->dk.loc.y == state->enemies[i].loc.y)
                {
                    --state->lives;
                    // printf("Lost a life\n");
                    // Give DK immunity - he can't be hurt until he leaves this cell.
                    state->dk.dk_immunity = 1;
                    // Set lose flag if out of lives.
                    if (state->lives == 0)
                        state->loseflag = 1;
                }
            }
        }
    }

    // Check to see if DK has collided with a pack...
    for (int i = 0; i < state->num_packs; ++i)
    {
        if (state->dk.loc.x == state->packs[i].loc.x && state->dk.loc.y == state->packs[i].loc.y && state->packs[i].exists)
        {
            // See which kind of pack DK has collided with, update gamestate accordingly.
            if (state->packs[i].health_pack)
            {
                // Give DK an extra life if he has less than 4.
                if (state->lives < 4)
                    ++state->lives;
            }
            else if (state->packs[i].point_pack)
            {
                ++state->dk.num_coins_grabbed;
            }
            else if (state->packs[i].boomerang_pack)
            {
                state->dk.has_boomerang = 1;
            }

            // Remove pack from stage.
            state->packs[i].exists = 0;
        }
    }

    // Check to see if DK has collided with a vehicle...
    // To prevent DK from teleporting back and forth using bidirectional vehicles, set dk_immunity after DK teleports.
    // dk_immunity won't be turned off until DK moves from the vehicle cell.
    if (!state->dk.dk_immunity)
    {
        for (int i = 0; i < state->num_vehicles; ++i)
        {

            // Check for collision with start...
            if (state->dk.loc.x == state->vehicles[i].start.loc.x && state->dk.loc.y == state->vehicles[i].start.loc.y)
            {

                // TO-DO: Insert vehicle animations (vine swinging, etc) if we have the time and ability

                // Update location of DK to finish location of vehicle...
                state->dk.loc.x = state->vehicles[i].finish.loc.x;
                state->dk.loc.y = state->vehicles[i].finish.loc.y;

                state->dk.dk_immunity = 1; // Set immunity.
            }

            // Check for collision with finish (only teleports DK if vehicle is bidirectional)
            // Added else to this conditional so that DK cannot teleport start -> finish and then immediately finish -> start using a bidirectional vehicle.
            else if (state->dk.loc.x == state->vehicles[i].finish.loc.x && state->dk.loc.y == state->vehicles[i].finish.loc.y && state->vehicles[i].bidirectional)
            {

                // TO-DO: Insert vehicle animations if we have time and ability.

                // Update location of DK to start location of vehicle...
                state->dk.loc.x = state->vehicles[i].start.loc.x;
                state->dk.loc.y = state->vehicles[i].start.loc.y;

                state->dk.dk_immunity = 1; // Set immunity.
            }
        }
    }
}


// Check to see if the passed coordinates are a "valid cell" for DK to move to.
// There are 3 kinds of valid cells:
// A cell is valid if it is occupied by a ladder.
// A cell is valid if it is directly above a cell occupied by a platform.
// A cell is valid if it is occupied by a platform and is directly above a ladder.
int is_valid_cell(int x, int y, struct gamestate *state) {
    int is_valid;
    // If cell is ladder, valid...
    if (state->map_tiles[(state->width)*y + x] == 2) is_valid = 1;
    // OK. IF CELL IS NOT A LADDER AND y = state->height, INVALID. OTHERWISE THE TESTS BELOW WILL ACCESS ELEMENTS
    // OUTSIDE OF ARRAY AND UNEXPECTED CELLS WILL SHOW UP AS VALID.
    else if (y == (state->height - 1)) is_valid = 0;
    // If cell is above a platform, valid...
    else if (state->map_tiles[(state->width)*(y + 1) + x] == 1) is_valid = 1;
    else if (state->map_tiles[(state->width)*y + x] == 1 && state->map_tiles[25*(y + 1) + x] == 2) is_valid = 1;
    else is_valid = 0;
    return is_valid;
}


// Spawns a pack randomly in the gamestate.
// Random location is simulated by the clock register.
// If flag is 1, spawn a health pack. If 0, spawn a point pack.
void spawn_pack(struct gamestate *state, int flag) {

    if (state->num_packs < MAXOBJECTS) {

        // Only spawn a pack if number of packs on screen less than MAXOBJECTS.

        // Need to determine a valid location for the pack.
        int x = 0;
        int y = state->height - 1;  // Initialized to an invalid cell...
        int pack_there = 0;         // Flag indicating that there is a pack at the indicated location.
        int is_ladder = 0;          // Flag indicating that the location is a ladder (DO NOT SPAWN ON LADDERS)

        while (!is_valid_cell(x, y, state) || pack_there || is_ladder) {
            // Select a random cell on the game map until a valid cell is found...
            x = *clo % state->width;
            y = *clo % state->height;

            // Check if there is a pack at (x, y)... (for now only checking for pack collisions,
            // can still spawn at vehicle or exit locations)
            pack_there = 0;
            for (int i = 0; i < state->num_packs; ++i) {
                if (state->packs[i].loc.x == x && state->packs[i].loc.y == y) {
                    pack_there = 1;
                    break;
                }
            }

            // Check to see if cell is occupied by a ladder.
            is_ladder = 0;
            if (state->map_tiles[state->width * y + x] == 2) is_ladder = 1;
        }

        ++state->num_packs;
        
        state->packs[state->num_packs - 1].loc.x = x;
        state->packs[state->num_packs - 1].loc.y = y;

        state->packs[state->num_packs - 1].exists = 1;
        state->packs[state->num_packs - 1].trampled = 0;

        if (flag) {
            // Spawn a health pack at coordinates (x, y)...
            state->packs[state->num_packs - 1].sprite.img = heartpack.pixel_data;
            state->packs[state->num_packs - 1].sprite.width = heartpack.width;
            state->packs[state->num_packs - 1].sprite.height = heartpack.height;
            state->packs[state->num_packs - 1].health_pack = 1;
            state->packs[state->num_packs - 1].point_pack = 0;
            state->packs[state->num_packs - 1].boomerang_pack = 0;
        } else {
            // Spawn a point pack at coordinates (x, y)...
            state->packs[state->num_packs - 1].sprite.img = coinpack.pixel_data;
            state->packs[state->num_packs - 1].sprite.width = coinpack.width;
            state->packs[state->num_packs - 1].sprite.height = coinpack.height;
            state->packs[state->num_packs - 1].health_pack = 0;
            state->packs[state->num_packs - 1].point_pack = 1;
            state->packs[state->num_packs - 1].boomerang_pack = 0;
        }

        uart_puts("Pack spawned...\n");

    }

}


//////////
// MAIN //
//////////

int main()
{

    // Define printf using provided uart methods.
    void printf(char *str)
    {
        uart_puts(str);
    }

    uart_puts("Running\n");

    /////////////////////////////
    // First, set up driver... //
    /////////////////////////////

    // Initialize buttons array to all 1s.
    int buttons[16];
    for (int i = 0; i < 16; ++i)
        buttons[i] = 1;

    // Initialize SNES lines and frame buffer.
    init_snes_lines();
    fb_init();

    uart_puts("Initialized\n");

    /////////////////////////////////
    // START MENU - BASICALLY DONE //
    /////////////////////////////////

    struct startMenu sm;
    struct gamestate state;
    int start_flag;

start_menu:

    start_flag = start_menu(&sm, buttons);

    if (start_flag == -1)
    {
        // Quit game...
        drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT / 2, "Exiting game...", 0xF);
        printf("Quitting game...\n");
        wait(1000000);
        drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT / 2, "                ", 0xF); // Erase message from screen.
        return 1;
    }

    drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT / 2, "Starting game...", 0xF);
    printf("Starting game...\n");
    wait(1000000);
    drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT / 2, "                 ", 0xF); // Erase message from screen.

    /////////////////
    // FIRST STAGE //
    /////////////////

    goto third_stage;

first_stage:

    state.width = 25;
    state.height = 25;

    state.score = 0;
    state.lives = 4;
    state.time = 1000000; // Display time in thousandths of a second
    state.map_selection = 1;

    state.winflag = 0;
    state.loseflag = 0;

    // Background images for first stage...

    state.background.img = (unsigned char*) black_image.pixel_data;
    state.background.width = black_image.width;
    state.background.height = black_image.height;

    state.platform.img = (unsigned char*) platform.pixel_data;
    state.platform.width = 32;
    state.platform.height = 32;

    state.ladder.img = (unsigned char*) ladder.pixel_data;
    state.ladder.width = ladder.width;
    state.ladder.height = ladder.height;

    // DK data...

    state.dk.sprite.img = (unsigned char*) dk_right0.pixel_data; // Initial image of DK will be standing, facing right.
    state.dk.sprite.width = dk_right0.width;
    state.dk.sprite.height = dk_right0.height;

    state.dk.loc.x = 3;
    state.dk.loc.y = 24;

    state.dk.speed = 1;
    state.dk.dk_immunity = 0;
    state.dk.num_coins_grabbed = 0;
    state.dk.has_boomerang = 0;
    
    state.dk.trampled = 0;

    // Enemies

    state.num_enemies = 2;

    for (int i = 0; i < 2; ++i)
    {
        state.enemies[i].sprite.img = (unsigned char*) mario_right1.pixel_data;
        state.enemies[i].sprite.width = mario_right1.width;
        state.enemies[i].sprite.height = mario_right1.height;

        state.enemies[i].speed = 1;
        state.enemies[i].enemy_direction = 0;
        state.enemies[i].exists = 1;
        
        state.enemies[i].trampled = 0;
    }

    state.enemies[0].loc.x = 21;
    state.enemies[0].loc.y = 14;

    state.enemies[1].loc.x = 21;
    state.enemies[1].loc.y = 20;

    // Packs

    state.num_packs = 4;

    // Health packs...

    for (int i = 0; i < 1; ++i)
    {
        state.packs[i].sprite.img = (unsigned char*) heartpack.pixel_data;
        state.packs[i].sprite.width = heartpack.width;
        state.packs[i].sprite.height = heartpack.height;

        state.packs[i].health_pack = 1;
        state.packs[i].point_pack = 0;
        state.packs[i].exists = 1;
    }

    state.packs[0].loc.x = 2;
    state.packs[0].loc.y = 2;

    // Point packs...

    for (int i = 1; i < 3; ++i)
    {
        state.packs[i].sprite.img = (unsigned char*) coinpack.pixel_data;
        state.packs[i].sprite.width = coinpack.width;
        state.packs[i].sprite.height = coinpack.height;

        state.packs[i].health_pack = 0;
        state.packs[i].point_pack = 1;
        state.packs[i].boomerang_pack = 0;
        state.packs[i].exists = 1;
    }

    state.packs[1].loc.x = 21;
    state.packs[1].loc.y = 14;
    state.packs[2].loc.x = 4;
    state.packs[2].loc.y = 14;

    // Create boomerang pack
    state.packs[3].sprite.img = (unsigned char*) bananarangpack.pixel_data;
    state.packs[3].sprite.width = bananarangpack.width;
    state.packs[3].sprite.height = bananarangpack.height;

    state.packs[3].health_pack = 0;
    state.packs[3].point_pack = 0;
    state.packs[3].boomerang_pack = 1;
    state.packs[3].exists = 1;

    state.packs[3].loc.x = 22;
    state.packs[3].loc.y = 20;

    // Boomerang setup;

    state.boomerang.sprite.img = (unsigned char*) bananarang.pixel_data;
    state.boomerang.sprite.width = bananarang.width;
    state.boomerang.sprite.height = bananarang.height;

    state.boomerang.tiles_per_second = 20;
    state.boomerang.exists = 0;    // Projectile not in game yet.
    state.boomerang.direction = 1; // 1 = right, 0 = left
    
    for (int i = 0; i < state.num_packs; ++i) state.packs[i].trampled = 0;
    
    
    // Vehicles...

    state.num_vehicles = 4;

    // First vehicle... maybe a vine? For now using health pack image.
    // This one will be unidirectional...
    state.vehicles[0].start.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[0].start.sprite.width = teleporter.width;
    state.vehicles[0].start.sprite.height = teleporter.height;
    state.vehicles[0].finish.sprite.img = (unsigned char*) emptypack.pixel_data;
    state.vehicles[0].finish.sprite.width = emptypack.width;
    state.vehicles[0].finish.sprite.height = emptypack.height;

    state.vehicles[0].start.loc.x = 16;
    state.vehicles[0].start.loc.y = 10;

    state.vehicles[0].finish.loc.x = 16;
    state.vehicles[0].finish.loc.y = 5;

    state.vehicles[0].bidirectional = 0;

    // Second vehicle... maybe a vine? For now using health pack image.
    // This one will be unidirectional...
    state.vehicles[1].start.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[1].start.sprite.width = teleporter.width;
    state.vehicles[1].start.sprite.height = teleporter.height;
    state.vehicles[1].finish.sprite.img = (unsigned char*) emptypack.pixel_data;
    state.vehicles[1].finish.sprite.width = emptypack.width;
    state.vehicles[1].finish.sprite.height = emptypack.height;

    state.vehicles[1].start.loc.x = 10;
    state.vehicles[1].start.loc.y = 5;

    state.vehicles[1].finish.loc.x = 20;
    state.vehicles[1].finish.loc.y = 1;

    state.vehicles[1].bidirectional = 0;
    
    // Third vehicle... maybe a vine? For now using health pack image.
    // This one will be unidirectional...
    state.vehicles[2].start.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[2].start.sprite.width = teleporter.width;
    state.vehicles[2].start.sprite.height = teleporter.height;
    state.vehicles[2].finish.sprite.img = (unsigned char*) emptypack.pixel_data;
    state.vehicles[2].finish.sprite.width = emptypack.width;
    state.vehicles[2].finish.sprite.height = emptypack.height;

    state.vehicles[2].start.loc.x = 21;
    state.vehicles[2].start.loc.y = 5;

    state.vehicles[2].finish.loc.x = 23;
    state.vehicles[2].finish.loc.y = 20;

    state.vehicles[2].bidirectional = 0;

    // Fourth vehicle... bidirectional.

    state.vehicles[3].start.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[3].start.sprite.width = teleporter.width;
    state.vehicles[3].start.sprite.height = teleporter.height;
    state.vehicles[3].finish.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[3].finish.sprite.width = teleporter.width;
    state.vehicles[3].finish.sprite.height = teleporter.height;

    state.vehicles[3].start.loc.x = 7;
    state.vehicles[3].start.loc.y = 2;

    state.vehicles[3].finish.loc.x = 7;
    state.vehicles[3].finish.loc.y = 10;

    state.vehicles[3].bidirectional = 1;
    
    for (int i = 0; i < state.num_vehicles; ++i) {
        state.vehicles[i].start.trampled = 0;
        state.vehicles[i].finish.trampled = 0;
    }

    // Exit... will be located at coords (5, 0), middle of top of screen...
    // Set up exit... (temporarily using coin image)
    state.exit.sprite.img = (unsigned char*) ladder.pixel_data;
    state.exit.sprite.width = ladder.width;
    state.exit.sprite.height = ladder.height;

    // Exit for first stage is top of ladder leading out of screen...
    state.exit.loc.x = 17;
    state.exit.loc.y = 0;
    state.exit.exists = 1;
    state.exit.trampled = 0;

    for (int i = 0; i < 25*25; ++i) state.map_tiles[i] = map1[i];

    //////////////////////
    // FIRST STAGE LOOP //
    //////////////////////

    unsigned int time0;
    unsigned int enemy_move_reference_time = *clo;
    unsigned int dk_sprite_change_reference = *clo;
    unsigned int boomerang_reference = *clo;
    unsigned int pack_spawn_timer = *clo;

    int dk_sprite_change_interval = 500000; // Suppose to be 0.5 second
    int enemy_move_delay = 1000000;         // Suppose to be 1 second
    int pack_spawn_delay = 10000000;        // Spawn a pack every 10 seconds.

gameloop:

    state.dk.sprite_tracker = 1;
    for (int i = 0; i < state.num_enemies; ++i) {
        state.enemies[i].sprite_tracker = 1;
    }

    // Set screen...
    set_screen(&state);

    // this loop will run while we're in the first level - break if either win flag or lose flag is set.
    while (!state.winflag && !state.loseflag)
    {

        // Record clock register contents so that we can calculate the time elapsed by each iteration of this loop,
        // then subtract that from the amount of time remaining.
        time0 = *clo;

        // Read controller.
        read_SNES(buttons);

        // If start has been pressed, enter pause menu...
        if (buttons[4 - 1] == 0) {
            int exit_game = pause_menu(buttons, &state);
            // If exit_game selected, print message and exit.
            if (exit_game == 1) {
                drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT / 2, "Exiting...", 0xF);
                wait(1000000);
                drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT / 2, "          ", 0xF);
                return 1;
            } else if (exit_game == 2) {
                // Restart from first stage.
                goto first_stage;
            }

            // Otherwise, start was pressed. Redraw game state in case anything was erased by pause menu.
            set_screen(&state);

        }

        // This block of code is entered every 0.5 seconds.
        // Flips spriteTracker flag
        // UPDATE - FLIPS FOR BOTH DK AND ENEMIES
        if (dk_sprite_change_reference + dk_sprite_change_interval <= *clo) {
            // Change sprite of DK.
            state.dk.sprite_tracker = 1 - state.dk.sprite_tracker;
            for (int i = 0; i < state.num_enemies; ++i) {
                state.enemies[i].sprite_tracker = 1 - state.enemies[i].sprite_tracker;
            }
            // Reset reference...
            dk_sprite_change_reference = *clo;
        }

        // Update direction being faced by DK...
        updateDKdirection(&state, state.dk.sprite_tracker);

        // Update enemy direction being faced by enemy.
        for (int i = 0; i < state.num_enemies; ++i) {
            updateEnemyDirection(&state.enemies[i], state.enemies[i].sprite_tracker);
        }

        // Move DK based on SNES input.
        DKmove(buttons, &state);

        // Check for collisions...
        checkDKCollisions(&state);

        // If sufficient time has elapsed, move enemies...
        if (enemy_move_reference_time + enemy_move_delay <= time0)
        {
            for (int i = 0; i < state.num_enemies; i++)
            {
                if (state.enemies[i].exists) {

                    // Move enemy i.

                    int oldx = state.enemies[i].loc.x;
                    int oldy = state.enemies[i].loc.y;

                    int newx = oldx;

                    if (state.enemies[i].enemy_direction == 0)
                    {
                        newx -= 1;
                    }
                    else
                    {
                        newx += 1;
                    }

                    if (!state.enemies[i].flying) {
                        // If enemy is not flying, check if new location is a valid cell.
                        // If invalid, turn enemy around.
                        if (is_valid_cell(newx, oldy, &state)) {
                            state.enemies[i].loc.x = newx;
                        } else {
                            state.enemies[i].enemy_direction = 1 - state.enemies[i].enemy_direction;
                        }
                    } else {
                        // Otherwise, enemy is flying - turn around at the edge of screen.
                        if (newx > -1 && newx < state.width) {
                            // Valid move.
                            state.enemies[i].loc.x = newx;
                        } else {
                            state.enemies[i].enemy_direction = 1 - state.enemies[i].enemy_direction;
                        }
                    }

                    // Draw enemy at new location and erase at old location.
                    draw_background(oldx, oldy, &state);
                    // draw_image(state.background, grid_to_pixel_x(oldx, state.width), grid_to_pixel_y(oldy, state.height));
                    draw_grid(&(state.enemies[i]), state.width, state.height);
                }
                
            }
            enemy_move_reference_time = *clo;
        }

        // Boomerang logic
        if (buttons[8] == 0)
        {
            // draw_image() underneath score, boomerang icon
            if (state.dk.has_boomerang)
            {
                
                if (state.dk.enemy_direction != 2)
                {
                    state.boomerang.direction = state.dk.enemy_direction;
                    state.boomerang.loc = state.dk.loc;
                    state.dk.has_boomerang = 0;
                    state.boomerang.exists = 1;
                }
            }
        }

        if (state.boomerang.exists)
        {
            if (boomerang_reference + (enemy_move_delay / state.boomerang.tiles_per_second) <= time0)
            {
                boomerang_reference = *clo;
                updateBoomerang(&state);
            }
        }

        // Check to see if DK has reached the exit, set winflag if he has...
        if (state.dk.loc.x == state.exit.loc.x && state.dk.loc.y == state.exit.loc.y)
        {
            state.winflag = 1;
            state.exit.exists = 0;
        }

        // Check to see if 30 seconds have elapsed since the last pack was spawned. If so, spawn a pack...
        if (pack_spawn_timer + pack_spawn_delay <= *clo) {
            // flag = *clo % 2 to simulate random spawn of either health or point pack.
            spawn_pack(&state, *clo % 2);
            // Reset spawn pack timer.
            pack_spawn_timer = *clo;
        }


        // draw game state.
        draw_state(&state, time0);

        // Lastly, wait for a brief period before executing loop body again. Quick fix to slow down
        // DK when holding down Jpad.
        wait(100000);
    }

    // First stage exited...

    // Before doing anything else, clear the screen...
    erase_state(&state);

    // If lost, print game over and return to menu.
    if (state.loseflag)
    {
        drawString(SCREENWIDTH/2 - 25, SCREENHEIGHT/2, "Game over!", 0xF);
        wait(2000000);
        drawString(SCREENWIDTH/2 - 25, SCREENHEIGHT/2, "           ", 0xF);

        // Copyable code to erase score/time/lives counters (functions which call drawString aren't working, so I can't make this into a function):

        // Erase time, score, lives before terminating...
        drawString(SCREENWIDTH - 200, FONT_HEIGHT, "       ", 0xF);     // Erase "SCORE:"
        drawString(SCREENWIDTH - 200, 2 * FONT_HEIGHT, "      ", 0xF);  // Erase "TIME:"
        drawString(SCREENWIDTH - 200, 3 * FONT_HEIGHT, "       ", 0xF); // ERASE "LIVES:"

        // Erase numbers (big buffer used, during testing time counter has been holding some crazy values):
        drawString(SCREENWIDTH - 100, FONT_HEIGHT, "                        ", 0xF);
        drawString(SCREENWIDTH - 100, 2 * FONT_HEIGHT, "                        ", 0xF);
        drawString(SCREENWIDTH - 100, 3 * FONT_HEIGHT, "                        ", 0xF);

        state.loseflag = 0;

        goto start_menu;
    }

    // If we didn't enter that code block, first stage won! Move on to next stage, but first erase score/times/lives...

    // Erase time, score, lives before terminating...
    drawString(SCREENWIDTH - 200, FONT_HEIGHT, "       ", 0xF);     // Erase "SCORE:"
    drawString(SCREENWIDTH - 200, 2 * FONT_HEIGHT, "      ", 0xF);  // Erase "TIME:"
    drawString(SCREENWIDTH - 200, 3 * FONT_HEIGHT, "       ", 0xF); // ERASE "LIVES:"

    // Erase numbers (big buffer used, during testing time counter has been holding some crazy values):
    drawString(SCREENWIDTH - 100, FONT_HEIGHT, "                        ", 0xF);
    drawString(SCREENWIDTH - 100, 2 * FONT_HEIGHT, "                        ", 0xF);
    drawString(SCREENWIDTH - 100, 3 * FONT_HEIGHT, "                        ", 0xF);

    drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT/2, "Stage won!", 0xF);
    wait(1000000);
    drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT/2, "                 ", 0xF);
    
    // Determine next stage...
    state.winflag = 0;
    state.map_selection ++;

    if(state.map_selection == 3){
        goto third_stage;
    }
    if(state.map_selection == 4){
        goto fourth_stage;
    }
    if (state.map_selection == 5) {
        // Game won!
        goto game_won;
    }

    // Move on to second stage...
second_stage:

    state.dk.loc.x = 17;
    state.dk.loc.y = 24;

    state.dk.speed = 1;
    state.dk.dk_immunity = 0;
    state.dk.has_boomerang = 0;
    
    state.dk.trampled = 0;

    // Enemies

    state.num_enemies = 2;

    for (int i = 0; i < 2; ++i)
    {
        state.enemies[i].sprite.img = (unsigned char*) enemy_image.pixel_data;
        state.enemies[i].sprite.width = enemy_image.width;
        state.enemies[i].sprite.height = enemy_image.height;

        state.enemies[i].speed = 1;
        state.enemies[i].enemy_direction = 0;
        state.enemies[i].exists = 1;
        
        state.enemies[i].trampled = 0;
    }

    state.enemies[0].loc.x = 10;
    state.enemies[0].loc.y = 12;

    state.enemies[1].loc.x = 15;
    state.enemies[1].loc.y = 18;

    // Packs

    state.num_packs = 4;

    // Health packs...

    for (int i = 0; i < 1; ++i)
    {
        state.packs[i].sprite.img = (unsigned char*) heartpack.pixel_data;
        state.packs[i].sprite.width = heartpack.width;
        state.packs[i].sprite.height = heartpack.height;

        state.packs[i].health_pack = 1;
        state.packs[i].point_pack = 0;
        state.packs[i].exists = 1;
    }

    state.packs[0].loc.x = 2;
    state.packs[0].loc.y = 21;

    // Point packs...

    for (int i = 1; i < 3; ++i)
    {
        state.packs[i].sprite.img = (unsigned char*) coinpack.pixel_data;
        state.packs[i].sprite.width = coinpack.width;
        state.packs[i].sprite.height = coinpack.height;

        state.packs[i].health_pack = 0;
        state.packs[i].point_pack = 1;
        state.packs[i].boomerang_pack = 0;
        state.packs[i].exists = 1;
    }

    state.packs[1].loc.x = 22;
    state.packs[1].loc.y = 20;
    state.packs[2].loc.x = 1;
    state.packs[2].loc.y = 21;

    // Create boomerang pack
    state.packs[3].sprite.img = (unsigned char*) bananarangpack.pixel_data;
    state.packs[3].sprite.width = bananarangpack.width;
    state.packs[3].sprite.height = bananarangpack.height;

    state.packs[3].health_pack = 0;
    state.packs[3].point_pack = 0;
    state.packs[3].boomerang_pack = 1;
    state.packs[3].exists = 1;

    state.packs[3].loc.x = 23;
    state.packs[3].loc.y = 14;

    // Boomerang setup;

    state.boomerang.sprite.img = (unsigned char*) bananarang.pixel_data;
    state.boomerang.sprite.width = bananarang.width;
    state.boomerang.sprite.height = bananarang.height;

    state.boomerang.tiles_per_second = 20;
    state.boomerang.exists = 0;    // Projectile not in game yet.
    state.boomerang.direction = 1; // 1 = right, 0 = left
    
    for (int i = 0; i < state.num_packs; ++i) state.packs[i].trampled = 0;
    
    
    // Vehicles...

    state.num_vehicles = 6;

    // First vehicle... maybe a vine? For now using health pack image.
    // This one will be unidirectional...
    state.vehicles[0].start.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[0].start.sprite.width = teleporter.width;
    state.vehicles[0].start.sprite.height = teleporter.height;
    state.vehicles[0].finish.sprite.img = (unsigned char*) emptypack.pixel_data;
    state.vehicles[0].finish.sprite.width = emptypack.width;
    state.vehicles[0].finish.sprite.height = emptypack.height;

    state.vehicles[0].start.loc.x = 17;
    state.vehicles[0].start.loc.y = 8;

    state.vehicles[0].finish.loc.x = 13;
    state.vehicles[0].finish.loc.y = 3;

    state.vehicles[0].bidirectional = 0;

    // First vehicle... maybe a vine? For now using health pack image.
    // This one will be unidirectional...
    state.vehicles[1].start.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[1].start.sprite.width = teleporter.width;
    state.vehicles[1].start.sprite.height = teleporter.height;
    state.vehicles[1].finish.sprite.img = (unsigned char*) emptypack.pixel_data;
    state.vehicles[1].finish.sprite.width = emptypack.width;
    state.vehicles[1].finish.sprite.height = emptypack.height;

    state.vehicles[1].start.loc.x = 12;
    state.vehicles[1].start.loc.y = 3;

    state.vehicles[1].finish.loc.x = 7;
    state.vehicles[1].finish.loc.y = 8;

    state.vehicles[1].bidirectional = 0;
    
    // First vehicle... maybe a vine? For now using health pack image.
    // This one will be unidirectional...
    state.vehicles[2].start.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[2].start.sprite.width = teleporter.width;
    state.vehicles[2].start.sprite.height = teleporter.height;
    state.vehicles[2].finish.sprite.img = (unsigned char*) emptypack.pixel_data;
    state.vehicles[2].finish.sprite.width = emptypack.width;
    state.vehicles[2].finish.sprite.height = emptypack.height;

    state.vehicles[2].start.loc.x = 6;
    state.vehicles[2].start.loc.y = 8;

    state.vehicles[2].finish.loc.x = 23;
    state.vehicles[2].finish.loc.y = 1;

    state.vehicles[2].bidirectional = 0;

    // Second vehicle... maybe a boat/jeep? For now using health pack image.
    // This one will be bidirectional...
    state.vehicles[3].start.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[3].start.sprite.width = teleporter.width;
    state.vehicles[3].start.sprite.height = teleporter.height;
    state.vehicles[3].finish.sprite.img = (unsigned char*) emptypack.pixel_data;
    state.vehicles[3].finish.sprite.width = emptypack.width;
    state.vehicles[3].finish.sprite.height = emptypack.height;

    state.vehicles[3].start.loc.x = 21;
    state.vehicles[3].start.loc.y = 1;

    state.vehicles[3].finish.loc.x = 11;
    state.vehicles[3].finish.loc.y = 3;

    state.vehicles[3].bidirectional = 0;

    state.vehicles[4].start.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[4].start.sprite.width = teleporter.width;
    state.vehicles[4].start.sprite.height = teleporter.height;
    state.vehicles[4].finish.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[4].finish.sprite.width = teleporter.width;
    state.vehicles[4].finish.sprite.height = teleporter.height;

    state.vehicles[4].start.loc.x = 5;
    state.vehicles[4].start.loc.y = 14;

    state.vehicles[4].finish.loc.x = 6;
    state.vehicles[4].finish.loc.y = 21;

    state.vehicles[4].bidirectional = 1;

    state.vehicles[5].start.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[5].start.sprite.width = teleporter.width;
    state.vehicles[5].start.sprite.height = teleporter.height;
    state.vehicles[5].finish.sprite.img = (unsigned char*) teleporter.pixel_data;
    state.vehicles[5].finish.sprite.width = teleporter.width;
    state.vehicles[5].finish.sprite.height = teleporter.height;

    state.vehicles[5].start.loc.x = 16;
    state.vehicles[5].start.loc.y = 8;

    state.vehicles[5].finish.loc.x = 17;
    state.vehicles[5].finish.loc.y = 14;

    for (int i = 0; i < state.num_vehicles; ++i) {
        state.vehicles[i].start.trampled = 0;
        state.vehicles[i].finish.trampled = 0;
    }

    // Exit... will be located at coords (5, 0), middle of top of screen...
    // Set up exit... (temporarily using coin image)
    state.exit.sprite.img = (unsigned char*) ladder.pixel_data;
    state.exit.sprite.width = coin_image.width;
    state.exit.sprite.height = coin_image.height;

    // Exit for first stage is top of ladder leading out of screen...
    state.exit.loc.x = 4;
    state.exit.loc.y = 0;
    state.exit.exists = 1;
    state.exit.trampled = 0;

    for (int i = 0; i < 25*25; ++i) state.map_tiles[i] = map2[i];

    goto gameloop;

third_stage:

    state.dk.loc.x = 17;
    state.dk.loc.y = 24;

    state.dk.speed = 1;
    state.dk.dk_immunity = 0;
    state.dk.has_boomerang = 0;
    
    state.dk.trampled = 0;

    // Enemies

    state.num_enemies = 8;

    for (int i = 0; i < 8; ++i)
    {
        state.enemies[i].sprite.img = (unsigned char*) enemy_image.pixel_data;
        state.enemies[i].sprite.width = enemy_image.width;
        state.enemies[i].sprite.height = enemy_image.height;

        state.enemies[i].speed = 1;
        state.enemies[i].enemy_direction = 0;
        state.enemies[i].exists = 1;
        
        state.enemies[i].trampled = 0;
    }

    for (int i = 0; i < 6; ++i) state.enemies[i].flying = 0;

    state.enemies[0].loc.x = 3;
    state.enemies[0].loc.y = 22;

    state.enemies[1].loc.x = 5;
    state.enemies[1].loc.y = 19;

    state.enemies[2].loc.x = 12;
    state.enemies[2].loc.y = 16;

    state.enemies[3].loc.x = 0;
    state.enemies[3].loc.y = 0;

    state.enemies[4].loc.x = 5;
    state.enemies[4].loc.y = 3;

    state.enemies[5].loc.x = 12;
    state.enemies[5].loc.y = 6;

// Two flying enemies...

    state.enemies[6].loc.x = 4;
    state.enemies[6].loc.y = 10;

    state.enemies[6].loc.x = 22;
    state.enemies[6].loc.y = 14;

    // Packs

    state.num_packs = 1;


    // Create boomerang pack
    state.packs[0].sprite.img = (unsigned char*) bananarangpack.pixel_data;
    state.packs[0].sprite.width = bananarangpack.width;
    state.packs[0].sprite.height = bananarangpack.height;

    state.packs[0].health_pack = 0;
    state.packs[0].point_pack = 0;
    state.packs[0].boomerang_pack = 1;
    state.packs[0].exists = 1;

    state.packs[0].loc.x = 2;
    state.packs[0].loc.y = 22;

    // Boomerang setup;

    state.boomerang.sprite.img = (unsigned char*) bananarang.pixel_data;
    state.boomerang.sprite.width = bananarang.width;
    state.boomerang.sprite.height = bananarang.height;

    state.boomerang.tiles_per_second = 20;
    state.boomerang.exists = 0;    // Projectile not in game yet.
    state.boomerang.direction = 1; // 1 = right, 0 = left
    
    for (int i = 0; i < state.num_packs; ++i) state.packs[i].trampled = 0;
    
    // No vehicles in this level...

    state.num_vehicles = 0;

    // Exit...
    state.exit.sprite.img = (unsigned char*) ladder.pixel_data;
    state.exit.sprite.width = ladder.width;
    state.exit.sprite.height = ladder.height;

    // Exit for third stage is top of ladder leading out of screen...
    state.exit.loc.x = 17;
    state.exit.loc.y = 0;
    state.exit.exists = 1;
    state.exit.trampled = 0;

    for (int i = 0; i < 25*25; ++i) state.map_tiles[i] = map3[i];

    goto gameloop;

fourth_stage:

game_won:

    drawString(SCREENWIDTH/2 - 50, SCREENHEIGHT/2, "Game won! Congratulations!", 0xF);

    return 1;
}
