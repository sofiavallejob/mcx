/*
 * [mcprint~ name] - print what is inside a multichannel cable.
 *   bang      print the first sample of every channel, one line
 *   block     print the whole next block, one line per channel
 *   peak      print the peak of each channel over the next block
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 */
#include "m_pd.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

static t_class *mcprint_class;

typedef struct _mcprint {
    t_object x_obj;
    t_float x_f;
    t_symbol *x_name;
    int x_mode;          /* 0 none, 1 first, 2 block, 3 peak */
    int x_n, x_nch;
    t_sample *x_in;
} t_mcprint;

static void mcprint_line(t_mcprint *x, const char *label, int nv, t_sample *v)
{
    char buf[MAXPDSTRING], num[32];
    int i;
    snprintf(buf, sizeof(buf), "%s%s", x->x_name->s_name, label);
    for (i = 0; i < nv; i++)
    {
        snprintf(num, sizeof(num), " %g", v[i]);
        if (strlen(buf) + strlen(num) >= MAXPDSTRING - 5) { strcat(buf, " ..."); break; }
        strcat(buf, num);
    }
    post("%s", buf);
}

static t_int *mcprint_perform(t_int *w)
{
    t_mcprint *x = (t_mcprint *)w[1];
    int c, j, n = x->x_n;
    if (!x->x_mode) return w + 2;
    if (x->x_mode == 1 || x->x_mode == 3)
    {
        t_sample vals[512];
        int nc = x->x_nch < 512 ? x->x_nch : 512;
        for (c = 0; c < nc; c++)
        {
            t_sample *in = x->x_in + c * n;
            if (x->x_mode == 1) vals[c] = in[0];
            else
            {
                t_sample pk = 0;
                for (j = 0; j < n; j++) if (fabs(in[j]) > pk) pk = fabs(in[j]);
                vals[c] = pk;
            }
        }
        mcprint_line(x, x->x_mode == 1 ? ":" : " peak:", nc, vals);
    }
    else
    {
        char label[32];
        for (c = 0; c < x->x_nch; c++)
        {
            snprintf(label, sizeof(label), " ch%d:", c + 1);
            mcprint_line(x, label, n, x->x_in + c * n);
        }
    }
    x->x_mode = 0;
    return w + 2;
}

static void mcprint_dsp(t_mcprint *x, t_signal **sp)
{
    x->x_n = sp[0]->s_length; x->x_nch = sp[0]->s_nchans; x->x_in = sp[0]->s_vec;
    dsp_add(mcprint_perform, 1, x);
}

static void mcprint_bang(t_mcprint *x) { x->x_mode = 1; }
static void mcprint_block(t_mcprint *x) { x->x_mode = 2; }
static void mcprint_peak(t_mcprint *x) { x->x_mode = 3; }

static void *mcprint_new(t_symbol *s)
{
    t_mcprint *x = (t_mcprint *)pd_new(mcprint_class);
    x->x_name = (s && *s->s_name) ? s : gensym("mcprint~");
    return x;
}

void mcprint_tilde_setup(void)
{
    mcprint_class = class_new(gensym("mcprint~"), (t_newmethod)mcprint_new, 0,
        sizeof(t_mcprint), CLASS_MULTICHANNEL, A_DEFSYM, 0);
    CLASS_MAINSIGNALIN(mcprint_class, t_mcprint, x_f);
    class_addmethod(mcprint_class, (t_method)mcprint_dsp, gensym("dsp"), A_CANT, 0);
    class_addbang(mcprint_class, mcprint_bang);
    class_addmethod(mcprint_class, (t_method)mcprint_block, gensym("block"), 0);
    class_addmethod(mcprint_class, (t_method)mcprint_peak, gensym("peak"), 0);
}
