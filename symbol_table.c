#include "symbol_table.h"
#include "string_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void symbol_table_init(SymbolTable *table) {
  table->head = NULL;
  table->scope = 0;
}

void symbol_table_enter_scope(SymbolTable *table) {
  table->scope++;
}

void symbol_table_exit_scope(SymbolTable *table) {
  Symbol **symbol = &table->head;
  while (*symbol != NULL) {
    if ((*symbol)->scope == table->scope) {
      Symbol *removed = *symbol;
      *symbol = removed->next;
      free(removed->name);
      free(removed);
    } else {
      symbol = &(*symbol)->next;
    }
  }
  table->scope--;
}

const Symbol *symbol_table_lookup(const SymbolTable *table, const char *name) {
  for (const Symbol *symbol = table->head; symbol != NULL;
       symbol = symbol->next) {
    if (strcmp(symbol->name, name) == 0) {
      return symbol;
    }
  }
  return NULL;
}

int symbol_table_insert(SymbolTable *table, const char *name, DataType type) {
  for (const Symbol *symbol = table->head; symbol != NULL;
       symbol = symbol->next) {
    if (symbol->scope == table->scope && strcmp(symbol->name, name) == 0) {
      return 0;
    }
  }

  Symbol *symbol = allocate_memory(sizeof(*symbol));
  if (symbol == NULL) {
    return -1;
  }
  symbol->name = string_copy(name);
  if (symbol->name == NULL) {
    free(symbol);
    return -1;
  }
  symbol->type = type;
  symbol->scope = table->scope;
  symbol->next = table->head;
  table->head = symbol;
  return 1;
}

int symbol_table_add_declaration(SymbolTable *table,
                                 const ASTNode *declaration) {
  const VarDeclaration *var = &declaration->data.varDeclaration;
  int result = symbol_table_insert(
      table, var->identifier, var->dataType);
  if (result == 0) {
    fprintf(stderr, "Duplicate declaration: %s\n", var->identifier);
  } else if (result < 0) {
    fprintf(stderr, "Out of memory while adding symbol\n");
  }
  return result;
}

int symbol_table_add_function(SymbolTable *table, const ASTNode *function) {
  const FunctionDeclaration *declaration = &function->data.functionDeclaration;
  int result = symbol_table_insert(
      table, declaration->identifier, declaration->returnType);
  if (result == 0) {
    fprintf(stderr, "Duplicate declaration: %s\n", declaration->identifier);
  } else if (result < 0) {
    fprintf(stderr, "Out of memory while adding symbol\n");
  }
  return result;
}

void symbol_table_print(const SymbolTable *table) {
  printf("Name                 Type       Scope\n");
  printf("-------------------------------------\n");
  for (const Symbol *symbol = table->head; symbol != NULL;
       symbol = symbol->next) {
    printf("%-20s %-10s %u\n", symbol->name, data_type_name(symbol->type),
           symbol->scope);
  }
}

void symbol_table_destroy(SymbolTable *table) {
  Symbol *symbol = table->head;
  while (symbol != NULL) {
    Symbol *next = symbol->next;
    free(symbol->name);
    free(symbol);
    symbol = next;
  }
  table->head = NULL;
}
