#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "9cc.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  user_input = argv[1];
  token = tokenize(user_input);
  translation_unit();

  for(int i = 0; code[i]; i++){
      dumpnodes(code[i]);
  }

  printf(".intel_syntax noprefix\n");

  gen_string_literals();
  for(int i = 0; code[i]; i++){
      gen(code[i]);
  }

  return 0;
}
