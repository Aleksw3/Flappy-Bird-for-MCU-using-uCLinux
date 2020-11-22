#include <stdio.h> //printf
#include <sys/mman.h>
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h>
#include <linux/fb.h> //framebuffer


#include "font8x8.h"
#include "function_def.h"
#include "colors.h"

struct fb_copyarea rect; 		//Defines the area that is updated in the framebuffer

void update_screen(int x, int y, int width, int height){
	/*
		Select area of the screen to update. 
		Refreshing the entire screen is pointless if only a small area has changed.
		(x,y) is the coordinates to the upper left corner, and width and height
		defines the size of the square to update
	*/
	rect.dx = x;
	rect.dy = y;
	rect.width  = width;
	rect.height = height;
	ioctl(fbfd, 0x4680, &rect);
}


void draw_item(int x, int y, int width, int height, uint16_t color, uint8_t* pixel_map, bool update)
{
	/*Can draw any item
      For a simple box just add coordinates, width, height, color, and set pixel map to NULL
	  To draw graphics, send the pixel map array as a pointer. Width and height defines the size
	  of the pixel map. 
	  TODO: FIgure out how to add color to a pixel map. 
	*/
	if(pixel_map == NULL)
	{
		for(int row_y=y; row_y < height+y; row_y++){
			for(int col_x=x; col_x < width+x; col_x++)
				fbp[col_x + row_y*SCREEN_WIDTH] = color;
		}
	}
	else{
		printf("You're on the wrong side of town cowboy\n");
		//insert pixel map function here
		// very similar to previous, but will draw
		// what is sent through the pixel map
		// pointer
	}
	if(update)
		update_screen(x, y, width, height);
}
void erase_item(int x, int y, int width, int height,uint16_t fill_color, bool update)
{
	/*
		Delete the item at an area by setting it to the background. 
		Same as draw function

	*/
	for(int row_y=y; row_y < height+y; row_y++){
		for(int col_x=x; col_x < width+x; col_x++)
			fbp[col_x + row_y*SCREEN_WIDTH] = fill_color;
	}
	if(update)
		update_screen(x, y, width, height);
}

void display_char(int x, int y, unsigned char ch, uint16_t color)
{
	/*
		Loop through the 8x8 chars saved in the font8x8 array.
		(x,y) are coordinates for the upper left corner
		ch is an 8-bit char. 
	*/
	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 8; j++){
			if(((font8x8_basic[ch][i])>>j) & 1)
				fbp[(x+j)+(i+y)*SCREEN_WIDTH]=color;
		}
	}
}

void display_string(int x, int y, char string[], int length, uint16_t color, bool update)
{
	/*
		Loops through all chars in the input strings
		and writes it to the LCD screen at the given
		coordinates
		(x,y) coordinates for the upper left corner
		length is the number of chars in the string
	*/
	printf("Writing: %s\n", string);
	int i = 0; // Defined outside of loop because i is used when function is done
	for(; i < length;i++){
		/*
		   x coordinate will increase by 8 for the width of the char
		   and 1 for a space inbetween
		*/
		if(x+(i*8)+8+y*SCREEN_WIDTH > SCREEN_WIDTH*SCREEN_HEIGHT ){
			//check that the new char is not outside of the buffer
			return;
		}
		display_char(x+(i*8),y, string[i],color); 
	}
	//Update area of the text in the framebuffer
	if(update)
		update_screen(x,y,8*i,10);
}

