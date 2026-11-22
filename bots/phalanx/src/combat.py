"""Champion-versus-champion tactics: focus fire, kiting, casting and retreat.

Basic attacks deal the attacker's strength and abilities deal modifier x focus; nothing mitigates.
Everyone moves one tile a turn and may move and attack in either order, so a ranged champion that
ends its turn three tiles from a melee enemy cannot be hit back by it.
"""
import cambc
import comms
import nav
from seed import jitter
from cambc import Vec2

K = cambc.AbilityKind
RETREAT_MODE = 0     # 0: retreat home; 1: retreat onto allies; 2: also stand and fight when caught
ATTACKING_CAMPS = (cambc.CampType.WOLF, cambc.CampType.ELK, cambc.CampType.MONOLITH, cambc.CampType.GOLEM)


LINKED = (cambc.ChampionType.CASSI, cambc.ChampionType.ISSAC)


class Foe:
    __slots__ = ("u", "pos", "hp", "dps", "rng", "gems", "handle", "stunned", "linked")

    def __init__(self, u):
        self.u = u
        self.pos = u.position()
        self.hp = u.health()
        self.dps = est_dps(u)
        self.rng = u.attack_range()
        self.handle = int(u)
        try:
            self.gems = u.gem_count()
        except cambc.Error:
            self.gems = 0
        self.linked = u.champion_name() in LINKED  # killing either of Cassi/Issac kills both
        self.stunned = False
        try:
            self.stunned = u.buffs().has(cambc.BuffTypes.STUN)
        except cambc.Error:
            pass

    def reach(self):
        """Distance from which this enemy can hit a tile next turn (one step plus range)."""
        return self.rng + (0 if self.stunned else 1)


def est_dps(c):
    try:
        return c.strength() * 1.1 + c.focus() * 0.25
    except cambc.Error:
        return 60.0


def alive(u):
    try:
        return u.is_visible() and u.health() > 0
    except cambc.Error:
        return False


class CampThreat:
    """An awake attacking camp: never moves, hits the nearest champion within its range."""
    __slots__ = ("pos", "dps", "rng", "stunned")

    def __init__(self, c):
        self.pos = c.position()
        self.dps = c.strength()
        self.rng = c.attack_range()
        self.stunned = False

    def reach(self):
        return self.rng


def threat_at(t, foes):
    return sum(f.dps for f in foes if t.chebyshev_distance(f.pos) <= f.reach())


def split_threat(t, foes, camps=()):
    """(melee threat, ranged threat) on tile t next turn; camps count as unavoidable ranged threat."""
    m = r = 0.0
    for c in camps:
        if t.chebyshev_distance(c.pos) <= c.rng:
            r += c.dps
    for f in foes:
        if t.chebyshev_distance(f.pos) <= f.reach():
            if f.rng <= 1:
                m += f.dps
            else:
                r += f.dps
    return m, r


def pick_focus(foes, anchor):
    """Team-consistent focus: low health, gem carriers, close to our group."""
    best, best_s = None, None
    for f in foes:
        s = (f.hp + 60 * f.pos.chebyshev_distance(anchor) - (250 if f.gems > 0 else 0)
             - (150 if f.stunned else 0) - (300 if f.linked else 0))
        if best_s is None or s < best_s or (s == best_s and f.handle < best.handle):
            best, best_s = f, s
    return best


class Tactician:
    def __init__(self, bot):
        self.bot = bot
        self.stats = [0, 0, 0]

    # ------------------------------------------------------------------ entry
    def turn(self, enemies, allies, retreat, aggressive=False):
        """Play one combat turn. `allies` are visible living allied champions (including self)."""
        bot = self.bot
        me = bot.me
        self.me = me
        self.pos = me.position()
        self.rng = me.attack_range()
        self.str = me.strength()
        foes = [Foe(e) for e in enemies if alive(e)]
        if not foes:
            return False
        self.foes = foes
        self.camp_threats = []
        for c in bot.camps:
            try:
                if (c.kind() in ATTACKING_CAMPS and c.is_awake() and c.health() > 0
                        and c.position().chebyshev_distance(self.pos) <= 5):
                    self.camp_threats.append(CampThreat(c))
            except cambc.Error:
                pass
        pts = [a.position() for a in allies]
        anchor = Vec2(sum(p.x for p in pts) // len(pts), sum(p.y for p in pts) // len(pts))
        self.anchor = anchor
        others = [a.position() for a in allies if int(a) != int(me)]
        self.anchor_others = (Vec2(sum(p.x for p in others) // len(others), sum(p.y for p in others) // len(others))
                              if others else None)
        focus = self.team_focus(foes, anchor)

        attacked = False
        # 1. A kill in range right now always comes first.
        kill = self.killable_in_range()
        if kill is not None:
            attacked = self.basic(kill)
            foes = [f for f in foes if alive(f.u)]
            self.foes = foes
            if not foes:
                return True
            if focus.u is kill.u or not alive(focus.u):
                focus = self.refocus(foes, anchor)

        # 2. Heal / shield / stun / nuke if worth standing still for.
        cast = self.cast(focus, allies, retreat)
        foes = [f for f in foes if alive(f.u)]
        self.foes = foes
        if not foes:
            return True
        if not alive(focus.u):
            focus = self.refocus(foes, anchor)

        # 3. Attack before moving when the focus (or anyone) is already in range.
        if not attacked and me.can_attack():
            t = self.best_in_range(focus)
            if t is not None:
                attacked = self.basic(t)
                foes = [f for f in foes if alive(f.u)]
                self.foes = foes
                if focus.u is t.u and not alive(focus.u) and foes:
                    focus = self.refocus(foes, anchor)

        # 4. Move to the best tile.
        if me.can_move() and foes:
            self.move(focus, attacked, retreat, aggressive)
            self.pos = me.position()

        # 5. Attack after moving if we have not yet.
        if not attacked and me.can_attack() and foes:
            t = self.best_in_range(focus)
            if t is not None:
                attacked = self.basic(t)
        if attacked:
            self.attack_reset(focus)
        return True

    def team_focus(self, foes, anchor):
        """Use the target a teammate is already hitting when we can see it; otherwise pick and share."""
        r = self.bot.r
        sf = None
        if sf is not None:
            for f in foes:
                if f.handle == sf[0]:
                    comms.set_focus(f.handle, f.pos, r)
                    return f
        return pick_focus(foes, anchor)

    def refocus(self, foes, anchor):
        return pick_focus(foes, anchor)

    # ------------------------------------------------------------------ attacks
    def in_range(self, f, pos=None):
        return (pos or self.pos).chebyshev_distance(f.pos) <= self.rng

    def killable_in_range(self):
        if not self.me.can_attack():
            return None
        best = None
        for f in self.foes:
            if f.hp <= self.str and self.in_range(f):
                if best is None or f.gems > best.gems or (f.gems == best.gems and f.dps > best.dps):
                    best = f
        return best

    def best_in_range(self, focus):
        if focus is not None and alive(focus.u) and self.in_range(focus):
            return focus
        cands = [f for f in self.foes if self.in_range(f) and alive(f.u)]
        if not cands:
            return None
        return min(cands, key=lambda f: (f.hp, f.handle))

    def basic(self, f):
        try:
            self.me.attack(f.u)
        except cambc.Error:
            return False
        try:
            f.hp = f.u.health() if f.u.is_visible() else 0
        except cambc.Error:
            f.hp = 0
        return True

    def attack_reset(self, focus):
        me = self.me
        if not me.can_cast():
            return
        for a in me.abilities():
            if a.has(K.ATTACK_RESET) and a.is_ready():
                t = self.best_in_range(focus)
                if t is None:
                    return
                try:
                    a.use()
                except cambc.Error:
                    return
                self.basic(t)
                return

    # ------------------------------------------------------------------ casting
    def cast(self, focus, allies, retreat):
        me = self.me
        if not me.can_cast():
            return False
        abil = me.abilities()
        threat_here = threat_at(self.pos, self.foes)
        for a in abil:
            if not a.is_ready():
                continue
            if a.has(K.HEAL):
                if self.cast_heal(a, allies):
                    return True
            elif a.has(K.SHIELD):
                if threat_here > 0 and me.health() < me.max_health() * 0.9:
                    try:
                        if a.use():
                            return True
                    except cambc.Error:
                        pass
        # Offensive casts forbid moving this turn. Ranged casters skip them when a melee enemy
        # could step next to them and they would rather kite.
        melee_close = any(f.rng <= 1 and f.pos.chebyshev_distance(self.pos) <= 2 and not f.stunned
                          for f in self.foes)
        for a in abil:
            if not a.is_ready():
                continue
            is_cc, is_dmg, is_aoe = a.has(K.CC), a.has(K.DAMAGE), a.has(K.AOE_DAMAGE)
            if not (is_cc or is_dmg or is_aoe):
                continue
            info = a.info()
            if info.exclusive_with_movement and self.rng > 1 and melee_close and not retreat:
                continue
            tiles = a.valid_tiles()
            cands = [f for f in self.foes if tiles.is_set(f.pos) and alive(f.u)]
            if not cands:
                continue
            if is_cc:
                # Stun whoever threatens most; a stunned enemy cannot act or flee.
                unstunned = [f for f in cands if not f.stunned]
                if not unstunned:
                    continue
                t = max(unstunned, key=lambda f: (f.dps * (2 if f.hp < 300 else 1), -f.handle))
            elif is_aoe:
                t = max(cands, key=lambda f: (sum(1 for g in cands if g.pos.chebyshev_distance(f.pos) <= 1),
                                              -f.hp))
            else:
                t = focus if focus in cands else min(cands, key=lambda f: f.hp)
            try:
                if a.use(t.u):
                    try:
                        t.hp = t.u.health() if t.u.is_visible() else 0
                    except cambc.Error:
                        t.hp = 0
                    if is_cc:
                        t.stunned = True
                    return True
            except cambc.Error:
                pass
        # Dash: close on a low target, or escape when retreating.
        for a in abil:
            if a.has(K.DASH) and a.is_ready():
                if self.cast_dash(a, focus, retreat):
                    return True
        return False

    def cast_heal(self, a, allies):
        tiles = a.valid_tiles()
        best, best_frac = None, 0.75
        for ally in allies:
            if int(ally) == int(self.me) or not tiles.is_set(ally.position()):
                continue
            frac = ally.health() / max(ally.max_health(), 1.0)
            if frac < best_frac:
                best, best_frac = ally, frac
        if best is None:
            return False
        try:
            return a.use(best)
        except cambc.Error:
            return False

    def cast_dash(self, a, focus, retreat):
        me = self.me
        blocked = nav.blocked_now(self.bot.w, self.pos)
        tiles = a.valid_tiles() & ~blocked
        if tiles.is_empty():
            return False
        if retreat:
            if me.health() > me.max_health() * 0.4 or threat_at(self.pos, self.foes) == 0:
                return False
            dest = min(tiles.positions(), key=lambda p: (threat_at(p, self.foes), self.bot.w.home_dist(p)))
            if threat_at(dest, self.foes) >= threat_at(self.pos, self.foes):
                return False
        else:
            if focus is None or self.in_range(focus) or focus.hp > self.str * 4:
                return False
            opts = [p for p in tiles.positions() if p.chebyshev_distance(focus.pos) <= self.rng]
            if not opts:
                return False
            dest = min(opts, key=lambda p: threat_at(p, self.foes))
        try:
            return a.use(dest)
        except cambc.Error:
            return False

    # ------------------------------------------------------------------ movement
    def move(self, focus, attacked, retreat, aggressive):
        me = self.me
        w = self.bot.w
        origin = self.pos
        blocked = nav.blocked_now(w, origin)
        tiles = blocked.reachable_tiles(origin, me.base_stats().base_move_speed).positions()
        if origin not in tiles:
            tiles.append(origin)
        ranged = self.rng > 1
        low = me.health() < me.max_health() * 0.35
        best, best_s = None, None
        for t in tiles:
            mt, rt = split_threat(t, self.foes, self.camp_threats)
            s = 0.0
            if retreat or low:
                if RETREAT_MODE >= 1 and self.anchor_others is not None:
                    # Fall back onto the team rather than walking home alone with an enemy on our heels.
                    s -= (mt + rt) * 3 + 8 * t.chebyshev_distance(self.anchor_others)
                else:
                    s -= (mt + rt) * 3 + w.home_dist(t) * 4
            else:
                if not attacked:
                    if focus is not None and t.chebyshev_distance(focus.pos) <= self.rng:
                        s += 400
                    elif any(t.chebyshev_distance(f.pos) <= self.rng for f in self.foes):
                        s += 330
                    elif focus is not None:
                        s -= (40 if aggressive else 15) * t.chebyshev_distance(focus.pos)
                elif not ranged and focus is not None:
                    # Melee stays glued to its target after hitting it.
                    s += 100 if t.chebyshev_distance(focus.pos) <= 1 else -10 * t.chebyshev_distance(focus.pos)
                if aggressive:
                    # Trading blows is fine when we are stronger; ranged still kite melee.
                    s -= mt * (1.5 if ranged else 0.1) + rt * 0.2
                else:
                    s -= (mt + rt) * (1.0 if ranged else 0.3)
                # Never run ahead of the group.
                s -= 60 * max(0, t.chebyshev_distance(self.anchor) - 3)
            s -= 0.01 * (t.x * 7 + t.y) + 0.005 * jitter(t.x * 101 + t.y, self.bot.r)  # tie-break
            if best_s is None or s > best_s:
                best, best_s = t, s
        if best is None or best == origin:
            return False
        return nav._try_moves(me, [best])
