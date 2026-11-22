"""Deterministic jitter for tie-breaks. SEED 0 disables it; test harnesses vary SEED to sample
different games on the same maps."""
SEED = 0


def jitter(a, b=0):
    """Pseudo-random value in [-1, 1) from integer keys; 0 when SEED is 0."""
    if not SEED:
        return 0.0
    h = (a * 73856093) ^ (b * 19349663) ^ (SEED * 83492791)
    h = (h * 2654435761) & 0xFFFFFFFF
    return h / 2147483648.0 - 1.0
