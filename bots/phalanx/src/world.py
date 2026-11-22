"""Static map knowledge, built once per champion on its first turn of play."""
import cambc
from cambc import game_map, Team, Vec2

CT = cambc.CampType
ATTACKING = (CT.WOLF, CT.ELK, CT.MONOLITH, CT.GOLEM)
GOLD_WEIGHT = 0.3  # value of one gold relative to one gem when choosing camps


def centroid(points):
    n = len(points)
    if not n:
        return Vec2(0, 0)
    return Vec2(sum(p.x for p in points) // n, sum(p.y for p in points) // n)


class GroupInfo:
    __slots__ = ("idx", "group", "camps", "spawns", "tiles", "center", "hp", "dps", "gold", "gems",
                 "respawn", "has_monolith", "ally_dist", "enemy_dist", "value", "base_cost", "attackers", "heavy")

    def __init__(self, idx, group, ally_c, enemy_c):
        self.idx = idx
        self.group = group
        self.camps = list(group.camps())
        self.spawns = [c.spawn_position() for c in self.camps]
        self.tiles = group.spawn_tiles()
        self.center = centroid(self.spawns)
        self.hp = 0.0
        self.dps = 0.0
        self.has_monolith = False
        self.attackers = 0
        for c in self.camps:
            if c.kind() in ATTACKING:
                self.attackers += 1
            st = cambc._bc.camp_design(int(c.kind()))
            self.hp += st.base_health
            self.dps += st.base_strength
            if c.kind() == CT.MONOLITH:
                self.has_monolith = True
        # Three or more hard-hitting camps: they wake together, so pull them one at a time.
        self.heavy = self.attackers >= 3 and self.dps >= 150
        self.gold = group.gold_value()
        self.gems = group.gem_value()
        self.respawn = group.max_respawn_timer()
        self.ally_dist = self.center.chebyshev_distance(ally_c)
        self.enemy_dist = self.center.chebyshev_distance(enemy_c)
        self.value = self.gems + GOLD_WEIGHT * self.gold
        self.base_cost = cambc.constants.CAPTURE_TIME + 2 + (6 if self.enemy_dist < self.ally_dist else 0)


class World:
    def __init__(self):
        self.W = game_map.width()
        self.H = game_map.height()
        self.walls = game_map.walls()
        self.holes = game_map.holes()
        self.altar_bb = game_map.altars()
        self.ally_spawn = game_map.spawn_region(Team.ALLY)
        self.enemy_spawn = game_map.spawn_region(Team.ENEMY)
        self.block = self.walls | self.holes | self.altar_bb | self.enemy_spawn
        self.altars = self.altar_bb.positions()
        self.ally_c = centroid(self.ally_spawn.positions())
        self.enemy_c = centroid(self.enemy_spawn.positions())
        self.groups = [GroupInfo(i, g, self.ally_c, self.enemy_c) for i, g in enumerate(game_map.camp_groups())]
        self.by_handle = {int(g.group): g for g in self.groups}
        # camp handle -> group index, for quick lookups of visible camps
        self.camp_group = {}
        for g in self.groups:
            for c in g.camps:
                self.camp_group[int(c)] = g.idx
        # altar approach tiles: passable tiles within submit range of each altar
        r = cambc.constants.SUBMIT_GEMS_RANGE
        self.altar_ring = []
        for a in self.altars:
            b = cambc.Bitboard()
            b.draw_chebyshev(a, r)
            self.altar_ring.append(b & ~self.block)
        # Path-distance fields, filled lazily under a fuel budget: groups, then altars.
        self.group_field = [None] * len(self.groups)
        self.altar_field = [None] * len(self.altars)
        self.spawn_field = None
        # altar_group[i][g]: path distance from altar i's tribute tiles to group g (filled once fields exist)
        self.altar_group = None
        self.group_altar_min = [30] * len(self.groups)
        self.group_order = None
        self._wall = None
        self._wall_key = None
        self._table = None
        # knowledge of group states: idx -> round the group is next expected alive (0 = alive now)
        self.respawn_at = [0] * len(self.groups)
        self.reward_ready = [False] * len(self.groups)

    def camp_wall(self, r):
        """Spawn tiles of every camp group not known to be dead: planning around them keeps routes
        consistent whether or not the camps are in sight."""
        key = tuple(1 if self.respawn_at[i] > r else 0 for i in range(len(self.groups)))
        if key != self._wall_key:
            b = cambc.Bitboard()
            for g in self.groups:
                if self.respawn_at[g.idx] <= r:
                    b |= g.tiles
            self._wall, self._wall_key = b, key
        return self._wall

    def _field(self, roots):
        f = cambc.DistanceField()
        f.compute(self.block & ~roots, roots, cambc.Bitboard())
        return f

    def precompute(self, reserve):
        """Fill missing distance fields while more than `reserve` fuel remains; nearest groups first."""
        fuel = cambc.fuel_remaining
        if self.spawn_field is None and fuel() > reserve:
            self.spawn_field = self._field(self.ally_spawn)
        if self.group_order is None:
            self.group_order = sorted(range(len(self.groups)), key=lambda i: self.groups[i].ally_dist)
        for i in self.group_order:
            if self.group_field[i] is None:
                if fuel() <= reserve:
                    return False
                self.group_field[i] = self._field(self.groups[i].tiles)
        for i in range(len(self.altars)):
            if self.altar_field[i] is None:
                if fuel() <= reserve:
                    return False
                self.altar_field[i] = self._field(self.altar_ring[i])
        # altar -> group distance table, one altar row at a time
        if self._table is None:
            self._table = []
        while len(self._table) < len(self.altars):
            if fuel() <= reserve:
                return False
            f = self.altar_field[len(self._table)]
            row = []
            for g in self.groups:
                d = f.get(g.spawns[0])
                row.append(9999 if d >= 65535 else d)
            self._table.append(row)
        if self.altar_group is None:
            table = self._table
            self.altar_group = table
            for gi in range(len(self.groups)):
                self.group_altar_min[gi] = min((table[i][gi] for i in range(len(self.altars))), default=9999)
        return True

    def group_dist(self, idx, pos):
        f = self.group_field[idx]
        if f is None:
            return self.groups[idx].center.chebyshev_distance(pos) * 1.5
        d = f.get(pos)
        return 9999 if d >= 65535 else d

    def altar_dist(self, idx, pos):
        f = self.altar_field[idx]
        if f is None:
            return self.altars[idx].chebyshev_distance(pos) * 1.5
        d = f.get(pos)
        return 9999 if d >= 65535 else d

    def home_dist(self, pos):
        if self.spawn_field is None:
            return self.ally_c.chebyshev_distance(pos) * 1.5
        d = self.spawn_field.get(pos)
        return 9999 if d >= 65535 else d
