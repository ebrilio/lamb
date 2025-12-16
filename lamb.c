// cc -o lamb lamb.c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

// Y = \f.(\x.(f (x x))) (\x.(f (x x)))

typedef enum {
    EXPR_VAR,
    EXPR_FUN,
    EXPR_APP,
} Expr_Kind;

typedef struct Expr Expr;

typedef struct {
    const char *name;
    size_t id;
} Var_Name;

Var_Name var_name_bound(const char *name, size_t id)
{
    Var_Name var = {
        .name = name,
        .id = id,
    };
    return var;
}

Var_Name var_name_free(const char *name)
{
    return var_name_bound(name, 0);
}

typedef struct {
    Var_Name arg;
    Expr *body;
} Expr_Fun;

struct Expr {
    Expr_Kind kind;
    union {
        Var_Name var;
        Expr_Fun fun;
        struct {
            Expr *lhs;
            Expr *rhs;
        } app;
    } as;
};

Expr *var(const char *name)
{
    Expr *expr = malloc(sizeof(*expr));
    assert(expr != NULL);
    expr->kind = EXPR_VAR;
    expr->as.var = var_name_free(name);
    return expr;
}

Expr *fun(const char *arg, Expr *body)
{
    Expr *expr = malloc(sizeof(*expr));
    assert(expr != NULL);
    expr->kind = EXPR_FUN;
    expr->as.fun.arg = var_name_free(arg);
    expr->as.fun.body = body;
    return expr;
}

Expr *fun_bound(Var_Name arg, Expr *body)
{
    Expr *expr = malloc(sizeof(*expr));
    assert(expr != NULL);
    expr->kind = EXPR_FUN;
    expr->as.fun.arg = arg;
    expr->as.fun.body = body;
    return expr;
}

Expr *app(Expr *lhs, Expr *rhs)
{
    Expr *expr = malloc(sizeof(*expr));
    assert(expr != NULL);
    expr->kind = EXPR_APP;
    expr->as.app.lhs = lhs;
    expr->as.app.rhs = rhs;
    return expr;
}

void expr_display(Expr *expr, String_Builder *sb)
{
    switch (expr->kind) {
    case EXPR_VAR:
        sb_appendf(sb, "%s", expr->as.var.name);
        break;
    case EXPR_FUN:
        sb_appendf(sb, "\\%s.", expr->as.fun.arg.name);
        expr_display(expr->as.fun.body, sb);
        sb_appendf(sb, "");
        break;
    case EXPR_APP:
        if (expr->as.app.lhs->kind != EXPR_VAR) sb_appendf(sb, "(");
        expr_display(expr->as.app.lhs, sb);
        if (expr->as.app.lhs->kind != EXPR_VAR) sb_appendf(sb, ")");
        sb_appendf(sb, " ");
        if (expr->as.app.rhs->kind != EXPR_VAR) sb_appendf(sb, "(");
        expr_display(expr->as.app.rhs, sb);
        if (expr->as.app.rhs->kind != EXPR_VAR) sb_appendf(sb, ")");
        break;
    default: UNREACHABLE("Expr_Kind");
    }
}

Expr *replace(Var_Name arg, Expr *body, Expr *val)
{
    switch (body->kind) {
    case EXPR_VAR:
        if (strcmp(body->as.var.name, arg.name) == 0 && body->as.var.id == arg.id) {
            return val;
        } else {
            return body;
        }
    case EXPR_FUN:
        return fun_bound(
            body->as.fun.arg,
            replace(arg, body->as.fun.body, val));
    case EXPR_APP:
        return app(
            replace(arg, body->as.app.lhs, val),
            replace(arg, body->as.app.rhs, val));
    default: UNREACHABLE("Expr_Kind");
    }
}

Expr *apply(Expr_Fun fun, Expr *val)
{
    return replace(fun.arg, fun.body, val);
}

Expr *eval1(Expr *expr)
{
    switch (expr->kind) {
    case EXPR_VAR:
        return expr;
    case EXPR_FUN: {
        Expr *body = eval1(expr->as.fun.body);
        if (body != expr->as.fun.body) {
            return fun_bound(expr->as.fun.arg, body);
        }
        return expr;
    }
    case EXPR_APP:
        if (expr->as.app.lhs->kind == EXPR_FUN) {
            return apply(expr->as.app.lhs->as.fun, expr->as.app.rhs);
        }

        Expr *lhs = eval1(expr->as.app.lhs);
        if (lhs != expr->as.app.lhs) {
            return app(lhs, expr->as.app.rhs);
        }

        Expr *rhs = eval1(expr->as.app.rhs);
        if (rhs != expr->as.app.rhs) {
            return app(lhs, rhs);
        }

        return expr;
    default: UNREACHABLE("Expr_Kind");
    }
}

void trace_expr(Expr *expr, String_Builder *sb)
{
    sb->count = 0;
    expr_display(expr, sb);
    sb_append_null(sb);
    printf("%s\n", sb->items);
}

void bind_var(Expr *body, Var_Name var)
{
    switch (body->kind) {
    case EXPR_VAR: {
        if (strcmp(body->as.var.name, var.name) == 0) {
            body->as.var.id = var.id;
        }
    } break;
    case EXPR_FUN: {
        bind_var(body->as.fun.body, var);
    } break;
    case EXPR_APP: {
        bind_var(body->as.app.lhs, var);
        bind_var(body->as.app.rhs, var);
    } break;
    default: UNREACHABLE("Expr_Kind");
    }
}

Expr *bind_vars(Expr *expr)
{
    static size_t id_counter = 1;
    switch (expr->kind) {
    case EXPR_VAR: return expr;
    case EXPR_FUN: {
        assert(expr->as.fun.arg.id == 0);
        expr->as.fun.arg.id = id_counter++;
        bind_var(expr->as.fun.body, expr->as.fun.arg);
        bind_vars(expr->as.fun.body);
        return expr;
    } break;
    case EXPR_APP: {
        bind_vars(expr->as.app.lhs);
        bind_vars(expr->as.app.rhs);
        return expr;
    } break;
    default: UNREACHABLE("Expr_Kind");
    }
}

typedef enum {
    TOKEN_INVALID,
    TOKEN_END,
    TOKEN_OPAREN,
    TOKEN_CPAREN,
    TOKEN_LAMBDA,
    TOKEN_DOT,
    TOKEN_NAME,
} Token_Kind;

const char *token_kind_display(Token_Kind kind)
{
    switch (kind) {
    case TOKEN_INVALID: return "TOKEN_INVALID";
    case TOKEN_END:     return "TOKEN_END";
    case TOKEN_OPAREN:  return "TOKEN_OPAREN";
    case TOKEN_CPAREN:  return "TOKEN_CPAREN";
    case TOKEN_LAMBDA:  return "TOKEN_LAMBDA";
    case TOKEN_DOT:     return "TOKEN_DOT";
    case TOKEN_NAME:    return "TOKEN_NAME";
    default: UNREACHABLE("Token_Kind");
    }
}

typedef struct {
    size_t pos, bol, row;
} Cur;

typedef struct {
    const char *content;
    size_t count;
    const char *file_path;

    Cur cur;

    Token_Kind token;
    String_Builder name;
    size_t row, col;
} Lexer;

void lexer_print_loc(Lexer *l, FILE *stream)
{
    if (l->file_path) fprintf(stream, "%s:", l->file_path);
    fprintf(stream, "%zu:%zu: ", l->row, l->col);
}

char lexer_curr_char(Lexer *l)
{
    if (l->cur.pos >= l->count) return 0;
    return l->content[l->cur.pos];
}

char lexer_next_char(Lexer *l)
{
    if (l->cur.pos >= l->count) return 0;
    char x = l->content[l->cur.pos++];
    if (x == '\n') {
        l->cur.row += 1;
        l->cur.bol = l->cur.pos;
    }
    return x;
}

bool lexer_next(Lexer *l)
{
    while (isspace(lexer_curr_char(l))) {
        lexer_next_char(l);
    }

    l->row = l->cur.row + 1;
    l->col = l->cur.pos - l->cur.bol + 1;

    char x = lexer_next_char(l);
    if (x == '\0') {
        l->token = TOKEN_END;
        return true;
    }

    switch (x) {
    case '(':  l->token = TOKEN_OPAREN; return true;
    case ')':  l->token = TOKEN_CPAREN; return true;
    case '\\': l->token = TOKEN_LAMBDA; return true;
    case '.':  l->token = TOKEN_DOT;    return true;
    }

    if (isalnum(x)) {
        l->token = TOKEN_NAME;
        l->name.count = 0;
        da_append(&l->name, x);
        while (isalnum(lexer_curr_char(l))) {
            x = lexer_next_char(l);
            da_append(&l->name, x);
        }
        sb_append_null(&l->name);
        return true;
    }

    l->token = TOKEN_INVALID;
    lexer_print_loc(l, stderr);
    fprintf(stderr, "ERROR: Unknown token starts with `%c`\n", x);
    return false;
}

bool lexer_peek(Lexer *l)
{
    Cur saved = l->cur;
    bool result = lexer_next(l);
    l->cur = saved;
    return result;
}

bool lexer_expect(Lexer *l, Token_Kind expected)
{
    if (!lexer_next(l)) return false;
    if (l->token != expected) {
        lexer_print_loc(l, stderr);
        fprintf(stderr, "ERROR: Unexpected token %s\n", token_kind_display(l->token));
        return false;
    }
    return true;
}

Expr *parse_expr(Lexer *l);

Expr *parse_fun(Lexer *l)
{
    if (!lexer_expect(l, TOKEN_NAME)) return NULL;
    const char *arg = strdup(l->name.items);
    if (!lexer_expect(l, TOKEN_DOT)) return NULL;
    Expr *body = parse_expr(l);
    if (body == NULL) return NULL;
    return fun(arg, body);
}

Expr *parse_primary(Lexer *l)
{
    if (!lexer_next(l)) return NULL;
    switch (l->token) {
    case TOKEN_OPAREN: {
        Expr *expr = parse_expr(l);
        if (!expr) return NULL;
        if (!lexer_expect(l, TOKEN_CPAREN)) return NULL;
        return expr;
    }
    case TOKEN_LAMBDA: return parse_fun(l);
    case TOKEN_NAME: return var(strdup(l->name.items));
    default:
        lexer_print_loc(l, stderr);
        fprintf(stderr, "ERROR: Unexpected token %s\n", token_kind_display(l->token));
        return NULL;
    }
}

Expr *parse_expr(Lexer *l)
{
    Expr *lhs = parse_primary(l);
    if (!lhs) return NULL;
    if (!lexer_peek(l)) return NULL;
    while (l->token != TOKEN_CPAREN && l->token != TOKEN_END) {
        Expr *rhs = parse_primary(l);
        if (!rhs) return NULL;
        lhs = app(lhs, rhs);
        if (!lexer_peek(l)) return NULL;
    }
    return lhs;
}

char buffer[1024];

int main()
{
    String_Builder sb = {0};

    for (;;) {
        printf("𝛌> ");
        fflush(stdout);
        if (!fgets(buffer, sizeof(buffer), stdin)) break;
        const char *source = buffer;
        Lexer l = {
            .content = source,
            .count = strlen(source),
        };
        Expr *expr = parse_expr(&l);
        if (!expr) continue;
        bind_vars(expr);

        trace_expr(expr, &sb);
        Expr *expr1 = eval1(expr);
        for (size_t i = 1; i < 10 && expr1 != expr; ++i) {
            expr = expr1;
            trace_expr(expr, &sb);
            expr1 = eval1(expr);
        }
        if (expr1 != expr) {
            printf("...\n");
        }
    }

    return 0;
}
