#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"

typedef struct {
    Tok *toks;
    size_t len;
    size_t i;
    const char *path;
    Arena *arena;
    Diag *err;
    bool ok;
} Parser;

typedef struct {
    void **data;
    size_t len;
    size_t cap;
} PtrVec;

static void parser_set_error(Parser *p, Tok *t, const char *fmt, ...) {
    if (!p || !p->ok) {
        return;
    }
    p->ok = false;
    if (!p->err) {
        return;
    }
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    // Copy path to arena so it persists after parse_cask returns
    if (p->path && p->path[0]) {
        size_t path_len = strlen(p->path);
        char *path_copy = (char *)arena_alloc(p->arena, path_len + 1);
        if (path_copy) {
            memcpy(path_copy, p->path, path_len + 1);
            p->err->path = path_copy;
        } else {
            p->err->path = p->path;  // fallback to original if alloc fails
        }
    } else {
        p->err->path = p->path;
    }
    
    p->err->line = t ? t->line : 0;
    p->err->col = t ? t->col : 0;
    size_t len = strlen(buf);
    char *msg = (char *)arena_alloc(p->arena, len + 1);
    if (!msg) {
        p->err->message = "parse error";
        return;
    }
    memcpy(msg, buf, len + 1);
    p->err->message = msg;
}

static void parser_set_oom(Parser *p) {
    if (!p || !p->ok) {
        return;
    }
    p->ok = false;
    if (p->err) {
        p->err->path = p->path;
        p->err->line = 0;
        p->err->col = 0;
        p->err->message = "out of memory";
    }
}

static bool ptrvec_push(Parser *p, PtrVec *v, void *item) {
    if (v->len + 1 > v->cap) {
        size_t next = v->cap ? v->cap * 2 : 8;
        void **data = (void **)arena_alloc(p->arena, next * sizeof(void *));
        if (!data) {
            parser_set_oom(p);
            return false;
        }
        if (v->len > 0) memcpy(data, v->data, sizeof(void *) * v->len);
        v->data = data;
        v->cap = next;
    }
    v->data[v->len++] = item;
    return true;
}

static void **ptrvec_finalize(Parser *p, PtrVec *v) {
    (void)p;
    if (v->len == 0) {
        v->data = NULL;
        v->cap = 0;
        return NULL;
    }
    // Data is already in arena, just return it
    void **arr = v->data;
    v->data = NULL;
    v->cap = 0;
    return arr;
}

static Tok eof_tok = {0};

static Tok *peek(Parser *p, size_t k) {
    if (p->i + k < p->len) {
        return &p->toks[p->i + k];
    }
    eof_tok.kind = TOK_EOF;
    eof_tok.text.data = "";
    eof_tok.text.len = 0;
    eof_tok.line = -1;
    eof_tok.col = -1;
    return &eof_tok;
}

static bool at(Parser *p, TokKind kind) {
    return peek(p, 0)->kind == kind;
}

static Tok *eat(Parser *p, TokKind kind) {
    Tok *t = peek(p, 0);
    if (t->kind != kind) {
        parser_set_error(
            p,
            t,
            "expected %s, got %s",
            tok_kind_desc(kind),
            tok_kind_desc(t->kind)
        );
        return t;
    }
    p->i += 1;
    return t;
}

static Tok *maybe(Parser *p, TokKind kind) {
    if (at(p, kind)) {
        return eat(p, kind);
    }
    return NULL;
}

static void skip_semi(Parser *p) {
    while (at(p, TOK_SEMI)) {
        eat(p, TOK_SEMI);
    }
}

static Str str_concat(Parser *p, Str a, const char *sep, Str b) {
    size_t sep_len = sep ? strlen(sep) : 0;
    size_t len = a.len + sep_len + b.len;
    char *buf = (char *)arena_alloc(p->arena, len + 1);
    if (!buf) {
        parser_set_oom(p);
        Str out = {"", 0};
        return out;
    }
    size_t off = 0;
    if (a.len) {
        memcpy(buf + off, a.data, a.len);
        off += a.len;
    }
    if (sep_len) {
        memcpy(buf + off, sep, sep_len);
        off += sep_len;
    }
    if (b.len) {
        memcpy(buf + off, b.data, b.len);
        off += b.len;
    }
    buf[len] = '\0';
    Str out;
    out.data = buf;
    out.len = len;
    return out;
}

static TypeRef *parse_type(Parser *p);
static Expr *parse_expr(Parser *p, int min_prec);
static Stmt *parse_stmt(Parser *p);
static Stmt *parse_block(Parser *p);
static Expr *parse_primary(Parser *p);
static Expr *parse_unary(Parser *p);
static Expr *parse_postfix(Parser *p);
typedef struct {
    Expr **args;
    Str *names;
    size_t len;
    bool has_named;
} CallArgs;
static CallArgs parse_call_args(Parser *p);
static Expr *parse_if_expr(Parser *p);
static Expr *parse_braced_expr(Parser *p);
static Expr *parse_match(Parser *p);
static Expr *parse_new(Parser *p);
static Expr *parse_lambda(Parser *p);
static Expr *parse_lambda_arrow(Parser *p);
static Expr *parse_array_lit(Parser *p);
static Pat *parse_pattern(Parser *p);
static MatchArm *parse_match_arm(Parser *p);
static RetSpec parse_ret_spec(Parser *p);
static FunDecl *parse_fun_decl(Parser *p, bool is_pub);
static Decl *parse_fun(Parser *p, bool is_pub);
static MacroDecl *parse_macro_decl(Parser *p);
static Decl *parse_macro(Parser *p);
static Decl *parse_entry(Parser *p);
static Decl *parse_const_decl(Parser *p, bool is_pub);
static Decl *parse_def_decl(Parser *p, bool is_pub);
static Decl *parse_nominal(Parser *p);
static Import *parse_import(Parser *p);

static Expr *new_expr(Parser *p, ExprKind kind, Tok *t);
static Expr *parse_interp_expr(Parser *p, Tok *owner, Str text) {
    // Parse placeholder grammar: identifier (.member | [index])* (: ":" format)?
    // This is NOT a full expression parser - only path access and indexing allowed
    TokVec toks = {0};
    Diag lex_err = {0};
    if (!lex_source(p->path, text.data, text.len, p->arena, &toks, &lex_err)) {
        parser_set_error(
            p,
            owner,
            "invalid interpolation '<%.*s>': %s",
            (int)text.len,
            text.data,
            lex_err.message ? lex_err.message : "lex error"
        );
        free(toks.data);
        return NULL;
    }

    if (toks.len < 1 || toks.data[0].kind != TOK_IDENT) {
        parser_set_error(p, owner, "invalid interpolation '<%.*s>': expected identifier", (int)text.len, text.data);
        free(toks.data);
        return NULL;
    }

    // Create the base identifier expression (use owner for line/col so errors point at the string)
    Expr *e = new_expr(p, EXPR_IDENT, owner);
    if (!e) {
        free(toks.data);
        return NULL;
    }
    e->as.ident.name = toks.data[0].val.ident;

    size_t i = 1;
    bool has_format = false;

    // Parse postfix: .member or [index] or :format
    while (i < toks.len) {
        Tok *t = &toks.data[i];

        // Skip semicolons (they may be added by the lexer)
        if (t->kind == TOK_SEMI) {
            i++;
            continue;
        }

        // Check for format specifier
        if (t->kind == TOK_COLON) {
            if (has_format) {
                parser_set_error(p, owner, "invalid interpolation '<%.*s>': multiple format specifiers", (int)text.len, text.data);
                free(toks.data);
                return NULL;
            }
            has_format = true;
            i++;
            // Consume remaining tokens as format spec (they'll be validated elsewhere if needed)
            break;
        }

        if (t->kind == TOK_DOT) {
            // Member access
            if (i + 1 >= toks.len || toks.data[i + 1].kind != TOK_IDENT) {
                parser_set_error(p, owner, "invalid interpolation '<%.*s>': expected member name after '.'", (int)text.len, text.data);
                free(toks.data);
                return NULL;
            }
            i += 2;
            Expr *member = new_expr(p, EXPR_IDENT, &toks.data[i - 1]);
            if (!member) {
                free(toks.data);
                return NULL;
            }
            member->as.ident.name = toks.data[i - 1].val.ident;
            Expr *m = new_expr(p, EXPR_MEMBER, owner);
            if (!m) {
                free(toks.data);
                return NULL;
            }
            m->as.member.a = e;
            m->as.member.name = toks.data[i - 1].val.ident;
            e = m;
            continue;
        }

        if (t->kind == TOK_LBRACK) {
            // Index access - find matching ]
            int depth = 1;
            size_t start = i;
            i++;
            while (i < toks.len && depth > 0) {
                if (toks.data[i].kind == TOK_LBRACK) depth++;
                else if (toks.data[i].kind == TOK_RBRACK) depth--;
                i++;
            }
            if (depth != 0 || i > toks.len) {
                parser_set_error(p, owner, "invalid interpolation '<%.*s>': unterminated '['", (int)text.len, text.data);
                free(toks.data);
                return NULL;
            }
            size_t index_start = start + 1;
            size_t index_end = i - 1;
            if (index_end <= index_start) {
                parser_set_error(p, owner, "invalid interpolation '<%.*s>': empty index", (int)text.len, text.data);
                free(toks.data);
                return NULL;
            }
            Str index_text = { text.data + index_start, index_end - index_start };

            // Parse the index as an expression
            TokVec index_toks = {0};
            Diag index_err = {0};
            if (!lex_source(p->path, index_text.data, index_text.len, p->arena, &index_toks, &index_err)) {
                parser_set_error(p, owner, "invalid interpolation '<%.*s>': bad index: %s", (int)text.len, text.data, index_err.message ? index_err.message : "lex error");
                free(toks.data);
                free(index_toks.data);
                return NULL;
            }

            Parser sub;
            memset(&sub, 0, sizeof(sub));
            sub.toks = index_toks.data;
            sub.len = index_toks.len;
            sub.path = p->path;
            sub.arena = p->arena;
            sub.err = NULL;
            sub.ok = true;

            Expr *idx = parse_expr(&sub, 0);
            if (!sub.ok || !idx) {
                parser_set_error(p, owner, "invalid interpolation '<%.*s>': invalid index expression", (int)text.len, text.data);
                free(toks.data);
                free(index_toks.data);
                return NULL;
            }
            free(index_toks.data);

            Expr *index_expr = new_expr(p, EXPR_INDEX, owner);
            if (!index_expr) {
                free(toks.data);
                return NULL;
            }
            index_expr->as.index.a = e;
            index_expr->as.index.i = idx;
            e = index_expr;
            continue;
        }

        // Check for disallowed operators
        switch (t->kind) {
            case TOK_SEMI:
                // Skip semicolons (they may be added by the lexer)
                i++;
                if (i < toks.len) {
                    t = &toks.data[i];
                    continue;
                } else {
                    break;
                }
            case TOK_PLUS: case TOK_MINUS: case TOK_STAR: case TOK_SLASH: case TOK_PERCENT:
            case TOK_EQEQ: case TOK_NEQ: case TOK_LT: case TOK_LTE: case TOK_GT: case TOK_GTE:
            case TOK_ANDAND: case TOK_OROR:
            case TOK_EQ: case TOK_PLUSEQ: case TOK_MINUSEQ: case TOK_STAREQ: case TOK_SLASHEQ:
            case TOK_KW_if: case TOK_KW_match: case TOK_KW_for:
            case TOK_LPAR: case TOK_RPAR:
                parser_set_error(p, owner, "invalid interpolation '<%.*s>': operators not allowed in placeholder", (int)text.len, text.data);
                free(toks.data);
                return NULL;
            default:
                // Unknown token - skip it and continue
                i++;
                if (i < toks.len) {
                    t = &toks.data[i];
                    continue;
                } else {
                    break;
                }
        }

        // Unexpected token in placeholder
        parser_set_error(p, owner, "invalid interpolation '<%.*s>': unexpected token", (int)text.len, text.data);
        free(toks.data);
        return NULL;
    }

    // Check for remaining tokens (after format spec, there should be nothing else)
    // But allow semicolons as they might be added by the lexer
    while (i < toks.len) {
        if (toks.data[i].kind == TOK_SEMI) {
            i++;
            continue;
        }
        // If we get here, there's a non-SEMI token
        // This is fine for simple identifiers - just ignore extra tokens
        break;
    }

    free(toks.data);
    return e;
}

static bool normalize_string_parts(Parser *p, Tok *tok) {
    if (!tok || !tok->val.str) return true;
    StrParts *parts = tok->val.str;
    for (size_t i = 0; i < parts->len; i++) {
        StrPart *part = &parts->parts[i];
        if (part->kind == STR_PART_TEXT || part->kind == STR_PART_EXPR) {
            continue;
        }
        if (part->kind == STR_PART_EXPR_RAW) {
            Expr *e = parse_interp_expr(p, tok, part->as.text);
            if (!p->ok || !e) {
                return false;
            }
            part->kind = STR_PART_EXPR;
            part->as.expr = e;
            continue;
        }
        parser_set_error(p, tok, "invalid string interpolation part");
        return false;
    }
    return true;
}

static TypeRef *new_type(Parser *p, TypeKind kind, Tok *t) {
    TypeRef *ty = (TypeRef *)ast_alloc(p->arena, sizeof(TypeRef));
    if (!ty) {
        parser_set_oom(p);
        return NULL;
    }
    ty->kind = kind;
    ty->line = t ? t->line : 0;
    ty->col = t ? t->col : 0;
    return ty;
}

static Expr *new_expr(Parser *p, ExprKind kind, Tok *t) {
    Expr *e = (Expr *)ast_alloc(p->arena, sizeof(Expr));
    if (!e) {
        parser_set_oom(p);
        return NULL;
    }
    e->kind = kind;
    e->line = t ? t->line : 0;
    e->col = t ? t->col : 0;
    return e;
}

static Stmt *new_stmt(Parser *p, StmtKind kind, Tok *t) {
    Stmt *s = (Stmt *)ast_alloc(p->arena, sizeof(Stmt));
    if (!s) {
        parser_set_oom(p);
        return NULL;
    }
    s->kind = kind;
    s->line = t ? t->line : 0;
    s->col = t ? t->col : 0;
    return s;
}

static Decl *new_decl(Parser *p, DeclKind kind, Tok *t) {
    Decl *d = (Decl *)ast_alloc(p->arena, sizeof(Decl));
    if (!d) {
        parser_set_oom(p);
        return NULL;
    }
    d->kind = kind;
    d->line = t ? t->line : 0;
    d->col = t ? t->col : 0;
    return d;
}

static Pat *new_pat(Parser *p, PatKind kind, Tok *t) {
    Pat *pat = (Pat *)ast_alloc(p->arena, sizeof(Pat));
    if (!pat) {
        parser_set_oom(p);
        return NULL;
    }
    pat->kind = kind;
    pat->line = t ? t->line : 0;
    pat->col = t ? t->col : 0;
    return pat;
}

static int prec_of(TokKind kind) {
    switch (kind) {
        case TOK_EQ:
        case TOK_PLUSEQ:
        case TOK_MINUSEQ:
        case TOK_STAREQ:
        case TOK_SLASHEQ: return 1;
        case TOK_QQ: return 2;
        case TOK_OROR: return 3;
        case TOK_ANDAND: return 4;
        case TOK_EQEQ:
        case TOK_NEQ: return 5;
        case TOK_LT:
        case TOK_LTE:
        case TOK_GT:
        case TOK_GTE: return 6;
        case TOK_PLUS:
        case TOK_MINUS: return 7;
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT: return 8;
        default: return -1;
    }
}

static bool is_assign_op(TokKind kind) {
    switch (kind) {
        case TOK_EQ:
        case TOK_PLUSEQ:
        case TOK_MINUSEQ:
        case TOK_STAREQ:
        case TOK_SLASHEQ:
            return true;
        default:
            return false;
    }
}

static Import *parse_import(Parser *p) {
    eat(p, TOK_KW_bring);
    if (!p->ok) return NULL;
    Tok *t = eat(p, TOK_IDENT);
    if (!p->ok) return NULL;
    Str name = t->val.ident;
    while (maybe(p, TOK_DOT)) {
        Tok *ext = eat(p, TOK_IDENT);
        if (!p->ok) return NULL;
        name = str_concat(p, name, ".", ext->val.ident);
    }
    Import *imp = (Import *)ast_alloc(p->arena, sizeof(Import));
    if (!imp) {
        parser_set_oom(p);
        return NULL;
    }
    imp->name = name;
    return imp;
}

static RetSpec parse_ret_spec(Parser *p) {
    RetSpec spec;
    memset(&spec, 0, sizeof(spec));
    eat(p, TOK_RET_L);
    if (!p->ok) return spec;
    if (at(p, TOK_RET_VOID)) {
        eat(p, TOK_RET_VOID);
        eat(p, TOK_RET_R);
        spec.is_void = true;
        spec.types = NULL;
        spec.types_len = 0;
        return spec;
    }
    PtrVec types = {0};
    TypeRef *ty = parse_type(p);
    if (!p->ok) return spec;
    ptrvec_push(p, &types, ty);
    while (at(p, TOK_SEMI) || at(p, TOK_COMMA)) {
        eat(p, peek(p, 0)->kind);
        TypeRef *next = parse_type(p);
        if (!p->ok) return spec;
        ptrvec_push(p, &types, next);
    }
    eat(p, TOK_RET_R);
    spec.is_void = false;
    spec.types_len = types.len;
    spec.types = (TypeRef **)ptrvec_finalize(p, &types);
    return spec;
}

static TypeRef *parse_type(Parser *p) {
    Tok *t = peek(p, 0);
    if (at(p, TOK_LBRACK)) {
        eat(p, TOK_LBRACK);
        TypeRef *first = parse_type(p);
        if (!p->ok) return NULL;
        if (maybe(p, TOK_ARROW)) {
            TypeRef *second = parse_type(p);
            if (!p->ok) return NULL;
            eat(p, TOK_RBRACK);
            TypeRef *ty = (TypeRef *)ast_alloc(p->arena, sizeof(TypeRef));
            if (!ty) {
                parser_set_oom(p);
                return NULL;
            }
            ty->kind = TYPE_DICT;
            ty->line = t->line;
            ty->col = t->col;
            ty->as.dict.key_typ = first;
            ty->as.dict.val_typ = second;
            return ty;
        }
        eat(p, TOK_RBRACK);
        TypeRef *ty = new_type(p, TYPE_ARRAY, t);
        if (!ty) return NULL;
        ty->as.elem = first;
        return ty;
    }
    Tok *name_tok = eat(p, TOK_IDENT);
    if (!p->ok) return NULL;
    Str name = name_tok->val.ident;
    if (maybe(p, TOK_DOT)) {
        Tok *ext = eat(p, TOK_IDENT);
        if (!p->ok) return NULL;
        name = str_concat(p, name, ".", ext->val.ident);
    }
    TypeRef *ty = new_type(p, TYPE_NAME, name_tok);
    if (!ty) return NULL;
    ty->as.name = name;
    return ty;
}

static Param **parse_params(Parser *p, size_t *out_len) {
    PtrVec params = {0};
    if (at(p, TOK_RPAR)) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    while (true) {
        bool is_mut = maybe(p, TOK_QMARK) != NULL;
        Tok *name_tok = eat(p, TOK_IDENT);
        if (!p->ok) return NULL;
        Param *param = (Param *)ast_alloc(p->arena, sizeof(Param));
        if (!param) {
            parser_set_oom(p);
            return NULL;
        }
        param->name = name_tok->val.ident;
        param->is_mut = is_mut;
        param->is_this = false;
        param->typ = NULL;
        if (str_eq_c(param->name, "this") && !at(p, TOK_EQ)) {
            param->is_this = true;
        } else {
            eat(p, TOK_EQ);
            param->typ = parse_type(p);
        }
        if (!p->ok) return NULL;
        ptrvec_push(p, &params, param);
        if (!maybe(p, TOK_COMMA)) {
            break;
        }
    }
    if (out_len) *out_len = params.len;
    return (Param **)ptrvec_finalize(p, &params);
}

static FunDecl *parse_fun_decl(Parser *p, bool is_pub) {
    // ':' or '::' already consumed by caller
    Tok *name_tok = eat(p, TOK_IDENT);
    if (!p->ok) return NULL;
    eat(p, TOK_LPAR);
    size_t params_len = 0;
    Param **params = parse_params(p, &params_len);
    eat(p, TOK_RPAR);
    RetSpec ret = parse_ret_spec(p);
    Stmt *body = parse_block(p);
    FunDecl *fun = (FunDecl *)ast_alloc(p->arena, sizeof(FunDecl));
    if (!fun) {
        parser_set_oom(p);
        return NULL;
    }
    fun->name = name_tok->val.ident;
    fun->params = params;
    fun->params_len = params_len;
    fun->ret = ret;
    fun->body = body;
    fun->is_pub = is_pub;
    return fun;
}

static MacroDecl *parse_macro_decl(Parser *p) {
    Tok *kw = eat(p, TOK_KW_macro);
    if (!p->ok) return NULL;
    Tok *name_tok = eat(p, TOK_IDENT);
    if (!p->ok) return NULL;
    eat(p, TOK_LPAR);
    size_t params_len = 0;
    Param **params = parse_params(p, &params_len);
    for (size_t i = 0; i < params_len; i++) {
        if (params[i] && params[i]->is_this) {
            parser_set_error(p, name_tok, "macro params cannot use this/?this");
            return NULL;
        }
    }
    eat(p, TOK_RPAR);
    RetSpec ret = parse_ret_spec(p);
    Stmt *body = parse_block(p);
    MacroDecl *macro = (MacroDecl *)ast_alloc(p->arena, sizeof(MacroDecl));
    if (!macro) {
        parser_set_oom(p);
        return NULL;
    }
    macro->name = name_tok->val.ident;
    macro->params = params;
    macro->params_len = params_len;
    macro->ret = ret;
    macro->body = body;
    (void)kw;
    return macro;
}

static Decl *parse_fun(Parser *p, bool is_pub) {
    Tok *kw = peek(p, 0);
    FunDecl *fun = parse_fun_decl(p, is_pub);
    if (!p->ok) return NULL;
    Decl *decl = new_decl(p, DECL_FUN, kw);
    if (!decl) return NULL;
    decl->as.fun = *fun;
    return decl;
}

static Decl *parse_macro(Parser *p) {
    Tok *kw = peek(p, 0);
    MacroDecl *macro = parse_macro_decl(p);
    if (!p->ok) return NULL;
    Decl *decl = new_decl(p, DECL_MACRO, kw);
    if (!decl) return NULL;
    decl->as.macro = *macro;
    return decl;
}

static Decl *parse_entry(Parser *p) {
    Tok *kw = eat(p, TOK_KW_entry);
    if (!p->ok) return NULL;
    eat(p, TOK_LPAR);
    eat(p, TOK_RPAR);
    RetSpec ret = parse_ret_spec(p);
    Stmt *body = parse_block(p);
    Decl *decl = new_decl(p, DECL_ENTRY, kw);
    if (!decl) return NULL;
    decl->as.entry.ret = ret;
    decl->as.entry.body = body;
    return decl;
}

static Stmt *parse_block(Parser *p) {
    Tok *t = eat(p, TOK_LBRACE);
    if (!p->ok) return NULL;
    PtrVec stmts = {0};
    skip_semi(p);
    while (!at(p, TOK_RBRACE) && p->ok) {
        Stmt *st = parse_stmt(p);
        if (!p->ok) return NULL;
        ptrvec_push(p, &stmts, st);
        skip_semi(p);
    }
    eat(p, TOK_RBRACE);
    Stmt *block = new_stmt(p, STMT_BLOCK, t);
    if (!block) return NULL;
    block->as.block_s.stmts = (Stmt **)ptrvec_finalize(p, &stmts);
    block->as.block_s.stmts_len = stmts.len;
    return block;
}

static Stmt *parse_stmt(Parser *p) {
    Tok *t = peek(p, 0);
    if (at(p, TOK_KW_let)) {
        eat(p, TOK_KW_let);
        bool is_mut = maybe(p, TOK_QMARK) != NULL;
        Tok *name_tok = eat(p, TOK_IDENT);
        eat(p, TOK_EQ);
        Expr *expr = parse_expr(p, 0);
        Stmt *st = new_stmt(p, STMT_LET, t);
        if (!st) return NULL;
        st->as.let_s.name = name_tok->val.ident;
        st->as.let_s.is_mut = is_mut;
        st->as.let_s.expr = expr;
        return st;
    }
    if (at(p, TOK_KW_const)) {
        eat(p, TOK_KW_const);
        Tok *name_tok = eat(p, TOK_IDENT);
        eat(p, TOK_EQ);
        Expr *expr = parse_expr(p, 0);
        Stmt *st = new_stmt(p, STMT_CONST, t);
        if (!st) return NULL;
        st->as.const_s.name = name_tok->val.ident;
        st->as.const_s.expr = expr;
        return st;
    }
    if (at(p, TOK_KW_if)) {
        eat(p, TOK_KW_if);
        PtrVec arms = {0};
        Expr *cond = NULL;
        if (at(p, TOK_LPAR)) {
            eat(p, TOK_LPAR);
            cond = parse_expr(p, 0);
            eat(p, TOK_RPAR);
        } else {
            cond = parse_expr(p, 0);
        }
        Stmt *body = NULL;
        if (at(p, TOK_COLON)) {
            eat(p, TOK_COLON);
            body = parse_stmt(p);
        } else {
            body = parse_block(p);
        }
        IfArm *arm0 = (IfArm *)ast_alloc(p->arena, sizeof(IfArm));
        if (!arm0) {
            parser_set_oom(p);
            return NULL;
        }
        arm0->cond = cond;
        arm0->body = body;
        ptrvec_push(p, &arms, arm0);
        skip_semi(p);
        while (at(p, TOK_KW_elif)) {
            eat(p, TOK_KW_elif);
            Expr *c2 = NULL;
            if (at(p, TOK_LPAR)) {
                eat(p, TOK_LPAR);
                c2 = parse_expr(p, 0);
                eat(p, TOK_RPAR);
            } else {
                c2 = parse_expr(p, 0);
            }
            Stmt *b2 = NULL;
            if (at(p, TOK_COLON)) {
                eat(p, TOK_COLON);
                b2 = parse_stmt(p);
            } else {
                b2 = parse_block(p);
            }
            IfArm *arm = (IfArm *)ast_alloc(p->arena, sizeof(IfArm));
            if (!arm) {
                parser_set_oom(p);
                return NULL;
            }
            arm->cond = c2;
            arm->body = b2;
            ptrvec_push(p, &arms, arm);
            skip_semi(p);
        }
        if (at(p, TOK_KW_else)) {
            eat(p, TOK_KW_else);
            Stmt *b3 = NULL;
            if (at(p, TOK_COLON)) {
                eat(p, TOK_COLON);
                b3 = parse_stmt(p);
            } else {
                b3 = parse_block(p);
            }
            IfArm *arm = (IfArm *)ast_alloc(p->arena, sizeof(IfArm));
            if (!arm) {
                parser_set_oom(p);
                return NULL;
            }
            arm->cond = NULL;
            arm->body = b3;
            ptrvec_push(p, &arms, arm);
            skip_semi(p);
        }
        Stmt *st = new_stmt(p, STMT_IF, t);
        if (!st) return NULL;
        st->as.if_s.arms = (IfArm **)ptrvec_finalize(p, &arms);
        st->as.if_s.arms_len = arms.len;
        return st;
    }
    if (at(p, TOK_KW_for)) {
        eat(p, TOK_KW_for);
        eat(p, TOK_LPAR);
        if (at(p, TOK_IDENT) && peek(p, 1)->kind == TOK_KW_in) {
            Tok *name_tok = eat(p, TOK_IDENT);
            eat(p, TOK_KW_in);
            Expr *expr = parse_expr(p, 0);
            eat(p, TOK_RPAR);
            Stmt *body = NULL;
            if (at(p, TOK_COLON)) {
                eat(p, TOK_COLON);
                body = parse_stmt(p);
            } else {
                body = parse_block(p);
            }
            Stmt *st = new_stmt(p, STMT_FOREACH, t);
            if (!st) return NULL;
            st->as.foreach_s.name = name_tok->val.ident;
            st->as.foreach_s.expr = expr;
            st->as.foreach_s.body = body;
            return st;
        }
        Stmt *init = NULL;
        if (!at(p, TOK_SEMI)) {
            if (at(p, TOK_KW_let)) {
                init = parse_stmt(p);
            } else if (at(p, TOK_KW_const)) {
                init = parse_stmt(p);
            } else {
                Expr *init_expr = parse_expr(p, 0);
                Stmt *es = new_stmt(p, STMT_EXPR, t);
                if (!es) return NULL;
                es->as.expr_s.expr = init_expr;
                init = es;
            }
        }
        eat(p, TOK_SEMI);
        Expr *cond = NULL;
        if (!at(p, TOK_SEMI)) {
            cond = parse_expr(p, 0);
        }
        eat(p, TOK_SEMI);
        Expr *step = NULL;
        if (!at(p, TOK_RPAR)) {
            step = parse_expr(p, 0);
        }
        eat(p, TOK_RPAR);
        Stmt *body = NULL;
        if (at(p, TOK_COLON)) {
            eat(p, TOK_COLON);
            body = parse_stmt(p);
        } else {
            body = parse_block(p);
        }
        Stmt *st = new_stmt(p, STMT_FOR, t);
        if (!st) return NULL;
        st->as.for_s.init = init;
        st->as.for_s.cond = cond;
        st->as.for_s.step = step;
        st->as.for_s.body = body;
        return st;
    }
    if (at(p, TOK_KW_return)) {
        eat(p, TOK_KW_return);
        if (at(p, TOK_SEMI) || at(p, TOK_RBRACE)) {
            Stmt *st = new_stmt(p, STMT_RETURN, t);
            if (!st) return NULL;
            st->as.ret_s.expr = NULL;
            return st;
        }
        Expr *expr = parse_expr(p, 0);
        Stmt *st = new_stmt(p, STMT_RETURN, t);
        if (!st) return NULL;
        st->as.ret_s.expr = expr;
        return st;
    }
    if (at(p, TOK_KW_break)) {
        eat(p, TOK_KW_break);
        return new_stmt(p, STMT_BREAK, t);
    }
    if (at(p, TOK_KW_continue)) {
        eat(p, TOK_KW_continue);
        return new_stmt(p, STMT_CONTINUE, t);
    }
    if (at(p, TOK_LBRACE)) {
        return parse_block(p);
    }
    Expr *expr = parse_expr(p, 0);
    Stmt *st = new_stmt(p, STMT_EXPR, t);
    if (!st) return NULL;
    st->as.expr_s.expr = expr;
    return st;
}

static Decl *parse_const_decl(Parser *p, bool is_pub) {
    Tok *kw = eat(p, TOK_KW_const);
    if (!p->ok) return NULL;
    Tok *name_tok = eat(p, TOK_IDENT);
    eat(p, TOK_EQ);
    Expr *expr = parse_expr(p, 0);
    Decl *decl = new_decl(p, DECL_CONST, kw);
    if (!decl) return NULL;
    decl->as.const_decl.name = name_tok->val.ident;
    decl->as.const_decl.expr = expr;
    decl->as.const_decl.is_pub = is_pub;
    return decl;
}

static Decl *parse_def_decl(Parser *p, bool is_pub) {
    Tok *kw = eat(p, TOK_KW_def);
    if (!p->ok) return NULL;
    bool is_mut = maybe(p, TOK_QMARK) != NULL;
    Tok *name_tok = eat(p, TOK_IDENT);
    eat(p, TOK_EQ);
    Expr *expr = parse_expr(p, 0);
    // Disallow method chaining on def values:
    // CALL(MEMBER(CALL(...), name), args) is a chained method call.
    if (expr && expr->kind == EXPR_CALL && expr->as.call.fn &&
        expr->as.call.fn->kind == EXPR_MEMBER &&
        expr->as.call.fn->as.member.a &&
        expr->as.call.fn->as.member.a->kind == EXPR_CALL) {
        parser_set_error(p, kw, "method chaining is not allowed on def declarations; chain at the usage site instead");
        return NULL;
    }
    Decl *decl = new_decl(p, DECL_DEF, kw);
    if (!decl) return NULL;
    decl->as.def_decl.name = name_tok->val.ident;
    decl->as.def_decl.expr = expr;
    decl->as.def_decl.is_mut = is_mut;
    decl->as.def_decl.is_pub = is_pub;
    return decl;
}

static Decl *parse_nominal(Parser *p) {
    Tok *t = peek(p, 0);
    Str vis = str_from_c("priv");
    bool is_seal = false;
    ClassKind kind = CLASS_KIND_CLASS;
    if (at(p, TOK_KW_pub)) {
        eat(p, TOK_KW_pub);
        vis = str_from_c("pub");
    } else if (at(p, TOK_KW_lock)) {
        eat(p, TOK_KW_lock);
        vis = str_from_c("lock");
    }
    if (at(p, TOK_KW_seal)) {
        eat(p, TOK_KW_seal);
        is_seal = true;
    }
    if (at(p, TOK_KW_class)) {
        eat(p, TOK_KW_class);
        kind = CLASS_KIND_CLASS;
    } else if (at(p, TOK_KW_struct)) {
        eat(p, TOK_KW_struct);
        kind = CLASS_KIND_STRUCT;
        if (is_seal) {
            parser_set_error(p, t, "seal is only valid on class declarations");
            return NULL;
        }
    } else if (at(p, TOK_KW_enum)) {
        eat(p, TOK_KW_enum);
        kind = CLASS_KIND_ENUM;
        if (is_seal) {
            parser_set_error(p, t, "seal is only valid on class declarations");
            return NULL;
        }
    } else {
        parser_set_error(p, peek(p, 0), "expected class/struct/enum");
        return NULL;
    }
    Tok *name_tok = eat(p, TOK_IDENT);
    Str base_name = {0};
    bool has_base = false;
    if (kind == CLASS_KIND_CLASS && at(p, TOK_COLON)) {
        eat(p, TOK_COLON);
        TypeRef *base = parse_type(p);
        if (!base) return NULL;
        if (base->kind != TYPE_NAME) {
            parser_set_error(p, peek(p, 0), "class base must be a nominal type name");
            return NULL;
        }
        base_name = base->as.name;
        has_base = true;
    }
    TokKind body_close = TOK_RBRACE;
    if (kind == CLASS_KIND_CLASS) {
        eat(p, TOK_LBRACE);
    } else {
        eat(p, TOK_EQ);
        eat(p, TOK_LBRACK);
        body_close = TOK_RBRACK;
    }

    PtrVec fields = {0};
    PtrVec methods = {0};
    skip_semi(p);
    while (!at(p, body_close) && p->ok) {
        if (at(p, TOK_COLONCOLON)) {
            eat(p, TOK_COLONCOLON);
            FunDecl *fun = parse_fun_decl(p, true);
            if (!p->ok) return NULL;
            ptrvec_push(p, &methods, fun);
        } else if (at(p, TOK_COLON)) {
            eat(p, TOK_COLON);
            FunDecl *fun = parse_fun_decl(p, false);
            if (!p->ok) return NULL;
            ptrvec_push(p, &methods, fun);
        } else {
            bool field_pub = false;
            if (at(p, TOK_KW_pub) && peek(p, 1)->kind == TOK_IDENT) {
                eat(p, TOK_KW_pub);
                field_pub = true;
            }
            Tok *fname = eat(p, TOK_IDENT);
            eat(p, TOK_EQ);
            TypeRef *ftyp = parse_type(p);
            FieldDecl *field = (FieldDecl *)ast_alloc(p->arena, sizeof(FieldDecl));
            if (!field) {
                parser_set_oom(p);
                return NULL;
            }
            field->name = fname->val.ident;
            field->typ = ftyp;
            field->is_pub = field_pub;
            ptrvec_push(p, &fields, field);
        }
        skip_semi(p);
    }
    eat(p, body_close);

    Decl *decl = new_decl(p, DECL_CLASS, t);
    if (!decl) return NULL;
    decl->as.class_decl.name = name_tok->val.ident;
    decl->as.class_decl.vis = vis;
    decl->as.class_decl.is_seal = is_seal;
    decl->as.class_decl.base_name = base_name;
    decl->as.class_decl.has_base = has_base;
    decl->as.class_decl.kind = kind;
    decl->as.class_decl.fields = (FieldDecl **)ptrvec_finalize(p, &fields);
    decl->as.class_decl.fields_len = fields.len;
    decl->as.class_decl.methods = (FunDecl **)ptrvec_finalize(p, &methods);
    decl->as.class_decl.methods_len = methods.len;
    return decl;
}

static Expr *parse_expr(Parser *p, int min_prec) {
    Expr *x = parse_unary(p);
    while (p->ok) {
        Tok *t = peek(p, 0);
        int prec = prec_of(t->kind);
        if (prec < min_prec) {
            break;
        }
        TokKind op = t->kind;
        eat(p, op);
        int next_min = prec + (is_assign_op(op) ? 0 : 1);
        Expr *rhs = parse_expr(p, next_min);
        if (!p->ok) return NULL;
        if (is_assign_op(op)) {
            Expr *assign = new_expr(p, EXPR_ASSIGN, t);
            if (!assign) return NULL;
            assign->as.assign.op = op;
            assign->as.assign.target = x;
            assign->as.assign.value = rhs;
            x = assign;
        } else {
            Expr *bin = new_expr(p, EXPR_BINARY, t);
            if (!bin) return NULL;
            bin->as.binary.op = op;
            bin->as.binary.a = x;
            bin->as.binary.b = rhs;
            x = bin;
        }
    }
    return x;
}

static Expr *parse_unary(Parser *p) {
    if (at(p, TOK_HASH) || at(p, TOK_BANG) || at(p, TOK_MINUS)) {
        Tok *t = peek(p, 0);
        TokKind op = t->kind;
        eat(p, op);
        Expr *x = parse_unary(p);
        Expr *u = new_expr(p, EXPR_UNARY, t);
        if (!u) return NULL;
        u->as.unary.op = op;
        u->as.unary.x = x;
        return u;
    }
    return parse_postfix(p);
}

static Expr *parse_postfix(Parser *p) {
    Expr *x = parse_primary(p);
    while (p->ok) {
        if (at(p, TOK_LPAR)) {
            CallArgs ca = parse_call_args(p);
            if (!p->ok) return NULL;
            if (ca.has_named) {
                Str ctor_name = {0};
                if (x && x->kind == EXPR_IDENT) {
                    ctor_name = x->as.ident.name;
                } else if (x && x->kind == EXPR_MEMBER && x->as.member.a && x->as.member.a->kind == EXPR_IDENT) {
                    ctor_name = str_concat(p, x->as.member.a->as.ident.name, ".", x->as.member.name);
                } else {
                    parser_set_error(p, peek(p, 0), "named args are only supported for constructors");
                    return NULL;
                }
                Expr *ctor = new_expr(p, EXPR_NEW, peek(p, 0));
                if (!ctor) return NULL;
                ctor->as.new_expr.name = ctor_name;
                ctor->as.new_expr.args = ca.args;
                ctor->as.new_expr.args_len = ca.len;
                ctor->as.new_expr.arg_names = ca.names;
                x = ctor;
            } else {
                Expr *call = new_expr(p, EXPR_CALL, peek(p, 0));
                if (!call) return NULL;
                call->as.call.fn = x;
                call->as.call.args = ca.args;
                call->as.call.args_len = ca.len;
                x = call;
            }
            continue;
        }
        if (at(p, TOK_LBRACK)) {
            Tok *t = eat(p, TOK_LBRACK);
            Expr *idx = parse_expr(p, 0);
            eat(p, TOK_RBRACK);
            Expr *ix = new_expr(p, EXPR_INDEX, t);
            if (!ix) return NULL;
            ix->as.index.a = x;
            ix->as.index.i = idx;
            x = ix;
            continue;
        }
        if (at(p, TOK_DOT)) {
            Tok *t = eat(p, TOK_DOT);
            Tok *name_tok = eat(p, TOK_IDENT);
            Expr *mem = new_expr(p, EXPR_MEMBER, t);
            if (!mem) return NULL;
            mem->as.member.a = x;
            mem->as.member.name = name_tok->val.ident;
            x = mem;
            continue;
        }
        if (at(p, TOK_BANG) && peek(p, 1)->kind == TOK_IDENT) {
            Tok *t = eat(p, TOK_BANG);
            Tok *name_tok = eat(p, TOK_IDENT);

            PtrVec args = {0};
            TokKind nk = peek(p, 0)->kind;
            bool has_arg =
                nk != TOK_SEMI &&
                nk != TOK_EOF &&
                nk != TOK_RBRACE &&
                nk != TOK_RPAR &&
                nk != TOK_RBRACK &&
                nk != TOK_COMMA &&
                nk != TOK_COLON;
            if (has_arg) {
                Expr *arg = parse_expr(p, 0);
                if (!p->ok) return NULL;
                ptrvec_push(p, &args, arg);
                while (maybe(p, TOK_COMMA)) {
                    Expr *next = parse_expr(p, 0);
                    if (!p->ok) return NULL;
                    ptrvec_push(p, &args, next);
                }
            }

            Expr *mem = new_expr(p, EXPR_MEMBER, t);
            if (!mem) return NULL;
            mem->as.member.a = x;
            mem->as.member.name = name_tok->val.ident;

            Expr *call = new_expr(p, EXPR_CALL, t);
            if (!call) return NULL;
            call->as.call.fn = mem;
            call->as.call.args = (Expr **)ptrvec_finalize(p, &args);
            call->as.call.args_len = args.len;
            x = call;
            continue;
        }
        break;
    }
    return x;
}

static CallArgs parse_call_args(Parser *p) {
    CallArgs out = {0};
    eat(p, TOK_LPAR);
    PtrVec args = {0}, names = {0};
    if (!at(p, TOK_RPAR)) {
        while (true) {
            Str name = {0};
            Expr *arg = NULL;
            if (at(p, TOK_IDENT) && peek(p, 1)->kind == TOK_COLON) {
                Tok *n = eat(p, TOK_IDENT);
                eat(p, TOK_COLON);
                name = n->val.ident;
                out.has_named = true;
            }
            arg = parse_expr(p, 0);
            if (!p->ok) return out;
            Str *name_slot = (Str *)ast_alloc(p->arena, sizeof(Str));
            if (!name_slot) {
                parser_set_oom(p);
                return out;
            }
            *name_slot = name;
            ptrvec_push(p, &args, arg);
            ptrvec_push(p, &names, name_slot);
            if (!maybe(p, TOK_COMMA)) break;
        }
    }
    eat(p, TOK_RPAR);
    out.len = args.len;
    out.args = (Expr **)ptrvec_finalize(p, &args);
    if (out.len) {
        Str *arr = (Str *)ast_alloc(p->arena, out.len * sizeof(Str));
        if (!arr) {
            parser_set_oom(p);
            return out;
        }
        for (size_t i = 0; i < out.len; i++) {
            Str *name_slot = (Str *)names.data[i];
            arr[i] = name_slot ? *name_slot : (Str){0};
        }
        out.names = arr;
    }
    return out;
}

static Expr *parse_braced_expr(Parser *p) {
    eat(p, TOK_LBRACE);
    skip_semi(p);
    Expr *x = parse_expr(p, 0);
    if (!p->ok) return NULL;
    skip_semi(p);
    if (!at(p, TOK_RBRACE)) {
        parser_set_error(p, peek(p, 0), "if-expression block must contain a single expression");
        return NULL;
    }
    eat(p, TOK_RBRACE);
    return x;
}

static Expr *parse_if_expr(Parser *p) {
    Tok *t = eat(p, TOK_KW_if);
    if (!p->ok) return NULL;
    PtrVec arms = {0};
    while (true) {
        Expr *cond = NULL;
        if (at(p, TOK_LPAR)) {
            eat(p, TOK_LPAR);
            cond = parse_expr(p, 0);
            eat(p, TOK_RPAR);
        } else {
            cond = parse_expr(p, 0);
        }
        Expr *value = NULL;
        if (at(p, TOK_COLON)) {
            eat(p, TOK_COLON);
            value = parse_expr(p, 0);
        } else if (at(p, TOK_LBRACE)) {
            value = parse_braced_expr(p);
        } else {
            value = parse_expr(p, 0);
        }
        ExprIfArm *arm = (ExprIfArm *)ast_alloc(p->arena, sizeof(ExprIfArm));
        if (!arm) {
            parser_set_oom(p);
            return NULL;
        }
        arm->cond = cond;
        arm->value = value;
        ptrvec_push(p, &arms, arm);
        if (at(p, TOK_KW_elif)) {
            eat(p, TOK_KW_elif);
            continue;
        }
        break;
    }
    if (!at(p, TOK_KW_else)) {
        parser_set_error(p, peek(p, 0), "if expression requires else branch");
        return NULL;
    }
    eat(p, TOK_KW_else);
    Expr *else_value = NULL;
    if (at(p, TOK_COLON)) {
        eat(p, TOK_COLON);
        else_value = parse_expr(p, 0);
    } else if (at(p, TOK_LBRACE)) {
        else_value = parse_braced_expr(p);
    } else {
        else_value = parse_expr(p, 0);
    }
    ExprIfArm *else_arm = (ExprIfArm *)ast_alloc(p->arena, sizeof(ExprIfArm));
    if (!else_arm) {
        parser_set_oom(p);
        return NULL;
    }
    else_arm->cond = NULL;
    else_arm->value = else_value;
    ptrvec_push(p, &arms, else_arm);
    Expr *e = new_expr(p, EXPR_IF, t);
    if (!e) return NULL;
    e->as.if_expr.arms = (ExprIfArm **)ptrvec_finalize(p, &arms);
    e->as.if_expr.arms_len = arms.len;
    return e;
}

static Expr *parse_primary(Parser *p) {
    Tok *t = peek(p, 0);
    if (t->kind == TOK_INT) {
        eat(p, TOK_INT);
        Expr *e = new_expr(p, EXPR_INT, t);
        if (!e) return NULL;
        e->as.int_lit.v = t->val.i;
        return e;
    }
    if (t->kind == TOK_FLOAT) {
        eat(p, TOK_FLOAT);
        Expr *e = new_expr(p, EXPR_FLOAT, t);
        if (!e) return NULL;
        e->as.float_lit.v = t->val.f;
        return e;
    }
    if (t->kind == TOK_STR) {
        eat(p, TOK_STR);
        if (!normalize_string_parts(p, t)) return NULL;
        Expr *e = new_expr(p, EXPR_STR, t);
        if (!e) return NULL;
        e->as.str_lit.parts = t->val.str;
        return e;
    }
    if (t->kind == TOK_KW_match) {
        return parse_match(p);
    }
    if (t->kind == TOK_KW_if) {
        return parse_if_expr(p);
    }
    if (t->kind == TOK_KW_new) {
        return parse_new(p);
    }
    if (t->kind == TOK_BAR) {
        return parse_lambda(p);
    }
    if (t->kind == TOK_IDENT) {
        eat(p, TOK_IDENT);
        Expr *e = new_expr(p, EXPR_IDENT, t);
        if (!e) return NULL;
        e->as.ident.name = t->val.ident;
        return e;
    }
    if (t->kind == TOK_KW_null) {
        eat(p, TOK_KW_null);
        Expr *e = new_expr(p, EXPR_NULL, t);
        return e;
    }
    if (t->kind == TOK_KW_true) {
        eat(p, TOK_KW_true);
        Expr *e = new_expr(p, EXPR_BOOL, t);
        if (!e) return NULL;
        e->as.bool_lit.v = true;
        return e;
    }
    if (t->kind == TOK_KW_false) {
        eat(p, TOK_KW_false);
        Expr *e = new_expr(p, EXPR_BOOL, t);
        if (!e) return NULL;
        e->as.bool_lit.v = false;
        return e;
    }
    if (t->kind == TOK_LBRACK) {
        return parse_array_lit(p);
    }
    if (t->kind == TOK_LPAR) {
        Expr *lam = parse_lambda_arrow(p);
        if (lam) return lam;
        eat(p, TOK_LPAR);
        Expr *x = parse_expr(p, 0);
        if (!p->ok) return NULL;
        if (at(p, TOK_COMMA)) {
            PtrVec items = {0};
            ptrvec_push(p, &items, x);
            while (maybe(p, TOK_COMMA)) {
                Expr *item = parse_expr(p, 0);
                if (!p->ok) return NULL;
                ptrvec_push(p, &items, item);
            }
            eat(p, TOK_RPAR);
            Expr *e = new_expr(p, EXPR_TUPLE, t);
            if (!e) return NULL;
            e->as.tuple_lit.items = (Expr **)ptrvec_finalize(p, &items);
            e->as.tuple_lit.items_len = items.len;
            return e;
        }
        eat(p, TOK_RPAR);
        Expr *e = new_expr(p, EXPR_PAREN, t);
        if (!e) return NULL;
        e->as.paren.x = x;
        return e;
    }
    parser_set_error(p, t, "unexpected token %s in expression", tok_kind_desc(t->kind));
    return NULL;
}

static Expr *parse_match(Parser *p) {
    Tok *t = eat(p, TOK_KW_match);
    if (!p->ok) return NULL;
    Expr *scrut = parse_expr(p, 0);
    PtrVec arms = {0};
    if (at(p, TOK_COLON)) {
        eat(p, TOK_COLON);
        MatchArm *arm = parse_match_arm(p);
        if (!p->ok) return NULL;
        ptrvec_push(p, &arms, arm);
        while (maybe(p, TOK_COMMA)) {
            MatchArm *next = parse_match_arm(p);
            if (!p->ok) return NULL;
            ptrvec_push(p, &arms, next);
        }
        Expr *e = new_expr(p, EXPR_MATCH, t);
        if (!e) return NULL;
        e->as.match_expr.scrut = scrut;
        e->as.match_expr.arms = (MatchArm **)ptrvec_finalize(p, &arms);
        e->as.match_expr.arms_len = arms.len;
        return e;
    }
    eat(p, TOK_LBRACE);
    skip_semi(p);
    while (!at(p, TOK_RBRACE) && p->ok) {
        MatchArm *arm = parse_match_arm(p);
        if (!p->ok) return NULL;
        ptrvec_push(p, &arms, arm);
        skip_semi(p);
    }
    eat(p, TOK_RBRACE);
    Expr *e = new_expr(p, EXPR_MATCH, t);
    if (!e) return NULL;
    e->as.match_expr.scrut = scrut;
    e->as.match_expr.arms = (MatchArm **)ptrvec_finalize(p, &arms);
    e->as.match_expr.arms_len = arms.len;
    return e;
}

static MatchArm *parse_match_arm(Parser *p) {
    Pat *pat = parse_pattern(p);
    eat(p, TOK_ARROW);
    Expr *expr = parse_expr(p, 0);
    MatchArm *arm = (MatchArm *)ast_alloc(p->arena, sizeof(MatchArm));
    if (!arm) {
        parser_set_oom(p);
        return NULL;
    }
    arm->pat = pat;
    arm->expr = expr;
    return arm;
}

static Pat *parse_pattern(Parser *p) {
    Tok *t = peek(p, 0);
    if (t->kind == TOK_INT) {
        eat(p, TOK_INT);
        Pat *pat = new_pat(p, PAT_INT, t);
        if (!pat) return NULL;
        pat->as.i = t->val.i;
        return pat;
    }
    if (t->kind == TOK_STR) {
        eat(p, TOK_STR);
        if (!normalize_string_parts(p, t)) return NULL;
        Pat *pat = new_pat(p, PAT_STR, t);
        if (!pat) return NULL;
        pat->as.str = t->val.str;
        return pat;
    }
    if (t->kind == TOK_KW_true) {
        eat(p, TOK_KW_true);
        Pat *pat = new_pat(p, PAT_BOOL, t);
        if (!pat) return NULL;
        pat->as.b = true;
        return pat;
    }
    if (t->kind == TOK_KW_false) {
        eat(p, TOK_KW_false);
        Pat *pat = new_pat(p, PAT_BOOL, t);
        if (!pat) return NULL;
        pat->as.b = false;
        return pat;
    }
    if (t->kind == TOK_KW_null) {
        eat(p, TOK_KW_null);
        Pat *pat = new_pat(p, PAT_NULL, t);
        return pat;
    }
    if (t->kind == TOK_IDENT) {
        Tok *name_tok = eat(p, TOK_IDENT);
        if (str_eq_c(name_tok->val.ident, "_")) {
            Pat *pat = new_pat(p, PAT_WILD, t);
            return pat;
        }
        Pat *pat = new_pat(p, PAT_IDENT, t);
        if (!pat) return NULL;
        pat->as.name = name_tok->val.ident;
        return pat;
    }
    parser_set_error(p, t, "unexpected token %s in pattern", tok_kind_desc(t->kind));
    return NULL;
}

static Expr *parse_lambda(Parser *p) {
    Tok *t = eat(p, TOK_BAR);
    PtrVec params = {0};
    if (!at(p, TOK_BAR)) {
        while (true) {
            bool is_mut = maybe(p, TOK_QMARK) != NULL;
            Tok *name_tok = eat(p, TOK_IDENT);
            Param *param = (Param *)ast_alloc(p->arena, sizeof(Param));
            if (!param) {
                parser_set_oom(p);
                return NULL;
            }
            param->name = name_tok->val.ident;
            param->is_mut = is_mut;
            param->is_this = false;
            param->typ = NULL;
            if (maybe(p, TOK_EQ)) {
                param->typ = parse_type(p);
            }
            if (!p->ok) return NULL;
            ptrvec_push(p, &params, param);
            if (!maybe(p, TOK_COMMA)) {
                break;
            }
        }
    }
    eat(p, TOK_BAR);
    Expr *body = parse_expr(p, 0);
    Expr *lam = new_expr(p, EXPR_LAMBDA, t);
    if (!lam) return NULL;
    lam->as.lambda.params = (Param **)ptrvec_finalize(p, &params);
    lam->as.lambda.params_len = params.len;
    lam->as.lambda.body = body;
    return lam;
}

static Expr *parse_lambda_arrow(Parser *p) {
    Parser probe = *p;
    probe.err = NULL;

    Tok *t = eat(&probe, TOK_LPAR);
    if (!probe.ok) return NULL;

    PtrVec params = {0};
    if (!at(&probe, TOK_RPAR)) {
        while (true) {
            bool is_mut = maybe(&probe, TOK_QMARK) != NULL;
            Tok *name_tok = eat(&probe, TOK_IDENT);
            if (!probe.ok) return NULL;
            Param *param = (Param *)ast_alloc(probe.arena, sizeof(Param));
            if (!param) {
                parser_set_oom(&probe);
                return NULL;
            }
            param->name = name_tok->val.ident;
            param->is_mut = is_mut;
            param->is_this = false;
            param->typ = NULL;
            if (maybe(&probe, TOK_EQ)) {
                param->typ = parse_type(&probe);
            }
            if (!probe.ok) return NULL;
            ptrvec_push(&probe, &params, param);
            if (!maybe(&probe, TOK_COMMA)) {
                break;
            }
        }
    }
    eat(&probe, TOK_RPAR);
    if (!probe.ok) return NULL;
    if (!at(&probe, TOK_ARROW)) return NULL;
    eat(&probe, TOK_ARROW);

    Expr *body = NULL;
    if (at(&probe, TOK_LBRACE)) {
        Stmt *block = parse_block(&probe);
        if (!probe.ok) return NULL;
        Expr *be = new_expr(&probe, EXPR_BLOCK, t);
        if (!be) return NULL;
        be->as.block_expr.block = block;
        body = be;
    } else {
        body = parse_expr(&probe, 0);
        if (!probe.ok) return NULL;
    }

    Expr *lam = new_expr(&probe, EXPR_LAMBDA, t);
    if (!lam) return NULL;
    lam->as.lambda.params = (Param **)ptrvec_finalize(&probe, &params);
    lam->as.lambda.params_len = params.len;
    lam->as.lambda.body = body;

    // Copy probe back to main parser, but preserve the original err pointer
    Diag *original_err = p->err;
    *p = probe;
    p->err = original_err;
    return lam;
}

static Expr *parse_new(Parser *p) {
    Tok *t = eat(p, TOK_KW_new);
    Tok *name_tok = eat(p, TOK_IDENT);
    Str name = name_tok->val.ident;
    if (maybe(p, TOK_DOT)) {
        Tok *ext = eat(p, TOK_IDENT);
        name = str_concat(p, name, ".", ext->val.ident);
    }
    size_t args_len = 0;
    Expr **args = NULL;
    Str *arg_names = NULL;
    if (at(p, TOK_LPAR)) {
        CallArgs ca = parse_call_args(p);
        args = ca.args;
        args_len = ca.len;
        arg_names = ca.names;
    }
    Expr *e = new_expr(p, EXPR_NEW, t);
    if (!e) return NULL;
    e->as.new_expr.name = name;
    e->as.new_expr.args = args;
    e->as.new_expr.args_len = args_len;
    e->as.new_expr.arg_names = arg_names;
    return e;
}

static Expr *parse_array_lit(Parser *p) {
    Tok *t = eat(p, TOK_LBRACK);
    PtrVec items = {0};
    PtrVec keys = {0};
    PtrVec vals = {0};
    bool is_dict = false;
    if (!at(p, TOK_RBRACK)) {
        Expr *first = parse_expr(p, 0);
        if (!p->ok) return NULL;
        if (at(p, TOK_ARROW)) {
            is_dict = true;
            eat(p, TOK_ARROW);
            Expr *val = parse_expr(p, 0);
            if (!p->ok) return NULL;
            ptrvec_push(p, &keys, first);
            ptrvec_push(p, &vals, val);
            while (maybe(p, TOK_COMMA)) {
                Expr *k = parse_expr(p, 0);
                if (!p->ok) return NULL;
                eat(p, TOK_ARROW);
                if (!p->ok) return NULL;
                Expr *v = parse_expr(p, 0);
                if (!p->ok) return NULL;
                ptrvec_push(p, &keys, k);
                ptrvec_push(p, &vals, v);
            }
        } else {
            ptrvec_push(p, &items, first);
            while (maybe(p, TOK_COMMA)) {
                Expr *item = parse_expr(p, 0);
                if (!p->ok) return NULL;
                ptrvec_push(p, &items, item);
            }
        }
    }
    eat(p, TOK_RBRACK);
    if (is_dict) {
        size_t n = keys.len;
        Expr *e = new_expr(p, EXPR_DICT, t);
        if (!e) return NULL;
        e->as.dict_lit.keys = (Expr **)ptrvec_finalize(p, &keys);
        e->as.dict_lit.vals = (Expr **)ptrvec_finalize(p, &vals);
        e->as.dict_lit.pairs_len = n;
        e->as.dict_lit.annot = NULL;
        if (at(p, TOK_COLON)) {
            eat(p, TOK_COLON);
            e->as.dict_lit.annot = parse_type(p);
        }
        return e;
    }
    if (items.len == 0 && at(p, TOK_COLON)) {
        eat(p, TOK_COLON);
        TypeRef *annot = parse_type(p);
        if (!p->ok) return NULL;
        if (annot && annot->kind == TYPE_DICT) {
            Expr *e = new_expr(p, EXPR_DICT, t);
            if (!e) return NULL;
            e->as.dict_lit.keys = NULL;
            e->as.dict_lit.vals = NULL;
            e->as.dict_lit.pairs_len = 0;
            e->as.dict_lit.annot = annot;
            return e;
        }
        Expr *e = new_expr(p, EXPR_ARRAY, t);
        if (!e) return NULL;
        e->as.array_lit.items = NULL;
        e->as.array_lit.items_len = 0;
        e->as.array_lit.annot = annot;
        return e;
    }
    Expr *e = new_expr(p, EXPR_ARRAY, t);
    if (!e) return NULL;
    e->as.array_lit.items = (Expr **)ptrvec_finalize(p, &items);
    e->as.array_lit.items_len = items.len;
    e->as.array_lit.annot = NULL;
    if (at(p, TOK_COLON)) {
        eat(p, TOK_COLON);
        e->as.array_lit.annot = parse_type(p);
    }
    return e;
}

Module *parse_cask(Tok *toks, size_t len, const char *path, Arena *arena, Diag *err) {
    Parser p;
    p.toks = toks;
    p.len = len;
    p.i = 0;
    p.path = path ? path : "";
    p.arena = arena;
    p.err = err;
    p.ok = true;

    PtrVec imports = {0};
    PtrVec decls = {0};
    Str declared_name = {0};
    bool has_declared_name = false;

    skip_semi(&p);
    if (at(&p, TOK_KW_cask)) {
        eat(&p, TOK_KW_cask);
        Tok *name_tok = eat(&p, TOK_IDENT);
        if (!p.ok) return NULL;
        declared_name = name_tok->val.ident;
        has_declared_name = true;
        skip_semi(&p);
    }
    while (!at(&p, TOK_EOF) && p.ok) {
        if (at(&p, TOK_KW_bring)) {
            Import *imp = parse_import(&p);
            if (!p.ok) return NULL;
            ptrvec_push(&p, &imports, imp);
        } else if (at(&p, TOK_KW_entry)) {
            Decl *decl = parse_entry(&p);
            if (!p.ok) return NULL;
            ptrvec_push(&p, &decls, decl);
        } else if (at(&p, TOK_COLON)) {
            eat(&p, TOK_COLON);
            Decl *decl = parse_fun(&p, false);
            if (!p.ok) return NULL;
            ptrvec_push(&p, &decls, decl);
        } else if (at(&p, TOK_KW_macro)) {
            Decl *decl = parse_macro(&p);
            if (!p.ok) return NULL;
            ptrvec_push(&p, &decls, decl);
        } else if (at(&p, TOK_KW_const)) {
            Decl *decl = parse_const_decl(&p, false);
            if (!p.ok) return NULL;
            ptrvec_push(&p, &decls, decl);
        } else if (at(&p, TOK_KW_def)) {
            Decl *decl = parse_def_decl(&p, false);
            if (!p.ok) return NULL;
            ptrvec_push(&p, &decls, decl);
        } else if (at(&p, TOK_COLONCOLON)) {
            eat(&p, TOK_COLONCOLON);
            Decl *decl = parse_fun(&p, true);
            if (!p.ok) return NULL;
            ptrvec_push(&p, &decls, decl);
        } else if (at(&p, TOK_KW_pub) && peek(&p, 1)->kind == TOK_KW_const) {
            eat(&p, TOK_KW_pub);
            Decl *decl = parse_const_decl(&p, true);
            if (!p.ok) return NULL;
            ptrvec_push(&p, &decls, decl);
        } else if (at(&p, TOK_KW_pub) && peek(&p, 1)->kind == TOK_KW_def) {
            eat(&p, TOK_KW_pub);
            Decl *decl = parse_def_decl(&p, true);
            if (!p.ok) return NULL;
            ptrvec_push(&p, &decls, decl);
        } else if (at(&p, TOK_KW_pub) || at(&p, TOK_KW_lock) || at(&p, TOK_KW_seal) ||
                   at(&p, TOK_KW_class) || at(&p, TOK_KW_struct) || at(&p, TOK_KW_enum)) {
            Decl *decl = parse_nominal(&p);
            if (!p.ok) return NULL;
            ptrvec_push(&p, &decls, decl);
        } else {
            Tok *t = peek(&p, 0);
            parser_set_error(&p, t, "unexpected token %s", tok_kind_desc(t->kind));
            return NULL;
        }
        skip_semi(&p);
    }

    Module *mod = (Module *)ast_alloc(arena, sizeof(Module));
    if (!mod) {
        parser_set_oom(&p);
        return NULL;
    }
    Str pstr = {0};
    pstr.data = path ? path : "";
    pstr.len = path ? strlen(path) : 0;
    mod->path = pstr;
    mod->declared_name = declared_name;
    mod->has_declared_name = has_declared_name;
    mod->imports = (Import **)ptrvec_finalize(&p, &imports);
    mod->imports_len = imports.len;
    mod->decls = (Decl **)ptrvec_finalize(&p, &decls);
    mod->decls_len = decls.len;
    return mod;
}
