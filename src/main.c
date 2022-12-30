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

// Forward Declarations
void lval_print(lval_t *t);
lval_t* lval_eval_sexpr(lval_t* x);

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

lval_t* lval_add(lval_t* v, lval_t* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval_t*) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval_t* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval_t* lval_read(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  lval_t* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }

  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

void lval_expr_print(lval_t* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; ++i) {
    lval_print(v->cell[i]);

    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval_t* t) {
  switch(t->type) {
    case LVAL_NUM: printf("%li", t->num); break;
    case LVAL_ERR: printf("Error: %s", t->err); break;
    case LVAL_SYM: printf("%s", t->sym); break;
    case LVAL_SEXPR: lval_expr_print(t, '(', ')'); break;
  }
}

void lval_println(lval_t* t) {
  lval_print(t);
  putchar('\n');
}

lval_t* lval_pop(lval_t* x, int i) {
  lval_t* p = x->cell[i];

  // Shift memory at item "i" over the top
  memmove(&x->cell[i], &x->cell[i + 1], sizeof(lval_t*) * x->count - i - 1);

  x->count--;

  x->cell = realloc(x->cell, sizeof(lval_t*) * x->count);
  return p;
}

lval_t* lval_take(lval_t* x, int i) {
  lval_t* p = lval_pop(x, i);
  lval_del(x);
  return p;
}

lval_t* builtin_op(lval_t* x, char* op) {
  // Ensure all args are numbers
  for (int i = 0; i < x->count; i++) {
    if (x->cell[i]->type != LVAL_NUM) {
      lval_del(x);
      return lval_err("Cannot operate on a non-number");
    }
  }

  lval_t* f = lval_pop(x, 0);

  // If there is only one argument then return the inverse version of it
  if ((strcmp(op, "-") == 0) && x->count == 0) {
    f->num = -f->num;
  }

  // While there are still arguments left perform the operation;
  while(x->count > 0) {
    lval_t* s = lval_pop(x, 0);

    if (strcmp(op, "+") == 0) { f->num = f->num + s->num; }
    if (strcmp(op, "-") == 0) { f->num = f->num - s->num; }
    if (strcmp(op, "*") == 0) { f->num = f->num * s->num; }
    if (strcmp(op, "%") == 0) { f->num = f->num % s->num; }
    if (strcmp(op, "^") == 0) { f-> num = pow(f->num, s->num); }
    if (strcmp(op, "/") == 0) {
      if (s->num == 0) {
        lval_del(f);
        lval_del(s);
        f =  lval_err("Division by Zero!");
        break;
      } else {
        f-> num = f->num / s->num;
      }
    }
    lval_del(s);
  }
  lval_del(x);
  return f;
}

lval_t* lval_eval(lval_t* x) {
  if (x->type == LVAL_SEXPR) { return lval_eval_sexpr(x); }
  return x;
}

lval_t* lval_eval_sexpr(lval_t* x) {

  // Eval the children first
  for (int i = 0; i < x->count; i++) {
    x->cell[i] = lval_eval(x->cell[i]);
  }

  for (int i = 0; i < x->count; i++) {
    if (x->cell[i]->type == LVAL_ERR) { return lval_take(x, i); }
  }

  // Empty expression
  if (x->count == 0) { return x; }

  // Single expression
  if (x->count == 1) { return lval_take(x, 0); }

  // Ensure first element is symbol
  lval_t* f = lval_pop(x, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(x);
    return lval_err("S-expression does not start with symbol!");
  }

  lval_t* result = builtin_op(x, f->sym);
  return result;
  
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
      lval_t* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);
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
