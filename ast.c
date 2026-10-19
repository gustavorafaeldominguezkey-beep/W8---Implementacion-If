#include "ast.h"
#include "string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ASTNode *allocate_new_node(void) {
  ASTNode *node = allocate_memory(sizeof(*node));

  if (node == NULL) {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }
  return node;
}

ASTNode *create_int_node(int val) {
  ASTNode *node = allocate_new_node();
  node->type = INT_LITERAL;
  node->expressionType = TYPE_INT;
  node->data.intValue = val;
  node->next = NULL;

  return node;
}

ASTNode *create_float_node(float val) {
  ASTNode *node = allocate_new_node();
  node->type = FLOAT_LITERAL;
  node->expressionType = TYPE_FLOAT;
  node->data.floatValue = val;
  node->next = NULL;

  return node;
}

ASTNode *create_bool_node(int val) {
  ASTNode *node = allocate_new_node();
  node->type = BOOL_LITERAL;
  node->expressionType = TYPE_BOOL;
  node->data.boolValue = val;
  node->next = NULL;

  return node;
}

ASTNode *create_char_node(char val) {
  ASTNode *node = allocate_new_node();
  node->type = CHAR_LITERAL;
  node->expressionType = TYPE_CHAR;
  node->data.charValue = val;
  node->next = NULL;

  return node;
}

ASTNode *create_str_node(const char *val) {
  ASTNode *node = allocate_new_node();

  node->type = STR_LITERAL;
  node->expressionType = TYPE_STRING;
  node->data.strValue = string_copy(val);
  if (node->data.strValue == NULL) {
    fprintf(stderr, "Out of memory\n");
    free(node);
    exit(EXIT_FAILURE);
  }

  return node;
}

ASTNode *create_identifier_node(const char *name, DataType type) {
  ASTNode *node = allocate_new_node();
  node->type = IDENTIFIER_REF;
  node->expressionType = type;
  node->data.identifier = string_copy(name);
  if (node->data.identifier == NULL) {
    fprintf(stderr, "Out of memory\n");
    free(node);
    exit(EXIT_FAILURE);
  }
  return node;
}

ASTNode *create_binary_node(BinaryOperator op, ASTNode *left, ASTNode *right) {
  ASTNode *node = allocate_new_node();
  node->type = BINARY_OP;
  node->expressionType =
      op == OP_EQ || op == OP_NE || op == OP_GT || op == OP_LT ||
              op == OP_GE || op == OP_LE || op == OP_OR
          ? TYPE_BOOL
          : (left->expressionType == TYPE_FLOAT ||
                     right->expressionType == TYPE_FLOAT
                 ? TYPE_FLOAT
                 : TYPE_INT);
  node->data.binary.op = op;
  node->data.binary.left = left;
  node->data.binary.right = right;
  return node;
}

ASTNode *create_print_node(ASTNode *value) {
  ASTNode *node = allocate_new_node();
  node->type = PRINT_STMT;
  node->data.print.value = value;
  return node;
}

ASTNode *create_var_decl(DataType data_type, const char *name, ASTNode *init) {
  ASTNode *node = allocate_new_node();
  node->type = VAR_DECL;
  node->data.varDeclaration.dataType = data_type;
  node->data.varDeclaration.identifier = string_copy(name);
  if (node->data.varDeclaration.identifier == NULL) {
    fprintf(stderr, "Out of memory\n");
    free(node);
    exit(EXIT_FAILURE);
  }
  node->data.varDeclaration.init = init;
  return node;
}

ASTNode *create_const_decl(DataType data_type, const char *name, ASTNode *init) {
  ASTNode *node = create_var_decl(data_type, name, init);
  node->type = CONST_DECL;
  return node;
}

FunctionParameter *create_function_parameter(DataType data_type, const char *name) {
  FunctionParameter *parameter = allocate_memory(sizeof(*parameter));
  if (parameter == NULL) {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }
  parameter->dataType = data_type;
  parameter->identifier = string_copy(name);
  if (parameter->identifier == NULL) {
    fprintf(stderr, "Out of memory\n");
    free(parameter);
    exit(EXIT_FAILURE);
  }
  return parameter;
}

ASTNode *create_function_decl(DataType return_type, const char *name,
                              FunctionParameter *parameters, ASTNode *body) {
  ASTNode *node = allocate_new_node();
  node->type = FUNCTION_DECL;
  node->data.functionDeclaration.returnType = return_type;
  node->data.functionDeclaration.identifier = string_copy(name);
  if (node->data.functionDeclaration.identifier == NULL) {
    fprintf(stderr, "Out of memory\n");
    free(node);
    exit(EXIT_FAILURE);
  }
  node->data.functionDeclaration.parameters = parameters;
  node->data.functionDeclaration.body = body;
  return node;
}

ASTNode *create_function_call(const char *name, ASTNode *arguments) {
  ASTNode *node = allocate_new_node();
  node->type = FUNCTION_CALL;
  node->expressionType = TYPE_UNKNOWN;
  node->data.functionCall.identifier = string_copy(name);
  if (node->data.functionCall.identifier == NULL) {
    fprintf(stderr, "Out of memory\n");
    free(node);
    exit(EXIT_FAILURE);
  }
  node->data.functionCall.arguments = arguments;
  return node;
}

ASTNode *create_return_node(ASTNode *value) {
  ASTNode *node = allocate_new_node();
  node->type = RETURN_STMT;
  node->data.returnStatement.value = value;
  return node;
}

ASTNode *create_if_node(ASTNode *condition, ASTNode *then_body,
                        ASTNode *else_body) {
  ASTNode *node = allocate_new_node();
  node->type = IF_STMT;
  node->data.ifStatement.condition = condition;
  node->data.ifStatement.thenBody = then_body;
  node->data.ifStatement.elseBody = else_body;
  return node;
}

DataType parse_string_data_type(const char *value) {
  if (value == NULL) {
    return TYPE_UNKNOWN;
  } else if (strcmp(value, "int") == 0) {
    return TYPE_INT;
  } else if (strcmp(value, "float") == 0) {
    return TYPE_FLOAT;
  } else if (strcmp(value, "char") == 0) {
    return TYPE_CHAR;
  } else if (strcmp(value, "bool") == 0) {
    return TYPE_BOOL;
  } else if (strcmp(value, "str") == 0 || strcmp(value, "string") == 0) {
    return TYPE_STRING;
  } else if (strcmp(value, "void") == 0) {
    return TYPE_VOID;
  }
  return TYPE_UNKNOWN;
}

const char *data_type_name(DataType type) {
  switch (type) {
  case TYPE_INT: return "int";
  case TYPE_FLOAT: return "float";
  case TYPE_CHAR: return "char";
  case TYPE_BOOL: return "bool";
  case TYPE_STRING: return "string";
  case TYPE_VOID: return "void";
  case TYPE_UNKNOWN: return "unknown";
  }
  return "unknown";
}

const char *binary_operator_name(BinaryOperator op) {
  switch (op) {
  case OP_ADD: return "+";
  case OP_SUB: return "-";
  case OP_MUL: return "*";
  case OP_DIV: return "/";
  case OP_EQ: return "==";
  case OP_NE: return "!=";
  case OP_GT: return ">";
  case OP_LT: return "<";
  case OP_GE: return ">=";
  case OP_LE: return "<=";
  case OP_OR: return "or";
  }
  return "?";
}

static void print_indent(unsigned int depth) {
  for (unsigned int index = 0; index < depth; index++) printf("  ");
}

static void print_ast_list(const ASTNode *node, unsigned int depth);

static void print_ast_node(const ASTNode *node, unsigned int depth) {
  print_indent(depth);
  switch (node->type) {
  case VAR_DECL:
  case CONST_DECL:
    printf("%s %s: %s\n", node->type == CONST_DECL ? "Const" : "Variable",
           node->data.varDeclaration.identifier,
           data_type_name(node->data.varDeclaration.dataType));
    if (node->data.varDeclaration.init != NULL) {
      print_indent(depth + 1);
      printf("Initializer\n");
      print_ast_list(node->data.varDeclaration.init, depth + 2);
    }
    break;
  case FUNCTION_DECL: {
    const FunctionDeclaration *function = &node->data.functionDeclaration;
    printf("Function %s -> %s\n", function->identifier,
           data_type_name(function->returnType));
    print_indent(depth + 1);
    printf("Parameters\n");
    if (function->parameters == NULL) {
      print_indent(depth + 2);
      printf("<none>\n");
    }
    for (const FunctionParameter *parameter = function->parameters;
         parameter != NULL; parameter = parameter->next) {
      print_indent(depth + 2);
      printf("%s: %s\n", parameter->identifier,
             data_type_name(parameter->dataType));
    }
    print_indent(depth + 1);
    printf("Body\n");
    if (function->body == NULL) {
      print_indent(depth + 2);
      printf("<empty>\n");
    } else {
      print_ast_list(function->body, depth + 2);
    }
    break;
  }
  case RETURN_STMT:
    printf("Return\n");
    if (node->data.returnStatement.value != NULL) {
      print_ast_list(node->data.returnStatement.value, depth + 1);
    }
    break;
  case IF_STMT:
    printf("If\n");
    print_indent(depth + 1);
    printf("Condition\n");
    print_ast_list(node->data.ifStatement.condition, depth + 2);
    print_indent(depth + 1);
    printf("Then\n");
    if (node->data.ifStatement.thenBody == NULL) {
      print_indent(depth + 2);
      printf("<empty>\n");
    } else {
      print_ast_list(node->data.ifStatement.thenBody, depth + 2);
    }
    if (node->data.ifStatement.elseBody != NULL) {
      print_indent(depth + 1);
      printf("Else\n");
      print_ast_list(node->data.ifStatement.elseBody, depth + 2);
    }
    break;
  case PRINT_STMT:
    printf("Print\n");
    print_ast_list(node->data.print.value, depth + 1);
    break;
  case FUNCTION_CALL:
    printf("Call %s\n", node->data.functionCall.identifier);
    print_indent(depth + 1);
    printf("Arguments\n");
    if (node->data.functionCall.arguments == NULL) {
      print_indent(depth + 2);
      printf("<none>\n");
    } else {
      print_ast_list(node->data.functionCall.arguments, depth + 2);
    }
    break;
  case BINARY_OP:
    printf("Binary %s\n", binary_operator_name(node->data.binary.op));
    print_indent(depth + 1);
    printf("Left\n");
    print_ast_list(node->data.binary.left, depth + 2);
    print_indent(depth + 1);
    printf("Right\n");
    print_ast_list(node->data.binary.right, depth + 2);
    break;
  case IDENTIFIER_REF:
    printf("Identifier %s\n", node->data.identifier);
    break;
  case INT_LITERAL:
    printf("Integer %d\n", node->data.intValue);
    break;
  case FLOAT_LITERAL:
    printf("Float %g\n", node->data.floatValue);
    break;
  case BOOL_LITERAL:
    printf("Boolean %s\n", node->data.boolValue ? "true" : "false");
    break;
  case CHAR_LITERAL:
    printf("Character '%c'\n", node->data.charValue);
    break;
  case STR_LITERAL:
    printf("String \"%s\"\n", node->data.strValue);
    break;
  }
}

static void print_ast_list(const ASTNode *node, unsigned int depth) {
  for (; node != NULL; node = node->next) print_ast_node(node, depth);
}

void print_ast(ASTNode *root) { print_ast_list(root, 0); }

void free_ast(ASTNode *node) {
  while (node != NULL) {
    /* Save next statement before freeing current node */
    ASTNode *next_node = node->next;

    switch (node->type) {
    case VAR_DECL:
    case CONST_DECL:
      if (node->data.varDeclaration.identifier) {
        free(node->data.varDeclaration.identifier);
      }
      /* Recursively free the initialization expression subtree */
      if (node->data.varDeclaration.init) {
        free_ast(node->data.varDeclaration.init);
      }
      break;

    case FUNCTION_DECL: {
      FunctionDeclaration *function = &node->data.functionDeclaration;
      free(function->identifier);
      free_ast(function->body);
      FunctionParameter *parameter = function->parameters;
      while (parameter != NULL) {
        FunctionParameter *next_parameter = parameter->next;
        free(parameter->identifier);
        free(parameter);
        parameter = next_parameter;
      }
      break;
    }

    case STR_LITERAL:
      /* Free the duplicated string literal */
      if (node->data.strValue) {
        free(node->data.strValue);
      }
      break;

    case IDENTIFIER_REF:
      free(node->data.identifier);
      break;

    case FUNCTION_CALL:
      free(node->data.functionCall.identifier);
      free_ast(node->data.functionCall.arguments);
      break;

    case BINARY_OP:
      free_ast(node->data.binary.left);
      free_ast(node->data.binary.right);
      break;

    case PRINT_STMT:
      free_ast(node->data.print.value);
      break;

    case RETURN_STMT:
      free_ast(node->data.returnStatement.value);
      break;

    case IF_STMT:
      free_ast(node->data.ifStatement.condition);
      free_ast(node->data.ifStatement.thenBody);
      free_ast(node->data.ifStatement.elseBody);
      break;

    case INT_LITERAL:
    case FLOAT_LITERAL:
    case BOOL_LITERAL:
    case CHAR_LITERAL:
      /* No dynamically allocated fields in these literals. */
      break;
    }

    /* Free the ASTNode struct itself */
    free(node);

    /* Advance to the next statement in the linked list */
    node = next_node;
  }
}
