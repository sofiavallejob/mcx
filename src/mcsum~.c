/*
 * [mcsum~] - sum all channels of a multichannel signal to mono,
 * optionally with one weight per channel (list, wraps), e.g. [weights 1 0.5 0.25(
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 */
#include "m_pd.h"
#include <string.h>

#define MAXW 512
static t_class *mcsum_class;

typedef struct _mcsum {
    t_object x_obj;
    t_float x_f;
    t_float x_w[MAXW];
    int x_nw;
    int x_n, x_nch;
    t_sample *x_in, *x_out, *x_tmp;
    int x_tmpsize;
} t_mcsum;

static t_int *mcsum_perform(t_int *w)
{
    t_mcsum *x = (t_mcsum *)w[1];
    int n = x->x_n, c, j;
    t_sample *acc = x->x_tmp;
    memset(acc, 0, n * sizeof(t_sample));
    for (c = 0; c < x->x_nch; c++)
    {
        t_float g = x->x_nw ? x->x_w[c % x->x_nw] : 1;
        t_sample *in = x->x_in + c * n;
        for (j = 0; j < n; j++) acc[j] += g * in[j];
    }
    memcpy(x->x_out, acc, n * sizeof(t_sample));
    return w + 2;
}

static void mcsum_dsp(t_mcsum *x, t_signal **sp)
{
    int n = sp[0]->s_length;
    x->x_n = n; x->x_nch = sp[0]->s_nchans; x->x_in = sp[0]->s_vec;
    signal_setmultiout(&sp[1], 1);
    x->x_out = sp[1]->s_vec;
    if (x->x_tmpsize < n)
    {
        if (x->x_tmp) freebytes(x->x_tmp, x->x_tmpsize * sizeof(t_sample));
        x->x_tmp = (t_sample *)getbytes(n * sizeof(t_sample));
        x->x_tmpsize = n;
    }
    dsp_add(mcsum_perform, 1, x);
}

static void mcsum_weights(t_mcsum *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    x->x_nw = 0;
    for (i = 0; i < argc && x->x_nw < MAXW; i++)
        if (argv[i].a_type == A_FLOAT) x->x_w[x->x_nw++] = argv[i].a_w.w_float;
}

static void *mcsum_new(t_symbol *s, int argc, t_atom *argv)
{
    t_mcsum *x = (t_mcsum *)pd_new(mcsum_class);
    mcsum_weights(x, 0, argc, argv);
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void mcsum_free(t_mcsum *x)
{
    if (x->x_tmp) freebytes(x->x_tmp, x->x_tmpsize * sizeof(t_sample));
}

void mcsum_tilde_setup(void)
{
    mcsum_class = class_new(gensym("mcsum~"), (t_newmethod)mcsum_new,
        (t_method)mcsum_free, sizeof(t_mcsum), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(mcsum_class, t_mcsum, x_f);
    class_addmethod(mcsum_class, (t_method)mcsum_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(mcsum_class, (t_method)mcsum_weights, gensym("weights"), A_GIMME, 0);
}
