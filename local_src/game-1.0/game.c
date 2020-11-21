#define _GNU_SOURCE

#include <stdio.h> //printf
#include <linux/fb.h> //framebuffer
//declares uintx_t
#include <sys/mman.h> // adds mmap and ioctl functions
#include <unistd.h>  //adds close() and usleep()
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <stdbool.h> // includes bool

#include "colors.h"
#include "function_def.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define SCREENBORDER 2*SCREEN_HEIGHT/3
#define SELECTED     1
#define NOT_SELECTED 0

#define FRONTSCREEN 0
#define GAMESCREEN 1
#define SCORESCREEN 2
#define EXITGAME 3
#define GAMEOVERSCREEN 4

#define GAME_TICK_TIME 20000
#define FRONTSCREEN_BACKGROUND_COLOR BLACK

// Defines for player
#define PLAYER_GRAVITY 1
#define STARTPOSITION 80 // height the bird will spawn at
#define BIRDPOSX SCREEN_WIDTH/4
#define BIRDSIZE 16
#define BIRDWIDTH 16
#define BIRDHEIGHT 16
#define MAX_PLAYER_VELOCITY 4

// Defines for pillars
#define PILLAR_WIDTH 30
#define DISTANCE_BETWEEN_PILLARS 180 //lowest distance for 3 pillars - 117
#define PILLAR_GAP 90
#define PILLAR_SPEED 2

/*Framebuffer variables*/
int fbfd; 						//File open
struct fb_var_screeninfo vinfo; //Can receive info about the screen - resolution
uint16_t* fbp; 					//Memory mapping - 16 bits per pixel on the screen


int resolution;


enum direction{UP,DOWN,RIGHT,LEFT,ACTION} dir;



/*GPIO variables*/
int gpio; //File open

struct Game_frontscreen frontscreen = {
	 FRONTSCREEN,
	 3, //number of links on the frontscreen
	 0,
	{
		{SCREEN_WIDTH/2 - 8/2*8 , SCREEN_HEIGHT/3+00, "New Game",   8, SELECTED,    0},
		{SCREEN_WIDTH/2 - 10/2*8, SCREEN_HEIGHT/3+10, "Highscores",10, NOT_SELECTED,1},
		{SCREEN_WIDTH/2 - 10/2*8, SCREEN_HEIGHT/3+20, "Exit Game",  9, NOT_SELECTED,2},
	}
};
struct screens curr_screen;

struct Game_play game_play = {
		GAMESCREEN, //ID
		0, // player's score
		"000",
		3, // number of pillars
	 {
	//{x_position, y_gap_center, gave_score}
		{0, 0, 0, NOT_SELECTED}, // pillar 1
		{0, 0, 0, NOT_SELECTED}, // pillar 2
		{0, 0, 0, NOT_SELECTED}, // pillar 3
	 }
};
struct Player player = {
			STARTPOSITION,
			0, // player's velocity
			10, // player's boost
			0, // game ticks since last button push
};
struct Game_highscore game_highscore = {
		SCORESCREEN,
		{
			{"000"}, {"000"}, {"000"}, {"000"}, {"000"},
		}
};
struct Game_over game_over = { GAMEOVERSCREEN };
// struct Game_exit game_exit = { 0, };


/*Function declarations*/

int init_gpio();
void sigio_handler(int);
void start_screen();
void selected_background(int,int,int,int,bool);

void spawn_map();
void draw_bird(int);
void update_bird();
void remove_bird(int);
void display_score();
int collision();
void init_pillar();
void spawn_pillar();
void update_pillar();
void remove_pillar(int);
void draw_pillar(int);
enum direction button_event(enum direction);

/*----------------------------------------------------------------------*/
/*                                 MAIN                                 */
/*----------------------------------------------------------------------*/

int main(int argc, char *argv[]){

	fbfd = open("/dev/fb0",O_RDWR); //Open framebuffer driver
	ioctl(fbfd,FBIOGET_VSCREENINFO, &vinfo); //Get information about screen

	resolution = vinfo.xres*vinfo.yres; //Size of the framebuffer
	fbp = mmap(NULL, resolution, PROT_READ | PROT_WRITE,MAP_SHARED, fbfd, 0);

	init_gpio();

	//Set background and title
	// draw_item(0,0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK, NULL, true);
	// usleep(5);


	srand(time(0)); // Init of random gen for pillar generate
	start_screen();
	while(curr_screen.exit==false){
		if(curr_screen.id_current_screen == FRONTSCREEN){
			usleep(10);
		}else if(curr_screen.id_current_screen == GAMESCREEN){ // is the game running?
			//  - add game-tick timer(https://developer.ibm.com/tutorials/l-timers-list/)?
			usleep(GAME_TICK_TIME);
			update_pillar();
			update_bird();
			if (collision() != 0){ // check for collision. if not, update score
				//save_score();
				printf("Collision detected\n");
				curr_screen.id_current_screen = GAMEOVERSCREEN;
			}
			display_score();
			// wait for game-tick timer wakeup, can use delay until timer is implemented
		} else if(curr_screen.id_current_screen == GAMEOVERSCREEN) { 	// game over, freeze screen
			// Write "Game Over" in the middle of the screen
			printf("current screen is GAMEOVERSCREEN\n");
			
			display_string(SCREEN_WIDTH/2 - 9/2*8, SCREEN_HEIGHT/3+10, "Game Over", 9, WHITE, true);
			display_string(SCREEN_WIDTH/2 - 27*8/2, SCREEN_HEIGHT/3+20, "Push any button to continue", 27, WHITE, true);
			usleep(10);
			// reset to initial 
			player.position = STARTPOSITION;
			player.velocity = 0;

			while(curr_screen.id_current_screen == GAMEOVERSCREEN)
				usleep(10); //small delayed needed for the variable to be able to change
			start_screen();
		}
	};
	printf("Exiting");
	draw_item(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK, NULL, true);
	display_string(SCREEN_WIDTH/2 - 3*8, SCREEN_HEIGHT/3, "Bye Bye", 7, WHITE, true);// Lets draw something cool on exit

	// //Unmap and close file
	close(gpio);
	munmap(fbp, resolution);
	close(fbfd);
	exit(EXIT_SUCCESS);
	return 0;
}

//void save_score()
//{
//	for(int i = 0;i<5;i++){


//	}
//}

void display_score()
{
	display_string(SCREEN_WIDTH-40, 10, game_play.player_score_string , 3*8, BLACK, true);
}

int collision()
{
	/*
		Check for collisions:
		1. Collision with frame of screen
		2. Collision with pillars
	*/
	printf("Checking collision \n");
	if(player.position-BIRDSIZE/2 < 0 || player.position+BIRDSIZE/2 > SCREEN_HEIGHT){ // Top and bottom of screen

		return -1;
	}
	for(int i = 0; i < game_play.num_of_pillars; i++){
		// check if player is within the pillars
		if(BIRDPOSX < game_play.pillars[i].x_position + PILLAR_WIDTH && BIRDPOSX+BIRDSIZE > game_play.pillars[i].x_position){
			// PILLAR_X < BIRD < PILLAR_X+PILLAR_WIDTH
			if(player.position - BIRDHEIGHT/2 <= game_play.pillars[i].y_gap_center - PILLAR_GAP/2 || player.position+BIRDSIZE/2 >= game_play.pillars[i].y_gap_center + PILLAR_GAP/2){
				return -1;
			}
		}

		// check if score should be updated
		/*
			TODO: Place in own function
		*/
		if ((game_play.pillars[i].x_position) < BIRDPOSX - (BIRDSIZE/2)-PILLAR_WIDTH){
			if(game_play.pillars[i].gave_score == 0){
				game_play.pillars[i].gave_score = 1;
				game_play.player_score += 1;
				game_play.player_score_string[0] =  game_play.player_score % 10;
				game_play.player_score_string[1] = (game_play.player_score % 100)/10;
				game_play.player_score_string[2] =  game_play.player_score/100;
				printf("Score updated\n");
			}
		}
	}
	return 0; // collision did not occur.
}

void init_pillar()
{
	/*
		Initialize three pillars, 
		which is the maximum number of
		pillars that can be on the screen
		at the same time
	*/
	spawn_pillar(0, PILLAR_WIDTH+SCREEN_WIDTH);
	spawn_pillar(1, PILLAR_WIDTH+SCREEN_WIDTH+DISTANCE_BETWEEN_PILLARS);
	spawn_pillar(2, PILLAR_WIDTH+SCREEN_WIDTH+DISTANCE_BETWEEN_PILLARS*2);
}

void spawn_pillar(int pillarnr, int pos_x)
{
	/*
		Set position and randomize gap of the pillars
	*/
	game_play.pillars[pillarnr].x_position = pos_x;
	game_play.pillars[pillarnr].deltax     = 0;
	game_play.pillars[pillarnr].y_gap_center = rand() % (SCREEN_HEIGHT - 2*PILLAR_GAP) + PILLAR_GAP/2;
	game_play.pillars[pillarnr].gave_score   = 0;
}

void update_pillar()
{	
	/*
		Remove the pillars on screen,
		update the position
		redraw pillars
	*/
	for(int i = 0; i < game_play.num_of_pillars; i++){
		remove_pillar(i);
		game_play.pillars[i].deltax = PILLAR_SPEED;
		game_play.pillars[i].x_position -= PILLAR_SPEED;

		//if pillar is outside of the screen, set position to right side and randomize gap
		if(game_play.pillars[i].x_position+PILLAR_WIDTH <= 0)
			spawn_pillar(i,game_play.pillars[(i+5)%3].x_position + DISTANCE_BETWEEN_PILLARS);

		draw_pillar(i);
	}
}

void remove_pillar(int i){

	//How much of the pillar is on screen
	int on_screen;
	int xpos;

	if(game_play.pillars[i].x_position < 0){
		on_screen = PILLAR_WIDTH - abs(game_play.pillars[i].x_position); 
		xpos = 0;
	}else{
		on_screen = SCREEN_WIDTH - game_play.pillars[i].x_position; 
		xpos = game_play.pillars[i].x_position;	
	}
	if(on_screen > 0 ){
		//Pillar is on the screen
		if(on_screen > PILLAR_WIDTH){
			//Entire pillar is on the screen
			on_screen = PILLAR_WIDTH;
		}
		//remove uppermost pillar. Replace with blue
		draw_item(xpos, 0, on_screen, game_play.pillars[i].y_gap_center-PILLAR_GAP/2, BLUE, NULL, false);


		int blue_y =  game_play.pillars[i].y_gap_center+PILLAR_GAP/2;
		int blue_height = SCREENBORDER - blue_y;
		if(blue_height < 0)
			blue_height = 0;
		int green_y = blue_y + blue_height;
		int green_height = SCREEN_HEIGHT - green_y;

		draw_item(xpos, blue_y, on_screen, blue_height, BLUE, NULL, false);
		draw_item(xpos, green_y, on_screen, green_height, GREEN, NULL, false);
		
		update_screen(xpos, 0, on_screen + game_play.pillars[i].deltax, SCREEN_HEIGHT);
	}
}

void draw_pillar(int i)
{	
	//How much of the pillar is on screen
	int on_screen;
	int xpos;

	if(game_play.pillars[i].x_position < 0){
		on_screen = PILLAR_WIDTH - abs(game_play.pillars[i].x_position); 
		xpos = 0;
	}else{
		on_screen = SCREEN_WIDTH - game_play.pillars[i].x_position; 
		xpos = game_play.pillars[i].x_position;	
	}
	if(on_screen > 0 ){
		//Pillar is on the screen
		if(on_screen > PILLAR_WIDTH){
			//Entire pillar is on the screen
			on_screen = PILLAR_WIDTH;
		}
		draw_item(xpos, 0, on_screen, game_play.pillars[i].y_gap_center-PILLAR_GAP/2, WHITE, NULL, false);
		draw_item(xpos, game_play.pillars[i].y_gap_center+PILLAR_GAP/2, on_screen, SCREEN_HEIGHT - game_play.pillars[i].y_gap_center + PILLAR_GAP/2, WHITE, NULL, false);
		update_screen(xpos, 0, on_screen + game_play.pillars[i].deltax, SCREEN_HEIGHT);
	}
}

int init_gpio()
{
	/*
		Initialize gamepad
		Open file found in /dev/
		Connect the interrupt
	*/
	gpio = open("/dev/gamepad", O_RDWR);
	if(gpio < 0){
		printf("Error opening gpio driver - %d\n",gpio);
		return -1;
	}
	if(signal(SIGIO,&sigio_handler) == SIG_ERR){
		printf("Error assigning handler to signal\n");
		return -1;
	}
	if(fcntl(gpio,F_SETOWN,getpid())== -1){
		printf("Error assigning owner\n");
		return -1;
	}
	long oflags = fcntl(gpio,F_GETFL);
	if(fcntl(gpio,F_SETFL, oflags|FASYNC)){
		printf("Error setting flags\n");
		return -1;
	}
	return 0;
}

enum direction button_event(enum direction old_event){
	/*
		Find which button is pushed, by 
		reading the register of the GPIO through
		the driver, and return a direction/event
	*/
	uint32_t gamepad_status;
	read(gpio,&gamepad_status,1);
	int i = 0;
	while(!(gamepad_status>>i & 1) && i<8)
		i++;
	switch(i){
		case 1:
		case 5:
	      	return DOWN;
	      break;
	    case 3:
	    case 7:
	      	return UP;
	      break;
	    case 2:
	    case 6:
	      	return RIGHT;
	      break;
	    case 0:
	      	return LEFT;
	    case 4:
	    	return ACTION;
	      break;
	    default:
	    	break;
	}
	return old_event;
}

void sigio_handler(int no)
{
	/*
		This function runs everytime an event occurs on the gamepad
		The events of the function is dependent on the current screen
		and the button pushed
	*/
	dir = button_event(dir);
	if(curr_screen.id_current_screen == FRONTSCREEN){
		if(dir == ACTION){
			/*
				Enter the screen of he link pushed.
				Game screen, highscore, or exit the game
			*/
			printf("Trying to enter a screen\n");
			switch(frontscreen.position){
				case 0:
					spawn_map();
					game_play.player_score = 0;
					//game_play.player_score_string[0] = '0';
					//game_play.player_score_string[1] = '0';
					//game_play.player_score_string[2] = '0';
					memset(&game_play.player_score_string[0], 0, sizeof(game_play.player_score_string));
					curr_screen.id_current_screen = GAMESCREEN;
					printf("Should've entered game now\n");
					break;
				case 1:
					printf("Trying to enter scorescreen\n");
					// curr_screen.id_current_screen = SCORESCREEN;
					break;
				case 2:
					printf("Trying to exit\n");
					curr_screen.exit = true;
					break;
			}
			usleep(10);
			return;
		}else if(dir == UP || dir == DOWN){
			/*
				Update the background and position
				of the selected link on the frontscreen
			*/
			for(int i = 0; i < frontscreen.items; i++)
				frontscreen.links[i].status = NOT_SELECTED;

			if(dir == UP){
				if(++frontscreen.position > frontscreen.items - 1)
					frontscreen.position = 0;
			}else if(dir == DOWN){
				if(--frontscreen.position < 0)
					frontscreen.position = frontscreen.items - 1;
			}
			frontscreen.links[frontscreen.position].status = SELECTED;
		
			for(int i = 0;i<frontscreen.items;i++)
				selected_background(frontscreen.links[i].x,frontscreen.links[i].y,frontscreen.links[i].length,frontscreen.links[i].status, true);

			usleep(10);
			return;
		}
	} else if(curr_screen.id_current_screen == GAMESCREEN){
		/*
			Game is running, change velocity of player
			when button is pushed
		*/
		if(dir == ACTION){
			player.velocity = 7;
		}
		usleep(10);
		return;
	} else if(curr_screen.id_current_screen == GAMEOVERSCREEN){
		/*
			Game over, you died.
			Wait for used to push button and return to frontscreen
		*/
		curr_screen.id_current_screen = FRONTSCREEN;
		usleep(10);
		return;
	}



}

void start_screen()
{
	/*
		Set screen to background color
		Initialize start screen.
		Display the strings and backgrounds.
	*/
	draw_item(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FRONTSCREEN_BACKGROUND_COLOR, NULL, false);
	display_string(SCREEN_WIDTH/2 -(13*8)/2, 1, "Flappy dog", 10, WHITE, false);
	for(int i = 0; i < frontscreen.items; i++){
		display_string(frontscreen.links[i].x,frontscreen.links[i].y,frontscreen.links[i].string,frontscreen.links[i].length, WHITE, false);
		selected_background(frontscreen.links[i].x,frontscreen.links[i].y,frontscreen.links[i].length,frontscreen.links[i].status, false);
	}
	update_screen(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	curr_screen.id_current_screen = FRONTSCREEN;
	curr_screen.exit = false;
	frontscreen.position = 0;
}

void spawn_map()
{ 
	/*
		Draw background
		Initialize pillars
		Draw player
	*/
	printf("Spawn map??\n");
	draw_item(0, 0, SCREEN_WIDTH, SCREENBORDER, BLUE, NULL, false);
	draw_item(0, SCREENBORDER, SCREEN_WIDTH, SCREEN_HEIGHT - SCREENBORDER, GREEN, NULL, false);
	update_screen(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	init_pillar();
	draw_bird(player.position);
	
}

void update_bird()
{
	/*
		Remove bird at its current position
		Update position
		Redraw bird at new position
		Change speed
	*/
	remove_bird(player.position);
	player.position -= player.velocity; //update position
	draw_bird(player.position);
	player.velocity -= PLAYER_GRAVITY; //Update speed
}


void draw_bird(int pos_y)
{	
	// Check if the bird is going outside of the bounds
	if(pos_y - BIRDHEIGHT / 2 < 0)
		draw_item(BIRDPOSX, 0, BIRDWIDTH, BIRDHEIGHT/2 + pos_y, BLACK, NULL, true);
	else if(pos_y + BIRDHEIGHT / 2 > SCREEN_HEIGHT)
		draw_item(BIRDPOSX, pos_y - BIRDHEIGHT / 2, BIRDWIDTH, BIRDHEIGHT/2 - (pos_y - SCREEN_HEIGHT), BLACK, NULL, true);
	else
		draw_item(BIRDPOSX, pos_y - BIRDHEIGHT/2, BIRDWIDTH, BIRDHEIGHT, BLACK, NULL, true);
}


void remove_bird(int pos_y)
{
	/*
		Remove the bird. Replace with background color
		Figure out how much is in the green and blue area
	*/
	pos_y -= BIRDHEIGHT/2;
	int bird_in_sky = 0;
	int bird_on_grass = pos_y + BIRDHEIGHT - SCREENBORDER;
	int pos_y_grass = pos_y;


	// Figure out how much of the bird is in the sky and how much is in the sky
	if(bird_on_grass <= 0){
		bird_in_sky = BIRDSIZE;
		bird_on_grass = 0;
	}else if(bird_on_grass >= BIRDHEIGHT){
		bird_in_sky = 0;
		bird_on_grass = BIRDHEIGHT;
	}else{
		bird_in_sky = BIRDSIZE - bird_on_grass;
		pos_y_grass = pos_y + bird_in_sky;
	}
	if(bird_on_grass > 0)
		draw_item(BIRDPOSX, pos_y_grass, BIRDWIDTH, bird_on_grass, GREEN, NULL, true);

	if(bird_in_sky > 0)
		draw_item(BIRDPOSX, pos_y, BIRDWIDTH, bird_in_sky, BLUE, NULL, true);

}


void selected_background(int x,int y,int width, int status, bool update)
{
	/*
		Change background of a box without affecting the text that is there
		THis is to "select" hgihlight text in the main menu
	*/
	int height = 8;
	int start_xy = x+(y-1)*SCREEN_HEIGHT;
	int stop_xy = (x+width) + (y+height+2)*SCREEN_WIDTH;
	int font_color = WHITE;

	int selected_bkground = (status == 1)? GRAY : FRONTSCREEN_BACKGROUND_COLOR;

	for(int i = start_xy; i < stop_xy; i++){
		if(fbp[i] != font_color)
			fbp[i] = selected_bkground;
	}
	if(update)
		update_screen(x, y, width, height);
}
