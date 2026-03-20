# DefenseInDepth — Implementation Plan

## Overview

Four phased deliverables, each independently playable and testable.
Each phase builds on the previous but does not break it.

---

## Phase 1 — Attacker Budget System ✅ COMPLETE

**Goal:** Replace infinite AI spawning with a per-zone points budget.
Each spawn costs points. Zone ends via budget exhaustion + AI cleared,
not just timer expiry or defender death.

### Files created/modified
| File | Status |
|---|---|
| `Scripts/Game/DiD/AFM_DiDAttackerBudget.c` | NEW — budget state class |
| `Scripts/Game/DiD/AFM_DiDZoneComponent.c` | MODIFIED — `FINISHED_REPELLED` state, budget attributes, rollover API |
| `Scripts/Game/DiD/Spawners/AFM_DiDSpawnerComponent.c` | MODIFIED — `m_iPointCostPerUnit`, `CanSpendBudget()`, budget consumption |
| `Scripts/Game/DiD/Spawners/AFM_DiDMechanizedSpawnerComponent.c` | MODIFIED — immediate budget consume per vehicle |
| `Scripts/Game/DiD/AFM_DiDZoneSystem.c` | MODIFIED — `m_OnZoneRepelled`, budget rollover in `ProgressToNextZone()` |
| `Scripts/Game/DiD/AFM_GameModeDiD.c` | MODIFIED — `OnZoneRepelled` handler, `RPC_DoZoneRepelled` hint |

### Key attributes (tunable in editor)
- `AFM_DiDZoneComponent.m_iPointsBudget` — 0 disables system; 80–150 typical
- `AFM_DiDZoneComponent.m_fBudgetRolloverFraction` — 0.5 default
- `AFM_DiDSpawnerComponent.m_iPointCostPerUnit` — 1 pt/soldier, higher for vehicles

---

## Phase 2 — Attacker Director ✅ COMPLETE

**Goal:** Replace autonomous per-spawner ticking with a central decision-maker
that coordinates all spawners, tracks attack phases, and spends budget intelligently.

### Files created
| File | Purpose |
|---|---|
| `Scripts/Game/DiD/AFM_DiDAttackPhase.c` | `EAFMAttackPhase` enum (PROBE/ASSAULT/FINAL) + `AFM_DiDBattlefieldState` class |
| `Scripts/Game/DiD/AFM_DiDSpawnRequest.c` | Scored spawn candidate (spawner ref + score + cost) |
| `Scripts/Game/DiD/AFM_DiDAttackerDirector.c` | Director GenericEntity: 25s decision cycle, weighted random spawner selection |

### Files modified
| File | Change |
|---|---|
| `Scripts/Game/DiD/AFM_DiDZoneComponent.c` | Detects `AFM_DiDAttackerDirector` child in `LateInit()`; delegates `Process()`/`Cleanup()`/`GetActiveAICount()` to director; added `GetTotalDefenseSeconds()` getter |
| `Scripts/Game/DiD/Spawners/AFM_DiDSpawnerComponent.c` | Added `ScoreRequest()` (base: 1.0), `TriggerSpawn()`, `CanSpawnNow()`, `GetPointCostPerUnit()` |
| `Scripts/Game/DiD/Spawners/AFM_DiDInfantrySpawnerComponent.c` | `ScoreRequest()` override — boosts when outnumbered/FINAL phase, penalises when saturated |
| `Scripts/Game/DiD/Spawners/AFM_DiDMechanizedSpawnerComponent.c` | `ScoreRequest()` override — off during PROBE, boosted in FINAL/high budget/many defenders |

### Architecture
```
AFM_DiDZoneEntity
├── PolylineShapeEntity
├── AFM_PlayerSpawnPointEntity
└── AFM_DiDAttackerDirector          ← GenericEntity child (director mode)
    ├── AFM_DiDInfantrySpawnerComponent
    └── AFM_DiDMechanizedSpawnerComponent
```

Director mode is opt-in — zones without an `AFM_DiDAttackerDirector` child continue
to use the legacy per-spawner `Process()` path unchanged.

### Decision cycle (every 25s)
1. Snapshot: defender count, AI in zone, budget ratio, time ratio → derive phase
2. Each spawner: `ScoreRequest(state)` → float
3. Filter: score > 0, spawner off cooldown (`CanSpawnNow`), budget covers cost
4. Weighted random pick from top 3 by score
5. `TriggerSpawn(now)` on winner

### Attack phases (budget-driven)
| Phase | Budget remaining | Behavior |
|---|---|---|
| PROBE | > 75% | Infantry only (mechanized scores 0) |
| ASSAULT | 75%–25% | All options active |
| FINAL | < 25% | Mechanized gets +2.0 bonus, infantry +0.5 |

### Key attributes (tunable in editor)
- `AFM_DiDAttackerDirector.m_iDecisionIntervalSeconds` — 25s default
- `AFM_DiDAttackerDirector.m_fAggression` — 0.75 default (scales score of expensive options)

---

## Phase 3 — Artillery Overhaul ✅ COMPLETE

**Goal:** Mortar becomes a destroyable zone asset, not a spawner.
Fire missions cost budget, with 4 round types. Players can counter-battery.

### Files created
| File | Purpose |
|---|---|
| `Scripts/Game/DiD/Entity/AFM_ArtillerySpawnPointEntity.c` | Marker entity for valid mortar positions (transform only) |
| `Scripts/Game/DiD/AFM_DiDZoneArtillery.c` | Artillery capability: mortar lifecycle, mission selection, round type targeting |

### Files modified
| File | Change |
|---|---|
| `Scripts/Game/DiD/AFM_DiDAttackPhase.c` | Added `m_fDefenderDensity` field to `AFM_DiDBattlefieldState` |
| `Scripts/Game/DiD/AFM_DiDAttackerDirector.c` | Detects `AFM_DiDZoneArtillery` child; `CheckMortarAlive()` every second; artillery evaluated in `RunDecisionCycle()`; `GetActiveAICount()` includes mortar crew; `Cleanup()` includes artillery |
| `Scripts/Game/DiD/AFM_DiDZoneComponent.c` | Scans for `AFM_ArtillerySpawnPointEntity` children; deferred director `Init()` (after spawn points collected); `GetArtillerySpawnPoints()` getter |
| `Scripts/Game/DiD/AFM_GameModeDiD.c` | `NotifyMortarDestroyed()` server method + `RPC_DoMortarDestroyed()` broadcast hint |

### Zone hierarchy
```
AFM_DiDZoneEntity
├── AFM_ArtillerySpawnPointEntity   ← 1..N (no artillery if absent)
├── AFM_ArtillerySpawnPointEntity
├── AFM_DiDAttackerDirector
│   ├── AFM_DiDInfantrySpawnerComponent
│   ├── AFM_DiDMechanizedSpawnerComponent
│   └── AFM_DiDZoneArtillery        ← artillery capability child of director
```

### Mortar lifecycle
- **Zone activates** → `AFM_DiDZoneArtillery.Initialize()` → `SpawnMortarTeam()` [FREE]
- **Director cycle** → `ScoreArtilleryMission()` → `TriggerMission()` → budget consumed, waypoint assigned
- **Destruction polling** → `CheckMortarAlive()` called every second → `HandleMortarDestroyed()` → broadcasts hint, nulls crew
- **Respawn** → director evaluates `ScoreRespawn()` → budget consumed → new mortar at different spawn point → NO hint to defenders

### Round types
| Type | Cost/round | Typical volley | Cooldown | Targeting |
|---|---|---|---|---|
| `HIGH_EXPLOSIVE` | 3 pts | 2–8 rounds | 4 min | Monte Carlo (defender cluster) |
| `SMOKE` | 1 pt | 4–6 rounds | 2 min | Zone centroid |
| `ILLUMINATION` | 2 pts | 2–3 rounds | 5 min | Zone centroid (night only — pending day/night API) |
| `PRACTICE` | 0 pts | 1 round | 30 sec | Best HE target, fallback |

### Respawn cost escalation
| Respawn # | Cost |
|---|---|
| 1st | 15 pts |
| 2nd | 25 pts |
| 3rd+ | Blocked (9999 pts) |

### Information asymmetry (implemented)
| Event | Defenders notified |
|---|---|
| Mortar spawns | No |
| Mortar fires mission | Yes (impacts visible) |
| Mortar destroyed | **Yes** — "Enemy mortar destroyed!" hint |
| Mortar respawns | **No** |

### Architecture notes
- `EAFMRoundType` is an internal enum — `SetAmmoType()` is intentionally NOT called (game's `SCR_EAIArtilleryAmmoType` enum values unverifiable without game source; crew fires prefab default ammo)
- `m_fDefenderDensity` = raw alive defender count (float). `m_fHEDensityThreshold` default = 2.0 (fire HE when 2+ defenders)
- `m_bIsNight` always false pending day/night API verification — illumination missions currently never fire
- `AFM_DiDMortarSpawnerComponent` kept for backward compatibility (legacy mode without director)
- Destruction detected by polling `EDamageState.DESTROYED` every second — preferred over callback subscription for cross-vehicle-type reliability

### Key attributes (tunable in editor)
- `AFM_DiDZoneArtillery.m_sMortarPrefab` — mortar vehicle prefab
- `AFM_DiDZoneArtillery.m_CrewConfig` — crew config (gunner only recommended)
- `AFM_DiDZoneArtillery.m_fHEDensityThreshold` — 2.0 default (min defenders to trigger HE)
- `AFM_DiDZoneArtillery.m_iHECooldownSeconds` — 240s (4 min)
- `AFM_DiDZoneArtillery.m_iSmokeCooldownSeconds` — 120s (2 min)
- `AFM_DiDZoneArtillery.m_iPracticeCooldownSeconds` — 30s
- `AFM_DiDZoneArtillery.m_iFirstRespawnCost` / `m_iSecondRespawnCost` — 15 / 25 pts

---

## Phase 4 — Combined Arms Sequencing

**Goal:** Director plans multi-step attacks where artillery smoke covers a ground push.
Both infantry and mechanized units can exploit a smoke screen.
The synergy is learnable by defenders — arriving impacts signal an imminent assault.

### New files
| File | Purpose |
|---|---|
| `Scripts/Game/DiD/AFM_DiDAttackSequence.c` | Named sequence types + two-step executor: fire step 1, wait delay, fire step 2 |

### Modified files
| File | Change |
|---|---|
| `Scripts/Game/DiD/AFM_DiDAttackerDirector.c` | Evaluate sequences alongside individual options each cycle; hold and execute pending sequence step 2 |

### Sequence data model
```cpp
enum EAFMSequenceType
{
    SMOKE_INFANTRY_PUSH,    // smoke → infantry wave (shorter delay, cheaper)
    SMOKE_ARMOR_PUSH,       // smoke → mechanized push (longer delay, costly)
}

class AFM_DiDAttackSequence
{
    EAFMSequenceType m_eType;
    int m_iTotalCost;              // reserved upfront before step 1 fires
    WorldTimestamp m_fStep2Time;   // when to execute step 2
    bool m_bStep1Done;
}
```

The director holds at most one pending sequence at a time (`m_PendingSequence`).
If a sequence is pending, it skips normal candidate scoring and waits for `m_fStep2Time`.

### Defined sequences
| Sequence | Step 1 | Delay | Step 2 | Total cost | Conditions |
|---|---|---|---|---|---|
| `SMOKE_INFANTRY_PUSH` | SMOKE mission (4–6 rounds) | 20s | Infantry spawner trigger | smoke cost + infantry cost | ASSAULT+, mortar active, infantry spawner available, AI outnumbered or defenders dense |
| `SMOKE_ARMOR_PUSH` | SMOKE mission (4–6 rounds) | 35s | Mechanized spawner trigger | smoke cost + vehicle cost | ASSAULT+, mortar active, mechanized spawner available, budget > 30% |

**Delay difference rationale:** infantry exploits smoke faster (they move on foot into the cloud immediately); armor needs the smoke denser and further developed before driving through, and has longer reaction time to position.

### Scoring
Sequences compete with individual spawner options in the same scoring pool, treated as a single candidate with a combined score.

```cpp
float ScoreSequence(EAFMSequenceType type, AFM_DiDBattlefieldState state)
{
    // Both sequences require artillery smoke
    if (!m_pArtillery || !m_pArtillery.IsMortarActive())
        return 0;
    if (m_pArtillery.IsOnCooldown(EAFMRoundType.SMOKE))
        return 0;

    switch (type)
    {
        case EAFMSequenceType.SMOKE_INFANTRY_PUSH:
            // Best when AI is outnumbered inside zone — smoke lets them close the gap
            if (state.m_ePhase == EAFMAttackPhase.PROBE) return 0;
            if (!InfantrySpawnerAvailable()) return 0;
            float score = 1.2;
            if (state.m_iAICountInZone < state.m_iDefenderCount)
                score += 1.0;   // outnumbered — smoke push is high value
            if (state.m_fDefenderDensity >= 3.0)
                score += 0.5;   // dense defenders = smoke helps a lot
            return score;

        case EAFMSequenceType.SMOKE_ARMOR_PUSH:
            if (state.m_ePhase == EAFMAttackPhase.PROBE) return 0;
            if (!MechanizedSpawnerAvailable()) return 0;
            if (state.m_fBudgetRatio < 0.3) return 0;
            return 1.5 + (state.m_ePhase == EAFMAttackPhase.FINAL ? 0.5 : 0);
    }
    return 0;
}
```

### Execution flow
```
RunDecisionCycle()
  ├── IF m_PendingSequence && now >= m_fStep2Time
  │     └── ExecuteStep2()    // trigger ground push, clear pending sequence
  │
  └── ELSE (normal cycle)
        ├── Score all spawners  (existing)
        ├── Score sequences     (new)
        ├── Weighted pick across all candidates
        └── IF sequence chosen:
              budget.Reserve(totalCost)
              artillery.TriggerMission(SMOKE, now)   // step 1
              m_PendingSequence = new sequence
              m_fStep2Time = now + delay
```

### Budget reservation
Both steps' costs are deducted from the budget before step 1 fires — preventing a race condition where another decision cycle spends the budget before step 2 can execute. If budget becomes insufficient between reservation and step 2 (e.g. a concurrent artillery respawn consumed it), step 2 fires anyway (cost already reserved).

### Defender experience
The sequence is intentionally learnable:
- Smoke impacts → experienced defenders recognize imminent assault, relay on comms
- Ground units arrive ~20–35s later through the smoke
- Counter: suppress the smoke landing zone before units enter, or fall back

Defenders who learn the pattern gain an advantage; the pattern still works because smoke genuinely degrades defender accuracy, making it effective even when expected.

---

## Cross-cutting concerns

### Player count scaling (all phases)
`AFM_DiDAttackerBudget` accepts a `ScaleForPlayerCount(int playerCount, int targetCount)` call
from the zone system after game start. Budget scales linearly:
`scaledBudget = baseBudget * max(0.5, playerCount / targetCount)`

Prevents 3-player sessions from facing 20-player-designed pressure.

### HUD changes summary
| Zone type | Left score | Right score | Timer |
|---|---|---|---|
| Regular zone | Defenders alive | **Attacker budget remaining** | Defense countdown |
| Wave zone | Defenders alive | Wave tickets remaining (existing) | Next spawn / transition |

---

## Dependencies

```
Phase 1 (Budget) ✅
    └── Phase 2 (Director) ✅
            ├── Phase 3 (Artillery) ✅
            └── Phase 4 (Sequences)
                  └── requires Phase 3 (mortar provides smoke step 1)
```

---

## Out of scope (tracked in GAMEPLAY_IDEAS.md)

- Player ticket pool per zone
- Retreat window before zone failure
- Prepare phase activities (fortifications, ammo cache destruction)
- Zone type modifiers (ELIMINATION, ESCORT)
- Inter-zone transit phase
