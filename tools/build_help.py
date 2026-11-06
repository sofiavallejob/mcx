"""Generates help/*-help.pd for mcx."""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from hcore import H

S = "\;"


def dsp(h, x, y):
    on = h.msg(x, y, f"{S} pd dsp 1")
    off = h.msg(x + 110, y, f"{S} pd dsp 0")
    h.text(x + 220, y + 6, "DSP on / off")


# ============================ mcexpr~ ============================
h = H("mcx/mcexpr~", 1180)
h.text(20, 70, "A signal expression that runs once per channel of a multichannel cable. Like expr~ \\, but it knows which channel it is on (ch) and takes one value per channel from lists.", 72)
dsp(h, 40, 130)

h.section(180, "1 - a detuned bank of 8 oscillators from one line")
a = h.obj(40, 220, "mcx/mcexpr~ -n 8 220 * (1 + ch * 0.004)")
b = h.obj(40, 260, "osc~")
c = h.obj(40, 300, "mcx/mcsum~")
d = h.obj(40, 340, "*~ 0.05")
e = h.obj(40, 380, "dac~")
h.c(a, 0, b); h.c(b, 0, c); h.c(c, 0, d); h.c(d, 0, e); h.c(d, 0, e, 1)
h.text(400, 222, "-n 8 makes 8 channels. ch is 0 to 7 \\, so each oscillator is slightly higher.", 40)

h.section(430, "2 - one value per channel from a list")
m1 = h.msg(40, 470, "200 250 300 375")
m2 = h.msg(200, 470, "100 150 200 300")
p = h.obj(40, 520, "mcx/mcexpr~ -n 4 f1")
p2 = h.obj(40, 580, "mcx/mcprint~ freqs")
pb = h.msg(260, 525, "bang")
h.c(m1, 0, p, 1); h.c(m2, 0, p, 1); h.c(p, 0, p2); h.c(pb, 0, p2)
h.text(400, 472, "a list into a control inlet gives each channel its own value. A float sets all channels. Short lists wrap around.", 40)

h.section(620, "3 - process a multichannel signal")
s1 = h.obj(40, 660, "noise~")
s2 = h.obj(140, 660, "osc~ 110")
sn = h.obj(40, 700, "snake~ in 2")
ex = h.obj(40, 740, "mcx/mcexpr~ tanh(x1 * (2 + ch * 6)) * 0.1")
h.c(s1, 0, sn); h.c(s2, 0, sn, 1); h.c(sn, 0, ex)
h.text(420, 702, "x1 is the incoming channel. Here channel 2 is driven harder than channel 1.", 40)

h.section(800, "reference")
h.text(40, 830, "variables:  x1..x8 signal inlets (x = x1)  f1..f8 control inlets  ch channel index from 0  nch channel count  sr sample rate  pi. expr~ style $v1 and $f1 also work.", 76)
h.text(40, 890, "operators:  + - * / % ^  == != < > <= >=  && || !  ( )", 76)
h.text(40, 920, "functions:  sin cos tan asin acos atan atan2 sinh cosh tanh exp log log10 log2 sqrt abs pow min max floor ceil round fmod sign wrap(v) clip(v lo hi) if(cond a b) mtof ftom dbtorms rmstodb rand()", 76)
h.text(40, 980, "channels:  the output has as many channels as the widest signal input. -n N forces N. A narrower input wraps around.", 76)
h.text(40, 1030, "messages:  seed <n> resets rand() so a random patch can be repeated.", 76)
h.save("mcexpr~-help.pd")

# ============================ mcsum~ ============================
h = H("mcx/mcsum~", 560)
h.text(20, 70, "Mixes all channels of a multichannel cable down to one \\, with an optional weight per channel.", 72)
dsp(h, 40, 120)
h.section(170, "example")
a = h.obj(40, 210, "mcx/mcexpr~ -n 4 1")
w1 = h.msg(250, 250, "weights 1 0.5 0.25 0.125")
w2 = h.msg(250, 290, "weights")
s = h.obj(40, 330, "mcx/mcsum~")
pr = h.obj(40, 380, "mcx/mcprint~ sum")
b = h.msg(180, 340, "bang")
h.c(a, 0, s); h.c(w1, 0, s); h.c(w2, 0, s); h.c(s, 0, pr); h.c(b, 0, pr)
h.text(480, 252, "weights wrap around. No weights: plain sum.", 30)
h.text(40, 430, "arguments: optional initial weights \\, e.g. [mcx/mcsum~ 1 0.5]", 72)
h.save("mcsum~-help.pd")

# ============================ mcnchs~ ============================
h = H("mcx/mcnchs~", 480)
h.text(20, 70, "Tells you how many channels a multichannel cable carries. It reports by itself whenever the count changes \\, and on bang.", 72)
dsp(h, 40, 130)
h.section(180, "example")
a = h.obj(40, 220, "mcx/mcexpr~ -n 6 0")
n = h.obj(40, 260, "mcx/mcnchs~")
f = h.num(40, 300, 5)
b = h.msg(220, 220, "bang")
h.c(a, 0, n); h.c(n, 0, f); h.c(b, 0, n)
h.text(330, 222, "change the -n argument and turn DSP on again: the number updates.", 40)
h.save("mcnchs~-help.pd")

# ============================ mcprint~ ============================
h = H("mcx/mcprint~", 500)
h.text(20, 70, "Shows what is inside a multichannel cable in the Pd window.", 72)
dsp(h, 40, 120)
h.section(170, "example")
a = h.obj(40, 210, "mcx/mcexpr~ -n 4 ch * 0.25 + rand() * 0.01")
b = h.msg(40, 260, "bang")
c = h.msg(110, 260, "peak")
d = h.msg(180, 260, "block")
p = h.obj(40, 310, "mcx/mcprint~ test")
h.c(a, 0, p); h.c(b, 0, p); h.c(c, 0, p); h.c(d, 0, p)
h.text(40, 360, "bang: first sample of each channel on one line. peak: the peak of each channel over one block. block: every sample \\, one line per channel.", 72)
h.text(40, 420, "argument: a name shown in front of each line.", 72)
h.save("mcprint~-help.pd")
print("help written")
