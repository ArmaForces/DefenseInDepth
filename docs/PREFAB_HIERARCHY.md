# DefenseInDepth — Prefab Hierarchy Reference

This document describes the full entity hierarchy for a single Defense in Depth zone,
using Zone 1 (Outskirts, Lamentin) as the concrete example.

---

## Overview

Each zone is a self-contained entity tree. The zone entity is the root; everything the
zone needs — its boundary, defender spawn, attacker director, AI spawners, and optional
artillery — lives as a child of that root.

```
AFM_DiDZoneEntity               ← zone root (carries AFM_DiDZoneComponent)
├── PolylineShapeEntity         ← zone boundary polygon
├── AFM_PlayerSpawnPointEntity  ← defender respawn point
├── AFM_ArtillerySpawnPointEntity × N  ← mortar spawn markers (optional)
└── AFM_DiDAttackerDirector     ← AI decision-maker
    ├── AFM_DiDZoneArtillery    ← mortar system (optional)
    ├── AFM_DiDInfantrySpawnerComponent
    │   ├── AFM_SpawnPointEntity × N
    │   └── AIWaypoint × N
    └── AFM_DiDMechanizedSpawnerComponent
        ├── AFM_SpawnPointEntity × N
        └── AIWaypoint × N
```

---

## Full Hierarchy (Zone 1 — "Outskirts")

### 1. Zone Root

```
AFM_DiDZoneEntity  "zone1"
  components:
    AFM_DiDZoneComponent
      m_sZoneName             = "Outskirts"
      m_iZoneIndex            = 1
      m_iPrepareTimeSeconds   = 900   (15 min prep)
      m_iDefenseTimeSeconds   = 900   (15 min defense)
      m_iPointsBudget         = 60    (attacker budget)
      m_fBudgetRolloverFraction = 0.9
    Hierarchy
```

**Script:** `Scripts/Game/DiD/AFM_DiDZoneComponent.c`

The component is the zone's brain. On `LateInit` it walks all immediate children,
categorises them by type, then hands references off to the director. It runs the
state machine (`INACTIVE → PREPARE → ACTIVE → FINISHED_*`) and calls
`AFM_DiDAttackerDirector.Process()` every second while active.

---

### 2. Zone Boundary

```
└── PolylineShapeEntity  "zone_1"
      IsClosed = true
      Points: 29 ShapePoint entries  (world-space XZ polygon)
      └── PrefabGeneratorEntity
            m_PrefabNames: ZoneBorderMarker_Blue.et
            (auto-generates visual border markers along the polyline)
```

The polyline defines the capture area used for:
- AI count checks (`GetAICountInsideZone`)
- Smoke/HE target sampling bounds (Monte Carlo)
- `Math2D.IsPointInPolygon` calls throughout the zone logic

---

### 3. Defender Spawn Point

```
└── AFM_PlayerSpawnPointEntity
      prefab: Prefabs/MP/AFM_DiDPlayerSpawnPointPrefab.et
      coords: relative to zone origin
```

One per zone. Defenders (BLUFOR players) respawn here. The zone component
caches this as `m_PlayerSpawnPoint` and exposes it via `GetPlayerSpawnPoint()`.

---

### 4. Artillery Spawn Points

```
└── AFM_ArtillerySpawnPointEntity  × N      (3 in Zone 1)
      prefab: Prefabs/MP/AFM_ArtillerySpawnPointEntity.et
      coords: [different positions scattered around zone perimeter]
```

These are position markers only — no logic. The zone component collects them
all into `m_aArtillerySpawnPoints[]` during `LateInit`, then passes the array
to `AFM_DiDAttackerDirector.Init()`, which in turn passes it to
`AFM_DiDZoneArtillery.Initialize()`.

The mortar spawns at one of these positions. On respawn it picks a different
one (up to 5 random attempts to avoid the last-used point).

---

### 5. Attacker Director

```
└── AFM_DiDAttackerDirector
      prefab: Prefabs/MP/AFM_DiDAttackerDirector_Default.et
      m_iDecisionIntervalSeconds = 25
      m_fAggression              = 0.75
```

**Script:** `Scripts/Game/DiD/AFM_DiDAttackerDirector.c`

Central coordinator. On each decision cycle it:
1. Builds a `AFM_DiDBattlefieldState` snapshot (defender count, budget ratio, phase, day/night)
2. Scores all child spawners via `ScoreRequest(state)`
3. Picks a spawner using weighted-random from the top 3 candidates
4. Separately evaluates and triggers artillery missions / mortar respawns

Children of the director are discovered in `Init()` by walking `GetChildren()`.

---

### 5a. Zone Artillery (child of Director)

```
    └── AFM_DiDZoneArtillery
          prefab: Prefabs/MP/AFM_DiDZoneArtilleryPrefab.et
          m_sMortarPrefab         = DiD_MortarComposition_US.et
          m_CrewConfig:
            m_sGunnerPrefab       = Character_USSR_Engineer.et
            m_bSpawnDriver        = false
          m_iHECooldownSeconds    = 90
          m_iSmokeCooldownSeconds = 60
          m_iIllumCooldownSeconds = 180
          m_fSampleRadius         = 25
          m_fSmokeMinDistance     = 25   (smoke screen: min offset from player blob)
          m_fSmokeMaxDistance     = 50   (smoke screen: max offset from player blob)
          m_fSmokeScreenSpread    = 20   (smoke screen: perpendicular spread per side)
```

**Script:** `Scripts/Game/DiD/AFM_DiDZoneArtillery.c`

Not a spawner — managed directly by the director. Lifecycle:

| Event | Action |
|---|---|
| `Initialize()` | Spawns mortar at a random `AFM_ArtillerySpawnPointEntity` (free) |
| Every second | `CheckMortarAlive()` polls damage state |
| Director cycle | `ScoreArtilleryMission()` / `TriggerMission()` |
| Mortar destroyed | `HandleMortarDestroyed()`, director may call `TriggerRespawn()` (max 2×, budget cost) |
| Zone ends | `Cleanup()` deletes mortar + all active waypoints |

**Smoke mission targeting** (as of current version): fires 3 sequential waypoints
in a line perpendicular to the mortar→player-blob axis, centered 25–50 m in front
of the player blob. Falls back to zone centroid when no alive defenders are found.

---

### 5b. Mortar Composition (spawned at runtime)

```
DiD_MortarComposition_USSR.et  (or _US.et)
  Turret : Mortar_2B14.et      ← the mortar weapon entity
    └── GenericEntity : AmmoBoxArsenal_Mortar_USSR.et  ← ammo supply
```

**Prefab path:** `Prefabs/Compositions/`

These are not placed in the world editor — they are spawned at runtime by
`AFM_DiDZoneArtillery.SpawnMortarTeam()` at the chosen `AFM_ArtillerySpawnPointEntity`.
An AI crew (gunner only) is spawned into the mortar's compartment via `AFM_CrewConfig`.

---

### 5c. Mechanized Spawner (child of Director)

```
    └── AFM_DiDMechanizedSpawnerComponent
          prefab: Prefabs/Spawners/AFM_MechanizedSpawner.et
          ├── AFM_SpawnPointEntity  "zone_1_sp_10"
          │     prefab: Prefabs/MP/AFM_DiDSpawnPointAI.et
          ├── AFM_SpawnPointEntity  "zone_1_sp_11"
          └── SCR_DefendWaypoint  "zone1_aiwp_1_def2"
                prefab: Prefabs/AI/Waypoints/DiD_AIWaypoint_Defend.et
                CompletionRadius: 80
```

Spawns vehicle groups (APCs, IFVs) at one of its `AFM_SpawnPointEntity` children.
The waypoints are assigned to the spawned group.

---

### 5d. Infantry Spawner (child of Director)

```
    └── AFM_DiDInfantrySpawnerComponent
          prefab: Prefabs/Spawners/AFM_DidInfantrySpawner.et
          ├── AFM_SpawnPointEntity  "zone_1_sp_1"   (5 total)
          ├── AFM_SpawnPointEntity  "zone_1_sp_2"
          ├── AFM_SpawnPointEntity  "zone_1_sp_3"
          ├── AFM_SpawnPointEntity  "zone_1_sp_8"
          ├── AFM_SpawnPointEntity  "zone_1_sp_9"
          ├── SCR_DefendWaypoint  "zone1_aiwp_1_def"   CompletionRadius: 80
          ├── SCR_DefendWaypoint  "zone1_aiwp_2_def"   CompletionRadius: 120
          └── SCR_SearchAndDestroyWaypoint  "zone_1_aiwp_3_sead"
```

Spawns infantry groups at one of its spawn point children. The director's
weighted-random selection decides which spawner fires each cycle — both mechs and
infantry compete for the same decision slot.

---

## Supporting Layer Entities (not zone children)

These sit in the same layer file but are **not** parented to the zone:

```
GenericEntity        : ArsenalBox_US.et          ← player equipment box
PS_ManualMarker      : SupportStationMarker.et   ← marks arsenal location on map
GenericEntity        : DiD_BuildingService_US.et ← service station composition
PS_ManualMarker × 3 : AttackMarker_Red.et        ← attack direction arrows (map markers)
```

---

## Prefab Reference Table

| Prefab file | Entity type | Purpose |
|---|---|---|
| `MP/AFM_DiDZonePrefab.et` | `AFM_DiDZoneEntity` | Zone root with `AFM_DiDZoneComponent` |
| `MP/AFM_DiDZoneWavesPrefab.et` | `AFM_DiDZoneEntity` | Zone root with `AFM_DiDWaveZoneComponent` (wave variant) |
| `MP/AFM_DiDPlayerSpawnPointPrefab.et` | `AFM_PlayerSpawnPointEntity` | Defender respawn point |
| `MP/AFM_ArtillerySpawnPointEntity.et` | `AFM_ArtillerySpawnPointEntity` | Mortar spawn marker |
| `MP/AFM_DiDAttackerDirector_Default.et` | `AFM_DiDAttackerDirector` | AI decision coordinator |
| `MP/AFM_DiDZoneArtilleryPrefab.et` | `AFM_DiDZoneArtillery` | Mortar system (child of director) |
| `MP/AFM_DiDSpawnPointAI.et` | `AFM_SpawnPointEntity` | AI group spawn position |
| `Compositions/DiD_MortarComposition_USSR.et` | `Turret` (2B14) | Runtime-spawned mortar (USSR) |
| `Compositions/DiD_MortarComposition_US.et` | `Turret` (M252) | Runtime-spawned mortar (US) |
| `Modes/AFM_GameMode_DiD.et` | `PS_GameModeCoop` | Game mode entity (placed once per world) |

---

## Setup Checklist for a New Zone

1. Place `AFM_DiDZonePrefab.et` in the layer — this is your zone root.
2. Set `m_sZoneName`, `m_iZoneIndex`, `m_iDefenseTimeSeconds`, `m_iPointsBudget`.
3. Add a `PolylineShapeEntity` child — draw the capture boundary (minimum 3 points, `IsClosed = true`).
4. Add `AFM_DiDPlayerSpawnPointPrefab.et` as a child — position behind the defenders' line.
5. Add `AFM_DiDAttackerDirector_Default.et` as a child of the zone.
6. Add spawner prefabs as children of the **director** (not the zone):
   - `AFM_DidInfantrySpawner.et` with `AFM_DiDSpawnPointAI.et` children and waypoints.
   - `AFM_MechanizedSpawner.et` (optional) with its own spawn points and waypoints.
7. For artillery support:
   - Add `AFM_DiDZoneArtilleryPrefab.et` as a child of the **director**.
   - Add 1–3 `AFM_ArtillerySpawnPointEntity.et` as children of the **zone** (not the director).
   - Set `m_sMortarPrefab` on the artillery entity to either mortar composition.
8. Set `m_iZoneIndex` sequentially (1 = first zone played, N = last).
