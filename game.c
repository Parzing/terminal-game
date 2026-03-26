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

static struct termios old_termios, new_termios;

#define MAX_FRAME_KEYS 4
#define FRAME_NS 16666667

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define CURSOR_TO(x, y) (printf("\e[%d;%dH", (x) + 1, (y) + 1))

#define MAX_X 60
#define MAX_Y 26


static int exit_loop;

void reset_terminal() {
	RESET_COLOR;
	SHOW_CURSOR;
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

void signal_handler(int signum) {
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

int* read_input(int* keys) {
	char buf[4096];
	int n = read(STDIN_FILENO, buf, sizeof(buf));
	int key_num = 0;

	for (int i = 0; i <= n-3; i+= 3) {
		int key = read_key(buf, i);
		if (key == 0) {
			continue;
		}
		keys[key_num] = key;
		key_num++;
		if (key_num == 4) {
			break;
		}
	}
	return keys;
}

void print_keys(int* keys) {
	for (int i = 0; i < MAX_FRAME_KEYS; i++){
		switch (keys[i]) {
			case 1: printf("Up\n");		break;
			case 2: printf("Down\n");	break;
			case 3: printf("Right\n");	break;
			case 4: printf("Left\n");	break;
		}
	}
}

void clear_keys(int* keys) {
	for (int i = 0; i < MAX_FRAME_KEYS; i++) {
		keys[i] = 0;
	}
}

void handle_player(int* keys, int* pos_x, int* pos_y) {
	for (int i = 0; i < MAX_FRAME_KEYS; i++) {
		switch(keys[i]) {
			case 1:
				*pos_y = MAX(1, *pos_y - 1);
				break;
			case 2:
				// left + right sides for 2 thickness
				*pos_y = MIN(MAX_Y-2, *pos_y + 1);
				break;
			case 3:
				// top + bottom + newline char at bottom
				*pos_x = MIN(MAX_X-3, *pos_x + 1);
				break;
			case 4:
				*pos_x = MAX(1, *pos_x -1);
			default:
				break;
		}
	}
}

void render(int pos_x, int pos_y) {
	CLEAR_SCREEN;
	CURSOR_TO(0, 0);

	for(int i = 0; i < MAX_X - 1; i++) {
		printf("X");
	}
	printf("\n");

	for(int i = 1; i < MAX_Y - 1; i++) {
		printf("X");
		for (int j = 1; j < MAX_X - 2; j++) {
			if(pos_x == j && pos_y == i) {
				printf("@");
			}
			else {
				printf(" ");
			}
		}
		printf("X\n");
	}

	for (int i = 0; i < MAX_X - 1; i++) {
		printf("X");
	}

	fflush(stdout);
}



int main() {
	configure_terminal();

	signal(SIGINT, signal_handler);

	struct timespec start = {};
	struct timespec end = {};
	struct timespec sleep = {};

	int keys[MAX_FRAME_KEYS];

	int pos_x = 5;
	int pos_y = 5;

	while(!exit_loop) {
		clock_gettime(CLOCK_MONOTONIC, &start);

		read_input(keys);
		// print_keys(keys);
		handle_player(keys, &pos_x, &pos_y);
		render(pos_x, pos_y);
		clear_keys(keys);

		clock_gettime(CLOCK_MONOTONIC, &end);

		long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
		long remaining_ns = FRAME_NS - elapsed_ns;

		if (remaining_ns > 0) {
			sleep.tv_nsec = remaining_ns;
			nanosleep(&sleep, NULL);
    	}
	}
}