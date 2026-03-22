# AI Commander — API Reference

APIs needed for the commander implementation, organised by subsystem.
Entries marked **[confirmed]** are already in use in the existing codebase.
Entries marked **[found]** were discovered in the Enfusion API but not yet used.
Entries marked **[unknown]** need verification in-engine before use.

---

## 1. Timing & Loop

| API | Usage |
|---|---|
| `ChimeraWorld world = GetGame().GetWorld()` | Get world reference **[confirmed]** |
| `WorldTimestamp now = world.GetServerTimestamp()` | Current server time **[confirmed]** |
| `now.DiffSeconds(other)` | Elapsed time between two timestamps **[confirmed]** |
| `now.PlusSeconds(n)` | Offset a timestamp forward **[confirmed]** |
| `GetGame().GetCallqueue().CallLater(fn, ms, repeat, args...)` | Deferred / staggered execution — used for scheduled package spawns **[confirmed]** |

The commander tick is driven by `Process()` called from the zone every second; the interval check `DiffSeconds >= m_iDecisionIntervalSeconds` gates actual work.

---

## 2. Entity Spawning

| API | Usage |
|---|---|
| `EntitySpawnParams params = new EntitySpawnParams()` | Spawn configuration **[confirmed]** |
| `spawnPoint.GetWorldTransform(mat); params.Transform = mat` | Place entity at spawn point transform **[confirmed]** |
| `GetGame().SpawnEntityPrefab(Resource.Load(prefab), world, params)` | Spawn any prefab (groups, vehicles, waypoints) **[confirmed]** |
| `SCR_EntityHelper.DeleteEntityAndChildren(entity)` | Clean up spawned entities **[confirmed]** |
| `GetGame().GetWorld().GetSurfaceY(x, z)` | Terrain height lookup — used to ground waypoint entities **[confirmed]** |

Waypoints themselves are spawned as entities via `SpawnEntityPrefab`. The artillery support waypoint prefab path is already confirmed:
```
{C524700A27CFECDD}Prefabs/AI/Waypoints/AIWaypoint_ArtillerySupport.et
```
All other runtime waypoint and marker prefab paths are provided via the **Config singleton** (see section 12).

---

## 3. AI Group Management

| API | Usage |
|---|---|
| `AIGroup group = AIGroup.Cast(spawnedEntity)` | Cast spawned entity to group **[confirmed]** |
| `group.AddWaypoint(waypoint)` | Append waypoint to group's queue **[confirmed]** |
| `group.RemoveWaypoint(waypoint)` | Remove a specific waypoint **[confirmed]** |
| `group.GetWaypoints(out array<AIWaypoint>)` | Read current waypoint list **[confirmed]** |
| `group.GetAgentsCount()` | Alive agent count — primary group health signal **[confirmed]** |
| `group.GetAgents(out array<AIAgent>)` | Get all agents in group **[confirmed]** |

### Waypoint Chain (staging → lighthouse → zone)

Multiple waypoints are added sequentially with `AddWaypoint`. The engine executes them in insertion order:

```cpp
group.AddWaypoint(stagingWaypoint)     // 1. regroup at staging point
group.AddWaypoint(lighthouseWaypoint)  // 2. approach via entry route
group.AddWaypoint(zoneWaypoint)        // 3. assault zone
```

To reassign an IDLE group to another zone, clear first then re-add:
```cpp
array<AIWaypoint> current = {};
group.GetWaypoints(current);
foreach (AIWaypoint wp : current)
    group.RemoveWaypoint(wp);
group.AddWaypoint(newTargetWaypoint);
```

---

## 4. Group State — SCR_AIGroupInfoComponent

Access path:
```cpp
SCR_AIGroupUtilityComponent util = SCR_AIGroup.Cast(aiGroup).GetGroupUtilityComponent();
SCR_AIGroupInfoComponent info = util.m_GroupInfo;
```

| API | Usage |
|---|---|
| `info.GetGroupControlMode()` | Poll current `EGroupControlMode` **[found]** |
| `info.GetOnControlModeChanged()` | `ScriptInvoker` — fires off-loop on any mode change **[found]** |
| `info.IsIllumFlareAllowed()` | Whether the group is permitted to use illumination flares **[found]** |

```cpp
enum EGroupControlMode
{
    NONE = 0,
    IDLE,               // no waypoints, not engaging — reorder or dead
    AUTONOMOUS,         // engaging enemy — leave alone
    FOLLOWING_WAYPOINT, // en route — no action needed
    LAST,
}
```

**Preferred usage:** subscribe to `GetOnControlModeChanged()` at group registration time for off-loop IDLE detection. Poll `GetGroupControlMode()` only during the tick for groups that haven't triggered the event.

---

## 5. Group Behaviour Control — SCR_AIGroupUtilityComponent

Access path (chained from group):
```cpp
SCR_AIGroupUtilityComponent util = SCR_AIGroup.Cast(aiGroup).GetGroupUtilityComponent();
```

| API | Usage |
|---|---|
| `util.SetCombatMode(EAIGroupCombatMode mode)` | Override combat behaviour from outside **[found]** |
| `util.GetCombatModeActual()` | Read the effective combat mode (may differ from external) **[found]** |
| `util.GetThreatMeasure()` | 0.0–N float — current threat level perceived by group **[found]** |
| `util.SetMaxAutonomousDistance(float dist)` | Limits how far from current position group engages autonomously **[found]** |
| `util.SetFireRateCoef(float coef)` | Scale fire rate (1.0 = normal, 0.5 = suppressed, 2.0 = aggressive) **[found]** |
| `util.m_OnMoveFailed` | `ScriptInvoker` — fires when group cannot reach its waypoint destination **[found]** |
| `util.m_OnAgentLifeStateChanged` | `ScriptInvokerBase` — fires when any agent in the group dies **[found]** |

### Commander-relevant applications

**Route pressure detection** — subscribe to `m_OnMoveFailed` when registering a group. A failure increments the route's wipe counter, triggering a cooldown on that lighthouse for future spawns.

**Threat-based stall detection** — `GetThreatMeasure() > threshold` while zone capture progress is unchanged for 2+ ticks indicates a heavily defended zone. More precise than capture-progress-only detection.

**Behaviour shaping by phase:**

| Phase | `SetCombatMode` | `SetMaxAutonomousDistance` | `SetFireRateCoef` |
|---|---|---|---|
| PROBE | `COMBAT` (default) | 150m | 0.8 |
| ASSAULT | `COMBAT` | 300m | 1.0 |
| SURGE | `COMBAT` | 500m | 1.5 |

Setting `SetMaxAutonomousDistance` prevents groups from chasing players away from their objective and keeps them on task.

---

## 6. Group Perception — SCR_AIGroupPerception

Access path (chained from group):
```cpp
SCR_AIGroupPerception perception = SCR_AIGroup.Cast(aiGroup).GetGroupUtilityComponent().m_Perception;
```

| API | Usage |
|---|---|
| `perception.GetOnEnemyDetected()` | Event when group first spots an enemy **[found]** |
| `perception.GetOnEnemyDetectedFiltered()` | Fires once per perception update (cheaper than full event) **[found]** |
| `perception.GetOnNoEnemy()` | Event when group loses all contacts — zone possibly cleared **[found]** |
| `perception.m_MostDangerousCluster` | `SCR_AIGroupTargetCluster` — the highest-threat target cluster **[found]** |
| `perception.m_aTargetEntities` | All currently known enemy entities **[found]** |
| `perception.CalculateClusterDangerScore(cluster)` | Float danger score for a target cluster **[found]** |

### SCR_AIGroupTargetCluster

| Property | Usage |
|---|---|
| `cluster.m_aEntities` | Entities (players) in this cluster |
| `cluster.m_aDistances` | Distances to each entity from group |
| `cluster.GetMinDistance()` | Distance to nearest enemy in cluster |

### Commander-relevant applications

- `GetOnEnemyDetected()` at registration → trigger mortar support for the group's assigned zone when contact is made
- `GetOnNoEnemy()` → zone may be clear; check `EGroupControlMode` to confirm and reassign IDLE group
- `m_MostDangerousCluster.m_aEntities.Count()` → enemy density signal, supplements the Monte Carlo mortar targeting

---

## 7. Waypoint Types

### Artillery Support Waypoint (SCR_AIWaypointArtillerySupport)

Used for mortar fire missions. **[confirmed]**

```cpp
SCR_AIWaypointArtillerySupport wp = SCR_AIWaypointArtillerySupport.Cast(spawnedEntity);
wp.SetTargetShotCount(count);
wp.SetAmmoType(SCR_EAIArtilleryAmmoType.XX);  // enum values — see below
wp.SetActive(true);
```

### SCR_EAIArtilleryAmmoType

Defined in `SCR_AIStaticArtilleryVehicleUsageComponent`. The component also exposes:
```cpp
SCR_AIStaticArtilleryVehicleUsageComponent artComp =
    SCR_AIStaticArtilleryVehicleUsageComponent.Cast(mortar.FindComponent(...));
ResourceName ammoRes = artComp.GetAmmoResourceName(SCR_EAIArtilleryAmmoType.XX);
```

```cpp
enum SCR_EAIArtilleryAmmoType
{
    HIGH_EXPLOSIVE,
    SMOKE,
    ILLUMINATION,
    PRACTICE
}
```

> `SCR_DeploySmokeCoverWaypoint` is handled autonomously by AI agents — no commander input needed. Agents deploy local smoke cover on their own as part of their combat behaviour.

---

## 8. Player & Faction Queries

| API | Usage |
|---|---|
| `SCR_Faction faction = m_Zone.GetDefenderFaction()` | Get defender faction **[confirmed]** |
| `faction.GetPlayersInFaction(out array<int> playerIds)` | All player IDs in faction **[confirmed]** |
| `GetGame().GetPlayerManager().GetPlayerController(playerId)` | Player controller from ID **[confirmed]** |
| `pc.GetControlledEntity()` | The character entity a player controls **[confirmed]** |
| `SCR_ChimeraCharacter.Cast(entity)` | Cast entity to character **[confirmed]** |
| `character.GetDamageManager().GetState() == EDamageState.DESTROYED` | Check if player is dead **[confirmed]** |
| `entity.GetOrigin()` | World position of any entity **[confirmed]** |
| `vector.DistanceSq(a, b)` | Squared distance check **[confirmed]** |

---

## 9. Zone Geometry

| API | Usage |
|---|---|
| `PolylineShapeEntity polyline = m_Zone.GetPolylineEntity()` | Zone boundary shape **[confirmed]** |
| `polyline.GetPointsPositions(out array<vector>)` | All boundary vertices **[confirmed]** |
| `polyline.GetOrigin()` | Polyline world-space origin offset **[confirmed]** |
| `Math2D.IsPointInPolygon(floatArray, x, z)` | Point-in-polygon test **[confirmed]** |

`Math2D.IsPointInPolygon` expects a flat `array<float>` of `[x0, z0, x1, z1, ...]` world-space pairs (Y ignored).

---

## 10. Day / Night

| API | Usage |
|---|---|
| `ChimeraWorld cw = ChimeraWorld.CastFrom(GetGame().GetWorld())` | Cast world **[confirmed]** |
| `cw.GetTimeAndWeatherManager().IsSunSet()` | Returns true at night — triggers illumination rounds **[confirmed]** |

---

## 11. Vehicle & Crew

| API | Usage |
|---|---|
| `SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(...))` | Compartment manager for crewing **[confirmed]** |
| `m_crewConfig.SpawnCrew(cm, waypoint)` | Spawn crew into vehicle with first waypoint — returns `AIGroup` **[confirmed]** |
| `SCR_CharacterDamageManagerComponent.SetPermitUnconsciousness(false, true)` | Disable AI unconsciousness on spawn **[confirmed]** |

---

## 12. Config Singleton (Prefab Paths)

All runtime-spawned waypoint and marker prefab paths are exposed via a script Config component attached to the game mode, available as a singleton instance. This avoids hardcoding resource GUIDs in the commander logic.

```cpp
AFM_DiDCommanderConfig cfg = AFM_DiDCommanderConfig.GetInstance();
ResourceName moveWpPrefab   = cfg.m_sMoveWaypointPrefab;
ResourceName attackWpPrefab = cfg.m_sAttackWaypointPrefab;
```

Paths to confirm and register in the config:
- Standard move waypoint prefab (for staging point and lighthouse waypoints)
- Standard attack waypoint prefab (for zone assault)
- Artillery support waypoint (already hardcoded — can migrate to config)

---

## 13. Budget

| API | Usage |
|---|---|
| `AFM_DiDAttackerBudget budget = zone.GetBudget()` | Get zone budget **[confirmed]** |
| `budget.CanAfford(cost)` | Pre-spawn affordability check **[confirmed]** |
| `budget.Consume(cost)` | Deduct from budget after spawn **[confirmed]** |
| `budget.GetRatio()` | 0.0–1.0 remaining ratio — drives phase transitions **[confirmed]** |
| `budget.IsExhausted()` | True when budget ≤ 0 **[confirmed]** |

---

## 14. Randomness

| API | Usage |
|---|---|
| `s_AIRandomGenerator.RandInt(lo, hi)` | Integer in range **[confirmed]** |
| `s_AIRandomGenerator.RandFloat01()` | Float 0–1 **[confirmed]** |
| `s_AIRandomGenerator.RandFloatXY(min, max)` | Float in range **[confirmed]** |
| `array.GetRandomElement()` | Pick random element **[confirmed]** |

---

## 15. Replication

| API | Usage |
|---|---|
| `[RplProp(onRplName: "Callback")]` | Replicated property with client callback **[confirmed]** |
| `[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]` | Broadcast RPC **[confirmed]** |
| `Rpc(MethodName, args...)` | Invoke RPC **[confirmed]** |
| `Replication.BumpMe()` | Force replication flush **[confirmed]** |
| `SCR_ChatComponent.RadioProtocolMessage(text)` | Radio voice line for commander announcements **[confirmed]** |

---

## 16. Remaining Unknowns

| Item | Needed for | Action |
|---|---|---|
| Standard move / attack waypoint prefab paths | Waypoint chain spawning | Search project assets, register in Config singleton |
