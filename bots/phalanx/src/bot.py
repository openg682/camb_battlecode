"""Champion decisions: team plan from the leader, then fight, bank, farm or regroup."""
import cambc
import combat
import comms
import nav
from seed import jitter
from cambc import game_map, Team, Vec2
from shop import Shopper
from world import World, ATTACKING

CT = cambc.CampType
CS = cambc.CampState

BANK_THRESHOLD = 70      # team-carried gems that send the team to an altar when enemies are about
BANK_MAX = 100           # always bank above this
BANK_DETOUR = 8          # bank when the altar costs at most this many extra steps
LOW_HP = 0.35            # fraction of max health that triggers a retreat
FIGHT_RATIO = 0.8        # below this fraction of their power we retreat
ENGAGE_RATIO = 1.05      # above it we press the attack
SHOP_GOLD = 300          # recall to shop when carrying this much gold and nothing else to do
DEBUG = False
SPLIT = False             # farm two camp groups at once when no enemy has been seen lately
SPLIT_QUIET = 40
CAMP_CAUTION = True
DEATH_LOG = True         # print one line per death (visible in downloaded ladder replays)
ENEMY_RACE_GEMS = 850     # bank everything once the enemy has submitted this many
DANGER_CAP = 0.7          # skip camps whose damage over the kill would exceed this share of team health
WARDS = True
ESCORT_AGE = 30          # escort bank trips when any teammate saw an enemy this recently
NO_CHASE = True
CHASE_SLACK = 2          # engage enemies within (attack range + this) tiles
NEXT_TARGET = True # while one champion claims a reward, send the rest to the next group
JOIN_FIGHTS = False 
PROFILE = False


def hp(u):
    """Current health, or 0 when the unit has died or left vision."""
    try:
        return u.health() if u.is_visible() else 0.0
    except cambc.Error:
        return 0.0


def awake(c):
    try:
        return c.is_awake()
    except cambc.Error:
        return False


def est_dps(c):
    try:
        return c.strength() * 1.1 + c.focus() * 0.25
    except cambc.Error:
        return 60.0


class Mate:
    """Last-known state of an allied champion."""
    __slots__ = ("pos", "hp", "mhp", "dps", "seen", "rng")

    def update(self, a, r):
        self.pos = a.position()
        self.hp = a.health()
        if r % 10 == 0 or not hasattr(self, "dps"):
            self.mhp = a.max_health()
            self.dps = est_dps(a)
            self.rng = a.attack_range()
        self.seen = r


class Bot:
    def __init__(self):
        self.w = None
        self.shopper = None
        self.last_b7 = 0
        self.mode = comms.FARM
        self.target = None
        self.altar = None
        self.melee = True
        self.w_done = False
        self.ally_cache = {}
        self.tally = 0
        self.team = []
        self.enemy_seen = -1000
        self.sighting = None
        self.shared_focus = None
        self.seen_enemy_local = -1000
        self.plan_round = -100
        self.squad_plan = {}
        self.altar_ban = {}
        self.blacklist = {}
        self.dl_target = None
        self.dl_round = 0
        self.approach_cache = {}
        self.caution_wait = 0
        self.caution_round = -100
        self.retreat_round = -100
        self.bank_stuck = 0
        self.next_target = None
        self.last_state = None
        self.altar_cache = None
        self.team_enemy_age = 999
        self.next_cache = None
        self.next_round = -100
        self.squad = 0
        self.squad_of = {}
        self.team_items = []
        self.last_fight = -100
        self.ranged_dps = 0.0
        self.waited = 0
        self.wait_round = -10
        self.plan_target = None

    # ------------------------------------------------------------------ setup
    def init(self, me):
        self.w = World()
        self.kind = me.champion_name()
        self.shopper = Shopper(self.kind)
        self.tactician = combat.Tactician(self)
        self.melee = me.attack_range() <= 1
        self.handle = int(me)
        self.remote_shop = me.passive() == cambc.PassiveType.REMOTE_SHOP
        self.kind_name = cambc.ChampionType.to_string(self.kind).split('.')[-1]
        handles = sorted(int(a) for a in cambc.champions(Team.ALLY))
        self.squad_of = {h: i % 2 for i, h in enumerate(handles)}
        self.squad = self.squad_of.get(self.handle, 0)

    # ------------------------------------------------------------------ turn
    def tr(self, label):
        if PROFILE and 700 <= cambc.round_number() <= 800:
            print(f"T r{cambc.round_number()} h{int(cambc.me())} {label} left={cambc.fuel_remaining()}")

    def play_turn(self):
        me = cambc.me()
        self.tr("start")
        if self.w is None:
            self.init(me)
        r = cambc.round_number()
        self.r = r
        if not self.w_done:
            self.w_done = self.w.precompute(800_000 if r <= 1 else 1_200_000)
            if self.w_done and PROFILE:
                print(f"PRECOMPUTE done r{r} h{int(me)} fuel_left={cambc.fuel_remaining()}")
        self.tr("precomputed")
        if not me.is_alive():
            if self.last_state is not None:
                # One compact line per death: these show up in ladder replays (team stdout) for analysis.
                print("DIED " + self.last_state, flush=True)
                self.last_state = None
            self.shopper.shop(me)
            self.tr("dead-shopped")
            return
        self.me = me
        self.f0 = cambc.fuel_remaining()
        self.pos = me.position()
        leader = comms.claim_leader(r)
        self.allies = []
        alive = set()
        for a in cambc.champions(Team.ALLY):
            if not a.is_alive():
                continue
            h = int(a)
            alive.add(h)
            if a.is_visible():
                self.allies.append(a)
                if leader:
                    rec = self.ally_cache.get(h)
                    if rec is None:
                        rec = self.ally_cache[h] = Mate()
                    rec.update(a, r)
        self.enemies = [e for e in cambc.champions(Team.ENEMY) if e.is_visible() and e.is_alive()]
        self.camps = [c for c in cambc.vision.camps() if c.is_visible()]
        for e in self.enemies:
            if not self.w.enemy_spawn.is_set(e.position()):
                self.seen_enemy_local = r
                comms.report_enemy_seen(r)
                break
        f1 = cambc.fuel_remaining()
        if PROFILE and self.f0 - f1 > 900_000:
            print(f"PRECOST r{r} h{self.handle} cost={self.f0 - f1} allies={len(self.allies)} enemies={len(self.enemies)} camps={len(self.camps)}")
        # Last-known records of every living ally (allies in bushes are invisible).
        self.team_items = [(h, rec) for h, rec in self.ally_cache.items() if h in alive]
        self.team = [rec for h, rec in self.team_items]
        if leader:
            self.tally = comms.take_gem_tally()
            comms.add_gems(me.gem_count())
            self.update_knowledge()
            f2 = cambc.fuel_remaining()
            self.lead()
            f3 = cambc.fuel_remaining()
            if PROFILE and r % 25 == 0:
                print(f"PROF r{r} h{self.handle} start={self.f0} pre={self.f0 - f1} know={f1 - f2} lead={f2 - f3}")
        else:
            comms.add_gems(me.gem_count())
            self.update_knowledge()
            if PROFILE and f1 - cambc.fuel_remaining() > 700_000:
                print(f"KNOWCOST r{r} h{self.handle} cost={f1 - cambc.fuel_remaining()}")
            if PROFILE and r % 25 == 0:
                print(f"PROFN r{r} h{self.handle} pre={self.f0 - f1} know={f1 - cambc.fuel_remaining()}")
        self.read_plan()
        self.tr("planned")
        if PROFILE and cambc.fuel_remaining() < 1_200_000:
            print(f"LOWFUEL pre-act r{r} h{self.handle} left={cambc.fuel_remaining()}")
        if me.is_recalling() or me.is_submitting_gems() or me.is_teleporting():
            self.why = "channel"
        else:
            self.why = "?"
            self.act()
            self.tr("acted " + self.why)
        used = self.f0 - cambc.fuel_remaining()
        if PROFILE and used > 1_800_000:
            print(f"HEAVY r{r} h{self.handle} used={used} why={self.why} mode={self.mode}")
        if PROFILE and r % 25 == 0:
            print(f"PROFEND r{r} h{self.handle} used={self.f0 - cambc.fuel_remaining()} why={self.why}")
        if DEATH_LOG:
            self.record_state(me, r)
        if DEBUG and r % 10 == 0:
            print(f"ACT r{r} h{self.handle} ({self.pos.x},{self.pos.y}) hp={me.health():.0f}/{me.max_health():.0f} "
                  f"gold={me.gold_count()} gems={me.gem_count()} why={self.why} tgt={self.target} mode={self.mode}")

    # ------------------------------------------------------------------ knowledge
    def update_knowledge(self):
        w = self.w
        r = self.r
        pos = self.pos
        camp_group = w.camp_group
        respawn_at = w.respawn_at
        for c in self.camps:
            gi = camp_group.get(int(c))
            if gi is not None and respawn_at[gi]:
                if hp(c) > 0:
                    respawn_at[gi] = 0
                    w.reward_ready[gi] = False
        # Poll group states near us, every other round, skipping groups known to respawn later.
        if True:
            vis = None
            for g in w.groups:
                gi = g.idx
                if respawn_at[gi] > r + 2:
                    continue
                if g.center.chebyshev_distance(pos) > 8:
                    continue
                if vis is None:
                    vis = cambc.vision.visible_tiles()
                if (g.tiles & vis).is_empty():
                    continue
                try:
                    st = g.group.state()
                except cambc.Error:
                    continue
                if isinstance(st, game_map.CampGroupRespawning):
                    if respawn_at[gi] <= r:
                        comms.report_group(gi)
                    respawn_at[gi] = r + st.timer
                    w.reward_ready[gi] = False
                elif isinstance(st, game_map.CampGroupRewardAvailable):
                    w.reward_ready[gi] = True
                    respawn_at[gi] = 0
                else:
                    w.reward_ready[gi] = False
                    respawn_at[gi] = 0
        mem = comms.read()
        if mem[7] != self.last_b7:
            self.last_b7 = mem[7]
            gi = mem[7] - 1
            if 0 <= gi < len(w.groups) and respawn_at[gi] <= r:
                respawn_at[gi] = r + w.groups[gi].respawn
                w.reward_ready[gi] = False

    def team_gems(self):
        return max(self.tally, self.me.gem_count())

    # ------------------------------------------------------------------ leader
    def lead(self):
        self.f_lead0 = cambc.fuel_remaining()
        w = self.w
        r = self.r
        allpts = [m.pos for m in self.team]
        cen = Vec2(sum(p.x for p in allpts) // len(allpts), sum(p.y for p in allpts) // len(allpts))
        # Travel is measured from the ally nearest the team centre and the leader (two field lookups per
        # group: averaging over every ally costs too much fuel on maps with many groups).
        mid = min(allpts, key=lambda p: p.chebyshev_distance(cen))
        pts = [mid] if mid == self.pos else [mid, self.pos]
        team_dps = sum(m.dps for m in self.team)
        team_hp = sum(m.hp for m in self.team)
        self.ranged_dps = sum(m.dps for m in self.team if m.rng > 1)
        gems = self.team_gems()
        submitted = cambc.global_submitted_gems(Team.ALLY)
        if r <= 1:
            comms.clear_focus()
        mem = comms.read()
        cur_target = mem[1] - 1 if mem[1] else None
        cur_mode = comms.mode(mem)
        if self.enemies or self.sighting is not None:
            self.enemy_seen = r

        fa = cambc.fuel_remaining()
        target = self.choose_group(pts, team_dps, team_hp, cur_target, gems)
        fb = cambc.fuel_remaining()
        if PROFILE and r % 25 == 0:
            print(f"PROFLEAD r{r} pre_choose={self.f_lead0 - fa} choose_group={fa - fb} groups={len(w.groups)}")
        mode = comms.FARM
        altar = None
        if gems > 0:
            if self.altar_cache is not None and r - self.altar_cache[0] < 4:
                altar, d_altar = self.altar_cache[1], self.altar_cache[2]
            else:
                altar, d_altar = self.choose_altar(pts)
                self.altar_cache = (r, altar, d_altar)
            if altar is not None and self.should_bank(gems, submitted, target, altar, d_altar, pts, cur_mode):
                mode = comms.BANK
            else:
                altar = None
        if target is None and mode == comms.FARM:
            mode = comms.GATHER
        target2 = None
        quiet = (mem[5] == 0 and r < 250) or comms.enemy_age(mem, r) > SPLIT_QUIET
        if SPLIT and quiet and mode == comms.FARM and target is not None:
            sq = [[], []]
            for h, m in self.team_items:
                sq[self.squad_of.get(h, 0)].append(m)
            if len(sq[0]) >= 1 and len(sq[1]) >= 1 and len(self.team) >= 3:
                prev_b = self.squad_plan.get("b", (0, None))[1]
                ta = self.pick_for(sq[0], prev_b, gems, "a")
                tb = self.pick_for(sq[1], ta, gems, "b")
                # Splitting only pays when both squads can take gem-bearing camps.
                if ta is not None and tb is not None and w.groups[ta].gems > 0 and w.groups[tb].gems > 0:
                    target, target2 = ta, tb
        if NEXT_TARGET and target2 is None and target is not None and w.reward_ready[target] and mode == comms.FARM:
            # One champion claims the reward; the rest head on to the next group.
            target2 = self.next_cache if (self.next_cache is not None and r - self.next_round < 3) else None
            if target2 is None:
                target2 = self.scan_groups(allpts, team_dps, team_hp, None, gems, target)
                self.next_cache, self.next_round = target2, r
        comms.set_plan(target, mode, altar)
        comms.set_target2(target2)
        if DEBUG and target != cur_target and r < 100:
            print(f"SWITCH r{r} leader h{self.handle} {cur_target}->{target} n={len(self.allies)} hp={team_hp:.0f} dps={team_dps:.0f}")
        if DEBUG and r % 10 == 0:
            tg = w.groups[target] if target is not None else None
            print(f"PLAN r{r} cen=({cen.x},{cen.y}) n={len(self.allies)} gems={gems} sub={submitted} mode={mode} "
                  f"target={target} {(tg.center.x, tg.center.y, tg.gems, tg.gold) if tg else ''} altar={altar} "
                  f"pos={[(a.position().x, a.position().y) for a in self.allies]}")

    def pick_for(self, members, exclude, gems, key):
        """Best group for a squad (its own positions, damage and health); cached a few rounds."""
        r = self.r
        cache = self.squad_plan.get(key)
        # Keep a squad on its group until the group has been claimed.
        if cache is not None and cache[1] is not None and cache[1] != exclude and self.w.respawn_at[cache[1]] <= r:
            return cache[1]
        if cache is not None and cache[1] is None and r - cache[0] < 3:
            return None
        pts = [m.pos for m in members]
        dps = sum(m.dps for m in members)
        hpv = sum(m.hp for m in members)
        t = self.scan_groups(pts, dps, hpv, cache[1] if cache else None, gems, exclude)
        self.squad_plan[key] = (r, t)
        return t

    def should_bank(self, gems, submitted, target, altar, d_altar, pts, cur_mode):
        w = self.w
        if cur_mode == comms.BANK:
            return True  # finish the trip
        # Carried gems count for nothing at the round limit or once the enemy reaches the target.
        if self.r + d_altar + 15 >= cambc.constants.MAX_ROUNDS:
            return True
        if cambc.global_submitted_gems(Team.ENEMY) >= ENEMY_RACE_GEMS:
            return True
        if submitted + gems >= cambc.constants.WINNING_GEMS:
            return True
        if target is None or gems >= BANK_MAX:
            return True
        if self.r - self.enemy_seen <= 30 and gems >= BANK_THRESHOLD:
            return True
        travel = sum(w.group_dist(target, p) for p in pts) / len(pts)
        ready_in = w.respawn_at[target] - self.r
        if ready_in > d_altar * 2:
            return True  # we would only be waiting anyway
        if w.altar_group is not None:
            detour = d_altar + w.altar_group[altar][target] - travel
            if detour <= BANK_DETOUR:
                return True
        return False

    def choose_group(self, pts, team_dps, team_hp, cur, gems):
        w = self.w
        r = self.r
        # Re-plan every few rounds; in between keep a still-valid target.
        if (cur is not None and r - self.plan_round < 6 and w.respawn_at[cur] <= r + 4
                and cur == self.plan_target):
            return cur
        # Give up on a target that has not paid out well past its estimated time.
        if cur is not None and cur == self.dl_target and r > self.dl_round and w.respawn_at[cur] <= r:
            self.blacklist[cur] = r + 150
            cur = None
        if cur is not None and w.respawn_at[cur] <= r + 4 and cambc.fuel_remaining() < 1_000_000:
            return cur  # re-plan on a turn with more fuel to spare
        self.plan_round = r
        best = self.scan_groups(pts, team_dps, team_hp, cur, gems, None)
        if best != self.dl_target:
            self.dl_target = best
            if best is not None:
                g = w.groups[best]
                travel = sum(w.group_dist(best, p) for p in pts) / max(len(pts), 1)
                self.dl_round = r + 2 * travel + 2 * g.hp / max(team_dps, 1.0) + 40
        self.plan_target = best
        return best

    def scan_groups(self, pts, team_dps, team_hp, cur, gems, exclude):
        w = self.w
        r = self.r
        best, best_score = None, 0.0
        carry_w = min(gems / 150.0, 1.5) * 0.5
        inv_dps = 1.0 / max(team_dps, 1.0)
        hp_cap = team_hp * DANGER_CAP
        npts = len(pts)
        fields = w.group_field
        respawn_at = w.respawn_at
        ready = w.reward_ready
        amin = w.group_altar_min
        # Cheap upper bound on each group's score (walking distance is never shorter than straight-line
        # distance): examine groups best-bound first and stop once nothing left can win.
        p0 = pts[0]
        order = []
        for g in w.groups:
            gi = g.idx
            if gi == exclude or self.blacklist.get(gi, 0) > r:
                continue
            ub = g.value / (g.center.chebyshev_distance(p0) - 3 + g.base_cost if g.center.chebyshev_distance(p0) > 3 else g.base_cost)
            if gi == cur:
                ub *= 1.5
            order.append((ub, gi))
        order.sort(reverse=True)
        groups = w.groups
        for ub, gi in order:
            if ub * 1.002 <= best_score:
                break
            g = groups[gi]
            f = fields[gi]
            if f is not None:
                travel = 0
                for p in pts:
                    travel += f.get(p)
                travel /= npts
            else:
                travel = g.center.chebyshev_distance(pts[0]) * 1.5
            if travel >= 9999:
                continue
            ready_in = respawn_at[gi] - r
            if ready_in > travel + 4:
                continue
            kill = 0.0 if ready[gi] else g.hp * inv_dps
            gdps = g.dps
            if g.heavy:
                # Pulled one camp at a time by the ranged champions (see heavy_pull).
                kill = 0.0 if ready[gi] else g.hp / max(self.ranged_dps, 1.0) + 10
                gdps = g.dps * 2.0 / g.attackers
            # Camps focus one champion at a time; skip groups whose damage would overwhelm the team.
            if gdps * kill > hp_cap:
                continue
            cost = (travel if travel > ready_in else ready_in) + kill + g.base_cost + carry_w * amin[gi]
            score = g.value / cost * (1.0 + 0.002 * jitter(gi, r // 25))
            if gi == cur:
                score *= 1.5
            if score > best_score:
                best, best_score = gi, score
        return best

    def choose_altar(self, pts):
        w = self.w
        best, best_d, best_raw = None, None, None
        for i, a in enumerate(w.altars):
            if a.chebyshev_distance(self.pos) <= 8 and self.enemy_lock(a) is not None:
                d_pen = 25  # locked: still usable after three hits, but prefer a clean altar
            else:
                d_pen = 0
            d = sum(w.altar_dist(i, p) for p in pts) / len(pts)
            if d >= 9999:
                continue
            # Prefer altars on our half of the map, and never one beside the enemy spawn.
            score = d + d_pen + max(0, a.chebyshev_distance(w.ally_c) - a.chebyshev_distance(w.enemy_c)) // 2
            if a.chebyshev_distance(w.enemy_c) <= 7:
                score += 60
            if best_d is None or score < best_d:
                best, best_d, best_raw = i, score, d
        return best, (best_raw if best_raw is not None else 9999)

    def read_plan(self):
        mem = comms.read()
        self.target = mem[1] - 1 if mem[1] else None
        t2 = comms.target2(mem)
        self.next_target = None
        if t2 is not None:
            if SPLIT and self.squad == 1:
                self.target = t2
            elif not SPLIT:
                self.next_target = t2
        self.mode = comms.mode(mem)
        self.team_enemy_age = comms.enemy_age(mem, self.r) if mem[5] else 999
        self.altar = comms.altar(mem)
        f = comms.focus(mem, self.r, max_age=2)
        self.sighting = Vec2(f[1], f[2]) if f else None
        self.shared_focus = f

    # ------------------------------------------------------------------ actions
    def act(self):
        me = self.me
        w = self.w
        pos = self.pos
        myhp, mhp = me.health(), me.max_health()
        at_spawn = w.ally_spawn.is_set(pos)
        near_enemies = []
        for e in self.enemies:
            ep = e.position()
            d = ep.chebyshev_distance(pos)
            if d > 8:
                continue
            # Enemies resting in their own spawn heal fully every round and cannot be chased.
            if w.enemy_spawn.is_set(ep):
                continue
            near_enemies.append(e)

        if at_spawn or (self.remote_shop and me.gold_count() >= 30):
            self.shopper.shop(me)
            if myhp < mhp * 0.95 and not near_enemies:
                self.why = "heal"
                return  # spawn heals fully next round

        # Retreat when badly hurt.
        if myhp < mhp * LOW_HP and not at_spawn:
            threats = [e.position() for e in near_enemies
                       if e.position().chebyshev_distance(pos) <= e.attack_range() + 3]
            threats += [c.position() for c in self.camps
                        if c.kind() in ATTACKING and awake(c) and hp(c) > 0 and c.position().chebyshev_distance(pos) <= 4]
            self.why = "retreat"
            if not threats:
                try:
                    me.recall()
                except cambc.Error:
                    pass
                return
            if near_enemies:
                caught = any(e.position().chebyshev_distance(pos) <= 2 for e in near_enemies)
                if combat.RETREAT_MODE >= 2 and caught:
                    # Running at equal speed only gives the chaser free hits: keep trading instead.
                    self.engage(near_enemies)
                else:
                    self.engage(near_enemies, force_retreat=True)
                return
            self.attack_best(near_enemies)
            nav.step_away(w, me, threats)
            return

        if NO_CHASE and near_enemies:
            # Only fight enemies close enough to trade blows, or worth chasing (carrying gems).
            myr = me.attack_range()
            close = []
            for e in near_enemies:
                d = e.position().chebyshev_distance(pos)
                if d <= max(myr, e.attack_range()) + CHASE_SLACK:
                    close.append(e)
                else:
                    try:
                        if e.gem_count() >= 30:
                            close.append(e)
                    except cambc.Error:
                        pass
            if close:
                near_enemies = near_enemies  # keep the full local picture for the power estimate
            else:
                near_enemies = []
        if near_enemies and self.engage(near_enemies):
            self.why = "engage"
            return

        self.place_lock()
        if WARDS:
            self.place_ward()
        gems = me.gem_count()
        # Join a fight a teammate is in nearby.
        sf = self.shared_focus
        if (JOIN_FIGHTS and sf is not None and self.mode != comms.BANK and myhp >= mhp * 0.5):
            fp = Vec2(sf[1], sf[2])
            if 2 < fp.chebyshev_distance(pos) <= 10:
                self.why = "join"
                nav.step_towards(w, me, nav.ring(fp, me.attack_range()) & ~w.block)
                return
        # Opportunistic tribute when an altar is right here or a couple of steps away.
        if gems > 0 and not near_enemies:
            if self.try_submit():
                self.why = "submit"
                return
            for i, a in enumerate(w.altars):
                if a.chebyshev_distance(pos) <= 3 and not self.camp_fight_active():
                    if self.break_lock(a):
                        self.why = "unlock"
                        return
                    if nav.step_towards(w, me, w.altar_ring[i]):
                        self.why = "to-altar"
                        if self.try_submit():
                            pass
                        return

        if self.mode == comms.BANK and self.altar is not None:
            if gems > 0 or self.r - self.seen_enemy_local <= 20 or self.team_enemy_age <= ESCORT_AGE:
                self.why = "bank"
                self.bank(gems)
                return

        if (me.gold_count() >= SHOP_GOLD and gems == 0 and not near_enemies
                and pos.chebyshev_distance(w.ally_c) > 6 and not self.camp_fight_active()):
            try:
                me.recall()
                self.why = "shop-recall"
                return
            except cambc.Error:
                pass

        if self.target is not None:
            self.why = "farm"
            self.farm(w.groups[self.target])
            return
        self.why = "gather"
        self.gather()

    # ------------------------------------------------------------------ combat
    def team_power(self, units):
        total = sum(hp(u) for u in units)
        dps = sum(est_dps(u) for u in units)
        return total * dps

    def engage(self, near_enemies, force_retreat=False):
        if PROFILE and cambc.fuel_remaining() < 900_000:
            print(f"LOWFUEL engage r{self.r} h{self.handle} left={cambc.fuel_remaining()}")
        pos = self.pos
        r = self.r
        vis = [a for a in self.allies if a.position().chebyshev_distance(pos) <= 8]
        friends = vis
        ours = self.team_power(vis)
        theirs = self.team_power(near_enemies)
        # Hysteresis: once fighting, only break off when clearly losing.
        bar = FIGHT_RATIO
        retreat = force_retreat or ours < theirs * bar
        if retreat:
            self.retreat_round = r
        if not retreat:
            self.last_fight = r
        aggressive = not retreat and ours >= theirs * ENGAGE_RATIO
        if DEBUG:
            print(f"FIGHT r{self.r} h{self.handle} ({pos.x},{pos.y}) hp={self.me.health():.0f} friends={len(friends)} "
                  f"foes={[(int(e), e.position().x, e.position().y, int(e.health())) for e in near_enemies]} "
                  f"ratio={ours / max(theirs, 1):.2f} retreat={retreat}")
        return self.tactician.turn(near_enemies, vis, retreat, aggressive)

    def pick_enemy(self, enemies):
        me = self.me
        dmg = max(me.strength(), 1.0)
        best, best_s = None, None
        for e in enemies:
            s = hp(e) / dmg + 0.7 * e.position().chebyshev_distance(self.pos)
            try:
                if e.gem_count() > 0:
                    s -= 3
            except cambc.Error:
                pass
            if best_s is None or s < best_s:
                best, best_s = e, s
        return best

    def fight(self, enemies):
        me = self.me
        target = self.pick_enemy(enemies)
        cast = self.cast_offensive(enemies, target, moved=False)
        if cast:
            enemies = [e for e in enemies if hp(e) > 0]
            if not enemies:
                return
            if hp(target) <= 0:
                target = self.pick_enemy(enemies)
        in_range = me.attack_tiles().is_set(target.position())
        if not in_range and not cast:
            self.try_dash(target)
            if not me.attack_tiles().is_set(target.position()):
                goal = nav.ring(target.position(), me.attack_range())
                nav.step_towards(self.w, me, goal)
        self.attack_unit(target, enemies)

    def attack_best(self, enemies):
        me = self.me
        if not me.can_attack():
            return False
        at = me.attack_tiles()
        inr = [e for e in enemies if at.is_set(e.position())]
        if not inr:
            return False
        t = min(inr, key=hp)
        return self.attack_unit(t, inr)

    def attack_unit(self, target, pool):
        me = self.me
        if not me.can_attack():
            return False
        at = me.attack_tiles()
        if hp(target) <= 0 or not at.is_set(target.position()):
            inr = [u for u in pool if hp(u) > 0 and at.is_set(u.position())]
            if not inr:
                return False
            target = min(inr, key=hp)
        try:
            me.attack(target)
        except cambc.Error:
            return False
        self.try_reset(target, pool)
        return True

    def try_reset(self, target, pool):
        me = self.me
        for a in me.abilities():
            if a.has(cambc.AbilityKind.ATTACK_RESET) and a.is_ready() and me.can_cast():
                try:
                    a.use()
                except cambc.Error:
                    return
                if hp(target) <= 0:
                    at = me.attack_tiles()
                    inr = [u for u in pool if hp(u) > 0 and at.is_set(u.position())]
                    if not inr:
                        return
                    target = min(inr, key=hp)
                try:
                    me.attack(target)
                except cambc.Error:
                    pass
                return

    def try_dash(self, target):
        me = self.me
        for a in me.abilities():
            if a.has(cambc.AbilityKind.DASH) and a.is_ready() and me.can_cast():
                tiles = a.valid_tiles() & nav.ring(target.position(), me.attack_range())
                tiles &= ~nav.blocked_now(self.w, self.pos)
                if tiles.is_empty():
                    return False
                dest = min(tiles.positions(), key=lambda p: p.chebyshev_distance(self.pos))
                try:
                    return a.use(dest)
                except cambc.Error:
                    return False
        return False

    def cast_offensive(self, enemies, target, moved):
        """Cast the best ability against champions; heals and shields count too. Returns True if cast."""
        me = self.me
        if not me.can_cast():
            return False
        K = cambc.AbilityKind
        abil = me.abilities()
        # Heal a hurt ally first.
        for a in abil:
            if a.has(K.HEAL) and a.is_ready():
                if self.cast_heal(a):
                    return True
        for a in abil:
            if not a.is_ready():
                continue
            info = a.info()
            if info is None or (info.exclusive_with_movement and moved):
                continue
            if a.has(K.SHIELD):
                if me.health() < me.max_health() * 0.85:
                    try:
                        if a.use():
                            return True
                    except cambc.Error:
                        pass
                continue
            if a.has(K.CC) or a.has(K.DAMAGE) or a.has(K.AOE_DAMAGE):
                tiles = a.valid_tiles()
                cands = [e for e in enemies if tiles.is_set(e.position())]
                if not cands:
                    continue
                t = target if target in cands else min(cands, key=hp)
                if a.has(K.CC):
                    # Stun the most dangerous enemy in range.
                    t = max(cands, key=est_dps)
                try:
                    if a.use(t):
                        return True
                except cambc.Error:
                    pass
        return False

    def cast_heal(self, a):
        tiles = a.valid_tiles()
        best, best_frac = None, 0.8
        for ally in self.allies:
            if int(ally) == self.handle or not tiles.is_set(ally.position()):
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

    def cast_on_camp(self, camp, live):
        me = self.me
        if not me.can_cast():
            return False
        K = cambc.AbilityKind
        for a in me.abilities():
            if a.has(K.HEAL) and a.is_ready() and self.cast_heal(a):
                return True
        for a in me.abilities():
            if not a.is_ready():
                continue
            if a.has(K.SHIELD):
                if me.health() < me.max_health() * 0.8:
                    try:
                        if a.use():
                            return True
                    except cambc.Error:
                        pass
                continue
            if a.has(K.DAMAGE) or a.has(K.AOE_DAMAGE) or a.has(K.CC):
                if not a.can_target_camps():
                    continue
                tiles = a.valid_tiles()
                t = camp if tiles.is_set(camp.position()) else None
                if t is None:
                    cands = [c for c in live if tiles.is_set(c.position())]
                    if not cands:
                        continue
                    t = min(cands, key=hp)
                try:
                    if a.use(t):
                        return True
                except cambc.Error:
                    pass
        return False

    # ------------------------------------------------------------------ economy
    def try_submit(self):
        me = self.me
        for a in self.w.altars:
            if a.chebyshev_distance(self.pos) <= cambc.constants.SUBMIT_GEMS_RANGE:
                try:
                    me.submit_gems(a)
                    return True
                except cambc.Error:
                    continue
        return False

    def record_state(self, me, r):
        """Remember what this champion was doing, cheaply, in case it dies before its next turn."""
        pos = self.pos
        ne, nd = 0, 99
        for e in self.enemies:
            try:
                d = e.position().chebyshev_distance(pos)
            except cambc.Error:
                continue
            if d <= 8:
                ne += 1
            if d < nd:
                nd = d
        na = 0
        for a in self.allies:
            try:
                if int(a) != self.handle and a.position().chebyshev_distance(pos) <= 6:
                    na += 1
            except cambc.Error:
                pass
        try:
            hpv, gems = int(me.health()), me.gem_count()
        except cambc.Error:
            hpv, gems = -1, -1
        self.last_state = (f"r{r} {self.kind_name} {self.why} at {pos.x},{pos.y} hp {hpv} enemies {ne} near {nd} "
                           f"allies6 {na} gems {gems} mode {self.mode} tgt {self.target}")

    def place_ward(self):
        """Keep a ward beside the altar we are at: altars are where carriers get ambushed, and a ward
        lets us see who is waiting there next time."""
        me = self.me
        try:
            if me.ward_cooldown() > 0:
                return False
        except cambc.Error:
            return False
        for a in self.w.altars:
            if a.chebyshev_distance(self.pos) > 3:
                continue
            try:
                mine = me.ward_positions()
            except cambc.Error:
                return False
            if any(wp.chebyshev_distance(a) <= 6 for wp in mine):
                return False
            tiles = me.ward_placement_tiles()
            if tiles.is_empty():
                return False
            spot = min(tiles.positions(), key=lambda p: p.chebyshev_distance(a))
            try:
                return me.place_ward(spot)
            except cambc.Error:
                return False
        return False

    def place_lock(self):
        """Lock any altar within reach that does not already carry our lock."""
        me = self.me
        try:
            slot = me.consumables()[cambc.ConsumableType.ALTAR_LOCK]
            if slot.count() == 0:
                return False
        except cambc.Error:
            return False
        for a in self.w.altars:
            if a.chebyshev_distance(self.pos) > 2:
                continue
            try:
                if cambc.vision.has_altar_lock(a) and cambc.vision.altar_lock_at(a).is_ally():
                    continue
                if slot.use(a):
                    return True
            except cambc.Error:
                continue
        return False

    def enemy_lock(self, altar):
        """The enemy's lock on `altar` if we can see one (an altar may hold one lock per team)."""
        try:
            for lock in cambc.vision.altar_locks(Team.ENEMY):
                if lock.position() == altar:
                    return lock
        except cambc.Error:
            pass
        return None

    def break_lock(self, altar):
        """Attack an enemy altar lock on `altar` when in reach. Returns True if this turn was used."""
        me = self.me
        if altar.chebyshev_distance(self.pos) > 8:
            return False
        lock = self.enemy_lock(altar)
        if lock is None:
            return False
        if DEBUG and self.r % 50 == 0:
            print(f"LOCK r{self.r} h{self.handle} ({self.pos.x},{self.pos.y}) altar=({altar.x},{altar.y}) hits={lock.hits_remaining()} canatk={me.can_attack()} inrange={me.attack_tiles().is_set(altar)}")
        if me.can_attack() and me.attack_tiles().is_set(altar):
            try:
                me.attack(lock)
                return True
            except cambc.Error:
                return False
        nav.step_towards(self.w, me, nav.ring(altar, me.attack_range()) & ~self.w.block)
        return True

    def bank(self, gems):
        if PROFILE and cambc.fuel_remaining() < 900_000:
            print(f"LOWFUEL bank r{self.r} h{self.handle} left={cambc.fuel_remaining()}")
        w = self.w
        me = self.me
        ai = self.altar
        if gems > 0 and self.altar_ban.get(ai, -1) >= self.r:
            # Our assigned altar is jammed: use the nearest other one.
            alts = [i for i in range(len(w.altars)) if self.altar_ban.get(i, -1) < self.r]
            if alts:
                ai = min(alts, key=lambda i: w.altar_dist(i, self.pos))
        a = w.altars[ai]
        if gems > 0 and self.r - self.retreat_round <= 2:
            # We just backed off from enemies: if they are holding this altar, use another.
            for e in self.enemies:
                if e.position().chebyshev_distance(a) <= 8:
                    self.altar_ban[ai] = self.r + 40
                    alts = [i for i in range(len(w.altars)) if self.altar_ban.get(i, -1) < self.r]
                    if alts:
                        ai = min(alts, key=lambda i: w.altar_dist(i, self.pos))
                        a = w.altars[ai]
                    break
        if DEBUG and self.r % 50 == 0:
            print(f"BANK r{self.r} h{self.handle} ({self.pos.x},{self.pos.y}) gems={gems} altar=({a.x},{a.y}) d={w.altar_dist(ai, self.pos)} stuck={self.bank_stuck} ban={self.altar_ban}")
        if gems > 0:
            if self.break_lock(a):
                return
            if self.try_submit():
                self.bank_stuck = 0
                return
            ok = nav.step_towards(w, me, w.altar_ring[ai])
            if DEBUG and 300 <= self.r <= 305 and self.handle == 0:
                print(f"BSTEP r{self.r} from ({self.pos.x},{self.pos.y}) ok={ok} now=({me.position().x},{me.position().y}) ring={[(q.x,q.y) for q in w.altar_ring[ai].positions()]}")
            if ok or w.altar_ring[ai].is_set(self.pos):
                self.bank_stuck = 0
            else:
                self.bank_stuck += 1
                if self.bank_stuck >= 2 and self.clear_path():
                    return
                if self.bank_stuck >= 6:
                    self.altar_ban[ai] = self.r + 40
                    self.bank_stuck = 0
            if DEBUG and not ok and self.r % 10 == 5:
                ring = w.altar_ring[self.altar]
                bl = nav.blocked_now(w, self.pos)
                print(f"BANKSTUCK r{self.r} h{self.handle} ({self.pos.x},{self.pos.y}) altar=({a.x},{a.y}) ring={ring.count()} "
                      f"free={(ring & ~bl).count()} canmove={me.can_move()} on={ring.is_set(self.pos)}")
            self.attack_nearby_camps()
            return
        # Escort: stay near the altar without blocking its tribute tiles.
        if self.attack_nearby_camps():
            return
        d = self.pos.chebyshev_distance(a)
        if d > 5 or d <= 2:
            nav.step_towards(w, me, nav.ring(a, 5) & ~nav.ring(a, 2) & ~w.block)

    def clear_path(self):
        """Attack any camp in reach (they may be standing in the only way through)."""
        me = self.me
        if not me.can_attack():
            return False
        at = me.attack_tiles()
        cs = [c for c in self.camps if hp(c) > 0 and at.is_set(c.position())]
        if not cs:
            return False
        return self.attack_unit(min(cs, key=hp), cs)

    def attack_nearby_camps(self):
        live = [c for c in self.camps if hp(c) > 0 and awake(c)]
        if not live:
            return False
        return self.attack_unit(min(live, key=hp), live)

    def camp_fight_active(self):
        for c in self.camps:
            if hp(c) > 0 and awake(c) and c.position().chebyshev_distance(self.pos) <= 4:
                return True
        return False

    # ------------------------------------------------------------------ farming
    def farm(self, g):
        if PROFILE and cambc.fuel_remaining() < 700_000:
            print(f"LOWFUEL farm-entry r{self.r} h{self.handle} left={cambc.fuel_remaining()} used={self.f0 - cambc.fuel_remaining()}")
        w = self.w
        me = self.me
        pos = self.pos
        idx = w.camp_group
        live = [c for c in self.camps if idx.get(int(c)) == g.idx and hp(c) > 0]
        if DEBUG and self.r % 10 == 0:
            print(f"FARM r{self.r} h{self.handle} live={[(int(c), c.position().x, c.position().y, int(c.state())) for c in live]} "
                  f"camps={[(int(c), c.position().x, c.position().y) for c in self.camps]}")
        if (not live and min(sp.chebyshev_distance(pos) for sp in g.spawns) <= 6
                and (w.reward_ready[g.idx] or self.group_cleared(g))):
            self.capture(g)
            return
        if not live:
            ap = self.approach(g)
            # Long groups: walk on to spawn tiles we cannot see yet before calling it done.
            vis = cambc.vision.visible_tiles()
            unseen = (g.tiles & ~vis)
            if not unseen.is_empty() and not w.reward_ready[g.idx]:
                near = min(unseen.positions(), key=lambda q: q.chebyshev_distance(pos))
                goal = nav.ring(near, 1) & ~w.block & ~g.tiles
                if goal.is_empty() or goal.is_set(pos):
                    goal = ap
                ok = nav.step_towards(w, me, goal)
            else:
                ok = nav.step_towards(w, me, ap)
            if DEBUG and not ok and self.r % 10 == 0:
                ap = self.approach(g)
                print(f"FARMSTUCK r{self.r} h{self.handle} ({pos.x},{pos.y}) canmove={me.can_move()} ap={ap.count()} "
                      f"on={ap.is_set(pos)} d={w.group_dist(g.idx, pos)} st={int(me.state())}")
            self.attack_nearby_camps()
            return
        attackers = [c for c in live if c.kind() in ATTACKING]
        if g.heavy and len(attackers) >= 3 and self.heavy_pull(g, live, attackers):
            return
        target = min(live, key=lambda c: (0 if c.kind() in ATTACKING else 1, hp(c), int(c)))
        if CAMP_CAUTION and not awake(target) and self.r - self.seen_enemy_local <= 2:
            # Waking a camp with enemies about invites a third-party fight; wait, but not forever
            # (a lone enemy loitering nearby must not block the camp all game).
            if any(e.position().chebyshev_distance(pos) <= 10 and not w.enemy_spawn.is_set(e.position())
                   for e in self.enemies):
                if self.caution_round != self.r - 1:
                    self.caution_wait = 0
                self.caution_round = self.r
                self.caution_wait += 1
                if self.caution_wait <= 15:
                    return
        tpos = target.position()
        at = me.attack_tiles()
        in_range = at.is_set(tpos)
        cast = False
        if in_range or me.attack_range() > 1:
            cast = self.cast_on_camp(target, live)
        if not in_range and not cast:
            goal = self.camp_goal(target, live)
            nav.step_towards(w, me, goal)
        self.attack_unit(target, live)

    def heavy_pull(self, g, live, attackers):
        if PROFILE:
            self._hp0 = cambc.fuel_remaining()
        """Groups with three or more attacking camps wake together when hit, but camps never move.
        Ranged champions fight from tiles that reach one camp and no other; melee waits out of reach.
        Returns False to fall back to the normal approach."""
        w = self.w
        me = self.me
        rng = me.attack_range()
        info = [(c, c.position(), c.attack_range()) for c in attackers]
        danger = cambc.Bitboard()
        for c, p, rg in info:
            danger.draw_chebyshev(p, rg)
        free = ~(w.block | cambc.vision.camp_tiles())
        if rng <= 1:
            # Melee cannot reach a camp without standing in range of the others: hold just outside.
            hold = nav.ring(g.center, 7) & free & ~danger
            if hold.is_set(self.pos) or hold.is_empty():
                return True
            nav.step_towards(w, me, hold)
            return True
        best = None
        for c, p, rg in info:
            safe = nav.ring(p, rng) & free
            for c2, p2, rg2 in info:
                if c2 is not c and p2.chebyshev_distance(p) <= rng + rg2:
                    safe &= ~nav.ring(p2, rg2)
            if safe.is_empty():
                continue
            key = (hp(c), int(c))
            if best is None or key < best[0]:
                best = (key, c, safe)
        if best is None:
            return False
        _, target, safe = best
        if PROFILE and self._hp0 - cambc.fuel_remaining() > 300_000:
            print(f"HEAVYPULL r{self.r} h{self.handle} cost={self._hp0 - cambc.fuel_remaining()} attackers={len(info)}")
        if not safe.is_set(self.pos):
            nav.step_towards(w, me, safe)
        if hp(target) > 0 and me.attack_tiles().is_set(target.position()):
            if not self.cast_on_camp(target, [target]):
                pass
            self.attack_unit(target, [target])
        return True

    def approach(self, g):
        """Tiles near a group by walking distance (not through walls), excluding its spawn tiles:
        standing on those would block its respawn."""
        cached = self.approach_cache.get(g.idx)
        if cached is not None:
            return cached
        w = self.w
        b = cambc.Bitboard()
        for s in g.spawns:
            b.draw_chebyshev(s, 2)
        b &= ~w.block & ~g.tiles
        f = w.group_field[g.idx]
        if f is None:
            return b  # fields not ready yet; do not cache
        good = cambc.Bitboard()
        for p in b.positions():
            if f.get(p) <= 3:
                good.set(p)
        if good.is_empty():
            good = b
        self.approach_cache[g.idx] = good
        return good

    def camp_goal(self, target, live):
        """Tiles to stand on: melee next to the camp, ranged at range but behind the tank."""
        me = self.me
        tpos = target.position()
        rng = me.attack_range()
        if self.melee:
            return nav.ring(tpos, 1)
        goal = nav.ring(tpos, rng) & ~nav.ring(tpos, 1)
        if target.kind() in ATTACKING and not self.tank_engaged(tpos) and self.waited < 3:
            # Wait just outside the camp's reach for a melee ally to take aggro (briefly: we may be
            # standing in the only doorway).
            if any(a.attack_range() <= 1 and a.position().chebyshev_distance(self.pos) <= 8
                   for a in self.allies if int(a) != self.handle):
                if self.wait_round != self.r - 1:
                    self.waited = 0
                self.waited += 1
                self.wait_round = self.r
                reach = target.attack_range() + 1
                return nav.ring(tpos, reach + 1) & ~nav.ring(tpos, reach - 1)
        if self.wait_round != self.r - 1:
            self.waited = 0
        return goal

    def tank_engaged(self, tpos):
        for a in self.allies:
            if a.attack_range() <= 1 and a.position().chebyshev_distance(tpos) <= 1:
                return True
        return False

    def group_cleared(self, g):
        try:
            return isinstance(g.group.state(), game_map.CampGroupRewardAvailable)
        except cambc.Error:
            return False

    def capture(self, g):
        """Stand on a free spawn tile of a cleared group until the reward is collected."""
        me = self.me
        w = self.w
        if g.tiles.is_set(self.pos):
            return
        if self.next_target is not None and self.next_target != g.idx:
            if not (cambc.vision.champion_tiles(Team.ALLY) & g.tiles).is_empty():
                self.farm(w.groups[self.next_target])
                return
        occupied = cambc.vision.champion_tiles()
        free = g.tiles & ~occupied
        if free.is_empty():
            self.attack_nearby_camps()
            return
        nav.step_towards(w, me, free)

    def gather(self):
        """No target: wait near the group that respawns soonest, else near the team."""
        w = self.w
        best, best_t = None, None
        for g in w.groups:
            t = max(w.respawn_at[g.idx] - self.r, w.group_dist(g.idx, self.pos))
            if best_t is None or t < best_t:
                best, best_t = g, t
        if best is not None:
            nav.step_towards(w, self.me, self.approach(best))
