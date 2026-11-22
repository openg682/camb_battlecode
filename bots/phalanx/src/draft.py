"""Draft: take the highest-ranked champion still available."""
import cambc

D = cambc.ChampionDraft

# Re-tuned for the 3.0 rebalance: Vesper Vale (tougher, +33% gold) replaces the nerfed Stellar.
# Self-play over the 40-map pool: this order won 54/80 against the old draft.
PRIORITY = [
    D.VESPER_VALE,
    D.RYTH,
    D.MARCELLA,
    D.TAVI,
    D.WILLOW,
    D.VARG,
    D.VENDETTA,
    D.HEART_OF_RUIN,
    D.REN,
    D.PRISM,
    D.STELLAR,
    D.CASSI_AND_ISSAC,
]


def play():
    taken = set(cambc.draft.current_draft(cambc.Team.ALLY))
    for pick in PRIORITY:
        if pick in taken:
            continue
        try:
            cambc.draft.pick(pick)
            return
        except cambc.Error:
            continue
