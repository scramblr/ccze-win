#ifndef CCZE_RULES_H
#define CCZE_RULES_H

#include "color.h"
#include <stddef.h>

typedef enum { RULE_COLOR, RULE_TOOL } RuleType;

typedef struct Rule {
    RuleType type;
    Color    color;
    char    *tool_cmd;
    char    *pattern_src;
    void    *re;
    struct Rule *next;
} Rule;

/* Load rules from file. Returns head of linked list (or NULL on error). */
Rule *rules_load(const char *filepath);

/* Free all rules */
void rules_free(Rule *head);

/* Print all loaded rules to stderr (for -l / --list-rules) */
void rules_list(Rule *head);

#endif /* CCZE_RULES_H */
