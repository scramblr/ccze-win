#include "color.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

static ColorMode g_mode = COLOR_MODE_NONE;
static HANDLE    g_hout = INVALID_HANDLE_VALUE;

static const char *ANSI_CODES[COL_COUNT] = {
    "\033[0m",   "\033[30m", "\033[31m", "\033[32m",
    "\033[33m",  "\033[34m", "\033[35m", "\033[36m",
    "\033[37m",  "\033[90m", "\033[91m", "\033[92m",
    "\033[93m",  "\033[94m", "\033[95m", "\033[96m",
    "\033[97m",
};

static const char *HTML_COLORS[COL_COUNT] = {
    NULL,        "#000",     "#c00",     "#0a0",
    "#aa0",      "#00c",     "#c0c",     "#0aa",
    "#ccc",      "#666",     "#f55",     "#5f5",
    "#ff5",      "#55f",     "#f5f",     "#5ff",
    "#fff",
};

static const WORD WIN_ATTRS[COL_COUNT] = {
    0,
    0,
    FOREGROUND_RED,
    FOREGROUND_GREEN,
    FOREGROUND_RED | FOREGROUND_GREEN,
    FOREGROUND_BLUE,
    FOREGROUND_RED | FOREGROUND_BLUE,
    FOREGROUND_GREEN | FOREGROUND_BLUE,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_INTENSITY,
    FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
};

static const struct { const char *n; Color c; } COLOR_TABLE[] = {
    {"RESET",          COL_RESET},
    {"BLACK",          COL_BLACK},
    {"RED",            COL_RED},
    {"GREEN",          COL_GREEN},
    {"YELLOW",         COL_YELLOW},
    {"BLUE",           COL_BLUE},
    {"MAGENTA",        COL_MAGENTA},
    {"CYAN",           COL_CYAN},
    {"WHITE",          COL_WHITE},
    {"BRIGHT_BLACK",   COL_BRIGHT_BLACK},
    {"BRIGHT_RED",     COL_BRIGHT_RED},
    {"BRIGHT_GREEN",   COL_BRIGHT_GREEN},
    {"BRIGHT_YELLOW",  COL_BRIGHT_YELLOW},
    {"BRIGHT_BLUE",    COL_BRIGHT_BLUE},
    {"BRIGHT_MAGENTA", COL_BRIGHT_MAGENTA},
    {"BRIGHT_CYAN",    COL_BRIGHT_CYAN},
    {"BRIGHT_WHITE",   COL_BRIGHT_WHITE},
    {NULL, COL_RESET}
};

void color_init(int mode_override) {
    if (mode_override == 'n') { g_mode = COLOR_MODE_NONE; return; }
    if (mode_override == 'a') { g_mode = COLOR_MODE_ANSI; return; }
    if (mode_override == 'h') { g_mode = COLOR_MODE_HTML; return; }

    /* Auto-detect */
    g_hout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_hout == INVALID_HANDLE_VALUE || g_hout == NULL) {
        g_mode = COLOR_MODE_NONE;
        return;
    }
    {
        DWORD mode = 0;
        if (!GetConsoleMode(g_hout, &mode)) {
            g_mode = COLOR_MODE_NONE;
            return;
        }
        if (SetConsoleMode(g_hout, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            g_mode = COLOR_MODE_ANSI;
        else
            g_mode = COLOR_MODE_WINCON;
    }
}

ColorMode color_mode(void) { return g_mode; }

static void html_escape_write(const char *text, int len) {
    int i;
    for (i = 0; i < len; i++) {
        switch (text[i]) {
        case '<': fputs("&lt;", stdout); break;
        case '>': fputs("&gt;", stdout); break;
        case '&': fputs("&amp;", stdout); break;
        case '"': fputs("&quot;", stdout); break;
        default:  fputc(text[i], stdout); break;
        }
    }
}

void color_write(Color c, const char *text, int len) {
    if ((unsigned)c >= (unsigned)COL_COUNT) c = COL_RESET;
    switch (g_mode) {
    case COLOR_MODE_NONE:
        fwrite(text, 1, len, stdout);
        break;
    case COLOR_MODE_ANSI:
        fputs(ANSI_CODES[c], stdout);
        fwrite(text, 1, len, stdout);
        fputs(ANSI_CODES[COL_RESET], stdout);
        break;
    case COLOR_MODE_WINCON: {
        CONSOLE_SCREEN_BUFFER_INFO info;
        WORD saved = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        if (GetConsoleScreenBufferInfo(g_hout, &info))
            saved = info.wAttributes;
        if (c != COL_RESET)
            SetConsoleTextAttribute(g_hout, WIN_ATTRS[c]);
        fwrite(text, 1, len, stdout);
        fflush(stdout);
        SetConsoleTextAttribute(g_hout, saved);
        break;
    }
    case COLOR_MODE_HTML:
        if (c == COL_RESET || !HTML_COLORS[c]) {
            html_escape_write(text, len);
        } else {
            fprintf(stdout, "<span style=\"color:%s\">", HTML_COLORS[c]);
            html_escape_write(text, len);
            fputs("</span>", stdout);
        }
        break;
    }
}

void color_write_plain(const char *text, int len) {
    if (g_mode == COLOR_MODE_HTML)
        html_escape_write(text, len);
    else
        fwrite(text, 1, len, stdout);
}

Color color_parse(const char *name) {
    int i;
    for (i = 0; COLOR_TABLE[i].n; i++)
        if (_stricmp(name, COLOR_TABLE[i].n) == 0) return COLOR_TABLE[i].c;
    return COL_RESET;
}

const char *color_name(Color c) {
    int i;
    for (i = 0; COLOR_TABLE[i].n; i++)
        if (COLOR_TABLE[i].c == c) return COLOR_TABLE[i].n;
    return "RESET";
}

void color_html_header(const char *cssfile) {
    fputs("<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<title>ccze output</title>\n", stdout);
    if (cssfile)
        fprintf(stdout, "<link rel=\"stylesheet\" href=\"%s\">\n", cssfile);
    else
        fputs("<style>body{background:#1e1e1e;color:#ccc;}</style>\n", stdout);
    fputs("</head>\n<body>\n<pre>\n", stdout);
}

void color_html_footer(void) {
    fputs("</pre>\n</body>\n</html>\n", stdout);
}
