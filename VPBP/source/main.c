#include "gpio.h"
#include "uart.h"
#include "fb.h"

// include images
#include "dk_image.h"
#include "enemy.h"

#define MAXOBJECTS 30
#define SCREENWIDTH 1888
#define SCREENHEIGHT 1000
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
    int speed;
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

    // Tracks an array of objects.
    struct object objects[MAXOBJECTS];
    int num_objects;        // Actual # of objects in world.

    // Flags.
    int winflag;
    int loseflag;
};

struct startMenu {
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

// Draws an image structure at specified pixel offsets.
void draw_image(struct image myimg, int offx, int offy) {
    myDrawImage(myimg.img, myimg.width, myimg.height, offx, offy);
}


// Main drawing method - draws a game state.
// Coordinates of all objects are in grid coords, so need to convert these to pixel
// coords in order to draw.
void draw_state(struct gamestate s) {
    // First, draw background at the origin...
    draw_image(s.background, 0, 0);
    // Draw each object...
    for (int i = 0; i < s.num_objects; ++i) {
        // Print state.objects[i] with grid coords (x, y) at location (x * SCREENWIDTH/state.width, y * SCREENHEIGHT/state.height)
        draw_image(s.objects[i].sprite, s.objects[i].loc.x * (SCREENWIDTH / s.width), s.objects[i].loc.y * (SCREENHEIGHT / s.height));
    }

    // TO-DO: ADD IN PRINTING LIVES, SCORE, TIME LEFT.

}


/*
* Start Menu option selection logic. Can either select "Start" or "Quit" in initial menu.
* Returns 1 if start is selected, -1 if quit is selected, and 0 if nothing has been selected.
*/
int startMenuSelectOption(int *buttons, struct startMenu *start){
    if(buttons[4] ==  0){
        // JP Up pressed - move to "start"
        (*start).startGameSelected = 1;
        (*start).quitGameSelected = 0;
        //Hover feature on "Start" Button
    }
    else if(buttons[5] ==  0){
        // JP Down pressed - move to "quit" button
        (*start).startGameSelected = 0;
        (*start).quitGameSelected = 1;
        //Hover feature on "Quit" Button
    }

    if(buttons[8] == 0){
        // 'A' is pressed
        if((*start).startGameSelected){
            // Begin game.
            return 1;   
        }
        else if((*start).quitGameSelected){
            //Quit Game
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
    if (buttons[4] == 0)
    {                                           // Right
        if (*state.objects[0].loc.x + *state.objects[0].speed <= *state.width) 
        // Ensure that DK does not step outside of screen
        {
            if (/*checkCollision(1) != 1*/ 1)
            { // Check if DK will collide with an Object
                *state.objects[0].loc.x += *state.objects[0].speed;
                *state.objects[0].collision = 0;
            }
            else
            {
                *state.objects[0].collision = 1;
            }
        }
    }

    else if (buttons[5] == 0)
    {                                          // Left
        if (*state.objects[0].loc.x - *state.objects[0].speed >= 0) 
        // Ensure that DK does not step outside of screen
        {
            if (/*checkCollision(2) != 1*/ 1)
            {
                *state.objects[0].loc.x -= *state.objects[0].speed;
                *state.objects[0].collision = 0;
            }
            else
            {
                *state.objects[0].collision = 1;
            }
        }
    }

    else if (buttons[7] == 0)
    {                                          // Up
        if (state.objects[0].loc.y - *state.objects[0].speed >= 0) 
        // Ensure that DK does not step outside of screen
        {
            if (/*checkCollision(3) != 1*/ 1)
            {
                *state.objects[0].loc.y -= *state.objects[0].speed;
                *state.objects[0].collision = 0;
            }
            else
            {
                *state.objects[0].collision = 1;
            }
        }
    }

    else if (buttons[6] == 0)
    {                                           // Down
        if (*state.objects[0].loc.y + *state.objects[0].speed <= *state.height) 
        // Ensure that DK does not step outside of screen
        {
            if (/*checkCollision(4) != 1*/ 1)
            {
                *state.objects[0].loc.y += *state.objects[0].speed;
                *state.objects[0].collision = 0;
            }
            else
            {
                *state.objects[0].collision = 1;
            }
        }
    }
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


// Randomly moves the passed object horizontally.
// To simulate randomness, takes an int argument (will be time once that's figured out).
void move_rand(struct object *ob_ptr, int time) {
    if (time % 3 == 0) {
        // Move right.
        (*ob_ptr).loc.x += 1;
    } else if (time % 3 == 1) {
        // Move left.
        (*ob_ptr).loc.x -= 1;
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

/////////////////////////////////
// START MENU - BASICALLY DONE //
/////////////////////////////////

struct startMenu sm;
sm.startGameSelected = 1;
sm.quitGameSelected = 0;
int start_flag = 0;
while (start_flag == 0) {
    // Display selection options with currently selected option being pointed to...
    if (sm.startGameSelected) {
    	drawString(300, 300, "-> START GAME", 0xF);
    	drawString(300, 350, "QUIT GAME", 0xF);
    } else if (sm.quitGameSelected) {
	drawString(300, 300, "START GAME", 0xF);
	drawString(300, 350, "-> QUIT GAME", 0xF);
    }
    // Loop while startMenuSelectOption returns 0 - so breaks when player presses
    // A on either start or quit option.
    read_SNES(buttons);
    // Pass address of sm so that flag attributes can be modified by function.
    start_flag = startMenuSelectOption(buttons, &sm);
}

if (start_flag == -1) {
    // Quit game...
    drawString(400, 400, "Exiting game...", 0xF);
    printf("Quitting game...\n");
    return 1;
}

drawString(400, 400, "Starting game...", 0xF);
printf("Starting game...\n");


/////////////////
// FIRST STAGE //
/////////////////

// Set up game state...

struct gamestate state;

state.width = 20;
state.height = 20;

state.score = 0;
state.lives = 4;
state.time = 1000;

// Background image for stage 1...

state.background.img = dk_image.pixel_data;
state.background.width = dk_image.width;
state.background.height = dk_image.height;

// Objects for stage 1... DK and an enemy...

state.num_objects = 2;

// DK

state.objects[0].sprite.img = dk_image.pixel_data;
state.objects[0].sprite.width = dk_image.width;
state.objects[0].sprite.height = dk_image.height;

state.objects[0].collision = 0;

state.objects[0].loc.x = 50;
state.objects[0].loc.y = 900;

// Enemy

state.objects[1].sprite.img = enemy_image.pixel_data;
state.objects[1].sprite.width = enemy_image.width;
state.objects[1].sprite.height = enemy_image.height;

state.objects[1].collision = 0;

state.objects[1].loc.x = 1000;
state.objects[1].loc.y = 900;


// this loop will run while we're in the first stage - break if DK exits stage (moves off the screen?)
while (1) {

    // Read controller.
    read_SNES(buttons);

    // Move DK accordingly.
    DKmove(buttons, &state);

    // Move enemies randomly - for now one enemy, done manually:
    move_rand(&state.objects[1], 1);
    
    // Draw the game state...
    draw_state(state);
    
}

return 1;


}
