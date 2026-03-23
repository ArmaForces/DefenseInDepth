# AI Commander — Implementation Plan

Steps are ordered by dependency. Each step has a single testable objective.
Complete and verify each step before starting the next.

Legend: **[new]** = new file, **[modify]** = change existing file, **[delete]** = remove file.

---

## Pre-work — Remove Obsolete Code

Before building new features, strip out legacy patterns that would otherwise need to be worked around.

### Step P1 — Delete Legacy Spawner Mode

**Delete:**
- `AFM_DiDMortarSpawnerComponent.c` **[delete]** — superseded by `AFM_DiDZoneArtillery`

**Modify:**
- `AFM_DiDSpawnerComponent` — remove `Process()`, `CanSpawnNow()`, per-spawner `m_aSpawnedAIGroups`, `m_fLastSpawnTime`, ticket system (`m_bUseTickets`, `m_iMaxTickets`, `ConsumeTickets()`, `GetRemainingTickets()`, `SetRemainingTickets()`, `IsTicketBased()`). Spawners are director-only from now on.
- `AFM_DiDInfantrySpawnerComponent` — remove overridden `Process()`
- `AFM_DiDZoneComponent` — remove legacy `m_aSpawners` array, `HandleLegacySpawners()`, any direct spawner `Process()` calls. Director is always required.
- `AFM_DiDWaveZoneComponent` — remove `GetRemainingTickets()` and any other references to the per-spawner ticket system. Audit for any remaining ticket-related fields on this class before marking complete.
- `AFM_DiDWaveSpawnerComponent` — remove `IsActive()` /  `IsInActiveRange()` wave range gating (wave difficulty is now handled by the director directly, not per-spawner filters)

**Remove from `AFM_DiDSpawnerComponent`:**
- `m_aAIWaypoints` field and its collection in `Prepare()` — replaced by route collection in A3
- `m_iWaveIntervalSeconds` / `GetWaveInterval()` / `SetWaveInterval()` — interval is director-owned
- `SpawnWave(int count)` — director calls `TriggerSpawn()` directly
- `GetNextSpawnTime()` — no longer relevant

**Keep:**
- `ScoreRequest()`, `TriggerSpawn()`, `GetPointCostPerUnit()`, `SpawnSingleGroup()`, `SpawnAI()`, `SpawnPrefab()`, `GetSpawnCount()`, `GetSpawnCountVariance()`

**Test objective:** Project compiles with no errors after removals. A zone with a director and spawners initializes and the director's decision cycle runs. No references to deleted methods remain.

---

## Phase A — Approach Routes

Lays the entity infrastructure that all subsequent coordination features depend on.

---

### Step A1 — Approach Route Marker Entities

**Create:**
- `AFM_LighthouseEntity.c` **[new]** — extends `SCR_AIWaypoint` (not `GenericEntity`)
- `AFM_StagingPointEntity.c` **[new]** — extends `SCR_AIWaypoint`
- `AFM_VehicleOverwatchEntity.c` **[new]** — extends `SCR_AIWaypoint`

All three carry no additional logic. They extend `SCR_AIWaypoint` so they can be:
1. Placed permanently in the world editor as waypoint entities
2. Added directly to a group via `group.AddWaypoint()` — no dynamic waypoint spawning needed for the approach chain
3. Identified by type via `AFM_LighthouseEntity.Cast(entity)`

Also create `AFM_ZoneAssaultWaypointEntity.c` **[new]** — extends `SCR_AIWaypoint`, placed as a single permanent child of each zone component to serve as the final attack destination. This eliminates all dynamic waypoint spawning.

**Test objective:** Place one of each entity as a child of a spawner in the world editor. Verify the editor accepts them without errors, they appear in the scene hierarchy, and a group can be given one as a waypoint at runtime.

---

### Step A2 — ApproachRoute Data Container

**Create:** `AFM_DiDApproachRoute.c` **[new]**

```
class AFM_DiDApproachRoute
{
    AFM_StagingPointEntity      m_StagingPoint
    AFM_LighthouseEntity        m_Lighthouse
    AFM_VehicleOverwatchEntity  m_VehicleOverwatch   // null for infantry-only routes
    float                       m_fInfantryTravelTicks
    float                       m_fMechanizedTravelTicks
}
```

Plain data container, no logic. Travel times are populated from `[Attribute]` fields on `AFM_LighthouseEntity` at collection time (see A3), not set on the route directly by the designer.

Add to `AFM_LighthouseEntity`:
```
[Attribute("2", UIWidgets.EditBox, "Infantry travel time in ticks (spawn to zone)", category: "DiD Route")]
float m_fInfantryTravelTicks;

[Attribute("3", UIWidgets.EditBox, "Mechanized travel time in ticks (spawn to zone)", category: "DiD Route")]
float m_fMechanizedTravelTicks;
```

**Test objective:** Class compiles and can be instantiated with its fields assigned.

---

### Step A3 — Route Collection in Spawner

**Modify:** `AFM_DiDSpawnerComponent.Prepare()`

Replace the removed `m_aAIWaypoints` collection with route collection:

- Scan children; build separate arrays of `AFM_StagingPointEntity`, `AFM_LighthouseEntity`, `AFM_VehicleOverwatchEntity`
- Match by **index order** (not proximity): the Nth lighthouse pairs with the Nth staging point (if present) and the Nth overwatch (if present). Entity order in the hierarchy determines pairing — document this convention for designers
- Copy `m_fInfantryTravelTicks` / `m_fMechanizedTravelTicks` from the lighthouse attribute into the route
- Store `array<ref AFM_DiDApproachRoute> m_aApproachRoutes`
- Also scan for a single `AFM_ZoneAssaultWaypointEntity` — store as `m_AssaultWaypoint` (null means no route is complete; warn designer)

Warn and abort `Prepare()` if no lighthouses found — routes are now required.

**Test objective:** Spawner with 2 lighthouses, 2 staging points, 1 overwatch logs "Found 2 approach routes". Route 0 pairs lighthouse 0 + staging 0 + overwatch 0; route 1 pairs lighthouse 1 + staging 1. Spawner with no lighthouses logs a warning.

---

### Step A4 — Waypoint Chain Generation

**Modify:** `AFM_DiDSpawnerComponent.SpawnAI()`

Replace single waypoint assignment with chain generation based on unit type. All waypoint entities are **permanent world entities** added directly via `group.AddWaypoint()` — no dynamic spawning or cleanup needed:

- Infantry: `group.AddWaypoint(route.m_StagingPoint)` → `group.AddWaypoint(route.m_Lighthouse)` → `group.AddWaypoint(m_AssaultWaypoint)`
- Mechanized: `group.AddWaypoint(route.m_Lighthouse)` → `group.AddWaypoint(route.m_VehicleOverwatch)` → `group.AddWaypoint(m_AssaultWaypoint)`

The `m_AssaultWaypoint` (`AFM_ZoneAssaultWaypointEntity`) is a single permanent ATTACK waypoint placed by the designer as a child of the zone component (not the spawner). All groups on all routes share it.

**Test objective:** Spawn an infantry group. Observe in-game it moves to the staging area, then through the lighthouse position, then into the zone — three distinct movement segments.

---

### Step A5 — Waypoint Prefab Config Singleton

**Create:** `AFM_DiDCommanderConfig.c` **[new]**

```
[BaseContainerProps(configRoot: true)]
class AFM_DiDCommanderConfig : ScriptComponent
{
    [Attribute] ResourceName m_sMoveWaypointPrefab
    [Attribute] ResourceName m_sAttackWaypointPrefab

    static AFM_DiDCommanderConfig GetInstance()
    {
        BaseGameMode gm = GetGame().GetGameMode();
        if (!gm) return null;
        return AFM_DiDCommanderConfig.Cast(gm.FindComponent(AFM_DiDCommanderConfig));
    }
}
```

Attach as a script component on the game mode entity. With the A4 correction (all waypoints pre-placed), this config is mainly used by artillery (`m_sAttackWaypointPrefab` is still needed for fire mission waypoints spawned at runtime).

**Test objective:** `AFM_DiDCommanderConfig.GetInstance()` returns non-null at runtime. Prefab paths set in the editor are readable from spawner code.

---

## Phase B — Group Lifecycle Registry

Gives the director full visibility into every group after spawn.

---

### Step B1 — Group Entry Struct + EUnitType Enum

**Modify:** `AFM_DiDGroupEntry.c` (untracked file — populate it)

First define the enum (can be in the same file or a shared file):
```
enum EUnitType
{
    INFANTRY,
    MECHANIZED,
    MORTAR
}
```

Then the entry struct:
```
class AFM_DiDGroupEntry
{
    AIGroup                     m_Group
    AFM_DiDZoneComponent        m_AssignedZone
    AFM_DiDApproachRoute        m_AssignedRoute     // null after cross-zone relocation
    EUnitType                   m_eUnitType
    int                         m_iSpawnTick
    int                         m_iAliveCount       // maintained by B5 event subscription
}
```

No `m_aDynamicWaypoints` field is needed — A4 uses permanent pre-placed waypoints exclusively.

**Test objective:** Class and enum compile and can be instantiated.

---

### Step B2 — Central Group Registry on Director

**Modify:** `AFM_DiDAttackerDirector`

- Add `array<ref AFM_DiDGroupEntry> m_aGroupRegistry`
- Add `GetDirector()` method to `AFM_DiDZoneComponent` returning the director (set during `Init()` when director child is found)
- After each spawn, spawner calls `m_Zone.GetDirector().RegisterGroup(group, zone, route, unitType)` — director creates the entry
- `GetActiveAICount()` sums `entry.m_iAliveCount` across registry
- `Cleanup()` iterates registry and deletes group entities (no dynamic waypoints to clean up)

**Test objective:** Spawn 3 groups. `m_aGroupRegistry.Count() == 3`. Kill all agents in one group — count drops to 2 on next tick.

---

### Step B3 — EGroupControlMode Event Subscription

**Modify:** `AFM_DiDAttackerDirector.RegisterGroup()`

```
SCR_AIGroupInfoComponent info = SCR_AIGroup.Cast(group)
    .GetGroupUtilityComponent().m_GroupInfo;
info.GetOnControlModeChanged().Insert(OnGroupControlModeChanged);
```

`OnGroupControlModeChanged`: the exact ScriptInvoker parameter signature is **unknown** — verify in-engine. Inside the callback, do NOT rely on parameters for the current mode; instead poll `info.GetGroupControlMode()` directly to read the actual new mode. Cross-reference registry by iterating entries to find the matching group.

**Test objective:** Spawn a group, let it reach its destination and go IDLE. Log line: `"Group [X] mode → IDLE"`.

---

### Step B4 — IDLE Group Handler

**Modify:** `AFM_DiDAttackerDirector`

Implement `HandleIdleGroup(AFM_DiDGroupEntry entry)`, called from `OnGroupControlModeChanged` when mode is `IDLE`:

```
HandleIdleGroup(entry) {
    zone = entry.m_AssignedZone

    if PlayersInsideZone(zone):
        IssuePatrolWaypoints(entry.m_Group, zone)
        // 2-3 SCR_AIWaypoint entities at random points inside zone polygon, cycled

    else if !ZoneCaptured(zone):
        IssueSweepWaypoint(entry.m_Group, zone.GetZoneCentroid())

    else:
        target = FindUndercoveredZone()  // see NOTE below
        if target:
            IssueDirectWaypoint(entry.m_Group, target.GetZoneAssaultWaypoint())
            entry.m_AssignedZone = target
            entry.m_AssignedRoute = null
}
```

**NOTE:** `FindUndercoveredZone()` requires access to all zones in the current stage, which is not available until Phase E. Stub this method returning `null` at B4 time with a TODO. The full implementation (lowest `(groupsEnRoute + groupsEngaging) / playerCount` ratio) is added in E2 when the stage context exists.

**Test objective:** A group reaches a captured zone with no players inside. The director logs "IDLE group: zone captured, stub FindUndercoveredZone returned null — no relocation". Patrol and sweep paths work for the non-captured cases.

---

### Step B5 — Agent Death via Event

**Modify:** `AFM_DiDAttackerDirector.RegisterGroup()`

Subscribe to `SCR_AIGroupUtilityComponent.m_OnAgentLifeStateChanged`. On each event decrement `entry.m_iAliveCount`. When it hits 0, flag entry for removal; clean up at the start of the next tick (iterate registry in reverse, remove flagged entries).

`GetActiveAICount()` now reads from `entry.m_iAliveCount` (no live polling via `group.GetAgentsCount()`). Initialize `m_iAliveCount` from `group.GetAgentsCount()` at registration time.

**Test objective:** Kill one agent in a 4-man group — director logs the death immediately. Kill all four — entry is removed from registry on the next tick.

---

## Phase C — Route Pressure Tracking

Teaches the director which routes are dangerous.

---

### Step C1 — Pressure Fields on ApproachRoute

**Modify:** `AFM_DiDApproachRoute` — add directly:

```
int m_iGroupsSent
int m_iGroupsWiped
int m_iCooldownTicksRemaining
```

**Decay mechanism:** to prevent old wipe data from permanently biasing selection, add:
```
int m_iWipeDecayTicksRemaining   // countdown; when it hits 0, halve m_iGroupsWiped
```
Reset `m_iWipeDecayTicksRemaining = 10` each time a group is successfully wiped. Decay runs in the director tick (see C3).

**Test objective:** Fields default to 0, are readable and writable.

---

### Step C2 — Wipe Detection

**Modify:** `AFM_DiDAttackerDirector`

When a group's alive count hits 0 (B5) and `entry.m_AssignedRoute != null`:
```
route.m_iGroupsWiped++
route.m_iWipeDecayTicksRemaining = 10
route.m_iCooldownTicksRemaining = Math.Round(Math.Lerp(4, 1, m_fAggression))
```

Also subscribe to `SCR_AIGroupUtilityComponent.m_OnMoveFailed` at registration — a move failure increments `m_iGroupsWiped` even if agents survive.

**Test objective:** Kill 2 groups on the same route. Log: `route.m_iGroupsWiped == 2, cooldown == N`.

---

### Step C3 — Pressure-Aware Route Selection + Cooldown Tick

**Modify:** `AFM_DiDAttackerDirector`

Add `UpdateRoutePressure()` called at the **start of each director decision tick** (not inside `SelectRoute()`). This ensures cooldowns and decay tick down even when no spawns occur:

```
UpdateRoutePressure() {
    foreach spawner in m_aSpawners:
        foreach route in spawner.m_aApproachRoutes:
            if route.m_iCooldownTicksRemaining > 0:
                route.m_iCooldownTicksRemaining--
            if route.m_iWipeDecayTicksRemaining > 0:
                route.m_iWipeDecayTicksRemaining--
                if route.m_iWipeDecayTicksRemaining == 0:
                    route.m_iGroupsWiped = Math.Max(0, route.m_iGroupsWiped / 2)
}
```

**Modify:** `AFM_DiDSpawnerComponent` — replace `m_aApproachRoutes.GetRandomElement()` with:

```
SelectRoute() {
    available = routes where cooldownTicksRemaining == 0
    if available.IsEmpty(): available = routes   // all hot — use least-bad

    // Weight: inverse wipe ratio (groupsWiped / max(1, groupsSent))
    return WeightedPick(available)
}
```

**Test objective:** Put one route on cooldown manually. All spawns use the other route until cooldown expires. After the cooldown, the hot route becomes available again.

---

## Phase D — Zone Capture Progress

Changes zone win/lose from survival-only to progressive capture.

---

### Step D1 — Capture Progress on Zone

**Modify:** `AFM_DiDZoneComponent`

Add fields:
```
[Attribute("0.02", UIWidgets.EditBox, "Capture progress rate per second when AI dominates zone", category: "DiD Zone")]
float m_fCaptureRatePerSecond;   // designer-configurable; default 0.02 (~50s for full capture)

float m_fCaptureProgress     // 0.0 player-held → 1.0 AI-captured
int   m_iStallTicks
float m_fLastCaptureProgress
```

In `HandleActiveZoneLogic()` each tick (use actual delta time from the 1s zone tick — `deltaTime = 1.0`):
```
aiInZone = GetAICountInsideZone()
defenders = GetDefenderCount()   // players alive inside zone polygon

if aiInZone > defenders:
    m_fCaptureProgress += m_fCaptureRatePerSecond * deltaTime
else if defenders > aiInZone:
    m_fCaptureProgress -= m_fCaptureRatePerSecond * deltaTime

m_fCaptureProgress = Math.Clamp(m_fCaptureProgress, 0.0, 1.0)

if Math.AbsFloat(m_fCaptureProgress - m_fLastCaptureProgress) < 0.001:
    m_iStallTicks++
else:
    m_iStallTicks = 0
m_fLastCaptureProgress = m_fCaptureProgress
```

Expose `GetCaptureProgress()` and `GetStallTicks()`.

**Test objective:** More AI than defenders → progress increases. More defenders → decreases. Clamped to [0, 1]. Stall ticks increment when progress is static.

---

### Step D2 — Capture Progress in Battlefield State

**Modify:** `AFM_DiDBattlefieldState` and `AFM_DiDAttackerDirector.BuildBattlefieldState()`

Add:
```
float m_fZoneCaptureProgress
int   m_iZoneStallTicks
```

Update `ScoreRequest()` in infantry and mechanized spawners:
- `stallTicks >= 2` → significant score reduction (more bodies won't break the stall — package escalation in F2 handles this case instead)

Note: when `ShouldIssuePackage()` returns true (F2), the package path takes over and the normal score-based path is skipped entirely. D2's score reduction only affects the fallback single-spawner path — no conflict.

**Test objective:** Stall a zone for 2+ ticks. Spawner log shows reduced score on the normal path.

---

### Step D3 — Capture-Based Zone End Condition

**Modify:** `AFM_DiDZoneComponent.HandleActiveZoneLogic()`

Add `FINISHED_CAPTURED` to `EAFMZoneState`. Transition when `m_fCaptureProgress >= 1.0`. Emit a distinct event so the game mode broadcasts the correct radio message ("Zone X has fallen").

The stage-level check in E3 reads zone state: `zone.GetZoneState() == EAFMZoneState.FINISHED_CAPTURED` counts as AI-captured. The event chain is sequential: zone transitions → stage evaluates — no circular dependency.

**Test objective:** Fill zone with AI, zero defenders inside. Zone finishes with `FINISHED_CAPTURED`. Radio message differs from "all players dead".

---

## Phase E — Multi-Zone Stages

Activates multiple zones simultaneously within a stage.

---

### Step E1 — Stage Data Container

**Create:** `AFM_DiDStage.c` **[new]** as a `ScriptComponent`

Scene hierarchy convention: `AFM_DiDStage` components are children of the `AFM_DiDZoneSystem` entity. Zone components are children of each stage entity. The zone system scans its children for `AFM_DiDStage` components at `Init()` and builds `array<ref AFM_DiDStage> m_aStages` sorted by `m_iStageIndex`.

```
class AFM_DiDStage : ScriptComponent
{
    [Attribute] int     m_iStageIndex
    [Attribute] float   m_fDefenseTimeSeconds

    [Attribute("75", UIWidgets.EditBox,
        "Seconds AI must hold majority before stage is lost (60–90 recommended)",
        category: "DiD Stage")]
    float m_fMajorityLostThresholdSeconds;

    array<AFM_DiDZoneComponent> m_aZones   // populated from children at Init()

    // Runtime tracking — server-only, not replicated
    float m_fAIMajorityHeldSeconds = 0.0   // counts up while AI holds majority; resets on retake

    bool IsMajorityCaptured()    // > 50% of zones in FINISHED_CAPTURED state
    bool IsDefendersMajority()   // > 50% of zones NOT in FINISHED_CAPTURED state
    int  GetCapturedCount()
    int  GetHeldCount()
    int  GetRemainingBudget()    // sum of remaining budget across all zone directors

    // 0.0–1.0 progress toward AI majority loss — drives HUD countdown
    float GetAIMajorityLossProgress()
}
```

**Test objective:** Stage with 2 zones. `IsMajorityCaptured()` false at 0 captured, true at 1 captured. `m_fMajorityLostThresholdSeconds` defaults to 75 and is editable in the Workbench attribute panel.

---

### Step E2 — Zone System Stage Activation

**Modify:** `AFM_DiDZoneSystem`

- Replace `m_ActiveZone` (single) with `m_ActiveStage` (`AFM_DiDStage`) and `array<AFM_DiDZoneComponent> m_aActiveZones`
- `StartZoneSystem()` activates all zones in stage index 0 simultaneously
- `ProcessZone()` → `ProcessStage()` — evaluates majority conditions and per-stage timer across all active zones
- Stage transition: clean up all active zones, collect `m_ActiveStage.GetRemainingBudget()` as carryover, activate next stage's zones passing carryover to each zone's `ActivateZone(carryover)` parameter
- `AFM_DiDZoneComponent.ActivateZone()` accepts an optional `int carryoverBudget = 0` parameter; the designer budget is added to it before creating `AFM_DiDAttackerBudget`

Complete `FindUndercoveredZone()` in the director: iterate `m_aActiveZones`, compute `(groupsEnRoute + groupsEngaging) / max(1, playerCountInZone)` per zone, return the zone with the lowest ratio.

**Test objective:** 2-zone stage — both zones enter `ACTIVE` simultaneously. Two director logs appear per decision cycle.

---

### Step E3 — Stage Win / Loss Conditions

**Modify:** `AFM_DiDZoneSystem.ProcessStage()`

Three independent conditions, checked each 1s tick:

```
// 1. Sustained AI majority — main stage-loss path
if stage.IsMajorityCaptured():
    if stage.m_fAIMajorityHeldSeconds == 0:
        NotifyMajorityLost(stage)           // first tick: radio "Fall back or retake"
    stage.m_fAIMajorityHeldSeconds += 1.0
    if stage.m_fAIMajorityHeldSeconds >= stage.m_fMajorityLostThresholdSeconds:
        ProgressToNextStage(EStageEndReason.AI_MAJORITY_SUSTAINED)
else:
    if stage.m_fAIMajorityHeldSeconds > 0:
        stage.m_fAIMajorityHeldSeconds = 0.0
        NotifyMajorityRecaptured(stage)     // radio "Zone majority recaptured!"

// 2. Defense timer expired — player win
if defenseTimerExpired && !stage.IsMajorityCaptured():
    OnStageHeld(stage)

// 3. All defenders dead — immediate stage loss, no fallback
if GetAliveDefenderCount() == 0:
    ProgressToNextStage(EStageEndReason.ALL_DEAD)
```

**`EStageEndReason` enum** (new, in this file):
```
enum EStageEndReason { AI_MAJORITY_SUSTAINED, ALL_DEAD, DEBUG_SKIP }
```

**`ProgressToNextStage(EStageEndReason reason)`** behavior differs by reason:

| Reason | AI cleanup | Next stage activates | Alive players | Dead players |
|---|---|---|---|---|
| `AI_MAJORITY_SUSTAINED` | Yes | Yes | Stay in world — fall back on their own | Respawn at next stage spawn points |
| `ALL_DEAD` | Yes | Yes | N/A (none alive) | Respawn at next stage spawn points |

For `AI_MAJORITY_SUSTAINED`: do NOT force-respawn or teleport alive players. They are already moving/fighting somewhere in the world. Activating the next stage is sufficient — its zones become `ACTIVE` around or ahead of them.

**Notifications needed on `AFM_GameModeDiD`** (add RPC methods mirroring existing `NotifyPhaseChanged` pattern):
- `NotifyMajorityLost()` — "Enemy holds the majority — fall back or retake in time!"
- `NotifyMajorityRecaptured()` — "Majority recaptured — hold your ground!"
- `NotifyStageLostFallback()` — "Stage lost — fall back to the next defensive position!"

`GetAliveDefenderCount()` must be a **global** check across all players in the defender faction, not per-zone. Implement as a helper on `AFM_DiDZoneSystem` iterating all faction players.

**Test objective:** Scenario A — AI captures both zones in a 2-zone stage and holds for 75s. Stage advances with "fall back" radio. Two players who are alive remain in-world at their current positions. Scenario B — Players kill all AI and retake majority at second 60. Radio fires "Majority recaptured", timer resets to 0, stage does NOT advance.

---

## Phase F — Combined Arms Packages

Coordinates mortar + infantry arrival timing.

---

### Step F1 — Assault Package Struct

**Create:** `AFM_DiDAssaultPackage.c` **[new]**

```
class AFM_DiDAssaultPackage
{
    AFM_DiDZoneArtillery        m_Artillery          // null = no pre-assault fire
    EAFMRoundType               m_ePreAssaultRound
    AFM_DiDSpawnerComponent     m_InfantrySpawner
    AFM_DiDSpawnerComponent     m_MechanizedSpawner  // null = infantry only
    AFM_DiDApproachRoute        m_Route
    float                       m_fTargetArrivalTicks
    int                         m_iTotalCost
}
```

**Test objective:** Class compiles. `m_iTotalCost` sums spawner costs correctly.

---

### Step F2 — Package Evaluation in Director

**Modify:** `AFM_DiDAttackerDirector.RunDecisionCycle()`

```
if ShouldIssuePackage(state):   // ASSAULT/FINAL phase AND stall >= 2 AND 2 spawner types affordable
    package = BuildBestPackage(state)
    if package && budget.CanAfford(package.m_iTotalCost):
        ExecutePackage(package, now)
        return          // ← explicit early return; normal weighted pick is skipped
// else: fall through to existing weighted single-spawner pick
```

The package path and the normal score-based path are mutually exclusive in a given cycle — no double-spending or conflict with D2's score reduction.

**Test objective:** Zone stalls 2+ ticks in ASSAULT phase. Log: `"Issuing assault package"`.

---

### Step F3 — Package Execution with Staggered Timing

**Modify:** `AFM_DiDAttackerDirector.ExecutePackage()`

```
ExecutePackage(package, now) {
    if package.m_Artillery:
        artillery.TriggerMission(state, now)   // fires immediately

    package.m_InfantrySpawner.TriggerSpawn(now, 1)

    if package.m_MechanizedSpawner:
        delay = Max(0, (package.m_fTargetArrivalTicks - route.m_fMechanizedTravelTicks) * TICK_MS)
        GetGame().GetCallqueue().CallLater(SpawnMechanized, delay, false, package, now)
}
```

**Test objective:** Mortar fires, infantry spawns, mechanized spawns ~30s later. All three arrive at the zone within the same 30s window.

---

## Phase G — Group Behaviour Tuning

Keeps groups on objective and adjusts aggression per phase.

---

### Step G1 — SetMaxAutonomousDistance on Spawn

**Modify:** `AFM_DiDSpawnerComponent.TriggerSpawn()`

Add `EAFMAttackPhase phase` parameter to `TriggerSpawn(WorldTimestamp now, int count, EAFMAttackPhase phase)`. Director passes the current phase when calling. `SpawnAI()` (called internally) uses it:

```
SCR_AIGroupUtilityComponent util = SCR_AIGroup.Cast(group).GetGroupUtilityComponent();
util.SetMaxAutonomousDistance(AutonomousDistance(phase, m_fAggression));

// PROBE=150, ASSAULT=300, FINAL=lerp(300,500,aggression)
```

The phase is not read from a state snapshot inside `SpawnAI()` — it is passed explicitly to avoid requiring a director reference on the spawner.

**Test objective:** Spawn a group. Players outside the zone draw fire only up to the configured distance, then group returns to waypoint path.

---

### Step G2 — SetFireRateCoef per Phase

**Modify:** `AFM_DiDAttackerDirector.RunDecisionCycle()`

After building battlefield state, apply to all registered groups:
```
foreach entry in m_aGroupRegistry:
    util.SetFireRateCoef(lerp(0.8, 1.2, phaseNormalized) * lerp(0.9, 1.1, m_fAggression))
```

**Test objective:** PROBE-phase groups visibly fire slower than FINAL-phase groups at identical aggression.

---

### Step G3 — Re-apply on Phase Transition

**Modify:** `AFM_DiDAttackerDirector`

Track `m_ePreviousPhase`. On change, re-apply `SetMaxAutonomousDistance` and `SetFireRateCoef` to all groups in registry.

**Test objective:** Phase transitions PROBE → ASSAULT mid-match. All existing groups immediately increase engagement range and fire rate.

---

## Phase H — Player Count Budget Scaling

---

### Step H1 — Reference Player Count Attribute

**Modify:** `AFM_DiDZoneComponent`

```
[Attribute("4", UIWidgets.EditBox, "Reference player count for budget scaling", category: "DiD Zone")]
int m_iReferencePlayerCount;
```

**Test objective:** Attribute visible in editor, defaults to 4, readable at runtime.

---

### Step H2 — Budget Multiplier at Activation

**Modify:** `AFM_DiDZoneComponent.ActivateZone(int carryoverBudget = 0)`

```
int aliveCount = GetAliveDefenderCount();
float modifier = Math.Clamp(Math.Lerp(0.5, 2.0, aliveCount / m_iReferencePlayerCount), 0.5, 2.0);
int scaledBudget = Math.Round(m_iPointsBudget * modifier) + carryoverBudget;
m_Budget = new AFM_DiDAttackerBudget(scaledBudget);
```

**Test objective:** 1 player / ref 4 → ~50% budget. 8 players / ref 4 → ~200% budget. Logged at activation.

---

## Phase I — Aggression Curve & Quiet Intervals

---

### Step I1 — Post-Heavy-Push Quiet Interval

**Modify:** `AFM_DiDAttackerDirector`

Add `int m_iQuietTicksRemaining`. In `RunDecisionCycle()`: if > 0, skip spawner selection (artillery still evaluates), decrement.

There is no explicit SURGE phase in `EAFMAttackPhase` — the surge behavior maps to the FINAL phase. Trigger the quiet interval when a FINAL-phase decision cycle spends more than 50% of remaining budget in a single tick:

```
if state.m_ePhase == EAFMAttackPhase.FINAL && spentThisCycle > remainingBudget * 0.5:
    m_iQuietTicksRemaining = Math.Round(Math.Lerp(3, 1, m_fAggression))
```

**Test objective:** After a large FINAL-phase spend, no new groups spawn for N ticks. Artillery fires normally. Spawning resumes after.

---

### Step I2 — Time-Based Spend Rate Ramp

**Modify:** `AFM_DiDAttackerDirector.RunDecisionCycle()`

Implement spend probability as a gating roll each cycle. Add `m_fBaseSpendProbability` (designer attribute, default 0.7):

```
timeUrgency = 1.0 - state.m_fTimeRatio
spendMultiplier = Math.Lerp(0.8, 1.4, timeUrgency)
effectiveProbability = Math.Min(1.0, m_fBaseSpendProbability * spendMultiplier)

if s_AIRandomGenerator.RandFloat01() > effectiveProbability:
    return   // skip this cycle — only artillery still evaluates
```

**Test objective:** Late in the stage (10% time remaining) spawns are noticeably more frequent than early stage at the same budget ratio.

---

## Phase J — Polish

---

### Step J1 — Near-Miss Detection & Recovery

**Modify:** `AFM_DiDZoneComponent`

```
float m_fCaptureHighWaterMark = 0.0
int   m_iRecoveryTicksRemaining = 0
```

Each tick: update high-water mark. When progress drops from > 0.7 to < 0.3 (players rallied after near-capture), set `m_iRecoveryTicksRemaining = 2`.

Director skips sending new groups to any zone with `m_iRecoveryTicksRemaining > 0` (checked in `BuildBestPackage()` and the normal spawn path). `m_iRecoveryTicksRemaining` decrements in the director tick.

**Test objective:** Push capture to 0.8, add defenders to push it back below 0.3. Director skips 2 ticks before sending new groups.

---

### Step J2 — Radio Announcements on Phase Transition

**Modify:** `AFM_DiDAttackerDirector`

Track `m_ePreviousPhase`. On change call `NotifyPhaseChanged()` on the game mode.

**Test objective:** Phase transitions mid-match trigger radio messages on all clients.

---

## Implementation Order Summary

| Step | Description | Depends on |
|---|---|---|
| P1 | Remove legacy spawner mode + mortar spawner | — |
| A1 | Marker entity classes (extend SCR_AIWaypoint) | — |
| A2 | ApproachRoute container + LighthouseEntity travel time attrs | A1 |
| A3 | Route collection in spawner (index-order matching) | P1, A1, A2 |
| A4 | Waypoint chain generation (permanent entities, no dynamic WPs) | A2, A3 |
| A5 | Config singleton with explicit GetInstance() impl | — |
| B1 | GroupEntry struct + EUnitType enum | A2 |
| B2 | Central group registry + GetDirector() on ZoneComponent | P1, B1 |
| B3 | EGroupControlMode subscription (poll mode inside callback) | B2 |
| B4 | IDLE group handler (FindUndercoveredZone stubbed until E2) | B3 |
| B5 | Agent death event subscription | B2 |
| C1 | Pressure fields + wipe decay on ApproachRoute | A2 |
| C2 | Wipe detection | B5, C1 |
| C3 | Pressure-aware route selection + UpdateRoutePressure in director tick | A3, C2 |
| D1 | CaptureProgress on zone (CAPTURE_RATE as attribute) | — |
| D2 | CaptureProgress in battlefield state | D1 |
| D3 | Capture-based zone end condition (FINISHED_CAPTURED state) | D1 |
| E1 | Stage container (majority-loss timer + threshold attribute) | D1 |
| E2 | Zone system stage activation + budget carryover + FindUndercoveredZone | E1 |
| E3 | Stage win/loss conditions (sustained majority timer, fallback vs respawn, radio notifications) | D3, E2 |
| F1 | AssaultPackage struct | A2 |
| F2 | Package evaluation in director (explicit early return) | B2, D2, F1 |
| F3 | Package execution with timing | F2 |
| G1 | SetMaxAutonomousDistance — phase passed via TriggerSpawn() | B2 |
| G2 | SetFireRateCoef per phase | B2 |
| G3 | Re-apply on phase transition | G1, G2 |
| H1 | Reference player count attribute | — |
| H2 | Budget multiplier at activation (accepts carryover param) | H1, E2 |
| I1 | Post-heavy-push quiet interval (maps to FINAL phase, not SURGE) | — |
| I2 | Time-based spend rate ramp (probability gating) | — |
| J1 | Near-miss detection | D1 |
| J2 | Radio phase announcements | — |
