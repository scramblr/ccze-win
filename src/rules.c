#define PCRE2_CODE_UNIT_WIDTH 8
#include "rules.h"
#include <pcre2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    {
        char *end = s + strlen(s);
        while (end > s && isspace((unsigned char)*(end - 1))) end--;
        *end = '\0';
        return s;
    }
}

static char *next_token(char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
    if (!**p) return NULL;
    {
        char *start = *p;
        while (**p && !isspace((unsigned char)**p)) (*p)++;
        {
            int len = (int)(*p - start);
            char *tok = (char *)malloc(len + 1);
            memcpy(tok, start, len);
            tok[len] = '\0';
            return tok;
        }
    }
}

Rule *rules_load(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    Rule *head = NULL, *tail = NULL;
    char line[1024];
    int lineno = 0;

    if (!f) {
        fprintf(stderr, "ccze: warning: could not open rule file: %s\n", filepath);
        return NULL;
    }

    while (fgets(line, sizeof(line), f)) {
        char *s, *p, *type_tok, *color_or_cmd, *pattern, *tool_cmd;
        char *tokens[64];
        int ntok = 0;
        RuleType rtype;
        Color col = COL_RESET;

        lineno++;
        s = trim(line);
        if (!*s || *s == '#') continue;

        p = s;
        type_tok = next_token(&p);
        if (!type_tok) continue;

        if (strcmp(type_tok, "color") == 0) rtype = RULE_COLOR;
        else if (strcmp(type_tok, "tool") == 0) rtype = RULE_TOOL;
        else {
            fprintf(stderr, "ccze: warning: line %d: unknown rule type '%s'\n",
                    lineno, type_tok);
            free(type_tok);
            continue;
        }
        free(type_tok);

        color_or_cmd = next_token(&p);
        if (!color_or_cmd) {
            fprintf(stderr, "ccze: warning: line %d: missing color/command\n", lineno);
            continue;
        }

        {
            char *tok;
            while (ntok < 63 && (tok = next_token(&p)) != NULL)
                tokens[ntok++] = tok;
        }

        if (ntok == 0) {
            fprintf(stderr, "ccze: warning: line %d: missing regex pattern\n", lineno);
            free(color_or_cmd);
            continue;
        }

        pattern = tokens[ntok - 1];
        tool_cmd = NULL;

        if (rtype == RULE_COLOR) {
            int i;
            col = color_parse(color_or_cmd);
            for (i = 0; i < ntok - 1; i++) free(tokens[i]);
        } else {
            size_t cmd_len = strlen(color_or_cmd);
            int i;
            for (i = 0; i < ntok - 1; i++) cmd_len += 1 + strlen(tokens[i]);
            tool_cmd = (char *)malloc(cmd_len + 1);
            strcpy(tool_cmd, color_or_cmd);
            for (i = 0; i < ntok - 1; i++) {
                strcat(tool_cmd, " ");
                strcat(tool_cmd, tokens[i]);
                free(tokens[i]);
            }
        }
        free(color_or_cmd);

        {
            int err_code;
            PCRE2_SIZE err_offset;
            pcre2_code *re = pcre2_compile(
                (PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
                PCRE2_DOTALL | PCRE2_MULTILINE,
                &err_code, &err_offset, NULL);

            if (!re) {
                PCRE2_UCHAR errbuf[256];
                pcre2_get_error_message(err_code, errbuf, sizeof(errbuf));
                fprintf(stderr, "ccze: warning: line %d: regex error at offset %d: %s\n",
                        lineno, (int)err_offset, (char *)errbuf);
                free(pattern);
                free(tool_cmd);
                continue;
            }

            {
                Rule *r = (Rule *)calloc(1, sizeof(Rule));
                r->type = rtype;
                r->color = col;
                r->tool_cmd = tool_cmd;
                r->pattern_src = pattern;
                r->re = re;
                r->next = NULL;
                if (!head) head = tail = r;
                else { tail->next = r; tail = r; }
            }
        }
    }

    fclose(f);
    return head;
}

void rules_free(Rule *head) {
    while (head) {
        Rule *next = head->next;
        pcre2_code_free((pcre2_code *)head->re);
        free(head->pattern_src);
        free(head->tool_cmd);
        free(head);
        head = next;
    }
}

void rules_list(Rule *head) {
    Rule *r;
    int i = 1;
    fprintf(stderr, "Loaded rules:\n");
    for (r = head; r; r = r->next, i++) {
        if (r->type == RULE_COLOR)
            fprintf(stderr, "  %3d  color  %-16s  %s\n", i, color_name(r->color), r->pattern_src);
        else
            fprintf(stderr, "  %3d  tool   %-16s  %s\n", i, r->tool_cmd, r->pattern_src);
    }
    if (i == 1) fprintf(stderr, "  (none)\n");
}
