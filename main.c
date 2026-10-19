#include "ast.h"
#include "runtime.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern FILE *yyin;               // Flex input file pointer
extern int yyparse(void);        // Bison parse entry point
extern ASTNode *ast_root;        // Root node populated by parser.y
extern SymbolTable symbol_table;

typedef struct {
  const char *input_path;
  int debug;
} Options;

static int parse_arguments(int argc, char **argv, Options *options) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      options->debug = 1;
      continue;
    }
    if (argv[i][0] == '-' || options->input_path != NULL) {
      return 0;
    }
    options->input_path = argv[i];
  }
  return 1;
}

static int read_input(const char *input_path) {
  if (input_path != NULL) {
    FILE *file = fopen(input_path, "r");
    if (!file) {
      perror("Error opening the file");
      return -1;
    }
    yyin = file;
  } else {
    yyin = stdin;
    printf("Reading from standard input (Ctrl+D to end)...\n");
  }
  return 0;
}

static void print_usage(const char *program_name) {
  fprintf(stderr, "Usage: %s [-d] [input-file]\n", program_name);
}

static void cleanup_lexer_input(void) {
  /* Clean up: close yyin only if it was opened from a file */
  if (yyin && yyin != stdin) {
    fclose(yyin);
    yyin = NULL;
  }
}

int main(int argc, char **argv) {
  Options options = {0};
  if (!parse_arguments(argc, argv, &options)) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  symbol_table_init(&symbol_table);
  if (read_input(options.input_path) != 0) {
    symbol_table_destroy(&symbol_table);
    return EXIT_FAILURE;
  }

  /* Run the Bison parser */
  int parse_result = yyparse();

  /* Close input stream as soon as parsing completes */
  cleanup_lexer_input();

  if (parse_result == 0) {
    if (options.debug) {
      printf("=== AST ===\n");
      print_ast(ast_root);
      printf("\n=== Global Symbol Table ===\n");
      symbol_table_print(&symbol_table);
      printf("\n");
    }

    int execution_succeeded = execute_program(ast_root);

    /* Clean up the AST heap allocations */
    free_ast(ast_root);
    symbol_table_destroy(&symbol_table);
    return execution_succeeded ? EXIT_SUCCESS : EXIT_FAILURE;
  } else {
    fprintf(stderr, "Parsing failed due to syntax errors.\n");
    free_ast(ast_root);
    symbol_table_destroy(&symbol_table);
    return EXIT_FAILURE;
  }
}
