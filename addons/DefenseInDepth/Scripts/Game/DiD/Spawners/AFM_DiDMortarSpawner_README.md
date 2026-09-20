# AFM_DiDMortarSpawnerComponent - AI Mortar Fire Support System

## Overview
Mortar fire support that shells the largest group of players in the zone and walks its fire in over consecutive salvos.

## Concept

### The Problem
- Static waypoints don't adapt to player movement
- Random targeting is ineffective
- Need to concentrate fire where defenders are clustered

### The Solution: Target the Largest Group
1. Take the positions of all living players in the zone that are in range and clear of own troops
2. Find the player with the most teammates within `m_fTargetGroupRadius`
3. Aim at the centre of that group, which keeps the aim point steady between salvos so fire can walk in
4. Create single-shot waypoints scattered around it
5. Update periodically as the battle evolves; with no valid player, fire a harassing round at a random spot in the zone

## Features

### ✅ Dynamic Targeting
- Adapts to defender movements
- Concentrates fire on largest groups
- Updates missions periodically

### ✅ Valid targets only
- Inside the zone boundary
- Within the mortar's min/max range
- Never within `m_fFriendlyFireRadius` of attacker AI

### ✅ Configurable Parameters
- Group radius (how spread out a group may be)
- Scatter and bracketing (how fast fire walks in)
- Update interval (responsiveness vs performance)
- Range constraints (realistic mortar capabilities)

## Configuration

### Attributes

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `m_crewConfig` | AFM_CrewConfig | - | Crew configuration (gunner only typically) |
| `m_aMortarPrefabs` | ResourceName[] | - | Mortar vehicle prefabs to spawn |
| `m_iFireMissionUpdateInterval` | int | 30 | Seconds between target updates |
| `m_iMonteCarloSamples` | int | 10 | Attempts to find a random spot for harassing fire when no player can be targeted |
| `m_fTargetGroupRadius` | float | 40 | Players this close (m) to each other count as one group; fire aims at the centre of the largest |
| `m_fMinTargetDistance` | float | 100 | Minimum range from mortar |
| `m_fMaxTargetDistance` | float | 800 | Maximum range from mortar |
| `m_bDebugVisualization` | bool | true | Show debug visualization |
| `m_fInitialDispersion` | float | 60 | Scatter (m) of the first salvo on a new target area; rounds land 50–100% of it from the aim point |
| `m_fMinDispersion` | float | 12 | Scatter (m) once fire has walked in; rounds land anywhere within it |
| `m_fDispersionStep` | float | 0.5 | Scatter multiplier for each consecutive salvo on the same area |
| `m_fSameTargetRadius` | float | 75 | Aim points this close (m) to the previous one count as the same area |
| `m_fFriendlyFireRadius` | float | 30 | Rounds never aim or land this close (m) to attacker AI |

### Accuracy and bracketing
The AI crew fires an exact ballistic solution, so all spread comes from this component. Each salvo is split into single-shot waypoints, each scattered around the aim point:

1. A new target area gets `m_fInitialDispersion`: the first rounds are clear near misses, which warn the defenders.
2. Each following salvo on the same area multiplies the scatter by `m_fDispersionStep`, down to `m_fMinDispersion`.
3. If the defenders move more than `m_fSameTargetRadius`, bracketing starts again.

A new salvo is only planned once the previous one has been fired, or after two update intervals if the crew is stuck.

### Tuning Guide

#### Update Interval (`m_iFireMissionUpdateInterval`)
- **Fast (15-30s)**: Responsive, more CPU load
- **Medium (30-60s)**: Balanced
- **Slow (60-120s)**: Static, lower CPU load

## How It Works

### 1. Spawning Phase
```
SpawnSingleGroup()
  └─> Spawn mortar entity at spawn point
  └─> Get compartment manager
  └─> Crew mortar using AFM_CrewConfig (gunner only)
  └─> Create MortarFireMissionData tracker
  └─> Perform initial target selection
```

### 2. Target Selection
```
FindBestTargetPosition(mortarPos)
  └─> Collect living player positions of the defender faction
  └─> Keep those inside the zone, in range and clear of attacker AI
  └─> Find the densest group of them (m_fTargetGroupRadius) and take its centre
  └─> Centre blocked by own troops? Aim at one of the players instead
  └─> No valid player? Random spot in the zone for harassing fire
```

### 3. Fire Mission Update
```
UpdateFireMission(fireMission)
  └─> Find the aim point
  └─> Same area as last salvo? Halve the scatter, else reset it
  └─> Clear old waypoints from AI group
  └─> Add one single-shot waypoint per round, each scattered around the aim point
  └─> Update tracking data
```

### 4. Lifecycle
```
Process() [called every frame]
  └─> IF zone is ACTIVE:
       └─> IF update interval elapsed:
            └─> IF the previous salvo has been fired:
                 └─> UpdateAllFireMissions()
                      └─> FOR each spawned mortar:
                           └─> Pick aim point and plan the next salvo
```

## Usage Examples

### Basic Setup
```enscript
// In World Editor:
// 1. Add AFM_DiDMortarSpawnerComponent as child of zone
// 2. Configure crew config for gunner
// 3. Add spawn point for mortar
// 4. Set mortar prefab
```

### Configuration Example
```enscript
// Slow, heavy fire that walks in over a long fight
m_iFireMissionUpdateInterval = 60
m_fTargetGroupRadius = 40
m_fInitialDispersion = 80
m_fMinDispersion = 12

// Fast and aggressive
m_iFireMissionUpdateInterval = 20
m_fTargetGroupRadius = 30
m_fInitialDispersion = 40
m_fMinDispersion = 10
```

### Crew Config for Mortar
```enscript
AFM_CrewConfig mortarCrew = new AFM_CrewConfig();
mortarCrew.m_bSpawnDriver = false;  // No driver needed
mortarCrew.m_bSpawnGunner = true;   // Gunner only
mortarCrew.m_bNoTurretDismount = true;  // Keep gunner in position
```

## Integration with Zone System

### Hierarchy
```
ZoneEntity (AFM_DiDZoneComponent)
├─ InfantrySpawner (spawns ground troops)
├─ MortarSpawner (AFM_DiDMortarSpawnerComponent)
│  └─ MortarSpawnPoint (AFM_SpawnPointEntity)
└─ PolylineShapeEntity (zone boundary)
```

### Data Flow
```
Zone Process Loop
  └─> Zone.HandleActiveZoneLogic()
       └─> FOR each spawner (including mortar):
            └─> spawner.Process()
                 └─> MortarSpawner.Process()
                      ├─> Check if update interval elapsed
                      └─> UpdateAllFireMissions()
                           └─> FOR each mortar:
                                ├─> Find the largest player group
                                ├─> Pick the scatter for this salvo
                                └─> Add one waypoint per round
```

## Performance Considerations

Target selection costs one pass over the living players of the defender faction plus one pass over attacker AI positions, and runs once per update interval (not every frame). The random-spot fallback only runs when no player can be targeted.

## Advanced Customization

### Custom Scoring Function
Override to add more sophisticated targeting:

```enscript
override protected vector FindBestTargetPosition(vector mortarPos, notnull array<vector> attackerPositions)
{
    // Custom aim point that considers, for example:
    // - Distance from the mortar (prefer closer)
    // - Terrain (prefer open areas)
    // - Previous fire missions (avoid the same spot)

    return super.FindBestTargetPosition(mortarPos, attackerPositions);
}
```

### Predictive Targeting
Account for defender movement:

```enscript
// Track defender velocities
// Predict position N seconds in future
// Sample at predicted positions
```

### Danger Zone Avoidance
Own troops are already avoided: aim points and impact points are rejected within `m_fFriendlyFireRadius` of attacker AI. Override `IsValidTargetPosition` to add further rules.

## Debugging

### Enable Visualization
```enscript
m_bDebugVisualization = true
```

**Visual Indicators:**
- Yellow spheres: Sample points with no targets
- Orange spheres: Sample points with some targets
- Red sphere: Best target position (most targets)
- Sphere radius = sample radius

### Console Logging
```enscript
PrintFormat("AFM_DiDMortarSpawnerComponent: Updated fire mission to %1 (%2 targets)", 
    targetPos.ToString(), fireMission.m_LastTargetCount, LogLevel.DEBUG);
```

Watch console for:
- Mortar spawning confirmations
- Target updates with position and count
- Errors (no valid targets, range issues, etc.)

## Troubleshooting

### Mortar Not Firing
1. Check crew spawned correctly
2. Verify zone has defenders
3. Ensure defenders within range constraints
4. Check update interval hasn't elapsed yet

### Poor Targeting
1. Increase sample count
2. Adjust sample radius
3. Verify range constraints realistic
4. Check zone polyline is correct

### Performance Issues
1. Reduce sample count
2. Increase update interval
3. Limit number of mortars
4. Disable debug visualization

## Future Enhancements

### Potential Improvements
- **Barrage patterns**: Multiple coordinated fire missions
- **Creeping barrage**: Progressive fire line
- **Counter-battery**: Target enemy mortars
- **Suppression zones**: Area denial rather than casualties
- **Ammo limits**: Force tactical decisions
- **Fire mission queuing**: Multiple targets in sequence
- **Observer integration**: Players can call fire missions

### Advanced Targeting
- **Heat maps**: Build density maps over time
- **Clustering**: Identify multiple target clusters
- **Path prediction**: Anticipate defender movement
- **Terrain analysis**: Prefer defilade positions
- **Time-on-target**: Coordinate multiple tubes

## Credits
- Integrated with ArmaForces Defense in Depth spawner system
