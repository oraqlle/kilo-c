#include <termios.h>
#include <unistd.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define CTRL_KEY(key) (key) & 0x1f

static struct termios orig_termios;

void die(const char *str) {
    perror(str);
    exit(1);
}

void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("disable_raw_mode :: tcsetattr");
    }
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("enable_raw_mode :: tcgetattr");
    }

    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("enable_raw_mode :: tcsetattr");
    }
}

char editor_read_key() {
    int nread = 0;
    char c = '\0';

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("editor_read_key :: read");
        }
    }

    return c;
}

void editor_process_keypress() {
    char c = editor_read_key();

    switch (c) {
        case CTRL_KEY('q'):
            exit(0);
            break;
    }
}

int main() {
    enable_raw_mode();

    while (1) {
        editor_process_keypress();
    }

    return 0;
}
