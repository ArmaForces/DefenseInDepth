# AFM_DiDMortarSpawnerComponent - AI Mortar Fire Support System

## Overview
Intelligent mortar fire support system that uses Monte Carlo sampling to dynamically select optimal fire positions based on defender concentrations within the zone.

## Concept

### The Problem
- Static waypoints don't adapt to player movement
- Random targeting is ineffective
- Need to concentrate fire where defenders are clustered

### The Solution: Monte Carlo Target Selection
1. Generate N random sample points within the zone
2. For each sample, count defenders within radius M
3. Select the sample point with the highest defender concentration
4. Create fire mission waypoint at that position
5. Update periodically as battle evolves

## Features

### ✅ Dynamic Targeting
- Adapts to defender movements
- Concentrates fire on largest groups
- Updates missions periodically

### ✅ Intelligent Sampling
- Respects min/max range constraints
- Only samples within zone boundaries  
- Validates distance from mortar position

### ✅ Configurable Parameters
- Sample count (accuracy vs performance)
- Sample radius (area of effect consideration)
- Update interval (responsiveness vs performance)
- Range constraints (realistic mortar capabilities)

### ✅ Debug Visualization
- Optional visual feedback of sample points
- Color-coded by target density
- Helps tune parameters

## Configuration

### Attributes

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `m_crewConfig` | AFM_CrewConfig | - | Crew configuration (gunner only typically) |
| `m_aMortarPrefabs` | ResourceName[] | - | Mortar vehicle prefabs to spawn |
| `m_iFireMissionUpdateInterval` | int | 30 | Seconds between target updates |
| `m_iMonteCarloSamples` | int | 10 | Number of sample points |
| `m_fSampleRadius` | float | 50 | Radius (m) to check around each sample |
| `m_fMinTargetDistance` | float | 100 | Minimum range from mortar |
| `m_fMaxTargetDistance` | float | 800 | Maximum range from mortar |
| `m_bDebugVisualization` | bool | true | Show debug visualization |

### Tuning Guide

#### Sample Count (`m_iMonteCarloSamples`)
- **Low (5-10)**: Fast, less accurate, good for large zones
- **Medium (10-20)**: Balanced, recommended
- **High (20-50)**: Slow, very accurate, overkill for most cases

#### Sample Radius (`m_fSampleRadius`)
- **Small (20-30m)**: Pinpoint targeting, requires many samples
- **Medium (50-70m)**: Balanced, catches small groups
- **Large (100m+)**: Area targeting, may miss optimal spots

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

### 2. Monte Carlo Target Selection
```
FindBestTargetPosition(mortarPos)
  └─> Get zone polyline boundary
  └─> Calculate zone bounding box
  └─> FOR each Monte Carlo sample:
       ├─> Generate random point in bounds
       ├─> Check if inside zone polygon
       ├─> Check if within range constraints
       ├─> Count defenders in sample radius
       └─> Track best sample (most targets)
  └─> Return position with most targets
```

### 3. Fire Mission Update
```
UpdateFireMission(fireMission)
  └─> Find best target via Monte Carlo
  └─> Create defend waypoint at target
  └─> Clear old waypoints from AI group
  └─> Assign new waypoint
  └─> Update tracking data
```

### 4. Lifecycle
```
Process() [called every frame]
  └─> IF zone is ACTIVE:
       └─> IF update interval elapsed:
            └─> UpdateAllFireMissions()
                 └─> FOR each spawned mortar:
                      └─> Run Monte Carlo sampling
                      └─> Update waypoint
```

## Algorithm Details

### Monte Carlo Sampling Pseudocode
```
function FindBestTarget(mortarPosition, zone):
    bestPosition = null
    maxTargets = 0
    
    for i = 1 to SAMPLE_COUNT:
        // Generate random point
        point = RandomPointInZone(zone)
        
        // Validate constraints
        if not IsInPolygon(point, zone):
            continue
        
        distance = Distance(mortarPosition, point)
        if distance < MIN_RANGE or distance > MAX_RANGE:
            continue
        
        // Count targets
        targetCount = CountDefendersInRadius(point, SAMPLE_RADIUS)
        
        // Update best
        if targetCount > maxTargets:
            maxTargets = targetCount
            bestPosition = point
    
    return bestPosition
```

### Target Counting
```
function CountDefendersInRadius(center, radius):
    count = 0
    
    for each defender in zone:
        if not defender.IsAlive():
            continue
        
        distance = Distance(center, defender.position)
        if distance <= radius:
            count++
    
    return count
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
// High-accuracy, slow updates (siege mortar)
m_iMonteCarloSamples = 20
m_fSampleRadius = 70
m_iFireMissionUpdateInterval = 60
m_fMaxTargetDistance = 1200

// Fast, responsive (light mortar)
m_iMonteCarloSamples = 10
m_fSampleRadius = 40
m_iFireMissionUpdateInterval = 20
m_fMaxTargetDistance = 600
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
                                ├─> Run Monte Carlo sampling
                                ├─> Find best target
                                └─> Update waypoint
```

## Performance Considerations

### CPU Impact
- Monte Carlo sampling is O(N × M) where:
  - N = number of samples
  - M = number of defenders
- Runs periodically (not every frame)
- Impact scales with:
  - Sample count
  - Defender count
  - Number of mortars
  - Update frequency

### Optimization Tips
1. **Reduce samples**: 10 samples usually sufficient
2. **Increase interval**: 30-60s is fine for most scenarios
3. **Limit mortars**: 1-2 per zone max
4. **Cache geometry**: Zone polyline doesn't change
5. **Early exits**: Skip if no defenders in zone

### Estimated Performance
- **10 samples, 20 defenders**: ~0.1ms per update
- **20 samples, 40 defenders**: ~0.4ms per update
- **50 samples, 100 defenders**: ~2-3ms per update

Update occurs every 30-60 seconds, so even heavy configs have minimal impact.

## Advanced Customization

### Custom Scoring Function
Override to add more sophisticated targeting:

```enscript
override protected vector FindBestTargetPosition(vector mortarPos)
{
    // Custom scoring that considers:
    // - Target density
    // - Distance from mortar (prefer closer)
    // - Terrain (prefer open areas)
    // - Previous fire missions (avoid same spot)
    
    float bestScore = 0;
    vector bestPos = vector.Zero;
    
    for (int i = 0; i < m_iMonteCarloSamples; i++)
    {
        vector samplePos = GenerateSample();
        
        int targets = CountDefendersInRadius(samplePos, m_fSampleRadius);
        float distance = vector.Distance(mortarPos, samplePos);
        float terrain = GetTerrainScore(samplePos);
        float history = GetHistoryPenalty(samplePos);
        
        float score = (targets * 10.0) - (distance * 0.1) + terrain - history;
        
        if (score > bestScore)
        {
            bestScore = score;
            bestPos = samplePos;
        }
    }
    
    return bestPos;
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
Avoid friendly fire:

```enscript
override protected int CountDefendersInRadius(vector centerPos, float radius)
{
    int defenders = super.CountDefendersInRadius(centerPos, radius);
    int friendlies = CountAttackersInRadius(centerPos, radius);
    
    // Heavy penalty for friendly fire risk
    if (friendlies > 0)
        return -100;
    
    return defenders;
}
```

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
- Monte Carlo sampling concept adapted from computational geometry
- Integrated with ArmaForces Defense in Depth spawner system
