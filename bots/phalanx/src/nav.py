"""Movement: one bounded distance-field search per call, stepping to the best reachable tile."""
import cambc
from cambc import Vec2

DIRS = (Vec2.NORTH, Vec2.NORTH_EAST, Vec2.EAST, Vec2.SOUTH_EAST,
        Vec2.SOUTH, Vec2.SOUTH_WEST, Vec2.WEST, Vec2.NORTH_WEST)


def ring(center, radius):
    b = cambc.Bitboard()
    b.draw_chebyshev(center, radius)
    return b


# Tiles where a move recently failed (hidden champions in bushes): tile -> round it expires.
_bumped = {}


def blocked_now(world, me_pos):
    b = world.block | cambc.vision.champion_tiles() | cambc.vision.camp_tiles() | cambc.vision.teleport_tiles()
    if _bumped:
        r = cambc.round_number()
        for t, until in list(_bumped.items()):
            if until < r:
                del _bumped[t]
            else:
                b.set(t, True)
    b.set(me_pos, False)
    return b


def _try_moves(me, candidates):
    """Attempt moves in order; a failed move marks the tile as occupied for a few rounds."""
    for p in candidates[:3]:
        try:
            if me.move_absolute(p):
                return True
        except cambc.Error:
            pass
        _bumped[p] = cambc.round_number() + 3
    return False


_stuck = [0]
_recent = []  # tiles left in the last few moves


def step_towards(world, me, goals, avoid=None):
    """Step towards the nearest goal tile. Returns True if the champion moved.

    Routes avoid visible units and every camp believed alive (so routes do not flip as camps go in
    and out of sight), never step back onto a recently left tile (groups otherwise circle each other
    in corridors), and fall back to planning through units, then through camps."""
    if not me.can_move():
        return False
    origin = me.position()
    if goals.is_set(origin):
        _stuck[0] = 0
        return False
    blocked = blocked_now(world, origin)
    start = cambc.Bitboard()
    start.set(origin)
    walls = (blocked | world.camp_wall(cambc.round_number())) & ~start
    candidates = None
    for attempt in range(3):
        if attempt == 1:
            walls = (world.block | world.camp_wall(cambc.round_number()) | cambc.vision.camp_tiles()) & ~start
        elif attempt == 2:
            walls = world.block & ~start
        g = goals & ~walls
        if g.is_empty():
            continue
        field = cambc.DistanceField()
        field.compute_to(walls, g, start, cambc.Bitboard())
        here = field.get(origin)
        if here == 0 or here >= 0xFFFF:
            continue
        options = []
        for p in blocked.reachable_tiles(origin, me.base_stats().base_move_speed).positions():
            if p == origin or p in _recent or (avoid is not None and avoid.is_set(p)):
                continue
            d = field.get(p)
            if d < here:
                options.append((d, p.chebyshev_distance(origin), p))
        if options:
            candidates = options
            break
    if not candidates:
        _stuck[0] += 1
        if _stuck[0] >= 3:
            _recent.clear()
        return False
    candidates.sort(key=lambda t: (t[0], t[1]))
    moved = _try_moves(me, [t[2] for t in candidates])
    _stuck[0] = 0 if moved else _stuck[0] + 1
    if moved:
        _recent.append(origin)
        if len(_recent) > 3:
            _recent.pop(0)
    return moved


def step_away(world, me, threats):
    """Step to the adjacent tile that maximises distance from the nearest threat, preferring our spawn."""
    if not me.can_move() or not threats:
        return False
    origin = me.position()
    blocked = blocked_now(world, origin)
    here = min(origin.chebyshev_distance(t) for t in threats) * 4 - world.home_dist(origin)
    options = []
    for p in blocked.reachable_tiles(origin, me.base_stats().base_move_speed).positions():
        if p == origin:
            continue
        dmin = min(p.chebyshev_distance(t) for t in threats)
        score = dmin * 4 - world.home_dist(p)
        if score > here:
            options.append((score, p))
    if not options:
        return False
    options.sort(key=lambda t: -t[0])
    return _try_moves(me, [t[1] for t in options])
