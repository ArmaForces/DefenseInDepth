# AI Commander — Implementation Gaps

Comparison between the brainstormed design (`01_general_plan.md`) and the current codebase.
Each gap is marked with a priority: **[core]** blocks the design from working, **[tactical]** improves quality, **[polish]** improves player experience.

---

## 1. Approach Routes — Lighthouses, Staging Points, Vehicle Overwatch [core]

**What exists:**
Each spawner has a flat `m_aAIWaypoints` array. A single random waypoint is picked per spawn. Groups get one destination.

**What's missing:**
- `AFM_LighthouseEntity` — marker entity placed by mission designer at zone entry points
- `AFM_StagingPointEntity` — covered assembly position on each route
- `AFM_VehicleOverwatchEntity` — hull-down position before zone for mechanized units
- `AFM_ApproachRoute` — data container linking staging → lighthouse → overwatch per route
- Waypoint chain generation: `SpawnPoint → StagingPoint → Lighthouse → Zone` for infantry; `SpawnPoint → Lighthouse → VehicleOverwatch → Zone` for mechanized
- Route registration on each zone (director collects routes from children the same way it currently collects spawn points)

**Impact:** Without routes, all groups path directly to a single waypoint. No approach variation, no coordination hooks, no route pressure tracking possible.

---

## 2. Group Lifecycle Tracking [core]

**What exists:**
`m_aSpawnedAIGroups` per spawner — a flat array used only for `GetActiveAICount()` and `Cleanup()`. No per-group metadata beyond the group reference itself.

**What's missing:**
- `AFM_DiDGroupEntry` — tracking struct per active group:
  ```
  AFM_DiDGroupEntry {
      AIGroup             group
      AFM_DiDZoneComponent    assignedZone
      AFM_ApproachRoute       assignedRoute   // null for cross-zone relocations
      EUnitType               unitType
      int                     spawnTick
  }
  ```
- Central group registry on the director (replaces per-spawner arrays)
- Per-tick alive count poll and dead group cleanup
- Subscription to `SCR_AIGroupInfoComponent.GetOnControlModeChanged()` at group registration
- IDLE group handler:
  - Players in zone → issue patrol waypoints
  - Zone uncaptured → issue sweep waypoints
  - Zone captured → relocate to most under-covered other zone (direct waypoint, no lighthouse)
- Subscription to `SCR_AIGroupUtilityComponent.m_OnMoveFailed` for route pressure tracking

**Impact:** Without this, groups are invisible to the commander after spawn. IDLE reassignment, cross-zone support, and route pressure are all impossible.

---

## 3. Route Pressure Tracking [tactical]

**What exists:**
Nothing. Spawners pick waypoints randomly each time. There is no memory of which routes are costing groups their lives.

**What's missing:**
- `AFM_RoutePresssure` per `AFM_ApproachRoute`:
  ```
  AFM_RoutePressure {
      int     groupsSent
      int     groupsWiped       // alive count hit 0 after using this route
      int     cooldownTicksRemaining
  }
  ```
- Increment `groupsWiped` on `m_OnMoveFailed` or when a group's alive count hits 0
- Route cooldown: skip a route for N ticks after a wipe (N scaled by aggressiveness)
- Route selection in spawner replaced by director-level route assignment that respects pressure

**Impact:** Without this, the commander keeps funnelling groups into the same killzone. Players camp one spot and the AI never adapts.

---

## 4. Multi-Zone Stages [core]

**What exists:**
`AFM_DiDZoneSystem` holds a `map<int, AFM_DiDZoneComponent>` and activates zones one at a time sequentially. One active zone = one stage. Stage advance = zone completion.

**What's missing:**
- Stage concept: a named set of 1..N simultaneously active zones
- `AFM_DiDStage` — groups zones together, exposes:
  - `array<AFM_DiDZoneComponent> m_aZones`
  - `bool IsMajorityCaptured()` — AI holds > 50% of zones
  - `bool IsAllZonesCaptured()` — for last-stand
- `AFM_DiDZoneSystem` updated to activate all zones in a stage simultaneously, advance to next stage when majority captured or all players dead
- Per-zone director remains as-is; stage coordinator sits above them for cross-zone budget and priority allocation
- Stage-level budget allocation: proportional per zone with minimum floor (already designed in `01_general_plan.md`)
- Player win condition: hold majority of zones for T minutes (timer logic currently only tracks zone-level survival)

**Impact:** The entire multi-zone tactical layer of the design doesn't exist yet. Currently stages are just sequential single zones.

---

## 5. Zone Capture Points [core]

**What exists:**
Zone win/loss is survival-based:
- All defenders dead → `FINISHED_FAILED`
- Timer expires with defenders alive → `FINISHED_HELD`
- Budget exhausted + no AI in zone → `FINISHED_REPELLED`

There is no capture progress. AI doesn't "own" a zone progressively.

**What's missing:**
- A capture/presence mechanic: AI presence in zone without defender contest advances a capture meter
- `m_fCaptureProgress` (0.0 = player-held, 1.0 = AI-captured) on each zone
- Capture ticks up when AI count in zone > defenders in zone; ticks down when defenders dominate
- Stage win condition for AI: majority of zones reach 1.0 before T expires
- This is a significant design change from the current survival model

**Note:** The current `FINISHED_FAILED` (all players dead) → zone advance model is already partly correct for the "kill all players → next stage" case. But the progressive capture angle is entirely absent.

---

## 6. Combined Arms Assault Packages [tactical]

**What exists:**
Director picks **one** spawner per decision cycle via weighted random. Artillery is evaluated independently. There is no coordination between the two decisions.

**What's missing:**
- `AFM_AssaultPackage` — a multi-unit spawn decision:
  ```
  AFM_AssaultPackage {
      AFM_DiDZoneArtillery    supportMission     // fire first
      AFM_DiDSpawnerComponent infantrySpawner
      AFM_DiDSpawnerComponent mechanizedSpawner  // optional
      AFM_ApproachRoute       sharedRoute
      int                     targetArrivalTick  // synchronized timing
  }
  ```
- Director can emit a package instead of individual spawns when:
  - Entering COMMIT/SURGE phase
  - Zone has stalled 2+ ticks
  - Budget covers at least two unit types
- Mortar fires 1 tick before infantry arrives (pre-assault suppression)
- Infantry and mechanized spawned with staggered timing based on route travel time estimates
- Travel time annotations on `AFM_ApproachRoute` (set by mission designer)

**Impact:** Without packages, infantry and artillery are never meaningfully coordinated. Simultaneous smoke + assault, or suppression + push, never happen.

---

## 7. Group Behaviour Tuning [tactical]

**What exists:**
Groups are spawned and left to the default AI behaviour. No external influence after spawn.

**What's missing:**
Use of `SCR_AIGroupUtilityComponent` post-spawn:
- `SetMaxAutonomousDistance(float)` — prevent groups from chasing players away from their objective
- `SetCombatMode(EAIGroupCombatMode)` — tune per phase (e.g. more aggressive in SURGE)
- `SetFireRateCoef(float)` — scale fire rate with aggression setting
- Subscribe `m_OnAgentLifeStateChanged` — replace the per-tick alive count poll with event-driven tracking

Suggested per-phase values (driven by `m_fAggression`):

| Phase | MaxAutonomousDist | FireRateCoef |
|---|---|---|
| PROBE | 150m | 0.8 |
| ASSAULT | 300m | 1.0 |
| SURGE | 500m | lerp(1.0, 1.5, aggressiveness) |

**Impact:** Currently groups wander off objectives to chase players and never return. `SetMaxAutonomousDistance` alone would significantly improve zone pressure consistency.

---

## 8. Intra-Stage Aggression Curve & Quiet Intervals [polish]

**What exists:**
Spending is continuous — every decision cycle the director evaluates and potentially spawns. No explicit quiet periods exist.

**What's missing:**
- Post-surge quiet interval: after a SURGE cycle, suppress spend rate for 2–3 ticks
- Intra-stage aggression ramp: spend rate scales upward across stage duration, not just by budget ratio
- `m_fTimeRatio` is already in `AFM_DiDBattlefieldState` — it can drive a spend rate multiplier that increases as time runs out, independent of the budget-driven phase

**Impact:** Without quiet intervals, pressure feels constant and numbing. Peaks and valleys make individual assaults feel impactful.

---

## 9. Player Count Scaling for Budget [tactical]

**What exists:**
`m_iPointsBudget` is a fixed `[Attribute]` set by the mission designer per zone.

**What's missing:**
- Budget multiplier at zone activation: `actualBudget = designerBudget * PlayerCountModifier()`
- `PlayerCountModifier = lerp(0.5, 2.0, alivePlayerCount / referencePlayerCount)`
- `m_iReferencePlayerCount` as an attribute on the zone or game mode (e.g. 4)
- Applied in `AFM_DiDZoneComponent.ActivateZone()` when initializing `m_Budget`

**Impact:** With a fixed budget, a 1-player session gets the same AI wave as an 8-player session — either trivially easy or impossible.

---

## 10. Near-Miss Detection [polish]

**What exists:**
Nothing. The director has no memory of previous assaults or close calls.

**What's missing:**
- Track `m_fLastCaptureHighWaterMark` per zone: highest capture progress reached
- If watermark > 0.7 and zone was repelled, flag zone as "contested" — don't immediately flood it, apply a brief recovery period
- Gives players a moment to feel the win before pressure builds again

---

## Summary Table

| Gap | Area | Priority | Estimated Effort |
|---|---|---|---|
| Approach routes (lighthouse, staging, overwatch entities + chain) | Infrastructure | **[core]** | Large |
| Group lifecycle registry + IDLE reassignment | Director logic | **[core]** | Medium |
| Route pressure tracking | Director logic | **[core]** | Small |
| Multi-zone stages | Zone system | **[core]** | Large |
| Zone capture progress | Zone logic | **[core]** | Medium |
| Combined arms assault packages | Director logic | **[tactical]** | Medium |
| Group behaviour tuning (SetMaxAutonomousDist etc.) | Spawner/director | **[tactical]** | Small |
| Intra-stage aggression curve + quiet intervals | Director logic | **[polish]** | Small |
| Player count budget scaling | Zone/budget | **[tactical]** | Small |
| Near-miss detection | Director logic | **[polish]** | Small |

---

## What Does NOT Need to Change

The following existing systems are well-implemented and align with the design:

- `AFM_DiDAttackerBudget` — budget tracking, rollover, ratio-based phases ✓
- `AFM_DiDBattlefieldState` + `EAFMAttackPhase` — snapshot model ✓
- `AFM_DiDZoneArtillery` — fire mission types, MC targeting, smoke perpendicular, respawn logic ✓
- `ScoreRequest()` / `TriggerSpawn()` director interface on spawners ✓
- `WeightedRandomPick()` from top 3 candidates ✓
- Aggression multiplier on expensive spawner options ✓
- Radio notifications via `SCR_ChatComponent.RadioProtocolMessage` ✓
- `AFM_DiDWaveZoneComponent` (orthogonal feature, not in scope) ✓
