# DefenseInDepth — Implementation Plan

## Overview

Four phased deliverables, each independently playable and testable.
Each phase builds on the previous but does not break it.

---

## Phase 1 — Attacker Budget System

**Goal:** Replace infinite AI spawning with a per-zone points budget.
Each spawn costs points. Zone ends via budget exhaustion + AI cleared,
not just timer expiry or defender death.

### New files
| File | Purpose |
|---|---|
| `Scripts/Game/DiD/AFM_DiDAttackerBudget.c` | Budget state: total, remaining, reservation logic |

### Modified files
| File | Change |
|---|---|
| `AFM_DiDZoneComponent.c` | Add `m_iPointsBudget` attribute; hold `AFM_DiDAttackerBudget` instance; add `FINISHED_REPELLED` state (budget + AI = 0) |
| `AFM_DiDSpawnerComponent.c` | Add `m_iPointCostPerUnit` attribute; call `zone.ReserveBudget(cost)` before spawning, confirm/release after |
| `AFM_DiDZoneSystem.c` | Handle `FINISHED_REPELLED` → invoke `m_OnZoneHeld` (defenders win) |
| `AFM_GameModeDiD.c` | Add `RPC_DoZoneRepelled()` hint: "Enemy assault repelled!" |
| `AFM_ScoreInfoDisplay.c` | For regular zones: right score shows remaining budget instead of AI count in zone |

### Budget reservation contract
```
// Before spawn
bool reserved = zone.GetBudget().Reserve(cost);
if (!reserved) return;  // can't afford, skip spawn

// After successful spawn (in DisableAIUnconsciousness callback)
zone.GetBudget().Confirm(cost);

// After failed spawn
zone.GetBudget().Release(cost);
```

### Key attributes (AFM_DiDZoneComponent)
```
[Attribute("120", UIWidgets.EditBox, "Attacker points budget for this zone", category: "DiD Budget")]
int m_iPointsBudget;

[Attribute("0.5", UIWidgets.EditBox, "Fraction of remaining budget rolled over to next zone on failure (0-1)", category: "DiD Budget")]
float m_fBudgetRolloverFraction;
```

### Key attributes (AFM_DiDSpawnerComponent)
```
[Attribute("1", UIWidgets.EditBox, "Budget points consumed per spawned unit", category: "DiD Budget")]
int m_iPointCostPerUnit;
```

### Budget rollover
`AFM_DiDZoneSystem.ProgressToNextZone()` reads `remainingBudget * rolloverFraction`
and passes it to `nextZone.GetBudget().AddBonus(rollover)` before activating.

### Tuning baseline
| Zone type | Base budget | Rationale |
|---|---|---|
| Infantry-only | 80–100 pts | ~20 squads of 4 @ 1pt/soldier |
| Mixed infantry + armor | 120–150 pts | infantry + 2–3 vehicles |
| Wave zone | Per-wave ticket (existing) | unchanged |

---

## Phase 2 — Attacker Director

**Goal:** Replace autonomous per-spawner ticking with a central decision-maker
that coordinates all spawners, tracks attack phases, and spends budget intelligently.

### New files
| File | Purpose |
|---|---|
| `Scripts/Game/DiD/AFM_DiDAttackerDirector.c` | Director entity: decision loop, phase tracking, spawner coordination |
| `Scripts/Game/DiD/AFM_DiDAttackPhase.c` | Enum + phase transition logic |
| `Scripts/Game/DiD/AFM_DiDSpawnRequest.c` | Scored spawn candidate (spawner ref + score + cost) |

### Modified files
| File | Change |
|---|---|
| `AFM_DiDZoneComponent.c` | `LateInit()` finds director child; `HandleActiveZoneLogic()` calls `director.Process()` instead of iterating spawners directly |
| `AFM_DiDSpawnerComponent.c` | Remove self-ticking `Process()`; add `ScoreRequest(AFM_DiDBattlefieldState state) → float` and `TriggerSpawn()` |
| `AFM_DiDInfantrySpawnerComponent.c` | Implement `ScoreRequest()` |
| `AFM_DiDMechanizedSpawnerComponent.c` | Implement `ScoreRequest()` |

### Director hierarchy in world
```
AFM_DiDZoneEntity
├── PolylineShapeEntity
├── AFM_PlayerSpawnPointEntity
└── AFM_DiDAttackerDirector          ← new GenericEntity child
    ├── AFM_DiDInfantrySpawnerComponent
    └── AFM_DiDMechanizedSpawnerComponent
```
Director owns spawners as children. Zone's `LateInit()` finds the director by type,
director finds its own spawner children.

### Attack phases (budget-driven, not time-driven)
```
enum EAFMAttackPhase
{
    PROBE,    // budget > 75% — infantry only, test defender positions
    ASSAULT,  // 75% → 25% — all options active, peak pressure
    FINAL     // budget < 25% — score bonus to all options, spend aggressively
}
```

### Decision cycle
```
every 25 seconds:
  1. Snapshot battlefield state (defenderCount, aiInZone, budgetRatio, timeRatio, phase)
  2. foreach spawner: score = spawner.ScoreRequest(state)
  3. filter: score > 0, budget covers cost, spawner off cooldown
  4. weighted random pick from top 3 candidates (scores as weights)
  5. reserve budget, call spawner.TriggerSpawn()
  6. start spawner cooldown
```

### Battlefield state snapshot (passed to all ScoreRequest calls)
```
class AFM_DiDBattlefieldState
{
    int m_iDefenderCount;
    int m_iAICountInZone;
    float m_fBudgetRatio;       // remaining / total
    float m_fTimeRatio;         // timeRemaining / totalTime
    float m_fDefenderDensity;   // defenders per 100m² (zone centroid sample)
    EAFMAttackPhase m_ePhase;
    bool m_bIsNight;
}
```

### Per-spawner ScoreRequest logic

**Infantry:**
```
baseScore = 1.0
+1.5  if aiInZone < defenderCount * 0.5   // outnumbered, need bodies
+0.5  if phase == FINAL
-2.0  if aiInZone > maxAICount * 0.8      // zone saturated
-0.5  if budgetRatio < 0.15               // budget critical
```

**Mechanized:**
```
baseScore = 0.1
+2.0  if phase == FINAL
+1.0  if budgetRatio > 0.6
+1.5  if defenderDensity is high (static defense)
-9.0  if on cooldown (min 3 min between armored pushes)
```

### Director attributes
```
[Attribute("25", UIWidgets.EditBox, "Decision cycle interval (seconds)", category: "DiD Director")]
int m_iDecisionIntervalSeconds;

[Attribute("0.75", UIWidgets.EditBox, "Aggression (0=conservative, 1=reckless)", category: "DiD Director")]
float m_fAggression;   // multiplies scores of expensive options
```

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

### Smoke targeting (attack axis)
```cpp
vector GetSmokeTargetPosition()
{
    vector spawnPos = m_pLastUsedSpawnPoint.GetOrigin();
    vector zoneEdge = FindNearestZoneBoundaryPoint(spawnPos);
    // lay smoke 60% along the approach, covering the gap between attacker and zone
    return spawnPos + (zoneEdge - spawnPos) * 0.6;
}
```

### Respawn point selection (avoids last position)
```cpp
AFM_ArtillerySpawnPointEntity GetNextSpawnPoint()
{
    if (m_aSpawnPoints.Count() == 1)
        return m_aSpawnPoints[0];

    array<AFM_ArtillerySpawnPointEntity> candidates = {};
    foreach (AFM_ArtillerySpawnPointEntity sp : m_aSpawnPoints)
    {
        if (sp != m_pLastUsedSpawnPoint)
            candidates.Insert(sp);
    }
    return candidates.GetRandomElement();
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
| Budget exhausted, no respawn | **No** | Internal |

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

### Smoke + armor sequence scoring
```cpp
float ScoreSmokeArmorSequence(AFM_DiDBattlefieldState state)
{
    // requires: smoke not on cooldown, armor not on cooldown,
    //           budget covers both, phase is ASSAULT or FINAL
    if (!CanAffordSequence(smokeCost + armorCost)) return -1;
    if (IsOnCooldown(SMOKE) || IsOnCooldown(MECHANIZED)) return -1;
    if (state.m_ePhase == EAFMAttackPhase.PROBE) return -1;

    float score = 2.0;
    score += state.m_fDefenderDensity * 1.5;  // static defense = armor-friendly
    score += (state.m_ePhase == EAFMAttackPhase.FINAL) ? 1.5 : 0;
    return score;
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
Phase 1 (Budget)
    └── Phase 2 (Director)
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
