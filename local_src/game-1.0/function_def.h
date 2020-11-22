#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h> 
#include <signal.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <stdbool.h> 

/*Function declarations*/
void display_char(int, int, unsigned char, uint16_t);
void display_string(int, int, char [], int, uint16_t, bool);
void update_screen(int, int, int, int);
void draw_item(int, int, int, int, uint16_t, uint16_t*, bool);
void erase_item(int, int, int ,int ,uint16_t, bool);


#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

extern uint16_t* fbp;
extern struct fb_copyarea rect;
extern int fbfd;
extern uint16_t bird_map[];

struct Game_frontscreen{
	int id;
	int items;
	int position;
	struct menulink{
		int x;
		int y;
		char string[50];
		int length;
		int status;
		int order;
	}links[3];
};

struct screens{
	int id_current_screen;
	int change;
	bool exit;
};
struct Game_play{
	int id;
	int player_score;
	char player_score_string[3];
	int num_of_pillars;
	struct pillar{
		int x_position; //leftmost edge of the pillar(towards the player)
		int deltax;
		int y_gap_center; //gap between the top and the bottom of the opening between pillars
		int width;
		int gave_score;
	} pillars[3];
};
struct Player{
	int position;
	int velocity;
	int boost;
	int tick_since_action;
};
struct Game_highscore{
	int id;
	struct highscores{
		int player_score_int;
		char player_score_string[3];
	} highscore[5];
};
struct Game_over{
	int id;
};
struct Game_exit{
	int pressed;
};
