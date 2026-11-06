import os
OUT = os.path.join(os.path.dirname(__file__), "..", "help")
W = 780
BG, CTL, LB = "#ffffff", "#5a5a5a", "#303030"


class H:
    def __init__(self, title, height):
        self.lines = [f"#N canvas 60 40 {W} {height} 12;"]
        self.n = 0
        self.conns = []
        self.add(f"#X obj 0 0 cnv 1 {W} 52 empty empty {title} 20 26 0 20 #ececec #202020 0;")

    def add(self, line):
        self.lines.append(line)
        self.n += 1
        return self.n - 1

    def text(self, x, y, t, width=None):
        w = f", f {width}" if width else ""
        return self.add(f"#X text {x} {y} {t}{w};")

    def section(self, y, title):
        self.add(f"#X obj 20 {y} cnv 1 {W - 40} 1 empty empty empty 0 0 0 12 #c8c8c8 #c8c8c8 0;")
        return self.add(f"#X text 20 {y + 8} {title};")

    def obj(self, x, y, t):
        return self.add(f"#X obj {x} {y} {t};")

    def msg(self, x, y, t):
        return self.add(f"#X msg {x} {y} {t};")

    def hsl(self, x, y, lo, hi, log, label, snd="empty", rcv="empty", w=200):
        return self.obj(x, y, f"hsl {w} 18 {lo} {hi} {log} 0 {snd} {rcv} {label} -2 -10 0 12 {BG} {CTL} {LB} 0 1")

    def num(self, x, y, width=8):
        return self.add(f"#X floatatom {x} {y} {width} 0 0 0 - - - 0;")

    def c(self, a, ao, b, bi=0):
        self.conns.append(f"#X connect {a} {ao} {b} {bi};")

    def save(self, name):
        with open(os.path.join(OUT, name), "w") as f:
            f.write("\n".join(self.lines + self.conns) + "\n")


