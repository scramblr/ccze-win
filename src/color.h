#ifndef CCZE_COLOR_H
#define CCZE_COLOR_H

typedef enum {
    COLOR_MODE_NONE,
    COLOR_MODE_ANSI,
    COLOR_MODE_WINCON,
    COLOR_MODE_HTML
} ColorMode;

typedef enum {
    COL_RESET = 0,
    COL_BLACK, COL_RED, COL_GREEN, COL_YELLOW,
    COL_BLUE, COL_MAGENTA, COL_CYAN, COL_WHITE,
    COL_BRIGHT_BLACK, COL_BRIGHT_RED, COL_BRIGHT_GREEN, COL_BRIGHT_YELLOW,
    COL_BRIGHT_BLUE, COL_BRIGHT_MAGENTA, COL_BRIGHT_CYAN, COL_BRIGHT_WHITE,
    COL_COUNT
} Color;

/* Initialize color output. mode_override: 0=auto, 'n'=none, 'a'=ansi, 'h'=html */
void color_init(int mode_override);

ColorMode color_mode(void);

/* Write text wrapped in color */
void color_write(Color c, const char *text, int len);

/* Write plain text (no color, but HTML-escaped in HTML mode) */
void color_write_plain(const char *text, int len);

/* Parse color name string -> Color enum. Returns COL_RESET on unknown. */
Color color_parse(const char *name);

/* Return human-readable color name for a Color enum value */
const char *color_name(Color c);

/* HTML mode: write document header/footer */
void color_html_header(const char *cssfile);
void color_html_footer(void);

#endif /* CCZE_COLOR_H */
