#include "gpio.h"
#include "uart.h"
#include "fb.h"

// include images
#include "dk_image.h"
#include "enemy.h"
#include "coin.h"
#include "health.h"

#define MAXOBJECTS 30
#define SCREENWIDTH 1888
#define SCREENHEIGHT 1000
#define JUMPHEIGHT 100

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
    struct coord loc;           // Coordinate location.
    int speed;

    // enemy_direction determines which direction moving enemies will walk.
    // 0 for left, 1 for right.
    int enemy_direction;
    
    // Immunity flag, only used for dk.
    int dk_immunity;

    // Pack type flags, only used for packs.
    int health_pack;
    int point_pack;

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
    int num_enemies;        // Actual # of enemies in world.

    struct object packs[MAXOBJECTS];
    int num_packs;          // Actual # of packs in world.
    
    // Also track dk.
    
    struct object dk;

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

// Draws an int at specified pixel offsets (right end of number at offx)
void draw_int(unsigned int n, int offx, int offy, unsigned char attr) {
    while (n > 0) {
	// Print the smallest digit of n.
	char digit = (n % 10) + 48;
	drawChar(digit, offx, offy, attr);
	offx -= FONT_WIDTH;
	n = n / 10;
    }
}

// Draws an image structure at specified pixel offsets
void draw_image(struct image myimg, int offx, int offy) {
    myDrawImage(myimg.img, myimg.width, myimg.height, offx, offy);
}


// Main drawing method - draws a game state.
// Coordinates of all objects are in grid coords, so need to convert these to pixel
// coords in order to draw.
void draw_state(struct gamestate state) {

    // First, draw background at the origin...
    draw_image(state.background, 0, 0);
    
    // Draw DK...
    draw_image(state.dk.sprite, state.dk.loc.x * (SCREENWIDTH / state.width), state.dk.loc.y * (SCREENHEIGHT / state.height));
    
    // Draw each enemy...
    for (int i = 0; i < state.num_enemies; ++i) {
        // Print state.enemies[i] with grid coords (x, y) at location (x * SCREENWIDTH/state.width, y * SCREENHEIGHT/state.height)
        draw_image(state.enemies[i].sprite, state.enemies[i].loc.x * (SCREENWIDTH / state.width), state.enemies[i].loc.y * (SCREENHEIGHT / state.height));
    }
    
    // Draw each pack...
    for (int i = 0; i < state.num_packs; ++i) {
        // Print state.packs[i] with grid coords (x, y) at location (x * SCREENWIDTH/state.width, y * SCREENHEIGHT/state.height)
        draw_image(state.packs[i].sprite, state.packs[i].loc.x * (SCREENWIDTH / state.width), state.packs[i].loc.y * (SCREENHEIGHT / state.height));
    }

    // Print score...
    draw_int(state.score, SCREENWIDTH, FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, FONT_HEIGHT, "SCORE:", 0xF);

    // Print time remaining...
    draw_int(state.time, SCREENWIDTH, 2*FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, 2*FONT_HEIGHT, "TIME:", 0xF);

    // Print lives remaining... (replace with hearts later)    
    draw_int(state.lives, SCREENWIDTH, 3*FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, 3*FONT_HEIGHT, "LIVES:", 0xF);
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

    int pressed = 0;

    if (buttons[7] == 0)
    {                                           // Right
        if ((*state).dk.loc.x + (*state).dk.speed <= (*state).width) 
        // Ensure that DK does not step outside of screen
        {
	    uart_puts("Right\n");
	    pressed = 7;

            if (1)
            { // Check if DK will collide with an Object
                (*state).dk.loc.x += (*state).dk.speed;
                (*state).dk.collision = 0;
            }
            else
            {
                (*state).dk.collision = 1;
            }
        }
    }

    else if (buttons[6] == 0)
    {                                          // Left
        if ((*state).dk.loc.x - (*state).dk.speed >= 0) 
        // Ensure that DK does not step outside of screen
        {
	    uart_puts("Left\n");
	    pressed = 6;

            if (1)
            {
                (*state).dk.loc.x -= (*state).dk.speed;
                (*state).dk.collision = 0;
            }
            else
            {
                (*state).dk.collision = 1;
            }
        }
    }

    else if (buttons[4] == 0)
    {                                          // Up
        if ((*state).dk.loc.y - (*state).dk.speed >= 0) 
        // Ensure that DK does not step outside of screen
        {
	    uart_puts("Up\n");
	    pressed = 4;

            if (1)
            {
                (*state).dk.loc.y -= (*state).dk.speed;
                (*state).dk.collision = 0;
            }
            else
            {
                (*state).dk.collision = 1;
            }
        }
    }

    else if (buttons[5] == 0)
    {                                           // Down
        if ((*state).dk.loc.y + (*state).dk.speed <= (*state).height) 
        // Ensure that DK does not step outside of screen
        {
	    uart_puts("Down\n");
	    pressed = 5;

            if (1)
            {
                (*state).dk.loc.y += (*state).dk.speed;
                (*state).dk.collision = 0;
            }
            else
            {
                (*state).dk.collision = 1;
            }
        }
    }

    // If DK moved, he loses his immunity.
    if (pressed > 0) (*state).dk.dk_immunity = 0;

    // Wait for pressed joypad button to be unpressed before function can be exited
    
    while (pressed > 0) {
	read_SNES(buttons);
	if (buttons[pressed] == 1) pressed = 0;
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
void move_enemy(struct object *ob_ptr, int width) {
    if ((*ob_ptr).enemy_direction == 0) {
	// Move left.
	if ((*ob_ptr).loc.x - (*ob_ptr).speed >= 0) (*ob_ptr).loc.x -= (*ob_ptr).speed;
	else (*ob_ptr).enemy_direction = 1;
    } else {
	// Move right.
	if ((*ob_ptr).loc.x + (*ob_ptr).speed <= width) (*ob_ptr).loc.x += (*ob_ptr).speed;
	else (*ob_ptr).enemy_direction = 0;	
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

state.winflag = 0;
state.loseflag = 0;

// Background image for stage 1...

state.background.img = dk_image.pixel_data;
state.background.width = dk_image.width;
state.background.height = dk_image.height;

// Objects for stage 1...

// DK

state.dk.sprite.img = dk_image.pixel_data;
state.dk.sprite.width = dk_image.width;
state.dk.sprite.height = dk_image.height;

state.dk.loc.x = 2;
state.dk.loc.y = 16;

state.dk.speed = 1;
state.dk.dk_immunity = 0;

// Enemies

state.num_enemies = 2;

for (int i = 0; i < 2; ++i) {
    state.enemies[i].sprite.img = enemy_image.pixel_data;
    state.enemies[i].sprite.width = enemy_image.width;
    state.enemies[i].sprite.height = enemy_image.height;

    state.enemies[i].speed = 1;
    state.enemies[i].enemy_direction = 0;
}

state.enemies[0].loc.x = 10;
state.enemies[0].loc.y = 12;

state.enemies[1].loc.x = 15;
state.enemies[1].loc.y = 18;

// Packs

state.num_packs = 2;

// Health packs...

for (int i = 0; i < 1; ++i) {
    state.packs[i].sprite.img = health_image.pixel_data;
    state.packs[i].sprite.width = health_image.width;
    state.packs[i].sprite.height = health_image.height;

    state.packs[i].health_pack = 1;
    state.packs[i].point_pack = 0;
}

state.packs[0].loc.x = 14;
state.packs[0].loc.y = 6;

// Point packs...

for (int i = 1; i < 2; ++i) {
    state.packs[i].sprite.img = coin_image.pixel_data;
    state.packs[i].sprite.width = coin_image.width;
    state.packs[i].sprite.height = coin_image.height;

    state.packs[i].health_pack = 0;
    state.packs[i].point_pack = 1;
}

state.packs[1].loc.x = 3;
state.packs[1].loc.y = 4;

// Record time in microseconds when first stage began...
int initial_time = *clo; 

// this loop will run while we're in the first level - break if either win flag or lose flag is set.
while (!state.winflag && !state.loseflag) {
  
    // Read controller.
    read_SNES(buttons);

    // Move DK accordingly.
    DKmove(buttons, &state);

    // For now, enemies are stationary.
    
    // Check to see if DK has collided with an enemy. DK can only be hurt if his immunity is turned off.
    if (!state.dk.dk_immunity){
        for (int i = 0; i < state.num_enemies; ++i) {
	        if (state.dk.loc.x == state.enemies[i].loc.x && state.dk.loc.y == state.enemies[i].loc.y) {
		        --state.lives;
		        printf("Lost a life\n");
		        // Give DK immunity - he can't be hurt until he leaves this cell.
		        state.dk.dk_immunity = 1;
		        // Set lose flag if out of lives.
		        if (state.lives == 0) state.loseflag = 1;
	        }
        }
    }


    // Check to see if DK has collided with a pack...

    for (int i = 0; i < state.num_packs; ++i) {
        if (state.dk.loc.x == state.packs[i].loc.x && state.dk.loc.y == state.packs[i].loc.y) {
            // See which kind of pack DK has collided with, update gamestate accordingly.
            if (state.packs[i].health_pack) {
                // Give DK an extra life if he has less than 4.
                if (state.lives < 4) ++state.lives;
            } else if (state.packs[i].point_pack) {
                // Give DK 1000 points.
                state.score += 1000;
            }
        }
    }

    // Check to see if DK has reached the top of the screen (end of level)
    if (state.dk.loc.y == 0) state.winflag = 1;


    // Update time remaining... (this should work as long as clock register does not reset)
    state.time = 1000 - ((*clo - initial_time)/1000);

    // Draw the game state... (have to insert function body)
    
    // First, draw background at the origin...
    draw_image(state.background, 0, 0);
    
    // Draw DK...
    draw_image(state.dk.sprite, state.dk.loc.x * (SCREENWIDTH / state.width), state.dk.loc.y * (SCREENHEIGHT / state.height));
    
    // Draw each enemy...
    for (int i = 0; i < state.num_enemies; ++i) {
        // Print state.enemies[i] with grid coords (x, y) at location (x * SCREENWIDTH/state.width, y * SCREENHEIGHT/state.height)
        draw_image(state.enemies[i].sprite, state.enemies[i].loc.x * (SCREENWIDTH / state.width), state.enemies[i].loc.y * (SCREENHEIGHT / state.height));
    }
    
    // Draw each pack...
    for (int i = 0; i < state.num_packs; ++i) {
        // Print state.packs[i] with grid coords (x, y) at location (x * SCREENWIDTH/state.width, y * SCREENHEIGHT/state.height)
        draw_image(state.packs[i].sprite, state.packs[i].loc.x * (SCREENWIDTH / state.width), state.packs[i].loc.y * (SCREENHEIGHT / state.height));
    }
    
    // Print score...
    draw_int(state.score, SCREENWIDTH, FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, FONT_HEIGHT, "SCORE:", 0xF);

    // Print time remaining...
    draw_int(state.time, SCREENWIDTH, 2*FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, 2*FONT_HEIGHT, "TIME:", 0xF);

    // Print lives remaining... (replace with hearts later)    
    draw_int(state.lives, SCREENWIDTH, 3*FONT_HEIGHT, 0xF);
    drawString(SCREENWIDTH - 200, 3*FONT_HEIGHT, "LIVES:", 0xF);    

    // End of drawing game state.
}

// First stage exited. If lost, print game over. If won, move on to next stage.

if (state.loseflag) {
    drawString(500, 500, "Game over!", 0xF);
    state.loseflag = 0;
    // TO-DO: Add goto menu here.
    return 1;
}

// First stage won! Move on to next stage...

drawString(500, 500, "First stage won!", 0xF);
state.winflag = 0;


return 1;


}