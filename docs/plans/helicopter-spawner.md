# Helicopter spawner – phase 1 plan

Scope: enemy attack helicopters only. No troop insertion, no planes (the Su-25 mod comes later).

Flight and weapons are handled by **REAPER_AiHelicopters – WCS** (`69A4D664A6284E06`, v1.0.7). The spawner decides when a helicopter comes, where it flies and when it leaves.

Default helicopter: `{3C6B3ED0C3AC30D5}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed_gunship_HE.et` (inherits the vanilla Mi-8 base, which the mod's config supports with weapon slot `Hardpoints`).

## What the mod provides

- **Controller attaches automatically.** Every helicopter prefab in `Configs/REAPER_AiHelicopterConfig.conf` gets a `REAPER_AiHelicopterControllerComponent` (as a helper child if the prefab lacks it). Get it via `SCR_HelicopterControllerComponent.REAPER_GetAiControllerComponent()`.
- **Crew.** Spawn `Prefabs/Groups/OPFOR/REAPER_USSR_HelicopterCrew.et` (4 pilots), then `SCR_AIGroup.REAPER_TeleportGroupInHelicopter(heli, true)` fills pilot, then turret, then cargo seats. It waits for delayed group spawning and retries on its own.
- **Airborne spawn.** A helicopter spawned more than 5 m above ground is held steady until the pilot initialises, then pushed forward. It can appear at the map edge already flying.
- **Waypoints** (all `REAPER_AiHelicopterBaseWaypoint`: max speed, height above terrain, "delete helicopter and crew" flag):
  - `REAPER_AiHelicopterMoveWaypoint` – completes within about 220 m after a timeout. With the delete flag set, the mod deletes the helicopter and crew 1 s after completion.
  - `REAPER_AiHelicopterHoverWaypoint` – holds position, never completes on its own, disables rocket attacks.
  - `REAPER_AiHelicopterLandWaypoint` – not used in phase 1.
- **Attacks are autonomous, on Move waypoints only.** Targets come from the crew group's perception (pilot perception factor ×5), so the helicopter is not omniscient. Priority: heavy vehicles, aircraft, medium vehicles, unarmoured vehicles, fortifications, infantry. Attack runs at about 180–540 m. Turret gunners fire as normal AI.
- **Controller settings:** `REAPER_SetWeaponProperties(rockets, playersOnly, vehiclesOnly, targetTimeout, autoReload, reloadDelay)`, `REAPER_SetOnlyAttackCloseToWaypoint(enabled, minRange 300–1000)`, `REAPER_SetHelicopterAndCrewDamageReduction`, `REAPER_SetLockVehicleForPlayers_S`, `REAPER_SetGameMasterDisabled`.
- **Events:** `REAPER_GetOnPilotInitSuccsessInvoker`, `REAPER_GetOnHelicopterDestroyedInvoker`, `REAPER_GetOnPilotKilledInvoker`, `REAPER_GetOnEnemyPlayerRegisteredInvoker`, `REAPER_GetOnPlayerAttackStartInvoker`.

## New: `AFM_DiDHeliSpawnerComponent : AFM_DiDSpawnerComponent`

One sortie at a time by default:

```
SPAWN ─▶ INBOUND ─▶ ATTACK ─▶ HOLD ─┐
                      ▲             │ (repeat until time on station runs out)
                      └─────────────┘
          any state ─▶ EGRESS ─▶ gone (deleted by the mod at the exit point)
```

1. **SPAWN** – spawn the helicopter at a random child `AFM_SpawnPointEntity`, raised by `m_fSpawnHeightAGL`. Spawn the crew group and teleport it in. Apply controller settings: weapon options, damage reduction 0, locked for players, Game Master access disabled.
2. **INBOUND** – Move waypoint to the edge of the attack area at cruise speed and height.
3. **ATTACK** – 2–4 Move waypoints on a circle of `m_fAttackRadius` (about 400–600 m) around the densest player group, on the side away from the attackers' push. The mod attacks what the crew sees along the way. The group centre comes from `AFM_DiDTargetingHelper` and only decides where to fly.
4. **HOLD** – Hover waypoint at a holding point for `m_iHoldSeconds` (reload, breather for players). Holding points are optional child waypoints placed behind terrain; without them, hold at the entry point.
5. **EGRESS** – Move waypoint back to the entry point with the delete flag set. Triggered by any of: time on station used up, health below `m_fBreakOffHealth`, a gunner killed, the zone leaving ACTIVE/FROZEN.

A destroyed helicopter or killed pilot ends the sortie; the wreck is removed on zone cleanup.

### Settings

| Group | Settings |
| --- | --- |
| Units | helicopter prefabs (default Mi-8MT gunship HE), crew group prefab (default REAPER USSR crew) |
| Timing | first sortie delay after zone goes active, cooldown between sorties, max sorties per zone, chance per check, first/last wave for wave zones (same meaning as `AFM_DiDWaveSpawnerComponent`) |
| Flight | cruise speed and height, attack speed and height, attack radius, attack waypoint count, hold time, time on station, break-off health, spawn height |
| Weapons (passed to the mod) | rockets allowed, players only, vehicles only, only close to waypoint and its range |
| Feedback | supply reward for shooting it down (no warning hint: the helicopter is loud enough to give itself away) |

### Runtime

The spawner runs its own 1 s `CallLater` tick while a sortie is active. `Process()` is only called while a zone is ACTIVE/FROZEN (wave zones: only during an active wave), so it can't be relied on to send the helicopter away when a wave ends.

## Changes to existing code

1. **`AFM_DiDZoneComponent.GetAICountInsideZone`** – skip characters sitting in a helicopter. Otherwise a helicopter over the zone makes attackers outnumber defenders and freezes the timer.
2. **Wave completion** – the crew group is not added to `m_aSpawnedAIGroups`, so a flying helicopter doesn't hold up the end of a wave. The runtime tick sends it to EGRESS when the wave ends.
3. **`Cleanup()`** – delete remaining helicopters with `SCR_EntityHelper.DeleteEntityAndChildren` (the mod removes crews the same way), plus wrecks, crew groups and our waypoints.
4. **`AFM_DiDTargetingHelper`** – move the densest-player-group search and the own-troops distance check out of `AFM_DiDMortarSpawnerComponent`. The mortar keeps its current behaviour.

## Decisions

- No warning hint: the helicopter is loud enough to give itself away.
- `players only` defaults to on, so rockets aren't spent on AI or player-built structures such as AA emplacements.

## Status

Implemented, not yet tested in game: `AFM_DiDTargetingHelper.c`, `Spawners/AFM_DiDHeliSpawnerComponent.c`, the zone count fix, and the mortar switched to the helper.

## Testing in Workbench

- Spawns in the air, crew gets in, flies to the attack area without stalling.
- Attacks only what the crew has seen: approach in cover and check it doesn't go straight for you.
- Holds, returns for another attack, then leaves and is deleted at the exit point.
- Break-off: damage it below the threshold; kill the gunner.
- Zone timer doesn't freeze while it circles overhead; wave zones still complete while it's in the air.
- Losing a zone deletes everything; no leftover waypoints (compare entity count before and after).
