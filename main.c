#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rrcc.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

char *filename;

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }
  if(strcmp(argv[1], "-p") == 0){
      return pp_main(argc, argv);
  }

  filename = argv[1];
  user_input = read_file(argv[1]);
  token = tokenize(user_input);
  Node *node_trans_unit = translation_unit();

  for(int i = 0; i < vector_size(node_trans_unit->trans_unit.decl); i++){
      dumpnodes(vector_get(node_trans_unit->trans_unit.decl, i));
  }

  printf(".intel_syntax noprefix\n");

  gen_string_literals();
  for(int i = 0; i < vector_size(node_trans_unit->trans_unit.decl); i++){
      gen(vector_get(node_trans_unit->trans_unit.decl, i));
  }

  return 0;
}
