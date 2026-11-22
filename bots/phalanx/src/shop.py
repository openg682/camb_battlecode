"""Item builds: each entry is a shard bought five times, which combines into its emblem."""
import cambc

I = cambc.ItemType
C = cambc.ChampionType

# Builds copied from the top ladder teams: stat-dense Bruiser emblems (7 stat points) and Power
# emblems (6) rather than passive emblems (5 points plus a small passive).
DPS = [I.POWER_SHARD_OF_STRENGTH, I.BRUISER_SHARD_OF_STRENGTH, I.POWER_SHARD_OF_STRENGTH,
       I.BRUISER_SHARD_OF_HEALTH, I.POWER_SHARD_OF_STRENGTH, I.BRUISER_SHARD_OF_STRENGTH]
BRUISER = [I.BRUISER_SHARD_OF_STRENGTH, I.BRUISER_SHARD_OF_HEALTH, I.BRUISER_SHARD_OF_STRENGTH,
           I.POWER_SHARD_OF_STRENGTH, I.BRUISER_SHARD_OF_STRENGTH, I.BRUISER_SHARD_OF_HEALTH]
HYBRID = [I.BRUISER_SHARD_OF_STRENGTH, I.BRUISER_SHARD_OF_FOCUS, I.BRUISER_SHARD_OF_HEALTH,
          I.BRUISER_SHARD_OF_FOCUS, I.BRUISER_SHARD_OF_STRENGTH, I.BRUISER_SHARD_OF_FOCUS]
CASTER = [I.BRUISER_SHARD_OF_FOCUS, I.BRUISER_SHARD_OF_HEALTH, I.BRUISER_SHARD_OF_FOCUS,
          I.POWER_SHARD_OF_FOCUS, I.BRUISER_SHARD_OF_FOCUS, I.BRUISER_SHARD_OF_HEALTH]
TANK = [I.BRUISER_SHARD_OF_HEALTH, I.BRUISER_SHARD_OF_STRENGTH, I.BRUISER_SHARD_OF_HEALTH,
        I.DURABILITY_SHARD_OF_HEALTH, I.BRUISER_SHARD_OF_STRENGTH, I.BRUISER_SHARD_OF_HEALTH]

BUILDS = {
    C.RYTH: TANK,
    C.MARCELLA: HYBRID,
    C.VARG: DPS,
    C.ISSAC: BRUISER,
    C.REN: TANK,
    C.HEART_OF_RUIN: CASTER,
    C.VENDETTA: BRUISER,
    C.STELLAR: CASTER,
    C.TAVI: BRUISER,
    C.PRISM: CASTER,
    C.CASSI: CASTER,
    C.WILLOW: DPS,
    C.VESPER_VALE: DPS,
}


class Shopper:
    def __init__(self, kind):
        self.build = BUILDS.get(kind, BRUISER)
        self.stage = 0
        self.bought = 0  # shards of the current stage bought so far
        self.failed = False

    def shop(self, me):
        # Carry an altar lock: our locks block only enemy tributes.
        try:
            lock = me.consumables()[cambc.ConsumableType.ALTAR_LOCK]
            if lock.count() == 0 and me.gold_count() >= cambc.ConsumableType.ALTAR_LOCK.cost():
                me.buy(cambc.ConsumableType.ALTAR_LOCK)
        except cambc.Error:
            pass
        if self.failed:
            return
        for _ in range(8):
            shard = self.build[self.stage % len(self.build)]
            gold = me.gold_count()
            cost = shard.cost()
            n = min(5 - self.bought, gold // cost)
            if n <= 0:
                return
            try:
                me.buy(shard, n)
            except cambc.Error as e:
                print("shop failed:", e)
                self.failed = True
                return
            if me.gold_count() >= gold:
                print("shop: purchase did not spend gold; stopping")
                return
            self.bought += n
            if self.bought >= 5:
                self.bought = 0
                self.stage += 1
