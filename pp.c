#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>
#include "rrcc.h"

typedef struct PPToken PPToken;
typedef struct ExpandHistory ExpandHistory;
typedef struct MacroRegistryEntry MacroRegistryEntry;
Vector *macro_registry;
Vector *include_pathes;
Vector *line_map;
bool pp_debug;

static void pp_next_token(PPToken **cur);
static bool pp_expect(PPToken **cur, char *str);
static bool pp_expect_newline(PPToken **cur);
static bool pp_expect_ident(PPToken **cur, char **ident, int *ident_len);
static bool pp_peek(PPToken **cur, char *str);
static bool pp_peek_newline(PPToken **cur);
static bool pp_peek_ident(PPToken **cur, char **ident, int *ident_len);
static PPToken *scan_replacement_list(MacroRegistryEntry *entry, Vector *vec);
static void process_token_concat_operator(PPToken *cur);
static PPToken *make_string(PPToken *token, PPToken *cur);
static PPToken *pp_new_str_token(PPToken *cur, char *str, int len);
PPToken *pp_parse_file();
int pp_length_in_line(PPToken *cur);

typedef enum {
    PPTK_NEWLINE,
    PPTK_HEADER_NAME,
    PPTK_RESERVED,
    PPTK_PUNC,
    PPTK_PPNUMBER,
    PPTK_IDENT,
    PPTK_CHAR_CONST,
    PPTK_STRING_LITERAL,
    PPTK_OTHER,
    PPTK_PLACE_MARKER,
    PPTK_EOF,
    PPTK_DUMMY,
} PPTokenKind;

struct PPToken {
    PPTokenKind kind;
    PPToken *prev;
    PPToken *next;
    int val;
    char *str;
    int len;
    Vector *literal;
    int literal_len;
    bool preceded_by_space;
    Vector *history;
    char *filename;
    int line_number;
};

struct ExpandHistory {
    char *ident;
    int ident_len;
};

char *pp_tokenkind_str(PPTokenKind kind) {
    switch(kind){
        case PPTK_NEWLINE: return "PPTK_NEWLINE";
        case PPTK_HEADER_NAME: return "PPTK_HEADER_NAME";
        case PPTK_RESERVED: return "PPTK_RESERVED";
        case PPTK_PUNC: return "PPTK_PUNC";
        case PPTK_PPNUMBER: return "PPTK_PPNUMBER";
        case PPTK_IDENT: return "PPTK_IDENT";
        case PPTK_CHAR_CONST: return "PPTK_CHAR_CONST";
        case PPTK_STRING_LITERAL: return "PPTK_STRING_LITERAL";
        case PPTK_OTHER: return "PPTK_OTHER";
        case PPTK_PLACE_MARKER: return "PPTK_PLACE_MARKER";
        case PPTK_EOF: return "PPTK_EOF";
        case PPTK_DUMMY: return "PPTK_DUMMY";
    }
    assert(false);
}

// Remove backslack + new-line 
void pp_phase2(char *user_input, char *processed, Vector *vec) {
    char *p = user_input;
    char *out = processed;
    int line = 1;
    while(*p) {
        if(*p == '\n') {
            line++;
        }
        if(*p == '\\') {
            p++;
            if(*p == '\n') {
                line++;
                p++;
                continue;
            }
            p--;
        }
        *out = *p;
        out++;
        p++;
        vector_push(vec, (void *)(long)line);
    }
    *out = '\0';
}

static PPToken *new_pptoken(PPTokenKind kind, PPToken *cur, char *str, int len){
    PPToken *tok = calloc(1, sizeof(PPToken));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    tok->prev = cur;
    tok->history = new_vector();
    cur->next = tok;
    if(str == user_input || (str[-1] == ' ' || str[-1] == '\t' || str[-1] == '\n')) {
        tok->preceded_by_space = true;
    }
    if(str >= user_input && str <= user_input + user_input_len) {
        tok->filename = filename;
        tok->line_number = (int)(long)vector_get(line_map, str - user_input);
    }
    return tok;
}

static PPToken *dup_pptoken(PPToken *cur, PPToken *src) {
    PPToken *tok = calloc(1, sizeof(PPToken));
    memcpy(tok, src, sizeof(PPToken));
    tok->prev = cur;
    tok->next = NULL;
    tok->history = vector_dup(src->history);
    cur->next = tok;
    return tok;
}

void pp_push_history(Vector *vec, char *ident, int ident_len) {
    ExpandHistory *history = calloc(1, sizeof(ExpandHistory));
    history->ident = ident;
    history->ident_len = ident_len;
    vector_push(vec, history);
}

void pp_list_push_history(PPToken *token, char *ident, int ident_len) {
    for(; token; token = token->next){
        pp_push_history(token->history, ident, ident_len);
    }
}

bool pp_check_in_history(PPToken *token, char *ident, int ident_len) {
    for(int i = 0; i < vector_size(token->history); i++) {
        ExpandHistory *history = vector_get(token->history, i);
        if(compare_ident(history->ident, history->ident_len, ident, ident_len)) {
            return true;
        }
    }
    return false;
}

static PPToken *pp_dup_list(PPToken *src) {
    PPToken head = {};
    PPToken *cur = &head;
    for(; src; src = src->next) {
        cur = dup_pptoken(cur, src);
    }
    return head.next;
}

static PPToken *pp_list_tail(PPToken *cur) {
    while(cur->next) {
        cur = cur->next;
    }
    return cur;
}

int pp_match_string_literal(char *p, Vector **char_vec) {
    char *b = p;
    if(*p != '"') {
        return 0;
    }
    p++;
    *char_vec = new_vector();
    while(*p) {
        if(*p == '\\'){
            p++;
            char c = read_escape(&p);
            vector_push(*char_vec, (void *)(long)c);
            p++;
            continue;
        }else if(*p == '"') {
            break;
        }
        vector_push(*char_vec, (void *)(long)*p);
        p++;
    }
    if(*p == '"') {
        p++;
    } else {
        error_at(p, "Expect \"");
    }
    return p - b;
}

PPToken *pp_tokenize() {
    char *p = user_input;
    PPToken head = {};
    PPToken *cur = &head;

    int include_state = 0;
    int prev_include_state = 0;
    while(*p) {
        if(*p == ' ' || *p == '\t') {
            p++;
            continue;
        }
        prev_include_state = include_state;
        include_state = 0;

        if(prev_include_state == 2) {
            include_state = 2;
        }

        if(*p == '\n') {
            include_state = 0;
            cur = new_pptoken(PPTK_NEWLINE, cur, p, 1);
            p++;
            continue;
        }

        if(include_state == 2){
            if(*p == '<') {
                char *b = p;
                p++;
                while(*p != '>' && *p != '\n') {
                    p++;
                }
                if(*p == '\n') {
                    error("Newline appeared in header name");
                }
                p++;
                cur = new_pptoken(PPTK_HEADER_NAME, cur, b, p - b);
                continue;
            } else if (*p == '"') {
                char *b = p;
                p++;
                while(*p != '"' && *p != '\n') {
                    p++;
                }
                if(*p == '\n') {
                    error("Newline appeared in header name");
                }
                p++;
                cur = new_pptoken(PPTK_HEADER_NAME, cur, b, p - b);
                continue;
            }
        }

        if(strncmp(p, "//", 2) == 0) {
            p += 2;
            while(*p && *p != '\n') {
                p++;
            }
            p++;
            continue;
        }

        if(strncmp(p, "/*", 2) == 0) {
            p += 2;
            while(*p) {
                if(strncmp("*/", p, 2) == 0) {
                    break;
                }
                p++;
            }
            p += 2;
            continue;
        }

        // pp-nubmber = ( digit | digit '.' ) (digit | identifier-nondigit | 'e' sign | 'E' sign | 'p' sign | 'P' sign | '.')*
        if(*p == '.' && isdigit(p[1]) || isdigit(*p)) {
            char *b = p;
            p++;
            while(1) {
                if((tolower(*p) == 'e' || tolower(*p) == 'p') && (p[1] == '+' || p[1] == '-')) {
                    p += 2;
                }else if(isdigit(*p) || *p == '.' || isalpha(*p) || *p == '_') {
                    p++;
                }else{
                    break;
                }
            }
            cur = new_pptoken(PPTK_PPNUMBER, cur, b, p - b);
            continue;
        }

        int punc_len = match_punc(p);
        if(punc_len) {
            if(punc_len == 1 && *p == '#') {
                include_state = 1;
            }
            cur = new_pptoken(PPTK_PUNC, cur, p, punc_len);
            if(p > user_input && (p[-1] == ' ' || p[-1] == '\t')) {
                cur->preceded_by_space = true;
            }
            p += punc_len;
            continue;
        }

        if(isalpha(*p) || *p == '_') {
            char *b = p;
            p++;
            while(isalpha(*p) || isdigit(*p) || *p == '_') {
                p++;
            }
            cur = new_pptoken(PPTK_IDENT, cur, b, p - b);
            if(prev_include_state == 1 && memcmp("include", b, p - b) == 0) {
                include_state = 2;
            }
            continue;
        }

        if(*p == '\'') {
            char *q = p;
            p++;
            char c;
            if(*p == '\\') {
                p++;
                c = read_escape(&p);
                p++;
            } else {
                c = *p;
                p++;
            }
            if(*p != '\''){
                error("expect ' (single quote)");
            }
            cur = new_pptoken(PPTK_CHAR_CONST, cur, q, p - q);
            cur->val = c;
            p++;
            continue;
        }

        if(*p == '"') {
            Vector *char_vec;
            int len = pp_match_string_literal(p, &char_vec);
            cur = new_pptoken(PPTK_STRING_LITERAL, cur, p + 1, len - 2);
            cur->literal = char_vec;
            cur->literal_len = vector_size(char_vec);
            p += len;
            continue;
        }

        cur = new_pptoken(PPTK_OTHER, cur, p, 1);
        p++;
    }
    cur = new_pptoken(PPTK_EOF, cur, p, 1);
    return head.next;
}

void pp_dump_token(PPToken *cur) {
    for(; cur; cur = cur->next) {
        if(cur->kind == PPTK_CHAR_CONST) {
            fprintf(stderr, "char: %d\n", cur->val);
        }else if(cur->kind == PPTK_NEWLINE) {
            fprintf(stderr, "new line\n");
            //fprintf(stderr, "kind: %d '%.*s'\n", cur->kind, cur->len, cur->str);
        }else{
            fprintf(stderr, "kind: %s '%.*s' preceded:%d\n", pp_tokenkind_str(cur->kind), cur->len, cur->str, cur->preceded_by_space);
        }
    }
}

static PPToken *pp_new_empty_list() {
    PPToken *empty = calloc(1, sizeof(PPTokenKind));
    empty->kind = PPTK_EOF;
    return empty;
}

static void pp_next_token(PPToken **cur) {
    *cur = (*cur)->next;
}

static bool pp_consume(PPToken **cur, char *str) {
    if(pp_peek(cur, str)) {
        pp_next_token(cur);
        return true;
    }
    return false;
}

static bool pp_expect(PPToken **cur, char *str) {
    if(pp_peek(cur, str)) {
        pp_next_token(cur);
        return true;
    }
    error_at((*cur)->str, "Expect %s\n", str);
    return false;
}

static bool pp_expect_newline(PPToken **cur) {
    if(pp_peek_newline(cur)) {
        pp_next_token(cur);
        return true;
    }
    error_at((*cur)->str, "Expect newline\n");
    return false;
}

static bool pp_expect_ident(PPToken **cur, char **ident, int *ident_len) {
    if(pp_peek_ident(cur, ident, ident_len)) {
        pp_next_token(cur);
        return true;
    }
    error_at((*cur)->str, "Expect identifier");
    return false;
}

static bool pp_peek(PPToken **cur, char *str) {
    if(((*cur)->kind == PPTK_IDENT || (*cur)->kind == PPTK_PUNC) && (*cur)->len == strlen(str) && strncmp((*cur)->str, str, strlen(str)) == 0) {
        return true;
    }
    return false;
}


static bool pp_consume_newline(PPToken **cur) {
    if(pp_peek_newline(cur)) {
        pp_next_token(cur);
        return true;
    }
    return false;
}

static bool pp_peek_newline(PPToken **cur) {
    if((*cur)->kind == PPTK_NEWLINE) {
        return true;
    }
    return false;
}

static bool pp_peek_ident(PPToken **cur, char **ident, int *ident_len) {
    if((*cur)->kind == PPTK_IDENT) {
        *ident = (*cur)->str;
        *ident_len = (*cur)->len;
        return true;
    }
    return false;
}

static bool pp_at_eof(PPToken **cur) {
    return (*cur)->kind == PPTK_EOF;
}

static bool pp_compare_punc(PPToken *token, char *str) {
    return token->kind == PPTK_PUNC && token->len == strlen(str) && memcmp(token->str, str, token->len) == 0;
}

static PPToken *preprocessing_file(PPToken **cur);
PPToken *pp_parse(PPToken **cur) {
    return preprocessing_file(cur);
}

static PPToken *group_part(PPToken **cur);
static PPToken *if_section(PPToken **cur);
static PPToken *if_group(PPToken **cur);
static PPToken *control_line(PPToken **cur);
static PPToken *non_directive(PPToken **cur);
static PPToken *text_line(PPToken **cur);
static int pp_constant_expression(PPToken **cur);
static int pp_conditional_expression(PPToken **cur);
static int pp_logical_OR_expression(PPToken **cur);
static int pp_logical_AND_expression(PPToken **cur);
static int pp_inclusive_OR_expression(PPToken **cur);
static int pp_exclusive_OR_expression(PPToken **cur);
static int pp_AND_expression(PPToken **cur);
static int pp_equality_expression(PPToken **cur);
static int pp_relational_expression(PPToken **cur);
static int pp_shift_expression(PPToken **cur);
static int pp_additive_expression(PPToken **cur);
static int pp_multiplicative_expression(PPToken **cur);
static int pp_cast_expression(PPToken **cur);
static int pp_unary_expression(PPToken **cur);
static int pp_postfix_expression(PPToken **cur);
static int pp_primary_expression(PPToken **cur);
static int pp_expression(PPToken **cur);
static int pp_assignment_expression(PPToken **cur);

static void macro_invocation(PPToken **cur, Vector **arg_vec, int arglen);

static PPToken *preprocessing_file(PPToken **cur) {
    PPToken head = {};
    PPToken *tail = &head;

    while(!pp_at_eof(cur)) {
        tail->next = group_part(cur);
        // pp_dump_token(tail);
        tail = pp_list_tail(tail);
        tail = new_pptoken(PPTK_NEWLINE, tail, (*cur)->str, (*cur)->len);
        tail->filename = (*cur)->filename;
        tail->line_number = (*cur)->line_number;
    }
    return head.next;
}

struct MacroRegistryEntry {
    bool func;
    char *ident;
    int ident_len;
    Vector *param_list;
    Vector *rep_list;
    bool vararg;
    bool is_file_macro;
    bool is_line_macro;
};

MacroRegistryEntry *find_macro(char *ident, int ident_len) {
    MacroRegistryEntry *entry;
    for(int i = 0; i < vector_size(macro_registry); i++) {
        entry = vector_get(macro_registry, i);
        if(compare_ident(ident, ident_len, entry->ident, entry->ident_len)) {
            return entry;
        }
    }
    return NULL;
}

bool macro_is_defined(char *ident, int ident_len) {
    return find_macro(ident, ident_len) != NULL;
}

// group-part:
//     if-section  -> #if, #ifdef or #ifndef
//     control-line -> #include, #define, ... or # new-line
//     text-line -> otherwise (not '#')
//     # non-directive -> #
static PPToken *group_part(PPToken **cur) {
    if(pp_consume(cur, "#")) {
        if(pp_peek(cur, "if") || pp_peek(cur, "ifdef") || pp_peek(cur, "ifndef")) {
            // if-section
            return if_section(cur);
        }else if(pp_peek(cur, "include") || pp_peek(cur, "define") || pp_peek(cur, "undef") || pp_peek(cur, "line")
                || pp_peek(cur, "error") || pp_peek(cur, "pragma") || pp_peek_newline(cur)) {
            return control_line(cur);
        }else if(pp_peek(cur, "elif") || pp_peek(cur, "else") || pp_peek(cur, "endif")){
            return NULL;
        }else{
            return non_directive(cur);
        }
    } else {
        return text_line(cur);
    }
}

static PPToken *if_section(PPToken **cur) {
    return if_group(cur);
}

static bool eval_constant_expression(PPToken **cur) {
    PPToken head = {};
    PPToken *tail = &head;

    // Process defined operator
    while(!pp_consume_newline(cur) && !pp_at_eof(cur)) {
        if(pp_consume(cur, "defined")) {
            bool paren = pp_consume(cur, "(");
            char *ident;
            int ident_len;
            pp_expect_ident(cur, &ident, &ident_len);
            bool found = macro_is_defined(ident, ident_len);
            if(paren) {
                pp_expect(cur, ")");
            }
            tail = new_pptoken(PPTK_PPNUMBER, tail, found ? "1" : "0", 1);
        } else {
            tail = dup_pptoken(tail, *cur);
            pp_next_token(cur);
        }
    }

    tail = new_pptoken(PPTK_EOF, tail, (*cur)->prev->str, (*cur)->prev->len);
    PPToken *tmp_cur = head.next;
    // debug_log("dump");
    // pp_dump_token(tmp_cur);
    PPToken *macro_replaced_tokens = text_line(&tmp_cur);
    PPToken *tmp2 = macro_replaced_tokens;
    PPToken *mtail = pp_list_tail(macro_replaced_tokens);
    new_pptoken(PPTK_EOF, mtail, (*cur)->prev->str, (*cur)->prev->len);

    int val = pp_constant_expression(&macro_replaced_tokens);
    if(macro_replaced_tokens->next) {
        error_at(macro_replaced_tokens->next->str, "Extra token after constant expression");
    }
    return val;
}

void pp_skip_until_newline(PPToken **cur) {
    while(!pp_consume_newline(cur)) {
        pp_next_token(cur);
    }
}

void pp_skip_group(PPToken **cur) {
    int level = 0;
    while(!pp_at_eof(cur)) {
        PPToken *pre = *cur;
        if(pp_consume(cur, "#")) {
            if(pp_consume(cur, "if") || pp_consume(cur, "ifdef") || pp_consume(cur, "ifndef")) {
                level++;
            }else if(pp_consume(cur, "elif") || pp_consume(cur, "else")) {
                if(!level) {
                    *cur = pre;
                    break;
                }
            }else if(pp_consume(cur, "endif")) {
                if(!level) {
                    *cur = pre;
                    break;
                }
                level--;
            }
        }
        pp_skip_until_newline(cur);
    }
}

static PPToken *if_group(PPToken **cur) {
    bool if_condition_met = false;
    if(pp_consume(cur, "if")) {
        // debug_log("if start %.*s", 20, (*cur)->str);
        if_condition_met = eval_constant_expression(cur);
    }else if(pp_consume(cur, "ifdef")) {
        char *ident;
        int ident_len;
        pp_expect_ident(cur, &ident, &ident_len);
        pp_expect_newline(cur);
        if_condition_met = macro_is_defined(ident, ident_len);
        // debug_log("ifdef check %.*s %d", ident_len, ident, if_condition_met);
    }else if(pp_consume(cur, "ifndef")) {
        char *ident;
        int ident_len;
        pp_expect_ident(cur, &ident, &ident_len);
        pp_expect_newline(cur);
        if_condition_met = !macro_is_defined(ident, ident_len);
    }
    bool any_met = if_condition_met;

    // if condition is not met, containing group is scanned only for nested if directive.
    PPToken head = {};
    PPToken *tail = &head;
    int state = 0;
    while(1) {
        if(pp_at_eof(cur)) {
            error("Expect #endif");
        }
        PPToken *pre = *cur;
        if(pp_consume(cur, "#")) {
            if(pp_consume(cur, "elif")) {
                if(state != 0) {
                    error_at((*cur)->str, "Invalid elif directive (after else?)");
                }
                bool met = eval_constant_expression(cur);
                if(!any_met && met) {
                    if_condition_met = true;
                    any_met = true;
                }else {
                    if_condition_met = false;
                }
                continue;
            } else if(pp_consume(cur, "else")) {
                if(state != 0) {
                    error_at((*cur)->str, "Invalid else directive (multiple else?)");
                }
                state = 1;
                pp_expect_newline(cur);
                if(!any_met) {
                    if_condition_met = true;
                    any_met = true;
                } else {
                    if_condition_met = false;
                }
                continue;
            } else if(pp_consume(cur, "endif")) {
                pp_expect_newline(cur);
                break;
            } else {
                *cur = pre;
            }
        }
        if(!if_condition_met) {
            pp_skip_group(cur);
        } else {
            tail->next = group_part(cur);
            tail = pp_list_tail(tail);
            tail = new_pptoken(PPTK_NEWLINE, tail, "\n", 1);
            tail->filename = (*cur)->filename;
            tail->line_number = (*cur)->line_number;
        }
    }
    return head.next;
}

static bool file_exists(char *file) {
    FILE *fp = fopen(file, "r");
    if(fp == NULL) {
        return false;
    }
    fclose(fp);
    return true;
}

static PPToken *control_line(PPToken **cur) {
    if(pp_consume(cur, "include")) {
        PPToken *headername = text_line(cur);
        if(headername->next != NULL) {
            error("Extra token after header name");
        }
        char *file;
        if(headername->kind == PPTK_HEADER_NAME) {
            file = calloc(1, headername->len - 2 + 1);
            memcpy(file, headername->str + 1, headername->len - 2);
        }else if(headername->kind == PPTK_STRING_LITERAL) {
            file = calloc(1, headername->len + 1);
            memcpy(file, headername->str, headername->len);
        }else{
            error("Invalid token as header name");
        }

        char *p = file;
        bool found = false;
        for(int i = 0; i < vector_size(include_pathes); i++) {
            char *d = vector_get(include_pathes, i);
            p = calloc(1, strlen(d) + strlen(file) + 2);
            strcpy(p, d);
            strcat(p, "/");
            strcat(p, file);
            debug_log("File check %s", p);
            if(file_exists(p)) {
                debug_log("Found %s", p);
                found = true;
                break;
            }
            free(p);
        }
        if(!found) {
            p = file;
            if(file_exists(p)) {
                found = true;
            }else{
                error("file %s not found", file);
            }
        }

        debug_log("include %s", p);
        char *tmp_filename = filename;
        char *tmp_user_input = user_input;
        int tmp_user_input_len = user_input_len;
        Vector *tmp_line_map = line_map;

        filename = p;
        line_map = new_vector();
        PPToken *token = pp_parse_file();

        filename = tmp_filename;
        user_input = tmp_user_input;
        user_input_len = tmp_user_input_len;
        line_map = tmp_line_map;

        PPToken *tail = pp_list_tail(token);
        tail = new_pptoken(PPTK_NEWLINE, tail, (*cur)->str, (*cur)->len);
        return token;
    } else if(pp_consume(cur, "define")) {
        MacroRegistryEntry *entry = calloc(1, sizeof(MacroRegistryEntry));
        pp_expect_ident(cur, &entry->ident, &entry->ident_len);
        entry->rep_list = new_vector();

        if(pp_debug) {
            PPToken *tmp = (*cur)->prev;
            debug_log("Process define %s:%d %.*s", (*cur)->filename, (*cur)->line_number, pp_length_in_line(tmp), tmp->str);
        }

        // function-like macro definition must have "(" which is not preceded by space
        if(!(*cur)->preceded_by_space && pp_consume(cur, "(")) {
            // define function-like macro
            entry->func = true;
            entry->param_list = new_vector();
            if(!pp_consume(cur, ")")) {
                while(1) {
                    char *ident;
                    int ident_len;
                    if(pp_consume(cur, "...")) {
                        entry->vararg = true;
                        break;
                    }
                    pp_expect_ident(cur, &ident, &ident_len);
                    vector_push(entry->param_list, (*cur)->prev);

                    if(!pp_consume(cur, ",")) {
                        break;
                    }
                }
                pp_expect(cur, ")");
            }
        }

        while(!pp_consume_newline(cur) && !pp_at_eof(cur)) {
            vector_push(entry->rep_list, (*cur));
            (*cur) = (*cur)->next;
        }
        vector_push(macro_registry, entry);
    } else if(pp_consume(cur, "undef")) {
        char *ident;
        int ident_len;
        pp_expect_ident(cur, &ident, &ident_len);
        MacroRegistryEntry *entry = find_macro(ident, ident_len);
        if(entry) {
            vector_remove(macro_registry, entry);
        }
        pp_expect_newline(cur);
    } else if(pp_consume(cur, "line")) {
        while(!pp_consume_newline(cur) && !pp_at_eof(cur)) {
            (*cur) = (*cur)->next;
        }
    } else if(pp_consume(cur, "error")) {
        while(!pp_consume_newline(cur) && !pp_at_eof(cur)) {
            if((*cur)->preceded_by_space) {
                fprintf(stderr, " ");
            }
            fprintf(stderr, "%.*s", (*cur)->len, (*cur)->str);
            (*cur) = (*cur)->next;
        }
        error("# error");
    } else if(pp_consume(cur, "pragma")) {
        while(!pp_consume_newline(cur) && !pp_at_eof(cur)) {
            fprintf(stderr, "pragma token: %.*s", (*cur)->len, (*cur)->str);
            (*cur) = (*cur)->next;
        }
    } else {
        while(!pp_consume_newline(cur) && !pp_at_eof(cur)) {
            (*cur) = (*cur)->next;
        }
    }
    return NULL;
}

static PPToken *non_directive(PPToken **cur) {
    while(!pp_consume_newline(cur) && !pp_at_eof(cur)) {
        (*cur) = (*cur)->next;
    }
    return NULL;
}

static PPToken *text_line(PPToken **cur) {
    //debug_log("text_line %.*s", (*cur)->len, (*cur)->str);
    PPToken gen_head = {};
    PPToken *gen_tail = &gen_head;

    while(!pp_consume_newline(cur) && !pp_at_eof(cur)) {
        char *ident;
        int ident_len;
        if(pp_peek_ident(cur, &ident, &ident_len)) {
            PPToken *macro_ident_token = *cur;
            MacroRegistryEntry *entry = find_macro(ident, ident_len);
            if(entry && !pp_check_in_history(macro_ident_token, macro_ident_token->str, macro_ident_token->len)) {
                // debug_log("Macro: %.*s func: %d", ident_len, ident, entry->func);
                pp_next_token(cur);
                if(entry->func) {
                    // If defined macro is func-like and "(" follows, it is macro invocation.
                    // TOOD: ignore new-line before "("
                    if(pp_consume(cur, "(")) {
                        // debug_log("Macro invocation");
                        Vector *vec;
                        // Parse macro invocation until ')'.
                        // vec is a list of each argument.
                        macro_invocation(cur, &vec, vector_size(entry->param_list));
                        // *cur points the token immidiately after ')'

                        if(vector_size(vec) < vector_size(entry->param_list)) {
                            error_at((*cur)->str, "Argument number in macro invocation too few: %.*s", ident_len, ident);
                        }
                        if(!entry->vararg && vector_size(vec) != vector_size(entry->param_list)) {
                            error_at((*cur)->str, "Argument number in macro invocation doesn't match: %.*s, supplied %d, expected %d", ident_len, ident,
                                    vector_size(vec), vector_size(entry->param_list));
                        }

                        PPToken *rep_out = scan_replacement_list(entry, vec);
                        for(PPToken *rep_out_tmp = rep_out; rep_out_tmp; rep_out_tmp = rep_out_tmp->next) {
                            rep_out_tmp->history = vector_dup(macro_ident_token->history);
                            pp_list_push_history(rep_out, entry->ident, entry->ident_len);
                        }
                        process_token_concat_operator(rep_out);

                        // rep_out: Tokens generated by rep_list
                        // *cur: Points the token immidiately after ")"
                        // Concat them.
                        if(rep_out != NULL) {
                            PPToken *cur2 = pp_list_tail(rep_out);
                            cur2->next = *cur;
                            (*cur)->prev = cur2;
                            // rescan from beggining of the newly instroduced tokens.
                            *cur = rep_out;
                        }
                        continue;
                    } else {
                        // It was not a macro invocation.
                        // To processes it as non macro tokens, rollback pointer.
                        *cur = macro_ident_token;
                    }
                } else {
                    // object-like macro
                    PPToken rep_out_head = {};
                    PPToken *rep_out = &rep_out_head; // Tokens generated by rep_list.
                    if(entry->is_file_macro) {
                        rep_out = pp_new_str_token(rep_out, filename, strlen(filename));
                        rep_out->preceded_by_space = true;
                    } else if(entry->is_line_macro) {
                        char buf[100];
                        int line = 1;
                        if(ident >= user_input && user_input + vector_size(line_map) > ident) {
                            line = (long)vector_get(line_map, ident - user_input);
                        }
                        sprintf(buf, "%d", line);
                        rep_out = new_pptoken(PPTK_PPNUMBER, rep_out, mystrdup(buf), strlen(buf));
                        rep_out->preceded_by_space = true;
                    }else {
                        for(int i = 0; i < vector_size(entry->rep_list); i++) {
                            PPToken *token = vector_get(entry->rep_list, i);
                            rep_out = dup_pptoken(rep_out, token);
                            rep_out->history = vector_dup(macro_ident_token->history);
                        }
                    }
                    pp_list_push_history(rep_out_head.next, entry->ident, entry->ident_len);
                    if(rep_out_head.next) {
                        rep_out_head.next->prev = NULL;
                    }
                    process_token_concat_operator(rep_out_head.next);
                    if(rep_out_head.next != NULL) {
                        rep_out = pp_list_tail(rep_out_head.next);
                        rep_out->next = *cur;
                        (*cur)->prev = rep_out;
                        // rescan from beggining of newly instroduced tokens.
                        *cur = rep_out_head.next;
                    }
                    continue;
                }
            }
        }
        //debug_log("dup: %.*s\n", (*cur)->len, (*cur)->str);
        gen_tail = dup_pptoken(gen_tail, *cur);
        pp_next_token(cur);
    }
    return gen_head.next;
}

static void macro_invocation(PPToken **cur, Vector **arg_vec, int arglen) {
    int paren_level = 0;
    Vector *vec = new_vector();
    *arg_vec = vec;
    PPToken head = {};
    PPToken *tail = &head;

    while(!pp_at_eof(cur)) {
        if(paren_level == 0 && pp_consume(cur, ")")) {
            break;
        } else if(paren_level == 0 && vector_size(vec) < arglen && pp_consume(cur, ",")) {
            // Don't match for extra arg for vararg.
            head.next->prev = NULL;
            vector_push(vec, head.next);
            tail = &head;
        } else {
            if(pp_consume(cur, "(")) {
                tail = dup_pptoken(tail, (*cur)->prev);
                paren_level++;
            }else if(pp_consume(cur, ")")) {
                tail = dup_pptoken(tail, (*cur)->prev);
                paren_level--;
            }else if(pp_consume_newline(cur)) {
                // remove newline. newline in macro invocation is processed as normal white space.
            }else{
                // Match for normal tokens or extra arg for vararg.
                tail = dup_pptoken(tail, *cur);
                pp_next_token(cur);
            }
        }
    }
    head.next->prev = NULL;
    vector_push(vec, head.next);
}

static PPToken *scan_replacement_list(MacroRegistryEntry *entry, Vector *vec) {
    // Scan replacement list to find parameter indentifiers.
    // Then replace it with properly processed argument.

    PPToken rep_out_head = {};
    PPToken *rep_out = &rep_out_head; // Tokens generated by rep_list.
    bool found_sharp = false;
    for(int i = 0; i < vector_size(entry->rep_list); i++) {
        PPToken *token = vector_get(entry->rep_list, i);
        PPToken head = {};
        PPToken *tail = &head;
        bool found = false;
        // debug_log("macro param %.*s %d", token->len, token->str, compare_slice(token->str, token->len, "__VA_ARGS__"));
        if(compare_slice(token->str, token->len, "__VA_ARGS__")) {
            if(!entry->vararg) {
                error_at(token->str, "__VA_ARGS__ paramter can't be used on non-vararg macro");
            }
            if(vector_size(vec) > vector_size(entry->param_list)) {
                tail->next = pp_dup_list(vector_get(vec, vector_size(vec) - 1));
                tail = pp_list_tail(tail);
            }
            found = true;
        }else if(compare_slice(token->str, token->len, "#")) {
            if(i + 1 == vector_size(entry->rep_list)) {
                error_at(token->str, "# operator must be followed by macro parameter");
            }
            found_sharp = true;
            continue;
        }else {
            for(int j = 0; j < vector_size(entry->param_list); j++) {
                PPToken *param = vector_get(entry->param_list, j);
                if(compare_ident(param->str, param->len, token->str, token->len)) {
                    found = true;
                    tail->next = pp_dup_list(vector_get(vec, j));
                    tail = pp_list_tail(tail);
                    break;
                }
            }
        }
        
        if(found_sharp) {
            found_sharp = false;
            if(!found) {
                error_at(token->str, "# operator must be followed by macro parameter");
            }
            rep_out = make_string(head.next, rep_out);
            continue;
        }

        if(found) {
            // Parameter token
            // TODO: Check if token is preceded or followed by #,##
            // Scan an argument.
            // An argument is scanned as if the argument makes up whole preprocessing_file.
            PPToken *tmp = pp_list_tail(head.next);
            new_pptoken(PPTK_EOF, tmp, tmp->str, tmp->len);

            // pp_dump_token(head);
            PPToken *new_head = text_line(&head.next);

            // Make sure introduced token don't join to previous one.
            new_head->preceded_by_space = true;

            // pp_dump_token(new_head);

            if(new_head == NULL) {
                // Generate place marker for empty list if it is operand of ##.
                bool found_sharp = false;
                if(i > 0) {
                    PPToken *prev_token = vector_get(entry->rep_list, i - 1);
                    if(pp_compare_punc(prev_token, "##")) {
                        found_sharp = true;
                    }
                }
                if(i + 1 < vector_size(entry->rep_list)) {
                    PPToken *next_token = vector_get(entry->rep_list, i + 1);
                    if(pp_compare_punc(next_token, "##")) {
                        found_sharp = true;
                    }
                }
                if(found_sharp) {
                    new_head = new_pptoken(PPTK_PLACE_MARKER, new_head, "", 0);
                }
            }
            rep_out->next = new_head;
            new_head->prev = rep_out;
            while(rep_out->next) {
                rep_out = rep_out->next;
            }
        } else {
            // Non-parameter tokens
            rep_out = dup_pptoken(rep_out, token);
        }
    }

    if(rep_out_head.next) {
        rep_out_head.next->prev = NULL;
    }
    return rep_out_head.next;
}

static void process_token_concat_operator(PPToken *cur) {
    // Process ## operator
    PPToken *cur2 = cur;
    for(; cur2; cur2 = cur2->next) {
        if(pp_compare_punc(cur2, "##")) {
            if(cur2->prev == NULL) {
                error_at(cur2->str, "## operator cannot appear on the beginning of replacement list");
            }
            if(cur2->next == NULL) {
                error_at(cur2->str, "## operator cannot appear on the end of replacement list");
            }
            PPToken *r = cur2->prev;
            PPToken *l = cur2->next;
            if(r->kind == PPTK_PLACE_MARKER) {
                // Place marker + other token = other token
                // (include place + place)
                *r = *l;
                if(r->next) {
                    r->next->prev = r;
                }
            } else if(l->kind == PPTK_PLACE_MARKER) {
                // Other token + Place marker = other token
                r->next = l->next;
                if(r->next) {
                    r->next->prev = r;
                }
            } else {
                // Generate new token
                char *old_str = r->str;
                int len = r->len + l->len;
                char *p = calloc(1, len);
                memcpy(p, r->str, r->len);
                memcpy(p + r->len, l->str, l->len);

                r->str = p;
                r->len = len;
                int punc_len = match_punc(p);
                if(p[0] == '.' && isdigit(p[1]) || isdigit(p[0])) {
                    r->kind = PPTK_PPNUMBER;
                } else if(punc_len == len) {
                    r->kind = PPTK_PUNC;
                }else if(isalpha(*p) || *p == '_') {
                    r->kind = PPTK_IDENT;
                }else if(*p == '"') {
                    Vector *char_vec;
                    int mlen = pp_match_string_literal(p, &char_vec);
                    if(mlen != len) {
                        error_at(old_str, "Invalid string literal was generated from ## operator");
                    }

                    r->literal = char_vec;
                    r->literal_len = vector_size(char_vec);
                    r->kind = PPTK_STRING_LITERAL;
                }else if(*p == '\'') {
                    r->kind = PPTK_CHAR_CONST;
                    char *q = p;
                    p++;
                    char c;
                    if(*p == '\\') {
                        p++;
                        c = read_escape(&p);
                        p++;
                    } else {
                        c = *p;
                        p++;
                    }
                    if(*p != '\''){
                        error("expect ' (single quote)");
                    }
                    p++;
                    if(p - q != len) {
                        error_at(old_str, "Invalid char const was generated from ## operator: %.*s", r->len, r->str);
                    }
                    r->val = c;
                }else {
                    r->kind = PPTK_OTHER;
                }
                r->next = cur2->next->next;
                if(r->next) {
                    r->next->prev = r;
                }
            }
        }
    }
    // Remove place marker
    cur2 = cur;
    while(cur2) {
        if(cur2->kind == PPTK_PLACE_MARKER) {
            cur2->prev->next = cur2->next;
            cur2->next->prev = cur2->prev;
        } else {
            cur2 = cur2->next;
        }
    }
}

static PPToken *make_string(PPToken *token, PPToken *cur) {
    Vector *char_vec = new_vector();
    for(; token; token = token->next) {
        for(int i = 0; i < token->len; i++) {
            vector_push(char_vec, (void *)(long)token->str[i]);
        }
        if(token->next) {
            vector_push(char_vec, (void *)(long)' ');
        }
    }
    char *ret = malloc(vector_size(char_vec) + 3);
    ret[0] = '"';
    for(int i = 0; i < vector_size(char_vec); i++){
        ret[i+1] = (char)(long)vector_get(char_vec, i);
    }
    ret[vector_size(char_vec) + 1] = '"';
    ret[vector_size(char_vec) + 2] = '\0';
    cur = new_pptoken(PPTK_STRING_LITERAL, cur, ret + 1, vector_size(char_vec));
    cur->literal = char_vec;
    cur->literal_len = vector_size(char_vec);
    return cur;
}

static PPToken *pp_new_str_token(PPToken *cur, char *str, int len) {
    char *p = malloc(len + 3);
    p[0] = '"';
    memcpy(p + 1, str, len);
    p[len + 1] = '"';
    p[len + 2] = '\0';
    cur = new_pptoken(PPTK_STRING_LITERAL, cur, p + 1, len + 2);
    cur->literal = new_vector();
    for(int i = 0; i < len; i++) {
        vector_push(cur->literal, (void *)(long)filename[i]);
    }
    cur->literal_len = len;
    return cur;
}

static int eval_as_int(PPToken *token) {
    if(token->kind == PPTK_PPNUMBER) {
        char *en;
        int ret = strtol(token->str, &en, 0);
        if(*en == 'L'){
            en++;
        }
        if(en != token->str + token->len) {
            error("Invalid number token %.*s", token->len, token->str);
        }
        return ret;
    }
    // Don't expand macro because the token is already macro expanded.
    if(token->kind == PPTK_IDENT) {
        return 0;
    }
    error("Invalid token in constant expression");
    return 0;
}


static int pp_constant_expression(PPToken **cur) {
    return pp_conditional_expression(cur);
}

static int pp_conditional_expression(PPToken **cur) {
    int val = pp_logical_OR_expression(cur);
    if(pp_consume(cur, "?")) {
        int if_true = pp_expression(cur);
        pp_consume(cur, ":");
        int if_false = pp_conditional_expression(cur);

        if(val) {
            return if_true;
        }else{
            return if_false;
        }
    }
    return val;
}

static int pp_logical_OR_expression(PPToken **cur) {
    int ret = pp_logical_AND_expression(cur);
    while(pp_consume(cur, "||")) {
        // Be careful of short circuit!
        int val = pp_logical_AND_expression(cur);
        ret = ret || val;
    }
    return ret;
}

static int pp_logical_AND_expression(PPToken **cur) {
    int ret = pp_inclusive_OR_expression(cur);
    while(pp_consume(cur, "&&")) {
        // Be careful of short circuit!
        int val = pp_inclusive_OR_expression(cur);
        ret = ret && val;
    }
    return ret;
}

static int pp_inclusive_OR_expression(PPToken **cur) {
    int ret = pp_exclusive_OR_expression(cur);
    while(pp_consume(cur, "|")) {
        ret = ret | pp_exclusive_OR_expression(cur);
    }
    return ret;
}

static int pp_exclusive_OR_expression(PPToken **cur) {
    int ret = pp_AND_expression(cur);
    while(pp_consume(cur, "^")) {
        ret = ret ^ pp_AND_expression(cur);
    }
    return ret;
}

static int pp_AND_expression(PPToken **cur) {
    int ret = pp_equality_expression(cur);
    while(pp_consume(cur, "&")) {
        ret = ret & pp_equality_expression(cur);
    }
    return ret;
}

static int pp_equality_expression(PPToken **cur) {
    int ret = pp_relational_expression(cur);
    while(1) {
        if(pp_consume(cur, "==")) {
            ret = ret == pp_relational_expression(cur);
        }else if(pp_consume(cur, "!=")) {
            ret = ret != pp_relational_expression(cur);
        }else {
            break;
        }
    }
    return ret;
}

static int pp_relational_expression(PPToken **cur) {
    int ret = pp_shift_expression(cur);
    while(1) {
        if(pp_consume(cur, "<")) {
            ret = ret < pp_shift_expression(cur);
        }else if(pp_consume(cur, ">")) {
            ret = ret > pp_shift_expression(cur);
        }else if(pp_consume(cur, "<=")) {
            ret = ret <= pp_shift_expression(cur);
        }else if(pp_consume(cur, ">=")) {
            ret = ret >= pp_shift_expression(cur);
        }else {
            break;
        }
    }
    return ret;
}

static int pp_shift_expression(PPToken **cur) {
    int ret = pp_additive_expression(cur);
    while(1) {
        if(pp_consume(cur, "<<")) {
            ret = ret << pp_additive_expression(cur);
        }else if(pp_consume(cur, ">>")) {
            ret = ret >> pp_additive_expression(cur);
        }else {
            break;
        }
    }
    return ret;
}

static int pp_additive_expression(PPToken **cur) {
    int ret = pp_multiplicative_expression(cur);
    while(1) {
        if(pp_consume(cur, "+")) {
            ret = ret + pp_multiplicative_expression(cur);
        }else if(pp_consume(cur, "-")) {
            ret = ret - pp_multiplicative_expression(cur);
        }else {
            break;
        }
    }
    return ret;
}

static int pp_multiplicative_expression(PPToken **cur) {
    int ret = pp_cast_expression(cur);
    while(1) {
        if(pp_consume(cur, "*")) {
            ret = ret * pp_cast_expression(cur);
        }else if(pp_consume(cur, "/")) {
            int val = pp_cast_expression(cur);
            if(val == 0) {
                error("Zero division");
            }
            ret = ret / val;
        }else if(pp_consume(cur, "%")) {
            int val = pp_cast_expression(cur);
            if(val == 0) {
                error("Zero division");
            }
            ret = ret % val;
        }else {
            break;
        }
    }
    return ret;
}

static int pp_cast_expression(PPToken **cur) {
    // no cast on preprocessor
    return pp_unary_expression(cur);
}

static int pp_unary_expression(PPToken **cur) {
    // no '&', '*', 'sizeof', '++', '--'
    if(pp_consume(cur, "!")) {
        return ! pp_cast_expression(cur);
    }
    if(pp_consume(cur, "+")) {
        return + pp_cast_expression(cur);
    }
    if(pp_consume(cur, "-")) {
        return - pp_cast_expression(cur);
    }
    if(pp_consume(cur, "~")) {
        return ~ pp_cast_expression(cur);
    }

    return pp_postfix_expression(cur);
}

static int pp_postfix_expression(PPToken **cur) {
    // no postfix operators
    return pp_primary_expression(cur);
}

static int pp_primary_expression(PPToken **cur) {
    if(pp_consume(cur, "(")) {
        int val = pp_expression(cur);
        pp_expect(cur, ")");
        return val;
    }
    int val = eval_as_int(*cur);
    pp_next_token(cur);
    return val;
}

static int pp_expression(PPToken **cur) {
    // no ',' operator
    return pp_assignment_expression(cur);
}

static int pp_assignment_expression(PPToken **cur) {
    // no assignment operators
    return pp_conditional_expression(cur);
}

static void append_printf(char **buf, char **tail, int *len, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int appendlen = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    int filled = *tail - *buf;
    if(filled + appendlen + 1 > *len) {
        int newlen = filled + appendlen + 1;
        if(newlen < *len * 2) {
            newlen = *len * 2;
        }
        *buf = realloc(*buf, newlen);
        *tail = *buf + filled;
        *len = newlen;
    }
    va_start(ap, fmt);
    vsnprintf(*tail, *len - filled, fmt, ap);
    va_end(ap);
    (*tail) += appendlen;
}

static char *reconstruct_tokens(PPToken *cur) {
    char *buf = calloc(1, 1);
    char *tail = buf;
    int len = 1;

    for(; cur; cur = cur->next) {
        if(cur->preceded_by_space) {
            append_printf(&buf, &tail, &len, " ");
        }
        if(cur->kind == PPTK_CHAR_CONST) {
            append_printf(&buf, &tail, &len, "%.*s", cur->len + 1, cur->str);
        }else if(cur->kind == PPTK_IDENT) {
            append_printf(&buf, &tail, &len, "%.*s", cur->len, cur->str);
        }else if(cur->kind == PPTK_PUNC) {
            append_printf(&buf, &tail, &len, "%.*s", cur->len, cur->str);
        }else if(cur->kind == PPTK_STRING_LITERAL) {
            append_printf(&buf, &tail, &len, "%.*s", cur->len + 2, cur->str - 1);
        }else if(cur->kind == PPTK_PPNUMBER) {
            append_printf(&buf, &tail, &len, "%.*s", cur->len, cur->str);
        }else if(cur->kind == PPTK_OTHER) {
            append_printf(&buf, &tail, &len, "%.*s", cur->len, cur->str);
        }else if(cur->kind == PPTK_NEWLINE) {
            append_printf(&buf, &tail, &len, "\n");
            append_printf(&buf, &tail, &len, "// %s:%d\n", cur->filename, cur->line_number);
        }
    }
    return buf;
}

static void inject_directive(char *directive) {
    user_input = directive;
    user_input_len = strlen(directive);
    filename = "<builtin>";

    char *processed = calloc(1, strlen(user_input) + 1);
    line_map = new_vector();
    pp_phase2(user_input, processed, line_map);

    PPToken *cur = pp_tokenize();
    PPToken *tail = pp_list_tail(cur);
    tail = new_pptoken(PPTK_NEWLINE, tail, "\n", 1);

    preprocessing_file(&cur);
}

PPToken *pp_parse_file() {
    user_input = read_file(filename);
    user_input_len = strlen(user_input);
    // debug_log("read file: '%s'\n", user_input);
    char *processed = calloc(1, strlen(user_input) + 1);
    pp_phase2(user_input, processed, line_map);
    // debug_log("processed file: '%s'\n", processed);

    user_input = processed;
    user_input_len = strlen(user_input);

    PPToken *pptoken = pp_tokenize();
    //pp_dump_token(pptoken);

    return pp_parse(&pptoken);
}

char *do_pp() {
    macro_registry = new_vector();
    char *tmp = user_input;
    char *tmp_filename = filename;
    inject_directive("#define __STDC__ 1");
    inject_directive("#define __STDC_HOSTED__ 1");
    inject_directive("#define __STDC_MB_MIGHT_NEQ_WC__ 1");
    inject_directive("#define __STDC_VERSION__ 199901L");
    char buf[1000], buf2[1100];
    time_t t;
    struct tm* tm_obj;
    t = time(NULL);
    tm_obj = localtime(&t);

    strftime(buf, sizeof(buf), "%b %d %Y", tm_obj);
    sprintf(buf2, "#define __DATE__ \"%s\"", buf);
    // buf must be malloc'ed because macro registry will have pointer to sub string.
    inject_directive(mystrdup(buf2));

    strftime(buf, sizeof(buf), "hh:mm:ss", tm_obj);
    sprintf(buf2, "#define __TIME__ \"%s\"", buf);
    inject_directive(mystrdup(buf2));

    MacroRegistryEntry *file_entry = calloc(1, sizeof(MacroRegistryEntry));
    file_entry->is_file_macro = true;
    file_entry->ident = "__FILE__";
    file_entry->ident_len = strlen(file_entry->ident);
    vector_push(macro_registry, file_entry);

    MacroRegistryEntry *line_entry = calloc(1, sizeof(MacroRegistryEntry));
    line_entry->is_line_macro = true;
    line_entry->ident = "__LINE__";
    line_entry->ident_len = strlen(line_entry->ident);
    vector_push(macro_registry, line_entry);

    user_input = tmp;
    line_map = new_vector();
    filename = tmp_filename;
    return reconstruct_tokens(pp_parse_file());
}

void init_include_pathes() {
    include_pathes = new_vector();
}

void append_include_pathes(char *p) {
    vector_push(include_pathes, p);
}

int pp_main(char *file) {
    filename = file;

    char *output = do_pp();
    printf("%s", output);

    return 0;
}

int pp_length_in_line(PPToken *cur) {
    char *p = strchr(cur->str, '\n');
    return p - cur->str;
}

