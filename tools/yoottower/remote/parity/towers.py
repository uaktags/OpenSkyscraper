"""towers.py [OUTDIR] : write the parity campaign's test towers (default tests/parity/).

Each tower is an OpenSky YootTower .tower that BldTool export can move into the
original game: Simple Elevators only (the exporter drops other movers), a floor
slab under every row an item covers, items the world and grade allow (ATTR
mAttrib / mGameLevel). Footprints are the plugins' TNDF width x height.
"""
import os
import sys

# TNDF footprints (width, height) of the prototypes used here.
SIZE = {
    'office': (9, 1), 'condo': (16, 1), 'restroom': (9, 1), 'security': (16, 1),
    'hotel_single': (4, 1), 'hotel_double': (6, 1), 'hotel_suite': (16, 1),
    'frontdesk': (15, 1), 'housekeeping': (15, 1), 'doctor': (26, 1),
    'trashfacility': (25, 2), 'powerroom': (23, 2), 'parkinglot': (4, 1), 'parkinggate': (16, 2),
    'theater': (31, 3), 'classictheater': (24, 2), 'eventcenter': (31, 2),
    'subway': (40, 3), 'boatdeck': (20, 2), 'school': (38, 2),
    'yt_barber': (12, 1), 'yt_bookstore': (15, 1), 'yt_burger': (16, 1), 'yt_cigar': (3, 1),
    'yt_coffee': (12, 1), 'yt_drink': (3, 1), 'yt_flower': (12, 1), 'yt_icecream': (12, 1),
    'yt_ramen': (12, 1), 'yt_record': (20, 1), 'yt_electric': (20, 1), 'yt_convenience': (20, 1),
    'yt_steak': (16, 1), 'yt_jennys': (30, 1), 'yt_mexcan': (24, 1), 'yt_aloha': (15, 1),
    'yt_surfshop': (13, 1), 'yt_seafood': (24, 1), 'yt_cocktail': (16, 1), 'yt_dutyfree': (30, 2),
    'yt_orangejulius': (16, 1),
}
# ToolDef consumption power (first variation), for the blackout tower.
POWER = {
    'office': 4, 'condo': 4, 'restroom': 1, 'security': 20, 'hotel_single': 3, 'hotel_double': 5,
    'hotel_suite': 8, 'frontdesk': 8, 'housekeeping': 12, 'doctor': 30, 'trashfacility': 20,
    'powerroom': 0, 'parkinglot': 8, 'parkinggate': 8, 'theater': 200, 'classictheater': 30,
    'eventcenter': 50, 'subway': 300, 'boatdeck': 20, 'school': 100, 'elevator-standard': 20,
    'yt_cigar': 1, 'yt_drink': 1, 'yt_record': 12, 'yt_electric': 16, 'yt_convenience': 12,
    'yt_jennys': 14, 'yt_mexcan': 12, 'yt_steak': 10, 'yt_seafood': 12,
}
EXTRA = {
    'office': ' evaluation="100.000" yootEstimate="300" occupied="1" population="0"',
    'classictheater': ' movie="MV_Cop.t2p"',
    'eventcenter': ' event="PlArt.t2p" startDay="0" span="3" today="PlArt.t2p"',
}


class Tower:
    def __init__(self, world, rating, funds=7877000, vip=0, time=0.020833):
        self.world, self.rating, self.funds, self.vip, self.time = world, rating, funds, vip, time
        self.slabs = {}      # y -> (x0, x1)
        self.items = []      # (type, x, y, extra)
        self.lifts = []      # (x, y0, height, cars, kind)
        self.lobby = None

    def floor(self, y, x0, x1):
        a, b = self.slabs.get(y, (x0, x1))
        self.slabs[y] = (min(a, x0), max(b, x1))

    def floors(self, ys, x0, x1):
        for y in ys:
            self.floor(y, x0, x1)

    def set_lobby(self, x0, x1, height):
        self.lobby = (x0, x1, height)

    def add(self, typ, x, y, extra=''):
        w, h = SIZE[typ]
        for r in range(y, y + h):
            if not (self.lobby and r < self.lobby[2] and r >= 0):
                lo, hi = self.slabs.get(r, (x, x + w))
                assert lo <= x and x + w <= hi, '%s at %d,%d is off the slab %r' % (typ, x, r, self.slabs.get(r))
        self.items.append((typ, x, y, extra or EXTRA.get(typ, '')))
        return x + w

    def row(self, y, x, types):
        for t in types:
            x = self.add(t, x, y)
        return x

    def lift(self, x, y0, height, cars=4, kind='elevator-standard'):
        self.lifts.append((x, y0, height, cars, kind))

    def power(self):
        return sum(POWER.get(t, 8) for t, _, _, _ in self.items) + 20 * len(self.lifts)

    def check(self):
        cells = {}
        for t, x, y, _ in self.items:
            w, h = SIZE[t]
            for r in range(y, y + h):
                for u in range(x, x + w):
                    assert (u, r) not in cells, '%s overlaps %s at %d,%d' % (t, cells[(u, r)], u, r)
                    cells[(u, r)] = t
        for x, y0, height, _, _ in self.lifts:
            for r in range(y0, y0 + height):
                for u in range(x, x + 4):
                    assert (u, r) not in cells, 'elevator at %d overlaps %s at %d,%d' % (x, cells[(u, r)], u, r)

    def xml(self):
        self.check()
        out = ['<tower funds="%d" rating="%d" time="%.6f" speed="1" rainy="0" tool="inspector" '
               'ruleset="yoottower" world="%s" yootVipFlags="%d" x="-112" y="180">' % (
                   self.funds, self.rating, self.time, self.world, self.vip),
               '<money todayIncome="0" todayExpenses="0" yesterdayIncome="0" yesterdayExpenses="0" '
               'lastQuarterBalance="%d" quarterIncome="0" quarterExpenses="0"/>' % self.funds]
        x0, x1, h = self.lobby
        out.append('<item type="lobby" x="%d" y="0" evaluation="50.0" width="%d" height="%d"/>' % (x0, x1 - x0, h))
        for y in sorted(self.slabs):
            a, b = self.slabs[y]
            out.append('<item type="floor" x="%d" y="%d" evaluation="50.0" width="%d"/>' % (a, y, b - a))
        for t, x, y, extra in self.items:
            ev = '' if 'evaluation=' in extra else ' evaluation="50.0"'
            st = ' tenantStatus="9"' if t.startswith('yt_') else ''
            out.append('<item type="%s" x="%d" y="%d"%s%s%s/>' % (t, x, y, ev, extra, st))
        for x, y0, height, cars, kind in self.lifts:
            stops = []
            for f in range(y0, y0 + height):
                lobby_row = 0 < f < h
                slab = self.slabs.get(f)
                served = f == 0 or (not lobby_row and slab and slab[0] <= x and x + 4 <= slab[1])
                if not served:
                    stops.append('<unserviced floor="%d"/>' % f)
            span = [f for f in range(y0, y0 + height) if not (0 < f < h)]
            car = ''.join('<car altitude="%d.0" homeFloor="%d"/>' % (span[k * len(span) // cars], span[k * len(span) // cars])
                          for k in range(cars))
            out.append('<item type="%s" x="%d" y="%d" evaluation="50.0" height="%d" showShaft="1">%s%s</item>'
                       % (kind, x, y0, height, ''.join(stops), car))
        out.append('</tower>')
        return '\n'.join(out) + '\n'


def offices(t, y, x, n):
    return t.row(y, x, ['office'] * n)


def tokyo_g2_hotel_subway():
    """Tokyo, 2 stars: offices, shops, a hotel with Front Desk and Housekeeping, a Security
    Room and the subway on the lowest floor."""
    t = Tower('tokyo', 1)
    t.set_lobby(-64, 56, 3)
    t.floors(range(-3, 0), -64, 56)
    t.floors(range(3, 13), -64, 56)
    t.lift(-4, -3, 15, 6)
    t.lift(20, 0, 13, 4)
    t.add('subway', -64, -3)
    t.row(-3, -24, ['restroom', 'yt_drink', 'yt_cigar']); t.row(-3, 0, ['yt_coffee']); t.row(-3, 24, ['yt_burger', 'yt_ramen'])
    t.row(-2, -24, ['yt_icecream']); t.row(-2, 0, ['yt_flower']); t.row(-2, 24, ['yt_bookstore', 'yt_barber'])
    t.row(-1, 24, ['yt_record'])
    t.row(3, -64, ['yt_electric', 'yt_convenience', 'restroom', 'yt_cigar']); t.row(3, 0, ['yt_coffee'])
    t.row(3, 24, ['yt_burger', 'yt_icecream'])
    for y in (4, 5, 6, 11):
        offices(t, y, -64, 6); offices(t, y, 0, 2); offices(t, y, 24, 3)
    t.row(7, -64, ['security', 'frontdesk', 'housekeeping', 'restroom']); t.row(7, 0, ['yt_flower'])
    t.row(7, 24, ['yt_steak', 'yt_barber'])
    for y in (8, 9):
        t.row(y, -64, ['hotel_suite', 'hotel_suite'] + ['hotel_double'] * 4)
        t.row(y, 0, ['hotel_single'] * 5)
        t.row(y, 24, ['hotel_double'] * 5)
    t.row(10, -64, ['yt_jennys', 'yt_mexcan']); t.row(10, 0, ['yt_bookstore']); t.row(10, 24, ['yt_record'])
    offices(t, 12, 24, 3)
    return t


def tokyo_g3_venues(time=0.020833, security=False):
    """Tokyo, 3 stars: Movie Theater, Event Center, Classic Theater, Doctor's Office, Trash
    Facility, Power Room, restrooms, parking (gate + lots), offices and shops."""
    t = Tower('tokyo', 2, time=time)
    t.set_lobby(-64, 56, 3)
    t.floor(-1, -108, 56)
    t.floor(-2, -64, 56)
    t.floors(range(3, 13), -64, 56)
    t.lift(-4, -2, 15, 6)
    t.lift(20, 0, 13, 4)
    t.add('parkinggate', -80, -1)
    for x in range(-84, -108, -4):
        t.add('parkinglot', x, -1)
    for x in range(-64, -44, 4):
        t.add('parkinglot', x, -1)
    t.add('trashfacility', -30, -2)
    t.add('powerroom', 0, -2)
    t.row(-2, 24, ['yt_burger', 'restroom'])
    t.add('theater', -64, 3)
    for y in (3, 4, 5):
        offices(t, y, -33, 3)
    t.add('eventcenter', -64, 6)
    t.row(6, -33, ['restroom', 'yt_coffee']); t.row(7, -33, ['yt_cigar', 'yt_burger'])
    t.add('classictheater', -64, 8)
    for y in (8, 9):
        offices(t, y, -40, 4)
    t.row(10, -64, ['doctor', 'restroom', 'yt_icecream', 'yt_cigar'])
    t.row(11, -64, ['yt_bookstore', 'yt_flower', 'yt_barber', 'restroom'])
    if security:
        t.add('security', -64, 12); offices(t, 12, -46, 4)
    else:
        offices(t, 12, -64, 6)
    for y in range(3, 13):
        offices(t, y, 0, 2)
        if y in (6, 10):
            t.row(y, 24, ['yt_ramen', 'restroom'])
        else:
            offices(t, y, 24, 3)
    return t


def tokyo_g3_reggae():
    """tokyo_g3_venues with a Security Room, starting on day 12 (Y1 Dec.), the day the 3-star
    MAEV events (Mr. Maru every 20 days, Uncle Reggae every 25) and Santa begin."""
    return tokyo_g3_venues(time=11.020833, security=True)


def tokyo_g5_large_elevator():
    """Tokyo, 4 stars: one tall Large Elevator (elevator-express, BigElev) on the lobby, with
    offices on six floors 6 rows apart (3..33). Seven served stops (lobby + six office floors),
    under the Large Elevator's cap of 8. Shops on the lobby row (visitor demand: an office-only
    tower never sends anyone to a mover in either game)."""
    t = Tower('tokyo', 3)
    t.set_lobby(-64, 56, 3)
    t.floors([3, 9, 15, 21, 27, 33], -140, 140)
    t.lift(-4, 0, 34, 8, kind='elevator-express')
    t.row(0, -60, ['yt_coffee', 'yt_burger', 'yt_flower'])
    for y in (3, 9, 15, 21, 27, 33):
        offices(t, y, -120, 4)
    return t


def tokyo_g4_blackout():
    """Tokyo, 4 stars, no Power Room: schools, the subway, theaters and the Event Center draw
    just over the default 5000 supply, so the 11:00 check blacks the tower out."""
    t = Tower('tokyo', 3)
    t.set_lobby(-64, 56, 3)
    t.floors(range(-3, 0), -64, 60)
    t.floors(range(3, 20), -140, 140)
    t.lift(-4, -3, 4, 2)
    t.add('subway', -64, -3)
    t.row(-3, -24, ['restroom']); t.row(-3, 0, ['yt_coffee', 'yt_burger', 'yt_flower'])
    for y in (-2, -1):
        offices(t, y, 0, 6)
    t.add('theater', -140, 3); t.add('eventcenter', -100, 3); t.add('classictheater', 40, 3)
    for y in range(6, 20, 2):
        for k in range(7 if y < 18 else 3):
            t.add('school', -133 + 38 * k, y)
    t.row(3, -60, ['yt_record', 'yt_electric']); t.row(4, -60, ['yt_convenience'])
    return t


def hawaii_hotel(t, y, x):
    return t.row(y, x, ['frontdesk', 'housekeeping'])


def hawaii_g2_resort():
    """Hawaii, 2 stars (its start grade): Hawaiian condos, a hotel with Front Desk and
    Housekeeping, shops and restaurants; big enough for the 2-star VIP (a Suite) and the
    500-population grade-up."""
    t = Tower('hawaii', 1, funds=5000000)
    t.set_lobby(-60, 76, 2)
    t.floors(range(2, 14), -60, 76)
    t.lift(-4, 0, 14, 6)
    t.lift(40, 0, 14, 4)
    t.row(2, -60, ['yt_aloha', 'yt_surfshop', 'yt_burger']); t.row(2, 0, ['yt_coffee', 'yt_icecream', 'restroom'])
    t.row(2, 44, ['yt_flower', 'yt_cigar', 'yt_drink', 'yt_orangejulius'][:3])
    hawaii_hotel(t, 3, -60); t.row(3, 0, ['yt_seafood', 'yt_drink']); t.row(3, 44, ['yt_steak', 'restroom'])
    for y in (4, 5):
        t.row(y, -60, ['hotel_suite', 'hotel_double', 'hotel_double', 'hotel_double', 'hotel_double'][:4] + ['hotel_double'])
        t.row(y, 0, ['hotel_double'] * 6)
        t.row(y, 44, ['hotel_suite', 'hotel_double', 'hotel_double'])
    for y in range(6, 12):
        t.row(y, -60, ['condo', 'condo', 'condo'][:3])
        t.row(y, 0, ['condo', 'condo'])
        t.row(y, 44, ['condo', 'condo'])
    t.row(12, -60, ['yt_jennys', 'yt_record']); t.row(12, 0, ['yt_burger', 'yt_coffee']); t.row(12, 44, ['yt_aloha', 'restroom'])
    t.row(13, -60, ['yt_seafood', 'yt_surfshop']); t.row(13, 0, ['yt_steak', 'restroom']); t.row(13, 44, ['yt_icecream', 'yt_flower'])
    return t


def hawaii_g3_boatdeck():
    """Hawaii, 3 stars: the Boat Deck at the left edge of B2 (the cruise), parking, Trash
    Facility, Power Room, Doctor's Office, a theater, the hotel and condos."""
    t = Tower('hawaii', 2, funds=5000000)
    t.set_lobby(-40, 60, 2)
    t.floor(-2, -144, 60)
    t.floor(-1, -144, 60)
    t.floors(range(2, 13), -40, 60)
    t.lift(10, -2, 15, 6)
    t.add('boatdeck', -144, -2)
    t.row(-2, -124, ['yt_burger', 'yt_coffee', 'restroom', 'yt_icecream', 'yt_flower'])
    t.add('parkinggate', -60, -1)
    for x in range(-64, -124, -4):
        t.add('parkinglot', x, -1)
    t.add('parkinglot', -44, -1)
    t.add('trashfacility', -36, -2)
    t.add('powerroom', 14, -2)
    t.row(-2, 37, ['yt_record'])
    t.add('theater', -40, 2)
    t.row(2, -9, ['yt_cocktail']); t.row(2, 14, ['doctor', 'restroom'])
    t.add('yt_dutyfree', 14, 3)
    t.row(3, -9, ['yt_seafood'][:0] + ['restroom'])
    t.row(4, -9, ['yt_drink', 'yt_cigar'])
    t.row(5, -40, ['frontdesk', 'housekeeping', 'hotel_suite']); t.row(5, 14, ['hotel_double'] * 7)
    t.row(6, -40, ['hotel_suite', 'hotel_suite'] + ['hotel_double']); t.row(6, 14, ['hotel_double'] * 7)
    for y in range(7, 12):
        t.row(y, -40, ['condo', 'condo', 'yt_drink']); t.row(y, 14, ['condo', 'condo'])
    t.row(12, -40, ['yt_aloha', 'yt_surfshop', 'yt_cigar']); t.row(12, 14, ['yt_steak', 'yt_coffee'])
    return t


def tokyo_g1_promo():
    """Tokyo, 1 star, no VIP flag yet: a Record store (the 1-star VIP's destination) and
    enough offices and shops to pass 500 (VIP visit) and 1000 (grade-up) people."""
    t = Tower('tokyo', 0)
    t.set_lobby(-64, 56, 3)
    t.floors(range(3, 15), -64, 56)
    t.lift(-4, 0, 15, 6)
    t.lift(20, 0, 15, 6)
    t.row(3, -64, ['yt_record', 'yt_burger', 'yt_coffee', 'restroom']); t.row(3, 0, ['yt_icecream'])
    t.row(3, 24, ['yt_bookstore', 'yt_barber'])
    for y in range(4, 15):
        if y in (7, 10, 13):
            t.row(y, -64, ['yt_burger', 'yt_coffee', 'yt_flower', 'yt_cigar', 'yt_drink'])
            t.row(y, 0, ['yt_ramen'])
            t.row(y, 24, ['yt_bookstore', 'yt_icecream'])
        else:
            offices(t, y, -64, 6); offices(t, y, 0, 2); offices(t, y, 24, 3)
    return t


TOWERS = [tokyo_g1_promo, tokyo_g2_hotel_subway, tokyo_g3_venues, tokyo_g3_reggae, tokyo_g4_blackout, tokyo_g5_large_elevator,
          hawaii_g2_resort, hawaii_g3_boatdeck]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(os.path.join(here, '..', '..', '..', '..', 'tests', 'parity'))
    os.makedirs(out, exist_ok=True)
    for fn in TOWERS:
        t = fn()
        path = os.path.join(out, fn.__name__ + '.tower')
        open(path, 'w', newline='\n').write(t.xml())
        print('%-24s %3d items  power %5d  %s' % (fn.__name__, len(t.items), t.power(), path))


if __name__ == '__main__':
    main()
