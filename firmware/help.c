/* HELP's lookup and its lines (unit 075); the text is help_text.c's, generated from help/help.txt. */
#include "help.h"

#include <string.h>

#include "calc.h"

const help_entry *help_find(int kind, int op, int arg, const char *menu)
{
    const help_entry *any = NULL;
    for (int i = 0; i < help_table_n; i++) {
        const help_entry *e = &help_table[i];
        if (e->kind != kind) continue;
        if (kind == HELP_MENU) {                /* the text starts with the menu's name */
            const char *end = strchr(e->text, HELP_MENU_END);
            if (menu && end && strlen(menu) == (size_t)(end - e->text) && !strncmp(e->text, menu, (size_t)(end - e->text)))
                return e;
            continue;
        }
        if (e->op != op) continue;
        if (kind == HELP_OP && e->arg >= 0) {
            if (e->arg == arg) return e;       /* the op with this argument, first */
            continue;
        }
        any = e;
    }
    return any;
}

const help_entry *help_for_action(const km_action *act)
{
    switch (act->kind) {
    case KM_OP: case KM_PROMPT: case KM_CONFIRM: return help_find(HELP_OP, act->op, act->arg, NULL);
    case KM_MENU: return help_find(HELP_MENU, 0, -1, act->legend);
    case KM_DEVICE: return help_find(HELP_DEV, act->arg, -1, NULL);
    case KM_SHIFT: return help_find(HELP_SHIFT, act->arg, -1, NULL);
    case KM_SOFT: return help_find(HELP_SOFT, 0, -1, NULL);
    case KM_UNDEFINED: return help_find(HELP_UNDEF, 0, -1, NULL);
    default: return help_find(HELP_NONE, 0, -1, NULL);
    }
}

/* whether a line's prefix names model; *body past the prefix; false when it has none */
static bool prefixed(const char *s, int n, int model, bool *mine, int *skip)
{
    int i = 0;
    *mine = false;
    for (;;) {
        int len = !strncmp(s + i, "STU", 3) ? 3 : (!strncmp(s + i, "33", 2) || !strncmp(s + i, "35", 2)) ? 2 : 0;
        if (!len || i + len > n) return false;
        int m = len == 3 ? AB_MODEL_STU : (s[i + 1] == '3' ? 33 : 35);
        if (m == model) *mine = true;
        i += len;
        if (i < n && s[i] == ',') {
            i++;
            continue;
        }
        if (i < n && s[i] == ':') {
            i++;
            if (i < n && s[i] == ' ') i++;
            *skip = i;
            return true;
        }
        return false;
    }
}

int help_lines(const help_entry *e, int model, const char *legend, char out[][HELP_LINE_BYTES], int max)
{
    if (!e) return 0;
    bool any_mine = false;
    const char *text = e->text;
    if (e->kind == HELP_MENU && strchr(text, HELP_MENU_END)) text = strchr(text, HELP_MENU_END) + 1;
    for (int pass = 0; pass < 2; pass++) {      /* first: whether any line names this mode; then the lines */
        int k = 0;
        for (const char *s = text; *s;) {
            const char *nl = strchr(s, '\n');
            int n = nl ? (int)(nl - s) : (int)strlen(s), skip = 0;
            bool mine = false, has = prefixed(s, n, model, &mine, &skip);
            if (pass == 0) {
                if (has && mine) any_mine = true;
            } else if (any_mine ? (has && mine) : !has) {
                if (k < max) {
                    char *o = out[k];
                    int w = 0;
                    for (int i = skip; i < n && w < HELP_LINE_BYTES - 1; i++) {
                        if (s[i] == '{' && i + 1 < n && s[i + 1] == '}') {
                            for (const char *l = legend ? legend : ""; *l && w < HELP_LINE_BYTES - 1; l++) o[w++] = *l;
                            i++;
                            continue;
                        }
                        o[w++] = s[i];
                    }
                    if (w == HELP_LINE_BYTES - 1)       /* cut: never inside a character (help_test fails any cut) */
                        while (w > 0 && ((unsigned char)o[w - 1] & 0xC0) == 0x80) w--;
                    if (w > 0 && (unsigned char)o[w - 1] >= 0xC0) w--;
                    o[w] = '\0';
                    k++;
                }
            }
            s = nl ? nl + 1 : s + n;
        }
        if (pass == 1) return k < max ? k : max;
    }
    return 0;
}
