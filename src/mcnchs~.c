/*
 * [mcnchs~] - outputs the channel count of a multichannel connection,
 * automatically whenever it changes (DSP restart) and on bang.
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 */
#include "m_pd.h"

static t_class *mcnchs_class;

typedef struct _mcnchs {
    t_object x_obj;
    t_float x_f;
    int x_nch, x_last;
    t_clock *x_clock;
    t_outlet *x_out;
} t_mcnchs;

static void mcnchs_bang(t_mcnchs *x) { outlet_float(x->x_out, x->x_nch); }

static void mcnchs_tick(t_mcnchs *x)
{
    if (x->x_nch != x->x_last) { x->x_last = x->x_nch; mcnchs_bang(x); }
}

static void mcnchs_dsp(t_mcnchs *x, t_signal **sp)
{
    x->x_nch = sp[0]->s_nchans;
    clock_delay(x->x_clock, 0);   /* report outside the DSP sort */
}

static void *mcnchs_new(void)
{
    t_mcnchs *x = (t_mcnchs *)pd_new(mcnchs_class);
    x->x_nch = 1; x->x_last = -1;
    x->x_clock = clock_new(x, (t_method)mcnchs_tick);
    x->x_out = outlet_new(&x->x_obj, &s_float);
    return x;
}

static void mcnchs_free(t_mcnchs *x) { clock_free(x->x_clock); }

void mcnchs_tilde_setup(void)
{
    mcnchs_class = class_new(gensym("mcnchs~"), (t_newmethod)mcnchs_new,
        (t_method)mcnchs_free, sizeof(t_mcnchs), CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(mcnchs_class, t_mcnchs, x_f);
    class_addmethod(mcnchs_class, (t_method)mcnchs_dsp, gensym("dsp"), A_CANT, 0);
    class_addbang(mcnchs_class, mcnchs_bang);
}
