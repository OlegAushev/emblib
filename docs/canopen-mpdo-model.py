#!/usr/bin/env python3
"""A model of the MPDO source of canopen-pdo-plan.ru.md (decision 4, 2.7).

Run: python3 -I canopen-mpdo-model.py [--runs 1000] [--only abcdef]
(about 40 s)

The model, stated once so that its numbers can be repeated:

- N = 39 objects, positions 0..38 (subindices 01h..27h of the object of
  statuses). A value is 0 ("no") or 1.
- The TPDO slot asks for a frame once a period T = 10 ms. Frame n goes out
  at t = phase + n * T. In this model `send` takes every frame, so `sent()`
  follows every `next()`: the shadow, the cursors and the counters move on
  every frame.
- A change at time t is seen by every frame at t' >= t.
- The shadow starts at 0. The change cursor starts on the last object, so
  its first search starts at 01h; the round cursor starts on the first
  object.
- The change cursor searches from the object after itself around the
  circle up to itself inclusive (`self_search=True`, as the plan has it),
  or only the other N - 1 objects (`self_search=False`, which the plan
  rejects).
- A frame from either cursor sets the shadow of its object to the value it
  carries; the change cursor never sends an object whose shadow already
  holds its value.

The rules:

- R1: one cursor. The first changed object after the cursor; if there is
  none, the next object of the circle after the same cursor. Either way
  the cursor moves to the object sent.
- R2: two cursors, no quota. A changed object if there is one, otherwise
  the round cursor.
- R3: two cursors, a counter of the frames that `send` took (in this
  model, every frame), modulo 4. The frame with counter 0 always comes
  from the round cursor; the other three come from the change cursor if
  anything changed, otherwise from the round cursor. The counter starts
  at 0, so the first frame is a round frame.
- R4: two cursors, a quota. At most three frames of the change cursor in a
  row; any frame of the round cursor, including one sent because nothing
  changed, resets the count.

Metrics:

(a) status 35 (position 35, subindex 24h) toggles 0 <-> 1 with probability
    p per period, before that period's frame; the host joins at 10 s, right
    after a frame; the time until it has received each of the 39 objects at
    least once is the number of frames it needed times T. 1000 runs, seeds
    0..999 (`random.Random(seed)`), the same sequence of toggles for every
    rule. A run gives up 50 s after the join. Percentiles are nearest-rank.
(b) the worst latency of a single change, over every reachable state of the
    cursors and counters and every object, in periods (a change just after
    a frame waits the whole period for the next one).
(c) k simultaneous changes: periods until the last of them is sent.
(d) the example of decision 4: change cursor on 01h; 1Fh changes at
    t = 0, 0Bh at 5 ms, 15h at 15 ms; the order sent, by the phase of the
    first frame at or after t = 0.
(e) a status raised alone, sent, then cleared alone: periods until the
    clear is sent, with and without the cursor's own object.
(a') as (a), with the search that skips the cursor's own object.
(b') a single change while status 35 toggles every period.
(f) the bound on a full picture under any rate of change: every object, or
    status 35 alone, toggles every period or differs from its shadow before
    every frame; the worst join.

The numbers of section 2.11 of the plan are the table that (a) prints, and
the worst cases of (b)-(f) in periods.
"""

import argparse
import itertools
import math
import random
import sys

N = 39
FULL = (1 << N) - 1
T_MS = 10


class Scanner:
    """`next()` picks a frame from the state; `sent()` commits it."""

    def __init__(self, rule, self_search=True, change_cursor=N - 1,
                 round_cursor=0, counter=0):
        assert rule in ("R1", "R2", "R3", "R4")
        self.rule = rule
        self.self_search = self_search
        self.value = [0] * N    # what the readers of the dictionary return
        self.shadow = [0] * N   # what `send` last took
        self.dirty = 0          # bit i: value[i] != shadow[i]
        self.chg = change_cursor
        self.rnd = round_cursor
        self.counter = counter  # R3: frames mod 4; R4: change frames in a row
        self.cand = None

    def write(self, i, v):
        self.value[i] = v
        if v != self.shadow[i]:
            self.dirty |= 1 << i
        else:
            self.dirty &= ~(1 << i)

    def _changed(self, cursor):
        d = self.dirty
        if not self.self_search:
            d &= ~(1 << cursor)
        s = (cursor + 1) % N
        hi = d >> s
        if hi:
            return s + (hi & -hi).bit_length() - 1
        lo = d & ((1 << s) - 1)
        if lo:
            return (lo & -lo).bit_length() - 1
        return None

    def next(self):
        r = self.rule
        if r == "R1":
            obj = self._changed(self.chg)
            src = "C"
            if obj is None:
                obj, src = (self.chg + 1) % N, "R"
        else:
            obj = None
            if r == "R2":
                obj = self._changed(self.chg)
            elif r == "R3":
                if self.counter != 0:
                    obj = self._changed(self.chg)
            else:
                if self.counter < 3:
                    obj = self._changed(self.chg)
            src = "C"
            if obj is None:
                obj, src = self.rnd, "R"
        self.cand = (obj, self.value[obj], src)
        return self.cand

    def sent(self):
        obj, v, src = self.cand
        self.shadow[obj] = v
        if self.value[obj] == v:
            self.dirty &= ~(1 << obj)
        else:
            self.dirty |= 1 << obj
        r = self.rule
        if r == "R1":
            self.chg = obj
        elif src == "C":
            self.chg = obj
        else:
            self.rnd = (self.rnd + 1) % N
        if r == "R3":
            self.counter = (self.counter + 1) % 4
        elif r == "R4":
            self.counter = self.counter + 1 if src == "C" else 0
        self.cand = None


def counters(rule):
    """The values the counter can hold while the source is idle or busy."""
    return range(4) if rule in ("R3", "R4") else range(1)


# -- (a) the time until a host that joined sees every object --------------

JOIN = 1000          # frames: 10 s
CAP = 5000           # frames after the join: 50 s
STATUS = 35


def join_run(rule, p, seed, self_search=True):
    rng = random.Random(seed)
    s = Scanner(rule, self_search)
    seen = 0
    v = 0
    for n in range(JOIN + CAP):
        if rng.random() < p:
            v ^= 1
            s.write(STATUS, v)
        obj, _, _ = s.next()
        s.sent()
        if n >= JOIN:
            seen |= 1 << obj
            if seen == FULL:
                return (n - JOIN + 1), 0
    return math.inf, N - bin(seen).count("1")


def nearest_rank(sorted_xs, q):
    return sorted_xs[max(0, math.ceil(q * len(sorted_xs)) - 1)]


def fmt_s(frames):
    if frames == math.inf:
        return "> 50 с"
    return f"{frames * T_MS / 1000:.2f} с".replace(".", ",")


def metric_a(runs, rules):
    print(f"(a) статус 35 меняется с вероятностью p за период, хост — с 10-й "
          f"секунды; время до полной картины, {runs} прогонов "
          f"(медиана / 95-й процентиль / максимум)")
    table = {}
    for p in (0.1, 0.5, 0.9):
        for rule in rules:
            res = [join_run(rule, p, seed) for seed in range(runs)]
            times = sorted(t for t, _ in res)
            missing = sorted(m for _, m in res)
            lost = sum(1 for t in times if t == math.inf)
            cell = " / ".join(fmt_s(nearest_rank(times, q))
                              for q in (0.5, 0.95, 1.0))
            if lost:
                gone = [m for m in missing if m]
                cell += (f"; не пришли за 50 с в {lost} из {runs}, "
                         f"не хватает {nearest_rank(gone, 0.5)} из 39 "
                         f"(медиана), от {gone[0]} до {gone[-1]}")
            seed1 = join_run(rule, p, 1)
            cell1 = (fmt_s(seed1[0]) if seed1[0] != math.inf
                     else f"{seed1[1]} из 39 не пришли")
            if lost == runs:
                short = (f"за 50 с не пришли {nearest_rank(gone, 0.5)} из 39 "
                         f"(от {gone[0]} до {gone[-1]})")
            else:
                short = " / ".join(
                    f"{nearest_rank(times, q) * T_MS / 1000:.2f}"
                    for q in (0.5, 0.95, 1.0)).replace(".", ",") + " с"
                if lost:
                    short += f"; за 50 с — не в {lost} из {runs}"
            table[(p, rule)] = short
            print(f"  p = {p:.1f}  {rule}: {cell}   [seed 1: {cell1}]")
    print()
    print("  | p | один курсор | второй курсор обхода "
          "| и каждый 4-й кадр из обхода | квота: не больше 3 подряд |")
    print("  |---|---|---|---|---|")
    for p in (0.1, 0.5, 0.9):
        row = " | ".join(table[(p, r)] for r in ("R1", "R2", "R3", "R4"))
        print(f"  | {p:.1f} | {row} |".replace("0.", "0,"))
    print()
    return table


def metric_a_variant_b(runs):
    print(f"(a') то же при поиске без самого курсора (поиск, который план отвергает), "
          f"{runs} прогонов, медиана / максимум")
    for p in (0.1, 0.5, 0.9):
        for rule in ("R3", "R4"):
            times = sorted(join_run(rule, p, seed, False)[0]
                           for seed in range(runs))
            print(f"  p = {p:.1f}  {rule} B: {fmt_s(nearest_rank(times, 0.5))}"
                  f" / {fmt_s(times[-1])}")


# -- (b) the latency of a single change ------------------------------------

def frames_until(s, want, limit=400):
    """Frames until the object `want[0]` goes out with value `want[1]`."""
    i, v = want
    for m in range(1, limit + 1):
        obj, val, _ = s.next()
        s.sent()
        if obj == i and val == v:
            return m
    return math.inf


def metric_b(rules):
    print("(b) одиночное изменение: худший случай в периодах "
          "(по положениям курсоров, счётчику и объекту)")
    out = {}
    for rule in rules:
        for self_search in ((True,) if rule == "R1" else (True, False)):
            worst = 0
            worst_idle = 0
            for c, r, q, i in itertools.product(range(N), range(N),
                                                counters(rule), range(N)):
                s = Scanner(rule, self_search, c, r, q)
                s.write(i, 1)
                m = frames_until(s, (i, 1))
                worst = max(worst, m)
                if not (rule == "R4" and q != 0):
                    worst_idle = max(worst_idle, m)
            tag = "A" if self_search else "B"
            extra = ""
            if rule == "R4":
                extra = (f"; после долгого покоя (счётчик 0) — "
                         f"{worst_idle}")
            out[(rule, self_search)] = (worst, worst_idle)
            print(f"  {rule} {tag}: {worst} периода(ов){extra}")
    return out


def metric_b_busy(rules):
    print("(b') одиночное изменение, пока статус 35 меняется каждый период")
    for rule in rules:
        worst = 0
        for c, r, q, i in itertools.product(range(N), range(N),
                                            counters(rule), range(N)):
            if i == STATUS:
                continue
            s = Scanner(rule, True, c, r, q)
            v = 0
            m = math.inf
            for n in range(400):
                v ^= 1
                s.write(STATUS, v)
                if n == 0:
                    s.write(i, 1)
                obj, val, _ = s.next()
                s.sent()
                if obj == i and val == 1:
                    m = n + 1
                    break
            worst = max(worst, m)
        print(f"  {rule}: {worst} периода(ов)")


# -- (c) a burst of k changes ----------------------------------------------

def metric_c(rules, ks=(3, 10, 39), samples=400):
    print("(c) пачка из k одновременных изменений: периодов до последнего "
          "(худший случай; k·13 мс для сравнения)")
    rng = random.Random(0)
    out = {}
    for k in ks:
        if k == N:
            subsets = [tuple(range(N))]
        elif math.comb(N, k) <= 10000:
            subsets = list(itertools.combinations(range(N), k))
        else:
            subsets = {tuple(range(k)), tuple(range(N - k, N))}
            while len(subsets) < samples:
                subsets.add(tuple(sorted(rng.sample(range(N), k))))
            subsets = list(subsets)
        for rule in rules:
            if rule == "R1":
                continue
            worst = 0
            worst_idle = 0
            # The circle is symmetric: put the change cursor on the last
            # object and move everything else.
            c = N - 1
            for r, q in itertools.product(range(N), counters(rule)):
                for sub in subsets:
                    s = Scanner(rule, True, c, r, q)
                    for i in sub:
                        s.write(i, 1)
                    left = set(sub)
                    m = 0
                    while left:
                        m += 1
                        obj, val, _ = s.next()
                        s.sent()
                        if val == 1:
                            left.discard(obj)
                    worst = max(worst, m)
                    if not (rule == "R4" and q != 0):
                        worst_idle = max(worst_idle, m)
            out[(k, rule)] = (worst, worst_idle)
            extra = (f" (после долгого покоя {worst_idle})"
                     if rule == "R4" else "")
            print(f"  k = {k:2d} {rule}: {worst} периодов = "
                  f"{worst * T_MS} мс{extra}; k·13 = {k * 13} мс")
    return out


# -- (d) the example of decision 4 ------------------------------------------

def example_order(rule, phase, r, q):
    """The order in which 1Fh, 0Bh and 15h of the example go out."""
    changes = [(0.0, 30), (5.0, 10), (15.0, 20)]   # 1Fh, 0Bh, 15h
    name = {30: "1Fh", 10: "0Bh", 20: "15h"}
    s = Scanner(rule, True, 0, r, q)               # change cursor on 01h
    order = []
    n = 0
    while len(order) < 3:
        t = phase + n * T_MS
        while changes and changes[0][0] <= t:
            s.write(changes.pop(0)[1], 1)
        obj, val, src = s.next()
        s.sent()
        if val == 1 and obj in name and name[obj] not in order:
            order.append(name[obj])
        n += 1
    return " ".join(order)


def metric_d(rules):
    print("(d) пример решения 4: курсор изменений на 01h; 1Fh в t = 0, "
          "0Bh в 5 мс, 15h в 15 мс; порядок по фазе первого кадра "
          "(кадр в момент изменения его уже видит) и по счётчику; "
          "в скобках — сколько из 39 положений курсора обхода дают иной "
          "порядок")
    for rule in rules:
        if rule == "R1":
            continue
        print(f"  {rule}:")
        for q in counters(rule):
            ranges = []
            for tenth in range(100):
                phase = tenth / 10
                orders = [example_order(rule, phase, r, q) for r in range(N)]
                main_order = max(set(orders), key=orders.count)
                other = sum(1 for o in orders if o != main_order)
                others = sorted(set(o for o in orders if o != main_order))
                key = (main_order, other, tuple(others))
                if ranges and ranges[-1][2] == key:
                    ranges[-1][1] = phase
                else:
                    ranges.append([phase, phase, key])
            for lo, hi, (o, other, others) in ranges:
                tail = f" (иначе в {other}: {', '.join(others)})" if other else ""
                label = f"счётчик {q}, " if rule in ("R3", "R4") else ""
                print(f"    {label}первый кадр в [{lo:.1f}; {hi + 0.1:.1f}) мс: "
                      f"{o}{tail}")


# -- (e) a clear after a raise ---------------------------------------------

def metric_e(rules):
    print("(e) статус поднят один, ушёл, затем снят один; периодов от "
          "снятия до кадра с 0 (худший случай)")
    for rule in rules:
        if rule == "R1":
            continue
        for self_search in (True, False):
            worst = 0
            for r, q, gap in itertools.product(range(N), counters(rule),
                                               range(N + 1)):
                s = Scanner(rule, self_search, N - 1, r, q)
                x = 17
                s.write(x, 1)
                if frames_until(s, (x, 1)) == math.inf:
                    continue
                for _ in range(gap):
                    s.next()
                    s.sent()
                s.write(x, 0)
                worst = max(worst, frames_until(s, (x, 0)))
            tag = ("поиск включает сам курсор" if self_search
                   else "не включает")
            print(f"  {rule}, {tag}: {worst} периодов = {worst * T_MS} мс")


# -- (f) the bound on a full picture ----------------------------------------

def worst_join(rule, change, frames=3000, joins=range(200, 2000)):
    """The worst time to a full picture over the frame the host joins at."""
    s = Scanner(rule)
    log = []
    for n in range(frames):
        change(s, n)
        obj, _, _ = s.next()
        s.sent()
        log.append(obj)
    worst = 0
    for j in joins:
        seen = 0
        m = math.inf
        for k, obj in enumerate(log[j:], 1):
            seen |= 1 << obj
            if seen == FULL:
                m = k
                break
        worst = max(worst, m)
    return worst


def all_toggle(s, n):
    for i in range(N):
        s.write(i, n & 1)


def one_toggles(s, n):
    s.write(STATUS, n & 1)


def one_always_changed(s, n):
    # The worst a single status can do: differ from its shadow before every
    # frame.
    s.write(STATUS, 1 - s.shadow[STATUS])


def all_always_changed(s, n):
    for i in range(N):
        s.write(i, 1 - s.shadow[i])


def metric_f(rules):
    print("(f) худшее время до полной картины по моменту подключения "
          "(предел правила — 39 кадров обхода по одному на 4 кадра, "
          "156 периодов)")
    for label, change in (
            ("все 39 меняются каждый период", all_toggle),
            ("все 39 перед каждым кадром отличаются от тени",
             all_always_changed),
            ("статус 35 меняется каждый период", one_toggles),
            ("статус 35 перед каждым кадром отличается от тени",
             one_always_changed)):
        for rule in rules:
            w = worst_join(rule, change)
            text = (f"{w} периодов = {fmt_s(w)}" if w != math.inf
                    else "картина не собирается никогда")
            print(f"  {label}, {rule}: {text}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=1000)
    ap.add_argument("--only", default="abcdef")
    args = ap.parse_args()
    rules = ("R1", "R2", "R3", "R4")
    if "a" in args.only:
        metric_a(args.runs, rules)
        metric_a_variant_b(min(args.runs, 200))
    if "b" in args.only:
        metric_b(rules)
        metric_b_busy(("R2", "R3", "R4"))
    if "c" in args.only:
        metric_c(rules)
    if "d" in args.only:
        metric_d(rules)
    if "e" in args.only:
        metric_e(rules)
    if "f" in args.only:
        metric_f(("R2", "R3", "R4"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
