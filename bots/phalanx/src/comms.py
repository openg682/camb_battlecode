"""Team protocol over the 8-byte shared memory.

byte 0  round stamp (round & 0xFF) written by this round's leader (the first living champion to act)
byte 1  target camp group index + 1 (0 = none)
byte 2  bits 0-1 mode (FARM, BANK, GATHER); bits 2-7 bank altar index + 1 (0 = none)
byte 3  team gem tally in units of 2: reset by the leader, each champion adds its own gems
byte 4  second squad's target camp group index + 1 (0 = team farms together)
byte 5  round & 0xFF when any champion last saw an enemy outside its spawn
byte 6  unused
byte 7  index + 1 of a camp group someone just saw captured or respawning
"""
import cambc

FARM, BANK, GATHER = 0, 1, 2

_mem = cambc.shared_memory


def read():
    buf = bytearray(8)
    _mem.read(0, buf)
    return buf


def claim_leader(round_number):
    """Return True if no living champion has acted yet this round, and claim the role."""
    stamp = round_number & 0xFF
    if _mem[0] == stamp:
        return False
    _mem[0] = stamp
    return True


def take_gem_tally():
    """Leader only: return last round's team gem total and start a fresh tally."""
    total = _mem[3] * 2
    _mem[3] = 0
    return total


def add_gems(gems):
    _mem[3] = min(255, _mem[3] + (gems + 1) // 2)


def set_plan(target_idx, mode, altar_idx):
    _mem[1] = (target_idx + 1) & 0xFF if target_idx is not None else 0
    a = (altar_idx + 1) if altar_idx is not None else 0
    _mem[2] = (mode & 3) | ((a & 63) << 2)


def mode(mem):
    return mem[2] & 3


def altar(mem):
    a = mem[2] >> 2
    return a - 1 if a else None


def set_focus(handle, pos, round_number):
    _mem[4] = ((handle + 1) & 15) | ((round_number & 15) << 4)
    _mem[5] = pos.x & 0xFF
    _mem[6] = pos.y & 0xFF


def focus(mem, round_number, max_age=1):
    """(enemy handle, x, y) of the team's focus target if set within `max_age` rounds, else None."""
    h = mem[4] & 15
    if not h:
        return None
    age = (round_number - (mem[4] >> 4)) & 15
    if age > max_age:
        return None
    return h - 1, mem[5], mem[6]


def clear_focus():
    _mem[4] = 0


def report_group(idx):
    _mem[7] = (idx + 1) & 0xFF


def set_target2(idx):
    _mem[4] = (idx + 1) & 0xFF if idx is not None else 0


def target2(mem):
    return mem[4] - 1 if mem[4] else None


def report_enemy_seen(round_number):
    _mem[5] = round_number & 0xFF


def enemy_age(mem, round_number):
    return (round_number - mem[5]) & 0xFF
