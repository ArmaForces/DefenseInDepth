# AI Commander — General Design Plan

## Overview

A PvE AI Commander that attacks human players defending specified zones. Players win by surviving and holding the majority of zones for a set duration (T minutes). If all players die, the match advances to the next stage.

---

## Game Flow

```
Match
└── Stage 1..N
    ├── Players defend 1-3 zones (final stage = 1 zone, last stand)
    ├── Defense timer T (10-15 min) — players win the stage by holding majority of zones for T
    ├── AI Commander gets P points at stage start (designer-defined, scaled by player count)
    ├── Unspent points carry over to next stage (rewards effective player defense)
    ├── Stage is LOST when AI holds majority of zones for M consecutive seconds (60-90s)
    │   └── Alive players fall back to the next stage on their own — no forced teleport
    └── If all players die → stage transition, current AIs despawned, carryover applied
```

### Stage Loss via Sustained AI Control

When AI captures and **holds** the majority of zones for M seconds without being pushed back, the stage is considered lost. The M-second window exists so players have time to retake the majority before the stage slips away — a successful counter-push resets the timer.

**Timeline of events:**

```
AI takes majority → radio warning "Fall back or retake — enemy holds majority"
...M seconds elapse without players recapturing...
Stage lost → radio "Stage lost — fall back to next position"
    → Next stage zones activate
    → Dead players respawn at next stage spawn points
    → Alive players remain in world, must physically move to next stage area
```

If players recapture majority before M seconds, the timer resets and the warning clears:
```
Players retake majority → radio "Zone majority recaptured — hold your ground!"
    → AI majority timer reset to 0
```

This mechanic rewards aggressive defence (retaking before the clock runs out) while still giving the AI a credible path to victory even if players are alive. Alive players being responsible for their own fallback creates a tense retreat dynamic.

---

## Commander Loop

Runs on discrete 30-second ticks.

```
Tick() {
    UpdateGroupRegistry()       // poll alive counts, handle mode changes
    UpdateZoneCoverage()        // compute real coverage per zone
    UpdateRoutePressure()       // track route casualties
    UpdateMortar()              // fire mission or respawn decision
    stance = EvaluateStance()   // PROBE / COMMIT / REINFORCE / PIVOT / SURGE
    spendBudget = BudgetRemaining * SpendRate(stance, aggressiveness)
    allocations = AllocateBudget(spendBudget, zones)
    foreach zone, allocation in allocations:
        IssueAssaultPackage(zone, allocation)
}
```

---

## Battlefield Knowledge

Commander is omniscient — it observes every tick:

- Player positions and count per zone
- Zone capture progress (0.0 → 1.0)
- Active AI groups: position, alive count, `EGroupControlMode`
- Remaining budget
- Time remaining in stage
- Time of day (day/night, affects mortar round selection)

---

## Group Feedback

```
enum EGroupControlMode {
    NONE,
    IDLE,               // no waypoints, not engaging — reorder or dead
    AUTONOMOUS,         // engaging enemy — in contact, leave alone
    FOLLOWING_WAYPOINT, // en route — no action needed
    LAST,
}
```

- Mode changes are event-driven (notification off-loop)
- Alive count is polled each tick
- Groups do not proactively report to commander

### Group Registry Entry

```
GroupEntry {
    Group           group
    Zone            assignedZone
    ApproachRoute   assignedRoute
    UnitType        type          // Infantry, Mechanized, Mortar
    int             lastAliveCount
    EGroupControlMode lastMode
}
```

### Per-Tick Group Update

```
UpdateGroup(entry) {
    if aliveCount == 0:
        → Unregister, free zone slot

    if mode == IDLE && aliveCount > 0:
        if PlayersInsideZone(zone):
            → Issue patrol waypoints within zone
        else if !ZoneCaptured(zone):
            → Issue sweep waypoints through zone center
        else:
            target = FindUndercoveredZone()
            → IssueWaypoint(group, target.center)   // direct, no lighthouse
            UpdateAssignment(entry, target)

    if mode == AUTONOMOUS:
        → Leave alone (in contact)

    if mode == FOLLOWING_WAYPOINT:
        → No action
}
```

---

## Zone Coverage Model

```
ZoneCoverage {
    int     groupsEnRoute       // FOLLOWING_WAYPOINT toward zone
    int     groupsEngaging      // AUTONOMOUS near zone
    int     groupsIdle          // IDLE at zone (capping or patrolling)
    float   captureProgress     // 0.0 → 1.0
    int     stallTicks          // ticks with no capture progress change
}
```

**Stall detection:** zone has `groupsEngaging > 0` but `captureProgress` unchanged for 2+ ticks → players are holding strongly → PIVOT or escalate unit type.

---

## Approach Routes & Designer Markers

Each zone has 1–N approach routes. Mission designer places all markers.

### ApproachRoute

```
ApproachRoute {
    Entity  stagingPoint            // infantry regroups here (covered, out of player LOS)
    Entity  lighthouse              // entry direction marker
    Entity  vehicleOverwatchPosition // hull-down fire position before zone
    float   infantryTravelTicks     // estimated spawn-to-zone travel time
    float   mechanizedTravelTicks
}
```

### Infantry Waypoint Chain

```
SpawnPoint → StagingPoint → Lighthouse → Zone center (ATTACK)
```

### Mechanized Waypoint Chain

```
SpawnPoint → Lighthouse → VehicleOverwatchPosition → Zone center
```

Vehicles don't use the staging point. They move to overwatch, provide suppressive fire while infantry engages, then push in.

### Full Designer Marker List Per Zone

| Marker | Per | Purpose |
|---|---|---|
| Staging point | Per approach route | Infantry regroup, covered position |
| Lighthouse | Per approach route | Entry direction waypoint |
| Vehicle overwatch position | Per approach route | Hull-down fire position before zone |
| Infantry spawn points | Per zone | Multiple, spread to avoid clustering |
| Vehicle spawn points | Per zone | Road-accessible |
| Mortar spawn point | Per zone | Where mortar asset is placed |

---

## Route Pressure Tracking

```
RoutePressure {
    ApproachRoute   route
    int             groupsSentThrough
    int             groupsWipedOnRoute
    int             ticksSinceLastDeath
}
```

**Route selection logic:**
- Prefer routes with lowest wipe ratio relative to groups sent
- After a wipe: apply cooldown (skip route for N ticks, scaled by aggressiveness)
- Avoid sending 3+ consecutive groups through the same route — rotate for variety

---

## Resource Budget

```
StageStart() {
    budget = stageDef.designerBudget * PlayerCountModifier()
           + carryoverFromPreviousStage
}

StageEnd() {
    carryoverFromPreviousStage = currentBudget
}

PlayerCountModifier() = lerp(0.5, 2.0, playerCount / referencePlayerCount)
```

### Unit Costs

| Unit Type | Cost | Role |
|---|---|---|
| Infantry squad | Low | Zone capture, volume, cheap |
| Mechanized (IFV + squad) | Medium | Breakthrough, vehicle threat |
| Mortar respawn | Significant | Restore fire support asset |
| Armor (MBT) | High | Psychological pressure |

---

## Commander Stance State Machine

```
PROBE ──(weak zone identified)──► COMMIT
COMMIT ──(stall 2+ ticks)───────► REINFORCE or PIVOT
COMMIT ──(< 25% time left)──────► SURGE
SURGE ──(budget depleted)───────► HOLD
```

| Stance | Behavior |
|---|---|
| PROBE | Small multi-directional attacks, identify least-defended zone / route |
| COMMIT | Concentrate spending on highest-priority zone |
| REINFORCE | Zone stalling, send more to same zone |
| PIVOT | Zone is a killzone, switch to next-best zone |
| SURGE | Dump remaining budget, simultaneous push on all under-held zones |

### Intra-Stage Aggression Curve

Aggression ramps naturally within a stage to create a dramatic arc:

```
PROBE → pressure build → big push → brief recovery → build again → SURGE at end
```

After a SURGE or heavy COMMIT, apply a **quiet interval** (2-3 ticks of reduced spend rate) before resuming. This creates tension-and-relief cycles.

### Last-Stand Stage (1 zone)

Probe phase still runs but is capped:

```
maxProbeTicks = max(1, BASE_PROBE_TICKS - stageIndex)
```

On the final stage, probe lasts 1 tick. Its purpose shifts from "which zone to focus on" (only one zone) to "which approach route is weakest" — one light group per lighthouse, simultaneously, then commit heavies through the least-contested route.

---

## Zone Priority Scoring

```
Priority = PlayerPresence * Wp
         + CaptureProgress * Wc      // almost captured → finish it
         - AICoverage * Wa           // already covered → deprioritize
         + StallPenalty * Ws         // stalling → consider pivot
         + (1 - TimeRatio) * Wt     // late game → all scores amplified
```

### Budget Allocation (all zones contested)

```
AllocateBudget(spendBudget) {
    scores = [Priority(z) for z in zones]
    total  = sum(scores)
    foreach zone, score:
        share = (score / total) * spendBudget
        share = max(share, MIN_ZONE_BUDGET)    // always contest every zone
        SpendOnZone(zone, share)
}
```

---

## Assault Packages (Combined Arms)

Instead of spawning units individually, the commander issues coordinated packages:

| Package | Composition | When |
|---|---|---|
| Soft probe | 1x infantry | PROBE phase |
| Fire and move | Mortar (HE or smoke) + 1x infantry | COMMIT, normal |
| Combined assault | Mortar (HE) + infantry + mechanized | COMMIT, stalled zone |
| Night assault | Mortar (illumination) + 2x infantry | COMMIT, night |
| Surge wave | Mortar (smoke at lighthouse) + 2x infantry + mechanized | SURGE |

### Synchronized Arrival

Travel times per route are known. Commander back-calculates spawn offsets so all units in a package arrive at the zone simultaneously:

```
targetArrivalTick = currentTick + max(travelTimes in package)

foreach unit in package:
    spawnDelay = targetArrivalTick - unit.route.travelTicks
    ScheduleSpawn(unit, currentTick + spawnDelay)
```

### Emergent Consolidation at Staging Point

Two infantry groups spawned on the same route in consecutive ticks will naturally converge at the staging point (first group dwells briefly, second catches up) and advance together toward the lighthouse — free coordination without explicit synchronization.

### Example Assault Choreography

```
T=0  (tick N):    Mortar fires smoke at lighthouse position
T=0  (tick N):    Infantry spawned → staging point
T=+1 (tick N+1):  Infantry at staging point, visibly massing
                  Mortar fires HE at zone (pre-assault suppression)
                  Vehicle spawned → lighthouse → overwatch position
T=+2 (tick N+2):  Infantry advances through smoke at lighthouse
                  Vehicle reaches overwatch, opens suppressive fire
T=+3 (tick N+3):  Infantry and vehicle hit zone simultaneously
```

Players can see the staging, creating a beat to react — making defense feel skillful.

---

## Mortar

Physical in-game asset. Spawned at designer-placed spawn point. Unlimited rounds, destructible.

```
MortarState {
    bool    isAlive
    int     lastFireTick
    int     COOLDOWN_TICKS      // scaled by aggressiveness
}
```

### Per-Tick Update

```
UpdateMortar() {
    if !isAlive:
        if ShouldRespawn():     // budget check + tactical value
            SpendPoints(MORTAR_RESPAWN_COST)
            Spawn()
        return

    if currentTick - lastFireTick >= COOLDOWN_TICKS:
        zone      = SelectFireZone()
        roundType = SelectRoundType(zone)
        IssueMission(zone, roundType)
        lastFireTick = currentTick
}
```

### Fire Mission Type Selection

Priority: **Illumination > HE > Smoke > Practice**

```
SelectRoundType(zone) {
    if IsNight() && AIAssaultIncoming(zone):
        → ILLUMINATION  (aim: zone center)

    else if AIAssaultIncoming(zone) && HighPlayerDensity(zone):
        → HE            (aim: Monte Carlo optimal point)

    else if AIApproachRouteExposed(zone):
        → SMOKE         (aim: lighthouse position — screens advance)

    else if PlayersMovingBetweenZones():
        → SMOKE         (aim: zone — API handles perpendicular geometry)

    else:
        → PRACTICE      (aim: Monte Carlo optimal point)
}
```

### Fire Zone Selection

```
SelectFireZone() {
    if zone = ZoneWithActiveAssault():       → return zone  // support the push
    if zone = ZoneAboutToBeAssaulted():      → return zone  // pre-assault suppression
    if zone = ZoneWithMostPlayers():         → return zone  // maximum disruption
}
```

---

## Aggressiveness Setting

Single `float aggressiveness` (0.0–1.0) that parameterizes the whole system:

| Parameter | Low (0.0) | High (1.0) |
|---|---|---|
| Base spend rate | 10%/tick | 30%/tick |
| Probe phase duration | 3 ticks | 1 tick |
| Route cooldown after wipe | 4 ticks | 1 tick |
| Preferred unit mix | Infantry-heavy | Mechanized/Armor-heavy |
| SURGE trigger (time remaining) | 15% | 35% |
| Mortar cooldown | 4 ticks | 1 tick |
| Mortar respawn willingness | Low | High |
| IDLE re-order delay | 2 ticks | Immediate |
| MIN_ZONE_BUDGET share | 5% of spend | 15% of spend |
| Post-surge quiet interval | 3 ticks | 1 tick |
| Preferred HE vs Practice bias | Practice | HE |

---

## Fun Experience Considerations

The commander's goal is to create a fun experience, not to optimally crush players.

- **Pulsed pressure, not constant drip** — save budget for 2-3 ticks then release a wave; constant small spawns are less exciting than real assaults
- **Post-surge quiet interval** — after a heavy push, reduce spend rate; players need tension and relief, not relentless pressure
- **Telegraph major attacks** — mortar fires 1 tick before a surge; players see smoke/suppression and know something is coming, giving them agency
- **Route rotation even when one route is "optimal"** — a small random weight in route selection makes the AI feel like a commander, not a solver
- **Near-miss awareness** — if a zone was almost captured and players rallied, don't immediately flood it again; let the player win land before building pressure again
- **Variety in unit composition** — don't always send the cheapest unit; mechanized and armor create different defensive challenges that keep gameplay fresh
- **Carryover as escalation** — unspent points carrying to the next stage means a perfectly-defended stage leads to a harder next stage, creating natural narrative escalation
