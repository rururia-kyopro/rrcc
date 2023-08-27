#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "rrcc.h"

// 指定されたファイルの内容を返す
char *read_file(char *path) {
  // ファイルを開く
  FILE *fp = fopen(path, "r");
  if (!fp)
    error("cannot open %s: %s", path, strerror(errno));

  // ファイルの長さを調べる
  if (fseek(fp, 0, SEEK_END) == -1)
    error("%s: fseek: %s", path, strerror(errno));
  size_t size = ftell(fp);
  if (fseek(fp, 0, SEEK_SET) == -1)
    error("%s: fseek: %s", path, strerror(errno));

  // ファイル内容を読み込む
  char *buf = calloc(1, size + 2);
  fread(buf, size, 1, fp);

  // ファイルが必ず"\n\0"で終わっているようにする
  if (size == 0 || buf[size - 1] != '\n')
    buf[size++] = '\n';
  buf[size] = '\0';
  fclose(fp);
  return buf;
}

static bool is_octdigit(char c) {
    return c >= '0' && c <= '7';
}

static int octdigit2i(char c) {
    if(c >= '0' && c <= '7') {
        return c - '0';
    }
    return -1;
}

static bool is_hexdigit(char c) {
    return c >= '0' && c <= '9' || c >= 'A' && c <= 'F' || c >= 'a' && c <= 'f';
}

static int hexdigit2i(char c) {
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'A' && c <= 'F'){
        return c - 'A' + 10;
    }
    if(c >= 'a' && c <= 'f'){
        return c - 'a' + 10;
    }
    return -1;
}

char read_escape(char **p) {
    if(**p == '"' || **p == '\\' || **p == '?' || **p == '\'') {
        return **p;
    }else if(**p == 'a') {
        return '\a';
    }else if(**p == 'b') {
        return '\b';
    }else if(**p == 'f') {
        return '\f';
    }else if(**p == 'n') {
        return '\n';
    }else if(**p == 'r') {
        return '\r';
    }else if(**p == 't') {
        return '\t';
    }else if(**p == 'v') {
        return '\v';
    }else if(is_octdigit(**p)) {
        long oct = 0;
        while(is_octdigit(**p)) {
            oct *= 8;
            oct += octdigit2i(**p);
            (*p)++;
        }
        (*p)--;
        return oct;
    }else if(**p == 'x') {
        (*p)++;
        long hex = 0;
        while(is_hexdigit(**p)) {
            hex *= 16;
            hex += hexdigit2i(**p);
            (*p)++;
        }
        (*p)--;
        return hex;
    }
    error("Invalid escape sequence");
    return 0;
}

