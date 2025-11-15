#include <string.h>
#include <sys/ioctl.h>

#include <termios.h>
#include <unistd.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(key) (key) & 0x1f

typedef struct {
    unsigned screen_rows;
    unsigned screen_cols;
    struct termios orig_termios;
} editor_config;

static editor_config e_config;

typedef struct {
    char *data;
    unsigned len;
} abuf;

#define ABUF_INIT {NULL, 0}

void abuf_append(abuf *ab, const char *str, unsigned len) {
    char *new = realloc(ab->data, ab->len + len);

    if (new == NULL) {
        return;
    }

    memcpy(&new[ab->len], str, len);
    ab->data = new;
    ab->len += len;
}

void abuf_free(abuf *ab) {
    free(ab->data);
    ab->data = NULL;
    ab->len = 0;
}

void die(const char *str) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(str);
    exit(1);
}

void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &e_config.orig_termios) == -1) {
        die("disable_raw_mode :: tcsetattr");
    }
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &e_config.orig_termios) == -1) {
        die("enable_raw_mode :: tcgetattr");
    }

    atexit(disable_raw_mode);

    struct termios raw = e_config.orig_termios;
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
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

void editor_draw_rows(abuf *ab) {
    for (unsigned y = 0; y < e_config.screen_rows; y++) {
        if (y == e_config.screen_rows / 3) {
            char welcome[80] = {0};
            unsigned welcome_len = snprintf(welcome, sizeof(welcome),
                                            "Kilo Editor -- version %s", KILO_VERSION);

            if (welcome_len > e_config.screen_cols) {
                welcome_len = e_config.screen_cols;
            }

            unsigned padding = (e_config.screen_cols - welcome_len) / 2;

            if (padding != 0) {
                abuf_append(ab, "~", 1);
            }

            while (padding--) {
                abuf_append(ab, " ", 1);
            }

            abuf_append(ab, welcome, welcome_len);

        } else {
            abuf_append(ab, "~", 1);
        }

        abuf_append(ab, "\x1b[K", 4);
        if (y < e_config.screen_rows - 1) {
            abuf_append(ab, "\r\n", 2);
        }
    }
}

void editor_refresh_screen() {
    abuf ab = ABUF_INIT;

    abuf_append(&ab, "\x1b[?25l", 6);
    abuf_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);
    abuf_append(&ab, "\x1b[H", 3);
    abuf_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.data, ab.len);
    abuf_free(&ab);
}

int get_cursor_position(unsigned *rows, unsigned *cols) {
    char buf[32] = {0};
    unsigned i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            break;
        }

        if (buf[i] == 'R') {
            break;
        }

        i += 1;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') {
        return -1;
    }

    if (sscanf(&buf[2], "%u;%u", rows, cols) != 2) {
        return -1;
    }

    return 0;
}

int get_window_size(unsigned *rows, unsigned *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }

        return get_cursor_position(rows, cols);
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

void init_editor() {
    if (get_window_size(&e_config.screen_rows, &e_config.screen_cols) == -1) {
        die("init_editor :: get_window_size");
    }
}

int main() {
    enable_raw_mode();
    init_editor();

    while (1) {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return 0;
}
