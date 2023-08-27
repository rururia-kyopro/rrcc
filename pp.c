#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rrcc.h"

typedef struct PPToken PPToken;
PPToken *pptoken;

typedef enum {
    PPTK_NEWLINE,
    PPTK_HEADER_NAME,
    PPTK_RESERVED,
    PPTK_PUNC,
    PPTK_PPNUMBER,
    PPTK_IDENT,
    PPTK_CHAR_CONST,
    PPTK_STRING_LITERAL,
    PPTK_EOF,
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
};

extern PPToken *pptoken;

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
}

static PPToken *new_pptoken(PPTokenKind kind, PPToken *cur, char *str, int len){
    PPToken *tok = calloc(1, sizeof(PPToken));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    tok->prev = cur;
    cur->next = tok;
    return tok;
}


void pp_tokenize() {
    char *p = user_input;
    PPToken head = {};
    PPToken *cur = &head;
    pptoken = cur;

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
            printf("read escape: %d %d\n", *p, '\\');
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
    }
    cur = new_pptoken(PPTK_EOF, cur, p, 1);
}

void pp_dump_token() {
    PPToken *cur = pptoken;
    for(; cur->kind != PPTK_EOF; cur = cur->next) {
        if(cur->kind == PPTK_CHAR_CONST){
            fprintf(stderr, "char: %d\n", cur->val);
        }else{
            fprintf(stderr, "kind: %d '%.*s'\n", cur->kind, cur->len, cur->str);
        }
    }
}

int pp_main(int argc, char **argv) {
    filename = argv[2];
    user_input = read_file(filename);
    char *processed = calloc(1, strlen(user_input) + 1);
    pp_phase2(user_input, processed);
    // printf("%s\n", processed);

    user_input = processed;

    pp_tokenize();
    pp_dump_token();

    return 0;
}

