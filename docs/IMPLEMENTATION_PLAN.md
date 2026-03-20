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

## Phase 3 — Artillery Overhaul

**Goal:** Mortar becomes a destroyable zone asset, not a spawner.
Fire missions cost budget, with 4 round types. Players can counter-battery.

### New files
| File | Purpose |
|---|---|
| `Scripts/Game/DiD/Entity/AFM_ArtillerySpawnPointEntity.c` | Marker entity for valid mortar positions (transform only) |
| `Scripts/Game/DiD/AFM_DiDZoneArtillery.c` | Artillery capability: mortar lifecycle, mission selection, round type targeting |

### Modified files
| File | Change |
|---|---|
| `AFM_DiDZoneComponent.c` | `LateInit()` scans for `AFM_ArtillerySpawnPointEntity` children → passes array to director |
| `AFM_DiDAttackerDirector.c` | Holds `AFM_DiDZoneArtillery` reference; includes artillery options in decision cycle |
| `AFM_DiDMortarSpawnerComponent.c` | **Removed** — replaced by `AFM_DiDZoneArtillery` |
| `AFM_GameModeDiD.c` | Add `RPC_DoMortarDestroyed()` and `RPC_DoMortarRespawned()` (no location revealed) |

### Zone hierarchy
```
AFM_DiDZoneEntity
├── AFM_ArtillerySpawnPointEntity   ← 1..N, random one used per spawn
├── AFM_ArtillerySpawnPointEntity
├── AFM_DiDAttackerDirector
│   ├── AFM_DiDInfantrySpawnerComponent
│   ├── AFM_DiDMechanizedSpawnerComponent
│   └── AFM_DiDZoneArtillery        ← artillery capability, child of director
```
No `AFM_ArtillerySpawnPointEntity` children = no artillery for this zone.

### Mortar lifecycle
```
Zone activates
  → AFM_DiDZoneArtillery.Initialize(spawnPoints[])
  → SpawnMortarTeam(RandomSpawnPoint())    [FREE — no budget cost]
  → idle until director calls a mission

Director calls mission
  → SelectRoundType(battlefieldState) → SCR_EAIArtilleryAmmoType
  → CalculateRoundCount(type, targetQuality) → int
  → cost = roundCount * GetCostPerRound(type)
  → budget.Reserve(cost)
  → SelectTargetPosition(type) → vector
  → SpawnWaypoint(targetPos, type, roundCount)
  → crew fires mission
  → cooldown[type] starts

Players destroy mortar
  → director detects: dmg.GetState() == EDamageState.DESTROYED
  → OnMortarDestroyed():
      broadcast hint to defenders: "Enemy mortar destroyed!"
      record m_iMissionsFiredBeforeDestruction
      m_SpawnedMortar = null
  → next director decision cycle evaluates ScoreArtilleryRespawn()

Director respawns mortar (optional, costs budget)
  → cost = GetRespawnCost()    [escalating: 15 → 25 pts]
  → budget.Reserve(cost)
  → SpawnMortarTeam(GetNextSpawnPoint())   [avoids last position]
  → NO notification to defenders
```

### Round types
| Type | Cost/round | Typical volley | Cooldown | Targeting |
|---|---|---|---|---|
| `HIGH_EXPLOSIVE` | 3 pts | 2–8 rounds | 4 min | Monte Carlo (defender cluster) |
| `SMOKE` | 1 pt | 4–6 rounds | 2 min | Attack axis midpoint |
| `ILLUMINATION` | 2 pts | 2–3 rounds | 5 min | Zone centroid, night only |
| `PRACTICE` | 0 pts | 1 round | 30 sec | Best HE target, fallback |

### Round type selection
```cpp
SCR_EAIArtilleryAmmoType SelectRoundType(AFM_DiDBattlefieldState state)
{
    if (state.m_bIsNight && !IsOnCooldown(ILLUMINATION) && state.m_fBudgetRatio > 0.3)
        return SCR_EAIArtilleryAmmoType.ILLUMINATION;

    if (m_bArmorPushPending && !IsOnCooldown(SMOKE))
        return SCR_EAIArtilleryAmmoType.SMOKE;

    if (state.m_fDefenderDensity > m_fHEDensityThreshold
        && !IsOnCooldown(HIGH_EXPLOSIVE)
        && state.m_fBudgetRatio > 0.2)
        return SCR_EAIArtilleryAmmoType.HIGH_EXPLOSIVE;

    return SCR_EAIArtilleryAmmoType.PRACTICE;  // harass for free
}
```

### Respawn cost escalation
```cpp
int GetRespawnCost()
{
    switch (m_iRespawnCount)
    {
        case 0: return 15;
        case 1: return 25;
        default: return 999;  // effectively blocked
    }
}
```

### Information asymmetry (by design)
| Event | Defenders notified | Attackers notified |
|---|---|---|
| Mortar spawns at zone start | No | Internal |
| Mortar fires mission | Yes (impacts) | Internal |
| Mortar destroyed | **Yes** — hint | Internal |
| Mortar respawns | **No** | Internal |

---

## Phase 4 — Combined Arms Sequencing

**Goal:** Director can plan multi-step actions (smoke → armor push).
Synergy between artillery and ground units becomes visible as a learnable pattern.

### New files
| File | Purpose |
|---|---|
| `Scripts/Game/DiD/AFM_DiDAttackSequence.c` | Two-step action: first action + delay + second action, budget reserved upfront |

### Modified files
| File | Change |
|---|---|
| `AFM_DiDAttackerDirector.c` | Evaluate combined sequences alongside individual options; execute pending sequence steps |

### Sequence concept
```cpp
class AFM_DiDAttackSequence
{
    ref AFM_DiDDirectorAction m_FirstAction;   // e.g. SMOKE mission
    float m_fDelaySeconds;                     // e.g. 30s for smoke to develop
    ref AFM_DiDDirectorAction m_SecondAction;  // e.g. mechanized push
    int m_iTotalCost;                          // reserved upfront
}
```

Sequences compete with individual options in the same decision cycle.
If the sequence scores highest, both actions are reserved and the director
begins executing step 1, then waits `m_fDelaySeconds` before step 2.

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
            ├── Phase 3 (Artillery)
            └── Phase 4 (Sequences)
```

Phases 3 and 4 can be developed in parallel once Phase 2 is stable.
Phase 3 is independently testable (artillery can run without sequences).

---

## Out of scope (tracked in GAMEPLAY_IDEAS.md)

- Player ticket pool per zone
- Retreat window before zone failure
- Prepare phase activities (fortifications, ammo cache destruction)
- Zone type modifiers (ELIMINATION, ESCORT)
- Inter-zone transit phase
