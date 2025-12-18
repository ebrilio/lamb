// ,---@>
//  W-W'
// cc -pedantic -std=c99 -o lamb lamb.c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#if defined(__GNUC__) || defined(__clang__)
//   https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Function-Attributes.html
#    ifdef __MINGW_PRINTF_FORMAT
#        define PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (__MINGW_PRINTF_FORMAT, STRING_INDEX, FIRST_TO_CHECK)))
#    else
#        define PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (printf, STRING_INDEX, FIRST_TO_CHECK)))
#    endif // __MINGW_PRINTF_FORMAT
#else
//   TODO: implement PRINTF_FORMAT for MSVC
#    define PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK)
#endif

#define UNUSED(value) (void)(value)
#define TODO(message) do { fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message); abort(); } while(0)
#define UNREACHABLE(message) do { fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); abort(); } while(0)

#define DA_INIT_CAP 256
#define da_reserve(da, expected_capacity)                                                  \
    do {                                                                                   \
        if ((expected_capacity) > (da)->capacity) {                                        \
            if ((da)->capacity == 0) {                                                     \
                (da)->capacity = DA_INIT_CAP;                                              \
            }                                                                              \
            while ((expected_capacity) > (da)->capacity) {                                 \
                (da)->capacity *= 2;                                                       \
            }                                                                              \
            (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items));     \
            assert((da)->items != NULL && "Buy more RAM lol");                             \
        }                                                                                  \
    } while (0)

#define da_append(da, item)                  \
    do {                                     \
        da_reserve((da), (da)->count + 1);   \
        (da)->items[(da)->count++] = (item); \
    } while (0)

#define sb_append_null(sb) da_append(sb, 0)

typedef struct {
    size_t unwrap;
} Expr_Index;

char *copy_string(const char *s)
{
    int n = strlen(s);
    char *ds = malloc(n + 1);
    assert(ds);
    memcpy(ds, s, n);
    ds[n] = '\0';
    return ds;
}

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} String_Builder;

int sb_appendf(String_Builder *sb, const char *fmt, ...) PRINTF_FORMAT(2, 3);
int sb_appendf(String_Builder *sb, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // NOTE: the new_capacity needs to be +1 because of the null terminator.
    // However, further below we increase sb->count by n, not n + 1.
    // This is because we don't want the sb to include the null terminator. The user can always sb_append_null() if they want it
    da_reserve(sb, sb->count + n + 1);
    char *dest = sb->items + sb->count;
    va_start(args, fmt);
    vsnprintf(dest, n+1, fmt, args);
    va_end(args);

    sb->count += n;

    return n;
}

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
    Expr_Index body;
} Expr_Fun;

struct Expr {
    Expr_Kind kind;
    bool visited;
    bool live;
    union {
        Var_Name var;
        Expr_Fun fun;
        struct {
            Expr_Index lhs;
            Expr_Index rhs;
        } app;
    } as;
};

struct {
    Expr *items;
    size_t count;
    size_t capacity;
} expr_pool = {0};

#define expr_slot(index) (                           \
    expr_pool.items[                                 \
        (assert(index.unwrap < expr_pool.count),     \
         assert(expr_pool.items[index.unwrap].live), \
         index.unwrap)])

#define expr_slot_unsafe(index) (expr_pool.items[index.unwrap])

struct {
    Expr_Index *items;
    size_t count;
    size_t capacity;
} expr_dead_pool = {0};

struct {
    const char **items;
    size_t count;
    size_t capacity;
} strings = {0};

const char *intern(const char *s)
{
    for (size_t i = 0; i < strings.count; ++i) {
        if (strcmp(strings.items[i], s) == 0) {
            return strings.items[i];
        }
    }
    char *result = copy_string(s);
    da_append(&strings, result);
    return result;
}

Expr_Index make_expr(void)
{
    Expr_Index result;
    if (expr_dead_pool.count > 0) {
        result = expr_dead_pool.items[--expr_dead_pool.count];
    } else {
        result.unwrap = expr_pool.count;
        Expr expr = {0};
        da_append(&expr_pool, expr);
    }
    assert(!expr_slot_unsafe(result).live);
    expr_slot_unsafe(result).live = true;
    return result;
}

void kill_expr(Expr_Index expr)
{
    expr_slot(expr).live = false;
    da_append(&expr_dead_pool, expr);
}

Expr_Index var(const char *name)
{
    Expr_Index expr = make_expr();
    expr_slot(expr).kind = EXPR_VAR;
    expr_slot(expr).as.var = var_name_free(intern(name));
    return expr;
}

Expr_Index fun(const char *arg, Expr_Index body)
{
    Expr_Index expr = make_expr();
    expr_slot(expr).kind = EXPR_FUN;
    expr_slot(expr).as.fun.arg = var_name_free(intern(arg));
    expr_slot(expr).as.fun.body = body;
    return expr;
}

Expr_Index fun_bound(Var_Name arg, Expr_Index body)
{
    Expr_Index expr = make_expr();
    expr_slot(expr).kind = EXPR_FUN;
    expr_slot(expr).as.fun.arg = arg;
    expr_slot(expr).as.fun.body = body;
    return expr;
}

Expr_Index app(Expr_Index lhs, Expr_Index rhs)
{
    Expr_Index expr = make_expr();
    expr_slot(expr).kind = EXPR_APP;
    expr_slot(expr).as.app.lhs = lhs;
    expr_slot(expr).as.app.rhs = rhs;
    return expr;
}

void expr_display(Expr_Index expr, String_Builder *sb)
{
    switch (expr_slot(expr).kind) {
    case EXPR_VAR:
        sb_appendf(sb, "%s", expr_slot(expr).as.var.name);
        break;
    case EXPR_FUN:
        sb_appendf(sb, "\\%s.", expr_slot(expr).as.fun.arg.name);
        expr_display(expr_slot(expr).as.fun.body, sb);
        break;
    case EXPR_APP: {
        Expr_Index lhs = expr_slot(expr).as.app.lhs;
        if (expr_slot(lhs).kind != EXPR_VAR) sb_appendf(sb, "(");
        expr_display(lhs, sb);
        if (expr_slot(lhs).kind != EXPR_VAR) sb_appendf(sb, ")");

        sb_appendf(sb, " ");

        Expr_Index rhs = expr_slot(expr).as.app.rhs;
        if (expr_slot(rhs).kind != EXPR_VAR) sb_appendf(sb, "(");
        expr_display(rhs, sb);
        if (expr_slot(rhs).kind != EXPR_VAR) sb_appendf(sb, ")");
    } break;
    default: UNREACHABLE("Expr_Kind");
    }
}

Expr_Index replace(Var_Name arg, Expr_Index body, Expr_Index val)
{
    switch (expr_slot(body).kind) {
    case EXPR_VAR:
        if (expr_slot(body).as.var.name == arg.name && expr_slot(body).as.var.id == arg.id) {
            return val;
        } else {
            return body;
        }
    case EXPR_FUN:
        return fun_bound(
            expr_slot(body).as.fun.arg,
            replace(arg, expr_slot(body).as.fun.body, val));
    case EXPR_APP:
        return app(
            replace(arg, expr_slot(body).as.app.lhs, val),
            replace(arg, expr_slot(body).as.app.rhs, val));
    default: UNREACHABLE("Expr_Kind");
    }
}

Expr_Index apply(Expr_Fun fun, Expr_Index val)
{
    return replace(fun.arg, fun.body, val);
}

Expr_Index eval1(Expr_Index expr)
{
    switch (expr_slot(expr).kind) {
    case EXPR_VAR:
        return expr;
    case EXPR_FUN: {
        Expr_Index body = eval1(expr_slot(expr).as.fun.body);
        if (body.unwrap != expr_slot(expr).as.fun.body.unwrap) {
            return fun_bound(expr_slot(expr).as.fun.arg, body);
        }
        return expr;
    }
    case EXPR_APP: {
        Expr_Index lhs = expr_slot(expr).as.app.lhs;
        Expr_Index rhs = expr_slot(expr).as.app.rhs;

        if (expr_slot(lhs).kind == EXPR_FUN) {
            return apply(expr_slot(lhs).as.fun, rhs);
        }

        Expr_Index new_lhs = eval1(lhs);
        if (lhs.unwrap != new_lhs.unwrap) {
            return app(new_lhs, rhs);
        }

        Expr_Index new_rhs = eval1(rhs);
        if (rhs.unwrap != new_rhs.unwrap) {
            return app(lhs, new_rhs);
        }

        return expr;
    }
    default: UNREACHABLE("Expr_Kind");
    }
}

void trace_expr(Expr_Index expr, String_Builder *sb)
{
    sb->count = 0;
    expr_display(expr, sb);
    sb_append_null(sb);
    printf("%s\n", sb->items);
}

void bind_var(Expr_Index body, Var_Name var)
{
    switch (expr_slot(body).kind) {
    case EXPR_VAR: {
        if (expr_slot(body).as.var.name == var.name) {
            expr_slot(body).as.var.id = var.id;
        }
    } break;
    case EXPR_FUN: {
        bind_var(expr_slot(body).as.fun.body, var);
    } break;
    case EXPR_APP: {
        bind_var(expr_slot(body).as.app.lhs, var);
        bind_var(expr_slot(body).as.app.rhs, var);
    } break;
    default: UNREACHABLE("Expr_Kind");
    }
}

Expr_Index bind_vars(Expr_Index expr)
{
    static size_t id_counter = 1;
    switch (expr_slot(expr).kind) {
    case EXPR_VAR: return expr;
    case EXPR_FUN: {
        assert(expr_slot(expr).as.fun.arg.id == 0);
        expr_slot(expr).as.fun.arg.id = id_counter++;
        bind_var(expr_slot(expr).as.fun.body, expr_slot(expr).as.fun.arg);
        bind_vars(expr_slot(expr).as.fun.body);
        return expr;
    } break;
    case EXPR_APP: {
        bind_vars(expr_slot(expr).as.app.lhs);
        bind_vars(expr_slot(expr).as.app.rhs);
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
    TOKEN_COLON,
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
    case TOKEN_COLON:   return "TOKEN_COLON";
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
    case ':':  l->token = TOKEN_COLON;  return true;
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

bool parse_expr(Lexer *l, Expr_Index *expr);

bool parse_fun(Lexer *l, Expr_Index *expr)
{
    if (!lexer_expect(l, TOKEN_NAME)) return false;
    const char *arg = copy_string(l->name.items);
    if (!lexer_expect(l, TOKEN_DOT)) return false;

    Token_Kind a, b;
    Cur saved = l->cur; {
        if (!lexer_next(l)) return false;
        a = l->token;
        if (!lexer_next(l)) return false;
        b = l->token;
    } l->cur = saved;

    Expr_Index body;
    if (a == TOKEN_NAME && b == TOKEN_DOT) {
        if (!parse_fun(l, &body)) return false;
    } else {
        if (!parse_expr(l, &body)) return false;
    }
    *expr = fun(arg, body);
    return true;
}

bool parse_primary(Lexer *l, Expr_Index *expr)
{
    if (!lexer_next(l)) return NULL;
    switch (l->token) {
    case TOKEN_OPAREN: {
        if (!parse_expr(l, expr)) return false;
        if (!lexer_expect(l, TOKEN_CPAREN)) return false;
        return true;
    }
    case TOKEN_LAMBDA: return parse_fun(l, expr);
    case TOKEN_NAME:
        *expr = var(copy_string(l->name.items));
        return true;
    default:
        lexer_print_loc(l, stderr);
        fprintf(stderr, "ERROR: Unexpected token %s\n", token_kind_display(l->token));
        return false;
    }
}

bool parse_expr(Lexer *l, Expr_Index *expr)
{
    if (!parse_primary(l, expr)) return false;
    if (!lexer_peek(l)) return false;
    while (l->token != TOKEN_CPAREN && l->token != TOKEN_END) {
        Expr_Index rhs;
        if (!parse_primary(l, &rhs)) return false;
        *expr = app(*expr, rhs);
        if (!lexer_peek(l)) return false;
    }
    return true;
}


typedef struct {
    const char *name;
    const char *signature;
    const char *description;
} Command;

typedef struct {
    Command *items;
    size_t count;
    size_t capacity;
} Commands;

bool command(Commands *commands, const char *input, const char *name, const char *signature, const char *description)
{
    Command command = {
        .name        = name,
        .signature   = signature,
        .description = description,
    };
    da_append(commands, command);
    while (*input && *name && *input == *name) {
        input++;
        name++;
    }
    return *input == '\0';
}

void print_available_commands(Commands *commands)
{
    printf("Available commands:\n");
    int max_name_width = 0;
    int max_sig_width = 0;
    for (size_t i = 0; i < commands->count; ++i) {
        Command command = commands->items[i];
        int name_width = strlen(command.name);
        int sig_width  = strlen(command.signature);
        if (name_width > max_name_width) max_name_width = name_width;
        if (sig_width  > max_sig_width)  max_sig_width  = sig_width;
    }
    for (size_t i = 0; i < commands->count; ++i) {
        Command command = commands->items[i];
        printf("  :%-*s %-*s - %s\n",
               max_name_width, command.name,
               max_sig_width,  command.signature,
               command.description);
    }
}

void gc_mark(Expr_Index root)
{
    expr_slot(root).visited = true;
    switch (expr_slot(root).kind) {
    case EXPR_VAR: break;
    case EXPR_FUN:
        gc_mark(expr_slot(root).as.fun.body);
        break;
    case EXPR_APP:
        gc_mark(expr_slot(root).as.app.lhs);
        gc_mark(expr_slot(root).as.app.rhs);
        break;
    }
}

void gc(Expr_Index root)
{
    for (size_t i = 0; i < expr_pool.count; ++i) {
        if (expr_pool.items[i].live) {
            expr_pool.items[i].visited = false;
        }
    }

    gc_mark(root);

    for (size_t i = 0; i < expr_pool.count; ++i) {
        if (expr_pool.items[i].live && !expr_pool.items[i].visited) {
            kill_expr((Expr_Index){i});
        }
    }
}

int main(void)
{
    static char buffer[1024];
    static String_Builder sb = {0};
    static Commands commands = {0};

    size_t limit = 10;
    printf(",---@>\n");
    printf(" W-W'\n");
    printf("Enter :help for more info\n");
    for (;;) {
        printf("@> ");
        fflush(stdout);
        if (!fgets(buffer, sizeof(buffer), stdin)) break;
        const char *source = buffer;
        Lexer l = {
            .content = source,
            .count = strlen(source),
        };

        if (!lexer_peek(&l)) continue;
        if (l.token == TOKEN_COLON) {
            if (!lexer_next(&l)) continue;
            if (!lexer_expect(&l, TOKEN_NAME)) continue;
            commands.count = 0;
            if (command(&commands, l.name.items, "intern", "", "print amount of interned strings")) {
                printf("Interns: %zu\n", strings.count);
                continue;
            }
            if (command(&commands, l.name.items, "limit", "[number]", "change evaluation limit (0 for no limit)")) {
                if (!lexer_peek(&l)) continue;
                switch (l.token) {
                case TOKEN_NAME:
                    if (!lexer_expect(&l, TOKEN_NAME)) continue;
                    limit = strtoul(l.name.items, NULL, 10);
                    if (limit) {
                        printf("Setting evaluation limit to %zu\n", limit);
                    } else {
                        printf("Evaluation limit is disabled\n");
                    }
                    continue;
                case TOKEN_END:
                    if (limit) {
                        printf("Evaluation limit is %zu\n", limit);
                    } else {
                        printf("Evaluation limit is disabled\n");
                    }
                    continue;
                default:
                    lexer_print_loc(&l, stderr);
                    fprintf(stderr, "ERROR: Unexpected token %s\n", token_kind_display(l.token));
                    continue;
                }
            }
            if (command(&commands, l.name.items, "quit", "", "quit the REPL")) break;
            if (command(&commands, l.name.items, "help", "", "print this help message")) {
                print_available_commands(&commands);
                continue;
            }
            print_available_commands(&commands);
            printf("ERROR: unknown command `%s`\n", l.name.items);
            continue;
        }

        Expr_Index expr;
        if (!parse_expr(&l, &expr)) continue;
        bind_vars(expr);

        trace_expr(expr, &sb);
        Expr_Index expr1 = eval1(expr);
        for (size_t i = 1; (limit == 0 || i < limit) && expr1.unwrap != expr.unwrap; ++i) {
            expr = expr1;
            gc(expr);
            trace_expr(expr, &sb);
            expr1 = eval1(expr);
        }
        if (expr1.unwrap != expr.unwrap) {
            printf("...\n");
        }

        expr_pool.count = 0;
        expr_dead_pool.count = 0;
    }

    return 0;
}
// Copyright 2025 Alexey Kutepov <reximkut@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
