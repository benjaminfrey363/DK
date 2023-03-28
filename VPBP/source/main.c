#include "gpio.h"
#include "uart.h"
#include "fb.h"

#include <stdio.h>
#include <unistd.h>

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

////////////////////////
// END OF DYLANS CODE //
////////////////////////

// Array to track which buttons have been pressed;
int buttons[16];

////////////////
// STRUCTURES //
////////////////

// Defines structure of a printable image.
//
// An image tracks an unsigned character array, a width, and a height.
struct image
{
    unsigned char *img;
    int width;
    int height;
};

// Structure to track object coordinates.
struct coord
{
    int x; // Horz coordinate
    int y; // Vert coordinate
};

// Defines object structure.
//
// An instance of object contains a sprite, a flag indicating collision
// with another object (not used yet), and a coordinate.
struct object
{
    // The fundamental traits of an object are a sprite and a location, all other
    // variables are used by different extensions of an object.
    struct image sprite; // sprite.
    struct coord loc;    // Coordinate location.

    // Speed variable for moving objects.
    int speed;

    // enemy_direction determines which direction moving enemies will walk.
    // 0 for left, 1 for right.
    int enemy_direction;

    // Immunity flag, only used for dk.
    int dk_immunity;
    // Counts number of point packs picked up, only used for DK.
    int num_coins_grabbed;

    // Pack type flags, only used for packs.
    int health_pack;
    int point_pack;
    int boomerang_pack;

    int has_boomerang;
    // Boolean used for packs and exits, removes them from the map after DK collides with them.
    int exists;
    
    // Boolean used to indicate that an enemy has collided with a pack, a vehicle, or an exit.
    int trampled;
};

// Vehicle structure - effectively teleports DK between cells.
struct vehicle
{
    struct object start;
    struct object finish;

    // Boolean indicating whether vehicle can be traversed finish -> start as well.
    int bidirectional;
};

struct projectile
{
    int tiles_per_second; // Number of grid panels travelled per second
    int register_hit;     // becomes true when projectile hits an enemy
    int exists;           // boolean to show existence of projectile
    int direction;

    struct image sprite;
    struct coord loc;
};

struct mapTiles
{
    struct coord ladders[20 * 20];
    struct coord platforms[20 * 20];

    int num_ladders;
    int num_platforms;
};

// Gamestate structure
struct gamestate
{
    int width;
    int height;

    int score;
    int lives;
    int time;

    // Also track background image...
    struct image background;

    // Tracks an array of enemies and packs.
    struct object enemies[MAXOBJECTS];
    int num_enemies; // Actual # of enemies in world.

    // Tracks an array of packs as well.
    struct object packs[MAXOBJECTS];
    int num_packs; // Actual # of packs in world.

    // Also tracks an array of vehicles.
    struct vehicle vehicles[MAXOBJECTS];
    int num_vehicles;

    // Tracks a boomerang object
    struct projectile boomerang;

    // Also track dk.
    struct object dk;

    struct mapTiles map1;
    struct mapTiles map2;
    struct mapTiles map3;
    struct mapTiles map4;

    int map_selection; // Used to select the current map.

    // Flags.
    int winflag;
    int loseflag;

    // Exit structure - DK colliding with exit causes win flag to be set.
    struct object exit;
};

struct startMenu
{
    int startGameSelected;
    int quitGameSelected;
};

/*
struct levelSelect{
    int levelOneSelected;
    int levelTwoSelected;
    int levelThreeSelected;
    int levelFourSelected;

};
*/

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
void draw_grid(struct object o, int width, int height)
{
    if (!o.trampled) myDrawImage(o.sprite.img, o.sprite.width, o.sprite.height, grid_to_pixel_x(o.loc.x, width), grid_to_pixel_y(o.loc.y, height));
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
void draw_state(struct gamestate state)
{

    // First, draw background at the origin...
    // draw_image(state.background, 0, 0);

    // Draw DK...
    draw_image(state.dk.sprite, state.dk.loc.x * (SCREENWIDTH / state.width), state.dk.loc.y * (SCREENHEIGHT / state.height));

    // Draw each enemy...
    for (int i = 0; i < state.num_enemies; ++i)
    {
        // Print state.enemies[i] with grid coords (x, y) at location (x * SCREENWIDTH/state.width, y * SCREENHEIGHT/state.height)
        draw_image(state.enemies[i].sprite, state.enemies[i].loc.x * (SCREENWIDTH / state.width), state.enemies[i].loc.y * (SCREENHEIGHT / state.height));
    }

    // Draw each pack...
    for (int i = 0; i < state.num_packs; ++i)
    {
        // Print state.packs[i] with grid coords (x, y) at location (x * SCREENWIDTH/state.width, y * SCREENHEIGHT/state.height)
        if (state.packs[i].exists)
            draw_image(state.packs[i].sprite, state.packs[i].loc.x * (SCREENWIDTH / state.width), state.packs[i].loc.y * (SCREENHEIGHT / state.height));
    }

    // Draw each vehicle...
    for (int i = 0; i < state.num_vehicles; ++i)
    {
        // Draw start...
        draw_image(state.vehicles[i].start.sprite, state.vehicles[i].start.loc.x * (SCREENWIDTH / state.width), state.vehicles[i].start.loc.y * (SCREENHEIGHT / state.height));
        // Draw finish...
        draw_image(state.vehicles[i].finish.sprite, state.vehicles[i].finish.loc.x * (SCREENWIDTH / state.width), state.vehicles[i].finish.loc.y * (SCREENHEIGHT / state.height));
    }

    // Print score...
    draw_int(state.score, SCREENWIDTH, FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, FONT_HEIGHT, "SCORE:", 0xF);

    // Print time remaining...
    draw_int(state.time, SCREENWIDTH, 2 * FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, 2 * FONT_HEIGHT, "TIME:", 0xF);

    // Print lives remaining... (replace with hearts later)
    draw_int(state.lives, SCREENWIDTH, 3 * FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, 3 * FONT_HEIGHT, "LIVES:", 0xF);
}

// Clear every object on the screeen using the eraser image.
void clear_screen(struct gamestate state, struct image eraser)
{
    // Erase DK...
    draw_image(eraser, state.dk.loc.x * (SCREENWIDTH / state.width), state.dk.loc.y * (SCREENHEIGHT / state.height));

    // Erase each enemy...
    for (int i = 0; i < state.num_enemies; ++i)
    {
        draw_image(eraser, state.enemies[i].loc.x * (SCREENWIDTH / state.width), state.enemies[i].loc.y * (SCREENHEIGHT / state.height));
    }

    // Erase each pack...
    for (int i = 0; i < state.num_packs; ++i)
    {
        draw_image(eraser, state.packs[i].loc.x * (SCREENWIDTH / state.width), state.packs[i].loc.y * (SCREENHEIGHT / state.height));
    }

    // Erase each vehicle...
    for (int i = 0; i < state.num_vehicles; ++i)
    {
        draw_image(eraser, state.vehicles[i].start.loc.x * (SCREENWIDTH / state.width), state.vehicles[i].start.loc.y * (SCREENHEIGHT / state.height));
        draw_image(eraser, state.vehicles[i].finish.loc.x * (SCREENWIDTH / state.width), state.vehicles[i].finish.loc.y * (SCREENHEIGHT / state.height));
    }
}

// Print black at every cell of the gamestate.
void black_screen(struct gamestate state)
{
    for (int i = 0; i < state.width; ++i)
    {
        for (int j = 0; j < state.height; ++j)
        {
            myDrawImage(black_image.pixel_data, black_image.width, black_image.height, i * (SCREENWIDTH / state.width), j * (SCREENWIDTH / state.height));
        }
    }
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

/*
//After "Start" is selected, User can select level (1-4). Level 1 is hovering initially
void levelSelection(int *buttons) {

    if(buttons[4] ==  0){
        if(start.levelSelect.levelTwoSelected == 1){
            start.levelSelect.levelTwoSelected = 0;
            start.levelSelect.levelOneSelected = 1;
            //Hover Text
        }
        else if(start.levelSelect.levelThreeSelected == 1){
            start.levelSelect.levelThreeSelected = 0;
            start.levelSelect.levelTwoSelected = 1;
            //Hover Text
        }
        else if(start.levelSelect.levelFourSelected == 1){
            start.levelSelect.levelFourSelected = 0;
            start.levelSelect.levelThreeSelected = 1;
            //Hover Text
        }
    }

    else if(buttons[5] ==  0){
        if(start.levelSelect.levelOneSelected == 1){
            start.levelSelect.levelOneSelected = 0;
            start.levelSelect.levelTwoSelected = 1;
            //Hover Text
        }
        else if(start.levelSelect.levelTwoSelected == 1){
            start.levelSelect.levelTwoSelected = 0;
            start.levelSelect.levelThreeSelected = 1;
            //Hover Text
        }
        else if(start.levelSelect.levelThreeSelected == 1){
            start.levelSelect.levelThreeSelected = 0;
            start.levelSelect.levelFourSelected = 1;
            //Hover Text
        }
    }

    if(buttons[8] == 0){
        if(start.levelSelect.levelOneSelected == 1){
            //Proceed to gamemap 1;
        }
        else if(start.levelSelect.levelTwoSelected == 1){
            //Proceed to gamemap 2;
        }
        else if(start.levelSelect.levelThreeSelected == 1){
            //Proceed to gamemap 3;
        }
        else if(start.levelSelect.levelFourSelected == 1){
            //Proceed to gamemap 4;
        }

    }
}
*/

////////////////////////
// MOVEMENT FUNCTIONS //
////////////////////////

/*
 * This method will be called in response to a directional input from the SNES controller.
 * int direction () can be changed to be the direct output from the controller instead of
 * converting the input to an integer. A directional input will check to ensure that DK
 * will still be on the screen and will not collide hit with an object.
 */

void DKmove(int *buttons, struct gamestate *state)
{

    int pressed = 0;

    // Record old coordinates so that background can be drawn there.
    int oldx = (*state).dk.loc.x;
    int oldy = (*state).dk.loc.y;

    if (buttons[7] == 0)
    { // Right
        if ((*state).dk.loc.x + (*state).dk.speed <= (*state).width)
        {
            // Ensure that DK does not step outside of screen
            uart_puts("Right\n");
            pressed = 7;
            (*state).dk.loc.x += (*state).dk.speed;
            (*state).dk.enemy_direction = 1;
        }
    }

    else if (buttons[6] == 0)
    { // Left
        if ((*state).dk.loc.x - (*state).dk.speed >= 0)
        {
            // Ensure that DK does not step outside of screen
            uart_puts("Left\n");
            pressed = 6;
            (*state).dk.loc.x -= (*state).dk.speed;
            (*state).dk.enemy_direction = 0;
        }
    }

    else if (buttons[4] == 0)
    { // Up
        if ((*state).dk.loc.y - (*state).dk.speed >= 0)
        {
            // Ensure that DK does not step outside of screen
            uart_puts("Up\n");
            pressed = 4;
            (*state).dk.loc.y -= (*state).dk.speed;
            (*state).dk.enemy_direction = 2;
        }
    }

    else if (buttons[5] == 0)
    { // Down
        if ((*state).dk.loc.y + (*state).dk.speed <= (*state).height)
        {
            // Ensure that DK does not step outside of screen
            uart_puts("Down\n");
            pressed = 5;
            (*state).dk.loc.y += (*state).dk.speed;
            (*state).dk.enemy_direction = 2;
        }
    }

    // If DK moved, he loses his immunity and we redraw the background at his old location.
    // If DK hasn't moved we don't redraw the background - this is to prevent DK from fading
    // in and out of black.
    if (pressed > 0)
    {
        (*state).dk.dk_immunity = 0;
        draw_image((*state).background, grid_to_pixel_x(oldx, (*state).width), grid_to_pixel_y(oldy, (*state).height));
    }

    if (pressed == 0)
    {
        if ((*state).dk.enemy_direction == 1)
        {
            (*state).dk.sprite.img = dk_right0.pixel_data;
        }
        else if ((*state).dk.enemy_direction == 0)
        {
            (*state).dk.sprite.img = dk_left0.pixel_data;
        }
    }

    // Draw DK at his new location...
    draw_grid((*state).dk, (*state).width, (*state).height);
    // draw_image((*state).dk.sprite, (*state).dk.loc.x * (SCREENWIDTH / (*state).width), (*state).dk.loc.y * (SCREENHEIGHT / (*state).height));

    // Wait for pressed joypad button to be unpressed before function can be exited
    // Time and score will continue to be updated while we're in this loop so that these values are not paused when Jpad is held down.

    unsigned int initial_time = *clo;

    while (pressed > 0)
    {
        read_SNES(buttons);
        if (buttons[pressed] == 1)
            pressed = 0;
        initial_time = ((*clo - initial_time) / 1000) + 1; // initial_time is time elapsed in thousandths of a second.
        // 1 is added to initial_time in case iterations of this loop are short enough that *clo - initial_time < 1000
        (*state).time -= initial_time;

        initial_time = *clo; // Setting of initial_time is put here as printing to screen is the most time-consuming part of this loop.

        // Print updated time to screen...
        draw_int((*state).time, SCREENWIDTH, 2 * FONT_HEIGHT, 0xF);
        // drawString(SCREENWIDTH - 200, 2*FONT_HEIGHT, "TIME:", 0xF);
    }
    uart_puts("Unpressed");
}

/*
 * This method will iterate through the array of object coordinates and check
 * if DK's next step in the given direction would result in a collision with
 * any object. Returning a 1 will indicate that DK will collide with an object.
 */
/*
int checkCollision(int direction, struct gamestate state)
{
   if (direction == 1)
   { // DK moving right
       for (int i = 1; i < state.num_objects; i++)
       { // Iterate through list of all positions, find if any x values match;
           if (state.positions[0].x + 1 == state.positions[i].x)
           { // If DK's X value is the same as another x value,
               if (state.positions[0].y == state.positions[i].y)
               {
                   return 1;
               }
           }
       }
   }
   else if (direction == 2)
   { // DK moving left
       for (int i = 1; i < state.num_objects; i++)
       { // Iterate through list of all positions, find if any x values match;
           if (state.positions[0].x - 1 == state.positions[i].x)
           { // If DK's X value is the same as another x value,
               if (state.positions[0].y == state.positions[i].y)
               {
                   return 1;
               }
           }
       }
   }
   else if (direction == 3)
   { // DK moving up
       for (int i = 1; i < state.num_objects; i++)
       { // Iterate through list of all positions, find if any x values match;
           if (state.positions[0].x == state.positions[i].x)
           { // If DK's X value is the same as another x value,
               if (state.positions[0].y - 1 == state.positions[i].y)
               {
                   return 1;
               }
           }
       }
   }
   else if (direction == 4)
   { // DK moving down
       for (int i = 1; i < state.num_objects; i++)
       { // Iterate through list of all positions, find if any x values match;
           if (state.positions[0].x == state.positions[i].x)
           { // If DK's X value is the same as another x value,
               if (state.positions[0].y + 1 == state.positions[i].y)
               {
                   return 1;
               }
           }
       }
   }
   return 0;
}
*/

/*
// Controls horizontal movement. Updates object offsets according to which buttons are pressed.
// Does not print anything.
// Takes offset arguments as pointers so that these offsets can be edited.
void move_horz(int *btns, int *offx) {
    if (btns[6] == 0) {
        // Pressing left
        if (*offx > 0) --(*offx);
    }
    if (btns[7] == 0) {
        // Pressing right
        if (*offx < SCREENWIDTH) ++(*offx);
    }
}
*/

/*
// Takes a gamestate as an argument. Gets DK to jump, while continuing to take button
// inputs and updating x position accordingly. Continues updating enemy positions as well
// (done randomly), and prints the gamestate at each step.
void jump(struct gamestate *state_ptr, int *btns) {
    for (int i = 0; i <= JUMPHEIGHT; ++i) {
        // Print sprite at location (offx, offy - i).
        // Check to see if left or right is pressed and update offx accordingly.
        read_SNES(buttons);
        move_horz(buttons, &((*state_ptr).objects[0].loc.x));
        --((*state_ptr).objects[0].loc.y);		// Decrement offy each iteration.

    // Randomly update locations of enemies...

    for (int i = 1; i < (*state_ptr).num_objects; ++i) {
        // For now all random movement is left
        move_rand(&((*state_ptr).objects[i]), 1);
    }

    // Draw gamestate.
    // First draw background...
    draw_image((*state_ptr).background, 0, 0);
    // Draw each object...
    for (int i = 0; i < (*state_ptr).num_objects; ++i)
        draw_image((*state_ptr).objects[i].sprite, (*state_ptr).objects[i].loc.x, (*state_ptr).objects[i].loc.y);
    }

    // Get sprite to fall back down...
    for (int i = JUMPHEIGHT; i >= 0; --i) {
        // Print sprite at location (offx, offy - i)
        // Check to see if left or right is pressed and update offx accordingly.
        read_SNES(buttons);
        move_horz(buttons, &((*state_ptr).objects[0].loc.x));
        ++((*state_ptr).objects[0].loc.y);		// Increment *offy each iteration.

        // Randomly update locations of enemies...

    for (int i = 1; i < (*state_ptr).num_objects; ++i) {
        // For now all random movement is left
        move_rand(&((*state_ptr).objects[i]), 1);
    }

    // Draw gamestate.
    // First draw background...
    draw_image((*state_ptr).background, 0, 0);
    // Draw each object...
    for (int i = 0; i < (*state_ptr).num_objects; ++i)
        draw_image((*state_ptr).objects[i].sprite, (*state_ptr).objects[i].loc.x, (*state_ptr).objects[i].loc.y);

    }
}
*/

// Moves enemy using enemy_direction.
// If enemy is at the edge of the screen, flip enemy_direction.
void move_enemy(struct object *ob_ptr, struct gamestate *state)
{

    // IMPORTANT - enemy always moves when this function is called.

    int oldx = (*ob_ptr).loc.x;
    int oldy = (*ob_ptr).loc.y;

    if ((*ob_ptr).enemy_direction == 0)
    {
        // Move left.
        if ((*ob_ptr).loc.x - (*ob_ptr).speed >= 0)
            (*ob_ptr).loc.x -= (*ob_ptr).speed;
        else
            (*ob_ptr).enemy_direction = 1;
    }
    else
    {
        // Move right.
        if ((*ob_ptr).loc.x + (*ob_ptr).speed <= (*state).width)
            (*ob_ptr).loc.x += (*ob_ptr).speed;
        else
            (*ob_ptr).enemy_direction = 0;
    }

    // Draw enemy at new location and erase at old location.
    draw_image((*state).background, grid_to_pixel_x(oldx, (*state).width), grid_to_pixel_y(oldy, (*state).height));
    draw_grid(*ob_ptr, (*state).width, (*state).height);
    
    // If old location corresponds to a trampled pack, vehicle, or exit, set trampled to false for that object.
    if (oldx == (*state).exit.loc.x && oldy == (*state).exit.loc.y && (*state).exit.trampled) (*state).exit.trampled = 0;
    
    for (int i = 0; i < (*state).num_packs; ++i) {
        if (oldx == (*state).packs[i].loc.x && oldy == (*state).packs[i].loc.y && (*state).packs[i].trampled) (*state).packs[i].trampled = 0;
    }
    
    for (int i = 0; i < (*state).num_vehicles; ++i) {
        if (oldx == (*state).vehicles[i].start.loc.x && oldy == (*state).vehicles[i].start.loc.y && (*state).vehicles[i].start.trampled) (*state).vehicles[i].start.trampled = 0;
        if (oldx == (*state).vehicles[i].finish.loc.x && oldy == (*state).vehicles[i].finish.loc.y && (*state).vehicles[i].finish.trampled) (*state).vehicles[i].finish.trampled = 0;
    }
    
    // If new location corresponds to a pack, vehicle, or exit, set tramped to true for that object.
    if ((*ob_ptr).loc.x == (*state).exit.loc.x && (*ob_ptr).loc.y == (*state).exit.loc.y) (*state).exit.trampled = 1;
    
    for (int i = 0; i < (*state).num_packs; ++i) {
        if ((*ob_ptr).loc.x == (*state).packs[i].loc.x && (*ob_ptr).loc.y == (*state).packs[i].loc.y) (*state).packs[i].trampled = 1;
    }
    
    for (int i = 0; i < (*state).num_vehicles; ++i) {
        if ((*ob_ptr).loc.x == (*state).vehicles[i].start.loc.x && (*ob_ptr).loc.y == (*state).vehicles[i].start.loc.y) (*state).vehicles[i].start.trampled = 1;
        if ((*ob_ptr).loc.x == (*state).vehicles[i].finish.loc.x && (*ob_ptr).loc.y == (*state).vehicles[i].finish.loc.y) (*state).vehicles[i].finish.trampled = 1;
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
    if ((*state).boomerang.direction == 0 && (*state).boomerang.loc.x > 0)
    {
        (*state).boomerang.loc.x--;
    }
    if ((*state).boomerang.direction == 1 && (*state).boomerang.loc.x < (*state).width)
    {
        (*state).boomerang.loc.x++;
    }
    // See if boomerang has hit any enemy, if so, kills enemy and reverses direction
    for (int i = 0; i < (*state).num_enemies; ++i)
    {
        if ((*state).boomerang.loc.x == (*state).enemies[i].loc.x && (*state).boomerang.loc.y == (*state).enemies[i].loc.y)
        {
            (*state).boomerang.register_hit;
            (*state).enemies[i].exists = 0;
            if ((*state).boomerang.direction == 1)
            {
                (*state).boomerang.direction == 0;
            }
            else if ((*state).boomerang.direction == 0)
            {
                (*state).boomerang.direction == 1;
            }
        }
    }
    // If boomerang hits either edge of the map, reverse direction
    if ((*state).boomerang.loc.x == 0)
    {
        if ((*state).boomerang.direction == 1)
        {
            (*state).boomerang.direction == 0;
        }
        else if ((*state).boomerang.direction == 0)
        {
            (*state).boomerang.direction == 1;
        }
    }
    else if ((*state).boomerang.loc.x == 0)
    {
        if ((*state).boomerang.direction == 1)
        {
            (*state).boomerang.direction == 0;
        }
        else if ((*state).boomerang.direction == 0)
        {
            (*state).boomerang.direction == 1;
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
        (*state).boomerang.sprite.img = bananarang2.pixel_data;
    }
    else if ((*state).boomerang.sprite.img == bananarang2.pixel_data)
    {
        (*state).boomerang.sprite.img = bananarang3.pixel_data;
    }
    else if ((*state).boomerang.sprite.img == bananarang3.pixel_data)
    {
        (*state).boomerang.sprite.img = bananarang.pixel_data;
    }

    // Draw boomerang if current position is not dk's position.
    if ((*state).boomerang.loc.x != (*state).dk.loc.x)
    {
        draw_image((*state).boomerang.sprite, grid_to_pixel_x((*state).boomerang.loc.x, (*state).width), grid_to_pixel_y((*state).boomerang.loc.y, (*state).height));
    }
    // Draw boomerang if previous position is not dk's position (as to not erase dk)
    else if (oldx != (*state).dk.loc.x)
    {
        draw_image((*state).background, grid_to_pixel_x(oldx, (*state).width), grid_to_pixel_y(oldy, (*state).height));
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

    /*
    // Clear the screen before we begin...
    for (int i = 0; i < state.width; ++i) {
        for (int j = 0; j < state.height; ++j) {
            myDrawImage(black_image.pixel_data, black_image.width, black_image.height, i * (SCREENWIDTH / state.width), j * (SCREENWIDTH / state.height));
        }
    }
    */

    /////////////////////////////////
    // START MENU - BASICALLY DONE //
    /////////////////////////////////

    struct startMenu sm;
    struct gamestate state;

start_menu:
    sm.startGameSelected = 1;
    sm.quitGameSelected = 0;
    int start_flag = 0;

    // Draw rectangle border...
    drawRect(SCREENWIDTH / 2 - 100, SCREENHEIGHT / 2 - 100, SCREENWIDTH / 2 + 100, SCREENHEIGHT / 2 + 100, 0xF, 0);

    while (start_flag == 0)
    {
        // Display selection options with currently selected option being pointed to...
        if (sm.startGameSelected)
        {
            drawString(SCREENWIDTH / 2 - 50, SCREENHEIGHT / 2 - 25, "-> START GAME", 0xF);
            drawString(SCREENWIDTH / 2 - 50, SCREENHEIGHT / 2 + 25, "   QUIT GAME", 0xF);
        }
        else if (sm.quitGameSelected)
        {
            drawString(SCREENWIDTH / 2 - 50, SCREENHEIGHT / 2 - 25, "   START GAME", 0xF);
            drawString(SCREENWIDTH / 2 - 50, SCREENHEIGHT / 2 + 25, "-> QUIT GAME", 0xF);
        }
        // Loop while startMenuSelectOption returns 0 - so breaks when player presses
        // A on either start or quit option.
        read_SNES(buttons);
        // Pass address of sm so that flag attributes can be modified by function.
        // Function will automatically erase text when selection moves up/down.
        start_flag = startMenuSelectOption(buttons, &sm);
    }

    // Erase selection menu from screen...
    drawRect(SCREENWIDTH / 2 - 100, SCREENHEIGHT / 2 - 100, SCREENWIDTH / 2 + 100, SCREENHEIGHT / 2 + 100, 0x0, 1);

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

first_stage:

    state.width = 25;
    state.height = 25;

    state.score = 0;
    state.lives = 4;
    state.time = 1000000; // Display time in thousandths of a second

    state.winflag = 0;
    state.loseflag = 0;

    // Background image for stage 1...

    state.background.img = black_image.pixel_data;
    state.background.width = black_image.width;
    state.background.height = black_image.height;
    state.map1.num_ladders = 22;
    int ladders1x[] = { 4, 4, 18, 18, 18, 18, 10, 10, 10, 10, 10, 10, 5, 5, 5, 5, 5, 14, 14, 14, 14, 14 };
    int ladders1y[] = { 2, 3, 0, 1, 2, 3, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 13, 14, 15, 16, 17 };
    for (int i = 0; i < state.map1.num_ladders; ++i) {
        state.map1.ladders[i].x = ladders1x[i];
        state.map1.ladders[i].y = ladders1y[i];
    }

    state.map1.num_platforms = 0;	//46;    
    /*
    int platforms1x[] = { 1, 2, 3, 4, 5, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0, 1, 2, 3, 4, 5, 6, 13, 14, 15, 16, 17, 18, 19 };
    int platforms1y[] = {
        1,
        1,
        1,
        1,
        1,
        4,
        4,
        4,
        4,
        4,
        4,
        4,
        4,
        4,
        4,
        4,
        4,
        4,
        4,
        4,
        12,
        12,
        12,
        12,
        12,
        12,
        12,
        12,
        12,
        12,
        12,
        12,
        12,
        12,
        18,
        18,
        18,
        18,
        18,
        18,
        18,
        18,
        18,
        18,
        18,
        18,
        18,
        18,
    };
    for (int i = 0; i < state.map1.num_platforms; ++i) {
        state.map1.platforms[i].x = platforms1x[i];
        state.map1.platforms[i].y = platforms1y[i];
    }
    */
                              // Objects for stage 1...

                              // DK

                              state.dk.sprite.img = dk_right0.pixel_data; // Initial image of DK will be standing, facing right.
    state.dk.sprite.width = dk_right0.width;
    state.dk.sprite.height = dk_right0.height;

    state.dk.loc.x = 0;
    state.dk.loc.y = 18;

    state.dk.speed = 1;
    state.dk.dk_immunity = 0;
    state.dk.num_coins_grabbed = 0;
    
    state.dk.trampled = 0;

    // Enemies

    state.num_enemies = 2;

    for (int i = 0; i < 2; ++i)
    {
        state.enemies[i].sprite.img = enemy_image.pixel_data;
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
        state.packs[i].sprite.img = heartpack.pixel_data;
        state.packs[i].sprite.width = heartpack.width;
        state.packs[i].sprite.height = heartpack.height;

        state.packs[i].health_pack = 1;
        state.packs[i].point_pack = 0;
        state.packs[i].exists = 1;
    }

    state.packs[0].loc.x = 1;
    state.packs[0].loc.y = 1;

    // Point packs...

    for (int i = 1; i < 3; ++i)
    {
        state.packs[i].sprite.img = coinpack.pixel_data;
        state.packs[i].sprite.width = coinpack.width;
        state.packs[i].sprite.height = coinpack.height;

        state.packs[i].health_pack = 0;
        state.packs[i].point_pack = 1;
        state.packs[i].boomerang_pack = 0;
        state.packs[i].exists = 1;
    }

    state.packs[1].loc.x = 3;
    state.packs[1].loc.y = 12;
    state.packs[2].loc.x = 16;
    state.packs[2].loc.y = 12;

    // Create boomerang pack
    state.packs[3].sprite.img = bananarangpack.pixel_data;
    state.packs[3].sprite.width = bananarangpack.width;
    state.packs[3].sprite.height = bananarangpack.height;

    state.packs[3].health_pack = 0;
    state.packs[3].point_pack = 0;
    state.packs[3].boomerang_pack = 1;
    state.packs[3].exists = 1;

    state.packs[3].loc.x = 18;
    state.packs[3].loc.y = 17;

    // Boomerang setup;

    state.boomerang.sprite.img = bananarang.pixel_data;
    state.boomerang.sprite.width = bananarang.width;
    state.boomerang.sprite.height = bananarang.height;

    state.boomerang.tiles_per_second = 2;
    state.boomerang.exists = 0;    // Projectile not in game yet.
    state.boomerang.direction = 1; // 1 = right, 0 = left
    
    for (int i = 0; i < state.num_packs; ++i) state.packs[i].trampled = 0;
    
    
    // Vehicles...

    state.num_vehicles = 2;

    // First vehicle... maybe a vine? For now using health pack image.
    // This one will be unidirectional...
    state.vehicles[0].start.sprite.img = health_image.pixel_data;
    state.vehicles[0].start.sprite.width = health_image.width;
    state.vehicles[0].start.sprite.height = health_image.height;
    state.vehicles[0].finish.sprite.img = health_image.pixel_data;
    state.vehicles[0].finish.sprite.width = health_image.width;
    state.vehicles[0].finish.sprite.height = health_image.height;

    state.vehicles[0].start.loc.x = 1;
    state.vehicles[0].start.loc.y = 2;

    state.vehicles[0].finish.loc.x = 1;
    state.vehicles[0].finish.loc.y = 5;

    state.vehicles[0].bidirectional = 0;

    // Second vehicle... maybe a boat/jeep? For now using health pack image.
    // This one will be bidirectional...
    state.vehicles[1].start.sprite.img = health_image.pixel_data;
    state.vehicles[1].start.sprite.width = health_image.width;
    state.vehicles[1].start.sprite.height = health_image.height;
    state.vehicles[1].finish.sprite.img = health_image.pixel_data;
    state.vehicles[1].finish.sprite.width = health_image.width;
    state.vehicles[1].finish.sprite.height = health_image.height;

    state.vehicles[1].start.loc.x = 1;
    state.vehicles[1].start.loc.y = 7;

    state.vehicles[1].finish.loc.x = 1;
    state.vehicles[1].finish.loc.y = 12;

    state.vehicles[1].bidirectional = 1;
    
    for (int i = 0; i < state.num_vehicles; ++i) {
        state.vehicles[i].start.trampled = 0;
        state.vehicles[i].finish.trampled = 0;
    }

    // Exit...

    // Set up exit... (temporarily using coin image)
    state.exit.sprite.img = coin_image.pixel_data;
    state.exit.sprite.width = coin_image.width;
    state.exit.sprite.height = coin_image.height;

    // Exit for first stage will be located at coords (5, 0), middle of top of screen...
    state.exit.loc.x = 5;
    state.exit.loc.y = 0;
    state.exit.exists = 1;
    state.exit.trampled = 0;

    //////////////////////
    // FIRST STAGE LOOP //
    //////////////////////

    unsigned int time0;
    unsigned int enemy_move_reference_time = *clo;
    unsigned int dk_sprite_change_reference = *clo;
    unsigned int boomerang_reference = *clo;

    int dk_sprite_change_interval = 500000; // Suppose to be 0.5 second
    int enemy_move_delay = 1000000;         // Suppose to be 1 second
    int dk_spriteTracker = 0;

    // this loop will run while we're in the first level - break if either win flag or lose flag is set.
    while (!state.winflag && !state.loseflag)
    {

        // Record clock register contents so that we can calculate the time elapsed by each iteration of this loop,
        // then subtract that from the amount of time remaining.
        time0 = *clo;

        // Read controller.
        read_SNES(buttons);

        ////////////////
        // PAUSE MENU //
        ////////////////

        // If start has been pressed, enter pause menu...
        if (buttons[4 - 1] == 0)
        {
            int restart_pressed = 1;
            int pressed_a = 0;
            wait(500000); // Wait for a bit to stop menu from immediately closing.

            // Draw white rectangle border and black rectangle fill...
            drawRect(SCREENWIDTH / 2 - 75, SCREENHEIGHT / 2 - 50, SCREENWIDTH / 2 + 200, SCREENHEIGHT / 2 + 100, 0x0, 1);
            drawRect(SCREENWIDTH / 2 - 75, SCREENHEIGHT / 2 - 50, SCREENWIDTH / 2 + 200, SCREENHEIGHT / 2 + 100, 0xF, 0);

            while (!pressed_a)
            {
                // First, display pause menu...

                if (restart_pressed)
                {
                    drawString(SCREENWIDTH / 2, SCREENHEIGHT / 2, "-> RESTART GAME", 0xF);
                    drawString(SCREENWIDTH / 2, SCREENHEIGHT / 2 + 50, "   QUIT GAME", 0xF);
                }
                else
                {
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

            if (pressed_a)
            {
                // Execute current selection. Otherwise, start was pressed to exit game menu and
                // we continue playing the game.

                // No matter what was selected, we should erase the game state before either restarting or exiting.

                // Erase DK...
                draw_image(state.background, grid_to_pixel_x(state.dk.loc.x, state.width), grid_to_pixel_y(state.dk.loc.y, state.height));

                // Erase each enemy...
                for (int i = 0; i < state.num_enemies; ++i)
                {
                    draw_image(state.background, grid_to_pixel_x(state.enemies[i].loc.x, state.width), grid_to_pixel_y(state.enemies[i].loc.y, state.height));
                }

                // Erase each pack...
                for (int i = 0; i < state.num_packs; ++i)
                {
                    draw_image(state.background, grid_to_pixel_x(state.packs[i].loc.x, state.width), grid_to_pixel_y(state.packs[i].loc.y, state.height));
                }

                // Erase each vehicle...
                for (int i = 0; i < state.num_vehicles; ++i)
                {
                    draw_image(state.background, grid_to_pixel_x(state.vehicles[i].start.loc.x, state.width), grid_to_pixel_y(state.vehicles[i].start.loc.y, state.height));
                    draw_image(state.background, grid_to_pixel_x(state.vehicles[i].finish.loc.x, state.width), grid_to_pixel_y(state.vehicles[i].finish.loc.y, state.height));
                }

                // Erase exit...
                draw_image(state.background, grid_to_pixel_x(state.exit.loc.x, state.width), grid_to_pixel_y(state.exit.loc.y, state.height));

                // Erase time, score, lives before terminating...
                drawString(SCREENWIDTH - 200, FONT_HEIGHT, "       ", 0xF);     // Erase "SCORE:"
                drawString(SCREENWIDTH - 200, 2 * FONT_HEIGHT, "      ", 0xF);  // Erase "TIME:"
                drawString(SCREENWIDTH - 200, 3 * FONT_HEIGHT, "       ", 0xF); // ERASE "LIVES:"

                // Erase numbers (big buffer used, during testing time counter has been holding some crazy values):
                drawString(SCREENWIDTH - 100, FONT_HEIGHT, "                        ", 0xF);
                drawString(SCREENWIDTH - 100, 2 * FONT_HEIGHT, "                        ", 0xF);
                drawString(SCREENWIDTH - 100, 3 * FONT_HEIGHT, "                        ", 0xF);

                if (restart_pressed)
                    goto first_stage; // Restart first level.
                else
                {
                    drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT / 2, "Exiting...", 0xF);
                    wait(2000000);
                    drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT / 2, "          ", 0xF);
                    return 1;
                }
            }
        }

        // Move DK accordingly.

        /*Based on clock value, this if-construct will be entered every 0.5 seconds. It will change the sprite model
         * for the direction that DK is facing.
         */
        if (dk_sprite_change_reference + dk_sprite_change_interval <= time0)
        {
            dk_sprite_change_reference = *clo;

            if (dk_spriteTracker == 0)
            {
                if (state.dk.enemy_direction == 1)
                { // if DK is facing right, switch to second right sprite
                    state.dk.sprite.img = dk_right2.pixel_data;
                }
                else if (state.dk.enemy_direction == 0)
                { // if DK is facing left, switch to second left sprite
                    state.dk.sprite.img = dk_left2.pixel_data;
                }
                else if (state.dk.enemy_direction == 2)
                { // if DK is climbing a ladder (up or down), switch to second ladder sprite
                    state.dk.sprite.img = dk_ladder2.pixel_data;
                }
                dk_spriteTracker = 1; // Tracker shifts, next interval will switch sprites.
            }
            else if (dk_spriteTracker == 1)
            {
                if (state.dk.enemy_direction == 1)
                { // if DK is facing right, switch to first right sprite
                    state.dk.sprite.img = dk_right1.pixel_data;
                }
                else if (state.dk.enemy_direction == 0)
                { // if DK is facing left, switch to first left sprite
                    state.dk.sprite.img = dk_left1.pixel_data;
                }
                else if (state.dk.enemy_direction == 2)
                { // if DK is climbing a ladder (up or down), switch to first ladder sprite
                    state.dk.sprite.img = dk_ladder1.pixel_data;
                }
                dk_spriteTracker = 0;
            }
        }

        DKmove(buttons, &state);

        // Check to see if DK has collided with an enemy. DK can only be hurt if his immunity is turned off.
        if (!state.dk.dk_immunity)
        {
            for (int i = 0; i < state.num_enemies; ++i)
            {
                if (state.dk.loc.x == state.enemies[i].loc.x && state.dk.loc.y == state.enemies[i].loc.y)
                {
                    --state.lives;
                    printf("Lost a life\n");
                    // Give DK immunity - he can't be hurt until he leaves this cell.
                    state.dk.dk_immunity = 1;
                    // Set lose flag if out of lives.
                    if (state.lives == 0)
                        state.loseflag = 1;
                }
            }
        }

        // Check to see if DK has collided with a pack...
        for (int i = 0; i < state.num_packs; ++i)
        {
            if (state.dk.loc.x == state.packs[i].loc.x && state.dk.loc.y == state.packs[i].loc.y && state.packs[i].exists)
            {
                // See which kind of pack DK has collided with, update gamestate accordingly.
                if (state.packs[i].health_pack)
                {
                    // Give DK an extra life if he has less than 4.
                    if (state.lives < 4)
                        ++state.lives;
                }
                else if (state.packs[i].point_pack)
                {
                    ++state.dk.num_coins_grabbed;
                }
                else if (state.packs[i].boomerang_pack)
                {
                    state.dk.has_boomerang = 1;
                }

                // Remove pack from stage.
                state.packs[i].exists = 0;
            }
        }

        // Check to see if DK has collided with a vehicle...
        // To prevent DK from teleporting back and forth using bidirectional vehicles, set dk_immunity after DK teleports.
        // dk_immunity won't be turned off until DK moves from the vehicle cell.
        if (!state.dk.dk_immunity)
        {
            for (int i = 0; i < state.num_vehicles; ++i)
            {

                // Check for collision with start...
                if (state.dk.loc.x == state.vehicles[i].start.loc.x && state.dk.loc.y == state.vehicles[i].start.loc.y)
                {

                    // TO-DO: Insert vehicle animations (vine swinging, etc) if we have the time and ability

                    // Update location of DK to finish location of vehicle...
                    state.dk.loc.x = state.vehicles[i].finish.loc.x;
                    state.dk.loc.y = state.vehicles[i].finish.loc.y;

                    state.dk.dk_immunity = 1; // Set immunity.
                }

                // Check for collision with finish (only teleports DK if vehicle is bidirectional)
                // Added else to this conditional so that DK cannot teleport start -> finish and then immediately finish -> start using a bidirectional vehicle.
                else if (state.dk.loc.x == state.vehicles[i].finish.loc.x && state.dk.loc.y == state.vehicles[i].finish.loc.y && state.vehicles[i].bidirectional)
                {

                    // TO-DO: Insert vehicle animations if we have time and ability.

                    // Update location of DK to start location of vehicle...
                    state.dk.loc.x = state.vehicles[i].start.loc.x;
                    state.dk.loc.y = state.vehicles[i].start.loc.y;

                    state.dk.dk_immunity = 1; // Set immunity.
                }
            }
        }

        // MOVE_ENEMY FUNCTION
        // IMPORTANT - enemy always moves when this function is called.

        if (enemy_move_reference_time + enemy_move_delay <= time0)
        {
            enemy_move_reference_time = *clo;
            for (int i = 0; i < state.num_enemies; i++)
            {
                int oldx = state.enemies[i].loc.x;
                int oldy = state.enemies[i].loc.y;

                if (state.enemies[i].enemy_direction == 0)
                {
                    // Move left.
                    if (state.enemies[i].loc.x - state.enemies[i].speed >= 0)
                        state.enemies[i].loc.x -= state.enemies[i].speed;
                    else
                        state.enemies[i].enemy_direction = 1;
                }
                else
                {
                    // Move right.
                    if (state.enemies[i].loc.x + state.enemies[i].speed <= state.width)
                        state.enemies[i].loc.x += state.enemies[i].speed;
                    else
                        state.enemies[i].enemy_direction = 0;
                }

                // Draw enemy at new location and erase at old location.
                draw_image(state.background, grid_to_pixel_x(oldx, state.width), grid_to_pixel_y(oldy, state.height));
                draw_grid(state.enemies[i], state.width, state.height);
                
                // If old location corresponds to a trampled pack, vehicle, or exit, set trampled to false for that object.
                if (oldx == state.exit.loc.x && oldy == state.exit.loc.y && state.exit.trampled) state.exit.trampled = 0;
    
                for (int i = 0; i < state.num_packs; ++i) {
                    if (oldx == state.packs[i].loc.x && oldy == state.packs[i].loc.y && state.packs[i].trampled) state.packs[i].trampled = 0;
                }
    
                for (int i = 0; i < state.num_vehicles; ++i) {
                    if (oldx == state.vehicles[i].start.loc.x && oldy == state.vehicles[i].start.loc.y && state.vehicles[i].start.trampled) state.vehicles[i].start.trampled = 0;
                    if (oldx == state.vehicles[i].finish.loc.x && oldy == state.vehicles[i].finish.loc.y && state.vehicles[i].finish.trampled) state.vehicles[i].finish.trampled = 0;
                }
    
                // If new location corresponds to a pack, vehicle, or exit, set tramped to true for that object.
                if (state.enemies[i].loc.x == state.exit.loc.x && state.enemies[i].loc.y == state.exit.loc.y) state.exit.trampled = 1;
    
                for (int i = 0; i < state.num_packs; ++i) {
                    if (state.enemies[i].loc.x == state.packs[i].loc.x && state.enemies[i].loc.y == state.packs[i].loc.y) state.packs[i].trampled = 1;
                }
    
                for (int i = 0; i < state.num_vehicles; ++i) {
                    if (state.enemies[i].loc.x == state.vehicles[i].start.loc.x && state.enemies[i].loc.y == state.vehicles[i].start.loc.y) state.vehicles[i].start.trampled = 1;
                    if (state.enemies[i].loc.x == state.vehicles[i].finish.loc.x && state.enemies[i].loc.y == state.vehicles[i].finish.loc.y) state.vehicles[i].finish.trampled = 1;
                }
                
            }
        }

        // Boomerang logic
        if (state.dk.has_boomerang)
        {
            // draw_image() underneath score, boomerang icon
            read_SNES(buttons);
            if (buttons[8] == 0)
            {
                if (state.dk.enemy_direction != 2)
                {
                    state.boomerang.direction = state.dk.enemy_direction;
                    state.boomerang.loc = state.dk.loc;
                    state.boomerang.exists = 1;
                }
            }
        }

        if (state.boomerang.exists)
        {
            if (boomerang_reference + (enemy_move_reference_time / state.boomerang.tiles_per_second) <= time0)
            {
                updateBoomerang(&state);
            }
        }

        // Check to see if DK has reached the exit, set winflag if he has...
        if (state.dk.loc.x == state.exit.loc.x && state.dk.loc.y == state.exit.loc.y)
        {
            state.winflag = 1;
            state.exit.exists = 0;
        }

        // Draw the game state... (have to insert function body)

        // Drawing of DK moved to DKmove - stops DK from disappearing when a button is held down.
        // Draw DK...
        // draw_image(state.dk.sprite, state.dk.loc.x * (SCREENWIDTH / state.width), state.dk.loc.y * (SCREENHEIGHT / state.height));

        // Draw each enemy...
        for (int i = 0; i < state.num_enemies; ++i)
        {
            if (state.enemies[i].exists)
            {
                draw_grid(state.enemies[i], state.width, state.height);
                // draw_image(state.enemies[i].sprite, state.enemies[i].loc.x * (SCREENWIDTH / state.width), state.enemies[i].loc.y * (SCREENHEIGHT / state.height));
                
                // Check to see if any enemies WHICH EXIST are on the same tile as a pack or a vehicle.
            }
        }

        // Draw each pack...
        for (int i = 0; i < state.num_packs; ++i)
        {
            if (state.packs[i].exists)
                draw_grid(state.packs[i], state.width, state.height);
            // draw_image(state.packs[i].sprite, state.packs[i].loc.x * (SCREENWIDTH / state.width), state.packs[i].loc.y * (SCREENHEIGHT / state.height));
        }

        // Draw each vehicle...
        for (int i = 0; i < state.num_vehicles; ++i)
        {
            // Draw start...
            draw_grid(state.vehicles[i].start, state.width, state.height);
            // draw_image(state.vehicles[i].start.sprite, state.vehicles[i].start.loc.x * (SCREENWIDTH / state.width), state.vehicles[i].start.loc.y * (SCREENHEIGHT / state.height));
            //  Draw finish...
            draw_grid(state.vehicles[i].finish, state.width, state.height);
            // draw_image(state.vehicles[i].finish.sprite, state.vehicles[i].finish.loc.x * (SCREENWIDTH / state.width), state.vehicles[i].finish.loc.y * (SCREENHEIGHT / state.height));
        }

        // Draw the exit...
        draw_grid(state.exit, state.width, state.height);
        // draw_image(state.exit.sprite, state.exit.loc.x * (SCREENWIDTH / state.width), state.exit.loc.y * (SCREENHEIGHT / state.height));

        // Update and print score...
        state.score = state.time + (250000 * state.lives) + (250000 * state.dk.num_coins_grabbed);
        draw_int(state.score, SCREENWIDTH, FONT_HEIGHT, 0xF);
        drawString(SCREENWIDTH - 200, FONT_HEIGHT, "SCORE:", 0xF);

        // Update and print time remaining...

        // First calculate time elapsed by this iteration of loop.
        time0 = (*clo - time0) / 1000; // time0 is time elapsed in thousandths of a second.
        state.time -= time0;

        // If state.time is now leq 0, set loseflag.
        if (state.time <= 0)
            state.loseflag = 1;

        draw_int(state.time, SCREENWIDTH, 2 * FONT_HEIGHT, 0xF);
        drawString(SCREENWIDTH - 200, 2 * FONT_HEIGHT, "TIME:", 0xF);

        // Print lives remaining... (replace with hearts later)
        draw_int(state.lives, SCREENWIDTH, 3 * FONT_HEIGHT, 0xF);
        drawString(SCREENWIDTH - 200, 3 * FONT_HEIGHT, "LIVES:", 0xF);

        // End of drawing game state.
    }

    // First stage exited...

    // Before doing anything else, clear the screen... (memcpy error when I try to call function, body copied here instead...)
    // Erase DK...
    draw_image(state.background, grid_to_pixel_x(state.dk.loc.x, state.width), grid_to_pixel_y(state.dk.loc.y, state.height));

    // Erase each enemy...
    for (int i = 0; i < state.num_enemies; ++i)
    {
        draw_image(state.background, grid_to_pixel_x(state.enemies[i].loc.x, state.width), grid_to_pixel_y(state.enemies[i].loc.y, state.height));
    }

    // Erase each pack...
    for (int i = 0; i < state.num_packs; ++i)
    {
        draw_image(state.background, grid_to_pixel_x(state.packs[i].loc.x, state.width), grid_to_pixel_y(state.packs[i].loc.y, state.height));
    }

    // Erase each vehicle...
    for (int i = 0; i < state.num_vehicles; ++i)
    {
        draw_image(state.background, grid_to_pixel_x(state.vehicles[i].start.loc.x, state.width), grid_to_pixel_y(state.vehicles[i].start.loc.y, state.height));
        draw_image(state.background, grid_to_pixel_x(state.vehicles[i].finish.loc.x, state.width), grid_to_pixel_y(state.vehicles[i].finish.loc.y, state.height));
    }

    // Erase exit...
    draw_image(state.background, grid_to_pixel_x(state.exit.loc.x, state.width), grid_to_pixel_y(state.exit.loc.y, state.height));

    // If lost, print game over and return to menu.
    if (state.loseflag)
    {
        drawString(SCREENWIDTH - 25, SCREENHEIGHT, "Game over!", 0xF);
        wait(2000000);
        drawString(SCREENWIDTH - 25, SCREENHEIGHT, "           ", 0xF);

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

    drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT, "First stage won!", 0xF);
    wait(1000000);
    drawString(SCREENWIDTH / 2 - 25, SCREENHEIGHT, "                 ", 0xF);
    state.winflag = 0;

    // Move on to next stage...

    return 1;
}
