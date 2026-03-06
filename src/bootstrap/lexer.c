#include "lexer.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    const char *path;
    const char *src;
    size_t len;
    size_t i;
    int line;
    int col;
    int nest;
    int ret_depth;
    TokKind last_real;
    TokKind last_sig;
    Arena *arena;
} Lexer;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} CharVec;

typedef struct {
    StrPart *data;
    size_t len;
    size_t cap;
} StrPartVec;

static const char *tok_name_default(TokKind kind) {
    switch (kind) {
        case TOK_EOF: return "EOF";
        case TOK_IDENT: return "IDENT";
        case TOK_INT: return "INT";
        case TOK_FLOAT: return "FLOAT";
        case TOK_STR: return "STR";
        case TOK_SEMI: return "SEMI";
        case TOK_LPAR: return "LPAR";
        case TOK_RPAR: return "RPAR";
        case TOK_LBRACK: return "LBRACK";
        case TOK_RBRACK: return "RBRACK";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
        case TOK_COMMA: return "COMMA";
        case TOK_DOT: return "DOT";
        case TOK_COLON: return "COLON";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_BANG: return "!";
        case TOK_EQ: return "=";
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_BAR: return "BAR";
        case TOK_EQEQ: return "==";
        case TOK_NEQ: return "!=";
        case TOK_LTE: return "<=";
        case TOK_GTE: return ">=";
        case TOK_ANDAND: return "&&";
        case TOK_OROR: return "||";
        case TOK_ARROW: return "=>";
        case TOK_PLUSEQ: return "+=";
        case TOK_MINUSEQ: return "-=";
        case TOK_STAREQ: return "*=";
        case TOK_SLASHEQ: return "/=";
        case TOK_QMARK: return "QMARK";
        case TOK_QQ: return "?" "?";
        case TOK_HASH: return "#";
        case TOK_RET_L: return "((";
        case TOK_RET_R: return "))";
        case TOK_RET_VOID: return "--";
        case TOK_KW_cask: return "KW_cask";
        case TOK_KW_bring: return "KW_bring";
        case TOK_KW_fun: return "KW_fun";
        case TOK_KW_macro: return "KW_macro";
        case TOK_KW_entry: return "KW_entry";
        case TOK_KW_class: return "KW_class";
        case TOK_KW_struct: return "KW_struct";
        case TOK_KW_enum: return "KW_enum";
        case TOK_KW_pub: return "KW_pub";
        case TOK_KW_lock: return "KW_lock";
        case TOK_KW_seal: return "KW_seal";
        case TOK_KW_def: return "KW_def";
        case TOK_KW_let: return "KW_let";
        case TOK_KW_const: return "KW_const";
        case TOK_KW_if: return "KW_if";
        case TOK_KW_else: return "KW_else";
        case TOK_KW_elif: return "KW_elif";
        case TOK_KW_return: return "KW_return";
        case TOK_KW_true: return "KW_true";
        case TOK_KW_false: return "KW_false";
        case TOK_KW_null: return "KW_null";
        case TOK_KW_for: return "KW_for";
        case TOK_KW_match: return "KW_match";
        case TOK_KW_new: return "KW_new";
        case TOK_KW_in: return "KW_in";
        case TOK_KW_break: return "KW_break";
        case TOK_KW_continue: return "KW_continue";
        default: return "<invalid>";
    }
}

const char *tok_kind_name(TokKind kind) {
    return tok_name_default(kind);
}

const char *tok_kind_desc(TokKind kind) {
    switch (kind) {
        // Literals
        case TOK_EOF: return "end of file";
        case TOK_IDENT: return "identifier";
        case TOK_INT: return "integer";
        case TOK_FLOAT: return "float";
        case TOK_STR: return "string";
        
        // Punctuation
        case TOK_SEMI: return "';'";
        case TOK_LPAR: return "'('";
        case TOK_RPAR: return "')'";
        case TOK_LBRACK: return "'['";
        case TOK_RBRACK: return "']'";
        case TOK_LBRACE: return "'{'";
        case TOK_RBRACE: return "'}'";
        case TOK_COMMA: return "','";
        case TOK_DOT: return "'.'";
        case TOK_COLON: return "':'";
        
        // Operators
        case TOK_PLUS: return "'+'";
        case TOK_MINUS: return "'-'";
        case TOK_STAR: return "'*'";
        case TOK_SLASH: return "'/'";
        case TOK_PERCENT: return "'%'";
        case TOK_BANG: return "'!'";
        case TOK_EQ: return "'='";
        case TOK_LT: return "'<'";
        case TOK_GT: return "'>'";
        case TOK_BAR: return "'|'";
        case TOK_EQEQ: return "'=='";
        case TOK_NEQ: return "'!='";
        case TOK_LTE: return "'<='";
        case TOK_GTE: return "'>='";
        case TOK_ANDAND: return "'&&'";
        case TOK_OROR: return "'||'";
        case TOK_ARROW: return "'=>'";
        case TOK_PLUSEQ: return "'+='";
        case TOK_MINUSEQ: return "'-='";
        case TOK_STAREQ: return "'*='";
        case TOK_SLASHEQ: return "'/='";
        case TOK_QMARK: return "'?'";
        case TOK_QQ: return "'?" "?'";
        case TOK_HASH: return "'#'";
        
        // Return syntax
        case TOK_RET_L: return "'(('";
        case TOK_RET_R: return "'))'";
        case TOK_RET_VOID: return "'--'";
        
        // Keywords
        case TOK_KW_cask: return "'cask'";
        case TOK_KW_bring: return "'bring'";
        case TOK_KW_fun: return "'fun'";
        case TOK_KW_macro: return "'macro'";
        case TOK_KW_entry: return "'entry'";
        case TOK_KW_class: return "'class'";
        case TOK_KW_struct: return "'struct'";
        case TOK_KW_enum: return "'enum'";
        case TOK_KW_pub: return "'pub'";
        case TOK_KW_lock: return "'lock'";
        case TOK_KW_seal: return "'seal'";
        case TOK_KW_def: return "'def'";
        case TOK_KW_let: return "'let'";
        case TOK_KW_const: return "'const'";
        case TOK_KW_if: return "'if'";
        case TOK_KW_else: return "'else'";
        case TOK_KW_elif: return "'elif'";
        case TOK_KW_return: return "'return'";
        case TOK_KW_true: return "'true'";
        case TOK_KW_false: return "'false'";
        case TOK_KW_null: return "'null'";
        case TOK_KW_for: return "'for'";
        case TOK_KW_match: return "'match'";
        case TOK_KW_new: return "'new'";
        case TOK_KW_in: return "'in'";
        case TOK_KW_break: return "'break'";
        case TOK_KW_continue: return "'continue'";
        
        default: return "unknown token";
    }
}

static char peek(Lexer *lx, size_t k) {
    size_t idx = lx->i + k;
    if (idx >= lx->len) {
        return '\0';
    }
    return lx->src[idx];
}

static void adv(Lexer *lx, size_t n) {
    for (size_t k = 0; k < n; k++) {
        if (lx->i >= lx->len) {
            return;
        }
        char ch = lx->src[lx->i++];
        if (ch == '\n') {
            lx->line += 1;
            lx->col = 1;
        } else {
            lx->col += 1;
        }
    }
}

static bool is_ident_start(char ch) {
    return isalpha((unsigned char)ch) || ch == '_';
}

static bool is_ident_mid(char ch) {
    return isalnum((unsigned char)ch) || ch == '_';
}

// str_from_slice is now in str.h

static char *arena_strndup(Arena *arena, const char *s, size_t len) {
    if (!arena) {
        return NULL;
    }
    char *out = (char *)arena_alloc(arena, len + 1);
    if (!out) {
        return NULL;
    }
    if (len) {
        memcpy(out, s, len);
    }
    out[len] = '\0';
    return out;
}

static Str arena_str(Arena *arena, const char *s, size_t len) {
    char *buf = arena_strndup(arena, s, len);
    Str out;
    out.data = buf ? buf : "";
    out.len = buf ? len : 0;
    return out;
}

static bool tokvec_push(TokVec *vec, Tok tok, Diag *err, Lexer *lx) {
    if (vec->len + 1 > vec->cap) {
        size_t next = vec->cap ? vec->cap * 2 : 64;
        while (next < vec->len + 1) {
            next *= 2;
        }
        Tok *new_data = (Tok *)realloc(vec->data, next * sizeof(Tok));
        if (!new_data) {
            if (err) {
                err->path = lx->path;
                err->line = lx->line;
                err->col = lx->col;
                err->message = "out of memory";
            }
            return false;
        }
        vec->data = new_data;
        vec->cap = next;
    }
    vec->data[vec->len++] = tok;
    return true;
}

static bool charvec_push(CharVec *vec, char c, Diag *err, Lexer *lx) {
    if (vec->len + 1 > vec->cap) {
        size_t next = vec->cap ? vec->cap * 2 : 64;
        while (next < vec->len + 1) {
            next *= 2;
        }
        char *new_data = (char *)realloc(vec->data, next);
        if (!new_data) {
            if (err) {
                err->path = lx->path;
                err->line = lx->line;
                err->col = lx->col;
                err->message = "out of memory";
            }
            return false;
        }
        vec->data = new_data;
        vec->cap = next;
    }
    vec->data[vec->len++] = c;
    return true;
}

static void charvec_free(CharVec *vec) {
    free(vec->data);
    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static bool strpartvec_push(StrPartVec *vec, StrPart part, Diag *err, Lexer *lx) {
    if (vec->len + 1 > vec->cap) {
        size_t next = vec->cap ? vec->cap * 2 : 16;
        while (next < vec->len + 1) {
            next *= 2;
        }
        StrPart *new_data = (StrPart *)realloc(vec->data, next * sizeof(StrPart));
        if (!new_data) {
            if (err) {
                err->path = lx->path;
                err->line = lx->line;
                err->col = lx->col;
                err->message = "out of memory";
            }
            return false;
        }
        vec->data = new_data;
        vec->cap = next;
    }
    vec->data[vec->len++] = part;
    return true;
}

static void strpartvec_free(StrPartVec *vec) {
    free(vec->data);
    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static bool set_error(Lexer *lx, Diag *err, int line, int col, const char *fmt, ...) {
    if (!err) {
        return false;
    }
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    err->path = lx->path;
    err->line = line;
    err->col = col;
    err->message = arena_strndup(lx->arena, buf, strlen(buf));
    if (!err->message) {
        err->message = "lex error";
    }
    return false;
}

static bool append_utf8(CharVec *buf, uint32_t code, Diag *err, Lexer *lx) {
    if (code <= 0x7F) {
        return charvec_push(buf, (char)code, err, lx);
    }
    if (code <= 0x7FF) {
        if (!charvec_push(buf, (char)(0xC0 | ((code >> 6) & 0x1F)), err, lx)) return false;
        return charvec_push(buf, (char)(0x80 | (code & 0x3F)), err, lx);
    }
    if (code <= 0xFFFF) {
        if (!charvec_push(buf, (char)(0xE0 | ((code >> 12) & 0x0F)), err, lx)) return false;
        if (!charvec_push(buf, (char)(0x80 | ((code >> 6) & 0x3F)), err, lx)) return false;
        return charvec_push(buf, (char)(0x80 | (code & 0x3F)), err, lx);
    }
    if (code <= 0x10FFFF) {
        if (!charvec_push(buf, (char)(0xF0 | ((code >> 18) & 0x07)), err, lx)) return false;
        if (!charvec_push(buf, (char)(0x80 | ((code >> 12) & 0x3F)), err, lx)) return false;
        if (!charvec_push(buf, (char)(0x80 | ((code >> 6) & 0x3F)), err, lx)) return false;
        return charvec_push(buf, (char)(0x80 | (code & 0x3F)), err, lx);
    }
    return true;
}

static bool is_stmt_end(TokKind kind) {
    switch (kind) {
        case TOK_RBRACE:
        case TOK_SEMI:
        case TOK_RPAR:
        case TOK_RBRACK:
        case TOK_INT:
        case TOK_FLOAT:
        case TOK_IDENT:
        case TOK_STR:
        case TOK_KW_true:
        case TOK_KW_false:
        case TOK_KW_null:
        case TOK_KW_break:
        case TOK_KW_continue:
            return true;
        default:
            return false;
    }
}

static void set_last(Lexer *lx, TokKind kind) {
    lx->last_real = kind;
    if (kind != TOK_SEMI) {
        lx->last_sig = kind;
    }
}

static bool emit_tok(Lexer *lx, TokVec *out, Diag *err, TokKind kind, Str text, int line, int col, Tok *tok) {
    Tok t;
    memset(&t, 0, sizeof(t));
    t.kind = kind;
    t.text = text;
    t.line = line;
    t.col = col;
    if (tok) {
        *tok = t;
    }
    return tokvec_push(out, t, err, lx);
}

static bool emit_simple(Lexer *lx, TokVec *out, Diag *err, TokKind kind, Str text, int line, int col) {
    return emit_tok(lx, out, err, kind, text, line, col, NULL);
}

static bool emit_ident(Lexer *lx, TokVec *out, Diag *err, Str text, int line, int col) {
    Tok t;
    if (!emit_tok(lx, out, err, TOK_IDENT, text, line, col, &t)) {
        return false;
    }
    out->data[out->len - 1].val.ident = text;
    return true;
}

static bool emit_int(Lexer *lx, TokVec *out, Diag *err, Str text, long long value, int line, int col) {
    Tok t;
    if (!emit_tok(lx, out, err, TOK_INT, text, line, col, &t)) {
        return false;
    }
    out->data[out->len - 1].val.i = value;
    return true;
}

static bool emit_float(Lexer *lx, TokVec *out, Diag *err, Str text, double value, int line, int col) {
    Tok t;
    if (!emit_tok(lx, out, err, TOK_FLOAT, text, line, col, &t)) {
        return false;
    }
    out->data[out->len - 1].val.f = value;
    return true;
}

static bool emit_str(Lexer *lx, TokVec *out, Diag *err, Str text, StrParts *parts, int line, int col) {
    Tok t;
    if (!emit_tok(lx, out, err, TOK_STR, text, line, col, &t)) {
        return false;
    }
    out->data[out->len - 1].val.str = parts;
    return true;
}

static bool make_str_parts(Lexer *lx, StrPartVec *parts, StrParts **out_parts) {
    StrParts *sp = (StrParts *)arena_alloc(lx->arena, sizeof(StrParts));
    if (!sp) {
        return false;
    }
    sp->len = parts->len;
    if (parts->len == 0) {
        sp->parts = NULL;
    } else {
        sp->parts = (StrPart *)arena_alloc(lx->arena, sizeof(StrPart) * parts->len);
        if (!sp->parts) {
            return false;
        }
        memcpy(sp->parts, parts->data, sizeof(StrPart) * parts->len);
    }
    *out_parts = sp;
    return true;
}

static bool flush_text_part(Lexer *lx, CharVec *buf, StrPartVec *parts, Diag *err) {
    if (buf->len == 0) {
        return true;
    }
    StrPart part;
    part.kind = STR_PART_TEXT;
    part.as.text = arena_str(lx->arena, buf->data, buf->len);
    if (!strpartvec_push(parts, part, err, lx)) {
        return false;
    }
    buf->len = 0;
    return true;
}

static bool append_hex_code(Lexer *lx, CharVec *buf, const char *hex, size_t hex_len, Diag *err, int line, int col) {
    if (hex_len == 0) {
        return set_error(lx, err, line, col, "bad \\u{...} escape");
    }
    uint32_t code = 0;
    for (size_t i = 0; i < hex_len; i++) {
        char c = hex[i];
        int val = 0;
        if (c >= '0' && c <= '9') {
            val = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            val = 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            val = 10 + (c - 'A');
        } else {
            return set_error(lx, err, line, col, "bad \\u{...} escape");
        }
        code = (code << 4) | (uint32_t)val;
        if (code > 0x10FFFF) {
            return set_error(lx, err, line, col, "bad \\u{...} escape");
        }
    }
    return append_utf8(buf, code, err, lx);
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static bool append_hex_escape(Lexer *lx, CharVec *buf, Diag *err, int line, int col) {
    unsigned int value = 0;
    for (int i = 0; i < 2; i++) {
        int hv = hex_value(peek(lx, 0));
        if (hv < 0) {
            return set_error(lx, err, line, col, "bad \\xHH escape");
        }
        value = (value << 4) | (unsigned int)hv;
        adv(lx, 1);
    }
    return append_utf8(buf, (uint32_t)value, err, lx);
}

static bool append_octal_escape(Lexer *lx, CharVec *buf, Diag *err, int line, int col) {
    unsigned int value = 0;
    int digits = 0;
    while (digits < 3) {
        char c = peek(lx, 0);
        if (c < '0' || c > '7') {
            break;
        }
        value = (value << 3) | (unsigned int)(c - '0');
        adv(lx, 1);
        digits++;
    }
    if (digits == 0) {
        return set_error(lx, err, line, col, "bad \\ooo escape");
    }
    if (value > 0xFF) {
        return set_error(lx, err, line, col, "bad \\ooo escape");
    }
    return append_utf8(buf, (uint32_t)value, err, lx);
}

bool lex_source(const char *path, const char *src, size_t len, Arena *arena, TokVec *out, Diag *err) {
    if (!out) {
        return false;
    }
    out->data = NULL;
    out->len = 0;
    out->cap = 0;

    Lexer lx;
    lx.path = path ? path : "";
    lx.src = src ? src : "";
    lx.len = len;
    lx.i = 0;
    lx.line = 1;
    lx.col = 0;
    lx.nest = 0;
    lx.ret_depth = 0;
    lx.last_real = TOK_INVALID;
    lx.last_sig = TOK_INVALID;
    lx.arena = arena;

    while (lx.i < lx.len) {
        char ch = peek(&lx, 0);
        char two0 = ch;
        char two1 = peek(&lx, 1);

        if (ch == ' ' || ch == '\t' || ch == '\r') {
            adv(&lx, 1);
            continue;
        }

        if (ch == '\n') {
            adv(&lx, 1);
            if (lx.nest == 0 && is_stmt_end(lx.last_sig)) {
                if (!emit_simple(&lx, out, err, TOK_SEMI, STR_LIT(";"), lx.line - 1, 0)) {
                    return false;
                }
                lx.last_real = TOK_SEMI;
            }
            continue;
        }

        if (two0 == '(' && two1 == '(' && lx.ret_depth == 0 && lx.last_sig == TOK_RPAR) {
            if (!emit_simple(&lx, out, err, TOK_RET_L, STR_LIT("(("), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            lx.ret_depth += 1;
            set_last(&lx, TOK_RET_L);
            continue;
        }

        if (two0 == ')' && two1 == ')' && lx.ret_depth > 0) {
            if (!emit_simple(&lx, out, err, TOK_RET_R, STR_LIT("))"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            if (lx.ret_depth > 0) {
                lx.ret_depth -= 1;
            }
            set_last(&lx, TOK_RET_R);
            continue;
        }

        if (two0 == '-' && two1 == '-' && lx.ret_depth > 0) {
            if (!emit_simple(&lx, out, err, TOK_RET_VOID, STR_LIT("--"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_RET_VOID);
            continue;
        }

        if (two0 == '-' && two1 == '-' && lx.ret_depth == 0) {
            adv(&lx, 2);
            while (lx.i < lx.len && peek(&lx, 0) != '\n') {
                adv(&lx, 1);
            }
            continue;
        }

        if (two0 == '=' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_EQEQ, STR_LIT("=="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_EQEQ);
            continue;
        }
        if (two0 == '!' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_NEQ, STR_LIT("!="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_NEQ);
            continue;
        }
        if (two0 == '<' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_LTE, STR_LIT("<="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_LTE);
            continue;
        }
        if (two0 == '>' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_GTE, STR_LIT(">="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_GTE);
            continue;
        }
        if (two0 == '&' && two1 == '&') {
            if (!emit_simple(&lx, out, err, TOK_ANDAND, STR_LIT("&&"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_ANDAND);
            continue;
        }
        if (two0 == '|' && two1 == '|') {
            if (!emit_simple(&lx, out, err, TOK_OROR, STR_LIT("||"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_OROR);
            continue;
        }
        if (two0 == '=' && two1 == '>') {
            if (!emit_simple(&lx, out, err, TOK_ARROW, STR_LIT("=>"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_ARROW);
            continue;
        }
        if (two0 == '+' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_PLUSEQ, STR_LIT("+="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_PLUSEQ);
            continue;
        }
        if (two0 == '-' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_MINUSEQ, STR_LIT("-="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_MINUSEQ);
            continue;
        }
        if (two0 == '*' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_STAREQ, STR_LIT("*="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_STAREQ);
            continue;
        }
        if (two0 == '/' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_SLASHEQ, STR_LIT("/="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_SLASHEQ);
            continue;
        }
        if (two0 == '?' && two1 == '?') {
            if (!emit_simple(&lx, out, err, TOK_QQ, STR_LIT("??"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_QQ);
            continue;
        }

        if (ch == ';') {
            if (!emit_simple(&lx, out, err, TOK_SEMI, STR_LIT(";"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 1);
            lx.last_real = TOK_SEMI;
            continue;
        }

        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
            ch == ',' || ch == '.' || ch == ':' || ch == '+' || ch == '-' || ch == '*' ||
            ch == '/' || ch == '%' || ch == '!' || ch == '=' || ch == '<' || ch == '>' ||
            ch == '|') {
            TokKind kind = TOK_INVALID;
            switch (ch) {
                case '(': kind = TOK_LPAR; break;
                case ')': kind = TOK_RPAR; break;
                case '[': kind = TOK_LBRACK; break;
                case ']': kind = TOK_RBRACK; break;
                case '{': kind = TOK_LBRACE; break;
                case '}': kind = TOK_RBRACE; break;
                case ',': kind = TOK_COMMA; break;
                case '.': kind = TOK_DOT; break;
                case ':': kind = TOK_COLON; break;
                case '+': kind = TOK_PLUS; break;
                case '-': kind = TOK_MINUS; break;
                case '*': kind = TOK_STAR; break;
                case '/': kind = TOK_SLASH; break;
                case '%': kind = TOK_PERCENT; break;
                case '!': kind = TOK_BANG; break;
                case '=': kind = TOK_EQ; break;
                case '<': kind = TOK_LT; break;
                case '>': kind = TOK_GT; break;
                case '|': kind = TOK_BAR; break;
                default: kind = TOK_INVALID; break;
            }
            if (!emit_simple(&lx, out, err, kind, str_from_slice(&lx.src[lx.i], 1), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 1);

            if (ch == '(' || ch == '[' || ch == '{') {
                lx.nest += 1;
            } else if (ch == ')' || ch == ']' || ch == '}') {
                if (lx.nest > 0) {
                    lx.nest -= 1;
                }
            }

            set_last(&lx, kind);
            continue;
        }

        if (ch == '?') {
            if (!emit_simple(&lx, out, err, TOK_QMARK, STR_LIT("?"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 1);
            set_last(&lx, TOK_QMARK);
            continue;
        }

        if (ch == '#') {
            if (!emit_simple(&lx, out, err, TOK_HASH, STR_LIT("#"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 1);
            set_last(&lx, TOK_HASH);
            continue;
        }

        if (ch == '"') {
            int start_line = lx.line;
            int start_col = lx.col;
            adv(&lx, 1);

            StrPartVec parts = {0};
            CharVec buf = {0};

            while (lx.i < lx.len) {
                char c = peek(&lx, 0);
                if (c == '"') {
                    adv(&lx, 1);
                    if (!flush_text_part(&lx, &buf, &parts, err)) {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return false;
                    }
                    StrParts *sp = NULL;
                    if (!make_str_parts(&lx, &parts, &sp)) {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return set_error(&lx, err, start_line, start_col, "out of memory");
                    }
                    if (!emit_str(&lx, out, err, STR_LIT("\"...\""), sp, start_line, start_col)) {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return false;
                    }
                    set_last(&lx, TOK_STR);
                    break;
                }
                if (c == '\n') {
                    strpartvec_free(&parts);
                    charvec_free(&buf);
                    return set_error(&lx, err, start_line, start_col, "unterminated string");
                }
                if (c == '\\') {
                    adv(&lx, 1);
                    char e = peek(&lx, 0);
                    if (e == 'n') {
                        if (!charvec_push(&buf, '\n', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 'a') {
                        if (!charvec_push(&buf, '\a', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 'b') {
                        if (!charvec_push(&buf, '\b', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 'e') {
                        if (!charvec_push(&buf, 0x1B, err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 'f') {
                        if (!charvec_push(&buf, '\f', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 't') {
                        if (!charvec_push(&buf, '\t', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 'r') {
                        if (!charvec_push(&buf, '\r', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 'v') {
                        if (!charvec_push(&buf, '\v', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == '\\') {
                        if (!charvec_push(&buf, '\\', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == '"') {
                        if (!charvec_push(&buf, '"', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == '\'') {
                        if (!charvec_push(&buf, '\'', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == '<') {
                        if (!charvec_push(&buf, '<', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == '>') {
                        if (!charvec_push(&buf, '>', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == '$') {
                        if (!charvec_push(&buf, '$', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == '?') {
                        if (!charvec_push(&buf, '?', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 'x') {
                        adv(&lx, 1);
                        if (!append_hex_escape(&lx, &buf, err, lx.line, lx.col)) {
                            strpartvec_free(&parts);
                            charvec_free(&buf);
                            return false;
                        }
                    } else if (e >= '0' && e <= '7') {
                        if (!append_octal_escape(&lx, &buf, err, lx.line, lx.col)) {
                            strpartvec_free(&parts);
                            charvec_free(&buf);
                            return false;
                        }
                    } else if (e == 'u' && peek(&lx, 1) == '{') {
                        adv(&lx, 2);
                        CharVec hexbuf = {0};
                        while (lx.i < lx.len && peek(&lx, 0) != '}') {
                            if (!charvec_push(&hexbuf, peek(&lx, 0), err, &lx)) { return false; }
                            adv(&lx, 1);
                        }
                        if (peek(&lx, 0) != '}') {
                            strpartvec_free(&parts);
                            charvec_free(&buf);
                            charvec_free(&hexbuf);
                            return set_error(&lx, err, lx.line, lx.col, "bad \\u{...} escape");
                        }
                        adv(&lx, 1);
                        if (!append_hex_code(&lx, &buf, hexbuf.data, hexbuf.len, err, lx.line, lx.col)) {
                            strpartvec_free(&parts);
                            charvec_free(&buf);
                            charvec_free(&hexbuf);
                            return false;
                        }
                        charvec_free(&hexbuf);
                    } else {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return set_error(&lx, err, lx.line, lx.col, "unknown escape");
                    }
                    continue;
                }
                if (c == '$' && peek(&lx, 1) == '$') {
                    // Start of $$...$$ interpolation placeholder
                    if (!flush_text_part(&lx, &buf, &parts, err)) {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return false;
                    }
                    adv(&lx, 2); // consume opening $$
                    size_t path_start = lx.i;

                    while (lx.i < lx.len) {
                        char pc = peek(&lx, 0);
                        if (pc == '\n' || pc == '\0') {
                            strpartvec_free(&parts);
                            charvec_free(&buf);
                            return set_error(&lx, err, start_line, start_col, "unterminated $$ placeholder");
                        }
                        if (pc == '$' && peek(&lx, 1) == '$') {
                            break;
                        }
                        adv(&lx, 1);
                    }

                    if (!(peek(&lx, 0) == '$' && peek(&lx, 1) == '$')) {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return set_error(&lx, err, start_line, start_col, "unterminated $$ placeholder");
                    }

                    size_t path_end = lx.i;
                    adv(&lx, 2); // consume closing $$

                    StrPart part;
                    part.kind = STR_PART_EXPR_RAW;
                    part.as.text = arena_str(lx.arena, lx.src + path_start, path_end - path_start);
                    if (!strpartvec_push(&parts, part, err, &lx)) { return false; }
                    continue;
                }
                if (!charvec_push(&buf, c, err, &lx)) { return false; }
                adv(&lx, 1);
            }
            strpartvec_free(&parts);
            charvec_free(&buf);
            continue;
        }

        if (isdigit((unsigned char)ch)) {
            int start_line = lx.line;
            int start_col = lx.col;
            size_t start = lx.i;
            while (lx.i < lx.len && isdigit((unsigned char)peek(&lx, 0))) {
                adv(&lx, 1);
            }
            bool is_float = false;
            if (peek(&lx, 0) == '.' && isdigit((unsigned char)peek(&lx, 1))) {
                is_float = true;
                adv(&lx, 1);
                while (lx.i < lx.len && isdigit((unsigned char)peek(&lx, 0))) {
                    adv(&lx, 1);
                }
            }
            size_t end = lx.i;
            Str text = str_from_slice(&lx.src[start], end - start);
            if (is_float) {
                char stack_tmp[64];
                char *tmp = (text.len < sizeof(stack_tmp)) ? stack_tmp : (char *)malloc(text.len + 1);
                if (!tmp) {
                    return set_error(&lx, err, start_line, start_col, "out of memory");
                }
                memcpy(tmp, text.data, text.len);
                tmp[text.len] = '\0';
                double val = strtod(tmp, NULL);
                if (tmp != stack_tmp) free(tmp);
                if (!emit_float(&lx, out, err, text, val, start_line, start_col)) {
                    return false;
                }
                set_last(&lx, TOK_FLOAT);
            } else {
                long long val = 0;
                for (size_t k = 0; k < text.len; k++) {
                    val = val * 10 + (long long)(text.data[k] - '0');
                }
                if (!emit_int(&lx, out, err, text, val, start_line, start_col)) {
                    return false;
                }
                set_last(&lx, TOK_INT);
            }
            continue;
        }

        if (is_ident_start(ch)) {
            int start_line = lx.line;
            int start_col = lx.col;
            size_t start = lx.i;
            while (lx.i < lx.len && is_ident_mid(peek(&lx, 0))) {
                adv(&lx, 1);
            }
            size_t end = lx.i;
            Str word = str_from_slice(&lx.src[start], end - start);

            TokKind kw = TOK_INVALID;
            switch (word.len) {
            case 2:
                if (word.data[0] == 'i' && word.data[1] == 'f') kw = TOK_KW_if;
                else if (word.data[0] == 'i' && word.data[1] == 'n') kw = TOK_KW_in;
                break;
            case 3:
                // Use direct character comparison for better branch prediction
                if (word.data[0] == 'f' && word.data[1] == 'u' && word.data[2] == 'n') kw = TOK_KW_fun;
                else if (word.data[0] == 'p' && word.data[1] == 'u' && word.data[2] == 'b') kw = TOK_KW_pub;
                else if (word.data[0] == 'd' && word.data[1] == 'e' && word.data[2] == 'f') kw = TOK_KW_def;
                else if (word.data[0] == 'l' && word.data[1] == 'e' && word.data[2] == 't') kw = TOK_KW_let;
                else if (word.data[0] == 'f' && word.data[1] == 'o' && word.data[2] == 'r') kw = TOK_KW_for;
                else if (word.data[0] == 'n' && word.data[1] == 'e' && word.data[2] == 'w') kw = TOK_KW_new;
                break;
            case 4:
                // Use memcmp with length check already done by case
                if (memcmp(word.data, "cask", 4) == 0) kw = TOK_KW_cask;
                else if (memcmp(word.data, "enum", 4) == 0) kw = TOK_KW_enum;
                else if (memcmp(word.data, "lock", 4) == 0) kw = TOK_KW_lock;
                else if (memcmp(word.data, "seal", 4) == 0) kw = TOK_KW_seal;
                else if (memcmp(word.data, "else", 4) == 0) kw = TOK_KW_else;
                else if (memcmp(word.data, "elif", 4) == 0) kw = TOK_KW_elif;
                else if (memcmp(word.data, "true", 4) == 0) kw = TOK_KW_true;
                else if (memcmp(word.data, "null", 4) == 0) kw = TOK_KW_null;
                break;
            case 5:
                if (memcmp(word.data, "bring", 5) == 0) kw = TOK_KW_bring;
                else if (memcmp(word.data, "entry", 5) == 0) kw = TOK_KW_entry;
                else if (memcmp(word.data, "class", 5) == 0) kw = TOK_KW_class;
                else if (memcmp(word.data, "const", 5) == 0) kw = TOK_KW_const;
                else if (memcmp(word.data, "false", 5) == 0) kw = TOK_KW_false;
                else if (memcmp(word.data, "match", 5) == 0) kw = TOK_KW_match;
                else if (memcmp(word.data, "macro", 5) == 0) kw = TOK_KW_macro;
                else if (memcmp(word.data, "break", 5) == 0) kw = TOK_KW_break;
                break;
            case 6:
                if (memcmp(word.data, "struct", 6) == 0) kw = TOK_KW_struct;
                else if (memcmp(word.data, "return", 6) == 0) kw = TOK_KW_return;
                break;
            case 8:
                if (memcmp(word.data, "continue", 8) == 0) kw = TOK_KW_continue;
                break;
            }

            if (kw != TOK_INVALID) {
                if (!emit_simple(&lx, out, err, kw, word, start_line, start_col)) {
                    return false;
                }
                set_last(&lx, kw);
            } else {
                if (!emit_ident(&lx, out, err, word, start_line, start_col)) {
                    return false;
                }
                set_last(&lx, TOK_IDENT);
            }
            continue;
        }

        // Show the unexpected character and its location in the error message
        char unexpected = peek(&lx, 0);
        if ((unsigned char)unexpected < 32 || unexpected == 127) {
            // Non-printable characters - show as hex
            return set_error(&lx, err, lx.line, lx.col, "unexpected character 0x%02X at line %d, column %d", (unsigned char)unexpected, lx.line, lx.col);
        } else {
            // Regular characters - show as-is
            return set_error(&lx, err, lx.line, lx.col, "unexpected character '%c' at line %d, column %d", unexpected, lx.line, lx.col);
        }
    }

    if (lx.nest == 0 && is_stmt_end(lx.last_sig)) {
        if (!emit_simple(&lx, out, err, TOK_SEMI, STR_LIT(";"), lx.line, lx.col)) {
            return false;
        }
        lx.last_real = TOK_SEMI;
    }

    if (out->len > 1) {
        size_t w = 0;
        for (size_t r = 0; r < out->len; r++) {
            if (out->data[r].kind == TOK_SEMI && w > 0 && out->data[w - 1].kind == TOK_SEMI) {
                continue;
            }
            out->data[w++] = out->data[r];
        }
        out->len = w;
    }

    return true;
}
