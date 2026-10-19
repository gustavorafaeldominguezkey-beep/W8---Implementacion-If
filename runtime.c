#include "runtime.h"
#include "string_utils.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  DataType type;
  union {
    int int_value;
    float float_value;
    char char_value;
    int bool_value;
    const char *string_value;
  } data;
} RuntimeValue;

typedef struct RuntimeVariable {
  const char *name;
  RuntimeValue value;
  int initialized;
  struct RuntimeVariable *next;
} RuntimeVariable;

static RuntimeVariable *find_variable(RuntimeVariable *variables,
                                       const char *name) {
  for (; variables != NULL; variables = variables->next) {
    if (strcmp(variables->name, name) == 0) return variables;
  }
  return NULL;
}

static const ASTNode *find_function(const ASTNode *program, const char *name) {
  for (; program != NULL; program = program->next) {
    if (program->type == FUNCTION_DECL &&
        strcmp(program->data.functionDeclaration.identifier, name) == 0) {
      return program;
    }
  }
  return NULL;
}

static void release_variables_until(RuntimeVariable **variables,
                                    RuntimeVariable *boundary) {
  /* Preserve the caller's frame while discarding a function's locals. */
  while (*variables != boundary) {
    RuntimeVariable *next = (*variables)->next;
    free(*variables);
    *variables = next;
  }
}

static RuntimeVariable *create_runtime_variable(const char *name,
                                                 RuntimeValue value,
                                                 int initialized,
                                                 RuntimeVariable *next) {
  RuntimeVariable *variable = allocate_memory(sizeof(*variable));
  if (variable == NULL) {
    fprintf(stderr, "Out of memory while creating runtime variable\n");
    return NULL;
  }
  variable->name = name;
  variable->value = value;
  variable->initialized = initialized;
  variable->next = next;
  return variable;
}

static int convert_value(RuntimeValue input, DataType target,
                         RuntimeValue *output) {
  output->type = target;
  switch (target) {
  case TYPE_INT:
    if (input.type == TYPE_INT) {
      output->data.int_value = input.data.int_value;
    } else if (input.type == TYPE_FLOAT &&
               (double)input.data.float_value >= INT_MIN &&
               (double)input.data.float_value <= INT_MAX) {
      output->data.int_value = (int)input.data.float_value;
    } else {
      return 0;
    }
    return 1;
  case TYPE_FLOAT:
    if (input.type == TYPE_FLOAT) {
      output->data.float_value = input.data.float_value;
    } else if (input.type == TYPE_INT) {
      output->data.float_value = (float)input.data.int_value;
    } else {
      return 0;
    }
    return 1;
  case TYPE_CHAR:
    if (input.type == TYPE_CHAR) {
      output->data.char_value = input.data.char_value;
    } else if (input.type == TYPE_INT && input.data.int_value >= CHAR_MIN &&
               input.data.int_value <= CHAR_MAX) {
      output->data.char_value = (char)input.data.int_value;
    } else {
      return 0;
    }
    return 1;
  case TYPE_BOOL:
    if (input.type != TYPE_BOOL) return 0;
    output->data.bool_value = input.data.bool_value;
    return 1;
  case TYPE_STRING:
    if (input.type != TYPE_STRING) return 0;
    output->data.string_value = input.data.string_value;
    return 1;
  case TYPE_VOID:
  case TYPE_UNKNOWN:
    return 0;
  }
  return 0;
}

static void print_value(RuntimeValue value) {
  switch (value.type) {
  case TYPE_INT: printf("%d\n", value.data.int_value); break;
  case TYPE_FLOAT: printf("%g\n", value.data.float_value); break;
  case TYPE_CHAR: printf("%c\n", value.data.char_value); break;
  case TYPE_BOOL: printf("%s\n", value.data.bool_value ? "true" : "false"); break;
  case TYPE_STRING: printf("%s\n", value.data.string_value); break;
  case TYPE_VOID:
  case TYPE_UNKNOWN: break;
  }
}

static int evaluate(const ASTNode *expression, RuntimeVariable *variables,
                    const ASTNode *program, RuntimeValue *result);

static int declare_variable(const ASTNode *statement,
                            RuntimeVariable **variables,
                            const ASTNode *program) {
  const VarDeclaration *declaration = &statement->data.varDeclaration;
  RuntimeValue value = {0};
  int initialized = 0;
  if (declaration->init != NULL) {
    if (!evaluate(declaration->init, *variables, program, &value) ||
        !convert_value(value, declaration->dataType, &value)) {
      fprintf(stderr, "Initializer type does not match variable '%s'\n",
              declaration->identifier);
      return 0;
    }
    initialized = 1;
  }
  RuntimeVariable *variable = create_runtime_variable(
      declaration->identifier, value, initialized, *variables);
  if (variable == NULL) return 0;
  *variables = variable;
  return 1;
}

static int execute_function(const ASTNode *function, const ASTNode *arguments,
                            RuntimeVariable *caller_variables,
                            const ASTNode *program, RuntimeValue *result);

static int execute_statement(const ASTNode *statement,
                             RuntimeVariable **variables,
                             const ASTNode *program);

static int evaluate_condition(const ASTNode *condition,
                              RuntimeVariable *variables,
                              const ASTNode *program, int *is_true) {
  RuntimeValue value;
  if (!evaluate(condition, variables, program, &value)) return 0;
  if (value.type != TYPE_BOOL) {
    fprintf(stderr, "If condition must evaluate to bool\n");
    return 0;
  }
  *is_true = value.data.bool_value;
  return 1;
}

static int execute_block(const ASTNode *body, RuntimeVariable **variables,
                         const ASTNode *program) {
  RuntimeVariable *boundary = *variables;
  for (const ASTNode *statement = body; statement != NULL;
       statement = statement->next) {
    if (!execute_statement(statement, variables, program)) {
      release_variables_until(variables, boundary);
      return 0;
    }
  }
  release_variables_until(variables, boundary);
  return 1;
}

static int execute_statement(const ASTNode *statement,
                             RuntimeVariable **variables,
                             const ASTNode *program) {
  RuntimeValue value;
  switch (statement->type) {
  case VAR_DECL:
  case CONST_DECL:
    return declare_variable(statement, variables, program);
  case PRINT_STMT:
    if (!evaluate(statement->data.print.value, *variables, program, &value)) {
      return 0;
    }
    print_value(value);
    return 1;
  case FUNCTION_CALL:
    return evaluate(statement, *variables, program, &value);
  case FUNCTION_DECL:
    return 1;
  case IF_STMT: {
    int is_true;
    if (!evaluate_condition(statement->data.ifStatement.condition, *variables,
                            program, &is_true)) {
      return 0;
    }
    const ASTNode *body = is_true ? statement->data.ifStatement.thenBody
                                  : statement->data.ifStatement.elseBody;
    return body == NULL || execute_block(body, variables, program);
  }
  default:
    fprintf(stderr, "Invalid statement node\n");
    return 0;
  }
}

static int bind_parameters(const FunctionDeclaration *function,
                           const ASTNode *arguments,
                           RuntimeVariable *caller_variables,
                           RuntimeVariable **variables,
                           const ASTNode *program) {
  const FunctionParameter *parameter = function->parameters;
  const ASTNode *argument = arguments;
  while (parameter != NULL && argument != NULL) {
    RuntimeValue value;
    /* Arguments run in the caller's scope, not the partially built callee frame. */
    if (!evaluate(argument, caller_variables, program, &value) ||
        !convert_value(value, parameter->dataType, &value)) {
      fprintf(stderr, "Argument type does not match parameter '%s'\n",
              parameter->identifier);
      return 0;
    }
    RuntimeVariable *variable = create_runtime_variable(
        parameter->identifier, value, 1, *variables);
    if (variable == NULL) return 0;
    *variables = variable;
    parameter = parameter->next;
    argument = argument->next;
  }
  if (parameter != NULL || argument != NULL) {
    fprintf(stderr, "Wrong argument count for function '%s'\n", function->identifier);
    return 0;
  }
  return 1;
}

static int execute_return(const ASTNode *statement,
                          const FunctionDeclaration *function,
                          RuntimeVariable *variables, const ASTNode *program,
                          RuntimeValue *result) {
  if (statement->data.returnStatement.value == NULL) {
    if (function->returnType == TYPE_VOID) {
      result->type = TYPE_VOID;
      return 1;
    }
    fprintf(stderr, "Function '%s' must return a value\n", function->identifier);
    return 0;
  }

  RuntimeValue value;
  if (function->returnType == TYPE_VOID ||
      !evaluate(statement->data.returnStatement.value, variables, program, &value) ||
      !convert_value(value, function->returnType, result)) {
    fprintf(stderr, "Invalid return value for function '%s'\n", function->identifier);
    return 0;
  }
  return 1;
}

//Execute body until have a return statement
static int execute_function_body(const FunctionDeclaration *declaration,
                                 RuntimeVariable **variables,
                                 const ASTNode *program,
                                 RuntimeValue *result) {
  for (const ASTNode *statement = declaration->body; statement != NULL;
       statement = statement->next) {
    if (statement->type == RETURN_STMT) {
      return execute_return(statement, declaration, *variables, program, result);
    }
    if (!execute_statement(statement, variables, program)) return 0;
  }
  if (declaration->returnType == TYPE_VOID) {
    result->type = TYPE_VOID;
    return 1;
  }
  fprintf(stderr, "Function '%s' has no return value\n", declaration->identifier);
  return 0;
}

static int execute_function(const ASTNode *function, const ASTNode *arguments,
                            RuntimeVariable *caller_variables,
                            const ASTNode *program, RuntimeValue *result) {
  const FunctionDeclaration *declaration = &function->data.functionDeclaration;
  RuntimeVariable *variables = caller_variables;
  int succeeded = bind_parameters(declaration, arguments, caller_variables,
                                  &variables, program) &&
                  execute_function_body(declaration, &variables, program, result);
  /* Every path out of a call must release its parameters and local declarations. */
  release_variables_until(&variables, caller_variables);
  return succeeded;
}

static int apply_float_operation(BinaryOperator op, float left, float right,
                                 RuntimeValue *result) {
  if (op == OP_DIV && right == 0.0f) {
    fprintf(stderr, "Division by zero\n");
    return 0;
  }
  result->type = TYPE_FLOAT;
  switch (op) {
  case OP_ADD: result->data.float_value = left + right; break;
  case OP_SUB: result->data.float_value = left - right; break;
  case OP_MUL: result->data.float_value = left * right; break;
  case OP_DIV: result->data.float_value = left / right; break;
  default: return 0;
  }
  return 1;
}

static int apply_integer_operation(BinaryOperator op, int left, int right,
                                   RuntimeValue *result) {
  if (op == OP_DIV && right == 0) {
    fprintf(stderr, "Division by zero\n");
    return 0;
  }
  long long value;
  switch (op) {
  case OP_ADD: value = (long long)left + right; break;
  case OP_SUB: value = (long long)left - right; break;
  case OP_MUL: value = (long long)left * right; break;
  case OP_DIV: value = (long long)left / right; break;
  default: return 0;
  }
  if (value < INT_MIN || value > INT_MAX) {
    fprintf(stderr, "Integer arithmetic overflow\n");
    return 0;
  }
  result->type = TYPE_INT;
  result->data.int_value = (int)value;
  return 1;
}

static int is_numeric_type(DataType type) {
  return type == TYPE_INT || type == TYPE_FLOAT;
}

static double numeric_value(RuntimeValue value) {
  return value.type == TYPE_INT ? (double)value.data.int_value
                                : (double)value.data.float_value;
}

static int compare_numeric_values(BinaryOperator op, double left, double right,
                                  int *comparison) {
  switch (op) {
  case OP_EQ: *comparison = left == right; return 1;
  case OP_NE: *comparison = left != right; return 1;
  case OP_GT: *comparison = left > right; return 1;
  case OP_LT: *comparison = left < right; return 1;
  case OP_GE: *comparison = left >= right; return 1;
  case OP_LE: *comparison = left <= right; return 1;
  default: return 0;
  }
}

static int apply_comparison(BinaryOperator op, RuntimeValue left,
                            RuntimeValue right, RuntimeValue *result) {
  int comparison = 0;
  if (is_numeric_type(left.type) && is_numeric_type(right.type)) {
    if (!compare_numeric_values(op, numeric_value(left), numeric_value(right),
                                &comparison)) {
      return 0;
    }
  } else {
    if (left.type != right.type || (op != OP_EQ && op != OP_NE)) {
      fprintf(stderr, "Comparison requires compatible operands\n");
      return 0;
    }
    switch (left.type) {
    case TYPE_BOOL:
      comparison = left.data.bool_value == right.data.bool_value;
      break;
    case TYPE_CHAR:
      comparison = left.data.char_value == right.data.char_value;
      break;
    case TYPE_STRING:
      comparison = strcmp(left.data.string_value, right.data.string_value) == 0;
      break;
    case TYPE_INT:
    case TYPE_FLOAT:
    case TYPE_VOID:
    case TYPE_UNKNOWN:
      fprintf(stderr, "Comparison requires compatible operands\n");
      return 0;
    }
    if (op == OP_NE) comparison = !comparison;
  }
  result->type = TYPE_BOOL;
  result->data.bool_value = comparison;
  return 1;
}

static int apply_binary_operation(BinaryOperator op, RuntimeValue left,
                                  RuntimeValue right, RuntimeValue *result) {
  switch (op) {
  case OP_OR:
    if (left.type != TYPE_BOOL || right.type != TYPE_BOOL) {
      fprintf(stderr, "or requires bool operands\n");
      return 0;
    }
    result->type = TYPE_BOOL;
    result->data.bool_value = left.data.bool_value || right.data.bool_value;
    return 1;
  case OP_EQ:
  case OP_NE:
  case OP_GT:
  case OP_LT:
  case OP_GE:
  case OP_LE:
    return apply_comparison(op, left, right, result);
  case OP_ADD:
  case OP_SUB:
  case OP_MUL:
  case OP_DIV:
    if (!is_numeric_type(left.type) || !is_numeric_type(right.type)) {
      fprintf(stderr, "Arithmetic requires numeric operands\n");
      return 0;
    }
    if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
      return apply_float_operation(op, (float)numeric_value(left),
                                   (float)numeric_value(right), result);
    }
    return apply_integer_operation(op, left.data.int_value,
                                   right.data.int_value, result);
  }
  return 0;
}

static int evaluate(const ASTNode *expression, RuntimeVariable *variables,
                    const ASTNode *program, RuntimeValue *result) {
  switch (expression->type) {
  case INT_LITERAL:
    result->type = TYPE_INT;
    result->data.int_value = expression->data.intValue;
    return 1;
  case FLOAT_LITERAL:
    result->type = TYPE_FLOAT;
    result->data.float_value = expression->data.floatValue;
    return 1;
  case BOOL_LITERAL:
    result->type = TYPE_BOOL;
    result->data.bool_value = expression->data.boolValue;
    return 1;
  case CHAR_LITERAL:
    result->type = TYPE_CHAR;
    result->data.char_value = expression->data.charValue;
    return 1;
  case STR_LITERAL:
    result->type = TYPE_STRING;
    result->data.string_value = expression->data.strValue;
    return 1;
  case IDENTIFIER_REF: {
    RuntimeVariable *variable = find_variable(variables, expression->data.identifier);
    if (variable == NULL || !variable->initialized) {
      fprintf(stderr, "Variable '%s' has no runtime value\n",
              expression->data.identifier);
      return 0;
    }
    *result = variable->value;
    return 1;
  }
  case FUNCTION_CALL: {
    const ASTNode *function =
        find_function(program, expression->data.functionCall.identifier);
    if (function == NULL) {
      fprintf(stderr, "Unknown function '%s'\n",
              expression->data.functionCall.identifier);
      return 0;
    }
    return execute_function(function, expression->data.functionCall.arguments,
                            variables, program, result);
  }
  case BINARY_OP: {
    RuntimeValue left;
    RuntimeValue right;
    if (!evaluate(expression->data.binary.left, variables, program, &left)) {
      return 0;
    }
    if (expression->data.binary.op == OP_OR) {
      if (left.type != TYPE_BOOL) {
        fprintf(stderr, "or requires bool operands\n");
        return 0;
      }
      if (left.data.bool_value) {
        result->type = TYPE_BOOL;
        result->data.bool_value = 1;
        return 1;
      }
    }
    return evaluate(expression->data.binary.right, variables, program, &right) &&
           apply_binary_operation(expression->data.binary.op, left, right, result);
  }
  default:
    fprintf(stderr, "Invalid expression node\n");
    return 0;
  }
}

int execute_program(const ASTNode *program) {
  RuntimeVariable *variables = NULL;
  int succeeded = 1;
  for (const ASTNode *statement = program; statement != NULL; statement = statement->next) {
    if (!execute_statement(statement, &variables, program)) {
      succeeded = 0;
      break;
    }
  }
  release_variables_until(&variables, NULL);
  return succeeded;
}
