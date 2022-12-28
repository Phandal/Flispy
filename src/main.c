#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/mpc.h"

#ifdef _WIN32
#define BUF_SIZE 2048

static char buffer[BUF_SIZE];
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, BUF_SIZE, stdin);
  char* cpy = malloc(strlen(buffer) + 1);
  cpy[strlen(buffer)] = '\0';
  return cpy;
}

void add_history(char* unused){}
#else
#include <editline/readline.h>
#endif

enum {
  LERR_DIV_BY_ZERO,
  LERR_BAD_OP,
  LERR_BAD_NUM,
};

enum { LVAL_NUM, LVAL_ERR };

typedef struct {
  int type;
  long num;
  int err;
} lval_t;

lval_t lval_num(long num) {
  lval_t v;
  v.type = LVAL_NUM;
  v.num = num;
  return v;
}

lval_t lval_err(int err) {
  lval_t v;
  v.type = LVAL_ERR;
  v.err = err;
  return v;
}

void lval_num_print(lval_t t) {
  printf("%li\n", t.num);
}

void lval_err_print(lval_t t) {
  switch(t.err){
    case LERR_DIV_BY_ZERO:
      puts("Error: Divide By Zero!");
      break;
    case LERR_BAD_OP:
      puts("Error: Invalid Operator!");
      break;
    case LERR_BAD_NUM:
      puts("Error: Invalid Number!");
      break;
  }
}

lval_t eval_op(lval_t x, char* op, lval_t y) {
  if(strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if(strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if(strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
  if(strcmp(op, "/") == 0) { return y.num != 0 ? lval_num(x.num / y.num) : lval_err(LERR_DIV_BY_ZERO); }
  if(strcmp(op, "%") == 0) { return lval_num(x.num % y.num); }
  if(strcmp(op, "^") == 0) { return lval_num(pow(x.num, y.num)); }
  return lval_err(LERR_BAD_OP);
}

lval_t eval(mpc_ast_t* t) {
  // Base Case
  if (strstr(t->tag, "number")) {
    return lval_num(atoi(t->contents));
  }

  // Recursive Case
  char* op = t->children[1]->contents;

  lval_t x = eval(t->children[2]);

  int i = 3;
  while(strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    if (x.type == LVAL_ERR) { return x; }
    ++i;
  }

  return lval_num(x.num);
}

int main(void) {
  char* input;
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Flispy = mpc_new("flispy");
  mpc_result_t r;

  mpca_lang(MPCA_LANG_DEFAULT,
      "\
        number : /-?[0-9]+/ ; \
        operator : '+' | '-' | '*' | '/' | '%' | '^' ; \
        expr : <number> | '(' <operator> <expr>+ ')' ;\
        flispy: /^/ <operator> <expr>+ /$/;\
      ",
      Number, Operator, Expr, Flispy);

  puts("Flispy Version 0.0.0.1");
  puts("Press Ctrl+c to exit\n");

  while(1) {
    input = readline("flispy> ");
    add_history(input);

    if(mpc_parse("<stdin>", input, Flispy, &r)) {
      // mpc_ast_print(r.output);
      lval_t result = eval(r.output);
      if (result.type == LVAL_NUM) {
        lval_num_print(result);
      } else {
        lval_err_print(result);
      }
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

  }
    free(input);
    mpc_cleanup(4, Number, Operator, Expr, Flispy);
    return 0;
}
