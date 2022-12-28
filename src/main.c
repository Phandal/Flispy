#include <errno.h>
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

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

typedef struct lval {
  int type;
  long num;
  char* err;
  char* sym;
  int count;
  struct lval** cell;
} lval_t;

lval_t* lval_num(long x) {
  lval_t* v = malloc(sizeof(lval_t));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval_t* lval_err(char* m) {
  lval_t* v = malloc(sizeof(lval_t));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

lval_t* lval_sym(char* s) {
  lval_t* v = malloc(sizeof(lval_t));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval_t* lval_sexpr(void) {
  lval_t* v = malloc(sizeof(lval_t));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval_t* v) {
  switch(v->type) {
    case LVAL_NUM:
      break;
    case LVAL_ERR:
      free(v->err);
      break;
    case LVAL_SYM:
      free(v->sym);
      break;
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; ++i) {
        lval_del(v->cell[i]);
      }
      free(v->cell);
      break;
  }
   free(v);
}

void lval_print(lval_t t) {
  if (t.type == LVAL_NUM) {
    printf("%li\n", t.num);
  } else {
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
}

lval_t eval_op(lval_t x, char* op, lval_t y) {
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

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
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  // Recursive Case
  char* op = t->children[1]->contents;

  lval_t x = eval(t->children[2]);

  int i = 3;
  while(strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    ++i;
  }

  return x;
}

int main(void) {
  char* input;
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Flispy = mpc_new("flispy");
  mpc_result_t r;

  mpca_lang(MPCA_LANG_DEFAULT,
      "\
        number : /-?[0-9]+/ ; \
        symbol : '+' | '-' | '*' | '/' | '%' | '^' ; \
        sexpr : '(' <expr>* ')' ; \
        expr : <number> | <symbol> | <sexpr> ;\
        flispy: /^/ <expr>* /$/;\
      ",
      Number, Symbol, Sexpr, Expr, Flispy);

  puts("Flispy Version 0.0.0.1");
  puts("Press Ctrl+c to exit\n");

  while(1) {
    input = readline("flispy> ");
    add_history(input);

    if(mpc_parse("<stdin>", input, Flispy, &r)) {
      // mpc_ast_print(r.output);
      lval_t result = eval(r.output);
      lval_print(result);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

  }
    free(input);
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Flispy);
    return 0;
}
