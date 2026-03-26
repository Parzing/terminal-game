#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <signal.h>
#include <string.h>

#define CLEAR_SCREEN (printf("\e[2J"))
#define HIDE_CURSOR (printf("\e[?25l"))
#define SHOW_CURSOR (printf("\e[?25h"))
#define RESET_COLOR (printf("\e[m"))



#define MAX_FRAME_KEYS 4
#define FRAME_NS 16666667

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define CURSOR_TO(x, y) (printf("\e[%d;%dH", (x) + 1, (y) + 1))

#define MAX_X 60
#define MAX_Y 26

static struct termios old_termios, new_termios;
static int exit_loop;

typedef struct {
	int keys[MAX_FRAME_KEYS];
	int pos_x;
	int pos_y;
	char old_screen[MAX_Y][MAX_X];
	char screen[MAX_Y][MAX_X];
} GameState;



void reset_terminal() {
	RESET_COLOR;
	SHOW_CURSOR;
	CURSOR_TO(MAX_Y+2, 0);
	fflush(stdout);
	tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
}

void configure_terminal() {
	tcgetattr(STDIN_FILENO, &old_termios);
	new_termios = old_termios;

	new_termios.c_lflag &= ~(ICANON | ECHO); // no echo + no canonical
	new_termios.c_cc[VMIN] = 0; // polling read
	new_termios.c_cc[VTIME] = 0;

	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	HIDE_CURSOR; // escape sequence for hide cursor
	atexit(reset_terminal);
}

void signal_handler(__attribute__((unused)) int signum) {
	exit_loop = 1;
}

int read_key(char* buf, int i) {
	if (buf[i] == '\033' && buf[i+1] == '[') {
		switch (buf[i+2]) {
			case 'A': return 1;
			case 'B': return 2;
			case 'C': return 3;
			case 'D': return 4;
		}
	}
	return 0;
}

void read_input(GameState* state) {
	char buf[4096];
	int n = read(STDIN_FILENO, buf, sizeof(buf));
	int key_num = 0;

	for (int i = 0; i <= n-3; i+= 3) {
		int key = read_key(buf, i);
		if (key == 0) {
			continue;
		}
		state->keys[key_num] = key;
		key_num++;
		if (key_num == MAX_FRAME_KEYS) {
			break;
		}
	}
}

void clear_keys(GameState* state) {
	for (int i = 0; i < MAX_FRAME_KEYS; i++) {
		state->keys[i] = 0;
	}
}

void handle_player(GameState* state) {
	for (int i = 0; i < MAX_FRAME_KEYS; i++) {
		switch(state->keys[i]) {
			case 1: // UP
				if (state->pos_y > 1) {
					state->screen[state->pos_y][state->pos_x] = ' ';
					state->screen[state->pos_y-1][state->pos_x] = '@';
					state->pos_y--;
				}
				break;
			case 2: // DOWN
				if (state->pos_y < MAX_Y-2) {
					state->screen[state->pos_y][state->pos_x] = ' ';
					state->screen[state->pos_y+1][state->pos_x] = '@';
					state->pos_y++;
				}
				break;
			case 3: // RIGHT
				if (state->pos_x < MAX_X - 3) {
					state->screen[state->pos_y][state->pos_x] = ' ';
					state->screen[state->pos_y][state->pos_x+1] = '@';
					state->pos_x++;
				}
				break;
			case 4: // LEFT
				if (state->pos_x > 1) {
					state->screen[state->pos_y][state->pos_x] = ' ';
					state->screen[state->pos_y][state->pos_x-1] = '@';
					state->pos_x--;
				}
				break;
			default:
				break;
		}
	}
	clear_keys(state);
}

void render(GameState *state) {
	for (int i = 0; i< MAX_Y; i++) {
		for (int j = 0; j < MAX_X; j++) {
			if(state->old_screen[i][j] != state->screen[i][j]) {
				CURSOR_TO(i, j);
				printf("%c", state->screen[i][j]);
			}
		}
	}
	fflush(stdout);
}


void update(GameState* state) {
	memcpy(state->screen, state->old_screen, sizeof(state->screen));
	handle_player(state);
}

void setup_board(GameState *state) {
	memset(state->screen, ' ', sizeof(state->screen));
	for( int i = 0; i < MAX_X - 1; i++) {
		state->screen[0][i] = 'X';
		state->screen[MAX_Y - 1][i] = 'X';
	}

	for (int j = 0; j < MAX_Y; j++) {
		state->screen[j][0] = 'X';
		state->screen[j][MAX_X-2] = 'X';
		state->screen[j][MAX_X-1] = '\n';
	}
	state->screen[state->pos_y][state->pos_x] = '@';
	render(state);
}


int main() {
	configure_terminal();

	signal(SIGINT, signal_handler);

	struct timespec start = {};
	struct timespec end = {};
	struct timespec sleep = {};

	CLEAR_SCREEN;
	GameState state = {
		.pos_x = 5,
		.pos_y = 5
	};
	setup_board(&state);



	while(!exit_loop) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		
		read_input(&state);
		update(&state);
		render(&state);
		memcpy(state.old_screen, state.screen, sizeof(state.screen));


		clock_gettime(CLOCK_MONOTONIC, &end);

		long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
		long remaining_ns = FRAME_NS - elapsed_ns;

		if (remaining_ns > 0) {
			sleep.tv_nsec = remaining_ns;
			nanosleep(&sleep, NULL);
    	}
	}
}