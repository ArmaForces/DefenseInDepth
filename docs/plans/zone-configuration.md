# Zone configuration

Goal: fewer properties on the components, and the ones that remain settable from the scenario header instead of only in the World Editor — including per phase, so one world can ship several scenarios at different difficulties.

## Today

129 `[Attribute]` properties across the mod:

| Component | Properties |
| --- | --- |
| Helicopter spawner | 32 |
| Mechanized spawner | 17 |
| Extraction component | 14 |
| Mortar spawner | 13 |
| Infantry spawner | 11 |
| COWABUNGA | 8 |
| Spawner base | 7 |
| Zone base | 7 |
| Wave zone | 7 |
| Crew config | 5 |
| Extraction zone | 4 |
| Wave spawner | 2 |
| Game mode | 2 |

Three zones with four spawners each means the same knob is authored a dozen times, and every balance change is a trip through the World Editor. That is why the balance track is slow and why a second map is expensive.

## Four buckets

Sorting all 129 by who actually needs to change them:

### A. Scenario header — the difficulty dial

Things a mission maker or server admin wants to change per scenario entry, without opening the world.

| Setting | Today | Notes |
| --- | --- | --- |
| Prepare time | `m_iPrepareTimeSeconds`, per zone | 3 of 4 players say it is too long |
| Defence time | `m_iDefenseTimeSeconds`, per zone | |
| Zone AI cap | `m_iMaxAICount`, per zone | Currently 0, meaning only per-spawner caps apply |
| AI count multiplier | new | Scales `m_iSpawnCountPerWave` across every spawner |
| Spawn interval multiplier | new | Scales `m_iWaveIntervalSeconds` across every spawner |
| Helicopters on/off | implicit | Today the only way is deleting the spawner |
| Mechanized on/off | implicit | |
| Mortars on/off | implicit | |
| COWABUNGA on/off | implicit | |
| Infantry hunting on/off | `m_bHuntPlayers` | |
| Extraction call delay | `m_iSpawnDelaySeconds` | |
| Landing zones used | `m_iLandingZoneCount` | |
| Starting supplies, carry-over | not built | Reserve the fields now, fill when the supplies track lands |

### B. Per phase

The same block as A, applied to one zone index. What genuinely differs between phases: times, AI cap, the multipliers, and which spawner types are live. A short stage-3 prep with helicopters enabled and a long stage-1 prep without them is the sort of thing that should be one header edit.

### C. Demote to constants

Tuning numbers that were set once and have not moved since. These are the bulk of the count.

- **Helicopter flight** (12): spawn height, cruise speed and height, attack speed and height, attack radius, waypoint reached radius, attack waypoint count, attack arc, hold seconds, break-off health, break-off on crew loss.
- **Extraction flight** (5): the same shape again, duplicated.
- **Waypoint radii and timeouts**: they exist to work around the mod's turn radius, not to be designed with.
- **Waypoint and marker prefab references** (5): hunt, suppress, attack, overwatch, LZ marker. Asset references, not tuning.
- **Mortar internals**: dispersion step, same-target radius, Monte Carlo samples, debug visualisation.
- **Re-task internals** on infantry and mechanized: interval, target group radius, re-task move distance — three values duplicated across two components.
- **Hint and announcement strings** (5): these belong in localisation, not in properties.

A shared `AFM_DiDFlightConfig` would collapse the helicopter and extraction flight blocks into one object both point at, instead of 17 separate properties.

### D. Stays authored per instance

Genuinely about this place: zone name and index, prefab lists (AI groups, vehicles, helicopters, mortar, crew configs), and the spawn points, waypoints and polyline that are child entities rather than properties.

## The header

`SCR_MissionHeaderCampaign` already does exactly this in vanilla, with `-1` meaning "leave the default alone":

```c
[Attribute("-1", UIWidgets.EditBox, "How many control points are required to win (override, -1 for default)")]
int m_iControlPointsCap;
```

Follow that convention rather than inventing one.

```c
class AFM_DiDMissionHeader : SCR_MissionHeader
{
    [Attribute(desc: "Applied to every zone")]
    ref AFM_DiDPhaseSettings m_DefaultPhase;

    [Attribute(desc: "Per zone, by zone index. Anything left unset falls back to the default block")]
    ref array<ref AFM_DiDPhaseOverride> m_aPhaseOverrides;
}

class AFM_DiDPhaseSettings
{
    // Absolute overrides, -1 = leave the zone's own value
    [Attribute("-1")] int m_iPrepareTimeSeconds;
    [Attribute("-1")] int m_iDefenseTimeSeconds;
    [Attribute("-1")] int m_iMaxAICount;

    // Multipliers, always applied, 1.0 = no change
    [Attribute("1.0")] float m_fAICountMultiplier;
    [Attribute("1.0")] float m_fSpawnIntervalMultiplier;

    // Tri-state so "unset" is distinct from "off"
    [Attribute()] AFM_EToggle m_eHelicopters;
    [Attribute()] AFM_EToggle m_eMechanized;
    [Attribute()] AFM_EToggle m_eMortars;
    [Attribute()] AFM_EToggle m_eCowabunga;
    [Attribute()] AFM_EToggle m_eInfantryHunting;
}

class AFM_DiDPhaseOverride : AFM_DiDPhaseSettings
{
    [Attribute("1", desc: "Zone index this block applies to")]
    int m_iZoneIndex;
}

enum AFM_EToggle { DEFAULT, OFF, ON }
```

**Why two kinds of field.** Absolute values (times, caps) are clearer replaced than scaled — "prep is 300 s" beats "prep is 0.6 of whatever the world says". Difficulty is clearer scaled than replaced, because it has to reach every spawner without naming them. Mixing them deliberately, rather than picking one model for everything, is what makes both readable.

**Resolution order:** zone's own property → default block → matching phase override. Later wins, and `-1` or `DEFAULT` means "do not touch".

### Where it is applied

`AFM_GameModeDiD` reads the header with `SCR_MissionHeader.Cast(GetGame().GetMissionHeader())`. Zones register themselves with the zone system at `LateInit`, 5 s after start, so the push has to happen after registration — `StartZoneSystem()` is the natural point, walking the registered zones and handing each its resolved block.

Each zone then applies its own values and forwards the multipliers to its spawners, which need a small `ApplyScaling(float countMultiplier, float intervalMultiplier)` on the spawner base.

## Work

1. `AFM_DiDPhaseSettings`, `AFM_DiDPhaseOverride`, `AFM_EToggle` and `AFM_DiDMissionHeader`.
2. Resolution in the game mode, applied at `StartZoneSystem()`.
3. `ApplyScaling` on `AFM_DiDSpawnerComponent`, plus enable/disable by spawner type on the zone.
4. Demote bucket C: flight config object, constants, prefab references, strings to localisation.
5. Author one header per difficulty and check the same world plays differently without an editor trip.

Steps 1–3 are what unblock the balance track; step 4 is the cleanup that makes a second map cheap.

## Open questions

- Should the header be able to add or remove whole spawner types, or only enable ones the world already has? Enabling only is far simpler and probably enough.
- Do the British Forces layers share this header schema, or get their own subclass?
- Wave zones have their own ticket and multiplier settings. Fold them into the phase block, or leave them authored until the ticket pool from the extraction plan lands?
