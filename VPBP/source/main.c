#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include "gpio.h"
#include "uart.h"
#include "fb.h"

// Image structures

#include "test-art.h"
#include "dk_image.c"

#include "gamestate.c"
#include "image.c"


#define MAXOBJECTS 30

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

// Array to track which buttons have been pressed.
int buttons[16];

// Array to track current locations of sprites on game map.
struct coord sprite_locs[MAXOBJECTS];

// Method to draw an image passed as an image structure at the specified coordinate.
void draw_image(struct image myimg, int offx, int offy) {
    myDrawImage(myimg.img, myimg.width, myimg.height, offx, offy);
}

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

// TO-DO: Display start menu (for now test image)

myDrawImage(test_image.pixel_data, test_image.width, test_image.height, 100, 100);

// Wait for start button to be pressed...
while (1) {
    read_SNES(buttons);                     // Read buttons.
	if (buttons[4 - 1] == 0) break;			// Break if start is pressed.
}

/////////////////
// FIRST STAGE //
/////////////////

// First initialize all structures we'll be using.

// Create dk structure.

struct DonkeyKong my_dk;
my_dk.sprite = dk_image;
my_dk.speed = 1;
my_dk.collision = 0;

// Create map1 structure.

struct gamemap map1;
map1.width = 20;
map1.height = 20;
map1.score = 0;
map1.lives = 4;
map1.time = 1000;

// Greate gamestate structure (THIS SAME STRUCTURE WILL BE USED THROUGHOUT
// THE ENTIRE PROGRAM)

struct gamestate state;
state.map = map1;
state.positions = struct coord locs[MAXOBJECTS];
// Initialize all positions to zero (for now)
for (int i = 0; i < MAXOBJECTS; ++i) {
    state.positions[i].x = 0;
    state.positions[i].y = 0;
}
state.num_objects = 1;                  // For now, only object is DK.
state.winflag = 0;
state.loseflag = 0;
state.dk = my_dk;

// To test, print DK and use controller to move him around without erasing old prints.
// Should be like the snake game.

// Initialize location of DK. CURRENTLY THESE ARE PIXEL COORDS - SHOULD BE GRID COORDS
state.positions[0].x = 100;
state.positions[0].y = 100;

while (1) {
    // Currently we're passing grid coords into draw_image, should be pixel coords.
    // TO-DO: edit draw_image so that it does take grid coords as arguments, but then converts
    // then to pixel coords before calling myDrawImage.

    // Read controller.
    read_SNES(buttons);
    // Move DK accordingly.
    DKmove(buttons, state);
    // Draw DK.
    draw_image(state.dk.sprite, state.positions[0].x, state.positions[0].y);
    // Break on start being pressed.
    if (buttons[4 - 1] == 0) break;
}

return 1;


}
