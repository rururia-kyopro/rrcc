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
int debug_parse = 0;

int main(int argc, char **argv) {
  init_include_pathes();
  for(int i = 1; i < argc; i++) {
      if(strncmp(argv[i], "-I", 2) == 0){ 
          if(strlen(argv[i]) == 2) {
              if(i + 1 >= argc) {
                  fprintf(stderr, "Error no argument for -I");
                  exit(1);
              }
              append_include_pathes(argv[i+1]);
              i += 1;
          } else {
              append_include_pathes(argv[i] + 2);
          }
      }else if(strncmp(argv[i], "-p", 2) == 0){
          return pp_main(argv[i+1]);
      }else if(strncmp(argv[i], "-d", 2) == 0){
          debug_parse = 1;
      }else if(strncmp(argv[i], "-t", 2) == 0){
          pp_debug = 1;
      } else {
          filename = argv[i];
          break;
      }
  }

  if(!filename) {
      fprintf(stderr, "No filename specified");
      exit(1);
  }

  user_input = read_file(filename);
  user_input_len = strlen(user_input);

  bool use_pp = true;
  if(use_pp) {
    user_input = do_pp();
    user_input_len = strlen(user_input);
    // debug_log("Preprocessed:\n%s", user_input);
  }

  token = tokenize(user_input);
  Node *node_trans_unit = translation_unit();

  if(debug_parse) {
      for(int i = 0; i < vector_size(node_trans_unit->trans_unit.decl); i++){
          dumpnodes(vector_get(node_trans_unit->trans_unit.decl, i));
      }
  }

  printf(".intel_syntax noprefix\n");

  init_codegen();
  for(int i = 0; i < vector_size(node_trans_unit->trans_unit.decl); i++){
      gen(vector_get(node_trans_unit->trans_unit.decl, i));
  }

  return 0;
}
