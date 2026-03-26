#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <signal.h>
#include <string.h>

static struct termios old_termios, new_termios;

#define MAX_FRAME_KEYS 4
#define FRAME_NS 16666667

static int exit_loop;

void reset_terminal() {
	printf("\e[m"); // esc seq for reset color changes
	printf("\e[?25h"); // show cursor 
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

	printf("\e[?25l"); // escape sequence for hide cursor
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


int main() {
	configure_terminal();

	signal(SIGINT, signal_handler);

	struct timespec start = {};
	struct timespec end = {};
	struct timespec sleep = {};

	int keys[MAX_FRAME_KEYS];

	while(!exit_loop) {
		clock_gettime(CLOCK_MONOTONIC, &start);

		read_input(keys);
		print_keys(keys);
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