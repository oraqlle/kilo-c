#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

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
    unsigned size;
    char *chars;
} editor_row_t;

typedef struct {
    unsigned cx;
    unsigned cy;
    unsigned row_offset;
    unsigned col_offset;
    unsigned screen_rows;
    unsigned screen_cols;
    unsigned num_erows;
    editor_row_t *erows;
    struct termios orig_termios;
} editor_config_t;

static editor_config_t editor_cfg;

enum editor_key {
    ARROW_UP = 1000,
    ARROW_LEFT,
    ARROW_DOWN,
    ARROW_RIGHT,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
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
                        case '1':
                            return HOME_KEY;
                        case '3':
                            return DEL_KEY;
                        case '4':
                            return END_KEY;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
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
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}

void editor_move_cursor(unsigned key) {
    editor_row_t *erow =
        (editor_cfg.cy >= editor_cfg.num_erows) ? NULL : &editor_cfg.erows[editor_cfg.cy];

    switch (key) {
        case ARROW_LEFT:
            if (editor_cfg.cx != 0) {
                editor_cfg.cx -= 1;
            }
            break;
        case ARROW_RIGHT:
            if (erow != NULL && editor_cfg.cx < erow->size) {
                editor_cfg.cx += 1;
            }
            break;
        case ARROW_UP:
            if (editor_cfg.cy != 0) {
                editor_cfg.cy -= 1;
            }
            break;
        case ARROW_DOWN:
            if (editor_cfg.cy < editor_cfg.num_erows) {
                editor_cfg.cy += 1;
            }
            break;
    }

    erow =
        (editor_cfg.cy >= editor_cfg.num_erows) ? NULL : &editor_cfg.erows[editor_cfg.cy];
    unsigned row_len = erow != NULL ? erow->size : 0;

    if (editor_cfg.cx > row_len) {
        editor_cfg.cx = row_len;
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

        case HOME_KEY:
            editor_cfg.cx = 0;
            break;

        case END_KEY:
            editor_cfg.cx = editor_cfg.screen_cols - 1;
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
        unsigned file_row = y + editor_cfg.row_offset;
        if (file_row >= editor_cfg.num_erows) {
            if (editor_cfg.num_erows == 0 && y == editor_cfg.screen_rows / 3) {
                char welcome[80] = {0};
                unsigned welcome_len = snprintf(
                    welcome, sizeof(welcome), "Kilo Editor -- version %s", KILO_VERSION);

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
        } else {
            long len = editor_cfg.erows[file_row].size - editor_cfg.col_offset;

            if (len < 0) {
                len = 0;
            }

            if (len > editor_cfg.screen_cols) {
                len = editor_cfg.screen_cols;
            }

            abuf_append(ab, &editor_cfg.erows[file_row].chars[editor_cfg.col_offset],
                        len);
        }

        abuf_append(ab, "\x1b[K", 3);
        if (y < editor_cfg.screen_rows - 1) {
            abuf_append(ab, "\r\n", 2);
        }
    }
}

void editor_scroll() {
    if (editor_cfg.cy < editor_cfg.row_offset) {
        editor_cfg.row_offset = editor_cfg.cy;
    }

    if (editor_cfg.cy >= editor_cfg.row_offset + editor_cfg.screen_rows) {
        editor_cfg.row_offset = editor_cfg.cy - editor_cfg.screen_rows + 1;
    }

    if (editor_cfg.cx < editor_cfg.col_offset) {
        editor_cfg.col_offset = editor_cfg.cx;
    }

    if (editor_cfg.cx >= editor_cfg.col_offset + editor_cfg.screen_cols) {
        editor_cfg.col_offset = editor_cfg.cx - editor_cfg.screen_cols + 1;
    }
}

void editor_refresh_screen() {
    editor_scroll();

    abuf ab = ABUF_INIT;

    abuf_append(&ab, "\x1b[?25l", 6);
    abuf_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);

    char buf[32] = {0};
    unsigned len = snprintf(buf, sizeof(buf), "\x1b[%u;%uH",
                            (editor_cfg.cy - editor_cfg.row_offset + 1),
                            (editor_cfg.cx - editor_cfg.col_offset + 1));
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

void editor_append_row(char *str, size_t len) {
    editor_cfg.erows = (editor_row_t *)realloc(
        editor_cfg.erows, sizeof(editor_row_t) * (editor_cfg.num_erows + 1));

    unsigned at = editor_cfg.num_erows;
    editor_cfg.erows[at].size = len;
    editor_cfg.erows[at].chars = (char *)calloc(len + 1, sizeof(char));
    memcpy(editor_cfg.erows[at].chars, str, len);
    editor_cfg.erows[at].chars[len] = '\0';
    editor_cfg.num_erows += 1;
}

void editor_open(char *filename) {
    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        die("editor_open :: fopen");
    }

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len = 0;

    if (line_len != -1) {
        while ((line_len = getline(&line, &line_cap, fp)) != -1) {

            while (line_len > 0 &&
                   (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
                line_len -= 1;
            }

            editor_append_row(line, line_len);
        }
    }

    free(line);
    fclose(fp);
}

void editor_init() {
    editor_cfg.cx = 0;
    editor_cfg.cy = 0;
    editor_cfg.row_offset = 0;
    editor_cfg.col_offset = 0;
    editor_cfg.num_erows = 0;
    editor_cfg.erows = NULL;

    if (get_window_size(&editor_cfg.screen_rows, &editor_cfg.screen_cols) == -1) {
        die("init_editor :: get_window_size");
    }
}

int main(int argc, char *argv[]) {
    enable_raw_mode();
    editor_init();

    if (argc >= 2) {
        editor_open(argv[1]);
    }

    while (1) {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return 0;
}
