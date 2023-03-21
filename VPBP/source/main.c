#include <unistd.h>
#include <stdio.h>
#include "gpio.h"
#include "uart.h"
#include "fb.h"

#define GPIO_BASE 0xFE200000

int main()
{

void printf(char *str) {
	uart_puts(str);
}
	fb_init();
	drawRect(100, 100, 300, 300, 0xe, 0);
	
	printf("Hello world\n");

	printf("Program is terminating\n");
    	return 0;
}
