/*
 * [mcexpr~] - multichannel signal expression for Pure Data (0.54+)
 *
 * Evaluates one expression per channel of a multichannel signal.
 *
 *   variables  x1..x8   signal inlets (x = x1). A connection with fewer
 *                       channels than the output wraps around.
 *              f1..f8   control inlets. A float sets all channels, a list
 *                       sets one value per channel (wrapping).
 *              ch       channel index (0 based)   nch  number of channels
 *              sr       sample rate              pi
 *   also accepts expr~ style $v1 / $f1
 *
 *   operators  + - * / % ^  == != < > <= >=  && || !   ( )
 *   functions  sin cos tan asin acos atan atan2 sinh cosh tanh exp log
 *              log10 log2 sqrt abs pow min max floor ceil round fmod
 *              clip(v,lo,hi) wrap(v) if(c,a,b) sign mtof ftom dbtorms
 *              rmstodb rand()
 *
 *   flag       -n N   force N output channels (default: widest input)
 *
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 */

#include "m_pd.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAXSIG 8
#define MAXCTL 8
#define MAXVALS 512
#define MAXPROG 512
#define MAXSTACK 32

enum {
    OP_CONST, OP_SIG, OP_CTL, OP_CH, OP_NCH, OP_SR, OP_RAND,
    OP_NEG, OP_NOT,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
    OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE, OP_AND, OP_OR,
    OP_F1, OP_F2, OP_IF, OP_CLIP
};

enum { F_SIN, F_COS, F_TAN, F_ASIN, F_ACOS, F_ATAN, F_SINH, F_COSH, F_TANH,
       F_EXP, F_LOG, F_LOG10, F_LOG2, F_SQRT, F_ABS, F_FLOOR, F_CEIL, F_ROUND,
       F_WRAP, F_SIGN, F_MTOF, F_FTOM, F_DBTORMS, F_RMSTODB };
enum { G_ATAN2, G_POW, G_MIN, G_MAX, G_FMOD };

typedef struct { int op; int arg; t_float k; } t_instr;

static const char *f1names[] = { "sin", "cos", "tan", "asin", "acos", "atan",
    "sinh", "cosh", "tanh", "exp", "log", "log10", "log2", "sqrt", "abs",
    "floor", "ceil", "round", "wrap", "sign", "mtof", "ftom", "dbtorms",
    "rmstodb", 0 };
static const char *f2names[] = { "atan2", "pow", "min", "max", "fmod", 0 };

static t_class *mcexpr_class;

typedef struct _mcexpr
{
    t_object x_obj;
    t_float x_f;                    /* main inlet scalar */
    t_instr x_prog[MAXPROG];
    int x_nprog;
    int x_nsig, x_nctl;             /* inlets used */
    int x_forcen;                   /* -n flag, 0 = auto */
    t_float x_ctl[MAXCTL][MAXVALS];
    int x_nctlvals[MAXCTL];
    unsigned int x_seed;
    /* dsp state */
    int x_n, x_nout;
    t_float x_sr;
    t_sample *x_in[MAXSIG];
    int x_inch[MAXSIG];
    t_sample *x_out;
    t_sample *x_stack;              /* MAXSTACK * n */
    t_sample *x_tmp;                /* n */
    int x_stacksize;
    t_inlet *x_ctlinlets[MAXCTL];
    t_pd *x_proxy[MAXCTL];
    int x_ok;
} t_mcexpr;

/* ---------------------------- parser ---------------------------- */

typedef struct { const char *s; int pos; t_mcexpr *x; int err; } t_parser;

static void emit(t_parser *p, int op, int arg, t_float k)
{
    t_mcexpr *x = p->x;
    if (x->x_nprog >= MAXPROG) { p->err = 1; return; }
    x->x_prog[x->x_nprog].op = op;
    x->x_prog[x->x_nprog].arg = arg;
    x->x_prog[x->x_nprog].k = k;
    x->x_nprog++;
}

static void skipws(t_parser *p) { while (isspace((unsigned char)p->s[p->pos])) p->pos++; }

static int accept(t_parser *p, const char *tok)
{
    size_t l = strlen(tok);
    skipws(p);
    if (!strncmp(p->s + p->pos, tok, l)) { p->pos += l; return 1; }
    return 0;
}

static void parse_expr(t_parser *p);

static void parse_args(t_parser *p, int n)
{
    int i;
    if (!accept(p, "(")) { p->err = 1; return; }
    for (i = 0; i < n; i++)
    {
        if (i && !accept(p, ",")) { p->err = 1; return; }
        parse_expr(p);
    }
    if (!accept(p, ")")) p->err = 1;
}

static void parse_primary(t_parser *p)
{
    char name[64];
    int i = 0;
    skipws(p);
    if (accept(p, "("))
    {
        parse_expr(p);
        if (!accept(p, ")")) p->err = 1;
        return;
    }
    if (isdigit((unsigned char)p->s[p->pos]) || p->s[p->pos] == '.')
    {
        char *end;
        double v = strtod(p->s + p->pos, &end);
        p->pos = (int)(end - p->s);
        emit(p, OP_CONST, 0, v);
        return;
    }
    /* identifiers (a leading $ as in expr~ is ignored) */
    if (p->s[p->pos] == '$') p->pos++;
    while ((isalnum((unsigned char)p->s[p->pos]) || p->s[p->pos] == '_') && i < 63)
        name[i++] = p->s[p->pos++];
    name[i] = 0;
    if (!i) { p->err = 1; return; }

    if ((name[0] == 'x' || name[0] == 'v' || name[0] == 'f') &&
        (name[1] == 0 || (isdigit((unsigned char)name[1]) && name[2] == 0)))
    {
        int idx = name[1] ? name[1] - '1' : 0;
        if (idx < 0 || idx >= MAXSIG) { p->err = 1; return; }
        if (name[0] == 'f')
        {
            if (!name[1]) goto func;
            emit(p, OP_CTL, idx, 0);
            if (idx + 1 > p->x->x_nctl) p->x->x_nctl = idx + 1;
        }
        else
        {
            emit(p, OP_SIG, idx, 0);
            if (idx + 1 > p->x->x_nsig) p->x->x_nsig = idx + 1;
        }
        return;
    }
    if (!strcmp(name, "ch")) { emit(p, OP_CH, 0, 0); return; }
    if (!strcmp(name, "nch")) { emit(p, OP_NCH, 0, 0); return; }
    if (!strcmp(name, "sr")) { emit(p, OP_SR, 0, 0); return; }
    if (!strcmp(name, "pi")) { emit(p, OP_CONST, 0, 3.14159265358979); return; }
func:
    if (!strcmp(name, "if")) { parse_args(p, 3); emit(p, OP_IF, 0, 0); return; }
    if (!strcmp(name, "clip")) { parse_args(p, 3); emit(p, OP_CLIP, 0, 0); return; }
    if (!strcmp(name, "rand"))
    {
        if (!accept(p, "(") || !accept(p, ")")) { p->err = 1; return; }
        emit(p, OP_RAND, 0, 0);
        return;
    }
    for (i = 0; f1names[i]; i++)
        if (!strcmp(name, f1names[i])) { parse_args(p, 1); emit(p, OP_F1, i, 0); return; }
    for (i = 0; f2names[i]; i++)
        if (!strcmp(name, f2names[i])) { parse_args(p, 2); emit(p, OP_F2, i, 0); return; }
    pd_error(p->x, "mcexpr~: unknown name '%s'", name);
    p->err = 1;
}

static void parse_unary(t_parser *p);

static void parse_pow(t_parser *p)
{
    parse_primary(p);
    if (accept(p, "^")) { parse_unary(p); emit(p, OP_POW, 0, 0); }
}

static void parse_unary(t_parser *p)
{
    if (accept(p, "-")) { parse_unary(p); emit(p, OP_NEG, 0, 0); return; }
    if (accept(p, "+")) { parse_unary(p); return; }
    skipws(p);
    if (p->s[p->pos] == '!' && p->s[p->pos + 1] != '=')
    {
        p->pos++; parse_unary(p); emit(p, OP_NOT, 0, 0); return;
    }
    parse_pow(p);
}

static void parse_mul(t_parser *p)
{
    parse_unary(p);
    for (;;)
    {
        if (accept(p, "*")) { parse_unary(p); emit(p, OP_MUL, 0, 0); }
        else if (accept(p, "/")) { parse_unary(p); emit(p, OP_DIV, 0, 0); }
        else if (accept(p, "%")) { parse_unary(p); emit(p, OP_MOD, 0, 0); }
        else break;
    }
}

static void parse_add(t_parser *p)
{
    parse_mul(p);
    for (;;)
    {
        if (accept(p, "+")) { parse_mul(p); emit(p, OP_ADD, 0, 0); }
        else if (accept(p, "-")) { parse_mul(p); emit(p, OP_SUB, 0, 0); }
        else break;
    }
}

static void parse_cmp(t_parser *p)
{
    parse_add(p);
    for (;;)
    {
        if (accept(p, "==")) { parse_add(p); emit(p, OP_EQ, 0, 0); }
        else if (accept(p, "!=")) { parse_add(p); emit(p, OP_NE, 0, 0); }
        else if (accept(p, "<=")) { parse_add(p); emit(p, OP_LE, 0, 0); }
        else if (accept(p, ">=")) { parse_add(p); emit(p, OP_GE, 0, 0); }
        else if (accept(p, "<")) { parse_add(p); emit(p, OP_LT, 0, 0); }
        else if (accept(p, ">")) { parse_add(p); emit(p, OP_GT, 0, 0); }
        else break;
    }
}

static void parse_and(t_parser *p)
{
    parse_cmp(p);
    while (accept(p, "&&")) { parse_cmp(p); emit(p, OP_AND, 0, 0); }
}

static void parse_expr(t_parser *p)
{
    if (p->err) return;
    parse_and(p);
    while (accept(p, "||")) { parse_and(p); emit(p, OP_OR, 0, 0); }
}

/* stack depth check so the evaluator never overflows */
static int prog_depth(t_mcexpr *x)
{
    int i, d = 0, max = 0;
    for (i = 0; i < x->x_nprog; i++)
    {
        switch (x->x_prog[i].op)
        {
        case OP_CONST: case OP_SIG: case OP_CTL: case OP_CH: case OP_NCH:
        case OP_SR: case OP_RAND: d++; break;
        case OP_NEG: case OP_NOT: case OP_F1: break;
        case OP_IF: case OP_CLIP: d -= 2; break;
        default: d--; break;
        }
        if (d > max) max = d;
    }
    return (d == 1) ? max : -1;
}

/* --------------------------- evaluation --------------------------- */

static t_float f1(int k, t_float a)
{
    switch (k)
    {
    case F_SIN: return sin(a);
    case F_COS: return cos(a);
    case F_TAN: return tan(a);
    case F_ASIN: return asin(a);
    case F_ACOS: return acos(a);
    case F_ATAN: return atan(a);
    case F_SINH: return sinh(a);
    case F_COSH: return cosh(a);
    case F_TANH: return tanh(a);
    case F_EXP: return exp(a);
    case F_LOG: return a > 0 ? log(a) : -1000;
    case F_LOG10: return a > 0 ? log10(a) : -1000;
    case F_LOG2: return a > 0 ? log2(a) : -1000;
    case F_SQRT: return a > 0 ? sqrt(a) : 0;
    case F_ABS: return fabs(a);
    case F_FLOOR: return floor(a);
    case F_CEIL: return ceil(a);
    case F_ROUND: return floor(a + 0.5);
    case F_WRAP: return a - floor(a);
    case F_SIGN: return (a > 0) - (a < 0);
    case F_MTOF: return 8.17579891564 * exp(0.0577622650 * a);
    case F_FTOM: return a > 0 ? 17.3123405046 * log(0.12231220585 * a) : -1500;
    case F_DBTORMS: return a <= 0 ? 0 : exp((2.302585092994 * 0.05) * (a - 100.));
    case F_RMSTODB: return a <= 0 ? 0 : 100 + 20. / 2.302585092994 * log(a);
    }
    return 0;
}

static t_float f2(int k, t_float a, t_float b)
{
    switch (k)
    {
    case G_ATAN2: return atan2(a, b);
    case G_POW: return (a < 0 && b != floor(b)) ? 0 : pow(a, b);
    case G_MIN: return a < b ? a : b;
    case G_MAX: return a > b ? a : b;
    case G_FMOD: return b != 0 ? fmod(a, b) : 0;
    }
    return 0;
}

static void mcexpr_eval(t_mcexpr *x, int c, t_sample *out)
{
    int n = x->x_n, i, j, sp = 0;
    t_sample *st = x->x_stack;
#define TOP(k) (st + (sp - 1 - (k)) * n)
    for (i = 0; i < x->x_nprog; i++)
    {
        t_instr *in = &x->x_prog[i];
        t_sample *a, *b, *d;
        t_float v;
        switch (in->op)
        {
        case OP_CONST: d = st + sp++ * n; for (j = 0; j < n; j++) d[j] = in->k; break;
        case OP_SIG:
        {
            int k = in->arg, nc = x->x_inch[k];
            d = st + sp++ * n;
            memcpy(d, x->x_in[k] + (c % nc) * n, n * sizeof(t_sample));
            break;
        }
        case OP_CTL:
            v = x->x_nctlvals[in->arg] ?
                x->x_ctl[in->arg][c % x->x_nctlvals[in->arg]] : 0;
            d = st + sp++ * n; for (j = 0; j < n; j++) d[j] = v; break;
        case OP_CH: d = st + sp++ * n; for (j = 0; j < n; j++) d[j] = c; break;
        case OP_NCH: d = st + sp++ * n; for (j = 0; j < n; j++) d[j] = x->x_nout; break;
        case OP_SR: d = st + sp++ * n; for (j = 0; j < n; j++) d[j] = x->x_sr; break;
        case OP_RAND:
            d = st + sp++ * n;
            for (j = 0; j < n; j++)
            {
                x->x_seed = x->x_seed * 1664525u + 1013904223u;
                d[j] = (x->x_seed >> 8) * (1.0 / 16777216.0) * 2. - 1.;
            }
            break;
        case OP_NEG: a = TOP(0); for (j = 0; j < n; j++) a[j] = -a[j]; break;
        case OP_NOT: a = TOP(0); for (j = 0; j < n; j++) a[j] = (a[j] == 0); break;
        case OP_F1: a = TOP(0); for (j = 0; j < n; j++) a[j] = f1(in->arg, a[j]); break;
        case OP_F2:
            a = TOP(1); b = TOP(0);
            for (j = 0; j < n; j++) a[j] = f2(in->arg, a[j], b[j]);
            sp--; break;
        case OP_IF:
        {
            t_sample *cnd = TOP(2), *ta = TOP(1), *fb = TOP(0);
            for (j = 0; j < n; j++) cnd[j] = cnd[j] != 0 ? ta[j] : fb[j];
            sp -= 2; break;
        }
        case OP_CLIP:
        {
            t_sample *s = TOP(2), *lo = TOP(1), *hi = TOP(0);
            for (j = 0; j < n; j++)
                s[j] = s[j] < lo[j] ? lo[j] : (s[j] > hi[j] ? hi[j] : s[j]);
            sp -= 2; break;
        }
        default:
            a = TOP(1); b = TOP(0);
            switch (in->op)
            {
            case OP_ADD: for (j = 0; j < n; j++) a[j] += b[j]; break;
            case OP_SUB: for (j = 0; j < n; j++) a[j] -= b[j]; break;
            case OP_MUL: for (j = 0; j < n; j++) a[j] *= b[j]; break;
            case OP_DIV: for (j = 0; j < n; j++) a[j] = b[j] != 0 ? a[j] / b[j] : 0; break;
            case OP_MOD: for (j = 0; j < n; j++) a[j] = b[j] != 0 ? fmod(a[j], b[j]) : 0; break;
            case OP_POW: for (j = 0; j < n; j++) a[j] = f2(G_POW, a[j], b[j]); break;
            case OP_EQ: for (j = 0; j < n; j++) a[j] = a[j] == b[j]; break;
            case OP_NE: for (j = 0; j < n; j++) a[j] = a[j] != b[j]; break;
            case OP_LT: for (j = 0; j < n; j++) a[j] = a[j] < b[j]; break;
            case OP_GT: for (j = 0; j < n; j++) a[j] = a[j] > b[j]; break;
            case OP_LE: for (j = 0; j < n; j++) a[j] = a[j] <= b[j]; break;
            case OP_GE: for (j = 0; j < n; j++) a[j] = a[j] >= b[j]; break;
            case OP_AND: for (j = 0; j < n; j++) a[j] = (a[j] != 0) && (b[j] != 0); break;
            case OP_OR: for (j = 0; j < n; j++) a[j] = (a[j] != 0) || (b[j] != 0); break;
            }
            sp--;
        }
    }
    memcpy(out, st, n * sizeof(t_sample));
#undef TOP
}

static t_int *mcexpr_perform(t_int *w)
{
    t_mcexpr *x = (t_mcexpr *)(w[1]);
    int c, n = x->x_n;
    for (c = 0; c < x->x_nout; c++)
    {
        if (x->x_ok)
        {
            mcexpr_eval(x, c, x->x_tmp);
            memcpy(x->x_out + c * n, x->x_tmp, n * sizeof(t_sample));
        }
        else memset(x->x_out + c * n, 0, n * sizeof(t_sample));
    }
    return (w + 2);
}

static void mcexpr_dsp(t_mcexpr *x, t_signal **sp)
{
    int i, n = sp[0]->s_length, nout = 1, nin = x->x_nsig ? x->x_nsig : 1;
    for (i = 0; i < nin; i++)
    {
        x->x_in[i] = sp[i]->s_vec;
        x->x_inch[i] = sp[i]->s_nchans > 0 ? sp[i]->s_nchans : 1;
        if (x->x_inch[i] > nout) nout = x->x_inch[i];
    }
    if (x->x_forcen > 0) nout = x->x_forcen;
    signal_setmultiout(&sp[nin], nout);
    x->x_out = sp[nin]->s_vec;
    x->x_n = n;
    x->x_nout = nout;
    x->x_sr = sp[0]->s_sr;
    if (x->x_stacksize < n)
    {
        if (x->x_stack) freebytes(x->x_stack, x->x_stacksize * MAXSTACK * sizeof(t_sample));
        if (x->x_tmp) freebytes(x->x_tmp, x->x_stacksize * sizeof(t_sample));
        x->x_stack = (t_sample *)getbytes(n * MAXSTACK * sizeof(t_sample));
        x->x_tmp = (t_sample *)getbytes(n * sizeof(t_sample));
        x->x_stacksize = n;
    }
    dsp_add(mcexpr_perform, 1, x);
}

/* ---------------------------- control ---------------------------- */

static void mcexpr_setctl(t_mcexpr *x, int k, int argc, t_atom *argv)
{
    int i, m = 0;
    for (i = 0; i < argc && m < MAXVALS; i++)
        if (argv[i].a_type == A_FLOAT) x->x_ctl[k][m++] = argv[i].a_w.w_float;
    x->x_nctlvals[k] = m;
}

/* control inlets are small proxy objects so each can take lists */
typedef struct { t_pd p_pd; t_mcexpr *p_owner; int p_k; } t_ctlproxy;
static t_class *ctlproxy_class;

static void ctlproxy_list(t_ctlproxy *p, t_symbol *s, int argc, t_atom *argv)
{
    mcexpr_setctl(p->p_owner, p->p_k, argc, argv);
}

static void ctlproxy_float(t_ctlproxy *p, t_float f)
{
    t_atom a; SETFLOAT(&a, f);
    mcexpr_setctl(p->p_owner, p->p_k, 1, &a);
}

static void mcexpr_seed(t_mcexpr *x, t_float f) { x->x_seed = (unsigned int)f; }

static void *mcexpr_new(t_symbol *s, int argc, t_atom *argv)
{
    t_mcexpr *x = (t_mcexpr *)pd_new(mcexpr_class);
    char buf[4096] = "", tmp[MAXPDSTRING];
    int i, k;
    t_parser p;
    x->x_seed = 307 + (unsigned int)(size_t)x;

    if (argc >= 2 && argv[0].a_type == A_SYMBOL && !strcmp(argv[0].a_w.w_symbol->s_name, "-n"))
    {
        x->x_forcen = (int)atom_getfloat(argv + 1);
        argc -= 2; argv += 2;
    }
    for (i = 0; i < argc; i++)
    {
        if (argv[i].a_type == A_COMMA) strcpy(tmp, ",");
        else if (argv[i].a_type == A_SEMI) strcpy(tmp, ";");
        else atom_string(argv + i, tmp, MAXPDSTRING);
        /* drop the backslash Pd uses to escape $ */
        {
            char *r = tmp, *w2 = tmp;
            while (*r) { if (*r != '\\') *w2++ = *r; r++; }
            *w2 = 0;
        }
        if (strlen(buf) + strlen(tmp) + 2 >= sizeof(buf)) break;
        strcat(buf, tmp);
        strcat(buf, " ");
    }
    p.s = buf; p.pos = 0; p.x = x; p.err = 0;
    if (argc) parse_expr(&p);
    skipws(&p);
    if (!argc) { emit(&p, OP_SIG, 0, 0); x->x_nsig = 1; }
    else if (p.err || p.s[p.pos])
    {
        pd_error(x, "mcexpr~: syntax error at '%s'", p.s + p.pos);
        x->x_nprog = 0;
    }
    if (x->x_nprog && prog_depth(x) > 0 && prog_depth(x) <= MAXSTACK) x->x_ok = 1;
    else if (argc) pd_error(x, "mcexpr~: could not compile '%s'", buf);

    for (k = 1; k < x->x_nsig; k++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    for (k = 0; k < x->x_nctl; k++)
    {
        t_ctlproxy *px = (t_ctlproxy *)pd_new(ctlproxy_class);
        px->p_owner = x; px->p_k = k;
        x->x_proxy[k] = &px->p_pd;
        x->x_ctlinlets[k] = inlet_new(&x->x_obj, &px->p_pd, 0, 0);
    }
    outlet_new(&x->x_obj, &s_signal);
    return (x);
}

static void mcexpr_free(t_mcexpr *x)
{
    int k;
    for (k = 0; k < x->x_nctl; k++) if (x->x_proxy[k]) pd_free(x->x_proxy[k]);
    if (x->x_stack) freebytes(x->x_stack, x->x_stacksize * MAXSTACK * sizeof(t_sample));
    if (x->x_tmp) freebytes(x->x_tmp, x->x_stacksize * sizeof(t_sample));
}

void mcexpr_tilde_setup(void)
{
    mcexpr_class = class_new(gensym("mcexpr~"), (t_newmethod)mcexpr_new,
        (t_method)mcexpr_free, sizeof(t_mcexpr), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(mcexpr_class, t_mcexpr, x_f);
    class_addmethod(mcexpr_class, (t_method)mcexpr_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(mcexpr_class, (t_method)mcexpr_seed, gensym("seed"), A_FLOAT, 0);
    ctlproxy_class = class_new(gensym("mcexpr~-ctl"), 0, 0, sizeof(t_ctlproxy),
        CLASS_PD, 0);
    class_addlist(ctlproxy_class, ctlproxy_list);
    class_addfloat(ctlproxy_class, ctlproxy_float);
}
