#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include "color.h"
#include "rules.h"
#include "tool.h"

#define CCZE_VERSION "1.0.0"

/* ----------------------------------------------------------------
 * Options
 * ---------------------------------------------------------------- */
typedef struct {
    int          mode_override;   /* 0=auto, 'n'=none, 'a'=ansi, 'h'=html */
    int          remove_facility; /* -r: strip syslog facility prefix */
    int          wordcolor;       /* -o wordcolor (default on) */
    int          transparent;     /* -o transparent (default on) */
    int          list_rules;      /* -l: list loaded rules and exit */
    const char  *rcfile;          /* -F: override config file path */
    const char  *cssfile;         /* -o cssfile=FILE */
    const char  *input_file;      /* positional arg */
} Options;

/* ----------------------------------------------------------------
 * Dynamic line buffer
 * ---------------------------------------------------------------- */
typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
} LineBuf;

static void linebuf_init(LineBuf *lb) {
    lb->cap = 4096;
    lb->buf = (char *)malloc(lb->cap);
    lb->len = 0;
}
static void linebuf_free(LineBuf *lb) { free(lb->buf); }

static int linebuf_readline(LineBuf *lb, FILE *fp) {
    int c;
    lb->len = 0;
    while ((c = fgetc(fp)) != EOF) {
        if (lb->len + 1 >= lb->cap) {
            lb->cap *= 2;
            lb->buf = (char *)realloc(lb->buf, lb->cap);
        }
        lb->buf[lb->len++] = (char)c;
        lb->buf[lb->len] = '\0';
        if (c == '\n') break;
    }
    return lb->len > 0;
}

/* ----------------------------------------------------------------
 * Syslog facility stripping (-r)
 * ---------------------------------------------------------------- */
static const char *strip_facility(const char *line, int *len) {
    const char *p = line;
    if (*p == '<') {
        p++;
        while (*p && *p != '>') p++;
        if (*p == '>') p++;
    } else {
        const char *colon = NULL;
        const char *s = line;
        while (*s && *s != ' ' && *s != '\n') {
            if (*s == ':') { colon = s; break; }
            s++;
        }
        if (colon && colon[1] == ' ') p = colon + 2;
    }
    *len -= (int)(p - line);
    return p;
}

/* ----------------------------------------------------------------
 * Word color (ccze-compatible)
 *
 * ccze uses prefix matching (strstr(word, prefix) == word).
 * We do the same: if a word *starts with* a bad/good/error/system
 * prefix, it gets that color. This handles "Failed", "Starting", etc.
 * ---------------------------------------------------------------- */

static int starts_with_lower(const char *word, int wlen, const char *prefix) {
    int plen = (int)strlen(prefix);
    int i;
    if (wlen < plen) return 0;
    for (i = 0; i < plen; i++) {
        if (tolower((unsigned char)word[i]) != prefix[i]) return 0;
    }
    return 1;
}

static Color wordcolor_lookup(const char *word, int wlen) {
    /* ccze error words - bold red */
    static const char *words_error[] = {
        "error", "crit", "invalid", "fail", "false", "alarm", "fatal", NULL
    };
    /* ccze bad words - bold yellow */
    static const char *words_bad[] = {
        "warn", "restart", "exit", "stop", "end", "shutting", "down", "close",
        "unreach", "can't", "cannot", "skip", "deny", "disable", "ignored",
        "miss", "oops", "not", "backdoor", "blocking", "ignoring",
        "unable", "readonly", "offline", "terminate", "empty", "virus", NULL
    };
    /* ccze good words - bold green */
    static const char *words_good[] = {
        "activ", "start", "ready", "online", "load", "ok", "register", "detected",
        "configured", "enable", "listen", "open", "complete", "attempt", "done",
        "check", "connect", "finish", "clean", "succeed", NULL
    };
    /* ccze system words - bold cyan */
    static const char *words_system[] = {
        "ext2-fs", "reiserfs", "vfs", "iso", "isofs", "cslip", "ppp", "bsd",
        "linux", "tcp/ip", "mtrr", "pci", "isa", "scsi", "ide", "atapi",
        "bios", "cpu", "fpu", "discharging", "resume", NULL
    };
    int i;

    for (i = 0; words_error[i]; i++)
        if (starts_with_lower(word, wlen, words_error[i])) return COL_BRIGHT_RED;
    for (i = 0; words_bad[i]; i++)
        if (starts_with_lower(word, wlen, words_bad[i])) return COL_BRIGHT_YELLOW;
    for (i = 0; words_good[i]; i++)
        if (starts_with_lower(word, wlen, words_good[i])) return COL_BRIGHT_GREEN;
    for (i = 0; words_system[i]; i++)
        if (starts_with_lower(word, wlen, words_system[i])) return COL_BRIGHT_CYAN;
    return COL_RESET;
}

/* Check if a span looks like a path (starts with /) */
static int is_path(const char *word, int wlen) {
    return wlen > 1 && word[0] == '/';
}

/* Check if a span looks like a URI */
static int is_uri(const char *word, int wlen) {
    return wlen > 5 && (
        (wlen > 7 && memcmp(word, "http://", 7) == 0) ||
        (wlen > 8 && memcmp(word, "https://", 8) == 0) ||
        (wlen > 6 && memcmp(word, "ftp://", 6) == 0));
}

/* Check if a span is all digits (a number) */
static int is_number(const char *word, int wlen) {
    int i;
    if (wlen == 0) return 0;
    for (i = 0; i < wlen; i++)
        if (!isdigit((unsigned char)word[i])) return 0;
    return 1;
}

/* Emit a plain-text span, applying wordcolor if enabled */
static void emit_plain(const char *text, int len, int wordcolor_on) {
    int i, ws, we;
    Color wc;

    if (!wordcolor_on) {
        color_write_plain(text, len);
        return;
    }
    i = 0;
    while (i < len) {
        /* Non-word characters: pass through */
        if (!isalnum((unsigned char)text[i]) && text[i] != '/' && text[i] != ':' && text[i] != '-' && text[i] != '_' && text[i] != '.') {
            ws = i;
            while (i < len && !isalnum((unsigned char)text[i]) && text[i] != '/' && text[i] != ':' && text[i] != '-' && text[i] != '_' && text[i] != '.')
                i++;
            color_write_plain(text + ws, i - ws);
            continue;
        }
        /* Extract a "token" (word or path or uri) */
        ws = i;
        while (i < len && (isalnum((unsigned char)text[i]) || text[i] == '/' || text[i] == ':' || text[i] == '-' || text[i] == '_' || text[i] == '.'))
            i++;
        we = i;

        /* Classify token */
        if (is_uri(text + ws, we - ws))
            color_write(COL_BRIGHT_GREEN, text + ws, we - ws);
        else if (is_path(text + ws, we - ws))
            color_write(COL_GREEN, text + ws, we - ws);
        else if (is_number(text + ws, we - ws))
            color_write(COL_BRIGHT_WHITE, text + ws, we - ws);
        else {
            wc = wordcolor_lookup(text + ws, we - ws);
            if (wc != COL_RESET)
                color_write(wc, text + ws, we - ws);
            else
                color_write_plain(text + ws, we - ws);
        }
    }
}

#define NO_RULE -1

/* ----------------------------------------------------------------
 * Syslog structural parser
 *
 * Matches: "Feb 22 00:00:18 hostname process[pid]: message"
 * Colors each field like ccze's mod_syslog.c:
 *   date     = bright cyan
 *   hostname = bright blue
 *   process  = green
 *   [        = bright green
 *   pid      = bright white
 *   ]        = bright green
 *   :        = green
 *   message  = wordcolor + rules
 *
 * Returns 1 if line was handled as syslog, 0 otherwise.
 * ---------------------------------------------------------------- */
static pcre2_code *g_syslog_re = NULL;

static void syslog_init(void) {
    int err;
    PCRE2_SIZE erroff;
    /* ccze regex: ^(\S*\s{1,2}\d{1,2}\s\d\d:\d\d:\d\d)\s(\S+)\s+((\S+:?)\s(.*))$ */
    g_syslog_re = pcre2_compile(
        (PCRE2_SPTR)"^(\\S+\\s{1,2}\\d{1,2}\\s\\d\\d:\\d\\d:\\d\\d)\\s(\\S+)\\s+((\\S+?)(?:\\[(\\d+)\\])?:\\s(.*))$",
        PCRE2_ZERO_TERMINATED, PCRE2_DOTALL, &err, &erroff, NULL);
}

static int process_syslog(const char *line, int line_len, Rule *rules, const Options *opts) {
    pcre2_match_data *md;
    PCRE2_SIZE *ov;
    int rc;

    if (!g_syslog_re) return 0;

    md = pcre2_match_data_create(16, NULL);
    rc = pcre2_match(g_syslog_re, (PCRE2_SPTR)line, line_len, 0, 0, md, NULL);
    if (rc < 0) {
        pcre2_match_data_free(md);
        return 0;
    }

    ov = pcre2_get_ovector_pointer(md);

    /* Group 1: date */
    color_write(COL_BRIGHT_CYAN, line + ov[2], (int)(ov[3] - ov[2]));
    color_write_plain(" ", 1);

    /* Group 2: hostname */
    color_write(COL_BRIGHT_BLUE, line + ov[4], (int)(ov[5] - ov[4]));
    color_write_plain(" ", 1);

    /* Group 4: process name */
    color_write(COL_GREEN, line + ov[8], (int)(ov[9] - ov[8]));

    /* Group 5: pid (optional) */
    if (ov[10] != PCRE2_UNSET) {
        color_write(COL_BRIGHT_GREEN, "[", 1);
        color_write(COL_BRIGHT_WHITE, line + ov[10], (int)(ov[11] - ov[10]));
        color_write(COL_BRIGHT_GREEN, "]", 1);
    }

    color_write(COL_GREEN, ":", 1);
    color_write_plain(" ", 1);

    /* Group 6: message — apply rules + wordcolor */
    {
        const char *msg = line + ov[12];
        int msg_len = (int)(ov[13] - ov[12]);

        if (rules) {
            /* Apply regex rules to the message portion */
            int *color_map = (int *)malloc(msg_len * sizeof(int));
            Rule **rule_arr = NULL;
            int nrules = 0, ri, r, i, j;
            Rule *rp;
            pcre2_match_data *rmd;

            for (i = 0; i < msg_len; i++) color_map[i] = NO_RULE;
            for (rp = rules; rp; rp = rp->next) nrules++;
            rule_arr = (Rule **)malloc(nrules * sizeof(Rule *));
            ri = 0;
            for (rp = rules; rp; rp = rp->next) rule_arr[ri++] = rp;

            rmd = pcre2_match_data_create(16, NULL);
            for (r = 0; r < nrules; r++) {
                pcre2_code *re = (pcre2_code *)rule_arr[r]->re;
                PCRE2_SIZE offset = 0;
                while (offset < (PCRE2_SIZE)msg_len) {
                    int mrc = pcre2_match(re, (PCRE2_SPTR)msg, msg_len, offset, 0, rmd, NULL);
                    if (mrc < 0) break;
                    {
                        PCRE2_SIZE *mov = pcre2_get_ovector_pointer(rmd);
                        int mstart = (int)mov[0], mend = (int)mov[1], claimed = 0;
                        if (mend <= mstart) { offset = mend + 1; continue; }
                        for (i = mstart; i < mend; i++)
                            if (color_map[i] != NO_RULE) { claimed = 1; break; }
                        if (!claimed)
                            for (i = mstart; i < mend; i++) color_map[i] = r;
                        offset = (PCRE2_SIZE)mend;
                    }
                }
            }
            pcre2_match_data_free(rmd);

            /* Emit message with rules + wordcolor */
            i = 0;
            while (i < msg_len) {
                if (color_map[i] == NO_RULE) {
                    j = i;
                    while (j < msg_len && color_map[j] == NO_RULE) j++;
                    emit_plain(msg + i, j - i, opts->wordcolor);
                    i = j;
                } else {
                    r = color_map[i];
                    j = i;
                    while (j < msg_len && color_map[j] == r) j++;
                    if (rule_arr[r]->type == RULE_COLOR)
                        color_write(rule_arr[r]->color, msg + i, j - i);
                    else {
                        int olen = 0;
                        char *out = tool_run(rule_arr[r]->tool_cmd, msg + i, j - i, &olen);
                        if (out) {
                            while (olen > 0 && (out[olen-1]=='\n'||out[olen-1]=='\r')) olen--;
                            color_write(COL_CYAN, out, olen);
                            free(out);
                        } else {
                            color_write_plain(msg + i, j - i);
                        }
                    }
                    i = j;
                }
            }
            free(color_map);
            free(rule_arr);
        } else {
            emit_plain(msg, msg_len, opts->wordcolor);
        }
    }

    /* Trailing newline if present */
    {
        int end = (int)(ov[13]);
        if (end < line_len)
            color_write_plain(line + end, line_len - end);
    }

    pcre2_match_data_free(md);
    return 1;
}

/* ----------------------------------------------------------------
 * Process one line (generic, non-syslog)
 * ---------------------------------------------------------------- */
static void process_line(const char *line, int line_len, Rule *rules, const Options *opts) {
    int *color_map;
    Rule **rule_arr = NULL;
    int nrules = 0;
    int ri, r, i, j;
    pcre2_match_data *md;
    Rule *rp;

    /* Strip syslog facility if requested */
    if (opts->remove_facility)
        line = strip_facility(line, &line_len);

    /* Try syslog structural parse first */
    if (process_syslog(line, line_len, rules, opts))
        return;

    /* Fallback: generic rule-based processing */
    if (!rules) {
        emit_plain(line, line_len, opts->wordcolor);
        return;
    }

    color_map = (int *)malloc(line_len * sizeof(int));
    for (i = 0; i < line_len; i++) color_map[i] = NO_RULE;

    for (rp = rules; rp; rp = rp->next) nrules++;
    rule_arr = (Rule **)malloc(nrules * sizeof(Rule *));
    ri = 0;
    for (rp = rules; rp; rp = rp->next) rule_arr[ri++] = rp;

    md = pcre2_match_data_create(16, NULL);
    for (r = 0; r < nrules; r++) {
        pcre2_code *re = (pcre2_code *)rule_arr[r]->re;
        PCRE2_SIZE offset = 0;
        while (offset < (PCRE2_SIZE)line_len) {
            int rc = pcre2_match(re, (PCRE2_SPTR)line, line_len, offset, 0, md, NULL);
            if (rc < 0) break;
            {
                PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(md);
                int mstart = (int)ovector[0];
                int mend = (int)ovector[1];
                int claimed;
                if (mend <= mstart) { offset = mend + 1; continue; }
                claimed = 0;
                for (i = mstart; i < mend; i++)
                    if (color_map[i] != NO_RULE) { claimed = 1; break; }
                if (!claimed)
                    for (i = mstart; i < mend; i++) color_map[i] = r;
                offset = (PCRE2_SIZE)mend;
            }
        }
    }
    pcre2_match_data_free(md);

    i = 0;
    while (i < line_len) {
        if (color_map[i] == NO_RULE) {
            j = i;
            while (j < line_len && color_map[j] == NO_RULE) j++;
            emit_plain(line + i, j - i, opts->wordcolor);
            i = j;
        } else {
            r = color_map[i];
            j = i;
            while (j < line_len && color_map[j] == r) j++;
            if (rule_arr[r]->type == RULE_COLOR) {
                color_write(rule_arr[r]->color, line + i, j - i);
            } else {
                int olen = 0;
                char *out = tool_run(rule_arr[r]->tool_cmd, line + i, j - i, &olen);
                if (out) {
                    while (olen > 0 && (out[olen - 1] == '\n' || out[olen - 1] == '\r')) olen--;
                    color_write(COL_CYAN, out, olen);
                    free(out);
                } else {
                    color_write_plain(line + i, j - i);
                }
            }
            i = j;
        }
    }

    free(color_map);
    free(rule_arr);
}

/* ----------------------------------------------------------------
 * Config file location
 * ---------------------------------------------------------------- */
static char *find_conf(void) {
    char exe_path[MAX_PATH];
    char *last_slash;
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    last_slash = strrchr(exe_path, '\\');
    if (!last_slash) last_slash = strrchr(exe_path, '/');
    if (last_slash) *(last_slash + 1) = '\0';
    strcat(exe_path, "ccze.conf");
    return _strdup(exe_path);
}

/* ----------------------------------------------------------------
 * CLI color overrides
 * ---------------------------------------------------------------- */
#define MAX_COLOR_OVERRIDES 64
static struct { char key[64]; char val[64]; } g_color_overrides[MAX_COLOR_OVERRIDES];
static int g_num_overrides = 0;

static void apply_color_overrides(Rule *rules) {
    int i;
    Rule *r;
    for (i = 0; i < g_num_overrides; i++) {
        Color newcol = color_parse(g_color_overrides[i].val);
        for (r = rules; r; r = r->next) {
            if (r->type == RULE_COLOR && _stricmp(color_name(r->color), g_color_overrides[i].key) == 0)
                r->color = newcol;
        }
    }
}

/* ----------------------------------------------------------------
 * Help & version
 * ---------------------------------------------------------------- */
static void print_version(void) {
    fprintf(stderr, "ccze %s - Windows log colorizer\n", CCZE_VERSION);
}

static void print_help(void) {
    print_version();
    fprintf(stderr,
        "\n"
        "Usage: ccze [OPTIONS] [FILE]\n"
        "\n"
        "Reads log data from FILE or stdin and outputs colorized text.\n"
        "\n"
        "Options:\n"
        "  -A, --raw-ansi        Force ANSI escape code output\n"
        "  -h, --html            Output colorized HTML\n"
        "  -m, --mode MODE       Output mode: ansi, html, none (default: auto)\n"
        "  -F, --rcfile FILE     Use FILE as config instead of ccze.conf\n"
        "  -c, --color KEY=COL   Override color KEY with COL\n"
        "  -r, --remove-facility Strip syslog facility/level prefix\n"
        "  -l, --list-rules      List loaded rules and exit\n"
        "  -o, --options OPT     Toggle options:\n"
        "                          wordcolor / nowordcolor\n"
        "                          transparent / notransparent\n"
        "                          cssfile=FILE (HTML mode)\n"
        "      --no-color        Disable all color output\n"
        "  -V, --version         Print version and exit\n"
        "      --help            Show this help\n"
        "\n"
        "Config file: ccze.conf (next to ccze.exe, or specify with -F)\n"
        "Rule format:\n"
        "  color  COLOR_NAME  PCRE2_REGEX\n"
        "  tool   COMMAND     PCRE2_REGEX\n"
    );
}

/* ----------------------------------------------------------------
 * Parse -o option values
 * ---------------------------------------------------------------- */
static void parse_option_flag(const char *opt, Options *opts) {
    if (strcmp(opt, "wordcolor") == 0)        opts->wordcolor = 1;
    else if (strcmp(opt, "nowordcolor") == 0)  opts->wordcolor = 0;
    else if (strcmp(opt, "transparent") == 0)  opts->transparent = 1;
    else if (strcmp(opt, "notransparent") == 0) opts->transparent = 0;
    else if (strncmp(opt, "cssfile=", 8) == 0) opts->cssfile = opt + 8;
    else fprintf(stderr, "ccze: warning: unknown option '%s'\n", opt);
}

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    Options opts;
    char *conf_path = NULL;
    Rule *rules;
    FILE *fp;
    LineBuf lb;
    int i;

    memset(&opts, 0, sizeof(opts));
    opts.wordcolor = 1;
    opts.transparent = 1;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help(); return 0;
        }
        else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version(); return 0;
        }
        else if (strcmp(argv[i], "--no-color") == 0) {
            opts.mode_override = 'n';
        }
        else if (strcmp(argv[i], "-A") == 0 || strcmp(argv[i], "--raw-ansi") == 0) {
            opts.mode_override = 'a';
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--html") == 0) {
            opts.mode_override = 'h';
        }
        else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) {
            if (++i >= argc) { fprintf(stderr, "ccze: -m requires an argument\n"); return 1; }
            if (strcmp(argv[i], "ansi") == 0)      opts.mode_override = 'a';
            else if (strcmp(argv[i], "html") == 0)  opts.mode_override = 'h';
            else if (strcmp(argv[i], "none") == 0)  opts.mode_override = 'n';
            else { fprintf(stderr, "ccze: unknown mode '%s'\n", argv[i]); return 1; }
        }
        else if (strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "--rcfile") == 0) {
            if (++i >= argc) { fprintf(stderr, "ccze: -F requires an argument\n"); return 1; }
            opts.rcfile = argv[i];
        }
        else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--color") == 0) {
            char *eq;
            if (++i >= argc) { fprintf(stderr, "ccze: -c requires KEY=COLOR\n"); return 1; }
            eq = strchr(argv[i], '=');
            if (!eq) { fprintf(stderr, "ccze: -c format is KEY=COLOR\n"); return 1; }
            if (g_num_overrides < MAX_COLOR_OVERRIDES) {
                int klen = (int)(eq - argv[i]);
                if (klen > 63) klen = 63;
                memcpy(g_color_overrides[g_num_overrides].key, argv[i], klen);
                g_color_overrides[g_num_overrides].key[klen] = '\0';
                strncpy(g_color_overrides[g_num_overrides].val, eq + 1, 63);
                g_color_overrides[g_num_overrides].val[63] = '\0';
                g_num_overrides++;
            }
        }
        else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--remove-facility") == 0) {
            opts.remove_facility = 1;
        }
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list-rules") == 0) {
            opts.list_rules = 1;
        }
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--options") == 0) {
            if (++i >= argc) { fprintf(stderr, "ccze: -o requires an argument\n"); return 1; }
            parse_option_flag(argv[i], &opts);
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "ccze: unknown option '%s' (try --help)\n", argv[i]);
            return 1;
        }
        else {
            opts.input_file = argv[i];
        }
    }

    color_init(opts.mode_override);
    syslog_init();

    if (opts.rcfile)
        conf_path = _strdup(opts.rcfile);
    else
        conf_path = find_conf();

    rules = rules_load(conf_path);
    free(conf_path);

    if (g_num_overrides > 0 && rules)
        apply_color_overrides(rules);

    if (opts.list_rules) {
        rules_list(rules);
        rules_free(rules);
        return 0;
    }

    fp = stdin;
    if (opts.input_file) {
        fp = fopen(opts.input_file, "r");
        if (!fp) {
            fprintf(stderr, "ccze: error: cannot open file: %s\n", opts.input_file);
            rules_free(rules);
            return 1;
        }
    }

    if (color_mode() == COLOR_MODE_HTML)
        color_html_header(opts.cssfile);

    linebuf_init(&lb);
    while (linebuf_readline(&lb, fp)) {
        process_line(lb.buf, (int)lb.len, rules, &opts);
    }
    fflush(stdout);
    linebuf_free(&lb);

    if (color_mode() == COLOR_MODE_HTML)
        color_html_footer();

    if (fp != stdin) fclose(fp);
    if (g_syslog_re) pcre2_code_free(g_syslog_re);
    rules_free(rules);
    return 0;
}
