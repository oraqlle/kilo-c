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
    unsigned cx;
    unsigned cy;
    unsigned screen_rows;
    unsigned screen_cols;
    struct termios orig_termios;
} editor_config_t;

static editor_config_t editor_cfg;

enum editor_key {
    ARROW_UP = 1000,
    ARROW_LEFT,
    ARROW_DOWN,
    ARROW_RIGHT,
    PAGE_UP,
    PAGE_DOWN
};

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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor_cfg.orig_termios) == -1) {
        die("disable_raw_mode :: tcsetattr");
    }
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &editor_cfg.orig_termios) == -1) {
        die("enable_raw_mode :: tcgetattr");
    }

    atexit(disable_raw_mode);

    struct termios raw = editor_cfg.orig_termios;
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

unsigned editor_read_key() {
    int nread = 0;
    char c = '\0';

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("editor_read_key :: read");
        }
    }

    if (c == '\x1b') {
        char seq[3] = {0};

        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }

        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }

                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                }
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}

void editor_move_cursor(unsigned key) {
    switch (key) {
        case ARROW_LEFT:
            if (editor_cfg.cx != 0) {
                editor_cfg.cx -= 1;
            }
            break;
        case ARROW_RIGHT:
            if (editor_cfg.cx != editor_cfg.screen_cols - 1) {
                editor_cfg.cx += 1;
            }
            break;
        case ARROW_UP:
            if (editor_cfg.cy != 0) {
                editor_cfg.cy -= 1;
            }
            break;
        case ARROW_DOWN:
            if (editor_cfg.cy != editor_cfg.screen_rows - 1) {
                editor_cfg.cy += 1;
            }
            break;
    }
}

void editor_process_keypress() {
    unsigned c = editor_read_key();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case PAGE_UP:
        case PAGE_DOWN: {
            unsigned times = editor_cfg.screen_rows;
            while (times--) {
                editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }

            break;
        }
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            editor_move_cursor(c);
            break;
    }
}

void editor_draw_rows(abuf *ab) {
    for (unsigned y = 0; y < editor_cfg.screen_rows; y++) {
        if (y == editor_cfg.screen_rows / 3) {
            char welcome[80] = {0};
            unsigned welcome_len = snprintf(welcome, sizeof(welcome),
                                            "Kilo Editor -- version %s", KILO_VERSION);

            if (welcome_len > editor_cfg.screen_cols) {
                welcome_len = editor_cfg.screen_cols;
            }

            unsigned padding = (editor_cfg.screen_cols - welcome_len) / 2;

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
        if (y < editor_cfg.screen_rows - 1) {
            abuf_append(ab, "\r\n", 2);
        }
    }
}

void editor_refresh_screen() {
    abuf ab = ABUF_INIT;

    abuf_append(&ab, "\x1b[?25l", 6);
    abuf_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);

    char buf[32] = {0};
    unsigned len =
        snprintf(buf, sizeof(buf), "\x1b[%u;%uH", editor_cfg.cy + 1, editor_cfg.cx + 1);
    abuf_append(&ab, buf, len);

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
    editor_cfg.cx = 0;
    editor_cfg.cy = 0;

    if (get_window_size(&editor_cfg.screen_rows, &editor_cfg.screen_cols) == -1) {
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
