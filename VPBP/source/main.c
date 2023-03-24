#include "gpio.h"
#include "uart.h"
#include "fb.h"

// include images
#include "test-art.h"
#include "dk_image.h"
#include "background.h"

#define MAXOBJECTS 30
#define SCREENWIDTH 1888
#define JUMPHEIGHT 100

// GPIO macros

#define GPIO_BASE 0xFE200000
#define CLO_REG 0xFE003004
#define GPSET0_s 7
#define GPCLR0_s 10
#define GPLEV0_s 13

#define CLK 11
#define LAT 9
#define DAT 10

#define INP_GPIO(p) *(gpio + ((p)/10)) &= ~(7 << (((p)%10)*3))
#define OUT_GPIO(p) *(gpio+((p)/10)) |= (1<<(((p)%10)*3))

unsigned int *gpio = (unsigned*)GPIO_BASE;
unsigned int *clo = (unsigned*)CLO_REG;

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
    gpio[index] = (gpio[index] & ~(0b111 << bit_shift) ) | (func << bit_shift);

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
    } else {
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

void init_snes_lines() {
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
	while (c > *clo);
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
	while (i <= 16) {
		wait(6);
		write_gpio(CLK, 0);
		wait(6);
		data = read_gpio(DAT);
		array[i - 1] = data;
		if (data == 0) {flag = 1;}
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
struct image {
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
    struct image sprite;        // sprite.
    int collision;              // collision flag.
    struct coord loc;           // Coordinate location.
};


// Gamestate structure
struct gamestate
{
    int score;
    int lives;
    int time;

    // Also track background image...
    struct image background;

    // Tracks an array of objects.
    struct object objects[MAXOBJECTS];
    int num_objects;        // Actual # of objects in world.

    // Flags.
    int winflag;
    int loseflag;
};


///////////////////////
// DRAWING FUNCTIONS //
///////////////////////

// Draws an image structure at specified pixel offsets.
void draw_image(struct image myimg, int offx, int offy) {
    myDrawImage(myimg.img, myimg.width, myimg.height, offx, offy);
}


// Main drawing method - draws a game state.
void draw_state(struct gamestate s) {
    // First, draw background at the origin...
    draw_image(s.background, 0, 0);
    // Draw each object...
    for (int i = 0; i < s.num_objects; ++i) {
        // Print state.objects[i]
        draw_image(s.objects[i].sprite, s.objects[i].loc.x, s.objects[i].loc.y);
    }

    // TO-DO: ADD IN PRINTING LIVES, SCORE, TIME LEFT.

}


////////////////////////
// MOVEMENT FUNCTIONS //
////////////////////////


/*
 * This method will be called in response to a directional input from the SNES controller.
 * int direction () can be changed to be the direct output from the controller instead of
 * converting the input to an integer. A directional input will check to ensure that DK
 * will still be on the screen and will not collide hit with an object.
 */

/*
void DKmove(int *buttons, struct gamestate state)
{
    int direction;
    if (buttons[4] == 0)
    {
        direction = 3;
    }
    else if (buttons[5] == 0)
    {
        direction = 4;
    }
    else if (buttons[7] == 0)
    {
        direction = 1;
    }
    else if (buttons[6] == 0)
    {
        direction = 2;
    }

    if (direction == 1)
    {                                           // Right
        if (state.positions[0].x + 1 <= 22) // Ensure that DK does not step outside of screen
        {
            if (checkCollision(1) != 1)
            { // Check if DK will collide with an Object
                state.positions[0].x += state.dk.speed;
                state.dk.collision = 0;
            }
            else
            {
                state.dk.collision = 1;
            }
        }
    }

    else if (direction == 2)
    {                                          // Left
        if (state.positions[0].x - 1 >= 0) // Ensure that DK does not step outside of screen
        {
            if (checkCollision(2) != 1)
            {
                state.positions[0].x -= state.dk.speed;
                state.dk.collision = 0;
            }
            else
            {
                state.dk.collision = 1;
            }
        }
    }

    else if (direction == 3)
    {                                          // Up
        if (state.positions[0].y - 1 >= 0) // Ensure that DK does not step outside of screen
        {
            if (checkCollision(3) != 1)
            {
                state.positions[0].y -= state.dk.speed;
                state.dk.collision = 0;
            }
            else
            {
                state.dk.collision = 1;
            }
        }
    }

    else if (direction == 4)
    {                                           // Down
        if (state.positions[0].y + 1 <= 20) // Ensure that DK does not step outside of screen
        {
            if (checkCollision(4) != 1)
            {
                state.positions[0].y += state.dk.speed;
                state.dk.collision = 0;
            }
            else
            {
                state.dk.collision = 1;
            }
        }
    }
}
*/


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


// TO-DO: GET TO TAKE GAMESTATE PTR AS ARGUMENT RATHER THAN AN OBJECT PTR
// Gets sprite to jump, prints this to the screen.
void jump(struct object *jumper_ptr, int *btns) {
    for (int i = 0; i <= JUMPHEIGHT; ++i) {
        // Print sprite at location (offx, offy - i).
        // Check to see if left or right is pressed and update offx accordingly.
        read_SNES(buttons);
        move_horz(buttons, &((*jumper_ptr).loc.x));
        --((*jumper_ptr).loc.y);		// Decrement offy each iteration.
        draw_image((*jumper_ptr).sprite, (*jumper_ptr).loc.x, (*jumper_ptr).loc.y);
    }
    // Get sprite to fall back down...
    for (int i = JUMPHEIGHT; i >= 0; --i) {
        // Print sprite at location (offx, offy - i)
        // Check to see if left or right is pressed and update offx accordingly.
        read_SNES(buttons);
        move_horz(buttons, &((*jumper_ptr).loc.x));      
        ++(*jumper_ptr).loc.y;			// Increment *offy each iteration.
        draw_image((*jumper_ptr).sprite, (*jumper_ptr).loc.x, (*jumper_ptr).loc.y);
    }
}



//////////
// MAIN //
//////////

int main()
{

// Define printf using provided uart methods.
void printf(char *str) {
	uart_puts(str);
}

/////////////////////////////
// First, set up driver... //
/////////////////////////////

// Initialize buttons array to all 1s.
int buttons[16];
for (int i = 0; i < 16; ++i) buttons[i] = 1;

// Initialize SNES lines and frame buffer.
init_snes_lines();
fb_init();

// TO-DO: Display start menu (for now test image in top left corner)

myDrawImage(dk_image.pixel_data, dk_image.width, dk_image.height, 100, 100);

// Wait for start button to be pressed...
while (1) {
    read_SNES(buttons);                     // Read buttons.
	if (buttons[4 - 1] == 0) break;			// Break if start is pressed.
}

/////////////////
// FIRST STAGE //
/////////////////

// Set up game state...

struct gamestate state;

state.score = 0;
state.lives = 4;
state.time = 1000;

// Background image for stage 1...

state.background.img = green_image.pixel_data;
state.background.width = green_image.width;
state.background.height = green_image.height;

// Objects for stage 1... for now just DK.

state.objects[0].sprite.img = dk_image.pixel_data;
state.objects[0].sprite.width = dk_image.width;
state.objects[0].sprite.height = dk_image.height;

state.objects[0].collision = 0;

// DK starting location
state.objects[0].loc.x = 50;
state.objects[0].loc.y = 900;

state.num_objects = 1;

// this loop will run while we're in the first stage - break if DK exits stage (moves off the screen?)
while (1) {

    // Read controller.
    read_SNES(buttons);

    // Move DK accordingly.
    move_horz(buttons, &state.objects[0].loc.x);

    // Jump with the B button (button code 1)
    if (buttons[0] == 0) {
        // Get DK to jump...
        jump(state.objects, buttons);
    }

    // Draw gamestate - not working, makefile error. Asked TA, temporarily replacing with body of draw_state.
    // draw_state(state);
    
    // draw_state:
    
    // First, draw background at the origin...
    draw_image(state.background, 0, 0);
    // Draw each object...
    for (int i = 0; i < state.num_objects; ++i) {
        // Print state.objects[i]
        draw_image(state.objects[i].sprite, state.objects[i].loc.x, state.objects[i].loc.y);
    }
    
    
    
}

return 1;


}
