%{
#include "ast.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>

extern int yylex();
void yyerror(const char *s);

ASTNode *ast_root = NULL;
SymbolTable symbol_table;

static ASTNode *create_numeric_binary_node(BinaryOperator op, ASTNode *left,
                                           ASTNode *right) {
    if ((left->expressionType != TYPE_INT && left->expressionType != TYPE_FLOAT) ||
        (right->expressionType != TYPE_INT && right->expressionType != TYPE_FLOAT)) {
        fprintf(stderr, "%s requires numeric operands\n", binary_operator_name(op));
        free_ast(left);
        free_ast(right);
        return NULL;
    }
    return create_binary_node(op, left, right);
}

static int is_numeric_type(DataType type) {
    return type == TYPE_INT || type == TYPE_FLOAT;
}

static int is_relational_operator(BinaryOperator op) {
    return op == OP_GT || op == OP_LT || op == OP_GE || op == OP_LE;
}

static ASTNode *create_comparison_node(BinaryOperator op, ASTNode *left,
                                       ASTNode *right) {
    int both_numeric = is_numeric_type(left->expressionType) &&
                       is_numeric_type(right->expressionType);
    int compatible = left->expressionType == right->expressionType &&
                     left->expressionType != TYPE_VOID &&
                     left->expressionType != TYPE_UNKNOWN;

    if ((is_relational_operator(op) && !both_numeric) ||
        (!is_relational_operator(op) && !both_numeric && !compatible)) {
        fprintf(stderr, "%s requires compatible operands\n",
                binary_operator_name(op));
        free_ast(left);
        free_ast(right);
        return NULL;
    }
    return create_binary_node(op, left, right);
}

static ASTNode *create_or_node(ASTNode *left, ASTNode *right) {
    if (left->expressionType != TYPE_BOOL ||
        right->expressionType != TYPE_BOOL) {
        fprintf(stderr, "or requires bool operands\n");
        free_ast(left);
        free_ast(right);
        return NULL;
    }
    return create_binary_node(OP_OR, left, right);
}

static ASTNode *require_boolean_condition(ASTNode *condition) {
    if (condition->expressionType == TYPE_BOOL) return condition;
    fprintf(stderr, "If condition requires a bool expression\n");
    free_ast(condition);
    return NULL;
}

static DataType parse_type_token(char *name) {
    DataType type = parse_string_data_type(name);
    free(name);
    return type;
}

static ASTNode *create_registered_declaration(DataType type, char *name,
                                               ASTNode *initializer, int is_const) {
    ASTNode *declaration = is_const
        ? create_const_decl(type, name, initializer)
        : create_var_decl(type, name, initializer);
    free(name);
    if (symbol_table_add_declaration(&symbol_table, declaration) == 1) {
        return declaration;
    }
    free_ast(declaration);
    return NULL;
}

static ASTNode *create_identifier_reference(char *name) {
    const Symbol *symbol = symbol_table_lookup(&symbol_table, name);
    if (symbol == NULL) {
        fprintf(stderr, "Undeclared identifier: %s\n", name);
        free(name);
        return NULL;
    }
    ASTNode *reference = create_identifier_node(name, symbol->type);
    free(name);
    return reference;
}

static ASTNode *create_typed_function_call(char *name, ASTNode *arguments) {
    ASTNode *call = create_function_call(name, arguments);
    const Symbol *symbol = symbol_table_lookup(&symbol_table, name);
    if (symbol != NULL) call->expressionType = symbol->type;
    free(name);
    return call;
}

static int enter_function_scope(FunctionParameter *parameters) {
    symbol_table_enter_scope(&symbol_table);
    for (FunctionParameter *parameter = parameters; parameter != NULL;
         parameter = parameter->next) {
        if (symbol_table_insert(&symbol_table, parameter->identifier,
                                parameter->dataType) != 1) {
            fprintf(stderr, "Duplicate parameter: %s\n", parameter->identifier);
            symbol_table_exit_scope(&symbol_table);
            return 0;
        }
    }
    return 1;
}

static ASTNode *create_registered_function(DataType return_type, char *name,
                                           FunctionParameter *parameters,
                                           ASTNode *body) {
    ASTNode *function = create_function_decl(return_type, name, parameters, body);
    free(name);
    symbol_table_exit_scope(&symbol_table);
    if (symbol_table_add_function(&symbol_table, function) == 1) return function;
    free_ast(function);
    return NULL;
}

%}

%union {
    int int_val;
    char *str_val;
    char char_val;
    float float_value;
    int bool_value; 
    struct ASTNode *node;
    struct FunctionParameter *parameter;
    DataType data_type;
}

%token TOKEN_VAR TOKEN_CONST TOKEN_PRINT TOKEN_PRINTV TOKEN_FUNC TOKEN_RETURN TOKEN_VOID TOKEN_IF TOKEN_ELSE
%token TOKEN_EQUALS TOKEN_NOT_EQUALS TOKEN_GREATER TOKEN_LESS TOKEN_GREATER_EQUAL TOKEN_LESS_EQUAL TOKEN_OR
%token <str_val> TOKEN_TYPE
%token <str_val> TOKEN_IDENTIFIER
%token <str_val> TOKEN_STRING_LITERAL
%token <int_val> TOKEN_INT_LITERAL
%token <float_value> TOKEN_FLOAT_LITERAL
%token <bool_value> TOKEN_BOOL_LITERAL
%token <char_val> TOKEN_CHAR_LITERAL

%left TOKEN_OR
%nonassoc TOKEN_EQUALS TOKEN_NOT_EQUALS TOKEN_GREATER TOKEN_LESS TOKEN_GREATER_EQUAL TOKEN_LESS_EQUAL
%left '+' '-'
%left '*' '/'

//any of these non-terminal rules produce a result
%type <node> program statement_list statement var_declaration if_statement block block_statement_list optional_else_block
%type <node> function_declaration function_statement_list function_statement return_statement function_call_statement print_statement expression argument_list optional_arguments
%type <parameter> parameter parameter_list optional_parameters
%type <data_type> non_void_type return_type

%%

program:
    statement_list {
        ast_root = $1;
    }
    ;

statement_list:
    statement {
        $$ = $1;
    }
    | statement statement_list {
        $1->next = $2;
        $$ = $1;
    }
    ;

statement:
    var_declaration ';' {
        $$ = $1;
    }
    | print_statement {
        $$ = $1;
    }
    | function_declaration {
        $$ = $1;
    }
    | function_call_statement {
        $$ = $1;
    }
    | if_statement {
        $$ = $1;
    }
    ;

print_statement:
    TOKEN_PRINT TOKEN_STRING_LITERAL ';' {
        ASTNode *value = create_str_node($2);
        free($2);
        $$ = create_print_node(value);
    }
    | TOKEN_PRINTV TOKEN_IDENTIFIER ';' {
        ASTNode *value = create_identifier_reference($2);
        if (value == NULL) YYERROR;
        $$ = create_print_node(value);
    }
    ;

var_declaration:
    TOKEN_VAR ':' non_void_type TOKEN_IDENTIFIER '=' expression {
        $$ = create_registered_declaration($3, $4, $6, 0);
        if ($$ == NULL) YYERROR;
    }
    | TOKEN_VAR ':' non_void_type TOKEN_IDENTIFIER {
        $$ = create_registered_declaration($3, $4, NULL, 0);
        if ($$ == NULL) YYERROR;
    }
    | TOKEN_CONST ':' non_void_type TOKEN_IDENTIFIER '=' expression {
        $$ = create_registered_declaration($3, $4, $6, 1);
        if ($$ == NULL) YYERROR;
    }
    ;

non_void_type:
    TOKEN_TYPE {
        $$ = parse_type_token($1);
    }
    ;

return_type:
    non_void_type { $$ = $1; }
    | TOKEN_VOID { $$ = parse_string_data_type("void"); }
    ;

function_declaration:
    TOKEN_FUNC TOKEN_IDENTIFIER '(' optional_parameters ')' ':' return_type '{' {
        if (!enter_function_scope($4)) YYERROR;
    } function_statement_list '}' {
        $$ = create_registered_function($7, $2, $4, $10);
        if ($$ == NULL) YYERROR;
    }
    ;

function_statement_list:
    { $$ = NULL; }
    | function_statement function_statement_list {
        $1->next = $2;
        $$ = $1;
    }
    ;

function_statement:
    var_declaration ';' { $$ = $1; }
    | print_statement { $$ = $1; }
    | return_statement { $$ = $1; }
    | function_call_statement { $$ = $1; }
    | if_statement { $$ = $1; }
    ;

if_statement:
    TOKEN_IF expression ':' block optional_else_block {
        ASTNode *condition = require_boolean_condition($2);
        if (condition == NULL) YYERROR;
        $$ = create_if_node(condition, $4, $5);
    }
    ;

block:
    '{' {
        symbol_table_enter_scope(&symbol_table);
    } block_statement_list '}' {
        symbol_table_exit_scope(&symbol_table);
        $$ = $3;
    }
    ;

optional_else_block:
    { $$ = NULL; }
    | TOKEN_ELSE block { $$ = $2; }
    ;

block_statement_list:
    { $$ = NULL; }
    | statement block_statement_list {
        $1->next = $2;
        $$ = $1;
    }
    ;

return_statement:
    TOKEN_RETURN expression ';' { $$ = create_return_node($2); }
    | TOKEN_RETURN ';' { $$ = create_return_node(NULL); }
    ;

function_call_statement:
    TOKEN_IDENTIFIER '(' optional_arguments ')' ';' {
        $$ = create_typed_function_call($1, $3);
    }
    ;

optional_parameters:
    { $$ = NULL; }
    | parameter_list { $$ = $1; }
    ;

parameter_list:
    parameter { $$ = $1; }
    | parameter ',' parameter_list {
        $1->next = $3;
        $$ = $1;
    }
    ;

parameter:
    TOKEN_IDENTIFIER ':' non_void_type {
        $$ = create_function_parameter($3, $1);
        free($1);
    }
    ;

optional_arguments:
    { $$ = NULL; }
    | argument_list { $$ = $1; }
    ;

argument_list:
    expression { $$ = $1; }
    | expression ',' argument_list {
        $1->next = $3;
        $$ = $1;
    }
    ;

expression:
    TOKEN_INT_LITERAL {
        $$ = create_int_node($1);
    }
    | TOKEN_STRING_LITERAL {
        $$ = create_str_node($1);
        free($1);
    }
    | TOKEN_FLOAT_LITERAL { $$ = create_float_node($1); }
    | TOKEN_BOOL_LITERAL { $$ = create_bool_node($1); }
    | TOKEN_CHAR_LITERAL { $$ = create_char_node($1); }
    | TOKEN_IDENTIFIER {
        $$ = create_identifier_reference($1);
        if ($$ == NULL) YYERROR;
    }
    | TOKEN_IDENTIFIER '(' optional_arguments ')' {
        $$ = create_typed_function_call($1, $3);
    }
    | '(' expression ')' { $$ = $2; }
    | expression '+' expression {
        $$ = create_numeric_binary_node(OP_ADD, $1, $3);
        if ($$ == NULL) YYERROR;
    }
    | expression '-' expression {
        $$ = create_numeric_binary_node(OP_SUB, $1, $3);
        if ($$ == NULL) YYERROR;
    }
    | expression '*' expression {
        $$ = create_numeric_binary_node(OP_MUL, $1, $3);
        if ($$ == NULL) YYERROR;
    }
    | expression '/' expression {
        $$ = create_numeric_binary_node(OP_DIV, $1, $3);
        if ($$ == NULL) YYERROR;
    }
    | expression TOKEN_EQUALS expression {
        $$ = create_comparison_node(OP_EQ, $1, $3);
        if ($$ == NULL) YYERROR;
    }
    | expression TOKEN_NOT_EQUALS expression {
        $$ = create_comparison_node(OP_NE, $1, $3);
        if ($$ == NULL) YYERROR;
    }
    | expression TOKEN_GREATER expression {
        $$ = create_comparison_node(OP_GT, $1, $3);
        if ($$ == NULL) YYERROR;
    }
    | expression TOKEN_LESS expression {
        $$ = create_comparison_node(OP_LT, $1, $3);
        if ($$ == NULL) YYERROR;
    }
    | expression TOKEN_GREATER_EQUAL expression {
        $$ = create_comparison_node(OP_GE, $1, $3);
        if ($$ == NULL) YYERROR;
    }
    | expression TOKEN_LESS_EQUAL expression {
        $$ = create_comparison_node(OP_LE, $1, $3);
        if ($$ == NULL) YYERROR;
    }
    | expression TOKEN_OR expression {
        $$ = create_or_node($1, $3);
        if ($$ == NULL) YYERROR;
    }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
}
