#!/usr/bin/env python3
# Copyright (c) 2026 OpenSkyscraper Project
"""Generate strategy-guide .tower saves (1..5 stars) for OpenSkyscraper.

Encodes the classic SimTower strategy into our native XML save format:
  - offices share the standard elevators (every built floor gets a stop)
  - stair/escalator stacks spread across the built width (<= ~1/2 screen apart)
  - fast food one floor below each sky lobby, escalators up to the lobby
  - 7 condo floors per 15-floor section carry the finances (premium pricing)
  - standard elevators serve every floor of their span (no dead-floor skips)
  - service elevators stop only at ground + hotel floors (housekeeping staff)
  - express elevators stop only at sky lobbies (+ parking on B7 from 4 stars)
  - parking ONLY on B7; metro station at B8..B10 (5 stars)
  - top section (90+) is condos only, cathedral crowns floor 97..100 (5 stars)

Coordinates: x in tiles (lot -187..188), y in floors (0 = ground, negative = B).
Item footprints mirror source/Item/*.h prototypes. Elevator car homes sit on
served floors only.
"""

import os

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "towers")


class Tower:
    def __init__(self, name, rating, funds, lo, hi, sections):
        self.name = name
        self.rating = rating          # 0 = 1 star .. 4 = 5 stars
        self.funds = funds
        self.lo, self.hi = lo, hi     # built width in tiles (hi exclusive)
        self.sections = sections      # number of 15-floor sections
        self.items = []
        # Transport geometry
        self.n_express = 4 if rating >= 2 else (2 if sections >= 2 else 0)
        self.express_x = [lo + 6 * i for i in range(self.n_express)]
        self.right_margin = 20 if rating >= 1 else 12
        self.std_x = [hi - 20, hi - 16, hi - 12]   # 3 shafts; section 0 uses 2
        self.svc_x = hi - 4                        # services stack in one column (disjoint spans)
        self.hotels_per_section = 3 if rating >= 4 else 2
        n_stacks = 2 if hi - lo <= 160 else (5 if hi - lo <= 240 else 8)
        ulo = lo + (6 * self.n_express if self.n_express else 0)
        uhi = hi - self.right_margin - 8
        span = uhi - ulo
        self.stack_x = [ulo + round(i * span / (n_stacks - 1)) for i in range(n_stacks)]
        self.stair_columns = [(x, x + 8) for x in self.stack_x]
        # Stats
        self.counts = {}

    # -- primitives ---------------------------------------------------------
    def add(self, xml):
        self.items.append(xml)
        key = xml.split('type="')[1].split('"')[0]
        self.counts[key] = self.counts.get(key, 0) + 1

    def slab(self, y):
        self.add(f'<item type="floor" x="{self.lo}" y="{y}" evaluation="90.0" width="{self.hi - self.lo}"/>')

    def lobby(self, y):
        self.add(f'<item type="lobby" x="{self.lo}" y="{y}" evaluation="90.0" width="{self.hi - self.lo}" height="1"/>')

    def margins(self):
        """Shaft footprints piercing room floors (left express, right std/svc)."""
        out = []
        if self.n_express:
            out.append((self.lo, self.lo + 6 * self.n_express))
        right = 20 if self.rating >= 2 else self.right_margin
        out.append((self.hi - right, self.hi))
        return [(a, b) for a, b in out if b > self.lo and a < self.hi]

    def row(self, y, kind, blocked):
        w = {"office": 9, "condo": 16, "fastfood": 16, "restaurant": 24,
             "hotel_double": 6, "hotel_suite": 7, "parking": 8}[kind]
        x = self.lo
        i = 0
        while x + w <= self.hi:
            if any(x < b1 and x + w > b0 for b0, b1 in blocked):
                x += 1
                continue
            attrs = f'x="{x}" y="{y}" evaluation="90.0"'
            if kind == "office":
                attrs += ' rent="10000" rentDeposit="10000"'
            elif kind == "condo":
                attrs += ' rent="8000" rentDeposit="8000"'  # premium resale, per the guide
            elif kind == "fastfood":
                attrs += f' variant="{i % 5}" open="1"'
            elif kind == "restaurant":
                attrs += f' variant="{i % 4}" open="1"'
            self.add(f'<item type="{kind}" {attrs}/>')
            i += 1
            x += w

    def elevator(self, kind, x, bottom, top, stops, cars):
        height = top - bottom + 1
        xml = (f'<item type="{kind}" x="{x}" y="{bottom}" evaluation="90.0" '
               f'height="{height}" showShaft="1">')
        for f in range(bottom, top + 1):
            if f not in stops:
                xml += f'<unserviced floor="{f}"/>'
        ordered = sorted(stops)
        for c in range(cars):
            home = ordered[c % len(ordered)]  # homes sit on SERVED floors only
            xml += f'<car altitude="{home}.0" homeFloor="{home}"/>'
        xml += "</item>"
        self.add(xml)

    # -- layout -------------------------------------------------------------
    def build(self):
        seven = self.sections

        for k in range(seven):
            sky = 15 * k
            self.slab(sky)
            self.lobby(sky)

            # Top section of a 5-star tower: condos only above floor 90.
            top_condo_section = (k == seven - 1 and self.rating >= 4 and seven >= 3)
            offices_top = sky if top_condo_section else sky + 3
            condos = range(sky + 1, sky + 7) if top_condo_section else range(sky + 4, sky + 11)
            hotels = [] if top_condo_section else list(range(sky + 11, sky + 11 + self.hotels_per_section))
            ff = sky - 1

            # Offices: stairs up from the lobby; elevators never stop here.
            for f in range(sky + 1, offices_top + 1):
                self.slab(f)
                blocked = self.margins() + self.stair_columns
                if self.rating >= 2 and k % 2 == 0 and f == sky + 1:
                    sx0 = self._gap(blocked, 16)
                    self.add(f'<item type="security" x="{sx0}" y="{f}" evaluation="90.0"/>')
                    blocked.append((sx0, sx0 + 16))
                if self.rating >= 3 and k == 0 and f == sky + 3:
                    mx = self._gap(blocked, 32)
                    self.add(f'<item type="medicalcenter" x="{mx}" y="{f}" evaluation="90.0"/>')
                    blocked.append((mx, mx + 32))
                self.row(f, "office", blocked)
            if offices_top > sky:
                for sx in self.stack_x:
                    for fl in range(sky, offices_top):
                        self.add(f'<item type="stairs" x="{sx}" y="{fl}" evaluation="90.0"/>')

            # Condos (the moneymaker) + hotels.
            for f in condos:
                self.slab(f)
                blocked = self.margins()
                if top_condo_section and self.rating >= 4 and seven >= 7:
                    blocked.append((-24, 24))  # cathedral above takes 97..100
                self.row(f, "condo", blocked)
            for i, f in enumerate(hotels):
                self.slab(f)
                kind = "hotel_suite" if (self.rating >= 3 and i == 0) else "hotel_double"
                self.row(f, kind, self.margins())

            # Fast food below the lobby; escalators up to it.
            self.slab(ff)
            self.row(ff, "fastfood", self.margins() + self.stair_columns)
            for sx in self.stack_x:
                self.add(f'<item type="escalator" x="{sx}" y="{ff}" evaluation="90.0"/>')

        # Cinema above the ground hotel block (floors 13..14, empty otherwise).
        if self.rating >= 2:
            self.slab(13)
            self.slab(14)
            self.add(f'<item type="cinema" x="{(self.lo + self.hi) // 2 - 15}" y="13" evaluation="90.0"/>')

        # Parking only on B7; express is the only way down (guide).
        if self.rating >= 3:
            self.slab(-7)
            self.row(-7, "parking", [(self.lo, self.lo + 6 * self.n_express)])

        # Metro station B8..B10, escalators up to parking (5 stars).
        if self.rating >= 4:
            self.add('<item type="metro" x="-15" y="-10" evaluation="90.0"/>')
            self.escalator_pair(-8)

        # Cathedral crowns floors 97..100, centered (5 stars with 7 sections).
        if self.rating >= 4 and seven >= 7:
            self.add('<item type="cathedral" x="-24" y="97" evaluation="90.0"/>')

        # ---- Express elevators: parking B7 + every sky lobby only ----
        if self.n_express:
            bottom = -7 if self.rating >= 3 else 0
            top = 15 * (seven - 1)
            stops = {bottom} | {15 * k for k in range(seven)}
            for x in self.express_x:
                self.elevator("elevator-express", x, bottom, top, stops, cars=6)

        # ---- Standard elevators: every floor of the span ----
        # Office floors are occupied, not dead, so they get stops too
        # (Tim's rule: elevators serve every floor the shaft passes that
        # is not dead). Section 0 uses 2 shafts (a service elevator takes
        # the third slot),
        # matching the guide's "all 24 shafts used" arithmetic at 5 stars.
        for k in range(seven):
            sky = 15 * k
            if k == seven - 1 and self.rating >= 4 and seven >= 3:
                top = sky + 9
                stops = {sky} | set(condos_top(self.rating, sky, seven))
            else:
                top = sky + 10 + (3 if self.rating >= 4 else 2)
                stops = set(range(sky, top + 1))
            n = 2 if k == 0 else 3
            for i in range(n):
                self.elevator("elevator-standard", self.std_x[i], sky, top, stops, cars=2)

        # ---- Service elevators: ground/sky lobby + hotel floors (staff) ----
        if self.rating >= 1:
            h = self.hotels_per_section
            stops = {0, 15 * min(1, seven - 1)} | self.hotels(0) | (self.hotels(1) if seven >= 2 else set())
            top = 15 * min(1, seven - 1) + 10 + h
            self.elevator("elevator-service", self.svc_x, 0, top, stops, cars=1)
        if self.rating >= 3 and seven >= 4:
            top = 45 + 10 + self.hotels_per_section
            stops = {30, 45} | self.hotels(2) | self.hotels(3)
            self.elevator("elevator-service", self.svc_x, 30, top, stops, cars=1)
            if self.rating >= 4:
                stops = {60, 75} | self.hotels(4) | self.hotels(5)
                self.elevator("elevator-service", self.svc_x, 60, 88, stops, cars=1)
            else:
                stops = {60} | self.hotels(4)
                self.elevator("elevator-service", self.svc_x, 60, 60 + 10 + self.hotels_per_section, stops, cars=1)

    def hotels(self, k):
        return set(range(15 * k + 11, 15 * k + 11 + self.hotels_per_section))

    def escalator_pair(self, y):
        for sx in (self.stack_x[0], self.stack_x[-1]):
            self.add(f'<item type="escalator" x="{sx}" y="{y}" evaluation="90.0"/>')

    def _gap(self, blocked, width):
        x = self.lo
        while x + width <= self.hi:
            if not any(x < b1 and x + width > b0 for b0, b1 in blocked):
                return x
            x += 1
        return self.lo


def condos_top(rating, sky, seven):
    return range(sky + 1, sky + 7)


def estimate_population(t):
    """Rough sustainable population: workers + residents + hotel guests."""
    pop = 0
    for kind, n in t.counts.items():
        if kind == "office":
            pop += 6 * n
        elif kind == "condo":
            pop += 3 * n
        elif kind == "hotel_double":
            pop += 2 * n
        elif kind == "hotel_suite":
            pop += 3 * n
    return pop


def emit(t):
    head = (f'<tower funds="{t.funds}" rating="{t.rating}" time="0.115385" speed="1" '
            f'rainy="0" tool="inspector" x="0" y="200">')
    return head + "\n" + "\n".join(t.items) + "\n</tower>\n"


TIERS = [
    # name    rating  funds      width          sections
    ("1star", 0, 2_000_000, (-40, 41), 1),
    ("2star", 1, 4_000_000, (-80, 80), 1),
    ("3star", 2, 10_000_000, (-120, 120), 2),
    ("4star", 3, 25_000_000, (-187, 188), 5),
    ("5star", 4, 50_000_000, (-187, 188), 7),
]


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, rating, funds, width, sections in TIERS:
        t = Tower(name, rating, funds, width[0], width[1], sections)
        t.build()
        xml = emit(t)
        path = os.path.join(OUT_DIR, f"{name}.tower")
        with open(path, "w") as f:
            f.write(xml)
        pop = estimate_population(t)
        print(f"{path}: {len(xml):7d} bytes, {sum(t.counts.values()):5d} items, "
              f"est. pop ~{pop:5d}, rating {rating} ({rating + 1}-star)")


if __name__ == "__main__":
    main()
