# Cambridge Battlecode 2026 — team "Hidden Fog Walkers"

Goal (set Nov 7 2026): reach 2000 rating by Tue Nov 10. Nov 7 start: 1678–1700, rank ~260/500; 2000 ≈ rank 90.
Read this file first every session/check-in. It is the project's memory: keep it consolidated, not a diary.

## Current state (keep short; rewrite, don't append)
- Last check-in: Tue Nov 10 12:24 (updated every check-in; >40 min old = loop stalled)
- Live (Nov 10 12:20, user choice): **macro08 = submission v56 (bots/phalanx_v71 = phalanx_cpp)**. Before that live: **econ04 = submission v52 (bots/phalanx_v67 = bots/phalanx_cpp; source ~/.cambc_lab/econ04)** — KEPT Nov 8
  ~13:30: H 13 + C 27 = 40/90 vs v47 8 + 21 = 29. = v47 + EARLY_GOLD_W 0 + fight08 (KITE + BURST_SAFE ranged) +
  draft without Vesper (4v Ryth/Marcella/Tavi/Willow, 5v + Varg) + Focus-heavy Marcella build. Fallback scripts now
  revert to v52 / phalanx_v67. Rating 1811.5, rank 189 (Nov 9 06:20) — up from ~1750 with v47.
  History of kept versions: v36 (v20) → v44 (v28) → v46 (v30) → v47 (v31) → econ04 (v52).
- v48 = dev46 (retreat at 50% HP near enemy champions) 19/45 = tie → reverted; v31 (v47) live (confirmed).
- Rating: 1699.3, rank 252 (Nov 7 ~23:00). v47 ranked (rk47/): 58–57 over 115 games — v47 sits at ~1700–1710.
  By size: 3v 11–15 (v36 era 63%!), 4v 11–9, 5v 36–33. Need a big structural gain (plan items 1, 1b) to move.
- v47 baselines: 3v-set 19/45, 5v-set 19/45 → COMBINED 38/90 (ladder_test.sh now runs both sets, base 38).
  5v-set per map: braided_river 3/3, bridges 2, orchard 2, great_wall 1, famine 0, monolith 0, sunburst 0, twisty 1,
  crossroads 1, cuts 1, happy_halloween 2, the_front 2, split 2, eight 0, sanctum 2.
- v49 = dev50 (travel dash + map fixes) LOST: 3v 16/45 + 5v 15/40 (one group 429) = 31/85 vs v47 38/90 → reverted, v31 live.
- v50 = dev52 (VESPER_CLAIMS only) LOST: 3v 16/45 + 5v 13/40 = 29/85 → reverted, v31 live.
- Control (v47 re-run, set 1 × 3 teams, 00:00): E=mc^2 and ChatGTP games IDENTICAL to v47's earlier run (unchanged);
  V4.1 Flash CHANGED its bot (different games; v47 now 3/5 vs 4/5 on that set). Per opponent (3v | 5v):
  v47 GTP 8/15 E 3/15 V 8/15 | GTP 9/15 E 4/15 V 6/15;  v49 GTP 7 E 3 V 6 | GTP 6 E 2/10 V 7;
  v50 GTP 7 E 2 V 7 | GTP 5 E 1/10 V 7. ⇒ vs the UNCHANGED opponents both v49 and v50 are genuinely worse; the
  baseline is valid except V4.1 (re-measure v47 vs V4.1 when quota allows). Tool: per-opponent split in tools/ (inline).
  Local: obs_bw05 138/256 vs dev45; dev50 142/256 vs dev45, idle econ 1184.
- Next candidate built: dev51 = dev50 + VESPER_CLAIMS (carry_near(): non-Vesper steps off / doesn't claim when a
  healthy Vesper is within 5 of the group; Vesper ignores allies on the tiles). Local h2h vs dev50 124/256 (neutral);
  gold share Vesper 0.13 → 0.46 (top-team level), Vesper deaths 3.9 → 2.6, Marcella 0.41 → 0.27. Ladder after v49.
- FIGHT AGENT report (Nov 8 ~10:05) — best candidate ~/.cambc_lab/fight04 (built, not yet laddered):
  findings: our ranged champs end next to a melee enemy 19% of rounds (Kungarna 6%) because attacking first ends
  movement (no kiting); low-HP champs stand still 38% of the time near enemies (recalling into unseen enemies, casting
  movement-locking spells while fleeing, stuns) and die 77% (Kungarna 31%/39%); top teams finish with stun → dash →
  burst. fight04 = 6 changes (combat.cpp unless noted): KITE (ranged steps out of melee reach first, then shoots),
  SAFE_RECALL (bot.cpp: walk home instead of recalling if hit/enemy seen in last 3 rounds), ESCAPE_DASH_HP (retreat
  dash without the 40% HP gate), FLEE_NO_CAST (no movement-locking casts while fleeing except stunning a reacher),
  ABILITY_KILLS (cast any damage spell that kills), FUTILE_FLIGHT (melee that can't escape hits back).
  Local: vs dev45 136/256; vs v32 156/256 (dev45 131); fight-episode death rate 0.265 vs dev45's 0.366.
  fight02 (changes 1–3) is the fallback. Tools: fight_dump.js, fight_metrics.py, fight_escape.py, fight_finish2.py,
  fight_kite.py, fight_kills.py, fight_kd.py, fight_local.sh.
- AGENTS (Nov 8 ~11:30, user: deploy agents where innovation is needed; main loop alone does ladder tests):
  fight agent (fightNN; new goal: what ~2000-rated teams do in fights), economy agent (econNN: farming throughput /
  target choice), macro agent (macroNN: draft, per-format shape, altars/locks, endgame). Each reports a candidate.
- ECONOMY AGENT: EARLY_GOLD_W 0.6 (1 gold ≈ 0.9 gem until r300, set in dev5, pre-engine-3, never re-tested) makes
  us pick gold-heavy groups: 5v gems per 1000 camp hp 9.4 vs opponents 14.0 (4v 12.0 vs 16.4); top teams match their
  opponents. econ01 = EARLY_GOLD_W 0: h2h 125/256, idle econ 1145 (v47 1206), fixes most v44–v47 map regressions
  (islands 2000 → 1136, tangle → 1462, guardians → 1538). Also: top teams' extra camp damage is damage per hit
  (their Vesper 64–81 vs our 42). Tools econ_*.py.
- FIGHT AGENT vs ~2000 teams (24 ranked games of Tzunami/Kamikaze/Prompt Beasts): our RANGED champions die 5.6/game
  to champions vs theirs 0.9 (melee 1.8 vs 1.4); our ranged spend 524 rounds/game within 5 of an enemy vs 242; ranged
  deaths while engaging at high HP (Willow avg 521 HP left) — movement scoring +400 for in-range vs ~−100 threat.
  fight08 = dev45 + KITE + BURST_SAFE (ranged never end a move where reachable enemies' basic + 2×focus spell ≥ 90%
  of current HP): vs v32 149/256 (dev45 131), vs dev45 134/256, ranged engage deaths 543 → 316.
- v47 on STRONG set H (~2000 teams): 8/45 (18%) → bar 13. econ01 (v64): 11/45 (+3, below bar) → reverted.
  fight08 (v65): 11/45 (+3) → reverted. Two independent +3s → combo01 (v66): 12/45 (+4) → just below bar, reverted.
  All candidates on H so far ≥ 11 vs v47 8. ECON AGENT: our Vesper earns 49 gold by r150 vs 136–326 for top teams
  (gold share, not item choice); our Marcella/Ryth hold 75–81 gold unspent vs ≤ 58; top teams' Marcella is Focus-
  heavy. econ04 = combo01 + Marcella build Focus-heavy (shop.cpp): local vs combo01 138/256, vs dev45 160/256 (62.5%).
  → econ04 on H + C: 13 + 27 = 40/90 → KEPT. New baselines: H 13, C 27, HC 40 (keep ≥ 47).
  econ05–08 (more shopping trips) slower to farm and no better locally → not laddered.
- MACRO AGENT: our lineups are fixed per format — 3v Ryth/Marcella/Tavi 66%, 4v + Vesper 44% (worst), 5v + Willow
  50%. Vesper is our worst champion in 4v (4.6 deaths, 3.8 kills, 91 banked gems vs Marcella 245) and weak in 5v
  (with Willow). Kamikaze (~2000) plays 4v Willow/Tavi/Marcella/Ryth (10/13 wins), 5v Prism/Marcella/Willow/Ryth/Varg.
  Losses = lost fights (5.6 untraded deaths per loss vs 2.6 per win); endgame banking would save ~1 of 83 losses.
  Candidate macro03E (draft.cpp only): 4v Ryth/Marcella/Tavi/Willow, 5v + Varg; local vs dev45 4v 35/52, 5v 103/162;
  vs v32/v30/v28 4v 112/156 (dev45 96), 5v 327/486 (dev45 284). Fallback macro02A (4v only). Also found: 01islands
  Willow fuel exhaustion stalls the team 700–950 rounds. Tools macro_*.py (macro_h2h.py has a per-format split).
  QUEUED after the set-H chain: macro03E on set H.
- fight09 (+ per-type burst + Marcella/Ryth check) worse (vs v32 137); fight10 (Marcella/Ryth check only) vs v32 156,
  vs dev45 130 — ≈ fight08.
- combo01 = dev45 + econ01 bot.cpp + fight08 combat.cpp/.h + macro03E draft.cpp (separate files): local vs dev45 150/256 (58.6%; 3v 17/42, 4v 33/52,
  5v 100/162) — best local result vs v47 so far. Queued on set H right after the econ01/fight08 chain.
- Nov 9: network outage then session-limit stops interrupted all agents; running fewer agents at once to save usage.
  fight13 (top-up healing, econ04 base): rule rarely fires (spawn rounds 142 → 149); local vs econ04 133/256, vs dev45
  173/256 (econ04 160); fight14 (+ recall) worse. fight13 on H + C: 12 + 25 = 37/90 → worse, reverted (v52 live). Rating ~1792 (13:10).
- MACRO loss classes vs ~2000 teams (set H): econ04's 32 losses = 11 early feeding (12+ deaths, ≥ 3 by r250) + 15 late
  feeding + 5 economy + 1 close. Gems level to r400; gold gap at r600 1542 vs 2790 with 7.8 deaths (100 gold/kill ≈ 60%
  of it). Deaths in losses: 60% retreating, 24% engaging, mostly EVEN fights, mostly mid/our half, 26% to unseen
  attackers. Same maps lose to all 3 teams (galaxy, starfish, lost_world, wheel, membrane, ribs).
  macro06 = econ04 + islands fuel guard + FEEDING BRAKE (after 4 team deaths in 150 rounds, squads skip groups within
  12 of the last enemy sighting for 150 rounds; replay: fires in 52/65 set-H losses vs 4/25 wins). Local vs dev45 155
  (econ04 160), vs econ04 132/256. v69 = macro06 on H + C: 8 + 22 = 30/90 → much worse, reverted (v52 live).
  Avoidance after deaths costs more farming than it saves (same lesson as v43 contact avoidance).
- fight15/16 BUSH AWARENESS (avoid tiles next to unseen bushes / vanished enemies): local vs econ04 96/256 and 110/256,
  MORE deaths (18 vs 14–15/game) — avoiding phantom threats breaks engagements; dropped. (Untried refinement: bush risk
  only as a small tie-break among equally threatened fleeing tiles.) Tool fight_unseen.sh.
- Rating with econ04 swings 1764–1811 (1764, rank 218 at 18:00 Nov 9).
- 6 ALWAYS-LOST MAPS (galaxy, starfish, lost_world, wheel, membrane, ribs): no bug/stall (econ04 reaches 1000 vs idle on
  all, beats dev45 on all). They are FIGHT losses: on galaxy/wheel/starfish we farm on the ENEMY'S side (deaths at 0.5–1.0
  of the spawn-to-spawn line; on wheel we sit at 0.77–0.89 by r100 while they stay at 0.13) and lose fights near their
  altar/respawn. macro07 (FAR_SIDE_W 0.7: groups nearer the enemy spawn worth 70%, ungated): vs econ04 116/256 → dropped
  (8/12 on the six maps). Untested: gate it to big maps (spawn distance > 60) or the first 300 rounds.
- ADAPT AGENT (505 econ04 ranked games vs rating expectation): enemy HEART → 141/302 = 47% (−26 wins vs expected;
  4v 39%, 5v 48%); without Heart 59% (+3). Heart takes 27% of our champion damage + 12–16k self-shields, dies 2.7/game
  (8.6k of our damage per Heart kill); with Heart our deaths rise (4v 12.0 → 16.1). Cassi+Issac −6 (small n). adapt03 =
  econ04 + Heart modelled as hp+shield and threat scaling with missing hp (combat.cpp, Heart-only): local 561/768 vs
  econ04 554 (noise). Focus-penalty / ratio / LOW_HP variants worse locally. v70 = adapt03 on H + C: 10 + 25 = 35/90
  → worse, reverted (v52 live). Rating 1748, rank 229 (20:20 Nov 9).
- RANKED REALITY CHECK (21:20 Nov 9, rating 1738): econ04's ranked level ≈ 1750–1770 (econ04-only games since 18:10:
  42/85 = 49%; during candidate test windows 6/15). Rating path: v47 1718 → 1770 (Nov 8), econ04 → 1811 peak (Nov 9
  06:20, likely variance) → 1738. Its ladder-set gain (H+C 40 vs 29) did not show up as a clear ranked gain over v47.
  Test windows expose ranked play to weaker candidates — near the deadline, test only strong candidates.
  22:27 Nov 9: rating 1774.6, rank 214 (econ04 alone, no test window) — consistent with its ~1760 level.
  23:26: 1749, rank 228. adapt04 = econ04 + STUN_DASHER_W 2.5 (our stuns prefer Heart/Vendetta while their dash is
  ready; stun target was pure dps, so low-str Heart was never stunned). Local: vs econ04 128/256 (identical — no
  Heart in our draft); vs nova03 (Heart/Vendetta draft) 170/256 vs econ04 171 → no effect, dropped (not laddered).
  00:30 Nov 10: 1762.6, rank 219. macro08 = econ04 + FAR_SIDE_W 0.7 only when spawns ≥ 40 apart (ribs, starfish,
  wheel). Local vs econ04 124/256 (noise). v71 (submission v56) = macro08: went live ~01:10, then the laptop
  went on BATTERY and slept — the script hung in `sleep 150` until 07:30, so no test games ran and macro08 played
  ~6 h of ranked untested (rating 1762 → 1747, rank 231). Killed the test, reverted to v52 at 07:35 (Live: 52).
  LESSON: before any ladder test check `pmset -g batt` says AC Power; on battery the machine sleeps and a live
  candidate is left unattended. 10:15: still on battery, v52 live, 1741.4 (rank 235); no tests until on AC. 11:20: battery, v52, 1728.4 (rank 246).
  12:20: v56 (macro08) found LIVE + pinned, set by hand (not by a script). User chose to KEEP v56 live. bots/phalanx_cpp
  synced to phalanx_v71 (= macro08). Ladder scripts still fall back to keep=52 / phalanx_v67. Rating 1729.7 (rank 242).
  Tools adapt_*.py; data ~/.cambc_lab/adapt/. Next ideas: stun Heart's dash; Cassi+Issac class.
- AGENTS (Nov 9 06:30), all building from ~/.cambc_lab/econ04: fight → fight13 proactive top-up healing; macro →
  loss classification vs ~2000 teams + one fix (macro06); economy → econ04's economy vs ~2000 teams (econ09); NEW
  adapt agent → opponent-adaptive play: classify the enemy draft at round 1 and adapt parameters (adaptNN).
- AGENT TASKS (12:30, all build from ~/.cambc_lab/combo01, share findings via the main agent): economy → item stat
  builds vs ~2000 teams (damage per hit gap; econ03, shop.cpp only); macro → retreat toward allies + 01islands fuel
  fix (macro04/05); fight → 3-a-side fights (combo01 3v 17/42 locally; fight11, combat.cpp only).
  macro04 (retreat toward healthy allies) WORSE locally (vs combo01 119/256) — dropped. macro05 = combo01 + islands
  fuel guard (only island maps change; no mid-game fuel exhaustion) — neutral, add to a later bundle. The 01islands
  stall was Vesper's never-firing hop; the no-Vesper draft already avoids it. Macro agent now analysing combo01's 33
  real losses vs ~2000 teams (mgH_v66).
- FIGHT AGENT 3v: fight11 (kite/burst off at ≤3 champions) 128/210, fight12 (+ hit back if no escape) 128/210 vs
  combo01 127/210 → no gain; combo01's 3v 17/42 vs dev45 looks like noise. ~2000 teams in 3v: episode deaths 12–29% vs
  v47 43%; they spend 359 champion-rounds/game AT SPAWN vs our 118 (heal + shop → enter fights at full HP); our 3v
  deaths: Marcella at 58% walking into 3 enemies, Ryth fleeing without hitting back, Tavi at 42% into a hidden
  Vendetta. → fight13: proactive top-up healing (no enemy near, < ~70% HP, home close → heal first).
- STRONGER TEST OPPONENTS (user, Nov 8): set H = mgrunH.sh — set C maps vs ~2000-rated Tzunami 9a848453…, Kamikaze
  f243d7fa…, Prompt Beasts 94f6d660… (45 games). Baseline v47 on H after the current test.
- `bots/phalanx_cpp` must equal the live version. Kept backups in bots/: phalanx_v36, v44, v46, v47 (+ nova); failed
  candidates' folders were removed Nov 8 09:20 (sources remain in `~/.cambc_lab/devNN`).

## PHASE: IMPLEMENTATION started Nov 7 23:20 (observation phase done; user: ready to implement, hard deadline 00:40)
- Until 00:40: no new ladder tests or bot changes (v48's in-flight test may finish). Observe at scale, answer the
  "Open questions", consolidate into Learnings/Observations, and end with a ranked IMPLEMENTATION PLAN section.
- After 00:40: implement the plan's top items one at a time with ladder tests (process below).

## NOVA — strategy redesigned from scratch (user permission Nov 8 00:00: rethink + separate submission)
Built only from verified mechanics + observations; reuses the MECHANICS modules of phalanx (world.cpp knowledge,
nav.cpp pathing, combat.cpp ability execution, shop.cpp, comms.h memory, update_knowledge/claim/bank primitives) but
replaces the ORGANISATION and ROLES. Lives in bots/nova (own dev copies ~/.cambc_lab/novaNN); submit as its own bot.
- Shape (what top teams do: 3.3 units of 5, Vesper alone 59–69%): 3 fixed units — CARRY = Vesper alone; two PAIRS of
  the other four (each pair solos small groups, together on big ones). Fewer than 5 champions: carry + 1 pair (+ solo).
- Carry: farms its own groups (prefers cheap/near altar/our side), claims its own rewards (others never claim when
  it is within 5), dashes on cooldown while travelling with no enemy within 8 (keep the dash for escapes otherwise),
  flees at 50% HP when an enemy champion is within 6. Target gold share ~0.4.
- Pairs: join the other pair or the carry when idle (no waiting), help the carry when an enemy is near it.
- Draft (step 2, after the shape is proven): Heart, Marcella, Ryth, Vendetta, Vesper (dashers usable now).
- Evaluation: local h2h vs v47 + goldshare/units/casts checks, then the ladder (both sets).
Status: nova01 BUILT (~/.cambc_lab/nova01; patch script ~/.cambc_lab/nova01_patch.py applied to dev45): CARRY_SHAPE
  (setup_shape(): Vesper its own squad, others alternate into 2 squads; fixed_k_ 3 with 5, 2 with 4), carry-only
  travel dash with no enemy within 8 (nav::travel_dash), carry retreats at 50% HP near enemy champions.
  Local 00dunes check: 3.26 units, Vesper alone 82% (top teams 3.3 / 59–69%); v47 2.57 units, Vesper alone 20%.
  Vesper dash 76 per 1241 rounds (63 calm). h2h vs v47 113/256 (44%). Gold share nova01: Marcella 0.38, Ryth 0.25,
  Vesper 0.19 (v47 0.13), Tavi 0.12, Willow 0.06; deaths up (Ryth 2.7 vs 1.9, Willow 4.5 vs 3.9) — solo Vesper still
  earns little and the pairs die more than v47's bigger squads.
- nova02 = nova01 patch on dev52 (shape + VESPER_CLAIMS): h2h 119/256 (46.5%) vs v47.
- nova03 = nova02 + draft Vesper, Ryth, Marcella, Vendetta, Heart (4v/5v; 3v unchanged Ryth/Marcella/Tavi) + travel
  dash for EVERY dasher but only with no enemy within 8 (v49 dashed regardless). Pairs by toughness: (Ryth, Heart),
  (Marcella, Vendetta). h2h vs v47 77/256 (30%; 5v 44/162) → the Heart/Vendetta draft is bad with our code again
  (as dev34). DROP draft changes. nova04 = nova02 + calm travel dash for every dasher (original draft): 115/256 (45%).
- LADDER: nova02 LOST 15/45 + 13/45 = 28/90 (all 90 games played) vs v47 38/90 → reverted, v31 live. Its first ranked
  set went 1–4 vs yuri. NOVA's split shape is worse with our fight code.
  zsh gotcha: "$f:a" applies the :a modifier — write "${f}:a" in label:path:side arguments.

## IMPLEMENTATION PLAN (drafted in the observation phase; execute after 00:40, one ladder test at a time)
ORDER: 0 (baseline v47 on the 5-a-side set: 9 groups) → 5c altar-distance clamp (bug fix) → 1b dash (mechanical, smallest change) → 1 carry claims →
3 draft → 2 / 5b. Each candidate = mgrun.sh + mgrun5.sh (18 groups); quota ≈ 50 groups / 6 h → ~2 candidates per
window. Check locally first with casts.js / goldshare.py that the change does what it should.
0. **New evaluation first.** Ladder test set is 36/45 3-a-side; ranked is 64% 5-a-side. READY: `zsh ~/.cambc_lab/mgrun5.sh tag`
   (writes mg5_tag.out/.groups, prints score). Add a 5-a-side mgrun
   (maps listed under "Proposed 5-a-side ladder test set"), re-measure v47 on it (baseline), and judge every change
   on BOTH sets (or mainly the 5-a-side one). Also use v47's ranked games (rk47/) as the ranked baseline.
1. **Feed the ranged carry (strongest evidence).** Every other team gives Vesper ~40–47% of team gold; ours gets
   13–17% because the melee champion next to the camp claims every reward ("one claimer is enough"). Our Vesper dies
   to 1633 damage (theirs 4268), earns 866 gold/game (theirs 2178). Change: when a cleared group's reward is waiting,
   melee champions step off / don't claim if Vesper (or the team's carry) is within ~5 tiles and healthy; the carry
   claims. Start from dev30's CARRY_CLAIMS code (it scored 57% locally vs v36 when bundled), applied to v47 alone.
   Measure: goldshare.py (Vesper share ↑), Vesper deaths ↓, both test sets.
   VERIFIED LOCALLY (5-a-side games in runs5/, gold share / deaths per game): dev29 (no claims) Marcella 0.38,
   Ryth 0.28, Vesper 0.13 (3.5 deaths); dev30 (CARRY_CLAIMS) Marcella 0.42, Vesper 0.26 (2.8 deaths), Ryth 0.18.
   dev30's is_carry includes Marcella (and Prism/Stellar/Cassi), so Marcella keeps 0.42 → make VESPER the only
   carry (Marcella still earns via her tribute passive) to reach the ~0.4 Vesper share top teams use.
   CODE SPEC (dev30's CARRY_CLAIMS is in ~/.cambc_lab/dev30/src/bot.cpp Bot::capture; it only half-works): it stops a
   non-carry from WALKING onto the tiles, but (a) a melee champion fighting the camp often already STANDS on the spawn
   tiles and collects anyway, and (b) capture() makes the carry back off whenever ANY ally is on the tiles — so the
   melee blocks the carry. Implement: carry = Vesper only; non-carry on the group's tiles with a healthy (>40% HP)
   carry within 5 → step OFF the tiles (onto a non-tile neighbour) once the camps are dead; carry ignores allies on the
   tiles and walks to a free tile. Check with goldshare.py locally before the ladder.
   CONFIRMED ON v47 RANKED (40 games, rk47/, 21–19): our Vesper 0.07–0.10 gold share, 540 gold/game, dies to 1413
   damage; Willow 420 gold, dies to 913; Marcella 0.39–0.42, Ryth 0.31. Opponents' Vesper 0.28–0.31. v47 deaths per
   champion are much lower than v36-era ranked (1.3–2.2/game vs 3–4) — v44–v47 did help.
1b. **DASH FOR MOBILITY (big, cheap).** Abilities (dbgstats): dash = Vesper, Vendetta, Issac, Ren, Heart (slot 1);
   dash range 2, cooldown 8, NOT exclusive with movement (move 1 + dash 2 in one turn) → dashing on cooldown while
   travelling = +25% speed. Cast rates (casts.js on v47 games, per 1000 alive rounds): THEIR Vesper dash 76, Vendetta
   82, Heart 101 — OUR Vesper 6. Our Tactician::cast_dash only fires when retreating below 40% HP under threat or
   chasing a nearly-dead focus; never for travel. This explains: their low-HP champions escape us, their Vendetta/
   Heart catch ours (top killers), and part of their farming throughput. Implement: whenever a dasher moves towards a
   destination ≥ 3 steps away (target, altar, home, fleeing) and the dash is ready, dash 2 tiles along the path
   (distance-field step); in fights keep the current logic. Also makes Vendetta/Heart drafts viable later (dev34's
   Heart draft failed partly because our code never dashed). Other cast rates are similar to opponents'.
   CONFIRMED on top teams (14 engine-3 replays, topz/casts2.txt): dash per 1000 alive rounds — Ad Astra Heart 107,
   Vesper 85, Ren 72; Kungarna Vendetta 77, Heart 69, Vesper 67; their opponents 57–94. 77–85% of all those dashes
   happen with NO enemy within 6 tiles → dashing is mainly a TRAVEL tool (+25% speed), used on cooldown.
   DESIGN: in nav::step_towards (nav.cpp) the cached route.field gives distance-to-goal. Before the normal step: if
   route.valid, field(origin) ≥ 4, and a ready dash ability exists (me.abilities(), a.is_dash() && a.is_ready()),
   pick the tile in a.valid_tiles() & ~blocked_now with the smallest field value (must be ≤ field(origin) − 2),
   a.use(tile), then continue with the normal step from me.position(). Skip while channelling/recalling and when the
   goal is a camp-fight position (keep combat dashes in Tactician). Count casts with casts.js afterwards (target ≥ 60
   per 1000 rounds for Vesper).
   BUILT: ~/.cambc_lab/dev50 = obs_bw05 + TRAVEL_DASH in nav.cpp step_towards (after a successful step, if the route
   field says ≥ 3 left and a dash is ready, dash to the valid tile that cuts the most distance, ≥ 2). Local game on
   00dunes: our Vesper dashed 65 times in 1076 rounds (55 with no enemy within 6) vs v47's Vesper 5.
2. **Let the carry farm on its own** (top teams' Vesper is 4–6 tiles from allies, alone 59–69% of the time, with the
   same enemy exposure as ours). Vesper as its own unit picking small/safe groups near altars, claiming its clears;
   with JOIN_IDLE so it never waits. Only after 1 shows a gain.
3. **Draft: Willow is our worst champion** (dies to 797 damage, 0.77 kills, 558 gold, 4.35 deaths/game in ranked;
   opponents' Willow equally bad). Replace Willow in the 5-a-side draft — prefer VENDETTA (dasher, str 65+6, the top
   killer against us) once 1b makes our code use dashes; Varg (1000+100 hp, str 50, shield) as the alternative — test on the 5-a-side set. Tavi is also weak (dies to ~1500) but is our 3-a-side pick
   where we already win 63%: leave it.
4. **Early economy / blowouts:** 2–3 deaths at r150–250 stall our income; we lose 4-a-side (46%) and near-spawn maps.
   Revisit after 1–3 change the gold distribution (fed champions die less).
5. **Fight finishing:** low-HP enemies escape us (24% death even when we stay on them) while ours die (66% after one
   round of pursuit). Check stun usage (Ryth/Tavi) on fleeing targets and whether our adjacent champions attack the
   low enemy. Investigate with buffs/cooldowns from the replay engine (Game_API_Champion_GetBuffs) if needed.
   (Spending is fine: median held gold 10–25 for everyone; we just earn less early — 64 gold per champion by r150
   vs 80–99 for top teams — tools/spend.py.)
   (Banking is not a lever — banking.py: banks/champion/1000r us 4.0 vs ranked opps 4.3, Kungarna 7.4, Ad Astra 3.2;
   gems per bank ~17–27 for all; gems collected but not banked us 20%, opps 28%, Kungarna 6%, Ad Astra 33%.)
5b. **Map-specific farming failures (cheap wins).** v47 ranked blowouts are FARMING collapses, not fights: 02the_pond
   gold 0.02× (kills 0–0), 01islands 0.18×/0.29×, 01boss_battle 0.24×, 01yellow_brick_road 0.35× (camp damage
   falls to ~0.5× from r150 with no kill difference). v47 vs an IDLE opponent fails to reach 1000 gems in 2000 rounds
   on 13/128 maps and needs ≥1500 rounds on 34: 01islands 30 gems (TALLY: Vesper "hop" 144 of 250 rounds, Willow
   walk 217 — island crossing traps champions), 04gem-caverns 231, 04lines 440 (solo build: 880 / 1184 rounds —
   much better there), 02tangle, 03twisty, 01shortcut, 00stampede, 04terraces, 00pilgrimage, 02the-pack;
   02the-pond 1948, 02famine 1989. (03homonoliths: gems only in two 60k-hp monolith corners — special, skip;
   03one-gem/ultrastarvation are genuinely gem-starved.) Fix the islands/hop logic first, then per-map checks.
   Islands mechanism (bot.cpp hop_toward): walk to the shore, then dash (waits while the dash is on cooldown) or
   tp crystal (only if d ≤ 3); champions without a usable dash/crystal wait on the shore → Vesper 144 "hop" rounds.
   Only a few maps → after item 1.
5c. **Per-map REGRESSIONS from v44–v47** (idle econ, rounds to 1000; v36 → v44 → v47): 01islands 1199 → 1809 → 2000,
   02tangle 1544 → 1983 → 2000, 02guardians 1630 → 2000 → 1976, 04bowls 1248 → 1539 → 1557 (v44 HURT cost);
   03riverbed 1240 → 1788, 04gap 1429 → 1813, 00happy_halloween 1013 → 1236, 04spiral_moat 1395 → 1747, 02the_pond
   1634 → 1948 — ALL caused by v46 BANK_W 1.0 (econ_e_dev41 identical to v47 on these; v47's scarce change touched
   none). BANK_W also made 00lost_world 1250 → 710. Try BANK_W 0.5, or cap the bank-distance term (e.g. ≤ 30 rounds),
   or ignore it when group_altar_min is the 30 default (unknown/unreachable altar).
   LIKELY BUG (world.cpp ~l.130): group_altar_min = altar walking-field distance from the group's first spawn; when
   that spawn is not reachable on foot (across water/holes) it is 9999 or the field's 0xFFFF, so BANK_W × gems/70 ×
   65535 makes the group unpickable. The regressed maps are the WATER maps (islands, riverbed, spiral_moat, the_pond).
   FIX (cheap, do with/before item 1b): clamp group_altar_min to ≤ 60 (or the 30 default when unreachable); verify
   with idle econ on those maps (target: back to v36 levels while keeping lost_world/road_of_riches gains).
   RESULT (idle econ, rounds; v36 / v47 / clamp60 / clamp+guard-no-HURT / +BANK_W 0.5): islands 1199/2000/2000/2000/1275,
   riverbed 1260/1788/1788/1685/1778, tangle 1544/2000/2000/1998/1929, guardians 1630/1976/1976/1544/1746,
   gap 1504/1813/1813/1813/1542, bowls 1248/1557/1557/1557/1404, happy_halloween 939/1236/1236/1236/1320,
   spiral_moat 1454/1747/1747/1747/1521, the_pond 1704/1948/1820/1820/1833, lost_world 1299/710/710/710/1099,
   road_of_riches 1698/1176/1048/1048/1391. Means (128 maps): v47 1206, clamp 1206, clamp+guard 1201.
   ⇒ The clamp barely matters (hypothesis mostly wrong); exempting altar GUARDS from the HURT cost fixes guardians
   (keep it: obs_clamp_guard); BANK_W flips maps both ways (0.5 fixes islands/gap/bowls/spiral, loses lost_world/
   road_of_riches) — target choice is very sensitive per map. Treat BANK_W as a per-map-class knob, not a constant.
   FULL 128-map idle econ: obs_bw05 (clamp 60 + guards exempt from HURT + BANK_W 0.5) = 1185 vs v47 1206, clamp 1206,
   clamp+guard 1201. → FIRST IMPLEMENTATION CANDIDATE = ~/.cambc_lab/obs_bw05 (dev base dev45). Gains: 00lost_world 1299 → 710,
   00road_of_riches 1698 → 1176, 04waves, 04wheel, 04bullseye. Ranked 3v fell to 42% under v47 (00collision 5/5 →
   0/3, 02the_pond 5/6 → 0/1, 00fragment 3/5 → 0/2; gold_rush 0/2 → 2/3, homonoliths 0/1 → 3/4).
   ⇒ The kept changes help on average but break specific maps: find why (e.g. HURT cost blocks the only groups on
   islands maps; BANK_W far-altar penalty) and guard them, or cap their effect.
6. Hypothesis only: on far-spawn gem-rich maps value gems more (close losses collect 0.6× their gems).
Do NOT retry without new evidence: item copying, fight-model tweaks, contact avoidance, forced solo, retreat earlier.

## How to work (user's direction, Nov 7: observation over larger periods > continuous blind testing)
1. Update "Last check-in". Check background tasks. Apply keep/revert (ladder_test.sh does it automatically).
2. OBSERVE before changing anything: take a batch of games (latest ladder test, recent ranked games, top-team
   replays), look for a pattern that repeats across several games/maps, and write it in "Observations".
3. Only build a change that answers an observed pattern; quick local sanity (h2h ≥ 100/256 vs current dev base),
   then ONE ladder test. Record evidence → change → result in "Results log" and the principle in "Learnings".
4. Prefer fewer, evidence-backed ladder tests (quota ≈ 50 groups/6 h; each test = 9 groups). Use waiting time for
   observation at scale (all 45 games of a test, ranked games by map, death logs), not for more variants.
5. Long-term aim: a cleaner bot built around the principles below once enough of them are proven.

## Learnings (consolidated principles, each with its evidence)
- **Consumables (user asked for a breakthrough, Nov 8 09:00):** settings — altar lock 10 gold, 5 hits to break; tp
  crystal 10 gold, channel 3 rounds then flash ≤ 3 tiles (across walls/holes); kill gold 100, no passive gold,
  submit range 1, tribute 0.1 round/gem. Per game: US buy 9.6 locks / 0.6 crystals; Kungarna 2.7 locks / 6.0
  CRYSTALS; Ad Astra's (top) opponents 7.2 crystals. Our code only uses crystals on islands maps; SHORTCUT_GAIN = 0.
  dev58 = v47 + general crystal shortcuts (nav::step_towards: no enemy within 7 and holding a crystal → flash to the
  tile within 3 that saves ≥ 6 steps beyond the channel; always carry one). Idle econ on maze maps (gems by r2000):
  twisty 576 → 1000 @1868, shortcut 902 → 1000 @1440, pilgrimage 733 → 828, tangle 583 → 634, terraces 790 → 814,
  the-pack 953 → 990, stampede 800 → 640, lines 440 → 419. Full idle econ 1184 (v47 1206); h2h 122/256; crystals
  bought 9.1/game (v47 0.5). Set C: 22/45 (v47 21) → below the 26 bar, reverted. Set C has almost no maze maps,
  so measuring on maze set M (mgrunM.sh: 03twisty, 01shortcut, 00pilgrimage, 04terraces, 02the_pack × 3 teams = 15).
  Maze set M: v47 5/15, dev58 5/15 → crystals don't win real games; dropped.
- v59 = dev59 = v47 + HUNT (chase lone enemies carrying ≥ 10 gems when a teammate is within 4; kills = 100 gold +
  their gems): local 120/256; set C 23/45 (+2, below the 26 bar) → reverted.
- v60 = fight04 on set C: 16/45 (v47 21) → WORSE despite local gains (156/256 vs v32). Reverted.
- v61 = fight02 (kite + safe recall + escape dash only) on set C: 17/45 → worse, reverted. Rating 1750 (rank 221).
  Fight agent's ladder diff (135 games in fight_tmp/ladC, tools/fight_ladcmp.py): fights DID improve on the ladder
  (episode deaths 0.259 → 0.231, ranged-next-to-melee 0.142 → 0.090), but SAFE_RECALL cost ~40 champion-rounds per
  game of low-HP walking (122 → 191 rounds) for −29 dead rounds; 21 new "retreat-walk" deaths. Locally it helped only
  because our own bot hunts recallers (real teams mostly don't). Finishing on low enemies slightly worse with kiting
  (0.137 → 0.122). → fight05 = fight04 without SAFE_RECALL (local vs v32 151/256, vs dev45 115/256). Next idea: don't
  kite away from a <35% enemy already in range. v62 = fight05 on set C: running (if kept, confirm on another set).
  fight07 (= fight05 + KITE_FINISH: don't kite off a <35% enemy in range) locally no better (low-HP kill rate 0.125 =
  fight05); only worth a ladder test if fight05 does well.
- v62 = fight05: set C 19/45 → reverted. Rating 1770.3, rank 208 (11:25).
- Set-C scores of 8 candidates: 16–23 vs v47 21 → all within noise of a 45-game set. NEW: 135-game test
  (tools/ladder_all.sh: 3v + 5v + C sets; v47 = 18 + 19 + 21 = 58; keep if ≥ 65). v63 = dev60 = crystals + hunt
  (the two slightly-positive mechanisms, dev58 22 and dev59 23 on C): 19 + 11 + 23 = 53/135 → reverted.
- **Our bot is strongest as one or two big groups.** Every change that spreads it out or shifts its gold has lost on
  the ladder vs unchanged opponents: v43 avoidance 9/45, v49 dash+map 31/85, v50 Vesper claims 29/85, nova02 split
  shape 28/90 (v47 38/90). Top teams can farm solo/in pairs because their FIGHTING wins small skirmishes; ours does
  not (we die 38% of fight episodes vs their 15%, and fail to finish low-HP enemies). ⇒ The next lever is fight
  quality (finishing, escapes, focus), not organisation; organisation changes only after fights improve.
- Copying a top-team STATISTIC into our bot keeps failing: items (v42), draft (dev34), forced solo (dev37), dash rate
  (v49), Vesper gold share 0.46 (v50, the best-evidenced idea) — all worse vs unchanged opponents. Our bot's other
  behaviour (fighting, retreat, target choice) is tuned around the current setup; changing one piece breaks the fit.
  A redesign has to change the pieces together and be validated as a whole (→ NOVA).
- Matching a top-team STATISTIC (dash count, econ speed) does not by itself win: v49 dashed like top teams and farmed
  faster vs idle, yet lost 31/85 vs 38/90. Possible reasons: dash on cooldown when a fight/escape needs it; the
  BANK_W/guard changes flip map choices. If retried, split them (dash alone, map fixes alone).
What has worked (all came from replays of maps we always lose — find the FIRST moment we fall behind):
- **Cost every target in rounds, including the hidden costs.** Healing time after camp damage (v44, HURT_ROUNDS 60:
  broken_curve opening went from 64 rounds on one heavy camp to first clear at r5) and the walk to an altar
  (v46, BANK_W 1.0: 02cuts 54-gem 80-round walks). Both kept.
- **Heuristics that switch strategy by map class misfire.** "Gem-scarce" (gem_rate < 2/round → gems ×20) made us
  ignore free centre gold on gold_rush/anvil/arena/elk_arena; the opponent bought items with it and killed us.
  Tiering (×20 only below 0.5/round) = v47, kept. Check other map-class switches (GLUT, styles) the same way.
What has NOT worked (don't repeat without new evidence):
- Copying top teams' items (v42 Ad Astra openers 10/45; Kungarna Prospector/Treasure locally 43–50%) or drafts
  (Kungarna draft 29% locally — our code plays Heart/Vendetta badly). Their gains come with their whole play style.
- Fight-model tweaks: discipline (v37), burst model (v40), stand-and-fight (dev47 43% locally).
- Contact avoidance by skipping camps near enemy sightings (v43 9/45): costs more farming than it saves.
- Forced solo farming (v41 per-map, dev37 20% locally); squad-splitting more readily (dev42 45% locally).
- Camp-defence items for all + item-aware danger (v45): tie 15/45 (helped 04compass 3/3, hurt broken_curve 0/6).
Measurement facts:
- Selection bias: our 45/90-game sets are deterministic, so any change flips many single games ~randomly; keeping
  "whichever scored higher" picks up luck. v44 → v46 → v47 (15 → 18 → 19 of 45) may be partly noise — ranked v47 is
  49% at ~1700. Prefer changes with a clear mechanism AND a margin of ≥ +5 of 90; treat +1/+2 as noise.
- **Our ladder test set is biased: 36 of its 45 games are 3-a-side** (our best format: ranked 63%), while ranked is
  64% 5-a-side (425/660 games, we win 51%) and 20% 4-a-side (46%). Kept versions were tuned mostly on 3-a-side.
  → Implementation phase: build a 5-a-side set (see "Open questions") and re-measure v47 on it first.
- Local self-play and idle-econ speed do not predict the ladder (dev30 57% local → 10/45). Local = disaster filter.
- A 45-game test has sd ≈ 3 wins; games are deterministic per (map, side, opponent) — re-running gives the same
  score, so differences of 1–2 are real for THESE 45 games but may not generalise. Ranked win rate is the truth.
Ranked data (660 engine-3 ranked games, Nov 6 18h – Nov 7 18h, mostly v36):
- Opponent drafts Heart of Ruin → we win 48% (n=384) vs 58% without (n=276): Heart is our problem champion
  (matches the death analysis). Opponent Tavi → 59% vs 48%. Others within ±3%.
- By team size: 3-a-side 63%, 4-a-side 46%, 5-a-side 51%.
- By map prefix: 00 58%, 01 35% (n=68), 02 60%, 03 57%, 04 45% (n=193, 29% of all games!), adgato 54%.
  Worst maps (n≥6): 04bowls 0/6, 04horseshoes 0/6, 04braided_river 1/7, 04bridges, 04bullseye, 04orchard 1/6,
  adgato_gemcombat3 2/8, 00great_wall, 00oasis, 01shortcut 2/7. Best: 00breadcrumbs 9/9, 00collision, 04passageway.
Whole-game shape (ALL 45 v47 games, match-summary metrics + timelines; tool timeline.py — RELIABLE):
- Totals: we kill 417 / die 408; they kill 361 / die 455; champion damage 1.87M us vs 1.14M them. On aggregate we
  win the fights! But they earn 331k gold vs our 228k and do 4.4M camp damage vs our 3.5M.
- Three kinds of game: WINS (19): gold ratio us/them 1.75 at r100, 2.0+ from r150, kills +5 by r800 (we snowball).
  CLOSE LOSSES (12): we LEAD gold until r400 (1.36 at r100) but collect only ~0.6× their GEMS from r100 → we farm
  gold groups while they farm gems; kills even. BLOWOUTS (14): gold even to r150, then our income stalls
  (r150 414 → r200 472) while theirs grows; kills still even at r300, then −4.5 by r600: economy falls first,
  fights follow. Typical blowout start: 2–3 of our deaths around r150–250 (each = 60 rounds of no farming) while the
  opponent keeps buying single shards from r42 (Treasure first, then Jungle/Bruiser/Durability).
- Close losses: camp damage EQUAL to theirs (ratio ~1.0 r100–r300) but gems 0.5–0.6× and gold 1.2–1.36× → same work,
  we pick gold-rich groups, they pick gem-rich ones (our early gold weight ≈ 0.3 + 0.6 = 0.9 value per gold until
  r300). With no fights (kills 0–0) the gold lead never converts and the gem race decides. Close-loss maps are far-
  spawn, gem-rich: broken_curve (spawn dist 27, 6.0 gems/round), the_farm (44, 11.9), cuts (57, 7.3). Blowout maps
  are near-spawn, gem-poor: arena (17, 1.9), plaza (13, 4.9), packs (28, 2.8), gold_rush (26, 1.3), anvil (30, 1.2).
  Ranked (mostly v36): near+gem-rich 46% (n=133), near+gem-poor 53%, far+gem-poor 56%, far+gem-rich 58% — weaker
  support; treat "value gems more where fights are rare" as a hypothesis.
- Blowouts: camp damage only 0.65–0.7× theirs from r100 (we farm less, not just die more).
- Per champion (v47's 45 games, summary metrics): damage taken per death — our Willow ~1290, Tavi ~1640, Vesper
  ~2520, Marcella ~3780, Ryth ~4010. Tavi (all 45 games, 3-a-side pick) and Willow are CHEAP KILLS feeding the
  enemy gold/gems. Their Heart soaks ~14.8k of our damage per game + ~12k shields. Ranked: opponents drafting Tavi
  → we win 59% (Tavi is weak); Heart → 48%.
- **Our back line is starved and dies cheap (77 older ranked games, mostly 5-a-side, rk/*.json):**
  our Vesper 3.97 deaths/g, 1.71 kills/g, 5.6k dmg/g, dies to 1633 dmg, 866 gold/g — THEIR Vesper 1.94 deaths,
  3.08 kills, 10.3k dmg, dies to 4268, 2178 gold/g (a fed carry). Our Willow 4.35 deaths, 0.77 kills, dies to 797,
  558 gold; our Tavi 4.14 deaths, dies to 1523, 938 gold. Our Marcella (2249 gold, 4.55 kills) and Ryth are fine.
  Top teams funnel gold to Vesper (earlier note: ~3500 gold, 0.6 deaths/game). ⇒ Strongest 5-a-side lever so far:
  feed and protect the ranged carry (claim priority for Vesper; keep Vesper/Tavi/Willow behind the melee; maybe
  replace Willow). The gold-funnel idea was only ever tested bundled with item changes (dev30), never alone.
  MECHANISM (bot.cpp Bot::capture/claiming): "one claimer is enough" — the first ally on a cleared group's spawn
  tiles takes the whole reward (gold AND gems); others stay off. That is almost always the melee champion next to
  the camp (Marcella/Ryth), so ranged champions never get paid. dev30's CARRY_CLAIMS (non-carries defer the claim to
  a healthy carry within 5 tiles) was the part that helped locally (dev29 items only 48% → dev30 +claims 57% vs v36),
  but it reached the ladder only bundled with the failed item openers. → candidate: CARRY_CLAIMS alone on v47.
  Gold share per champion (goldshare.py, share of team earned gold): EVERY other team makes Vesper the carry —
  Kungarna Vesper 0.39–0.40, Ad Astra Vesper 0.44–0.47 (Prism 0.39–0.48 when drafted), our mid opponents Vesper 0.43,
  older ranked opponents Vesper 0.28. US: Marcella 0.37–0.43, Ryth 0.29–0.35, Vesper 0.13–0.17, Tavi 0.16–0.20,
  Willow 0.09 (Marcella's passive +2 gold per tributed gem inflates hers). Our carry is Marcella by accident.
  Vesper position (carrypos.py, r50–800, away from spawn): nearest ally median — Ad Astra 6, Kungarna 4, mid opps 6,
  US 2; nearest enemy — Ad Astra 18, Kungarna 11, us 12; enemy within 5 tiles — AA 10%, Kung 22%, us 16%.
  ⇒ Top teams' Vesper farms on its own (4–6 tiles from allies, same enemy exposure) and takes its own rewards; ours is
  glued to the squad while a melee champion claims. Candidate: Vesper as its own farming unit that claims its clears.
- **Everyone else farms mostly SOLO (units.py: components of champions within 3 tiles, r50–600, away from spawn):**
  5v5 — Ad Astra 3.3 units, 58% of champions alone; Kungarna 3.3 / 58%; mid opponents 3.3 / 63%; US 2.7 / 38%.
  3v3 — mid opponents 2.2 units / 69% alone; US 1.9 / 52%. Their Vesper is alone 59–69% of the time.
  Their champions are MORE isolated than ours yet die less → isolation itself is not what kills us; weak/unfed
  champions and our fight handling are. Our solo mode is still weak on v47 code (idle econ, mean rounds to 1000):
  K≤2 dynamic (v47) 1206, fixed 3 squads 1293, fixed solo 1434 (solo faster on only 31/128 maps; several hit 2000).
  WHY (TALLY, solo on 04dunes vs idle): after ~r250 every champion is in "gather" (no valid target, waiting for
  respawns) — 504–606 of 750 rounds! Once small groups are cleared, every remaining group fails the single-champion
  danger cap, and forced solo never merges. Opponents' 3.3 units of 5 = solo on small groups + pairs for big ones.
  ⇒ Rewrite principle: per-champion target choice (solo what you can), and when nothing is soloable JOIN the nearest
  ally's target instead of waiting; the carry claims its own clears.
  Tested locally (JOIN_IDLE: no target → farm the nearest other squad's target): idle econ solo 1434 → 1368,
  3 squads 1293 → 1287, K≤2 1206 → 1205. Joining helps solo but squads still farm faster vs an idle opponent, so
  splitting alone is not the lever; top teams' solo success needs their targeting/claims too (or econ-vs-idle
  misses what matters: contested groups, deaths, carry gold).
- Top teams (econstyle.py, motion.py; Kungarna 6 + Ad Astra 19 engine-3 games): time split is the SAME as ours —
  ~35–38% of champion-rounds standing still away from spawn, ~55–59% moving, for us, mid teams, Kungarna, Ad Astra.
  Per champion, camp damage by r300: us 3920, our opponents 4520, Kungarna 4700, Ad Astra 5450 (5-a-side vs top
  teams) → we farm ~15–25% less per champion, not 2×. Gem/gold choice is NOT a top-team trait (Kungarna 15 gems per
  100 gold, Ad Astra 8, us 8, mid opponents 12–15).
- Local TALLY (dev45, 8 maps, first 250 rounds): champions spend 40–156 rounds "walk" + 20–70 "approach" and only
  15–52 "fight" on camps; on 02cuts walk = 138–156 of 250 for every champion.
- ⇒ The game is decided by r100–r250 economy: gem collection rate (close losses) and early deaths (blowouts).
CAUTION: the replay-based tools below used only the ~half of games whose replays downloaded (429s), and the replay
death detector under-counts enemy deaths; prefer summary metrics for totals.
Fight conversion (v47's 45 games; tools fightctx.py, focus.py, episodes.py):
- Equal damage, half the kills: we dealt 553k damage to champions for 96 kills (5758/kill); they dealt 527k for 198
  kills (2661/kill). Focus is NOT the difference (share of a round's damage on the top victim: 0.66 us, 0.69 them).
- Fight episodes (damage taken with an enemy within 4 until death or 6 calm rounds): we die in 38% (204/534), they
  in 15% (99/665). After losing ≥40% HP we die 73%, they 44%. They escape from <35% HP 297 times, from <20% 151
  times (us 148 / 81).
- Pursuit is equal (attackers within 3 tiles for 3.3 vs 3.2 of the 5 rounds after the victim drops below 35%), but
  when we stay on a low enemy 5/5 rounds it dies only 24%; when they stay on us 1/5 rounds we die 66%.
  ⇒ the gap is FINISHING: low-HP enemies next to our champions survive; ours die. Causes to check: their stuns
    (Ryth/Tavi) locking our fleeing champions; our champions near a low enemy not attacking it (farming a camp,
    retreating, or FLEE_FIRST); their heal/shield (Heart).
- Their deaths happen mostly when outnumbered (42%); 44% of OUR deaths happen when NOT outnumbered.
- Map structure (93 maps profiled, ranked win rate): spawn distance <25 → 46%, 25–35 52%, 35–45 56%, ≥45 63%:
  the sooner the enemy is in reach, the worse we do. Single-altar maps 63% (fine). (`mapinfo/all.txt`)
Why we lose (current picture):
- Deaths decide most losses. v47's 45 games: 213 deaths, 155 (73%) while RETREATING. Death context (deathctx.py):
  - They are NOT slow bleeds: median HP 15 rounds before death = 1425 (142/155 had ≥600); first enemy within 4 tiles
    a median ~13 rounds before death at ~full HP. Full HP → dead in ~13 rounds of contact; LOW_HP 35% is far too late.
  - They are ISOLATED: nearest ally median 5 tiles (quartiles 2/5/10); 86/155 had no ally within 4. Usually 1–2
    enemies within 4. More on our OWN half (89) than theirs (66): enemies come into our half to hunt.
  - Killers nearby (champion_type ids): Vendetta 73, Vesper 67, Heart of Ruin 65, Ryth 17, Marcella only 6.
    So it is melee bruisers (Vendetta, Heart with dash) + Vesper, not Marcella burst.
  - 48 of the 58 "0 enemies seen" deaths had an enemy within 4 tiles in the replay: vision is PER CHAMPION (allies
    don't share sight; bushes hide units), so a lone champion is blind to an approaching bruiser.
  - Engage deaths (40) look different: nearest ally median 2 tiles.
- Opponents reach contested centres first (gold_rush: enemy r18, us r50) and win the fight there.
- Strong opponents spread champions over the map (5 places at once on 02cuts) and re-take cheap/free groups on
  respawn near altars; we move as one blob to big groups.

## Observations (case studies; tools in brackets)
- 01broken_curve vs V4.1 (replay positions): v36 spent r12–r76 on ONE heavy corner group (g10 at 35,5) at 400 HP each
  with a recall in between, for 26 gems, while V4.1's Vesper farmed the centre next to the central altar. v47: first
  clear r5, close losses (832–1006 vs E=mc^2); the remaining gap builds after r300.
- 02cuts: 4 diagonal altars (14,14) (25,25) (34,34) (45,45), no guards. Opponent spreads 5 champions from r20 and
  re-takes the free centre group g22 (29,29; 28 gems, 480 hp, 0 dps) every respawn; we blob onto g9 (5820 hp, 100 dps).
- 01gold_rush: centre has 4 free gold groups (0 dps, 600 hp) + 4 tiny ones; edge groups have 11 gems. Gem rate 1.32.
- Kungarna vs V4.1 (behav.py): enemy within 6 tiles 6–10% of the time vs our 22–25%; they carry 15–19 gems vs our 5;
  0.67 home trips/100 rounds vs our 1.0 (maps differ: confound).
- Top-3 teams vs us: go home ~2× as often while healthy; farm solo cheap clusters across the map (45 clears by r300
  vs our 5 on 00road_of_riches).
- Ad Astra (UK #2): two climbs Oct 27–28 and Oct 29–Nov 2 coincided with Prospector + more clears (47→55→72/1000r).
- Kungarna (UK, 2385): jumps Nov 3 and Nov 5 were item changes (Prospector→Jungle→…; Prospector→Treasure→Bruiser).

## Open questions / next observations (pick from here)
- ANSWERED (see Learnings): retreat deaths = isolated champions caught at full HP by Vendetta/Heart/Vesper;
  retreating earlier (v48) did not help → the problem is fight conversion (finishing), see below.
- NEXT (higher priority): close losses — why do we collect 0.6× their gems from r100 while ahead on gold?
  (target values: GOLD_WEIGHT 0.3, EARLY_GOLD_W 0.6 until r300; which groups do we pick vs them?)
- Blowouts — what causes our deaths at r150–250 (contesting their side? their groups?), and what do they buy?
- Finishing (tools/finish.py, v47's games, victim < 35% HP with an attacker within 3): we deal NO damage in 60% of
  those rounds (they 51%), mean HP lost/round 64 vs 78, kill per round 3.4% vs 8.8% (partly our low-max-HP Tavi/
  Willow). Real but moderate gap; no single bug found.
- Then: when our champion is within 3 tiles of a <35% enemy, what is it doing (attacking it? a camp? retreating?)
  and how much damage does the enemy take per round vs what our <35% champions take? Stun usage by each team?
- What were the isolated champions doing before contact (banking trip, shopping walk, solo target, recall)?
- How do opponents' champions avoid this — are they closer together, or do they just pick fights?
- Do bushes on the death maps line up with the "unseen" deaths?
- broken_curve/packs/gold_rush after r300: what happens when the gap opens (fights, banking, positions)?
- v47 ranked games are being collected to ~/.cambc_lab/rk47 (tools/rkcollect.py every 30 min until 00:40; sides.txt
  = matchid map side result opponent picks). Analyse at the end: wins/close/blowout by team size, deaths by champion.
- Map-class switches still in the code (GLUT_RATIO, styles): do they misfire like the gem-scarce one did?

- Proposed 5-a-side ladder test set (ranked win rate, n): {04braided_river 14%/7, 04bridges 17%/6, 04orchard 17%/6,
  00great_wall 29%/7, 02famine 33%/9}, {00monolith 33%/9, 04sunburst 40%/10, 03twisty 44%/9, 04crossroads 43%/7,
  02cuts 43%/7}, {00happy_halloween 60%/10, 03the_front 60%/10, 00split 70%/10, 00eight 73%/11, 04sanctum 50%/8}.
  Worst 5-a-side overall: 01corridors 0/5, 04braided_river, 04bridges, 04bullseye, 04orchard, 01elk_arena, 04gap.

## Results log (ladder: 45 games vs the three mid teams; local = h2h 256 games)
| version | change | result |
|---|---|---|
| v32 | old baseline | 6/30 (sets 1–2) |
| v36 (v20) | retreat fix + walk-home shopping | 13/45 (re-measured 13/45) |
| v37 / v38 / v39 | fight discipline / fewer bank trips / gold-first opening | 6/30, 6/30, 4/30 |
| v40 | burst-aware fight model | 10/45 |
| v41 | per-map solo/squad styles | 3/10 on switched maps (v36 6/15) |
| v42 = dev30 | Ad Astra item openers + carries claim | local 57%; 10/45 |
| v43 = dev35 | skip camps near recent enemy sightings | 9/45 |
| dev31–34, 36, 37 | Kungarna items / draft, more avoidance, solo+items | local 20–50% → not submitted |
| **v44 = dev39** | HURT_ROUNDS 60: healing-time cost in camp scoring | local 56%; **15/45 kept** |
| v45 = dev40 | + Jungle/Dur/Monster for all + item-aware danger | 15/45 tie → reverted |
| **v46 = dev41** | + BANK_W 1.0: walk to altar in camp scoring | local 48%; **18/45 kept** |
| **v47 = dev45** | + gem-scarce ×20 only below 0.5 gems/round (else ×3) | local 52%; **19/45 kept** |
| dev42 / dev47 | SPLIT_GAIN 1.0 / STAND_RATIO 1.0 | local 45% / 43% → dropped |
| v49 = dev50 | v47 + TRAVEL_DASH + guards exempt from HURT + BANK_W 0.5 + altar distance cap 60 | local 142/256, econ 1184; LADDER 16/45 + 15/40 = 31/85 vs 38/90 (worse) |
| v50 = dev52 | v47 + VESPER_CLAIMS (Vesper share 0.13 → 0.46 locally) | LADDER 16/45 + 13/40 = 29/85 (worse) |
| v51 = obs_clamp_guard | v47 + altar guards exempt from HURT + altar distance cap 60 | LADDER 18 + 18 = 36/90 (vs stale 38) |
| v52 = dev53 | v47 + Kungarna opener (Prospector → Treasure for all) | LADDER 14 + 11/40 = 25/85 (worse) |
| v53 = dev54 | v47 + Varg instead of Willow (5v draft) | local 48%; LADDER 16 + 16 = 32/90 (worse) |
| v54 = dev57 | v47 + chase further (CHASE_SLACK 4, wounded within 8) | local 41%; LADDER 16 + 14 = 30/90 |
| set C re-tests | dev50 / dev52 / obs_clamp_guard on fresh set C (v47 21/45) | 17 / 17 / 20 of 45 |
| v59 = dev59 | v47 + hunt lone gem carriers | set C 23/45 (v47 21) — below bar |
| v60 = fight04 | agent's fight module (6 changes) | local vs v32 156 (v47 131); set C 16/45 (worse) |
| v61 = fight02 | kite + safe recall + escape dash | set C 17/45 (worse) |
| v62 = fight05 | fight04 without SAFE_RECALL | set C 19/45 |
| v66 = combo01 | econ01 + fight08 + macro03E | set H 12/45 (v47 8) |
| **v67 = econ04** | combo01 + Focus-heavy Marcella | **H 13 + C 27 = 40/90 (v47 29) → KEPT, live as v52** |
| v68 = fight13 | econ04 + proactive top-up healing | H 12 + C 25 = 37/90 (worse) |
| v69 = macro06 | econ04 + feeding brake + islands fuel guard | H 8 + C 22 = 30/90 (much worse) |
| v70 = adapt03 | econ04 + Heart shield/damage model | H 10 + C 25 = 35/90 (worse) |
| nova01–04 | NOVA shape / +claims / +Heart-Vendetta draft / +calm dash all | local 44 / 46.5 / 30 / 45% |
| nova02 | NOVA: Vesper solo carry + 2 pairs + Vesper claims + carry dash/early retreat | local 46.5%; LADDER 15+13 = 28/90 (worse) |
| v48 = dev46 | retreat at 50% HP near enemy champions | local 52%; 19/45 tie → reverted |
Observation games vs 2000-rated teams (we were fed fish, Gamblr) on our worst maps: 0/10.

## Tools and operations
- CLI `~/.cambc_lab/venv5/bin/cambc` (cambc 5.0.0, engine 3.0.0). Login lasts 7 days (`cambc login`).
- **Ladder test (one command):** `SKIP_LOCAL=1 zsh ~/.cambc_lab/tools/ladder_test.sh devNN vNN "note"` — (optional local
  h2h vs $LOCAL_BASE) → copy to bots/phalanx_vNN → submit → wait Live (60 min) → 45 games → keep if > base, else
  revert to keep version and resync. Defaults: base 19, keep 31, kdir phalanx_v47, LOCAL_BASE dev45 — UPDATE after a keep.
  `V_GIVEN=N` reuses an existing submission. Don't edit the script while it runs (zsh reads it lazily).
- LAPTOP HEAT (user, Nov 9 20:05): local runs at --jobs 10 maxed all 10 cores. All local tools now use --jobs 4;
  run at most one local job at a time.
- DISK (Nov 9 20:15): the lab had grown to 47 GB (disk 93% full). Deleted runs5 folders older than a day and fight_tmp;
  h2h.py / macro_h2h.py / econ.py / h2h3.py now drop 'events' and 'heatmaps' from result json after each run
  (keeps metrics + timelines). Clean old runs5 folders periodically.
- Gotchas: after a submit the mode can read "follow newest ready version" (the new build goes live by itself);
  `submissions activate N` pins again. Queued set-C tests must STOP after a KEEP (later reverts would undo it).
- `cambc submit` must run inside ~/Downloads/camb_battlecode (needs cambc.toml); notes ≤ 80 chars; server
  builds can queue 20+ min; after any activate, poll until `Live: N`; 429 = slow down.
- `mgrun.sh tag` (9 groups: 3 map sets × E=mc^2 21b63196…, V4.1 Flash fd795986…, ChatGTP 87493851…;
  sets {00dunes,02cuts,01broken_curve,04lines,00packs}, {01broken_curve,00packs,01gold_rush,02arena,00anvil},
  {00breadcrumbs,01the_farm,04compass,04plaza,03honeycomb}, sides a,b,a,b,a); `score.py mg_tag.out`.
  Group ids: `mg_tag.txt` (json lines) / `mg_vNN.groups`.
- Local: `h2h.py label bot opp` (256 games, results in runs5/label_opp/*.json); `econ.py label bot $M` with
  `M=$(ls ws5/maps | sed 's/\.[^.]*$//' | sort -u)`; `variant.sh name base 'sed bot.cpp' 'sed world.h' 'sed world.cpp'`.
- Observation: `cleartl.py groupid index` (clears/banks/deaths timeline), `deathcause.py`, `goldtl.py`, `farmcmp.py`;
  `obs.sh outdir groupids` / `obsm.sh outdir side:map:matchid…` (replay dump + behav.py profile);
  `deathctx.py zip side …` (each of our deaths: HP 15 rounds before, contact timing, enemies/allies within 4,
  nearest ally, map half, enemy kinds; v47 data in ~/.cambc_lab/v47died/ with sides.txt + ctx.txt);
  `timeline.py sides.txt` (wins/close/blowout medians of gold, gems, kills at r50…800 from summaries);
  `econstyle.py` (gems/gold per camp damage, per-champion camp damage; our games or top-team summaries),
  `motion.py R label:zip:side…` (share of rounds still/moving/at spawn), TALLY build = dbgtally (bot activity counts);
  `casts.js wasm replay` (ability casts per champion and slot; aggregated in v47died/casts.txt);
  `spend.py`, `banking.py` (gold spent/held; bank events and gems per bank, same arguments as goldshare.py);
  `units.py label:zip:side…` (farming units per team, alone share by champion);
  `carrypos.py label:zip:side…` (carry distance to allies/enemies, screening; CARRY=type id);
  `goldshare.py` (each champion's share of team gold, us vs opponents vs top teams);
  `fightctx.py`/`focus.py`/`episodes.py zip side …` (both teams' deaths, damage per kill, fight episodes, pursuit);
  `replay_dump.js wasm replay 2` → per-round champion lines `C idx team kind x y hp gems`;
  engine-2 replays (before Nov 4) need the v2_0_0 wasm. Our DIED lines: download match zip,
  `unzip -p zip team-X.stdout | tr -d '\000' | strings | grep DIED` (fields: round, champ, mode, pos, hp, enemies…).
  Map internals: copy dev to dbgNN, printf World at init, `cambc run --rounds 3`, read team-a.stdout with `strings`.
- Data: `~/.cambc_lab/adastra/`, `~/.cambc_lab/kung/` (ranked.jsonl + summaries), `climb.py`, `jump.py`, `rksum.py`.
  Ladder: `cambc ladder --page P --limit 50`; our ranked games: `cambc matches list --team me --kind ranked --json`.

## Mechanics cheat sheet (verified)
- Champion base stats (engine 3, printed via dbgstats; hp+growth, str+growth, range): Ryth 1200+100, 45+4.5, r1 ·
  Vesper 1000+100, 35+5, r3 · Vendetta 900+100, 65+6, r1 · Cassi 600+65, 30+3, r3 · Issac 900+80, 45+3, r1 ·
  Stellar 650+65, 30+3.5, r3 · Prism 700+65, 35+2.5, r3 · Marcella 1200+100, 40+3.5, r1 · Willow 700+50, 55+3, r3 ·
  Ren 900+100, 35+5, r1 · Varg 1000+100, 50+5, r1 · Tavi 900+75, 45+5, r3 · Heart 1000+100, 30+3, r1 (+shields).
  All: move 1, vision 8 (per champion, not shared), camp damage multiplier 1.0.
- Camp ranges: wolf 3, elk 4, golem 4, monolith 5; camps never move, hit the nearest champion in range.
- Champion ranges: melee 1; Tavi/Vesper/Willow etc. 3. Abilities (mod × focus, cd 8): Marcella damage ×2; Ryth area
  stun ×1; Tavi stun ×1.6; Heart dash ×1.5 + shield. All shards cost 30 gold; 5 combine into an emblem.
- Item passives (emblem only): Monster +15% camp damage · Durability −10% all damage · Treasure +10% gold · Jungle −30%
  camp damage · Home recall 50% shorter · Tribute banking 33% shorter · Capture in 3 turns (default 5) · Prospector +25%
  of cluster rewards · Hunter +50 gold per kill · Skirmish +10% vs champions · Collectors +1%/emblem family ·
  Power = 6 stat points, Bruiser = 5+1+1 · Greed = 25 shards → 40 points. Owned items: `me.item_count()[item_type]`.
- Respawn 60 rounds, recall 5. Camp rewards split among champions on the group's spawn tiles; gems carried transfer
  to the killer; no gem passing between allies. Marcella gains 2 gold per gem she tributes.
- Match list `characters` use the DRAFT enum: 0 Ryth, 1 Vesper, 2 Vendetta, 3 Cassi+Issac, 4 Stellar, 5 Prism,
  6 Marcella, 7 Willow, 8 Ren, 9 Varg, 10 Tavi, 11 Heart.
- Shared memory 8 bytes (comms.h): ≤3 squads with enemy reports, ≤5 without; group index must be < 63.
- Group value (world.cpp): gems + 0.3 × gold; gem-scarce/gold-glut maps rescale. Danger = dps × hp (no items).
- More: memory files game-mechanics.md, ladder-insights.md in the Claude project memory.
