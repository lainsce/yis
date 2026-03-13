#include "typecheck.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "str.h"

static void set_err(Diag *err, const char *msg) {
    if (!err) {
        return;
    }
    err->path = NULL;
    err->line = 0;
    err->col = 0;
    err->message = msg;
}

static void *arena_array(Arena *arena, size_t count, size_t size) {
    if (count == 0) {
        return NULL;
    }
    return arena_alloc(arena, count * size);
}

static Expr *lower_expr(Arena *arena, Expr *e, Diag *err);
static Stmt *lower_stmt(Arena *arena, Stmt *s, Diag *err);
static Str cask_name_for_path(Arena *arena, Str path);
static Str normalize_import_name(Arena *arena, Str name);

static Expr **lower_expr_list(Arena *arena, Expr **items, size_t len, Diag *err) {
    if (len == 0) {
        return NULL;
    }
    Expr **out = (Expr **)arena_array(arena, len, sizeof(Expr *));
    if (!out) {
        set_err(err, "out of memory");
        return NULL;
    }
    for (size_t i = 0; i < len; i++) {
        out[i] = lower_expr(arena, items[i], err);
        if (!out[i] && err && err->message) {
            return NULL;
        }
    }
    return out;
}

typedef struct {
    Str cask;
    Str name;
    MacroDecl *decl;
} LowerMacro;

typedef struct {
    Str cask_name;
    Str cask_path;
    Str *imports;
    size_t imports_len;
    LowerMacro *macros;
    size_t macros_len;
    int macro_depth;
} LowerCtx;

typedef struct {
    Str name;
    Str path;
    Str *imports;
    size_t imports_len;
} LowerModule;

static LowerCtx *g_lower_ctx = NULL;

static bool is_ident_name(Expr *e, const char *name) {
    if (!e || e->kind != EXPR_IDENT) {
        return false;
    }
    return str_eq_c(e->as.ident.name, name);
}

static Expr *new_expr_like(Arena *arena, Expr *src, ExprKind kind, Diag *err) {
    Expr *e = (Expr *)ast_alloc(arena, sizeof(Expr));
    if (!e) {
        set_err(err, "out of memory");
        return NULL;
    }
    e->kind = kind;
    e->line = src ? src->line : 0;
    e->col = src ? src->col : 0;
    return e;
}

static Stmt *new_stmt_like(Arena *arena, Stmt *src, StmtKind kind, Diag *err) {
    Stmt *s = (Stmt *)ast_alloc(arena, sizeof(Stmt));
    if (!s) {
        set_err(err, "out of memory");
        return NULL;
    }
    s->kind = kind;
    s->line = src ? src->line : 0;
    s->col = src ? src->col : 0;
    return s;
}

static Decl *new_decl_like(Arena *arena, Decl *src, DeclKind kind, Diag *err) {
    Decl *d = (Decl *)ast_alloc(arena, sizeof(Decl));
    if (!d) {
        set_err(err, "out of memory");
        return NULL;
    }
    d->kind = kind;
    d->line = src ? src->line : 0;
    d->col = src ? src->col : 0;
    return d;
}

typedef struct {
    Str name;
    Expr *expr;
} MacroBind;

static Expr *clone_expr_with_binds(Arena *arena, Expr *e, MacroBind *binds, size_t binds_len, Diag *err);
static Stmt *clone_stmt_with_binds(Arena *arena, Stmt *s, MacroBind *binds, size_t binds_len, Diag *err);

static Expr *find_bind_expr(MacroBind *binds, size_t binds_len, Str name) {
    for (size_t i = 0; i < binds_len; i++) {
        if (str_eq(binds[i].name, name)) return binds[i].expr;
    }
    return NULL;
}

static LowerMacro *find_macro_in_cask(LowerCtx *ctx, Str cask, Str name) {
    if (!ctx) return NULL;
    for (size_t i = 0; i < ctx->macros_len; i++) {
        LowerMacro *m = &ctx->macros[i];
        if (str_eq(m->cask, cask) && str_eq(m->name, name)) {
            return m;
        }
    }
    return NULL;
}

static LowerMacro *resolve_macro_for_call(LowerCtx *ctx, Str name) {
    if (!ctx) return NULL;
    LowerMacro *local = find_macro_in_cask(ctx, ctx->cask_name, name);
    if (local) return local;
    for (size_t i = 0; i < ctx->imports_len; i++) {
        LowerMacro *m = find_macro_in_cask(ctx, ctx->imports[i], name);
        if (m) return m;
    }
    return NULL;
}

static bool is_module_ident(LowerCtx *ctx, Expr *e) {
    if (!ctx || !e || e->kind != EXPR_IDENT) return false;
    if (str_eq(e->as.ident.name, ctx->cask_name)) return true;
    for (size_t i = 0; i < ctx->imports_len; i++) {
        if (str_eq(e->as.ident.name, ctx->imports[i])) return true;
    }
    return false;
}

static Expr *macro_body_expr(MacroDecl *md) {
    if (!md || !md->body) return NULL;
    Stmt *body = md->body;
    if (body->kind == STMT_BLOCK) {
        if (body->as.block_s.stmts_len != 1) return NULL;
        body = body->as.block_s.stmts[0];
    }
    if (!body) return NULL;
    if (body->kind == STMT_EXPR) return body->as.expr_s.expr;
    if (body->kind == STMT_RETURN) return body->as.ret_s.expr;
    return NULL;
}

static StrParts *clone_str_parts_with_binds(Arena *arena, StrParts *src, MacroBind *binds, size_t binds_len, Diag *err) {
    if (!src) return NULL;
    StrParts *out = (StrParts *)ast_alloc(arena, sizeof(StrParts));
    if (!out) {
        set_err(err, "out of memory");
        return NULL;
    }
    out->len = src->len;
    out->parts = src->len ? (StrPart *)arena_array(arena, src->len, sizeof(StrPart)) : NULL;
    if (src->len && !out->parts) {
        set_err(err, "out of memory");
        return NULL;
    }
    for (size_t i = 0; i < src->len; i++) {
        out->parts[i] = src->parts[i];
        if (src->parts[i].kind == STR_PART_EXPR && src->parts[i].as.expr) {
            out->parts[i].as.expr = clone_expr_with_binds(arena, src->parts[i].as.expr, binds, binds_len, err);
        }
    }
    return out;
}

static Expr **clone_expr_list_with_binds(Arena *arena, Expr **items, size_t len, MacroBind *binds, size_t binds_len, Diag *err) {
    if (len == 0) return NULL;
    Expr **out = (Expr **)arena_array(arena, len, sizeof(Expr *));
    if (!out) {
        set_err(err, "out of memory");
        return NULL;
    }
    for (size_t i = 0; i < len; i++) {
        out[i] = clone_expr_with_binds(arena, items[i], binds, binds_len, err);
    }
    return out;
}

static Stmt **clone_stmt_list_with_binds(Arena *arena, Stmt **items, size_t len, MacroBind *binds, size_t binds_len, Diag *err) {
    if (len == 0) return NULL;
    Stmt **out = (Stmt **)arena_array(arena, len, sizeof(Stmt *));
    if (!out) {
        set_err(err, "out of memory");
        return NULL;
    }
    for (size_t i = 0; i < len; i++) {
        out[i] = clone_stmt_with_binds(arena, items[i], binds, binds_len, err);
    }
    return out;
}

static Expr *clone_expr_with_binds(Arena *arena, Expr *e, MacroBind *binds, size_t binds_len, Diag *err) {
    if (!e) return NULL;
    if (e->kind == EXPR_IDENT) {
        Expr *bound = find_bind_expr(binds, binds_len, e->as.ident.name);
        if (bound) return clone_expr_with_binds(arena, bound, NULL, 0, err);
    }
    Expr *out = new_expr_like(arena, e, e->kind, err);
    if (!out) return NULL;
    switch (e->kind) {
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_IDENT:
        case EXPR_NULL:
        case EXPR_BOOL:
            out->as = e->as;
            return out;
        case EXPR_STR:
            out->as.str_lit.parts = clone_str_parts_with_binds(arena, e->as.str_lit.parts, binds, binds_len, err);
            return out;
        case EXPR_UNARY:
            out->as.unary.op = e->as.unary.op;
            out->as.unary.x = clone_expr_with_binds(arena, e->as.unary.x, binds, binds_len, err);
            return out;
        case EXPR_BINARY:
            out->as.binary.op = e->as.binary.op;
            out->as.binary.a = clone_expr_with_binds(arena, e->as.binary.a, binds, binds_len, err);
            out->as.binary.b = clone_expr_with_binds(arena, e->as.binary.b, binds, binds_len, err);
            return out;
        case EXPR_ASSIGN:
            out->as.assign.op = e->as.assign.op;
            out->as.assign.target = clone_expr_with_binds(arena, e->as.assign.target, binds, binds_len, err);
            out->as.assign.value = clone_expr_with_binds(arena, e->as.assign.value, binds, binds_len, err);
            return out;
        case EXPR_CALL:
            out->as.call.fn = clone_expr_with_binds(arena, e->as.call.fn, binds, binds_len, err);
            out->as.call.args_len = e->as.call.args_len;
            out->as.call.args = clone_expr_list_with_binds(arena, e->as.call.args, e->as.call.args_len, binds, binds_len, err);
            return out;
        case EXPR_INDEX:
            out->as.index.a = clone_expr_with_binds(arena, e->as.index.a, binds, binds_len, err);
            out->as.index.i = clone_expr_with_binds(arena, e->as.index.i, binds, binds_len, err);
            return out;
        case EXPR_MEMBER:
            out->as.member.a = clone_expr_with_binds(arena, e->as.member.a, binds, binds_len, err);
            out->as.member.name = e->as.member.name;
            return out;
        case EXPR_PAREN:
            out->as.paren.x = clone_expr_with_binds(arena, e->as.paren.x, binds, binds_len, err);
            return out;
        case EXPR_ARRAY:
            out->as.array_lit.items_len = e->as.array_lit.items_len;
            out->as.array_lit.items = clone_expr_list_with_binds(arena, e->as.array_lit.items, e->as.array_lit.items_len, binds, binds_len, err);
            out->as.array_lit.annot = e->as.array_lit.annot;
            return out;
        case EXPR_DICT: {
            out->as.dict_lit.pairs_len = e->as.dict_lit.pairs_len;
            out->as.dict_lit.keys = clone_expr_list_with_binds(arena, e->as.dict_lit.keys, e->as.dict_lit.pairs_len, binds, binds_len, err);
            out->as.dict_lit.vals = clone_expr_list_with_binds(arena, e->as.dict_lit.vals, e->as.dict_lit.pairs_len, binds, binds_len, err);
            out->as.dict_lit.annot = e->as.dict_lit.annot;
            return out;
        }
        case EXPR_TUPLE:
            out->as.tuple_lit.items_len = e->as.tuple_lit.items_len;
            out->as.tuple_lit.items = clone_expr_list_with_binds(arena, e->as.tuple_lit.items, e->as.tuple_lit.items_len, binds, binds_len, err);
            return out;
        case EXPR_MATCH: {
            out->as.match_expr.scrut = clone_expr_with_binds(arena, e->as.match_expr.scrut, binds, binds_len, err);
            out->as.match_expr.arms_len = e->as.match_expr.arms_len;
            out->as.match_expr.arms = e->as.match_expr.arms_len ? (MatchArm **)arena_array(arena, e->as.match_expr.arms_len, sizeof(MatchArm *)) : NULL;
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                MatchArm *src = e->as.match_expr.arms[i];
                MatchArm *arm = (MatchArm *)ast_alloc(arena, sizeof(MatchArm));
                if (!arm) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                arm->pat = src->pat;
                arm->expr = clone_expr_with_binds(arena, src->expr, binds, binds_len, err);
                out->as.match_expr.arms[i] = arm;
            }
            return out;
        }
        case EXPR_LAMBDA:
            out->as.lambda = e->as.lambda;
            out->as.lambda.body = clone_expr_with_binds(arena, e->as.lambda.body, binds, binds_len, err);
            return out;
        case EXPR_BLOCK:
            out->as.block_expr.block = clone_stmt_with_binds(arena, e->as.block_expr.block, binds, binds_len, err);
            return out;
        case EXPR_NEW:
            out->as.new_expr = e->as.new_expr;
            out->as.new_expr.args = clone_expr_list_with_binds(arena, e->as.new_expr.args, e->as.new_expr.args_len, binds, binds_len, err);
            return out;
        case EXPR_IF:
            out->as.if_expr.arms_len = e->as.if_expr.arms_len;
            out->as.if_expr.arms = e->as.if_expr.arms_len ? (ExprIfArm **)arena_array(arena, e->as.if_expr.arms_len, sizeof(ExprIfArm *)) : NULL;
            for (size_t i = 0; i < e->as.if_expr.arms_len; i++) {
                ExprIfArm *src = e->as.if_expr.arms[i];
                ExprIfArm *arm = (ExprIfArm *)ast_alloc(arena, sizeof(ExprIfArm));
                if (!arm) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                arm->cond = src->cond ? clone_expr_with_binds(arena, src->cond, binds, binds_len, err) : NULL;
                arm->value = clone_expr_with_binds(arena, src->value, binds, binds_len, err);
                out->as.if_expr.arms[i] = arm;
            }
            return out;
        case EXPR_TERNARY:
            out->as.ternary.cond = clone_expr_with_binds(arena, e->as.ternary.cond, binds, binds_len, err);
            out->as.ternary.then_expr = clone_expr_with_binds(arena, e->as.ternary.then_expr, binds, binds_len, err);
            out->as.ternary.else_expr = clone_expr_with_binds(arena, e->as.ternary.else_expr, binds, binds_len, err);
            return out;
        case EXPR_MOVE:
            out->as.move.x = clone_expr_with_binds(arena, e->as.move.x, binds, binds_len, err);
            return out;
        default:
            out->as = e->as;
            return out;
    }
}

static Stmt *clone_stmt_with_binds(Arena *arena, Stmt *s, MacroBind *binds, size_t binds_len, Diag *err) {
    if (!s) return NULL;
    Stmt *out = new_stmt_like(arena, s, s->kind, err);
    if (!out) return NULL;
    switch (s->kind) {
        case STMT_LET:
            out->as.let_s = s->as.let_s;
            out->as.let_s.expr = clone_expr_with_binds(arena, s->as.let_s.expr, binds, binds_len, err);
            return out;
        case STMT_CONST:
            out->as.const_s = s->as.const_s;
            out->as.const_s.expr = clone_expr_with_binds(arena, s->as.const_s.expr, binds, binds_len, err);
            return out;
        case STMT_IF: {
            out->as.if_s.arms_len = s->as.if_s.arms_len;
            out->as.if_s.arms = s->as.if_s.arms_len ? (IfArm **)arena_array(arena, s->as.if_s.arms_len, sizeof(IfArm *)) : NULL;
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                IfArm *src = s->as.if_s.arms[i];
                IfArm *arm = (IfArm *)ast_alloc(arena, sizeof(IfArm));
                if (!arm) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                arm->cond = src->cond ? clone_expr_with_binds(arena, src->cond, binds, binds_len, err) : NULL;
                arm->body = clone_stmt_with_binds(arena, src->body, binds, binds_len, err);
                out->as.if_s.arms[i] = arm;
            }
            return out;
        }
        case STMT_FOR:
            out->as.for_s.init = s->as.for_s.init ? clone_stmt_with_binds(arena, s->as.for_s.init, binds, binds_len, err) : NULL;
            out->as.for_s.cond = s->as.for_s.cond ? clone_expr_with_binds(arena, s->as.for_s.cond, binds, binds_len, err) : NULL;
            out->as.for_s.step = s->as.for_s.step ? clone_expr_with_binds(arena, s->as.for_s.step, binds, binds_len, err) : NULL;
            out->as.for_s.body = clone_stmt_with_binds(arena, s->as.for_s.body, binds, binds_len, err);
            return out;
        case STMT_FOREACH:
            out->as.foreach_s = s->as.foreach_s;
            out->as.foreach_s.expr = clone_expr_with_binds(arena, s->as.foreach_s.expr, binds, binds_len, err);
            out->as.foreach_s.body = clone_stmt_with_binds(arena, s->as.foreach_s.body, binds, binds_len, err);
            return out;
        case STMT_BREAK:
        case STMT_CONTINUE:
            out->as = s->as;
            return out;
        case STMT_RETURN:
            out->as.ret_s.expr = s->as.ret_s.expr ? clone_expr_with_binds(arena, s->as.ret_s.expr, binds, binds_len, err) : NULL;
            return out;
        case STMT_EXPR:
            out->as.expr_s.expr = clone_expr_with_binds(arena, s->as.expr_s.expr, binds, binds_len, err);
            return out;
        case STMT_BLOCK:
            out->as.block_s.stmts_len = s->as.block_s.stmts_len;
            out->as.block_s.stmts = clone_stmt_list_with_binds(arena, s->as.block_s.stmts, s->as.block_s.stmts_len, binds, binds_len, err);
            return out;
        default:
            out->as = s->as;
            return out;
    }
}

static Expr *lower_expr(Arena *arena, Expr *e, Diag *err) {
    if (!e) {
        return NULL;
    }

    LowerCtx *lctx = g_lower_ctx;
    if (lctx && e->kind == EXPR_CALL && e->as.call.fn && e->as.call.fn->kind == EXPR_MEMBER) {
        Expr *recv = e->as.call.fn->as.member.a;
        Str mname = e->as.call.fn->as.member.name;
        if (!is_module_ident(lctx, recv)) {
            LowerMacro *macro = resolve_macro_for_call(lctx, mname);
            if (macro) {
                if (lctx->macro_depth > 64) {
                    set_err(err, "macro expansion exceeded max depth");
                    return NULL;
                }
                if (e->as.call.args_len != macro->decl->params_len) {
                    set_err(err, "macro call arity mismatch");
                    return NULL;
                }
                Expr *body_expr = macro_body_expr(macro->decl);
                if (!body_expr) {
                    set_err(err, "macro body must contain a single expression");
                    return NULL;
                }

                size_t binds_len = 1 + macro->decl->params_len;
                MacroBind *binds = (MacroBind *)arena_array(arena, binds_len, sizeof(MacroBind));
                if (!binds) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                binds[0].name = str_from_c("this");
                binds[0].expr = lower_expr(arena, recv, err);
                for (size_t i = 0; i < macro->decl->params_len; i++) {
                    binds[i + 1].name = macro->decl->params[i]->name;
                    binds[i + 1].expr = lower_expr(arena, e->as.call.args[i], err);
                }

                Expr *expanded = clone_expr_with_binds(arena, body_expr, binds, binds_len, err);
                if (!expanded) return NULL;

                int saved_depth = lctx->macro_depth;
                lctx->macro_depth = saved_depth + 1;
                Expr *out = lower_expr(arena, expanded, err);
                lctx->macro_depth = saved_depth;
                return out;
            }
        }
    }

    // Lower #x to stdr.len(x)
    if (e->kind == EXPR_UNARY && e->as.unary.op == TOK_HASH) {
        Expr *inner = lower_expr(arena, e->as.unary.x, err);
        if (!inner && err && err->message) {
            return NULL;
        }
        Expr *id = new_expr_like(arena, e, EXPR_IDENT, err);
        if (!id) return NULL;
        id->as.ident.name = str_from_c("stdr");
        Expr *mem = new_expr_like(arena, e, EXPR_MEMBER, err);
        if (!mem) return NULL;
        mem->as.member.a = id;
        mem->as.member.name = str_from_c("len");
        Expr *call = new_expr_like(arena, e, EXPR_CALL, err);
        if (!call) return NULL;
        call->as.call.fn = mem;
        call->as.call.args_len = 1;
        call->as.call.args = (Expr **)arena_array(arena, 1, sizeof(Expr *));
        if (!call->as.call.args) {
            set_err(err, "out of memory");
            return NULL;
        }
        call->as.call.args[0] = inner;
        return call;
    }

    // Lower stdr.writef/readf/str to writef/readf/str
    if (e->kind == EXPR_CALL) {
        Expr *fn = e->as.call.fn;
        if (fn && fn->kind == EXPR_MEMBER) {
            Expr *base = fn->as.member.a;
            if (base && base->kind == EXPR_IDENT && str_eq_c(base->as.ident.name, "stdr")) {
                if (str_eq_c(fn->as.member.name, "writef") ||
                    str_eq_c(fn->as.member.name, "readf") ||
                    str_eq_c(fn->as.member.name, "str")) {
                    Expr *id = new_expr_like(arena, fn, EXPR_IDENT, err);
                    if (!id) return NULL;
                    id->as.ident.name = fn->as.member.name;
                    Expr *call = new_expr_like(arena, e, EXPR_CALL, err);
                    if (!call) return NULL;
                    call->as.call.fn = id;
                    call->as.call.args_len = e->as.call.args_len;
                    call->as.call.args = lower_expr_list(arena, e->as.call.args, e->as.call.args_len, err);
                    return call;
                }
            }
        }
    }

    // Lower writef/readf varargs into writef/readf(fmt, (args...))
    if (e->kind == EXPR_CALL && e->as.call.fn && e->as.call.fn->kind == EXPR_IDENT) {
        Str fname = e->as.call.fn->as.ident.name;
        bool is_writef = str_eq_c(fname, "writef");
        bool is_readf = str_eq_c(fname, "readf");
        if (is_writef || is_readf) {
            size_t argc = e->as.call.args_len;
            if (argc == 0) {
                Expr *call = new_expr_like(arena, e, EXPR_CALL, err);
                if (!call) return NULL;
                call->as.call.fn = lower_expr(arena, e->as.call.fn, err);
                call->as.call.args_len = 0;
                call->as.call.args = NULL;
                return call;
            }
            if (argc == 2 && e->as.call.args[1] && e->as.call.args[1]->kind == EXPR_TUPLE) {
                Expr *call = new_expr_like(arena, e, EXPR_CALL, err);
                if (!call) return NULL;
                call->as.call.fn = lower_expr(arena, e->as.call.fn, err);
                call->as.call.args_len = 2;
                call->as.call.args = (Expr **)arena_array(arena, 2, sizeof(Expr *));
                if (!call->as.call.args) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                call->as.call.args[0] = lower_expr(arena, e->as.call.args[0], err);
                call->as.call.args[1] = lower_expr(arena, e->as.call.args[1], err);
                return call;
            }
            Expr *fmt = lower_expr(arena, e->as.call.args[0], err);
            if (!fmt && err && err->message) return NULL;
            size_t rest_len = argc > 1 ? argc - 1 : 0;
            Expr **rest_items = NULL;
            if (rest_len > 0) {
                rest_items = (Expr **)arena_array(arena, rest_len, sizeof(Expr *));
                if (!rest_items) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                for (size_t i = 0; i < rest_len; i++) {
                    rest_items[i] = lower_expr(arena, e->as.call.args[i + 1], err);
                    if (!rest_items[i] && err && err->message) return NULL;
                }
            }
            Expr *tuple = new_expr_like(arena, e, EXPR_TUPLE, err);
            if (!tuple) return NULL;
            tuple->as.tuple_lit.items = rest_items;
            tuple->as.tuple_lit.items_len = rest_len;
            Expr *call = new_expr_like(arena, e, EXPR_CALL, err);
            if (!call) return NULL;
            call->as.call.fn = lower_expr(arena, e->as.call.fn, err);
            call->as.call.args_len = 2;
            call->as.call.args = (Expr **)arena_array(arena, 2, sizeof(Expr *));
            if (!call->as.call.args) {
                set_err(err, "out of memory");
                return NULL;
            }
            call->as.call.args[0] = fmt;
            call->as.call.args[1] = tuple;
            return call;
        }
    }

    // Lower move(x) to MoveExpr(x)
    if (e->kind == EXPR_CALL && e->as.call.fn && is_ident_name(e->as.call.fn, "move") && e->as.call.args_len == 1) {
        Expr *inner = lower_expr(arena, e->as.call.args[0], err);
        if (!inner && err && err->message) return NULL;
        Expr *mv = new_expr_like(arena, e, EXPR_MOVE, err);
        if (!mv) return NULL;
        mv->as.move.x = inner;
        return mv;
    }

    // Recurse
    switch (e->kind) {
        case EXPR_UNARY: {
            Expr *x = lower_expr(arena, e->as.unary.x, err);
            Expr *u = new_expr_like(arena, e, EXPR_UNARY, err);
            if (!u) return NULL;
            u->as.unary.op = e->as.unary.op;
            u->as.unary.x = x;
            return u;
        }
        case EXPR_BINARY: {
            Expr *a = lower_expr(arena, e->as.binary.a, err);
            Expr *b = lower_expr(arena, e->as.binary.b, err);
            Expr *bin = new_expr_like(arena, e, EXPR_BINARY, err);
            if (!bin) return NULL;
            bin->as.binary.op = e->as.binary.op;
            bin->as.binary.a = a;
            bin->as.binary.b = b;
            return bin;
        }
        case EXPR_ASSIGN: {
            Expr *t = lower_expr(arena, e->as.assign.target, err);
            Expr *v = lower_expr(arena, e->as.assign.value, err);
            Expr *a = new_expr_like(arena, e, EXPR_ASSIGN, err);
            if (!a) return NULL;
            a->as.assign.op = e->as.assign.op;
            a->as.assign.target = t;
            a->as.assign.value = v;
            return a;
        }
        case EXPR_CALL: {
            Expr *fn = lower_expr(arena, e->as.call.fn, err);
            Expr **args = lower_expr_list(arena, e->as.call.args, e->as.call.args_len, err);
            Expr *call = new_expr_like(arena, e, EXPR_CALL, err);
            if (!call) return NULL;
            call->as.call.fn = fn;
            call->as.call.args = args;
            call->as.call.args_len = e->as.call.args_len;
            return call;
        }
        case EXPR_INDEX: {
            Expr *a = lower_expr(arena, e->as.index.a, err);
            Expr *i = lower_expr(arena, e->as.index.i, err);
            Expr *ix = new_expr_like(arena, e, EXPR_INDEX, err);
            if (!ix) return NULL;
            ix->as.index.a = a;
            ix->as.index.i = i;
            return ix;
        }
        case EXPR_MEMBER: {
            Expr *a = lower_expr(arena, e->as.member.a, err);
            Expr *m = new_expr_like(arena, e, EXPR_MEMBER, err);
            if (!m) return NULL;
            m->as.member.a = a;
            m->as.member.name = e->as.member.name;
            return m;
        }
        case EXPR_PAREN: {
            Expr *x = lower_expr(arena, e->as.paren.x, err);
            Expr *p = new_expr_like(arena, e, EXPR_PAREN, err);
            if (!p) return NULL;
            p->as.paren.x = x;
            return p;
        }
        case EXPR_ARRAY: {
            Expr **items = lower_expr_list(arena, e->as.array_lit.items, e->as.array_lit.items_len, err);
            Expr *arr = new_expr_like(arena, e, EXPR_ARRAY, err);
            if (!arr) return NULL;
            arr->as.array_lit.items = items;
            arr->as.array_lit.items_len = e->as.array_lit.items_len;
            arr->as.array_lit.annot = e->as.array_lit.annot;
            return arr;
        }
        case EXPR_DICT: {
            Expr **keys = lower_expr_list(arena, e->as.dict_lit.keys, e->as.dict_lit.pairs_len, err);
            Expr **vals = lower_expr_list(arena, e->as.dict_lit.vals, e->as.dict_lit.pairs_len, err);
            Expr *d = new_expr_like(arena, e, EXPR_DICT, err);
            if (!d) return NULL;
            d->as.dict_lit.keys = keys;
            d->as.dict_lit.vals = vals;
            d->as.dict_lit.pairs_len = e->as.dict_lit.pairs_len;
            d->as.dict_lit.annot = e->as.dict_lit.annot;
            return d;
        }
        case EXPR_TUPLE: {
            Expr **items = lower_expr_list(arena, e->as.tuple_lit.items, e->as.tuple_lit.items_len, err);
            Expr *t = new_expr_like(arena, e, EXPR_TUPLE, err);
            if (!t) return NULL;
            t->as.tuple_lit.items = items;
            t->as.tuple_lit.items_len = e->as.tuple_lit.items_len;
            return t;
        }
        case EXPR_MATCH: {
            Expr *scrut = lower_expr(arena, e->as.match_expr.scrut, err);
            size_t n = e->as.match_expr.arms_len;
            MatchArm **arms = NULL;
            if (n > 0) {
                arms = (MatchArm **)arena_array(arena, n, sizeof(MatchArm *));
                if (!arms) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                for (size_t i = 0; i < n; i++) {
                    MatchArm *src = e->as.match_expr.arms[i];
                    MatchArm *arm = (MatchArm *)ast_alloc(arena, sizeof(MatchArm));
                    if (!arm) {
                        set_err(err, "out of memory");
                        return NULL;
                    }
                    arm->pat = src->pat;
                    arm->expr = lower_expr(arena, src->expr, err);
                    arms[i] = arm;
                }
            }
            Expr *m = new_expr_like(arena, e, EXPR_MATCH, err);
            if (!m) return NULL;
            m->as.match_expr.scrut = scrut;
            m->as.match_expr.arms = arms;
            m->as.match_expr.arms_len = n;
            return m;
        }
        case EXPR_LAMBDA: {
            Expr *body = lower_expr(arena, e->as.lambda.body, err);
            Expr *lam = new_expr_like(arena, e, EXPR_LAMBDA, err);
            if (!lam) return NULL;
            lam->as.lambda.params = e->as.lambda.params;
            lam->as.lambda.params_len = e->as.lambda.params_len;
            lam->as.lambda.body = body;
            return lam;
        }
        case EXPR_BLOCK: {
            Stmt *block = lower_stmt(arena, e->as.block_expr.block, err);
            Expr *b = new_expr_like(arena, e, EXPR_BLOCK, err);
            if (!b) return NULL;
            b->as.block_expr.block = block;
            return b;
        }
        case EXPR_NEW: {
            Expr **args = lower_expr_list(arena, e->as.new_expr.args, e->as.new_expr.args_len, err);
            Expr *n = new_expr_like(arena, e, EXPR_NEW, err);
            if (!n) return NULL;
            n->as.new_expr.name = e->as.new_expr.name;
            n->as.new_expr.args = args;
            n->as.new_expr.args_len = e->as.new_expr.args_len;
            n->as.new_expr.arg_names = e->as.new_expr.arg_names;
            return n;
        }
        case EXPR_IF: {
            size_t n = e->as.if_expr.arms_len;
            ExprIfArm **arms = NULL;
            if (n > 0) {
                arms = (ExprIfArm **)arena_array(arena, n, sizeof(ExprIfArm *));
                if (!arms) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                for (size_t i = 0; i < n; i++) {
                    ExprIfArm *src = e->as.if_expr.arms[i];
                    ExprIfArm *arm = (ExprIfArm *)ast_alloc(arena, sizeof(ExprIfArm));
                    if (!arm) {
                        set_err(err, "out of memory");
                        return NULL;
                    }
                    arm->cond = src->cond ? lower_expr(arena, src->cond, err) : NULL;
                    arm->value = lower_expr(arena, src->value, err);
                    arms[i] = arm;
                }
            }
            Expr *ie = new_expr_like(arena, e, EXPR_IF, err);
            if (!ie) return NULL;
            ie->as.if_expr.arms = arms;
            ie->as.if_expr.arms_len = n;
            return ie;
        }
        case EXPR_TERNARY: {
            Expr *c = lower_expr(arena, e->as.ternary.cond, err);
            Expr *a = lower_expr(arena, e->as.ternary.then_expr, err);
            Expr *b = lower_expr(arena, e->as.ternary.else_expr, err);
            Expr *t = new_expr_like(arena, e, EXPR_TERNARY, err);
            if (!t) return NULL;
            t->as.ternary.cond = c;
            t->as.ternary.then_expr = a;
            t->as.ternary.else_expr = b;
            return t;
        }
        case EXPR_MOVE: {
            Expr *x = lower_expr(arena, e->as.move.x, err);
            Expr *m = new_expr_like(arena, e, EXPR_MOVE, err);
            if (!m) return NULL;
            m->as.move.x = x;
            return m;
        }
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_STR:
        case EXPR_IDENT:
        case EXPR_NULL:
        case EXPR_BOOL: {
            Expr *c = new_expr_like(arena, e, e->kind, err);
            if (!c) return NULL;
            c->as = e->as;
            return c;
        }
        default:
            return e;
    }
}

static Stmt *lower_stmt(Arena *arena, Stmt *s, Diag *err) {
    if (!s) return NULL;
    switch (s->kind) {
        case STMT_LET: {
            Stmt *out = new_stmt_like(arena, s, STMT_LET, err);
            if (!out) return NULL;
            out->as.let_s.name = s->as.let_s.name;
            out->as.let_s.is_mut = s->as.let_s.is_mut;
            out->as.let_s.expr = lower_expr(arena, s->as.let_s.expr, err);
            return out;
        }
        case STMT_CONST: {
            Stmt *out = new_stmt_like(arena, s, STMT_CONST, err);
            if (!out) return NULL;
            out->as.const_s.name = s->as.const_s.name;
            out->as.const_s.expr = lower_expr(arena, s->as.const_s.expr, err);
            return out;
        }
        case STMT_RETURN: {
            Stmt *out = new_stmt_like(arena, s, STMT_RETURN, err);
            if (!out) return NULL;
            out->as.ret_s.expr = s->as.ret_s.expr ? lower_expr(arena, s->as.ret_s.expr, err) : NULL;
            return out;
        }
        case STMT_BREAK:
            return new_stmt_like(arena, s, STMT_BREAK, err);
        case STMT_CONTINUE:
            return new_stmt_like(arena, s, STMT_CONTINUE, err);
        case STMT_EXPR: {
            Stmt *out = new_stmt_like(arena, s, STMT_EXPR, err);
            if (!out) return NULL;
            out->as.expr_s.expr = lower_expr(arena, s->as.expr_s.expr, err);
            return out;
        }
        case STMT_IF: {
            size_t n = s->as.if_s.arms_len;
            IfArm **arms = NULL;
            if (n > 0) {
                arms = (IfArm **)arena_array(arena, n, sizeof(IfArm *));
                if (!arms) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                for (size_t i = 0; i < n; i++) {
                    IfArm *src = s->as.if_s.arms[i];
                    IfArm *arm = (IfArm *)ast_alloc(arena, sizeof(IfArm));
                    if (!arm) {
                        set_err(err, "out of memory");
                        return NULL;
                    }
                    arm->cond = src->cond ? lower_expr(arena, src->cond, err) : NULL;
                    arm->body = lower_stmt(arena, src->body, err);
                    arms[i] = arm;
                }
            }
            Stmt *out = new_stmt_like(arena, s, STMT_IF, err);
            if (!out) return NULL;
            out->as.if_s.arms = arms;
            out->as.if_s.arms_len = n;
            return out;
        }
        case STMT_FOR: {
            Stmt *out = new_stmt_like(arena, s, STMT_FOR, err);
            if (!out) return NULL;
            out->as.for_s.init = s->as.for_s.init ? lower_stmt(arena, s->as.for_s.init, err) : NULL;
            out->as.for_s.cond = s->as.for_s.cond ? lower_expr(arena, s->as.for_s.cond, err) : NULL;
            out->as.for_s.step = s->as.for_s.step ? lower_expr(arena, s->as.for_s.step, err) : NULL;
            out->as.for_s.body = lower_stmt(arena, s->as.for_s.body, err);
            return out;
        }
        case STMT_FOREACH: {
            Stmt *out = new_stmt_like(arena, s, STMT_FOREACH, err);
            if (!out) return NULL;
            out->as.foreach_s.name = s->as.foreach_s.name;
            out->as.foreach_s.expr = lower_expr(arena, s->as.foreach_s.expr, err);
            out->as.foreach_s.body = lower_stmt(arena, s->as.foreach_s.body, err);
            return out;
        }
        case STMT_BLOCK: {
            size_t n = s->as.block_s.stmts_len;
            Stmt **stmts = NULL;
            if (n > 0) {
                stmts = (Stmt **)arena_array(arena, n, sizeof(Stmt *));
                if (!stmts) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                for (size_t i = 0; i < n; i++) {
                    stmts[i] = lower_stmt(arena, s->as.block_s.stmts[i], err);
                }
            }
            Stmt *out = new_stmt_like(arena, s, STMT_BLOCK, err);
            if (!out) return NULL;
            out->as.block_s.stmts = stmts;
            out->as.block_s.stmts_len = n;
            return out;
        }
        default:
            return s;
    }
}

static Stmt *wrap_block(Arena *arena, Stmt *body, Diag *err) {
    if (!body) return NULL;
    if (body->kind == STMT_BLOCK) {
        return body;
    }
    Stmt *block = (Stmt *)ast_alloc(arena, sizeof(Stmt));
    if (!block) {
        set_err(err, "out of memory");
        return NULL;
    }
    block->kind = STMT_BLOCK;
    block->line = body->line;
    block->col = body->col;
    block->as.block_s.stmts_len = 1;
    block->as.block_s.stmts = (Stmt **)arena_array(arena, 1, sizeof(Stmt *));
    if (!block->as.block_s.stmts) {
        set_err(err, "out of memory");
        return NULL;
    }
    block->as.block_s.stmts[0] = body;
    return block;
}

static Decl *lower_decl(Arena *arena, Decl *d, Diag *err) {
    if (!d) return NULL;
    switch (d->kind) {
        case DECL_FUN: {
            Stmt *body = lower_stmt(arena, d->as.fun.body, err);
            body = wrap_block(arena, body, err);
            Decl *out = new_decl_like(arena, d, DECL_FUN, err);
            if (!out) return NULL;
            out->as.fun = d->as.fun;
            out->as.fun.body = body;
            return out;
        }
        case DECL_MACRO: {
            Decl *out = new_decl_like(arena, d, DECL_MACRO, err);
            if (!out) return NULL;
            out->as.macro = d->as.macro;
            out->as.macro.body = wrap_block(arena, lower_stmt(arena, d->as.macro.body, err), err);
            return out;
        }
        case DECL_ENTRY: {
            Stmt *body = lower_stmt(arena, d->as.entry.body, err);
            body = wrap_block(arena, body, err);
            Decl *out = new_decl_like(arena, d, DECL_ENTRY, err);
            if (!out) return NULL;
            out->as.entry = d->as.entry;
            out->as.entry.body = body;
            return out;
        }
        case DECL_CONST: {
            Decl *out = new_decl_like(arena, d, DECL_CONST, err);
            if (!out) return NULL;
            out->as.const_decl = d->as.const_decl;
            out->as.const_decl.expr = lower_expr(arena, d->as.const_decl.expr, err);
            return out;
        }
        case DECL_DEF: {
            Decl *out = new_decl_like(arena, d, DECL_DEF, err);
            if (!out) return NULL;
            out->as.def_decl = d->as.def_decl;
            out->as.def_decl.expr = lower_expr(arena, d->as.def_decl.expr, err);
            return out;
        }
        case DECL_CLASS: {
            Decl *out = new_decl_like(arena, d, DECL_CLASS, err);
            if (!out) return NULL;
            out->as.class_decl = d->as.class_decl;
            size_t n = d->as.class_decl.methods_len;
            FunDecl **methods = NULL;
            if (n > 0) {
                methods = (FunDecl **)arena_array(arena, n, sizeof(FunDecl *));
                if (!methods) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                for (size_t i = 0; i < n; i++) {
                    FunDecl *src = d->as.class_decl.methods[i];
                    FunDecl *fn = (FunDecl *)ast_alloc(arena, sizeof(FunDecl));
                    if (!fn) {
                        set_err(err, "out of memory");
                        return NULL;
                    }
                    *fn = *src;
                    fn->body = wrap_block(arena, lower_stmt(arena, src->body, err), err);
                    methods[i] = fn;
                }
            }
            out->as.class_decl.methods = methods;
            out->as.class_decl.methods_len = n;
            return out;
        }
        default:
            return d;
    }
}

Program *lower_program(Program *prog, Arena *arena, Diag *err) {
    if (!prog) return NULL;

    size_t mods_len = prog->mods_len;
    LowerModule *mods = mods_len ? (LowerModule *)arena_array(arena, mods_len, sizeof(LowerModule)) : NULL;
    if (mods_len && !mods) {
        set_err(err, "out of memory");
        return NULL;
    }
    for (size_t i = 0; i < mods_len; i++) {
        Module *m = prog->mods[i];
        Str mod_name = cask_name_for_path(arena, m->path);
        if (m->has_declared_name) {
            mod_name = m->declared_name;
        }
        mods[i].name = mod_name;
        mods[i].path = m->path;
        mods[i].imports_len = m->imports_len;
        if (m->imports_len > 0) {
            mods[i].imports = (Str *)arena_array(arena, m->imports_len, sizeof(Str));
            if (!mods[i].imports) {
                set_err(err, "out of memory");
                return NULL;
            }
            for (size_t j = 0; j < m->imports_len; j++) {
                mods[i].imports[j] = normalize_import_name(arena, m->imports[j]->name);
            }
        } else {
            mods[i].imports = NULL;
        }
    }

    size_t macro_count = 0;
    for (size_t i = 0; i < mods_len; i++) {
        Module *m = prog->mods[i];
        for (size_t j = 0; j < m->decls_len; j++) {
            if (m->decls[j]->kind == DECL_MACRO) macro_count++;
        }
    }
    LowerMacro *macros = macro_count ? (LowerMacro *)arena_array(arena, macro_count, sizeof(LowerMacro)) : NULL;
    if (macro_count && !macros) {
        set_err(err, "out of memory");
        return NULL;
    }
    size_t macro_i = 0;
    for (size_t i = 0; i < mods_len; i++) {
        Module *m = prog->mods[i];
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind != DECL_MACRO) continue;
            macros[macro_i].cask = mods[i].name;
            macros[macro_i].name = d->as.macro.name;
            macros[macro_i].decl = &d->as.macro;
            macro_i++;
        }
    }

    Program *out = (Program *)ast_alloc(arena, sizeof(Program));
    if (!out) {
        set_err(err, "out of memory");
        return NULL;
    }
    out->mods_len = mods_len;
    out->mods = (Module **)arena_array(arena, mods_len, sizeof(Module *));
    if (!out->mods && mods_len > 0) {
        set_err(err, "out of memory");
        return NULL;
    }
    for (size_t i = 0; i < mods_len; i++) {
        Module *m = prog->mods[i];
        Module *nm = (Module *)ast_alloc(arena, sizeof(Module));
        if (!nm) {
            set_err(err, "out of memory");
            return NULL;
        }
        nm->path = m->path;
        nm->declared_name = m->declared_name;
        nm->has_declared_name = m->has_declared_name;
        nm->imports = m->imports;
        nm->imports_len = m->imports_len;
        nm->decls = (Decl **)arena_array(arena, m->decls_len, sizeof(Decl *));
        if (!nm->decls && m->decls_len > 0) {
            set_err(err, "out of memory");
            return NULL;
        }
        LowerCtx lctx;
        memset(&lctx, 0, sizeof(lctx));
        lctx.cask_name = mods[i].name;
        lctx.cask_path = mods[i].path;
        lctx.imports = mods[i].imports;
        lctx.imports_len = mods[i].imports_len;
        lctx.macros = macros;
        lctx.macros_len = macro_count;
        lctx.macro_depth = 0;
        LowerCtx *saved_ctx = g_lower_ctx;
        g_lower_ctx = &lctx;

        size_t out_decl_len = 0;
        for (size_t j = 0; j < m->decls_len; j++) {
            if (m->decls[j]->kind == DECL_MACRO) {
                continue;
            }
            nm->decls[out_decl_len++] = lower_decl(arena, m->decls[j], err);
        }
        g_lower_ctx = saved_ctx;
        nm->decls_len = out_decl_len;
        out->mods[i] = nm;
    }
    return out;
}

// ----------------------------
// Type system helpers
// ----------------------------

typedef struct {
    Str name;
    Ty *ty;
} SubstEntry;

typedef struct {
    SubstEntry *data;
    size_t len;
    size_t cap;
} Subst;

void locals_init(Locals *loc) {
    loc->scopes = NULL;
    loc->len = 0;
    loc->cap = 0;
    // push root scope
    if (loc->len + 1 > loc->cap) {
        size_t next = loc->cap ? loc->cap * 2 : 4;
        LocalScope *sc = (LocalScope *)malloc(next * sizeof(LocalScope));
        if (!sc) {
            return;
        }
        loc->scopes = sc;
        loc->cap = next;
    }
    loc->scopes[0].entries = NULL;
    loc->scopes[0].len = 0;
    loc->scopes[0].cap = 0;
    loc->len = 1;
}

void locals_free(Locals *loc) {
    if (!loc) return;
    for (size_t i = 0; i < loc->len; i++) {
        free(loc->scopes[i].entries);
    }
    free(loc->scopes);
    loc->scopes = NULL;
    loc->len = 0;
    loc->cap = 0;
}

void locals_push(Locals *loc) {
    if (loc->len + 1 > loc->cap) {
        size_t next = loc->cap ? loc->cap * 2 : 4;
        LocalScope *sc = (LocalScope *)realloc(loc->scopes, next * sizeof(LocalScope));
        if (!sc) {
            return;
        }
        loc->scopes = sc;
        loc->cap = next;
    }
    LocalScope *s = &loc->scopes[loc->len++];
    s->entries = NULL;
    s->len = 0;
    s->cap = 0;
}

void locals_pop(Locals *loc) {
    if (loc->len == 0) return;
    LocalScope *s = &loc->scopes[loc->len - 1];
    free(s->entries);
    loc->len -= 1;
}

static LocalEntry *locals_find_in_scope(LocalScope *scope, Str name) {
    for (size_t i = 0; i < scope->len; i++) {
        if (str_eq(scope->entries[i].name, name)) {
            return &scope->entries[i];
        }
    }
    return NULL;
}

void locals_define(Locals *loc, Str name, Binding b) {
    if (loc->len == 0) {
        return;
    }
    LocalScope *scope = &loc->scopes[loc->len - 1];
    LocalEntry *existing = locals_find_in_scope(scope, name);
    if (existing) {
        existing->binding = b;
        return;
    }
    if (scope->len + 1 > scope->cap) {
        size_t next = scope->cap ? scope->cap * 2 : 8;
        LocalEntry *ents = (LocalEntry *)realloc(scope->entries, next * sizeof(LocalEntry));
        if (!ents) {
            return;
        }
        scope->entries = ents;
        scope->cap = next;
    }
    scope->entries[scope->len].name = name;
    scope->entries[scope->len].binding = b;
    scope->len += 1;
}

Binding *locals_lookup(Locals *loc, Str name) {
    for (size_t i = loc->len; i-- > 0;) {
        LocalScope *scope = &loc->scopes[i];
        LocalEntry *entry = locals_find_in_scope(scope, name);
        if (entry) {
            return &entry->binding;
        }
    }
    return NULL;
}

static Locals locals_clone(Locals *src) {
    Locals out;
    out.scopes = NULL;
    out.len = 0;
    out.cap = 0;
    if (!src) return out;
    out.len = src->len;
    out.cap = src->len;
    out.scopes = (LocalScope *)malloc(out.cap * sizeof(LocalScope));
    if (!out.scopes) {
        out.len = 0;
        out.cap = 0;
        return out;
    }
    for (size_t i = 0; i < src->len; i++) {
        LocalScope *s = &out.scopes[i];
        LocalScope *os = &src->scopes[i];
        s->len = os->len;
        s->cap = os->len;
        if (os->len == 0) {
            s->entries = NULL;
            continue;
        }
        s->entries = (LocalEntry *)malloc(os->len * sizeof(LocalEntry));
        if (!s->entries) {
            s->len = 0;
            s->cap = 0;
            continue;
        }
        memcpy(s->entries, os->entries, os->len * sizeof(LocalEntry));
    }
    return out;
}

static bool set_errf(Diag *err, Str path, int line, int col, const char *fmt, ...) {
    if (!err) return false;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    char *msg = (char *)malloc(strlen(buf) + 1);
    if (!msg) {
        err->message = "type error";
    } else {
        strcpy(msg, buf);
        err->message = msg;
    }
    err->path = path.data;
    err->line = line;
    err->col = col;
    return false;
}

static Str arena_str_copy(Arena *arena, const char *s, size_t len) {
    Str out = {"", 0};
    if (!arena) return out;
    char *buf = (char *)arena_alloc(arena, len + 1);
    if (!buf) return out;
    memcpy(buf, s, len);
    buf[len] = '\0';
    out.data = buf;
    out.len = len;
    return out;
}

static bool str_ends_with(Str s, const char *suffix) {
    size_t slen = s.len;
    size_t suf = strlen(suffix);
    if (slen < suf) return false;
    return memcmp(s.data + (slen - suf), suffix, suf) == 0;
}

static Str str_slice(Str s, size_t start, size_t len) {
    Str out;
    out.data = s.data + start;
    out.len = len;
    return out;
}

static bool str_is_explicit_generic_name(Str s) {
    if (s.len == 0) return false;
    if (!(s.data[0] >= 'A' && s.data[0] <= 'Z')) return false;
    for (size_t i = 0; i < s.len; i++) {
        char c = s.data[i];
        if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
            return false;
        }
    }
    return true;
}

static Ty *ty_new(Arena *arena, TyTag tag) {
    Ty *t = (Ty *)arena_alloc(arena, sizeof(Ty));
    if (!t) return NULL;
    memset(t, 0, sizeof(Ty));
    t->tag = tag;
    return t;
}

static Ty *ty_prim(Arena *arena, const char *name) {
    Ty *t = ty_new(arena, TY_PRIM);
    if (!t) return NULL;
    t->name = str_from_c(name);
    return t;
}

static Ty *ty_class(Arena *arena, Str name) {
    Ty *t = ty_new(arena, TY_CLASS);
    if (!t) return NULL;
    t->name = name;
    return t;
}

static Ty *ty_array(Arena *arena, Ty *elem) {
    Ty *t = ty_new(arena, TY_ARRAY);
    if (!t) return NULL;
    t->elem = elem;
    return t;
}

static Ty *ty_dict(Arena *arena, Ty *key, Ty *val) {
    Ty *t = ty_new(arena, TY_DICT);
    if (!t) return NULL;
    t->key = key;
    t->elem = val;
    return t;
}

static Ty *ty_tuple(Arena *arena, Ty **items, size_t len) {
    Ty *t = ty_new(arena, TY_TUPLE);
    if (!t) return NULL;
    t->items = items;
    t->items_len = len;
    return t;
}

static Ty *ty_void(Arena *arena) {
    return ty_new(arena, TY_VOID);
}

static Ty *ty_null(Arena *arena) {
    return ty_new(arena, TY_NULL);
}

static Ty *ty_mod(Arena *arena, Str name) {
    Ty *t = ty_new(arena, TY_MOD);
    if (!t) return NULL;
    t->name = name;
    return t;
}

static Ty *ty_fn(Arena *arena, Ty **params, size_t params_len, Ty *ret) {
    Ty *t = ty_new(arena, TY_FN);
    if (!t) return NULL;
    t->params = params;
    t->params_len = params_len;
    t->ret = ret;
    return t;
}

static Ty *ty_nullable(Arena *arena, Ty *elem) {
    Ty *t = ty_new(arena, TY_NULLABLE);
    if (!t) return NULL;
    t->elem = elem;
    return t;
}

static Ty *ty_gen(Arena *arena, Str name) {
    Ty *t = ty_new(arena, TY_GEN);
    if (!t) return NULL;
    t->name = name;
    return t;
}

static bool ty_is_numeric(Ty *t) {
    return t && t->tag == TY_PRIM && str_eq_c(t->name, "num");
}

static bool ty_is_string(Ty *t) {
    return t && t->tag == TY_PRIM && str_eq_c(t->name, "string");
}

static bool ty_is_any(Ty *t) {
    return t && t->tag == TY_PRIM && str_eq_c(t->name, "any");
}

static void ty_desc(Ty *t, char *buf, size_t cap) {
    if (!buf || cap == 0) return;
    if (!t) {
        snprintf(buf, cap, "<null>");
        return;
    }
    switch (t->tag) {
        case TY_PRIM:
            snprintf(buf, cap, "%.*s", (int)t->name.len, t->name.data);
            return;
        case TY_CLASS:
            snprintf(buf, cap, "class %.*s", (int)t->name.len, t->name.data);
            return;
        case TY_ARRAY:
            snprintf(buf, cap, "array");
            return;
        case TY_DICT:
            snprintf(buf, cap, "dict");
            return;
        case TY_TUPLE:
            snprintf(buf, cap, "tuple");
            return;
        case TY_VOID:
            snprintf(buf, cap, "void");
            return;
        case TY_NULL:
            snprintf(buf, cap, "null");
            return;
        case TY_MOD:
            snprintf(buf, cap, "cask");
            return;
        case TY_FN:
            snprintf(buf, cap, "fn");
            return;
        case TY_NULLABLE:
            snprintf(buf, cap, "nullable");
            return;
        case TY_GEN:
            snprintf(buf, cap, "gen %.*s", (int)t->name.len, t->name.data);
            return;
        default:
            snprintf(buf, cap, "type");
            return;
    }
}

static bool ty_is_null(Ty *t) {
    return t && t->tag == TY_NULL;
}

static bool ty_is_void(Ty *t) {
    return t && t->tag == TY_VOID;
}

static bool ty_is_nullable(Ty *t) {
    return t && t->tag == TY_NULLABLE;
}

static Ty *ty_strip_nullable(Ty *t) {
    if (ty_is_nullable(t) && t->elem) {
        return t->elem;
    }
    return t;
}

static Subst subst_init(void) {
    Subst s;
    s.data = NULL;
    s.len = 0;
    s.cap = 0;
    return s;
}

static void subst_free(Subst *s) {
    free(s->data);
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
}

static Ty *subst_get(Subst *s, Str name) {
    if (!s) return NULL;
    for (size_t i = 0; i < s->len; i++) {
        if (str_eq(s->data[i].name, name)) {
            return s->data[i].ty;
        }
    }
    return NULL;
}

static bool subst_set(Subst *s, Str name, Ty *ty) {
    if (!s) return false;
    for (size_t i = 0; i < s->len; i++) {
        if (str_eq(s->data[i].name, name)) {
            s->data[i].ty = ty;
            return true;
        }
    }
    if (s->len + 1 > s->cap) {
        size_t next = s->cap ? s->cap * 2 : 8;
        SubstEntry *data = (SubstEntry *)realloc(s->data, next * sizeof(SubstEntry));
        if (!data) return false;
        s->data = data;
        s->cap = next;
    }
    s->data[s->len].name = name;
    s->data[s->len].ty = ty;
    s->len += 1;
    return true;
}

static Ty *ty_apply_subst(Arena *arena, Ty *t, Subst *subst);

static Ty *ty_apply_subst(Arena *arena, Ty *t, Subst *subst) {
    if (!t) return NULL;
    if (t->tag == TY_GEN) {
        Ty *sub = subst_get(subst, t->name);
        if (sub) return sub;
        return t;
    }
    if (t->tag == TY_ARRAY) {
        Ty *elem = ty_apply_subst(arena, t->elem, subst);
        return ty_array(arena, elem);
    }
    if (t->tag == TY_TUPLE) {
        size_t n = t->items_len;
        Ty **items = (Ty **)arena_array(arena, n, sizeof(Ty *));
        if (!items) return NULL;
        for (size_t i = 0; i < n; i++) {
            items[i] = ty_apply_subst(arena, t->items[i], subst);
        }
        return ty_tuple(arena, items, n);
    }
    if (t->tag == TY_FN) {
        size_t n = t->params_len;
        Ty **params = (Ty **)arena_array(arena, n, sizeof(Ty *));
        if (!params) return NULL;
        for (size_t i = 0; i < n; i++) {
            params[i] = ty_apply_subst(arena, t->params[i], subst);
        }
        Ty *ret = ty_apply_subst(arena, t->ret, subst);
        return ty_fn(arena, params, n, ret);
    }
    if (t->tag == TY_NULLABLE) {
        Ty *elem = ty_apply_subst(arena, t->elem, subst);
        return ty_nullable(arena, elem);
    }
    return t;
}

static Ty *unify(Arena *arena, Ty *a, Ty *b, Str path, const char *where, Subst *subst, Diag *err) {
    if (!a || !b) return NULL;
    if (ty_is_any(a)) return a;
    if (ty_is_any(b)) return b;
    if (ty_is_null(a) && ty_is_null(b)) {
        return ty_null(arena);
    }
    if (ty_is_null(a)) {
        return ty_is_nullable(b) ? b : ty_nullable(arena, b);
    }
    if (ty_is_null(b)) {
        return ty_is_nullable(a) ? a : ty_nullable(arena, a);
    }
    if (ty_is_nullable(a) || ty_is_nullable(b)) {
        Ty *ua = ty_strip_nullable(a);
        Ty *ub = ty_strip_nullable(b);
        Ty *u = unify(arena, ua, ub, path, where, subst, err);
        if (!u) return NULL;
        return ty_nullable(arena, u);
    }
    if (a->tag == TY_GEN) {
        Ty *sub = subst_get(subst, a->name);
        if (sub) {
            return unify(arena, sub, b, path, where, subst, err);
        }
        subst_set(subst, a->name, b);
        return b;
    }
    if (b->tag == TY_GEN) {
        Ty *sub = subst_get(subst, b->name);
        if (sub) {
            return unify(arena, a, sub, path, where, subst, err);
        }
        subst_set(subst, b->name, a);
        return a;
    }
    if (a->tag != b->tag) {
        char ea[64];
        char eb[64];
        ty_desc(a, ea, sizeof(ea));
        ty_desc(b, eb, sizeof(eb));
        set_errf(err, path, 0, 0, "type mismatch%s%s (expected %s, got %s)",
                 where && where[0] ? ": " : "", where ? where : "", ea, eb);
        return NULL;
    }
    switch (a->tag) {
        case TY_ARRAY: {
            Ty *elem = unify(arena, a->elem, b->elem, path, where, subst, err);
            if (!elem) return NULL;
            return ty_array(arena, elem);
        }
        case TY_TUPLE: {
            if (a->items_len != b->items_len) {
                set_errf(err, path, 0, 0, "tuple arity mismatch%s%s", where && where[0] ? ": " : "", where ? where : "");
                return NULL;
            }
            Ty **items = (Ty **)arena_array(arena, a->items_len, sizeof(Ty *));
            if (!items) return NULL;
            for (size_t i = 0; i < a->items_len; i++) {
                items[i] = unify(arena, a->items[i], b->items[i], path, where, subst, err);
                if (!items[i]) return NULL;
            }
            return ty_tuple(arena, items, a->items_len);
        }
        case TY_FN: {
            if (a->params_len != b->params_len) {
                set_errf(err, path, 0, 0, "fn arity mismatch%s%s", where && where[0] ? ": " : "", where ? where : "");
                return NULL;
            }
            Ty **params = (Ty **)arena_array(arena, a->params_len, sizeof(Ty *));
            if (!params) return NULL;
            for (size_t i = 0; i < a->params_len; i++) {
                params[i] = unify(arena, a->params[i], b->params[i], path, where, subst, err);
                if (!params[i]) return NULL;
            }
            Ty *ret = unify(arena, a->ret, b->ret, path, where, subst, err);
            if (!ret) return NULL;
            return ty_fn(arena, params, a->params_len, ret);
        }
        case TY_PRIM:
        case TY_CLASS:
        case TY_MOD:
        case TY_VOID:
        case TY_NULL:
        case TY_GEN: {
            if (a->tag == TY_PRIM && !str_eq(a->name, b->name)) {
                char ea[64];
                char eb[64];
                ty_desc(a, ea, sizeof(ea));
                ty_desc(b, eb, sizeof(eb));
                set_errf(err, path, 0, 0, "type mismatch%s%s (expected %s, got %s)",
                         where && where[0] ? ": " : "", where ? where : "", ea, eb);
                return NULL;
            }
            if (a->tag == TY_CLASS && !str_eq(a->name, b->name)) {
                char ea[64];
                char eb[64];
                ty_desc(a, ea, sizeof(ea));
                ty_desc(b, eb, sizeof(eb));
                set_errf(err, path, 0, 0, "type mismatch%s%s (expected %s, got %s)",
                         where && where[0] ? ": " : "", where ? where : "", ea, eb);
                return NULL;
            }
            return a;
        }
        default:
            return a;
    }
}

static bool ensure_assignable(Arena *arena, Ty *expected, Ty *actual, Str path, const char *where, Diag *err) {
    if (!expected || !actual) return false;
    if (expected->tag == TY_PRIM && str_eq_c(expected->name, "any")) return true;
    if (actual->tag == TY_PRIM && str_eq_c(actual->name, "any")) return true;
    if (ty_is_null(expected)) {
        if (ty_is_null(actual) || ty_is_nullable(actual)) {
            return true;
        }
        char ea[64];
        char eb[64];
        ty_desc(expected, ea, sizeof(ea));
        ty_desc(actual, eb, sizeof(eb));
        set_errf(err, path, 0, 0, "type mismatch%s%s (expected %s, got %s)",
                 where && where[0] ? ": " : "", where ? where : "", ea, eb);
        return false;
    }
    if (ty_is_null(actual)) {
        if (ty_is_nullable(expected)) {
            return true;
        }
        char ea[64];
        char eb[64];
        ty_desc(expected, ea, sizeof(ea));
        ty_desc(actual, eb, sizeof(eb));
        set_errf(err, path, 0, 0, "type mismatch%s%s (expected %s, got %s)",
                 where && where[0] ? ": " : "", where ? where : "", ea, eb);
        return false;
    }
    if (ty_is_nullable(expected)) {
        Ty *base_expected = ty_strip_nullable(expected);
        Ty *base_actual = ty_is_nullable(actual) ? ty_strip_nullable(actual) : actual;
        return ensure_assignable(arena, base_expected, base_actual, path, where, err);
    }
    if (ty_is_nullable(actual)) {
        Ty *base_actual = ty_strip_nullable(actual);
        return ensure_assignable(arena, expected, base_actual, path, where, err);
    }
    if (expected->tag == TY_ARRAY && actual->tag == TY_ARRAY) {
        return ensure_assignable(arena, expected->elem, actual->elem, path, where, err);
    }
    if (expected->tag == TY_TUPLE && actual->tag == TY_TUPLE) {
        if (expected->items_len != actual->items_len) {
            set_errf(err, path, 0, 0, "tuple arity mismatch%s%s", where && where[0] ? ": " : "", where ? where : "");
            return false;
        }
        for (size_t i = 0; i < expected->items_len; i++) {
            if (!ensure_assignable(arena, expected->items[i], actual->items[i], path, where, err)) {
                return false;
            }
        }
        return true;
    }
    if (expected->tag == TY_FN && actual->tag == TY_FN) {
        if (expected->params_len != actual->params_len) {
            set_errf(err, path, 0, 0, "fn arity mismatch%s%s", where && where[0] ? ": " : "", where ? where : "");
            return false;
        }
        for (size_t i = 0; i < expected->params_len; i++) {
            if (!ensure_assignable(arena, expected->params[i], actual->params[i], path, where, err)) {
                return false;
            }
        }
        if (!ensure_assignable(arena, expected->ret, actual->ret, path, where, err)) {
            return false;
        }
        return true;
    }
    if (expected->tag == TY_PRIM && actual->tag == TY_PRIM) {
        if (!str_eq(expected->name, actual->name)) {
            char ea[64];
            char eb[64];
            ty_desc(expected, ea, sizeof(ea));
            ty_desc(actual, eb, sizeof(eb));
            set_errf(err, path, 0, 0, "type mismatch%s%s (expected %s, got %s)",
                     where && where[0] ? ": " : "", where ? where : "", ea, eb);
            return false;
        }
    }
    if (expected->tag == TY_CLASS && actual->tag == TY_CLASS) {
        if (!str_eq(expected->name, actual->name)) {
            char ea[64];
            char eb[64];
            ty_desc(expected, ea, sizeof(ea));
            ty_desc(actual, eb, sizeof(eb));
            set_errf(err, path, 0, 0, "type mismatch%s%s (expected %s, got %s)",
                     where && where[0] ? ": " : "", where ? where : "", ea, eb);
            return false;
        }
    }
    return true;
}

static Str qualify_class_name(Arena *arena, Str mod, Str name) {
    if (memchr(name.data, '.', name.len)) {
        return name;
    }
    size_t len = mod.len + 1 + name.len;
    char *buf = (char *)arena_alloc(arena, len + 1);
    if (!buf) {
        Str out = {"", 0};
        return out;
    }
    memcpy(buf, mod.data, mod.len);
    buf[mod.len] = '.';
    memcpy(buf + mod.len + 1, name.data, name.len);
    buf[len] = '\0';
    Str out;
    out.data = buf;
    out.len = len;
    return out;
}

static Str cask_name_for_path(Arena *arena, Str path) {
    const char *p = path.data;
    size_t len = path.len;
    size_t start = 0;
    for (size_t i = 0; i < len; i++) {
        char c = p[i];
        if (c == '/' || c == '\\') {
            start = i + 1;
        }
    }
    size_t name_len = len - start;
    if (name_len >= 3 &&
        p[start + name_len - 3] == '.' &&
        p[start + name_len - 2] == 'y' &&
        p[start + name_len - 1] == 'i') {
        name_len -= 3;
    }
    Str name = arena_str_copy(arena, p + start, name_len);
    
    return name;
}

static Str normalize_import_name(Arena *arena, Str name) {
    Str n = name;
    if (str_ends_with(n, ".yi")) {
        n = arena_str_copy(arena, n.data, n.len - 3);
    }
    // For dotted paths (e.g. "widgets.queue"), use the last segment
    size_t last_dot = 0;
    bool has_dot = false;
    for (size_t i = 0; i < n.len; i++) {
        if (n.data[i] == '.') {
            last_dot = i;
            has_dot = true;
        }
    }
    if (has_dot) {
        return arena_str_copy(arena, n.data + last_dot + 1, n.len - last_dot - 1);
    }
    return n;
}

static ClassInfo *find_class(GlobalEnv *env, Str qname) {
    for (size_t i = 0; i < env->classes_len; i++) {
        if (str_eq(env->classes[i].qname, qname)) {
            return &env->classes[i];
        }
    }
    return NULL;
}

static FunSig *find_fun(GlobalEnv *env, Str cask, Str name) {
    for (size_t i = 0; i < env->funs_len; i++) {
        if (str_eq(env->funs[i].cask, cask) && str_eq(env->funs[i].name, name)) {
            return &env->funs[i];
        }
    }
    return NULL;
}

static ModuleImport *find_imports(GlobalEnv *env, Str cask) {
    for (size_t i = 0; i < env->cask_imports_len; i++) {
        if (str_eq(env->cask_imports[i].cask, cask)) {
            return &env->cask_imports[i];
        }
    }
    return NULL;
}

static ModuleConsts *find_cask_consts(GlobalEnv *env, Str cask) {
    for (size_t i = 0; i < env->cask_consts_len; i++) {
        if (str_eq(env->cask_consts[i].cask, cask)) {
            return &env->cask_consts[i];
        }
    }
    return NULL;
}

static ModuleGlobals *find_cask_globals(GlobalEnv *env, Str cask) {
    for (size_t i = 0; i < env->cask_globals_len; i++) {
        if (str_eq(env->cask_globals[i].cask, cask)) {
            return &env->cask_globals[i];
        }
    }
    return NULL;
}

static GlobalVar *find_global(ModuleGlobals *mg, Str name) {
    if (!mg) return NULL;
    for (size_t i = 0; i < mg->len; i++) {
        if (str_eq(mg->vars[i].name, name)) {
            return &mg->vars[i];
        }
    }
    return NULL;
}

static ConstEntry *find_const(ModuleConsts *mc, Str name) {
    if (!mc) return NULL;
    for (size_t i = 0; i < mc->len; i++) {
        if (str_eq(mc->entries[i].name, name)) {
            return &mc->entries[i];
        }
    }
    return NULL;
}

static bool is_cross_cask(Str from_cask, Str target_cask) {
    return !str_eq(from_cask, target_cask);
}

static Ty *ty_from_type_ref(GlobalEnv *env, TypeRef *tref, Str ctx_mod, Str *imports, size_t imports_len, Diag *err) {
    if (!tref) return NULL;
    if (tref->kind == TYPE_ARRAY) {
        Ty *elem = ty_from_type_ref(env, tref->as.elem, ctx_mod, imports, imports_len, err);
        return ty_array(env->arena, elem);
    }
    if (tref->kind == TYPE_DICT) {
        Ty *key = ty_from_type_ref(env, tref->as.dict.key_typ, ctx_mod, imports, imports_len, err);
        Ty *val = ty_from_type_ref(env, tref->as.dict.val_typ, ctx_mod, imports, imports_len, err);
        if (!key || !val) return NULL;
        return ty_dict(env->arena, key, val);
    }
    Str n = tref->as.name;
    if (str_eq_c(n, "str")) {
        n = str_from_c("string");
    }
    if (str_eq_c(n, "int") || str_eq_c(n, "float") || str_eq_c(n, "char") || str_eq_c(n, "byte")) {
        set_errf(err, ctx_mod, tref->line, tref->col, "unknown type '%.*s' (use num)", (int)n.len, n.data);
        return NULL;
    }
    if (str_eq_c(n, "bool")) {
        return ty_prim(env->arena, "bool");
    }
    if (str_eq_c(n, "string")) {
        return ty_prim(env->arena, "string");
    }
    if (str_eq_c(n, "void")) {
        return ty_void(env->arena);
    }
    if (str_eq_c(n, "num")) {
        return ty_prim(env->arena, "num");
    }
    if (str_eq_c(n, "any")) {
        return ty_prim(env->arena, "any");
    }
    if (memchr(n.data, '.', n.len)) {
        // cask-qualified class
        size_t dot = 0;
        for (; dot < n.len; dot++) {
            if (n.data[dot] == '.') break;
        }
        Str mod = str_slice(n, 0, dot);
        bool in_scope = false;
        if (str_eq(mod, ctx_mod)) in_scope = true;
        for (size_t i = 0; i < imports_len && !in_scope; i++) {
            if (str_eq(imports[i], mod)) in_scope = true;
        }
        if (!in_scope) {
            set_errf(err, ctx_mod, tref->line, tref->col, "unknown type '%.*s'", (int)n.len, n.data);
            return NULL;
        }
        ClassInfo *ci = find_class(env, n);
        if (ci) {
            if (is_cross_cask(ctx_mod, ci->cask) && !str_eq_c(ci->vis, "pub")) {
                set_errf(err, ctx_mod, tref->line, tref->col, "type '%.*s' is not public", (int)n.len, n.data);
                return NULL;
            }
            return ty_class(env->arena, n);
        }
        set_errf(err, ctx_mod, tref->line, tref->col, "unknown type '%.*s'", (int)n.len, n.data);
        return NULL;
    }
    Str qn = qualify_class_name(env->arena, ctx_mod, n);
    ClassInfo *ci = find_class(env, qn);
    if (ci) return ty_class(env->arena, qn);
    if (str_is_explicit_generic_name(n)) {
        return ty_gen(env->arena, n);
    }
    set_errf(err, ctx_mod, tref->line, tref->col, "unknown type '%.*s'", (int)n.len, n.data);
    return NULL;
}

static bool eval_const_expr(GlobalEnv *env, Expr *e, ConstVal *out, Diag *err) {
    if (!e || !out) return false;
    if (e->kind == EXPR_INT) {
        out->ty = ty_prim(env->arena, "num");
        out->is_float = false;
        out->i = e->as.int_lit.v;
        return true;
    }
    if (e->kind == EXPR_FLOAT) {
        out->ty = ty_prim(env->arena, "num");
        out->is_float = true;
        out->f = e->as.float_lit.v;
        return true;
    }
    if (e->kind == EXPR_BOOL) {
        out->ty = ty_prim(env->arena, "bool");
        out->b = e->as.bool_lit.v;
        return true;
    }
    if (e->kind == EXPR_NULL) {
        out->ty = ty_null(env->arena);
        return true;
    }
    if (e->kind == EXPR_STR) {
        StrParts *parts = e->as.str_lit.parts;
        size_t total = 0;
        for (size_t i = 0; i < parts->len; i++) {
            if (parts->parts[i].kind != STR_PART_TEXT) {
                set_err(err, "const string cannot interpolate");
                return false;
            }
            total += parts->parts[i].as.text.len;
        }
        Str s = arena_str_copy(env->arena, "", 0);
        if (total > 0) {
            char *buf = (char *)arena_alloc(env->arena, total + 1);
            if (!buf) {
                set_err(err, "out of memory");
                return false;
            }
            size_t off = 0;
            for (size_t i = 0; i < parts->len; i++) {
                memcpy(buf + off, parts->parts[i].as.text.data, parts->parts[i].as.text.len);
                off += parts->parts[i].as.text.len;
            }
            buf[total] = '\0';
            s.data = buf;
            s.len = total;
        }
        out->ty = ty_prim(env->arena, "string");
        out->s = s;
        return true;
    }
    if (e->kind == EXPR_PAREN) {
        return eval_const_expr(env, e->as.paren.x, out, err);
    }
    if (e->kind == EXPR_UNARY) {
        ConstVal cv = {0};
        if (!eval_const_expr(env, e->as.unary.x, &cv, err)) return false;
        if (e->as.unary.op == TOK_MINUS) {
            if (!cv.ty || cv.ty->tag != TY_PRIM || !str_eq_c(cv.ty->name, "num")) {
                set_err(err, "const unary - expects numeric");
                return false;
            }
            *out = cv;
            if (cv.is_float) {
                out->f = -cv.f;
            } else {
                out->i = -cv.i;
            }
            return true;
        }
        if (e->as.unary.op == TOK_BANG) {
            if (!cv.ty || cv.ty->tag != TY_PRIM || !str_eq_c(cv.ty->name, "bool")) {
                set_err(err, "const ! expects bool");
                return false;
            }
            out->ty = ty_prim(env->arena, "bool");
            out->b = !cv.b;
            return true;
        }
        set_err(err, "unsupported const unary op");
        return false;
    }
    if (e->kind == EXPR_BINARY && (e->as.binary.op == TOK_PLUS || e->as.binary.op == TOK_MINUS || e->as.binary.op == TOK_STAR || e->as.binary.op == TOK_SLASH || e->as.binary.op == TOK_PERCENT)) {
        ConstVal a = {0};
        ConstVal b = {0};
        if (!eval_const_expr(env, e->as.binary.a, &a, err)) return false;
        if (!eval_const_expr(env, e->as.binary.b, &b, err)) return false;
        if (e->as.binary.op == TOK_PLUS && ty_is_string(a.ty) && ty_is_string(b.ty)) {
            size_t total = a.s.len + b.s.len;
            Str s = arena_str_copy(env->arena, "", 0);
            if (total > 0) {
                char *buf = (char *)arena_alloc(env->arena, total + 1);
                if (!buf) {
                    set_err(err, "out of memory");
                    return false;
                }
                memcpy(buf, a.s.data, a.s.len);
                memcpy(buf + a.s.len, b.s.data, b.s.len);
                buf[total] = '\0';
                s.data = buf;
                s.len = total;
            }
            out->ty = ty_prim(env->arena, "string");
            out->s = s;
            return true;
        }
        if (!a.ty || !b.ty || a.ty->tag != TY_PRIM || b.ty->tag != TY_PRIM || !str_eq_c(a.ty->name, "num") || !str_eq_c(b.ty->name, "num")) {
            set_err(err, "const op expects numeric literals (or string + string)");
            return false;
        }
        bool a_float = a.is_float;
        bool b_float = b.is_float;
        if (e->as.binary.op == TOK_PERCENT && (a_float || b_float)) {
            set_err(err, "const % not supported for float");
            return false;
        }
        out->ty = ty_prim(env->arena, "num");
        if (a_float || b_float) {
            double av = a_float ? a.f : (double)a.i;
            double bv = b_float ? b.f : (double)b.i;
            out->is_float = true;
            switch (e->as.binary.op) {
                case TOK_PLUS: out->f = av + bv; break;
                case TOK_MINUS: out->f = av - bv; break;
                case TOK_STAR: out->f = av * bv; break;
                case TOK_SLASH: out->f = av / bv; break;
                default: break;
            }
        } else {
            long long av = a.i;
            long long bv = b.i;
            out->is_float = false;
            switch (e->as.binary.op) {
                case TOK_PLUS: out->i = av + bv; break;
                case TOK_MINUS: out->i = av - bv; break;
                case TOK_STAR: out->i = av * bv; break;
                case TOK_SLASH: out->i = av / bv; break;
                case TOK_PERCENT: out->i = av % bv; break;
                default: break;
            }
        }
        return true;
    }
    set_err(err, "const expression must be a literal or simple numeric expression");
    return false;
}

GlobalEnv *build_global_env(Program *prog, Arena *arena, Diag *err) {
    if (!prog || !arena) return NULL;
    GlobalEnv *env = (GlobalEnv *)arena_alloc(arena, sizeof(GlobalEnv));
    if (!env) {
        set_err(err, "out of memory");
        return NULL;
    }
    memset(env, 0, sizeof(GlobalEnv));
    env->arena = arena;

    env->cask_names_len = prog->mods_len;
    env->cask_names = (ModuleName *)arena_array(arena, prog->mods_len, sizeof(ModuleName));
    env->cask_imports_len = prog->mods_len;
    env->cask_imports = (ModuleImport *)arena_array(arena, prog->mods_len, sizeof(ModuleImport));
    env->cask_globals_len = prog->mods_len;
    env->cask_globals = (ModuleGlobals *)arena_array(arena, prog->mods_len, sizeof(ModuleGlobals));
    if (!env->cask_names || !env->cask_imports || !env->cask_globals) {
        set_err(err, "out of memory");
        return NULL;
    }

    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        Str mod_name = cask_name_for_path(arena, m->path);
        if (m->has_declared_name && !str_eq(m->declared_name, mod_name)) {
            // The entry module (index 0) may declare a cask name that
            // differs from its filename (e.g. main.yi with "cask quilter")
            // to set the project/app identity.  Other modules must match.
            if (i == 0) {
                mod_name = m->declared_name;
            } else {
                set_errf(err, m->path, 1, 1,
                         "%.*s: cask declaration '%.*s' must match file name '%.*s'",
                         (int)m->path.len, m->path.data,
                         (int)m->declared_name.len, m->declared_name.data,
                         (int)mod_name.len, mod_name.data);
                return NULL;
            }
        }
        env->cask_names[i].path = m->path;
        env->cask_names[i].name = mod_name;
        env->cask_imports[i].cask = mod_name;
        env->cask_imports[i].imports_len = m->imports_len;
        if (m->imports_len > 0) {
            env->cask_imports[i].imports = (Str *)arena_array(arena, m->imports_len, sizeof(Str));
            if (!env->cask_imports[i].imports) {
                set_err(err, "out of memory");
                return NULL;
            }
            for (size_t j = 0; j < m->imports_len; j++) {
                env->cask_imports[i].imports[j] = normalize_import_name(arena, m->imports[j]->name);
            }
        }
        env->cask_globals[i].cask = mod_name;
        env->cask_globals[i].vars = NULL;
        env->cask_globals[i].len = 0;
    }

    // cask globals (def)
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        size_t def_count = 0;
        for (size_t j = 0; j < m->decls_len; j++) {
            if (m->decls[j]->kind == DECL_DEF) def_count++;
        }
        if (def_count == 0) continue;
        ModuleGlobals *mg = &env->cask_globals[i];
        mg->vars = (GlobalVar *)arena_array(arena, def_count, sizeof(GlobalVar));
        if (!mg->vars) {
            set_err(err, "out of memory");
            return NULL;
        }
        size_t idx = 0;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind != DECL_DEF) continue;
            if (find_global(mg, d->as.def_decl.name)) {
                set_errf(err, m->path, d->line, d->col,
                         "%.*s: duplicate global '%.*s'",
                         (int)m->path.len, m->path.data,
                         (int)d->as.def_decl.name.len, d->as.def_decl.name.data);
                return NULL;
            }
            mg->vars[idx].name = d->as.def_decl.name;
            mg->vars[idx].ty = NULL;
            mg->vars[idx].is_mut = d->as.def_decl.is_mut;
            mg->vars[idx].is_pub = d->as.def_decl.is_pub;
            idx++;
        }
        mg->len = idx;
    }

    // count classes and funs
    size_t class_count = 0;
    size_t fun_count = 0;
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_CLASS) class_count++;
            if (d->kind == DECL_FUN) fun_count++;
        }
    }
    env->classes_len = class_count;
    env->classes = class_count ? (ClassInfo *)arena_array(arena, class_count, sizeof(ClassInfo)) : NULL;
    env->funs_len = fun_count;
    env->funs = fun_count ? (FunSig *)arena_array(arena, fun_count, sizeof(FunSig)) : NULL;
    if ((class_count && !env->classes) || (fun_count && !env->funs)) {
        set_err(err, "out of memory");
        return NULL;
    }

    // cask consts
    env->cask_consts_len = 0;
    env->cask_consts = (ModuleConsts *)arena_array(arena, prog->mods_len, sizeof(ModuleConsts));
    if (prog->mods_len && !env->cask_consts) {
        set_err(err, "out of memory");
        return NULL;
    }

    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        Str mod_name = env->cask_names[i].name;
        size_t const_count = 0;
        for (size_t j = 0; j < m->decls_len; j++) {
            if (m->decls[j]->kind == DECL_CONST) const_count++;
        }
        if (const_count == 0) continue;
        ModuleConsts *mc = &env->cask_consts[env->cask_consts_len++];
        mc->cask = mod_name;
        mc->len = const_count;
        mc->entries = const_count ? (ConstEntry *)arena_array(arena, const_count, sizeof(ConstEntry)) : NULL;
        size_t idx = 0;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind != DECL_CONST) continue;
            ConstDecl *cd = &d->as.const_decl;
            if (find_const(mc, cd->name)) {
                set_errf(err, m->path, d->line, d->col,
                         "%.*s: duplicate const '%.*s'",
                         (int)m->path.len, m->path.data, (int)cd->name.len, cd->name.data);
                return NULL;
            }
            ConstVal cv = {0};
            if (!eval_const_expr(env, cd->expr, &cv, err)) {
                return NULL;
            }
            mc->entries[idx].name = cd->name;
            mc->entries[idx].val = cv;
            mc->entries[idx].is_pub = cd->is_pub;
            idx++;
        }
    }

    // class shells
    size_t cidx = 0;
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        Str mod_name = env->cask_names[i].name;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind != DECL_CLASS) continue;
            Str qname = qualify_class_name(arena, mod_name, d->as.class_decl.name);
            if (find_class(env, qname)) {
                set_errf(err, m->path, d->line, d->col,
                         "%.*s: duplicate class '%.*s'",
                         (int)m->path.len, m->path.data,
                         (int)d->as.class_decl.name.len, d->as.class_decl.name.data);
                return NULL;
            }
            ClassInfo *ci = &env->classes[cidx++];
            memset(ci, 0, sizeof(ClassInfo));
            ci->name = d->as.class_decl.name;
            ci->cask = mod_name;
            ci->qname = qname;
            ci->vis = d->as.class_decl.vis;
            ci->is_seal = d->as.class_decl.is_seal;
            ci->base_qname = (Str){0};
            ci->kind = d->as.class_decl.kind;
            ci->cask_path = m->path;
        }
    }

    // fill class fields + methods
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        Str mod_name = env->cask_names[i].name;
        ModuleImport *imps = find_imports(env, mod_name);
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind != DECL_CLASS) continue;
            Str qname = qualify_class_name(arena, mod_name, d->as.class_decl.name);
            ClassInfo *ci = find_class(env, qname);
            if (!ci) continue;
            if (d->as.class_decl.has_base) {
                TypeRef base_ref;
                memset(&base_ref, 0, sizeof(base_ref));
                base_ref.kind = TYPE_NAME;
                base_ref.line = d->line;
                base_ref.col = d->col;
                base_ref.as.name = d->as.class_decl.base_name;
                Ty *base_ty = ty_from_type_ref(env, &base_ref, mod_name, imps ? imps->imports : NULL, imps ? imps->imports_len : 0, err);
                if (!base_ty || base_ty->tag != TY_CLASS) {
                    set_errf(err, m->path, d->line, d->col, "%.*s: base type must be a class",
                             (int)m->path.len, m->path.data);
                    return NULL;
                }
                ClassInfo *base_ci = find_class(env, base_ty->name);
                if (!base_ci) {
                    set_errf(err, m->path, d->line, d->col, "%.*s: unknown base class '%.*s'",
                             (int)m->path.len, m->path.data,
                             (int)d->as.class_decl.base_name.len, d->as.class_decl.base_name.data);
                    return NULL;
                }
                if (base_ci->kind != CLASS_KIND_CLASS) {
                    set_errf(err, m->path, d->line, d->col, "%.*s: base type '%.*s' is not a class",
                             (int)m->path.len, m->path.data,
                             (int)base_ci->name.len, base_ci->name.data);
                    return NULL;
                }
                if (str_eq(base_ci->qname, ci->qname)) {
                    set_errf(err, m->path, d->line, d->col, "%.*s: class '%.*s' cannot inherit from itself",
                             (int)m->path.len, m->path.data,
                             (int)ci->name.len, ci->name.data);
                    return NULL;
                }
                if (base_ci->is_seal) {
                    set_errf(err, m->path, d->line, d->col, "%.*s: cannot inherit from sealed class '%.*s'",
                             (int)m->path.len, m->path.data,
                             (int)base_ci->name.len, base_ci->name.data);
                    return NULL;
                }
                ci->base_qname = base_ci->qname;
            }

            size_t fields_len = d->as.class_decl.fields_len;
            size_t methods_len = d->as.class_decl.methods_len;
            ci->fields_len = fields_len;
            ci->methods_len = methods_len;
            ci->fields = fields_len ? (FieldEntry *)arena_array(arena, fields_len, sizeof(FieldEntry)) : NULL;
            ci->methods = methods_len ? (MethodEntry *)arena_array(arena, methods_len, sizeof(MethodEntry)) : NULL;
            if ((fields_len && !ci->fields) || (methods_len && !ci->methods)) {
                set_err(err, "out of memory");
                return NULL;
            }

            for (size_t f = 0; f < fields_len; f++) {
                FieldDecl *fd = d->as.class_decl.fields[f];
                Ty *fty = ty_from_type_ref(env, fd->typ, mod_name, imps ? imps->imports : NULL, imps ? imps->imports_len : 0, err);
                if (!fty) return NULL;
                ci->fields[f].name = fd->name;
                ci->fields[f].ty = fty;
                ci->fields[f].is_pub = fd->is_pub;
            }

            for (size_t m_i = 0; m_i < methods_len; m_i++) {
                FunDecl *md = d->as.class_decl.methods[m_i];
                if (md->params_len == 0 || !md->params[0]->is_this) {
                    set_errf(err, m->path, d->line, d->col,
                             "%.*s: method '%.*s' in class '%.*s' must begin with this/?this",
                             (int)m->path.len, m->path.data,
                             (int)md->name.len, md->name.data,
                             (int)d->as.class_decl.name.len, d->as.class_decl.name.data);
                    return NULL;
                }
                bool recv_mut = md->params[0]->is_mut;
                Ty *ret_ty = NULL;
                if (md->ret.is_void) {
                    ret_ty = ty_void(arena);
                } else if (md->ret.types_len == 1) {
                    ret_ty = ty_from_type_ref(env, md->ret.types[0], mod_name, imps ? imps->imports : NULL, imps ? imps->imports_len : 0, err);
                } else {
                    size_t rn = md->ret.types_len;
                    Ty **items = (Ty **)arena_array(arena, rn, sizeof(Ty *));
                    if (!items) {
                        set_err(err, "out of memory");
                        return NULL;
                    }
                    for (size_t r = 0; r < rn; r++) {
                        items[r] = ty_from_type_ref(env, md->ret.types[r], mod_name, imps ? imps->imports : NULL, imps ? imps->imports_len : 0, err);
                    }
                    ret_ty = ty_tuple(arena, items, rn);
                }
                size_t param_count = md->params_len - 1;
                Ty **param_types = param_count ? (Ty **)arena_array(arena, param_count, sizeof(Ty *)) : NULL;
                Str *param_names = param_count ? (Str *)arena_array(arena, param_count, sizeof(Str)) : NULL;
                for (size_t p = 0; p < param_count; p++) {
                    Param *pp = md->params[p + 1];
                    if (pp->is_this) {
                        set_errf(err, m->path, d->line, d->col, "%.*s: only first param may be this", (int)m->path.len, m->path.data);
                        return NULL;
                    }
                    Ty *pty = ty_from_type_ref(env, pp->typ, mod_name, imps ? imps->imports : NULL, imps ? imps->imports_len : 0, err);
                    if (!pty) return NULL;
                    param_types[p] = pty;
                    param_names[p] = pp->name;
                }
                FunSig *sig = (FunSig *)arena_alloc(arena, sizeof(FunSig));
                if (!sig) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                sig->name = md->name;
                sig->cask = mod_name;
                sig->params = param_types;
                sig->params_len = param_count;
                sig->param_names = param_names;
                sig->param_names_len = param_count;
                sig->ret = ret_ty;
                sig->is_method = true;
                sig->recv_mut = recv_mut;
                sig->owner_class = ci->qname;
                sig->cask_path = m->path;
                sig->extern_stub = false;
                sig->is_pub = md->is_pub;

                // check duplicate method name
                for (size_t k = 0; k < m_i; k++) {
                    if (str_eq(ci->methods[k].name, md->name)) {
                        set_errf(err, m->path, d->line, d->col,
                                 "%.*s: duplicate method '%.*s' in class '%.*s'",
                                 (int)m->path.len, m->path.data,
                                 (int)md->name.len, md->name.data,
                                 (int)d->as.class_decl.name.len, d->as.class_decl.name.data);
                        return NULL;
                    }
                }
                ci->methods[m_i].name = md->name;
                ci->methods[m_i].sig = sig;
                ci->methods[m_i].is_pub = md->is_pub;
            }
        }
    }

    // top-level funs + entry
    size_t findex = 0;
    env->entry = NULL;
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        Str mod_name = env->cask_names[i].name;
        ModuleImport *imps = find_imports(env, mod_name);
        (void)imps;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_FUN) {
                FunDecl *fd = &d->as.fun;
                if (fd->params_len > 0 && fd->params[0]->is_this) {
                    set_errf(err, m->path, d->line, d->col,
                             "%.*s: free function '%.*s' cannot take this/?this",
                             (int)m->path.len, m->path.data,
                             (int)fd->name.len, fd->name.data);
                    return NULL;
                }
                // duplicate check
                for (size_t k = 0; k < findex; k++) {
                    if (str_eq(env->funs[k].cask, mod_name) && str_eq(env->funs[k].name, fd->name)) {
                        set_errf(err, m->path, d->line, d->col,
                                 "%.*s: duplicate function '%.*s'",
                                 (int)m->path.len, m->path.data,
                                 (int)fd->name.len, fd->name.data);
                        return NULL;
                    }
                }
                Ty *ret_ty = NULL;
                if (fd->ret.is_void) {
                    ret_ty = ty_void(arena);
                } else if (fd->ret.types_len == 1) {
                    ret_ty = ty_from_type_ref(env, fd->ret.types[0], mod_name, imps ? imps->imports : NULL, imps ? imps->imports_len : 0, err);
                } else {
                    size_t rn = fd->ret.types_len;
                    Ty **items = (Ty **)arena_array(arena, rn, sizeof(Ty *));
                    if (!items) {
                        set_err(err, "out of memory");
                        return NULL;
                    }
                    for (size_t r = 0; r < rn; r++) {
                        items[r] = ty_from_type_ref(env, fd->ret.types[r], mod_name, imps ? imps->imports : NULL, imps ? imps->imports_len : 0, err);
                    }
                    ret_ty = ty_tuple(arena, items, rn);
                }
                size_t pcount = fd->params_len;
                Ty **params = pcount ? (Ty **)arena_array(arena, pcount, sizeof(Ty *)) : NULL;
                Str *pnames = pcount ? (Str *)arena_array(arena, pcount, sizeof(Str)) : NULL;
                for (size_t p = 0; p < pcount; p++) {
                    Param *pp = fd->params[p];
                    Ty *pty = ty_from_type_ref(env, pp->typ, mod_name, imps ? imps->imports : NULL, imps ? imps->imports_len : 0, err);
                    if (!pty) return NULL;
                    params[p] = pty;
                    pnames[p] = pp->name;
                }
                bool is_empty_body = fd->body && fd->body->kind == STMT_BLOCK && fd->body->as.block_s.stmts_len == 0;
                bool has_internal_prefix = fd->name.len >= 2 && fd->name.data[0] == '_' && fd->name.data[1] == '_';
                // Empty-body "__*" declarations are external C stubs (except stdr intrinsics).
                bool extern_stub = is_empty_body && has_internal_prefix && !str_eq_c(mod_name, "stdr");
                env->funs[findex].name = fd->name;
                env->funs[findex].cask = mod_name;
                env->funs[findex].params = params;
                env->funs[findex].params_len = pcount;
                env->funs[findex].param_names = pnames;
                env->funs[findex].param_names_len = pcount;
                env->funs[findex].ret = ret_ty;
                env->funs[findex].is_method = false;
                env->funs[findex].recv_mut = false;
                env->funs[findex].owner_class.data = NULL;
                env->funs[findex].owner_class.len = 0;
                env->funs[findex].cask_path = m->path;
                env->funs[findex].extern_stub = extern_stub;
                env->funs[findex].is_pub = fd->is_pub;
                findex++;
            }
            if (d->kind == DECL_ENTRY) {
                env->entry = &d->as.entry;
            }
        }
    }

    // typecheck cask globals after functions/classes are registered
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        ModuleGlobals *mg = &env->cask_globals[i];
        if (!mg->len) continue;
        Str mod_name = env->cask_names[i].name;
        ModuleImport *imps = find_imports(env, mod_name);
        Str *imports = imps ? imps->imports : NULL;
        size_t imports_len = imps ? imps->imports_len : 0;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind != DECL_DEF) continue;
            GlobalVar *gv = find_global(mg, d->as.def_decl.name);
            if (!gv) continue;
            if (gv->ty) continue;
            Ty *ty = tc_expr(d->as.def_decl.expr, env, m->path, mod_name, imports, imports_len, err);
            if (!ty) return NULL;
            gv->ty = ty;
        }
    }

    if (!env->entry) {
        set_err(err, "missing entry() in init.yi");
        return NULL;
    }

    return env;
}

static bool cask_in_scope(Str name, Ctx *ctx, Locals *loc) {
    if (locals_lookup(loc, name)) return false;
    if (str_eq(name, ctx->cask_name)) return true;
    for (size_t i = 0; i < ctx->imports_len; i++) {
        if (str_eq(ctx->imports[i], name)) return true;
    }
    return false;
}

static Str shadowed_cask_name(Expr *base, Ctx *ctx, Locals *loc) {
    Str empty = {"", 0};
    if (!base || base->kind != EXPR_IDENT) return empty;
    Str name = base->as.ident.name;
    if (!locals_lookup(loc, name)) return empty;
    if (str_eq(name, ctx->cask_name)) return name;
    for (size_t i = 0; i < ctx->imports_len; i++) {
        if (str_eq(ctx->imports[i], name)) return name;
    }
    return empty;
}

static bool is_mut_lvalue(Expr *e, Ctx *ctx, Locals *loc, GlobalEnv *env) {
    if (!e) return false;
    if (e->kind == EXPR_IDENT) {
        Binding *b = locals_lookup(loc, e->as.ident.name);
        if (b) return b->is_mut && !b->is_const && !b->is_moved;
        ModuleGlobals *mg = find_cask_globals(env, ctx->cask_name);
        GlobalVar *gv = find_global(mg, e->as.ident.name);
        return gv && gv->is_mut;
    }
    if (e->kind == EXPR_MEMBER) {
        return is_mut_lvalue(e->as.member.a, ctx, loc, env);
    }
    if (e->kind == EXPR_INDEX) {
        return is_mut_lvalue(e->as.index.a, ctx, loc, env);
    }
    return false;
}

static bool is_stdr_prelude(Str name) {
    return str_eq_c(name, "write") || str_eq_c(name, "writef") || str_eq_c(name, "readf") || str_eq_c(name, "len") || str_eq_c(name, "slice") || str_eq_c(name, "concat") || str_eq_c(name, "str_concat") || str_eq_c(name, "char_code") || str_eq_c(name, "is_null") || str_eq_c(name, "str") || str_eq_c(name, "num");
}

static Ty *tc_expr_inner(Expr *e, Ctx *ctx, Locals *loc, GlobalEnv *env, Diag *err);
static void tc_stmt_inner(Stmt *s, Ctx *ctx, Locals *loc, GlobalEnv *env, Ty *ret_ty, Diag *err);
static void tc_pat(Pat *pat, Ty *scrut_ty, Ctx *ctx, Locals *loc, GlobalEnv *env, Diag *err);
static Ty *tc_call(Expr *call_expr, Ctx *ctx, Locals *loc, GlobalEnv *env, Diag *err);

static void set_arg_diag(Diag *err, Ctx *ctx, Expr *arg) {
    if (!err || !ctx || !arg || err->line != 0) {
        return;
    }
    err->path = ctx->cask_path.data;
    err->line = arg->line;
    err->col = arg->col;
}

static Ty *numeric_result(Arena *arena, Ty *a, Ty *b, Str path, int line, int col, const char *op, Diag *err) {
    if (!ty_is_numeric(a) || !ty_is_numeric(b)) {
        set_errf(err, path, line, col, "operator %s expects numeric types", op);
        return NULL;
    }
    return ty_prim(arena, "num");
}

static Ty *plus_result(Arena *arena, Ty *a, Ty *b, Str path, int line, int col, const char *op, Diag *err) {
    if (ty_is_string(a) || ty_is_string(b)) {
        if (ty_is_void(a) || ty_is_void(b)) {
            set_errf(err, path, line, col, "operator %s does not accept void", op);
            return NULL;
        }
        return ty_prim(arena, "string");
    }
    return numeric_result(arena, a, b, path, line, col, op, err);
}

static bool is_assign_op(TokKind op) {
    switch (op) {
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

static bool is_compound_assign_op(TokKind op) {
    switch (op) {
        case TOK_PLUSEQ:
        case TOK_MINUSEQ:
        case TOK_STAREQ:
        case TOK_SLASHEQ:
            return true;
        default:
            return false;
    }
}

static Ty *tc_assignment_result(Arena *arena, Ty *lhs, Ty *rhs, TokKind op, Str path, int line, int col, Diag *err) {
    if (!lhs || !rhs) {
        return NULL;
    }
    if (!is_compound_assign_op(op)) {
        if (!ensure_assignable(arena, lhs, rhs, path, "assignment", err)) {
            if (err && err->line == 0) {
                err->line = line;
                err->col = col;
            }
            return NULL;
        }
        return unify(arena, lhs, rhs, path, "assignment", NULL, err);
    }
    if (ty_is_nullable(lhs) || ty_is_nullable(rhs)) {
        set_errf(err, path, line, col, "operator on nullable value");
        return NULL;
    }
    Ty *nr = NULL;
    if (op == TOK_PLUSEQ) {
        nr = plus_result(arena, ty_strip_nullable(lhs), ty_strip_nullable(rhs), path, line, col, tok_kind_name(op), err);
    } else {
        nr = numeric_result(arena, ty_strip_nullable(lhs), ty_strip_nullable(rhs), path, line, col, tok_kind_name(op), err);
    }
    if (!nr) {
        return NULL;
    }
    return unify(arena, lhs, nr, path, tok_kind_name(op), NULL, err);
}

static Ty *tc_call(Expr *call_expr, Ctx *ctx, Locals *loc, GlobalEnv *env, Diag *err) {
    Expr *fn = call_expr->as.call.fn;
    size_t argc = call_expr->as.call.args_len;
    Expr **args = call_expr->as.call.args;

    // cask-qualified calls
    if (fn && fn->kind == EXPR_MEMBER && fn->as.member.a && fn->as.member.a->kind == EXPR_IDENT) {
        Str mod = fn->as.member.a->as.ident.name;
        if (cask_in_scope(mod, ctx, loc)) {
            Str name = fn->as.member.name;
            FunSig *sig = find_fun(env, mod, name);
            if (!sig) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: unknown %.*s.%.*s",
                         (int)ctx->cask_path.len, ctx->cask_path.data,
                         (int)mod.len, mod.data,
                         (int)name.len, name.data);
                return NULL;
            }
            if (is_cross_cask(ctx->cask_name, mod) && !sig->is_pub) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: function '%.*s.%.*s' is not public",
                         (int)ctx->cask_path.len, ctx->cask_path.data,
                         (int)mod.len, mod.data,
                         (int)name.len, name.data);
                return NULL;
            }
            if (argc != sig->params_len) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: '%.*s.%.*s' expects %zu args",
                         (int)ctx->cask_path.len, ctx->cask_path.data,
                         (int)mod.len, mod.data,
                         (int)name.len, name.data,
                         sig->params_len);
                return NULL;
            }
            Subst subst = subst_init();
            for (size_t i = 0; i < argc; i++) {
                Ty *at = tc_expr_inner(args[i], ctx, loc, env, err);
                if (!ensure_assignable(env->arena, sig->params[i], at, ctx->cask_path, "arg", err)) {
                    set_arg_diag(err, ctx, args[i]);
                    subst_free(&subst);
                    return NULL;
                }
                unify(env->arena, sig->params[i], at, ctx->cask_path, "arg", &subst, err);
                if (err && err->message) {
                    subst_free(&subst);
                    return NULL;
                }
            }
            Ty *ret = ty_apply_subst(env->arena, sig->ret, &subst);
            subst_free(&subst);
            return ret;
        }
        if (!locals_lookup(loc, mod)) {
            ModuleGlobals *mg = find_cask_globals(env, ctx->cask_name);
            GlobalVar *gv = mg ? find_global(mg, mod) : NULL;
            if (gv) {
                // treat as method call on a global value
                goto method_call;
            }
            set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: unknown name '%.*s' (cask not in scope)",
                     (int)ctx->cask_path.len, ctx->cask_path.data,
                     (int)mod.len, mod.data);
            return NULL;
        }
    }

    // method call
method_call:
    if (fn && fn->kind == EXPR_MEMBER) {
        Expr *base = fn->as.member.a;
        Ty *base_ty = tc_expr_inner(base, ctx, loc, env, err);
        if (!base_ty) return NULL;
        if (ty_is_nullable(base_ty)) {
            set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: call on nullable value", (int)ctx->cask_path.len, ctx->cask_path.data);
            return NULL;
        }
        base_ty = ty_strip_nullable(base_ty);
        Str mname = fn->as.member.name;

        if (base_ty->tag == TY_ARRAY && base_ty->elem) {
            if (str_eq_c(mname, "add")) {
                if (argc != 1) {
                    set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: array.add expects 1 arg", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                if (!is_mut_lvalue(base, ctx, loc, env)) {
                    set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: array.add requires mutable binding", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                Ty *ta = tc_expr_inner(args[0], ctx, loc, env, err);
                ensure_assignable(env->arena, base_ty->elem, ta, ctx->cask_path, "array.add", err);
                unify(env->arena, base_ty->elem, ta, ctx->cask_path, "array.add", NULL, err);
                return ty_void(env->arena);
            }
            if (str_eq_c(mname, "remove")) {
                if (argc != 1) {
                    set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: array.remove expects 1 arg", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                if (!is_mut_lvalue(base, ctx, loc, env)) {
                    set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: array.remove requires mutable binding", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                Ty *ti = tc_expr_inner(args[0], ctx, loc, env, err);
                unify(env->arena, ti, ty_prim(env->arena, "num"), ctx->cask_path, "array.remove index", NULL, err);
                return base_ty->elem;
            }
            set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: unknown array method '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)mname.len, mname.data);
            return NULL;
        }

        if (base_ty->tag == TY_PRIM && (str_eq_c(base_ty->name, "bool") || str_eq_c(base_ty->name, "num"))) {
            if (str_eq_c(mname, "to_string")) {
                if (argc != 0) {
                    set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: to_string takes no args", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                return ty_prim(env->arena, "string");
            }
        }

        if (base_ty->tag == TY_CLASS) {
            ClassInfo *ci = find_class(env, base_ty->name);
            if (!ci) return NULL;
            MethodEntry *method = NULL;
            for (size_t i = 0; i < ci->methods_len; i++) {
                if (str_eq(ci->methods[i].name, mname)) {
                    method = &ci->methods[i];
                    break;
                }
            }
            if (!method) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "'%.*s' has no method '%.*s'",
                         (int)ci->name.len, ci->name.data,
                         (int)mname.len, mname.data);
                return NULL;
            }
            if (is_cross_cask(ctx->cask_name, ci->cask) && !method->is_pub) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: method '%.*s.%.*s' is not public",
                         (int)ctx->cask_path.len, ctx->cask_path.data,
                         (int)ci->name.len, ci->name.data,
                         (int)mname.len, mname.data);
                return NULL;
            }
            FunSig *sig = method->sig;
            if (sig->recv_mut && !is_mut_lvalue(base, ctx, loc, env)) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: method '%.*s.%.*s' requires mutable receiver",
                         (int)ctx->cask_path.len, ctx->cask_path.data,
                         (int)ci->name.len, ci->name.data,
                         (int)mname.len, mname.data);
                return NULL;
            }
            if (argc != sig->params_len) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: '%.*s.%.*s' expects %zu args",
                         (int)ctx->cask_path.len, ctx->cask_path.data,
                         (int)ci->name.len, ci->name.data,
                         (int)mname.len, mname.data,
                         sig->params_len);
                return NULL;
            }
            Subst subst = subst_init();
            for (size_t i = 0; i < argc; i++) {
                Ty *at = tc_expr_inner(args[i], ctx, loc, env, err);
                if (!ensure_assignable(env->arena, sig->params[i], at, ctx->cask_path, "arg", err)) {
                    set_arg_diag(err, ctx, args[i]);
                    subst_free(&subst);
                    return NULL;
                }
                unify(env->arena, sig->params[i], at, ctx->cask_path, "arg", &subst, err);
                if (err && err->message) {
                    subst_free(&subst);
                    return NULL;
                }
            }
            Ty *ret = ty_apply_subst(env->arena, sig->ret, &subst);
            subst_free(&subst);
            return ret;
        }

        Str shadow = shadowed_cask_name(base, ctx, loc);
        if (shadow.len) {
            set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: '%.*s' is a local binding that shadows cask '%.*s'",
                     (int)ctx->cask_path.len, ctx->cask_path.data,
                     (int)shadow.len, shadow.data,
                     (int)shadow.len, shadow.data);
            return NULL;
        }

        /* Infix-call fallback: x !fn args  →  fn(x, args...) */
        {
            FunSig *sig = find_fun(env, ctx->cask_name, mname);
            if (!sig && is_stdr_prelude(mname)) {
                for (size_t i = 0; i < ctx->imports_len; i++) {
                    if (str_eq_c(ctx->imports[i], "stdr")) {
                        sig = find_fun(env, str_from_c("stdr"), mname);
                        break;
                    }
                }
            }
            if (sig) {
                size_t total = 1 + argc;
                if (total != sig->params_len) {
                    set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: '%.*s' expects %zu args, got %zu",
                             (int)ctx->cask_path.len, ctx->cask_path.data,
                             (int)mname.len, mname.data, sig->params_len, total);
                    return NULL;
                }
                Subst subst = subst_init();
                if (!ensure_assignable(env->arena, sig->params[0], base_ty, ctx->cask_path, "arg", err)) {
                    set_arg_diag(err, ctx, base);
                    subst_free(&subst);
                    return NULL;
                }
                unify(env->arena, sig->params[0], base_ty, ctx->cask_path, "arg", &subst, err);
                if (err && err->message) { subst_free(&subst); return NULL; }
                for (size_t i = 0; i < argc; i++) {
                    Ty *at = tc_expr_inner(args[i], ctx, loc, env, err);
                    if (!ensure_assignable(env->arena, sig->params[1 + i], at, ctx->cask_path, "arg", err)) {
                        set_arg_diag(err, ctx, args[i]);
                        subst_free(&subst);
                        return NULL;
                    }
                    unify(env->arena, sig->params[1 + i], at, ctx->cask_path, "arg", &subst, err);
                    if (err && err->message) { subst_free(&subst); return NULL; }
                }
                Ty *ret = ty_apply_subst(env->arena, sig->ret, &subst);
                subst_free(&subst);
                return ret;
            }
        }

        set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: cannot call member on value", (int)ctx->cask_path.len, ctx->cask_path.data);
        return NULL;
    }

    if (fn && fn->kind == EXPR_IDENT) {
        Str fname = fn->as.ident.name;
        Binding *b = locals_lookup(loc, fname);
        if (b) {
            Ty *fn_ty = tc_expr_inner(fn, ctx, loc, env, err);
            if (!fn_ty) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: unknown function '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)fname.len, fname.data);
                return NULL;
            }
            if (ty_is_any(fn_ty)) {
                for (size_t i = 0; i < argc; i++) tc_expr_inner(args[i], ctx, loc, env, err);
                return fn_ty;
            }
            if (fn_ty->tag != TY_FN) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: unknown function '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)fname.len, fname.data);
                return NULL;
            }
            if (argc != fn_ty->params_len) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: call expects %zu args", (int)ctx->cask_path.len, ctx->cask_path.data, fn_ty->params_len);
                return NULL;
            }
            Subst subst = subst_init();
            for (size_t i = 0; i < argc; i++) {
                Ty *at = tc_expr_inner(args[i], ctx, loc, env, err);
                unify(env->arena, fn_ty->params[i], at, ctx->cask_path, "fn value call", &subst, err);
            }
            Ty *ret = ty_apply_subst(env->arena, fn_ty->ret, &subst);
            subst_free(&subst);
            return ret;
        }

        ModuleGlobals *mg = find_cask_globals(env, ctx->cask_name);
        GlobalVar *gv = find_global(mg, fname);
        if (gv) {
            if (!gv->ty) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: global '%.*s' used before definition",
                         (int)ctx->cask_path.len, ctx->cask_path.data, (int)fname.len, fname.data);
                return NULL;
            }
            if (gv->ty->tag != TY_FN) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: unknown function '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)fname.len, fname.data);
                return NULL;
            }
            if (argc != gv->ty->params_len) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: call expects %zu args", (int)ctx->cask_path.len, ctx->cask_path.data, gv->ty->params_len);
                return NULL;
            }
            Subst subst = subst_init();
            for (size_t i = 0; i < argc; i++) {
                Ty *at = tc_expr_inner(args[i], ctx, loc, env, err);
                unify(env->arena, gv->ty->params[i], at, ctx->cask_path, "fn value call", &subst, err);
            }
            Ty *ret = ty_apply_subst(env->arena, gv->ty->ret, &subst);
            subst_free(&subst);
            return ret;
        }

        if (str_eq_c(fname, "str")) {
            if (argc != 1) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: str expects 1 arg", (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            tc_expr_inner(args[0], ctx, loc, env, err);
            return ty_prim(env->arena, "string");
        }

        FunSig *sig = find_fun(env, ctx->cask_name, fname);
        if (!sig && is_stdr_prelude(fname)) {
            bool allow = str_eq_c(ctx->cask_name, "stdr");
            for (size_t i = 0; i < ctx->imports_len && !allow; i++) {
                if (str_eq_c(ctx->imports[i], "stdr")) allow = true;
            }
            if (allow) {
                sig = find_fun(env, str_from_c("stdr"), fname);
            }
        }
        if (!sig) {
            Ty *fn_ty = tc_expr_inner(fn, ctx, loc, env, err);
            if (!fn_ty || fn_ty->tag != TY_FN) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: unknown function '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)fname.len, fname.data);
                return NULL;
            }
            if (argc != fn_ty->params_len) {
                set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: call expects %zu args", (int)ctx->cask_path.len, ctx->cask_path.data, fn_ty->params_len);
                return NULL;
            }
            Subst subst = subst_init();
            for (size_t i = 0; i < argc; i++) {
                Ty *at = tc_expr_inner(args[i], ctx, loc, env, err);
                unify(env->arena, fn_ty->params[i], at, ctx->cask_path, "fn value call", &subst, err);
            }
            Ty *ret = ty_apply_subst(env->arena, fn_ty->ret, &subst);
            subst_free(&subst);
            return ret;
        }

        if (argc != sig->params_len) {
            set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: '%.*s' expects %zu args",
                     (int)ctx->cask_path.len, ctx->cask_path.data, (int)fname.len, fname.data, sig->params_len);
            return NULL;
        }
        Subst subst = subst_init();
        for (size_t i = 0; i < argc; i++) {
            Ty *at = tc_expr_inner(args[i], ctx, loc, env, err);
            if (!ensure_assignable(env->arena, sig->params[i], at, ctx->cask_path, "arg", err)) {
                set_arg_diag(err, ctx, args[i]);
                subst_free(&subst);
                return NULL;
            }
            unify(env->arena, sig->params[i], at, ctx->cask_path, "arg", &subst, err);
        }
        Ty *ret = ty_apply_subst(env->arena, sig->ret, &subst);
        subst_free(&subst);
        return ret;
    }

    Ty *fn_ty = tc_expr_inner(fn, ctx, loc, env, err);
    if (!fn_ty || fn_ty->tag != TY_FN) {
        set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: unsupported call form", (int)ctx->cask_path.len, ctx->cask_path.data);
        return NULL;
    }
    if (argc != fn_ty->params_len) {
        set_errf(err, ctx->cask_path, fn->line, fn->col, "%.*s: call expects %zu args", (int)ctx->cask_path.len, ctx->cask_path.data, fn_ty->params_len);
        return NULL;
    }
    Subst subst = subst_init();
    for (size_t i = 0; i < argc; i++) {
        Ty *at = tc_expr_inner(args[i], ctx, loc, env, err);
        unify(env->arena, fn_ty->params[i], at, ctx->cask_path, "fn value call", &subst, err);
    }
    Ty *ret = ty_apply_subst(env->arena, fn_ty->ret, &subst);
    subst_free(&subst);
    return ret;
}

static Ty *tc_expr_inner(Expr *e, Ctx *ctx, Locals *loc, GlobalEnv *env, Diag *err) {
    if (!e) return NULL;
    switch (e->kind) {
        case EXPR_INT:
        case EXPR_FLOAT:
            return ty_prim(env->arena, "num");
        case EXPR_BOOL:
            return ty_prim(env->arena, "bool");
        case EXPR_NULL:
            return ty_null(env->arena);
        case EXPR_STR:
            if (e->as.str_lit.parts) {
                for (size_t i = 0; i < e->as.str_lit.parts->len; i++) {
                    StrPart *part = &e->as.str_lit.parts->parts[i];
                    if (part->kind == STR_PART_EXPR && part->as.expr) {
                        tc_expr_inner(part->as.expr, ctx, loc, env, err);
                    } else if (part->kind != STR_PART_TEXT) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: invalid interpolation part", (int)ctx->cask_path.len, ctx->cask_path.data);
                        return NULL;
                    }
                }
            }
            return ty_prim(env->arena, "string");
        case EXPR_TUPLE: {
            size_t n = e->as.tuple_lit.items_len;
            Ty **items = (Ty **)arena_array(env->arena, n, sizeof(Ty *));
            if (!items) return NULL;
            for (size_t i = 0; i < n; i++) {
                items[i] = tc_expr_inner(e->as.tuple_lit.items[i], ctx, loc, env, err);
            }
            return ty_tuple(env->arena, items, n);
        }
        case EXPR_IDENT: {
            Binding *b = locals_lookup(loc, e->as.ident.name);
            if (b) {
                if (b->is_moved) {
                    set_errf(err, ctx->cask_path, e->line, e->col,
                             "%.*s: use of moved value '%.*s'",
                             (int)ctx->cask_path.len, ctx->cask_path.data,
                             (int)e->as.ident.name.len, e->as.ident.name.data);
                    return NULL;
                }
                return b->ty;
            }
            if (cask_in_scope(e->as.ident.name, ctx, loc)) {
                return ty_mod(env->arena, e->as.ident.name);
            }
            ModuleGlobals *mg = find_cask_globals(env, ctx->cask_name);
            GlobalVar *gv = find_global(mg, e->as.ident.name);
            if (gv) {
                if (!gv->ty) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: global '%.*s' used before definition",
                             (int)ctx->cask_path.len, ctx->cask_path.data,
                             (int)e->as.ident.name.len, e->as.ident.name.data);
                    return NULL;
                }
                return gv->ty;
            }
            FunSig *sig = find_fun(env, ctx->cask_name, e->as.ident.name);
            if (!sig && is_stdr_prelude(e->as.ident.name)) {
                bool allow = str_eq_c(ctx->cask_name, "stdr");
                for (size_t i = 0; i < ctx->imports_len && !allow; i++) {
                    if (str_eq_c(ctx->imports[i], "stdr")) allow = true;
                }
                if (allow) sig = find_fun(env, str_from_c("stdr"), e->as.ident.name);
            }
            if (sig) {
                return ty_fn(env->arena, sig->params, sig->params_len, sig->ret);
            }
            set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: unknown name '%.*s'",
                     (int)ctx->cask_path.len, ctx->cask_path.data,
                     (int)e->as.ident.name.len, e->as.ident.name.data);
            return NULL;
        }
        case EXPR_ARRAY: {
            if (e->as.array_lit.items_len == 0) {
                if (!e->as.array_lit.annot) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: cannot infer type of empty array []", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                Ty *annot = ty_from_type_ref(env, e->as.array_lit.annot, ctx->cask_name, ctx->imports, ctx->imports_len, err);
                if (!annot) return NULL;
                if (annot->tag != TY_ARRAY || !annot->elem) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: empty array annotation must be array type like [num]", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                return annot;
            }
            Ty *t0 = tc_expr_inner(e->as.array_lit.items[0], ctx, loc, env, err);
            for (size_t i = 1; i < e->as.array_lit.items_len; i++) {
                Ty *ti = tc_expr_inner(e->as.array_lit.items[i], ctx, loc, env, err);
                t0 = unify(env->arena, t0, ti, ctx->cask_path, "array literal", NULL, err);
            }
            if (e->as.array_lit.annot) {
                Ty *annot = ty_from_type_ref(env, e->as.array_lit.annot, ctx->cask_name, ctx->imports, ctx->imports_len, err);
                if (!annot) return NULL;
                if (annot->tag != TY_ARRAY || !annot->elem) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: array annotation must be array type like [num]", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                t0 = unify(env->arena, t0, annot->elem, ctx->cask_path, "array annotation", NULL, err);
            }
            return ty_array(env->arena, t0);
        }
        case EXPR_DICT: {
            Ty *key_ty = ty_prim(env->arena, "string");
            if (e->as.dict_lit.pairs_len == 0) {
                if (!e->as.dict_lit.annot) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: cannot infer type of empty dict []", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                Ty *annot = ty_from_type_ref(env, e->as.dict_lit.annot, ctx->cask_name, ctx->imports, ctx->imports_len, err);
                if (!annot) return NULL;
                if (annot->tag != TY_DICT || !annot->key || !annot->elem) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: empty dict annotation must be dict type like [string -> num]", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                return annot;
            }
            Ty *val_ty = tc_expr_inner(e->as.dict_lit.vals[0], ctx, loc, env, err);
            for (size_t i = 0; i < e->as.dict_lit.pairs_len; i++) {
                Ty *kt = tc_expr_inner(e->as.dict_lit.keys[i], ctx, loc, env, err);
                unify(env->arena, kt, key_ty, ctx->cask_path, "dict key", NULL, err);
                if (i > 0) {
                    Ty *vt = tc_expr_inner(e->as.dict_lit.vals[i], ctx, loc, env, err);
                    val_ty = unify(env->arena, val_ty, vt, ctx->cask_path, "dict literal", NULL, err);
                }
            }
            if (e->as.dict_lit.annot) {
                Ty *annot = ty_from_type_ref(env, e->as.dict_lit.annot, ctx->cask_name, ctx->imports, ctx->imports_len, err);
                if (!annot) return NULL;
                if (annot->tag != TY_DICT || !annot->elem) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: dict annotation must be dict type like [string -> num]", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                val_ty = unify(env->arena, val_ty, annot->elem, ctx->cask_path, "dict annotation", NULL, err);
            }
            return ty_dict(env->arena, key_ty, val_ty);
        }
        case EXPR_UNARY: {
            Ty *tx = tc_expr_inner(e->as.unary.x, ctx, loc, env, err);
            if (e->as.unary.op == TOK_BANG) {
                if (ty_is_nullable(tx)) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: ! on nullable value", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                unify(env->arena, tx, ty_prim(env->arena, "bool"), ctx->cask_path, "!", NULL, err);
                return ty_prim(env->arena, "bool");
            }
            if (e->as.unary.op == TOK_MINUS) {
                if (ty_is_nullable(tx)) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: unary - on nullable value", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                if (!ty_is_numeric(ty_strip_nullable(tx))) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: unary - expects numeric", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                return ty_prim(env->arena, "num");
            }
            if (e->as.unary.op == TOK_HASH) {
                if (ty_is_nullable(tx)) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: # on nullable value", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                if (tx->tag == TY_ARRAY || (tx->tag == TY_PRIM && str_eq_c(tx->name, "string"))) {
                    return ty_prim(env->arena, "num");
                }
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: # expects array or string", (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            return tx;
        }
        case EXPR_BINARY: {
            Ty *ta = tc_expr_inner(e->as.binary.a, ctx, loc, env, err);
            Ty *tb = tc_expr_inner(e->as.binary.b, ctx, loc, env, err);
            if (!ta || !tb) {
                return NULL;
            }
            TokKind op = e->as.binary.op;
            if (op == TOK_QQ) {
                if (ty_is_void(ta) || ty_is_void(tb)) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: ?? operands cannot be void", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                if (ty_is_null(ta)) return tb;
                if (ty_is_nullable(ta)) {
                    return unify(env->arena, ty_strip_nullable(ta), tb, ctx->cask_path, "??", NULL, err);
                }
                return unify(env->arena, ta, tb, ctx->cask_path, "??", NULL, err);
            }
            if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR || op == TOK_SLASH || op == TOK_PERCENT) {
                if (ty_is_nullable(ta) || ty_is_nullable(tb)) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "operator on nullable value");
                    return NULL;
                }
                if (op == TOK_PLUS) {
                    return plus_result(env->arena, ty_strip_nullable(ta), ty_strip_nullable(tb), ctx->cask_path, e->line, e->col, tok_kind_name(op), err);
                }
                return numeric_result(env->arena, ty_strip_nullable(ta), ty_strip_nullable(tb), ctx->cask_path, e->line, e->col, tok_kind_name(op), err);
            }
            if (op == TOK_LT || op == TOK_LTE || op == TOK_GT || op == TOK_GTE) {
                if (ty_is_nullable(ta) || ty_is_nullable(tb)) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: comparison on nullable value", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                if (!ty_is_numeric(ty_strip_nullable(ta)) || !ty_is_numeric(ty_strip_nullable(tb))) {
                    char da[64];
                    char db[64];
                    ty_desc(ta, da, sizeof(da));
                    ty_desc(tb, db, sizeof(db));
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: comparison expects numeric types (left: %s, right: %s)",
                             (int)ctx->cask_path.len, ctx->cask_path.data, da, db);
                    return NULL;
                }
                return ty_prim(env->arena, "bool");
            }
            if (op == TOK_ANDAND || op == TOK_OROR) {
                if (ty_is_void(ta) || ty_is_void(tb)) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: logical op on void value", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                return ty_prim(env->arena, "bool");
            }
            if (op == TOK_EQEQ || op == TOK_NEQ) {
                unify(env->arena, ta, tb, ctx->cask_path, tok_kind_name(op), NULL, err);
                return ty_prim(env->arena, "bool");
            }
            set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: unknown binary op", (int)ctx->cask_path.len, ctx->cask_path.data);
            return NULL;
        }
        case EXPR_ASSIGN: {
            TokKind op = is_assign_op(e->as.assign.op) ? e->as.assign.op : TOK_EQ;
            if (e->as.assign.target->kind == EXPR_IDENT) {
                Binding *b = locals_lookup(loc, e->as.assign.target->as.ident.name);
                if (b) {
                    if (b->is_const) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: cannot assign to const '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)e->as.assign.target->as.ident.name.len, e->as.assign.target->as.ident.name.data);
                        return NULL;
                    }
                    if (!b->is_mut) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: cannot assign to immutable '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)e->as.assign.target->as.ident.name.len, e->as.assign.target->as.ident.name.data);
                        return NULL;
                    }
                    Ty *tv = tc_expr_inner(e->as.assign.value, ctx, loc, env, err);
                    Ty *new_ty = tc_assignment_result(env->arena, b->ty, tv, op, ctx->cask_path, e->line, e->col, err);
                    if (!new_ty) {
                        return NULL;
                    }
                    b->ty = new_ty;
                    b->is_moved = false;
                    return new_ty;
                }
                ModuleGlobals *mg = find_cask_globals(env, ctx->cask_name);
                GlobalVar *gv = find_global(mg, e->as.assign.target->as.ident.name);
                if (gv) {
                    if (!gv->is_mut) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: cannot assign to immutable '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)e->as.assign.target->as.ident.name.len, e->as.assign.target->as.ident.name.data);
                        return NULL;
                    }
                    if (!gv->ty) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: global '%.*s' used before definition", (int)ctx->cask_path.len, ctx->cask_path.data, (int)e->as.assign.target->as.ident.name.len, e->as.assign.target->as.ident.name.data);
                        return NULL;
                    }
                    Ty *tv = tc_expr_inner(e->as.assign.value, ctx, loc, env, err);
                    return tc_assignment_result(env->arena, gv->ty, tv, op, ctx->cask_path, e->line, e->col, err);
                }
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: assign to unknown '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)e->as.assign.target->as.ident.name.len, e->as.assign.target->as.ident.name.data);
                return NULL;
            }
            if (e->as.assign.target->kind == EXPR_MEMBER || e->as.assign.target->kind == EXPR_INDEX) {
                Expr *base = (e->as.assign.target->kind == EXPR_MEMBER) ? e->as.assign.target->as.member.a : e->as.assign.target->as.index.a;
                if (!is_mut_lvalue(base, ctx, loc, env)) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: cannot mutate through immutable binding", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return NULL;
                }
                Ty *tt = tc_expr_inner(e->as.assign.target, ctx, loc, env, err);
                Ty *tv = tc_expr_inner(e->as.assign.value, ctx, loc, env, err);
                return tc_assignment_result(env->arena, tt, tv, op, ctx->cask_path, e->line, e->col, err);
            }
            set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: invalid assignment target", (int)ctx->cask_path.len, ctx->cask_path.data);
            return NULL;
        }
        case EXPR_MEMBER: {
            Ty *ta = tc_expr_inner(e->as.member.a, ctx, loc, env, err);
            if (!ta) {
                return NULL;
            }
            if (ty_is_nullable(ta)) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: member access on nullable value", (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            ta = ty_strip_nullable(ta);
            if (!ta) {
                return NULL;
            }
            if (ta->tag == TY_MOD) {
                ModuleConsts *mc = find_cask_consts(env, ta->name);
                if (mc) {
                    ConstEntry *ce = find_const(mc, e->as.member.name);
                    if (ce) {
                        if (is_cross_cask(ctx->cask_name, ta->name) && !ce->is_pub) {
                            set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: const '%.*s.%.*s' is not public",
                                     (int)ctx->cask_path.len, ctx->cask_path.data,
                                     (int)ta->name.len, ta->name.data,
                                     (int)e->as.member.name.len, e->as.member.name.data);
                            return NULL;
                        }
                        return ce->val.ty;
                    }
                }
                ModuleGlobals *mg = find_cask_globals(env, ta->name);
                GlobalVar *gv = find_global(mg, e->as.member.name);
                if (gv) {
                    if (is_cross_cask(ctx->cask_name, ta->name) && !gv->is_pub) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: global '%.*s.%.*s' is not public",
                                 (int)ctx->cask_path.len, ctx->cask_path.data,
                                 (int)ta->name.len, ta->name.data,
                                 (int)e->as.member.name.len, e->as.member.name.data);
                        return NULL;
                    }
                    if (!gv->ty) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: global '%.*s' used before definition",
                                 (int)ctx->cask_path.len, ctx->cask_path.data,
                                 (int)e->as.member.name.len, e->as.member.name.data);
                        return NULL;
                    }
                    return gv->ty;
                }
                FunSig *mod_fun = find_fun(env, ta->name, e->as.member.name);
                if (mod_fun) {
                    if (is_cross_cask(ctx->cask_name, ta->name) && !mod_fun->is_pub) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: function '%.*s.%.*s' is not public",
                                 (int)ctx->cask_path.len, ctx->cask_path.data,
                                 (int)ta->name.len, ta->name.data,
                                 (int)e->as.member.name.len, e->as.member.name.data);
                        return NULL;
                    }
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: cask function '%.*s.%.*s' must be called", (int)ctx->cask_path.len, ctx->cask_path.data, (int)ta->name.len, ta->name.data, (int)e->as.member.name.len, e->as.member.name.data);
                    return NULL;
                }
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: unknown cask member '%.*s.%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)ta->name.len, ta->name.data, (int)e->as.member.name.len, e->as.member.name.data);
                return NULL;
            }
            if (ta->tag == TY_CLASS) {
                ClassInfo *ci = find_class(env, ta->name);
                if (!ci) return NULL;
                if (str_eq_c(ci->vis, "lock")) {
                    bool in_same_file = str_eq(ctx->cask_path, ci->cask_path);
                    bool in_own_method = ctx->has_current_class && str_eq(ctx->current_class, ci->qname);
                    if (!(in_same_file || in_own_method)) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: cannot access field '%.*s' of lock class '%.*s'",
                                 (int)ctx->cask_path.len, ctx->cask_path.data,
                                 (int)e->as.member.name.len, e->as.member.name.data,
                                 (int)ci->name.len, ci->name.data);
                        return NULL;
                    }
                }
                for (size_t i = 0; i < ci->fields_len; i++) {
                    if (str_eq(ci->fields[i].name, e->as.member.name)) {
                        if (is_cross_cask(ctx->cask_name, ci->cask) && !ci->fields[i].is_pub) {
                            set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: field '%.*s.%.*s' is not public",
                                     (int)ctx->cask_path.len, ctx->cask_path.data,
                                     (int)ci->name.len, ci->name.data,
                                     (int)e->as.member.name.len, e->as.member.name.data);
                            return NULL;
                        }
                        return ci->fields[i].ty;
                    }
                }
                for (size_t i = 0; i < ci->methods_len; i++) {
                    if (str_eq(ci->methods[i].name, e->as.member.name)) {
                        if (is_cross_cask(ctx->cask_name, ci->cask) && !ci->methods[i].is_pub) {
                            set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: method '%.*s.%.*s' is not public",
                                     (int)ctx->cask_path.len, ctx->cask_path.data,
                                     (int)ci->name.len, ci->name.data,
                                     (int)e->as.member.name.len, e->as.member.name.data);
                            return NULL;
                        }
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: method '%.*s' must be called", (int)ctx->cask_path.len, ctx->cask_path.data, (int)e->as.member.name.len, e->as.member.name.data);
                        return NULL;
                    }
                }
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: unknown member '%.*s' on class", (int)ctx->cask_path.len, ctx->cask_path.data, (int)e->as.member.name.len, e->as.member.name.data);
                return NULL;
            }
            Str shadow = shadowed_cask_name(e->as.member.a, ctx, loc);
            if (shadow.len) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: '%.*s' shadows cask '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)shadow.len, shadow.data, (int)shadow.len, shadow.data);
                return NULL;
            }
            set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: member access on non-object", (int)ctx->cask_path.len, ctx->cask_path.data);
            return NULL;
        }
        case EXPR_INDEX: {
            Ty *ta = tc_expr_inner(e->as.index.a, ctx, loc, env, err);
            Ty *ti = tc_expr_inner(e->as.index.i, ctx, loc, env, err);
            if (!ta || !ti) return NULL;
            if (ty_is_nullable(ta)) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: indexing nullable value", (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            ta = ty_strip_nullable(ta);
            if (!ta) return NULL;
            if (ta->tag == TY_DICT) {
                unify(env->arena, ti, ty_prim(env->arena, "string"), ctx->cask_path, "dict index", NULL, err);
            } else if (ta->tag != TY_PRIM || !str_eq_c(ta->name, "any")) {
                unify(env->arena, ti, ty_prim(env->arena, "num"), ctx->cask_path, "index", NULL, err);
            }
            if (ty_is_nullable(ta)) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: indexing nullable value", (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            ta = ty_strip_nullable(ta);
            if (!ta) return NULL;
            if (ta->tag == TY_ARRAY && ta->elem) return ty_nullable(env->arena, ta->elem);
            if (ta->tag == TY_DICT && ta->elem) return ty_nullable(env->arena, ta->elem);
            if (ta->tag == TY_TUPLE && ta->items) {
                if (e->as.index.i->kind == EXPR_INT) {
                    long long idx = e->as.index.i->as.int_lit.v;
                    if (idx < 0 || (size_t)idx >= ta->items_len) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: tuple index out of range", (int)ctx->cask_path.len, ctx->cask_path.data);
                        return NULL;
                    }
                    return ta->items[idx];
                }
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: tuple index must be integer literal", (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            if (ta->tag == TY_PRIM && str_eq_c(ta->name, "string")) return ty_prim(env->arena, "string");
            if (ta->tag == TY_PRIM && str_eq_c(ta->name, "any")) return ty_prim(env->arena, "any");
            set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: indexing requires array, dict, or string", (int)ctx->cask_path.len, ctx->cask_path.data);
            return NULL;
        }
        case EXPR_TERNARY: {
            Ty *tc = tc_expr_inner(e->as.ternary.cond, ctx, loc, env, err);
            if (ty_is_void(tc)) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: ternary condition cannot be void", (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            Ty *ta = tc_expr_inner(e->as.ternary.then_expr, ctx, loc, env, err);
            Ty *tb = tc_expr_inner(e->as.ternary.else_expr, ctx, loc, env, err);
            return unify(env->arena, ta, tb, ctx->cask_path, "ternary", NULL, err);
        }
        case EXPR_MATCH: {
            Ty *scrut_ty = tc_expr_inner(e->as.match_expr.scrut, ctx, loc, env, err);
            Ty *arm_ty = NULL;
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                Locals arm_loc = locals_clone(loc);
                tc_pat(e->as.match_expr.arms[i]->pat, scrut_ty, ctx, &arm_loc, env, err);
                Ty *t = tc_expr_inner(e->as.match_expr.arms[i]->expr, ctx, &arm_loc, env, err);
                locals_free(&arm_loc);
                arm_ty = arm_ty ? unify(env->arena, arm_ty, t, ctx->cask_path, "match", NULL, err) : t;
            }
            if (!arm_ty) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: match requires at least one arm", (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            return arm_ty;
        }
        case EXPR_LAMBDA: {
            Locals lambda_loc = locals_clone(loc);
            size_t n = e->as.lambda.params_len;
            Ty **param_tys = (Ty **)arena_array(env->arena, n, sizeof(Ty *));
            int gen_id = 0;
            for (size_t i = 0; i < n; i++) {
                Param *p = e->as.lambda.params[i];
                if (p->is_this) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: lambda params cannot be this", (int)ctx->cask_path.len, ctx->cask_path.data);
                    locals_free(&lambda_loc);
                    return NULL;
                }
                Ty *ty = NULL;
                if (!p->typ) {
                    gen_id++;
                    char buf[64];
                    snprintf(buf, sizeof(buf), "_%.*s_%d", (int)p->name.len, p->name.data, gen_id);
                    Str gen = arena_str_copy(env->arena, buf, strlen(buf));
                    ty = ty_gen(env->arena, gen);
                } else {
                    ty = ty_from_type_ref(env, p->typ, ctx->cask_name, ctx->imports, ctx->imports_len, err);
                }
                Binding b = { ty, p->is_mut, false, false };
                locals_define(&lambda_loc, p->name, b);
                param_tys[i] = ty;
            }
            Ty *body_ty = tc_expr_inner(e->as.lambda.body, ctx, &lambda_loc, env, err);
            locals_free(&lambda_loc);
            return ty_fn(env->arena, param_tys, n, body_ty);
        }
        case EXPR_BLOCK: {
            Ty *ret_ty = ty_prim(env->arena, "any");
            tc_stmt_inner(e->as.block_expr.block, ctx, loc, env, ret_ty, err);
            return ret_ty;
        }
        case EXPR_IF: {
            Ty *arm_ty = NULL;
            bool saw_else = false;
            for (size_t i = 0; i < e->as.if_expr.arms_len; i++) {
                ExprIfArm *arm = e->as.if_expr.arms[i];
                if (arm->cond) {
                    Ty *ct = tc_expr_inner(arm->cond, ctx, loc, env, err);
                    if (ty_is_void(ct)) {
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: if condition cannot be void", (int)ctx->cask_path.len, ctx->cask_path.data);
                        return NULL;
                    }
                } else {
                    saw_else = true;
                }
                Ty *vt = tc_expr_inner(arm->value, ctx, loc, env, err);
                arm_ty = arm_ty ? unify(env->arena, arm_ty, vt, ctx->cask_path, "if expression", NULL, err) : vt;
            }
            if (!saw_else) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: if expression requires else branch", (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            return arm_ty ? arm_ty : ty_null(env->arena);
        }
        case EXPR_NEW: {
            Str name = e->as.new_expr.name;
            Str qname = name;
            if (!memchr(name.data, '.', name.len)) {
                qname = qualify_class_name(env->arena, ctx->cask_name, name);
            } else {
                size_t dot = 0;
                for (; dot < name.len; dot++) if (name.data[dot] == '.') break;
                Str mod = str_slice(name, 0, dot);
                bool ok = str_eq(mod, ctx->cask_name);
                for (size_t i = 0; i < ctx->imports_len && !ok; i++) if (str_eq(ctx->imports[i], mod)) ok = true;
                if (!ok) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: unknown class '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)name.len, name.data);
                    return NULL;
                }
            }
            ClassInfo *ci = find_class(env, qname);
            if (!ci) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: unknown class '%.*s'", (int)ctx->cask_path.len, ctx->cask_path.data, (int)name.len, name.data);
                return NULL;
            }
            if (is_cross_cask(ctx->cask_name, ci->cask) && !str_eq_c(ci->vis, "pub")) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: class '%.*s' is not public",
                         (int)ctx->cask_path.len, ctx->cask_path.data,
                         (int)ci->name.len, ci->name.data);
                return NULL;
            }
            MethodEntry *init = NULL;
            for (size_t i = 0; i < ci->methods_len; i++) {
                if (str_eq_c(ci->methods[i].name, "init")) {
                    init = &ci->methods[i];
                    break;
                }
            }

            bool has_named = false;
            bool has_positional = false;
            for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                bool named = e->as.new_expr.arg_names && e->as.new_expr.arg_names[i].len > 0;
                if (named) has_named = true;
                else has_positional = true;
            }
            if (has_named && has_positional) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: constructor cannot mix named and positional args", (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }

            if (has_named) {
                bool *seen = ci->fields_len ? (bool *)calloc(ci->fields_len, sizeof(bool)) : NULL;
                if (ci->fields_len && !seen) {
                    set_err(err, "out of memory");
                    return NULL;
                }
                for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                    Str aname = e->as.new_expr.arg_names[i];
                    size_t fidx = ci->fields_len;
                    for (size_t f = 0; f < ci->fields_len; f++) {
                        if (str_eq(ci->fields[f].name, aname)) {
                            fidx = f;
                            break;
                        }
                    }
                    if (fidx == ci->fields_len) {
                        free(seen);
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: unknown field '%.*s' in constructor", (int)ctx->cask_path.len, ctx->cask_path.data, (int)aname.len, aname.data);
                        return NULL;
                    }
                    if (seen[fidx]) {
                        free(seen);
                        set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: duplicate field '%.*s' in constructor", (int)ctx->cask_path.len, ctx->cask_path.data, (int)aname.len, aname.data);
                        return NULL;
                    }
                    seen[fidx] = true;
                    Ty *at = tc_expr_inner(e->as.new_expr.args[i], ctx, loc, env, err);
                    ensure_assignable(env->arena, ci->fields[fidx].ty, at, ctx->cask_path, "field init", err);
                }
                free(seen);
                return ty_class(env->arena, qname);
            }

            if (init) {
                FunSig *sig = init->sig;
                if (e->as.new_expr.args_len != sig->params_len) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: '%.*s.init' expects %zu args", (int)ctx->cask_path.len, ctx->cask_path.data, (int)ci->name.len, ci->name.data, sig->params_len);
                    return NULL;
                }
                Subst subst = subst_init();
                for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                    Ty *at = tc_expr_inner(e->as.new_expr.args[i], ctx, loc, env, err);
                    ensure_assignable(env->arena, sig->params[i], at, ctx->cask_path, "arg", err);
                    unify(env->arena, sig->params[i], at, ctx->cask_path, "arg", &subst, err);
                }
                subst_free(&subst);
                if (!ty_is_void(sig->ret)) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: '%.*s.init' must return void", (int)ctx->cask_path.len, ctx->cask_path.data, (int)ci->name.len, ci->name.data);
                    return NULL;
                }
            } else if ((ci->kind == CLASS_KIND_STRUCT || ci->kind == CLASS_KIND_ENUM) && e->as.new_expr.args_len > 0) {
                if (e->as.new_expr.args_len != ci->fields_len) {
                    set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: '%.*s' expects %zu args", (int)ctx->cask_path.len, ctx->cask_path.data, (int)ci->name.len, ci->name.data, ci->fields_len);
                    return NULL;
                }
                for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                    Ty *at = tc_expr_inner(e->as.new_expr.args[i], ctx, loc, env, err);
                    ensure_assignable(env->arena, ci->fields[i].ty, at, ctx->cask_path, "field init", err);
                }
            } else if (e->as.new_expr.args_len > 0) {
                set_errf(err, ctx->cask_path, e->line, e->col, "%.*s: class '%.*s' has no init method", (int)ctx->cask_path.len, ctx->cask_path.data, (int)ci->name.len, ci->name.data);
                return NULL;
            }
            return ty_class(env->arena, qname);
        }
        case EXPR_MOVE: {
            if (!e->as.move.x || e->as.move.x->kind != EXPR_IDENT) {
                set_errf(err, ctx->cask_path, e->line, e->col,
                         "%.*s: move(...) requires a mutable local binding identifier",
                         (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            Binding *b = locals_lookup(loc, e->as.move.x->as.ident.name);
            if (!b) {
                set_errf(err, ctx->cask_path, e->line, e->col,
                         "%.*s: move(...) is only supported on mutable local bindings",
                         (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            if (b->is_const || !b->is_mut) {
                set_errf(err, ctx->cask_path, e->line, e->col,
                         "%.*s: move(...) requires a mutable local binding",
                         (int)ctx->cask_path.len, ctx->cask_path.data);
                return NULL;
            }
            if (b->is_moved) {
                set_errf(err, ctx->cask_path, e->line, e->col,
                         "%.*s: value '%.*s' has already been moved",
                         (int)ctx->cask_path.len, ctx->cask_path.data,
                         (int)e->as.move.x->as.ident.name.len, e->as.move.x->as.ident.name.data);
                return NULL;
            }
            b->is_moved = true;
            return b->ty;
        }
        case EXPR_CALL:
            return tc_call(e, ctx, loc, env, err);
        case EXPR_PAREN:
            return tc_expr_inner(e->as.paren.x, ctx, loc, env, err);
        default:
            return NULL;
    }
}

static void tc_pat(Pat *pat, Ty *scrut_ty, Ctx *ctx, Locals *loc, GlobalEnv *env, Diag *err) {
    if (!pat) return;
    if (pat->kind == PAT_WILD) return;
    if (pat->kind == PAT_IDENT) {
        Binding b = { scrut_ty, false, false, false };
        locals_define(loc, pat->as.name, b);
        return;
    }
    if (pat->kind == PAT_INT) {
        unify(env->arena, scrut_ty, ty_prim(env->arena, "num"), ctx->cask_path, "match pattern", NULL, err);
        return;
    }
    if (pat->kind == PAT_STR) {
        unify(env->arena, scrut_ty, ty_prim(env->arena, "string"), ctx->cask_path, "match pattern", NULL, err);
        return;
    }
    if (pat->kind == PAT_BOOL) {
        unify(env->arena, scrut_ty, ty_prim(env->arena, "bool"), ctx->cask_path, "match pattern", NULL, err);
        return;
    }
    if (pat->kind == PAT_NULL) {
        unify(env->arena, scrut_ty, ty_null(env->arena), ctx->cask_path, "match pattern", NULL, err);
        return;
    }
    set_errf(err, ctx->cask_path, pat->line, pat->col, "%.*s: unsupported match pattern", (int)ctx->cask_path.len, ctx->cask_path.data);
}

static void tc_stmt_inner(Stmt *s, Ctx *ctx, Locals *loc, GlobalEnv *env, Ty *ret_ty, Diag *err) {
    if (!s) return;
    switch (s->kind) {
        case STMT_LET: {
            Ty *t = tc_expr_inner(s->as.let_s.expr, ctx, loc, env, err);
            Binding b = { t, s->as.let_s.is_mut, false, false };
            locals_define(loc, s->as.let_s.name, b);
            return;
        }
        case STMT_CONST: {
            Ty *t = tc_expr_inner(s->as.const_s.expr, ctx, loc, env, err);
            Binding b = { t, false, true, false };
            locals_define(loc, s->as.const_s.name, b);
            return;
        }
        case STMT_EXPR:
            tc_expr_inner(s->as.expr_s.expr, ctx, loc, env, err);
            return;
        case STMT_RETURN: {
            if (ty_is_void(ret_ty)) {
                if (s->as.ret_s.expr) {
                    set_errf(err, ctx->cask_path, s->line, s->col, "%.*s: return value in void function", (int)ctx->cask_path.len, ctx->cask_path.data);
                    return;
                }
                return;
            }
            if (!s->as.ret_s.expr) {
                set_errf(err, ctx->cask_path, s->line, s->col, "%.*s: missing return value", (int)ctx->cask_path.len, ctx->cask_path.data);
                return;
            }
            Ty *t = tc_expr_inner(s->as.ret_s.expr, ctx, loc, env, err);
            if (!ensure_assignable(env->arena, ret_ty, t, ctx->cask_path, "return", err)) {
                if (err && err->line == 0) {
                    err->line = s->line;
                    err->col = s->col;
                }
                return;
            }
            unify(env->arena, ret_ty, t, ctx->cask_path, "return", NULL, err);
            if (err && err->message && err->line == 0) {
                err->line = s->line;
                err->col = s->col;
            }
            return;
        }
        case STMT_IF: {
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                Locals arm_loc = locals_clone(loc);
                IfArm *arm = s->as.if_s.arms[i];
                if (arm->cond) {
                    Ty *ct = tc_expr_inner(arm->cond, ctx, &arm_loc, env, err);
                    if (ty_is_void(ct)) {
                        set_errf(err, ctx->cask_path, s->line, s->col, "%.*s: if condition cannot be void", (int)ctx->cask_path.len, ctx->cask_path.data);
                    }
                }
                tc_stmt_inner(arm->body, ctx, &arm_loc, env, ret_ty, err);
                locals_free(&arm_loc);
            }
            return;
        }
        case STMT_BREAK:
            if (ctx->loop_depth <= 0) {
                set_errf(err, ctx->cask_path, s->line, s->col, "%.*s: break used outside loop", (int)ctx->cask_path.len, ctx->cask_path.data);
            }
            return;
        case STMT_CONTINUE:
            if (ctx->loop_depth <= 0) {
                set_errf(err, ctx->cask_path, s->line, s->col, "%.*s: continue used outside loop", (int)ctx->cask_path.len, ctx->cask_path.data);
            }
            return;
        case STMT_FOR: {
            locals_push(loc);
            if (s->as.for_s.init) {
                tc_stmt_inner(s->as.for_s.init, ctx, loc, env, ret_ty, err);
            }
            if (s->as.for_s.cond) {
                Ty *ct = tc_expr_inner(s->as.for_s.cond, ctx, loc, env, err);
                if (ty_is_void(ct)) {
                    set_errf(err, ctx->cask_path, s->line, s->col, "%.*s: for condition cannot be void", (int)ctx->cask_path.len, ctx->cask_path.data);
                }
            }
            if (s->as.for_s.step) {
                tc_expr_inner(s->as.for_s.step, ctx, loc, env, err);
            }
            ctx->loop_depth++;
            tc_stmt_inner(s->as.for_s.body, ctx, loc, env, ret_ty, err);
            ctx->loop_depth--;
            locals_pop(loc);
            return;
        }
        case STMT_FOREACH: {
            Ty *it = tc_expr_inner(s->as.foreach_s.expr, ctx, loc, env, err);
            it = ty_strip_nullable(it);
            Ty *elem = NULL;
            if (it && it->tag == TY_ARRAY && it->elem) {
                elem = it->elem;
            } else if (it && it->tag == TY_PRIM && str_eq_c(it->name, "string")) {
                elem = ty_prim(env->arena, "string");
            } else {
                set_errf(err, ctx->cask_path, s->line, s->col, "%.*s: foreach expects array or string", (int)ctx->cask_path.len, ctx->cask_path.data);
                return;
            }
            locals_push(loc);
            Binding b = { elem, false, false, false };
            locals_define(loc, s->as.foreach_s.name, b);
            ctx->loop_depth++;
            tc_stmt_inner(s->as.foreach_s.body, ctx, loc, env, ret_ty, err);
            ctx->loop_depth--;
            locals_pop(loc);
            return;
        }
        case STMT_BLOCK: {
            locals_push(loc);
            for (size_t i = 0; i < s->as.block_s.stmts_len; i++) {
                tc_stmt_inner(s->as.block_s.stmts[i], ctx, loc, env, ret_ty, err);
                if (err && err->message) break;
            }
            locals_pop(loc);
            return;
        }
        default:
            return;
    }
}

Ty *tc_expr(Expr *e, GlobalEnv *env, Str cask_path, Str cask_name, Str *imports, size_t imports_len, Diag *err) {
    Ctx ctx;
    ctx.cask_path = cask_path;
    ctx.cask_name = cask_name;
    ctx.imports = imports;
    ctx.imports_len = imports_len;
    ctx.has_current_class = false;
    ctx.loop_depth = 0;
    Locals loc;
    locals_init(&loc);
    Ty *t = tc_expr_inner(e, &ctx, &loc, env, err);
    locals_free(&loc);
    return t;
}

Ty *tc_expr_ctx(Expr *e, Ctx *ctx, Locals *loc, GlobalEnv *env, Diag *err) {
    if (!ctx || !loc) {
        return NULL;
    }
    return tc_expr_inner(e, ctx, loc, env, err);
}

typedef struct {
    YisLintMode mode;
    int warnings;
    int errors;
} LintState;

static bool ty_requires_non_null(Ty *t) {
    if (!t) return false;
    if (ty_is_void(t) || ty_is_null(t) || ty_is_nullable(t)) return false;
    if (t->tag == TY_PRIM && str_eq_c(t->name, "any")) return false;
    return true;
}

static void lint_emit(LintState *ls, Str path, int line, int col, const char *msg, const char *hint) {
    if (!ls || !msg) return;
    const char *level = (ls->mode == YIS_LINT_STRICT) ? "error" : "warning";
    if (ls->mode == YIS_LINT_STRICT) ls->errors++;
    else ls->warnings++;

    if (line <= 0) line = 1;
    if (col <= 0) col = 1;
    int end_col = col + 1;
    fprintf(stderr, "%s: %.*s:%d:%d-%d:%d: %s\n",
            level,
            (int)path.len, path.data,
            line, col, line, end_col,
            msg);
    if (hint && hint[0]) {
        fprintf(stderr, "  hint: %s\n", hint);
    }
}

static bool match_has_null_arm(Expr *e) {
    if (!e || e->kind != EXPR_MATCH) return false;
    for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
        MatchArm *arm = e->as.match_expr.arms[i];
        if (arm && arm->pat && arm->pat->kind == PAT_NULL) return true;
    }
    return false;
}

static bool expr_value_has_unchecked_index(Expr *e);

static bool stmt_value_has_unchecked_index(Stmt *s) {
    if (!s) return false;
    switch (s->kind) {
        case STMT_RETURN:
            return expr_value_has_unchecked_index(s->as.ret_s.expr);
        case STMT_EXPR:
            return expr_value_has_unchecked_index(s->as.expr_s.expr);
        case STMT_BLOCK:
            for (size_t i = 0; i < s->as.block_s.stmts_len; i++) {
                if (stmt_value_has_unchecked_index(s->as.block_s.stmts[i])) return true;
            }
            return false;
        case STMT_IF:
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                IfArm *arm = s->as.if_s.arms[i];
                if (arm && stmt_value_has_unchecked_index(arm->body)) return true;
            }
            return false;
        default:
            return false;
    }
}

static bool expr_value_has_unchecked_index(Expr *e) {
    if (!e) return false;
    switch (e->kind) {
        case EXPR_INDEX:
            return true;
        case EXPR_UNARY:
            return expr_value_has_unchecked_index(e->as.unary.x);
        case EXPR_PAREN:
            return expr_value_has_unchecked_index(e->as.paren.x);
        case EXPR_MOVE:
            return expr_value_has_unchecked_index(e->as.move.x);
        case EXPR_BINARY:
            if (e->as.binary.op == TOK_QQ) {
                return expr_value_has_unchecked_index(e->as.binary.b);
            }
            return expr_value_has_unchecked_index(e->as.binary.a) ||
                   expr_value_has_unchecked_index(e->as.binary.b);
        case EXPR_TERNARY:
            return expr_value_has_unchecked_index(e->as.ternary.then_expr) ||
                   expr_value_has_unchecked_index(e->as.ternary.else_expr);
        case EXPR_IF:
            for (size_t i = 0; i < e->as.if_expr.arms_len; i++) {
                ExprIfArm *arm = e->as.if_expr.arms[i];
                if (arm && expr_value_has_unchecked_index(arm->value)) return true;
            }
            return false;
        case EXPR_MATCH: {
            bool scrut_guarded = match_has_null_arm(e);
            if (!scrut_guarded && expr_value_has_unchecked_index(e->as.match_expr.scrut)) return true;
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                MatchArm *arm = e->as.match_expr.arms[i];
                if (arm && expr_value_has_unchecked_index(arm->expr)) return true;
            }
            return false;
        }
        case EXPR_BLOCK:
            return stmt_value_has_unchecked_index(e->as.block_expr.block);
        default:
            return false;
    }
}

static void lint_check_index_flow(Expr *value_expr, Ctx *ctx, LintState *ls, const char *context_desc) {
    if (!value_expr || !ctx || !ls) return;
    if (!expr_value_has_unchecked_index(value_expr)) return;
    char msg[320];
    snprintf(msg, sizeof(msg),
             "indexing expression may yield null when used as %s",
             context_desc ? context_desc : "a non-null value");
    lint_emit(ls, ctx->cask_path, value_expr->line, value_expr->col, msg,
              "use ??, an explicit null check, or match to handle null.");
}

static void lint_check_truthiness(Expr *cond, Ctx *ctx, Locals *loc, GlobalEnv *env, LintState *ls, const char *where) {
    if (!cond || !ctx || !loc || !env || !ls) return;
    Diag tmp = {0};
    Ty *ct = tc_expr_ctx(cond, ctx, loc, env, &tmp);
    if (!ct || ty_is_void(ct)) return;
    if (ct->tag == TY_PRIM && str_eq_c(ct->name, "bool")) return;
    char tdesc[64];
    ty_desc(ct, tdesc, sizeof(tdesc));
    char msg[320];
    snprintf(msg, sizeof(msg), "implicit truthiness in %s condition (type %s)", where, tdesc);
    lint_emit(ls, ctx->cask_path, cond->line, cond->col, msg,
              "use an explicit comparison or null check.");
}

static bool stmt_guarantees_return(Stmt *s) {
    if (!s) return false;
    switch (s->kind) {
        case STMT_RETURN:
            return true;
        case STMT_BLOCK:
            for (size_t i = 0; i < s->as.block_s.stmts_len; i++) {
                if (stmt_guarantees_return(s->as.block_s.stmts[i])) {
                    return true;
                }
            }
            return false;
        case STMT_IF: {
            bool has_else = false;
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                IfArm *arm = s->as.if_s.arms[i];
                if (arm && !arm->cond) has_else = true;
            }
            if (!has_else) return false;
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                IfArm *arm = s->as.if_s.arms[i];
                if (!arm || !stmt_guarantees_return(arm->body)) return false;
            }
            return true;
        }
        default:
            return false;
    }
}

static void describe_fallthrough(Stmt *s, char *out, size_t out_cap) {
    if (!out || out_cap == 0) return;
    if (!s) {
        snprintf(out, out_cap, "function body can reach end without return");
        return;
    }
    if (s->kind == STMT_BLOCK) {
        if (s->as.block_s.stmts_len == 0) {
            snprintf(out, out_cap, "empty body can reach end without return");
            return;
        }
        describe_fallthrough(s->as.block_s.stmts[s->as.block_s.stmts_len - 1], out, out_cap);
        return;
    }
    if (s->kind == STMT_IF) {
        bool has_else = false;
        for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
            IfArm *arm = s->as.if_s.arms[i];
            if (arm && !arm->cond) has_else = true;
        }
        if (!has_else) {
            snprintf(out, out_cap, "if branch at line %d has no else and can fall through", s->line);
            return;
        }
        for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
            IfArm *arm = s->as.if_s.arms[i];
            if (arm && !stmt_guarantees_return(arm->body)) {
                if (arm->cond) {
                    snprintf(out, out_cap, "if branch at line %d can fall through", arm->body ? arm->body->line : s->line);
                } else {
                    snprintf(out, out_cap, "else branch at line %d can fall through", arm->body ? arm->body->line : s->line);
                }
                return;
            }
        }
    }
    snprintf(out, out_cap, "control path can reach end without return");
}

static Ty *lint_ret_ty_from_spec(GlobalEnv *env, RetSpec *ret, Str mod_name, Str *imports, size_t imports_len, Diag *err) {
    if (!ret) return ty_void(env->arena);
    if (ret->is_void) return ty_void(env->arena);
    if (ret->types_len == 1) {
        return ty_from_type_ref(env, ret->types[0], mod_name, imports, imports_len, err);
    }
    Ty **items = (Ty **)arena_array(env->arena, ret->types_len, sizeof(Ty *));
    if (!items) return NULL;
    for (size_t i = 0; i < ret->types_len; i++) {
        items[i] = ty_from_type_ref(env, ret->types[i], mod_name, imports, imports_len, err);
        if (!items[i]) return NULL;
    }
    return ty_tuple(env->arena, items, ret->types_len);
}

static bool is_empty_body_stub(Stmt *body) {
    return body && body->kind == STMT_BLOCK && body->as.block_s.stmts_len == 0;
}

static bool check_nonvoid_return_coverage(Stmt *body, Ty *ret_ty, Str path, Str name, int line, int col, Diag *err) {
    if (!ret_ty || ty_is_void(ret_ty) || is_empty_body_stub(body)) {
        return true;
    }
    if (stmt_guarantees_return(body)) {
        return true;
    }
    char why[256];
    describe_fallthrough(body, why, sizeof(why));
    set_errf(err, path, line, col,
             "%.*s: missing return coverage in function '%.*s': %s",
             (int)path.len, path.data,
             (int)name.len, name.data,
             why);
    return false;
}

static void lint_expr(Expr *e, Ctx *ctx, Locals *loc, GlobalEnv *env, LintState *ls);
static void lint_stmt(Stmt *s, Ctx *ctx, Locals *loc, GlobalEnv *env, Ty *ret_ty, LintState *ls);

static void lint_call_args(Expr *call, Ctx *ctx, Locals *loc, GlobalEnv *env, LintState *ls) {
    if (!call || call->kind != EXPR_CALL) return;
    Expr *fn = call->as.call.fn;
    Ty **params = NULL;
    size_t params_len = 0;

    if (fn && fn->kind == EXPR_IDENT) {
        Binding *b = locals_lookup(loc, fn->as.ident.name);
        if (b && b->ty && b->ty->tag == TY_FN) {
            params = b->ty->params;
            params_len = b->ty->params_len;
        } else {
            FunSig *sig = find_fun(env, ctx->cask_name, fn->as.ident.name);
            if (!sig && is_stdr_prelude(fn->as.ident.name)) {
                bool allow = str_eq_c(ctx->cask_name, "stdr");
                for (size_t i = 0; i < ctx->imports_len && !allow; i++) {
                    if (str_eq_c(ctx->imports[i], "stdr")) allow = true;
                }
                if (allow) sig = find_fun(env, str_from_c("stdr"), fn->as.ident.name);
            }
            if (sig) {
                params = sig->params;
                params_len = sig->params_len;
            }
        }
    } else if (fn && fn->kind == EXPR_MEMBER) {
        Expr *base = fn->as.member.a;
        if (base && base->kind == EXPR_IDENT) {
            Str mod = base->as.ident.name;
            if (cask_in_scope(mod, ctx, loc)) {
                FunSig *sig = find_fun(env, mod, fn->as.member.name);
                if (sig) {
                    params = sig->params;
                    params_len = sig->params_len;
                }
            }
        }
        if (!params) {
            Diag tmp = {0};
            Ty *base_ty = tc_expr_ctx(base, ctx, loc, env, &tmp);
            base_ty = ty_strip_nullable(base_ty);
            if (base_ty && base_ty->tag == TY_CLASS) {
                ClassInfo *ci = find_class(env, base_ty->name);
                if (ci) {
                    for (size_t i = 0; i < ci->methods_len; i++) {
                        if (str_eq(ci->methods[i].name, fn->as.member.name)) {
                            params = ci->methods[i].sig->params;
                            params_len = ci->methods[i].sig->params_len;
                            break;
                        }
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < call->as.call.args_len; i++) {
        Expr *arg = call->as.call.args[i];
        if (params && i < params_len && ty_requires_non_null(params[i])) {
            lint_check_index_flow(arg, ctx, ls, "a non-null call argument");
        }
        lint_expr(arg, ctx, loc, env, ls);
    }
}

static void lint_expr(Expr *e, Ctx *ctx, Locals *loc, GlobalEnv *env, LintState *ls) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_UNARY:
            lint_expr(e->as.unary.x, ctx, loc, env, ls);
            return;
        case EXPR_BINARY:
            lint_expr(e->as.binary.a, ctx, loc, env, ls);
            lint_expr(e->as.binary.b, ctx, loc, env, ls);
            return;
        case EXPR_ASSIGN: {
            Ty *target_ty = NULL;
            if (e->as.assign.target && e->as.assign.target->kind == EXPR_IDENT) {
                Binding *b = locals_lookup(loc, e->as.assign.target->as.ident.name);
                if (b) target_ty = b->ty;
            }
            if (target_ty && ty_requires_non_null(target_ty)) {
                lint_check_index_flow(e->as.assign.value, ctx, ls, "a non-null assignment");
            }
            lint_expr(e->as.assign.target, ctx, loc, env, ls);
            lint_expr(e->as.assign.value, ctx, loc, env, ls);
            return;
        }
        case EXPR_CALL: {
            if (e->as.call.fn && e->as.call.fn->kind == EXPR_MEMBER) {
                lint_check_index_flow(e->as.call.fn->as.member.a, ctx, ls, "a call receiver");
                lint_expr(e->as.call.fn->as.member.a, ctx, loc, env, ls);
            } else {
                lint_expr(e->as.call.fn, ctx, loc, env, ls);
            }
            lint_call_args(e, ctx, loc, env, ls);
            return;
        }
        case EXPR_INDEX:
            lint_expr(e->as.index.a, ctx, loc, env, ls);
            lint_expr(e->as.index.i, ctx, loc, env, ls);
            return;
        case EXPR_MEMBER:
            lint_check_index_flow(e->as.member.a, ctx, ls, "a member access receiver");
            lint_expr(e->as.member.a, ctx, loc, env, ls);
            return;
        case EXPR_PAREN:
            lint_expr(e->as.paren.x, ctx, loc, env, ls);
            return;
        case EXPR_MATCH:
            lint_expr(e->as.match_expr.scrut, ctx, loc, env, ls);
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                MatchArm *arm = e->as.match_expr.arms[i];
                if (arm) lint_expr(arm->expr, ctx, loc, env, ls);
            }
            return;
        case EXPR_LAMBDA:
            lint_expr(e->as.lambda.body, ctx, loc, env, ls);
            return;
        case EXPR_BLOCK:
            lint_stmt(e->as.block_expr.block, ctx, loc, env, ty_null(env->arena), ls);
            return;
        case EXPR_NEW:
            for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                lint_expr(e->as.new_expr.args[i], ctx, loc, env, ls);
            }
            return;
        case EXPR_IF:
            for (size_t i = 0; i < e->as.if_expr.arms_len; i++) {
                ExprIfArm *arm = e->as.if_expr.arms[i];
                if (!arm) continue;
                if (arm->cond) lint_check_truthiness(arm->cond, ctx, loc, env, ls, "if");
                if (arm->cond) lint_expr(arm->cond, ctx, loc, env, ls);
                lint_expr(arm->value, ctx, loc, env, ls);
            }
            return;
        case EXPR_TERNARY:
            lint_check_truthiness(e->as.ternary.cond, ctx, loc, env, ls, "ternary");
            lint_expr(e->as.ternary.cond, ctx, loc, env, ls);
            lint_expr(e->as.ternary.then_expr, ctx, loc, env, ls);
            lint_expr(e->as.ternary.else_expr, ctx, loc, env, ls);
            return;
        case EXPR_MOVE:
            lint_expr(e->as.move.x, ctx, loc, env, ls);
            return;
        case EXPR_TUPLE:
            for (size_t i = 0; i < e->as.tuple_lit.items_len; i++) {
                lint_expr(e->as.tuple_lit.items[i], ctx, loc, env, ls);
            }
            return;
        case EXPR_ARRAY:
            for (size_t i = 0; i < e->as.array_lit.items_len; i++) {
                lint_expr(e->as.array_lit.items[i], ctx, loc, env, ls);
            }
            return;
        case EXPR_DICT:
            for (size_t i = 0; i < e->as.dict_lit.pairs_len; i++) {
                lint_expr(e->as.dict_lit.keys[i], ctx, loc, env, ls);
                lint_expr(e->as.dict_lit.vals[i], ctx, loc, env, ls);
            }
            return;
        default:
            return;
    }
}

static void lint_stmt(Stmt *s, Ctx *ctx, Locals *loc, GlobalEnv *env, Ty *ret_ty, LintState *ls) {
    if (!s) return;
    switch (s->kind) {
        case STMT_LET: {
            Diag tmp = {0};
            Ty *t = tc_expr_ctx(s->as.let_s.expr, ctx, loc, env, &tmp);
            Binding b = { t, s->as.let_s.is_mut, false, false };
            locals_define(loc, s->as.let_s.name, b);
            lint_expr(s->as.let_s.expr, ctx, loc, env, ls);
            return;
        }
        case STMT_CONST: {
            Diag tmp = {0};
            Ty *t = tc_expr_ctx(s->as.const_s.expr, ctx, loc, env, &tmp);
            Binding b = { t, false, true, false };
            locals_define(loc, s->as.const_s.name, b);
            lint_expr(s->as.const_s.expr, ctx, loc, env, ls);
            return;
        }
        case STMT_EXPR:
            lint_expr(s->as.expr_s.expr, ctx, loc, env, ls);
            return;
        case STMT_RETURN:
            if (s->as.ret_s.expr && ty_requires_non_null(ret_ty)) {
                lint_check_index_flow(s->as.ret_s.expr, ctx, ls, "a non-null return value");
            }
            lint_expr(s->as.ret_s.expr, ctx, loc, env, ls);
            return;
        case STMT_IF:
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                IfArm *arm = s->as.if_s.arms[i];
                if (!arm) continue;
                Locals arm_loc = locals_clone(loc);
                if (arm->cond) {
                    lint_check_truthiness(arm->cond, ctx, &arm_loc, env, ls, "if");
                    lint_expr(arm->cond, ctx, &arm_loc, env, ls);
                }
                lint_stmt(arm->body, ctx, &arm_loc, env, ret_ty, ls);
                locals_free(&arm_loc);
            }
            return;
        case STMT_FOR:
            locals_push(loc);
            if (s->as.for_s.init) lint_stmt(s->as.for_s.init, ctx, loc, env, ret_ty, ls);
            if (s->as.for_s.cond) {
                lint_check_truthiness(s->as.for_s.cond, ctx, loc, env, ls, "for");
                lint_expr(s->as.for_s.cond, ctx, loc, env, ls);
            }
            if (s->as.for_s.step) lint_expr(s->as.for_s.step, ctx, loc, env, ls);
            lint_stmt(s->as.for_s.body, ctx, loc, env, ret_ty, ls);
            locals_pop(loc);
            return;
        case STMT_FOREACH:
            lint_expr(s->as.foreach_s.expr, ctx, loc, env, ls);
            locals_push(loc);
            locals_define(loc, s->as.foreach_s.name, (Binding){ ty_prim(env->arena, "any"), false, false, false });
            lint_stmt(s->as.foreach_s.body, ctx, loc, env, ret_ty, ls);
            locals_pop(loc);
            return;
        case STMT_BLOCK:
            locals_push(loc);
            for (size_t i = 0; i < s->as.block_s.stmts_len; i++) {
                lint_stmt(s->as.block_s.stmts[i], ctx, loc, env, ret_ty, ls);
            }
            locals_pop(loc);
            return;
        default:
            return;
    }
}

bool lint_program(Program *prog, Arena *arena, YisLintMode mode, int *warning_count, int *error_count) {
    Diag err = {0};
    GlobalEnv *env = build_global_env(prog, arena, &err);
    if (!env) return false;

    LintState ls;
    ls.mode = mode;
    ls.warnings = 0;
    ls.errors = 0;

    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        Str mod_name = env->cask_names[i].name;
        ModuleImport *imps = find_imports(env, mod_name);
        Str *imports = imps ? imps->imports : NULL;
        size_t imports_len = imps ? imps->imports_len : 0;

        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_FUN) {
                Locals loc;
                locals_init(&loc);
                Ctx ctx;
                ctx.cask_path = m->path;
                ctx.cask_name = mod_name;
                ctx.imports = imports;
                ctx.imports_len = imports_len;
                ctx.has_current_class = false;
                ctx.loop_depth = 0;

                for (size_t p = 0; p < d->as.fun.params_len; p++) {
                    Param *pp = d->as.fun.params[p];
                    Ty *pty = ty_from_type_ref(env, pp->typ, mod_name, imports, imports_len, &err);
                    locals_define(&loc, pp->name, (Binding){ pty, pp->is_mut, false, false });
                }
                Ty *ret_ty = lint_ret_ty_from_spec(env, &d->as.fun.ret, mod_name, imports, imports_len, &err);
                if (ret_ty && !ty_is_void(ret_ty) && !is_empty_body_stub(d->as.fun.body) && !stmt_guarantees_return(d->as.fun.body)) {
                    char why[256];
                    char msg[384];
                    describe_fallthrough(d->as.fun.body, why, sizeof(why));
                    snprintf(msg, sizeof(msg),
                             "missing return coverage in function '%.*s': %s",
                             (int)d->as.fun.name.len, d->as.fun.name.data, why);
                    lint_emit(&ls, m->path, d->line, d->col, msg,
                              "add explicit return statements for every path.");
                }
                lint_stmt(d->as.fun.body, &ctx, &loc, env, ret_ty, &ls);
                locals_free(&loc);
            } else if (d->kind == DECL_CLASS) {
                ClassInfo *ci = find_class(env, qualify_class_name(env->arena, mod_name, d->as.class_decl.name));
                if (!ci) continue;
                for (size_t mi = 0; mi < d->as.class_decl.methods_len; mi++) {
                    FunDecl *md = d->as.class_decl.methods[mi];
                    Locals loc;
                    locals_init(&loc);
                    Ctx ctx;
                    ctx.cask_path = m->path;
                    ctx.cask_name = mod_name;
                    ctx.imports = imports;
                    ctx.imports_len = imports_len;
                    ctx.has_current_class = true;
                    ctx.current_class = ci->qname;
                    ctx.loop_depth = 0;
                    for (size_t p = 0; p < md->params_len; p++) {
                        Param *pp = md->params[p];
                        Ty *pty = pp->is_this ? ty_class(env->arena, ci->qname)
                                              : ty_from_type_ref(env, pp->typ, mod_name, imports, imports_len, &err);
                        locals_define(&loc, pp->name, (Binding){ pty, pp->is_mut, false, false });
                    }
                    Ty *ret_ty = lint_ret_ty_from_spec(env, &md->ret, mod_name, imports, imports_len, &err);
                    if (ret_ty && !ty_is_void(ret_ty) && !is_empty_body_stub(md->body) && !stmt_guarantees_return(md->body)) {
                        char why[256];
                        char msg[384];
                        describe_fallthrough(md->body, why, sizeof(why));
                        snprintf(msg, sizeof(msg),
                                 "missing return coverage in function '%.*s.%.*s': %s",
                                 (int)ci->name.len, ci->name.data,
                                 (int)md->name.len, md->name.data, why);
                        lint_emit(&ls, m->path, md->body ? md->body->line : d->line, md->body ? md->body->col : d->col, msg,
                                  "add explicit return statements for every path.");
                    }
                    lint_stmt(md->body, &ctx, &loc, env, ret_ty, &ls);
                    locals_free(&loc);
                }
            } else if (d->kind == DECL_ENTRY) {
                Locals loc;
                locals_init(&loc);
                Ctx ctx;
                ctx.cask_path = m->path;
                ctx.cask_name = mod_name;
                ctx.imports = imports;
                ctx.imports_len = imports_len;
                ctx.has_current_class = false;
                ctx.loop_depth = 0;
                Ty *ret_ty = lint_ret_ty_from_spec(env, &d->as.entry.ret, mod_name, imports, imports_len, &err);
                lint_stmt(d->as.entry.body, &ctx, &loc, env, ret_ty, &ls);
                locals_free(&loc);
            }
        }
    }

    if (warning_count) *warning_count = ls.warnings;
    if (error_count) *error_count = ls.errors;
    if (mode == YIS_LINT_STRICT) {
        return ls.errors == 0;
    }
    return true;
}

bool typecheck_program(Program *prog, Arena *arena, Diag *err) {
    if (!arena) {
        set_err(err, "internal error: missing arena");
        return false;
    }
    GlobalEnv *env = build_global_env(prog, arena, err);
    if (!env) {
        return false;
    }
    (void)env;
    // typecheck functions
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        Str mod_name = env->cask_names[i].name;
        ModuleImport *imps = find_imports(env, mod_name);
        Str *imports = imps ? imps->imports : NULL;
        size_t imports_len = imps ? imps->imports_len : 0;

        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_FUN) {
                Locals loc;
                locals_init(&loc);
                Ctx ctx;
                ctx.cask_path = m->path;
                ctx.cask_name = mod_name;
                ctx.imports = imports;
                ctx.imports_len = imports_len;
                ctx.has_current_class = false;
                ctx.loop_depth = 0;
                for (size_t p = 0; p < d->as.fun.params_len; p++) {
                    Param *pp = d->as.fun.params[p];
                    Ty *pty = ty_from_type_ref(env, pp->typ, mod_name, imports, imports_len, err);
                    Binding b = { pty, pp->is_mut, false, false };
                    locals_define(&loc, pp->name, b);
                }
                Ty *ret_ty = d->as.fun.ret.is_void ? ty_void(env->arena) : NULL;
                if (!d->as.fun.ret.is_void) {
                    if (d->as.fun.ret.types_len == 1) {
                        ret_ty = ty_from_type_ref(env, d->as.fun.ret.types[0], mod_name, imports, imports_len, err);
                    } else {
                        size_t rn = d->as.fun.ret.types_len;
                        Ty **items = (Ty **)arena_array(env->arena, rn, sizeof(Ty *));
                        for (size_t r = 0; r < rn; r++) {
                            items[r] = ty_from_type_ref(env, d->as.fun.ret.types[r], mod_name, imports, imports_len, err);
                        }
                        ret_ty = ty_tuple(env->arena, items, rn);
                    }
                }
                if (!check_nonvoid_return_coverage(d->as.fun.body, ret_ty, m->path, d->as.fun.name, d->line, d->col, err)) {
                    locals_free(&loc);
                    return false;
                }
                // Skip body checking for empty-body declarations.
                bool is_empty_body = is_empty_body_stub(d->as.fun.body);
                if (!is_empty_body) {
                    tc_stmt_inner(d->as.fun.body, &ctx, &loc, env, ret_ty, err);
                    if (err && err->message) {
                        locals_free(&loc);
                        return false;
                    }
                }
                locals_free(&loc);
            } else if (d->kind == DECL_CLASS) {
                ClassInfo *ci = find_class(env, qualify_class_name(env->arena, mod_name, d->as.class_decl.name));
                if (!ci) continue;
                for (size_t m_i = 0; m_i < d->as.class_decl.methods_len; m_i++) {
                    FunDecl *md = d->as.class_decl.methods[m_i];
                    Locals loc;
                    locals_init(&loc);
                    Ctx ctx;
                    ctx.cask_path = m->path;
                    ctx.cask_name = mod_name;
                    ctx.imports = imports;
                    ctx.imports_len = imports_len;
                    ctx.has_current_class = true;
                    ctx.current_class = ci->qname;
                    ctx.loop_depth = 0;
                    // receiver
                    Ty *self_ty = ty_class(env->arena, ci->qname);
                    Binding self_b = { self_ty, md->params[0]->is_mut, false, false };
                    locals_define(&loc, md->params[0]->name, self_b);
                    for (size_t p = 1; p < md->params_len; p++) {
                        Param *pp = md->params[p];
                        Ty *pty = ty_from_type_ref(env, pp->typ, mod_name, imports, imports_len, err);
                        Binding b = { pty, pp->is_mut, false, false };
                        locals_define(&loc, pp->name, b);
                    }
                    Ty *ret_ty = md->ret.is_void ? ty_void(env->arena) : NULL;
                    if (!md->ret.is_void) {
                        if (md->ret.types_len == 1) {
                            ret_ty = ty_from_type_ref(env, md->ret.types[0], mod_name, imports, imports_len, err);
                        } else {
                            size_t rn = md->ret.types_len;
                            Ty **items = (Ty **)arena_array(env->arena, rn, sizeof(Ty *));
                            for (size_t r = 0; r < rn; r++) {
                                items[r] = ty_from_type_ref(env, md->ret.types[r], mod_name, imports, imports_len, err);
                            }
                            ret_ty = ty_tuple(env->arena, items, rn);
                        }
                    }
                    Str method_name = qualify_class_name(env->arena, ci->name, md->name);
                    int method_line = md->body ? md->body->line : d->line;
                    int method_col = md->body ? md->body->col : d->col;
                    if (!check_nonvoid_return_coverage(md->body, ret_ty, m->path, method_name, method_line, method_col, err)) {
                        locals_free(&loc);
                        return false;
                    }
                    if (!is_empty_body_stub(md->body)) {
                        tc_stmt_inner(md->body, &ctx, &loc, env, ret_ty, err);
                        if (err && err->message) {
                            locals_free(&loc);
                            return false;
                        }
                    }
                    locals_free(&loc);
                }
            } else if (d->kind == DECL_ENTRY) {
                Locals loc;
                locals_init(&loc);
                Ctx ctx;
                ctx.cask_path = m->path;
                ctx.cask_name = mod_name;
                ctx.imports = imports;
                ctx.imports_len = imports_len;
                ctx.has_current_class = false;
                ctx.loop_depth = 0;
                Ty *ret_ty = d->as.entry.ret.is_void ? ty_void(env->arena) : NULL;
                if (!d->as.entry.ret.is_void) {
                    if (d->as.entry.ret.types_len == 1) {
                        ret_ty = ty_from_type_ref(env, d->as.entry.ret.types[0], mod_name, imports, imports_len, err);
                    } else {
                        size_t rn = d->as.entry.ret.types_len;
                        Ty **items = (Ty **)arena_array(env->arena, rn, sizeof(Ty *));
                        for (size_t r = 0; r < rn; r++) {
                            items[r] = ty_from_type_ref(env, d->as.entry.ret.types[r], mod_name, imports, imports_len, err);
                        }
                        ret_ty = ty_tuple(env->arena, items, rn);
                    }
                }
                if (!check_nonvoid_return_coverage(d->as.entry.body, ret_ty, m->path, str_from_c("entry"), d->line, d->col, err)) {
                    locals_free(&loc);
                    return false;
                }
                tc_stmt_inner(d->as.entry.body, &ctx, &loc, env, ret_ty, err);
                if (err && err->message) {
                    locals_free(&loc);
                    return false;
                }
                locals_free(&loc);
            }
        }
    }
    return err == NULL || err->message == NULL;
}
