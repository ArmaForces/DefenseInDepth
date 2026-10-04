# Branching stages – investigation

Goal: a route like **1 → 2a or 2b → 3**, where the choice between 2a and 2b is random or follows a
condition. Written 2026-10-01 from reading the zone system as it stands. **Investigation only – nothing
implemented.**

---

## How progression works today

Zones register themselves into `AFM_DiDZoneSystem` keyed by an authored index
(`AFM_DiDZoneSystem.c:86-99`), and progression is arithmetic:

```c
newZoneIndex = m_ActiveZone.GetZoneIndex() + 1;        // :352
if (!m_aZones.Find(newZoneIndex, nextZone) || !nextZone)
    → all zones completed, match over                  // :356-364
```

So "the next stage" means "the zone numbered one higher", and the end of the route is "no zone at the next
number". Three things lean on that:

| Piece | Where | Assumption |
| --- | --- | --- |
| `ValidateZoneIndices` | `:262-277` | Indices must run `start … start+count-1` with **no gaps**, and says so as an error |
| `ReportStrandedZones` | `:282-292` | Anything numbered above a missing index is a mistake worth shouting about |
| `GetMaxZoneIndex` | `:429-432` | Route length **is** the number of registered zones – this is the `/3` in the HUD's `Zone 2/3` |

Everything else is already branch-agnostic, which is the good news:

- A zone owns its content. Spawners, props, supply cache, spawn point and polyline are found by scanning its
  own children (`AFM_DiDZoneComponent.c:160-190`), so an unplayed zone simply never activates.
- Carry-over is read from the outgoing zone and handed to whichever zone is activated next – tickets at
  `AFM_DiDZoneSystem.c:349-353`, supplies through `AFM_DiDSupplies.TakeCarryOver` / `SeedZone`.
- Players are moved to the active zone's own spawn point, props are handed to the defending side on
  activation, and the stats only count stages survived.
- Nothing draws zones on the map. The polyline is used for point-in-polygon tests
  (`AFM_DiDZoneComponent.c:262-276`), not for a marker, so an unchosen branch is not visible to players.

**Consequence:** branching is a change to *one decision* – which zone is activated next – plus the three
index assumptions above. It is not a change to how a stage runs.

---

## What breaks if you just add a second zone 2

Authoring 2a and 2b both as index 2 fails at registration: `RegisterZone` rejects a duplicate index outright
(`:92-96`). Numbering them 2 and 3 with the final stage at 4 "works" in the sense that the match plays
1 → 2 → 3 → 4: both branches get played, one after the other, and `ValidateZoneIndices` is happy because the
indices are contiguous. That is the trap worth naming – the failure is silent and looks like a long mission.

---

## Three ways to do it

### A. Stage number plus variant

Each zone keeps an authored **stage number** and gains a **variant key** (empty for unbranched stages).
Progression asks for all zones at `stage + 1` and picks one.

- Smallest change to the mental model: "stage 2 has two variants".
- `m_aZones` becomes `map<int, ref array<AFM_DiDZoneComponent>>`, or stays flat with a stage lookup built at
  validation time.
- Route length is the count of distinct stage numbers – the HUD keeps working.
- Cannot express "2a leads to 3a, 2b leads to 3b" without further rules.

### B. Explicit successors

Each zone lists the zones that may follow it, by index, with a weight and an optional condition.

- Expresses any graph: convergent, divergent, dead ends, shortcuts, loops if you really want one.
- Deletes the arithmetic entirely; `ValidateZoneIndices` becomes a reachability check from the starting zone,
  which is a better check than contiguity.
- Route length is no longer a constant. The HUD would show the longest path, or the progress along the chosen
  route, or stop showing a total.
- Authoring is per-zone and easy to get wrong in a way only a graph walk catches.

### C. Route config

A config file lists the route as an ordered list of stage groups, each group naming candidate zone indices
and a selection rule. The zone system follows the config rather than any authored order.

- The route lives in one readable place, and a scenario can carry a different one through
  `AFM_DiDScenarioSettings` exactly as the side configs do.
- Fits what the mod already does: zone timings are overridden per index from the mission header
  (`AFM_DiDPhaseSettings.c:143-147`).
- Two places to keep in step: the world's zones and the config that routes them.

**Recommendation: A for the authoring model, with the selection rule written as its own class so it can be
shared.** It is the shape the mission actually needs – a few stages, one or two with alternatives – and it
keeps the index-based scenario overrides working unchanged. B is the right answer only if branches need to
lead to *different* later stages, and C is worth adding later if routes start wanting to differ per scenario
rather than per world.

---

## Choosing the variant

One class, one method – `bool CanBeChosen(AFM_DiDZoneComponent previousZone)` plus a weight – with
implementations picked per zone in the Workbench:

| Rule | Reads | Example use |
| --- | --- | --- |
| Random weight | – | 2a and 2b at 50/50, or 70/30 to favour the better-built one |
| Previous stage outcome | `GetZoneState()` → `FINISHED_HELD` / `FINISHED_FAILED` | Lost stage 1 → the gentler 2b, so a bad start is not a death spiral |
| Defenders left | `GetDefenderCount()` | Few players standing → the smaller zone |
| Supplies left | `AFM_DiDSupplies.GetStored()` before carry-over | Rich team → the expensive-to-hold variant |
| Player count | `PlayerManager.GetPlayerCount()` | Pick the zone sized for the lobby |
| Scenario setting | `AFM_DiDScenarioSettings` | A scenario pins the route for a scripted session |
| Admin choice | chat command | Testing, and a deliberate pick on a public server |

Selection must happen **on the authority only**, once, at the moment of progression – the chosen zone then
activates and everything already replicated about "the active zone" carries the news. Clients need no new
state.

A ticket-pool subtlety: the pool is sized when the attack starts, not at activation
(`AFM_DiDZoneComponent.c:1057-1062`), so a variant chosen for "few players left" still sizes itself correctly
if players rejoin during its prepare phase.

---

## Work involved

1. **Zone component:** add stage number and variant key, keep `GetZoneIndex` as the registration key, and add
   the selection-rule attribute. (`AFM_DiDZoneComponent.c`)
2. **Zone system:** replace the `+ 1` lookup with `SelectNextZone(currentZone)`; group zones by stage at
   validation; rewrite `ValidateZoneIndices` as "every stage number from the first to the last has at least
   one zone", and `ReportStrandedZones` as "these zones belong to no stage on the route".
   (`AFM_DiDZoneSystem.c:262-292, 340-375, 429-432`)
3. **Route length:** `GetMaxZoneIndex` returns the number of distinct stages rather than the zone count, so
   `Zone 2/3` stays honest with four zones in the world. (`AFM_GameModeDiD.c` HUD state at `:105`, `:541-566`)
4. **Logging:** every progression line currently prints an index; print `stage 2 (variant b)` instead, or the
   logs become unreadable the first time a branch is taken.
5. **Scenario overrides:** decide whether a phase override keys on zone index (works unchanged) or on stage
   number (needs a variant key too). (`AFM_DiDPhaseSettings.c:143-147`)
6. **A chat command to force a variant**, in the spirit of PS's `/adv`. Without it, testing a 30/70 branch
   means replaying the mission until the dice cooperate.

Half a day for 1–4, which is the part that makes branching work. 5 and 6 are small and worth doing at the
same time rather than later.

---

## Things to decide before building it

- **Do branches reconverge?** If 2a and 2b both lead to 3, option A is enough. If they lead to different
  stage 3s, it is option B, and that is a different afternoon.
- **Is the branch announced?** Players will notice they are somewhere new; a hint naming the stage would make
  a random route feel authored rather than buggy.
- **Does an unplayed branch cost anything?** Its props and composition are in the world from the start.
  Worth measuring on the Lamentin world before shipping a map with six branch zones; if it matters, zone
  content could be moved into layers loaded on activation, which is a much bigger change.
- **Should the choice be visible in the debriefing?** The match report would read better with "Route: 1 → 2b
  → 3" on it, and that is three lines once the stats page exists.
