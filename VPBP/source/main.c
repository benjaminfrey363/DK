#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include "gpio.h"
#include "uart.h"
#include "fb.h"
#include "test-art.h"

#include "gamestate.c"

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


// TO-DO: Implement these methods!

// Draws gamemap map with background image described by img, width, height.
// Offsets for drawing will be 0.
void drawMap(struct gamemap map, unsigned char * img, int width, int height) {}


// Updates map - I think we can do this by checking object positions in the gamestate,
// and compare these to the sprite locations in the array sprite_locs. We can then only
// redraw those sprites whose locations have changed.
//
// QUESTION - how do we "undraw" a sprite in order to draw it somewhere else? Do we need to
// redraw the whole map?
void updateMap(struct gamestate state, unsigned char * img, int width, int height) {}



int main()
{

// Define printf using provided uart methods.
void printf(char *str) {
	uart_puts(str);
}

/////////////////////////////
// First, set up driver... //
/////////////////////////////

// Initialize buttons array.
int buttons[16];
for (int i = 0; i < 16; ++i) buttons[i] = 0;

// Initialize SNES lines and frame buffer.
init_snes_lines();
fb_init();

// TO-DO: Display start menu (for now test image)

myDrawImage(test_image.pixel_data, test_image.width, test_image.height, 100, 100);

// Wait for start button to be pressed...
while (1) {
	if (buttons[4 - 1] == 0) break;			// Break if start is pressed.
}

// Start game...

/////////////////
// FIRST STAGE //
/////////////////

myDrawImage(test_image.pixel_data, test_image.width, test_image.height, 200, 200);




return 1;


}
