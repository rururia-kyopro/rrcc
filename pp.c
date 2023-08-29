#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include "rrcc.h"

typedef struct PPToken PPToken;
typedef struct MacroRegistryEntry MacroRegistryEntry;
Vector *macro_registry;

static void pp_next_token(PPToken **cur);
static bool pp_expect(PPToken **cur, char *str);
static bool pp_expect_newline(PPToken **cur);
static bool pp_expect_ident(PPToken **cur, char **ident, int *ident_len);
static bool pp_peek(PPToken **cur, char *str);
static bool pp_peek_newline(PPToken **cur);
static bool pp_peek_ident(PPToken **cur, char **ident, int *ident_len);
static PPToken *scan_replacement_list(MacroRegistryEntry *entry, Vector *vec);
PPToken *pp_parse_file();

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
        case PPTK_EOF: return "PPTK_EOF";
        case PPTK_DUMMY: return "PPTK_DUMMY";
    }
    assert(false);
}

// Remove backslack + new-line 
void pp_phase2(char *user_input, char *processed) {
    char *p = user_input;
    char *out = processed;
    while(*p) {
        if(*p == '\\') {
            p++;
            if(*p == '\n') {
                p++;
                continue;
            }
            p--;
        }
        *out = *p;
        out++;
        p++;
    }
    *out = '\0';
}

static PPToken *new_pptoken(PPTokenKind kind, PPToken *cur, char *str, int len){
    PPToken *tok = calloc(1, sizeof(PPToken));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    tok->prev = cur;
    cur->next = tok;
    if(str == user_input || (str[-1] == ' ' || str[-1] == '\t' || str[-1] == '\n')) {
        tok->preceded_by_space = true;
    }
    return tok;
}

static PPToken *dup_pptoken(PPToken *cur, PPToken *src) {
    PPToken *tok = calloc(1, sizeof(PPToken));
    memcpy(tok, src, sizeof(PPToken));
    tok->prev = cur;
    tok->next = NULL;
    cur->next = tok;
    return tok;
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

        // The longer the punctuator, the more it must be placed in front.
        static const char *punc[] = {
            // 4 chars
            "%:%:",
            // 3 chars
            "<<=", ">>=", "...",
            // 2 chars
            "->", "++", "--",
            "<<", ">>", "<=",
            ">=", "==", "!=",
            "&&", "||", "*=",
            "/=", "%=", "+=",
            "-=", "&=", "^=",
            "|=", "##", "<:",
            ":>", "<%", "%>"};

        bool found = false;
        int len = 0;
        for(int i = 0; i < sizeof(punc) / sizeof(punc[0]); i++){
            if(strncmp(p, punc[i], strlen(punc[i])) == 0) {
                found = true;
                len = strlen(punc[i]);
                break;
            }
        }
        if(found) {
            cur = new_pptoken(PPTK_PUNC, cur, p, len);
            p += len;
            continue;
        }
        if(strchr("[](){}.%*+-~!/%<>^|?:;=,#", *p)) {
            if(*p == '#') {
                include_state = 1;
            }
            cur = new_pptoken(PPTK_PUNC, cur, p, 1);
            if(p > user_input && (p[-1] == ' ' || p[-1] == '\t')) {
                cur->preceded_by_space = true;
            }
            p++;
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
            debug_log("read escape: %d %d\n", *p, '\\');
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
            p++;
            char *literal = p;
            int state = 0;
            Vector *char_vec = new_vector();
            while(*p) {
                if(state == 1) {
                    char c = read_escape(&p);
                    vector_push(char_vec, (void *)(long)c);
                    p++;
                    state = 0;
                    continue;
                }
                if(*p == '"') {
                    break;
                }
                if(*p == '\\'){
                    state = 1;
                }else{
                    vector_push(char_vec, (void *)(long)*p);
                    state = 0;
                }
                p++;
            }
            int len = p - literal;
            if(*p == '"') {
                p++;
                cur = new_pptoken(PPTK_STRING_LITERAL, cur, literal, len);
                cur->literal = char_vec;
                cur->literal_len = vector_size(char_vec);
                continue;
            } else {
                error_at(p, "expect \" (double quote)");
            }
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

static PPToken *preprocessing_file(PPToken **cur);
PPToken *pp_parse(PPToken **cur) {
    macro_registry = new_vector();
    return preprocessing_file(cur);
}

static PPToken *group_part(PPToken **cur);
static PPToken *if_section(PPToken **cur);
static PPToken *if_group(PPToken **cur);
static PPToken *control_line(PPToken **cur);
static PPToken *non_directive(PPToken **cur);
static PPToken *text_line(PPToken **cur);
static PPToken *constant_expression(PPToken **cur);
static PPToken *conditional_expression(PPToken **cur);
static PPToken *logical_OR_expression(PPToken **cur);
static PPToken *expression(PPToken **cur);
static PPToken *logical_AND_expression(PPToken **cur);
static PPToken *inclusive_OR_expression(PPToken **cur);
static PPToken *exclusive_OR_expression(PPToken **cur);
static PPToken *AND_expression(PPToken **cur);
static PPToken *equality_expression(PPToken **cur);
static PPToken *relational_expression(PPToken **cur);
static PPToken *shift_expression(PPToken **cur);
static PPToken *additive_expression(PPToken **cur);
static PPToken *multiplicative_expression(PPToken **cur);
static PPToken *cast_expression(PPToken **cur);
static PPToken *unary_expression(PPToken **cur);
static PPToken *postfix_expression(PPToken **cur);
static PPToken *primary_expression(PPToken **cur);

static void macro_invocation(PPToken **cur, Vector **arg_vec);

static PPToken *preprocessing_file(PPToken **cur) {
    PPToken head = {};
    PPToken *tail = &head;

    while(!pp_at_eof(cur)) {
        tail->next = group_part(cur);
        // pp_dump_token(tail);
        tail = pp_list_tail(tail);
        tail = new_pptoken(PPTK_NEWLINE, tail, (*cur)->str, (*cur)->len);
    }
    return head.next;
}

struct MacroRegistryEntry {
    bool func;
    char *ident;
    int ident_len;
    Vector *param_list;
    Vector *rep_list;
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
            bool found = find_macro(ident, ident_len) != NULL;
            if(paren) {
                pp_expect(cur, ")");
            }
            tail = new_pptoken(PPTK_PPNUMBER, tail, found ? "1" : "0", 1);
        } else {
            tail = dup_pptoken(tail, *cur);
        }
        pp_next_token(cur);
    }

    tail = new_pptoken(PPTK_EOF, tail, tail->str, tail->len);
    PPToken *tmp_cur = head.next;
    PPToken *macro_replaced_tokens = text_line(&tmp_cur);

    PPToken *constant_token = constant_expression(&macro_replaced_tokens);
    if(constant_token->next) {
        error_at(constant_token->next->str, "Extra token after constant expression");
    }
    if(constant_token->kind != PPTK_PPNUMBER) {
        error_at(constant_token->str, "Invalid constant expression");
    }
    debug_log("ki:%s %.*s\n", pp_tokenkind_str(constant_token->kind), constant_token->len, constant_token->str);
    return constant_token->val;
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
            if(pp_consume(cur, "if")) {
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
        if_condition_met = eval_constant_expression(cur);
    }else if(pp_consume(cur, "ifdef")) {
        char *ident;
        int ident_len;
        pp_expect_ident(cur, &ident, &ident_len);
        pp_expect_newline(cur);
        if_condition_met = find_macro(ident, ident_len) != NULL;
    }else if(pp_consume(cur, "ifndef")) {
        char *ident;
        int ident_len;
        pp_expect_ident(cur, &ident, &ident_len);
        pp_expect_newline(cur);
        if_condition_met = find_macro(ident, ident_len) == NULL;
    }
    bool any_met = if_condition_met;

    // if condition is not met, containing group is scanned only for nested if directive.
    PPToken head = {};
    PPToken *tail = &head;
    int state = 0;
    while(!pp_at_eof(cur)) {
        PPToken *pre = *cur;
        if(pp_consume(cur, "#")) {
            if(pp_consume(cur, "elif")) {
                if(state != 0) {
                    error("Invalid elif directive (after else?)");
                }
                bool met = eval_constant_expression(cur);
                if(!any_met && met) {
                    if_condition_met = true;
                    any_met = true;
                }
                continue;
            } else if(pp_consume(cur, "else")) {
                if(state != 0) {
                    error("Invalid else directive (multiple else?)");
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
        }
    }
    return head.next;
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

        debug_log("include %s", file);
        char *tmp_filename = filename;
        char *tmp_user_input = user_input;
        filename = file;
        PPToken *token = pp_parse_file();
        filename = tmp_filename;
        user_input = tmp_user_input;

        PPToken *tail = pp_list_tail(token);
        tail = new_pptoken(PPTK_NEWLINE, tail, (*cur)->str, (*cur)->len);
        return token;
    } else if(pp_consume(cur, "define")) {
        MacroRegistryEntry *entry = calloc(1, sizeof(MacroRegistryEntry));
        pp_expect_ident(cur, &entry->ident, &entry->ident_len);
        entry->rep_list = new_vector();

        // function-like macro definition must have "(" which is not preceded by space
        if(!(*cur)->preceded_by_space && pp_consume(cur, "(")) {
            // define function-like macro
            entry->func = true;
            entry->param_list = new_vector();
            while(1) {
                char *ident;
                int ident_len;
                pp_expect_ident(cur, &ident, &ident_len);
                vector_push(entry->param_list, (*cur)->prev);

                if(!pp_consume(cur, ",")) {
                    break;
                }
            }
            pp_expect(cur, ")");
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
    debug_log("text_line %.*s", (*cur)->len, (*cur)->str);
    PPToken gen_head = {};
    PPToken *gen_tail = &gen_head;

    while(!pp_consume_newline(cur) && !pp_at_eof(cur)) {
        char *ident;
        int ident_len;
        if(pp_peek_ident(cur, &ident, &ident_len)) {
            PPToken *macro_ident_token = *cur;
            MacroRegistryEntry *entry = find_macro(ident, ident_len);
            if(entry) {
                debug_log("Macro: %.*s func: %d", ident_len, ident, entry->func);
                pp_next_token(cur);
                if(entry->func) {
                    // If defined macro is func-like and "(" follows, it is macro invocation.
                    // TOOD: ignore new-line before "("
                    if(pp_consume(cur, "(")) {
                        debug_log("Macro invocation");
                        Vector *vec;
                        // Parse macro invocation until ')'.
                        // vec is a list of each argument.
                        macro_invocation(cur, &vec);
                        // *cur points the token immidiately after ')'

                        if(vector_size(vec) != vector_size(entry->param_list)) {
                            error("Argument number in macro invocation doesn't match");
                        }
                        debug_log("Arg length: %d\n", vector_size(vec));

                        PPToken *rep_out = scan_replacement_list(entry, vec);

                        // rep_out: Tokens generated by rep_list
                        // *cur: Points the token immidiately after ")"
                        // Concat them.
                        PPToken *cur2 = pp_list_tail(rep_out);
                        cur2->next = *cur;
                        (*cur)->prev = cur2;
                        // rescan from beggining of the newly instroduced tokens.
                        *cur = rep_out;
                        continue;
                    } else {
                        // It was not a macro invocation.
                        // To processes it as non macro tokens, rollback pointer.
                        *cur = macro_ident_token;
                    }
                } else {
                    PPToken rep_out_head = {};
                    PPToken *rep_out = &rep_out_head; // Tokens generated by rep_list.
                    for(int i = 0; i < vector_size(entry->rep_list); i++) {
                        PPToken *token = vector_get(entry->rep_list, i);
                        rep_out = dup_pptoken(rep_out, token);
                    }
                    rep_out->next = *cur;
                    (*cur)->prev = rep_out;
                    // rescan from beggining of newly instroduced tokens.
                    *cur = rep_out_head.next;
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

static void macro_invocation(PPToken **cur, Vector **arg_vec) {
    int paren_level = 0;
    Vector *vec = new_vector();
    *arg_vec = vec;
    PPToken head = {};
    PPToken *tail = &head;

    while(!pp_at_eof(cur)) {
        if(paren_level == 0 && pp_consume(cur, ")")) {
            break;
        } else if(paren_level == 0 && pp_consume(cur, ",")) {
            head.next->prev = NULL;
            vector_push(vec, head.next);
            tail = &head;
        } else {
            if(pp_consume(cur, "(")) {
                tail = dup_pptoken(tail, *cur);
                paren_level++;
            }else if(pp_consume(cur, ")")) {
                tail = dup_pptoken(tail, *cur);
                paren_level--;
            }else if(pp_consume_newline(cur)) {
                // remove newline. newline in macro invocation is processed as normal white space.
            }else{
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
    for(int i = 0; i < vector_size(entry->rep_list); i++) {
        PPToken *token = vector_get(entry->rep_list, i);
        bool found = false;
        int param_i = 0;
        for(int j = 0; j < vector_size(entry->param_list); j++) {
            PPToken *param = vector_get(entry->param_list, j);
            if(compare_ident(param->str, param->len, token->str, token->len)) {
                param_i = j;
                found = true;
                break;
            }
        }

        if(found) {
            // Parameter token
            // TODO: Check if token is preceded or followed by #,##
            // Scan an argument.
            // An argument is scanned as if the argument makes up whole preprocessing_file.
            PPToken *head = pp_dup_list(vector_get(vec, param_i));
            PPToken *tmp = pp_list_tail(head);
            new_pptoken(PPTK_EOF, tmp, tmp->str, tmp->len);

            debug_log("==Dump argument==");
            // pp_dump_token(head);
            debug_log("==End of dump argument==");
            PPToken *new_head = text_line(&head);
            debug_log("==Recursive ret==");

            // Make sure introduced token don't join to previous one.
            new_head->preceded_by_space = true;

            // pp_dump_token(new_head);

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
    return rep_out_head.next;
}

static PPToken *constant_expression(PPToken **cur) {
    if((*cur)->next) {
        error("Unsupported");
    }
    PPToken *ret = *cur;
    ret->val = atoi(ret->str);
    pp_next_token(cur);
    return ret;
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
        }
    }
    return buf;
}

PPToken *pp_parse_file() {
    user_input = read_file(filename);
    // debug_log("read file: '%s'\n", user_input);
    char *processed = calloc(1, strlen(user_input) + 1);
    pp_phase2(user_input, processed);
    // debug_log("processed file: '%s'\n", processed);

    user_input = processed;

    PPToken *pptoken = pp_tokenize();
    pp_dump_token(pptoken);

    return pp_parse(&pptoken);
}

char *do_pp() {
    return reconstruct_tokens(pp_parse_file());
}

int pp_main(int argc, char **argv) {
    filename = argv[2];

    char *output = do_pp();
    printf("%s", output);

    return 0;
}

