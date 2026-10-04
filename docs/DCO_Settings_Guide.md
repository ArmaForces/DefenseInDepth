# DCO global AI settings guide

English translation of the property descriptions in `DCO_GlobalAi.c`, in the file's own category split,
followed by a proposed preset for aggressive, pushing bots.

The original descriptions are mostly Indonesian; the first few are English. Source copy: `docs/DCO_Settings.txt`.
Defaults are the `defvalue` in the attribute. The "Behavior" flags are a bitmask and are left as is.

Contents: [Uncategorised](#uncategorised) · [Weapon Usage](#weapon-usage) · [Performance](#performance) ·
[Behavior](#behavior) · [Idle](#idle) · [Straggler](#straggler) · [Combat](#combat) · [CQB](#cqb) ·
[Night](#night) · [Contact Report](#contact-report) · [Commander Spawner](#commander-spawner) ·
[Contact Sharing](#contact-sharing) · [Cross-Group Medic](#cross-group-medic) ·
[Player Awareness](#player-awareness) · [Investigate](#investigate) · [Server Config](#server-config) ·
[Proposed preset](#proposed-preset-aggressive-pushing-bots)

---

## Uncategorised

| Field | Default | Range | Description |
|---|---|---|---|
| `unitAimSkillAccuracy` | 1.8 | 0.1 - 10 | Global AI unit skill level |
| `m_fTimeToMaxAccuracy` | 15 | 1 - 60 | Unit skill |
| `m_fAiPerception` | 1.5 | 0.5 - 5 | Unit perception |
| `m_bIsMagicallyResupplied` | off | | Magical ammo |
| `m_fVehicleDismountDanger` | 700 | 0 - 2000 | Described as "Unit Perception" (copy-pasted description). The name suggests a danger distance for dismounting from vehicles; nothing in the file confirms it. |
| `m_eAISkillDefault` | 2 | enum `DCO_AISKILL` | AI custom skill in combat |
| `m_fSuppressionEffect` | 1 | 0 - 2 | How much suppression affects this AI |
| `m_fTakeCoverChance` | 0.9 | 0 - 1 | Base chance that the AI actively looks for cover when detected in the open. Scaled further by personality: RECKLESS drops the most (careless), AGGRESSIVE drops moderately (combat-oriented but disciplined), CAUTIOUS goes up. |
| `m_fPersonalityWeightStandard` | 80 | 0 - 100 | Weight of the MEASURED personality (relative to the other three, they don't have to total 100) |
| `m_fPersonalityWeightCautious` | 7 | 0 - 100 | Weight of the CAUTIOUS personality |
| `m_fPersonalityWeightAggressive` | 12 | 0 - 100 | Weight of the AGGRESSIVE personality |
| `m_fPersonalityWeightReckless` | 3 | 0 - 100 | Weight of the RECKLESS personality |
| `m_fDodgeChance` | 0.6 | 0 - 1 | Chance the AI dodges (runs for cover) each time it hears gunfire. Scaled by personality if `m_bDodgeScaleByPersonality` is on. |
| `m_fDodgeCooldown` | 8.0 | 0 - 120 s | Cooldown before the same AI may dodge again. This is the main brake: without a cooldown, full-auto fire makes the dodge chance meaningless. |
| `m_fDodgeMaxDist` | 250 | 0 - 1000 m | Maximum distance at which gunfire can still trigger a dodge |
| `m_fDodgeSearchDist` | 30 | 5 - 100 m | Search distance for buildings / cover when dodging |
| `m_bDodgeScaleByPersonality` | on | | Scale dodge chance by AI personality (CAUTIOUS up, RECKLESS down) |
| `m_iDodgeShotThreshold` | 1 | 1 - 20 | Number of enemy shots (sufficiently threatening ones) before the AI will dodge. 1 = immediately on the first shot (old behaviour). |
| `m_fDodgeShotWindow` | 5.0 | 1 - 30 s | Time window for counting shots. If no new shots arrive in this time, the count resets to 0. |

## Weapon Usage

| Field | Default | Range | Description |
|---|---|---|---|
| `m_fGrenadeUsage` | 0.5 | 0 - 1 | How often the AI throws frag grenades. 0 = never, 0.5 = default, 1 = very often (chance multiplied by 2). |
| `m_fGLUsage` | 0.7 | 0 - 1 | How often the AI uses grenade launchers (UGL). 0 = never, 0.5 = default, 1 = very often (chance multiplied by 2). |
| `m_fGLAccuracy` | 1 | 0.1 - 5 | Grenade launcher (UGL) accuracy. Higher = tighter explosion spread. 1 = default. |
| `m_fSmokeUsage` | 0.5 | 0 - 1 | How often the AI throws smoke to cover movement. 0 = never, 0.5 = default, 1 = very often (chance multiplied by 2). |

## Performance

| Field | Default | Range | Description |
|---|---|---|---|
| `m_eLODMode` | 0 | enum `DCO_EAILODMode` | AI simulation LOD for groups owned by the AI Commander. Groups without a commander are always vanilla (unless overridden per group). VANILLA = the engine decides. PREVENT_MAX_LOD = commander groups keep running far from players. FULL_DETAIL = commander groups at full detail (heaviest). |
| `m_bPerfProfiling` | off | | Performance profiling: logs time per DCO section, queries / raycasts per second, FPS and frame spikes every 30 seconds to the benchmark log and `$profile:DCO_Bench/perf_*.log`. OFF = near-zero cost. |

## Behavior

| Field | Default | Range | Description |
|---|---|---|---|
| `m_iBehaviorFlags` | 8388607 | flags `DCO_EBehavior` | Which individual / squad DCO behaviours are active. 8388607 is 23 bits set, i.e. everything on. |
| `m_fTacticStallPct` | 15 | 5 - 50 | An attack counts as stalled if the lead group has advanced less than this percent since the check 2 minutes ago |

## Idle

| Field | Default | Range | Description |
|---|---|---|---|
| `m_fIdleEnterTime` | 30 | 5 - 300 s | Seconds a group has no waypoint before it goes IDLE (members find cover, then stay still) |
| `m_fIdleCoverSearchDist` | 100 | 10 - 200 m | Distance from the leader's position to search for cover / a position inside a building while IDLE |
| `m_fIdleOverrunDist` | 15 | 0 - 100 m | IDLE: an enemy this close = overrun, the AI may leave cover |
| `m_bIdleLeaveToAssist` | on | | IDLE: may leave cover to help friends (attack enemies that aren't currently shooting at this group) |
| `m_bIdleLeaveToInvestigate` | off | | IDLE: may leave cover to investigate a threat |
| `m_bIdleFollowCommander` | on | | IDLE: the group still accepts commander orders. Off = the commander won't take groups that are IDLE. |

## Straggler

| Field | Default | Range | Description |
|---|---|---|---|
| `m_bStragglerEnabled` | on | | Pull back members who have fallen far behind their leader |
| `m_fStragglerTime` | 15 | 5 - 120 s | Seconds a member stays outside cohesion distance before being pulled back |
| `m_fStragglerStuckTime` | 30 | 10 - 300 s | Seconds the distance to the leader doesn't shrink = stuck. Another point is tried once; after that the member is marked stuck (not counted in group spread). |
| `m_bStragglerLog` | off | | Log every straggler: distance, active behaviour, combat mode, cover, leash. For diagnostics. |

## Combat

| Field | Default | Range | Description |
|---|---|---|---|
| `m_fSuperiorRatio` | 2 | 1 - 5 | The squad advances (attack posture + bounding) when living members >= this ratio x known enemies and it isn't heavily suppressed. It may also leave an IDLE hold / Defend leash. |
| `m_bLongRangeHold` | on | | Target beyond the weapon's effective range and not an immediate threat: stay put (prone if line of sight persists, otherwise crouch), slow single shots. |
| `m_fRifleEffectiveRange` | 350 | 100 - 800 m | Effective rifle range. Beyond this the fire rate is limited (slow single shots) and threat may not increase. |
| `m_fMGEffectiveRange` | 700 | 200 - 1500 m | Effective MG range. MGs may still fire bursts (suppression) at long range up to this distance. |
| `m_bOvermatchAssault` | on | | Overmatch assault: a clearly superior squad (weighted strength >= superior ratio x personality) with a confirmed contact will advance by bounding even while the enemy is still visible. |
| `m_fOvermatchMaxDist` | 250 | 100 - 500 m | Maximum enemy distance at which an overmatch assault may start |
| `m_bShareBuildings` | off | | Allow several groups to share one building (indoor defend / idle / garrison positions). OFF = one building, one group. |
| `m_iCoverPreference` | 0 | 0 / 1 / 2 | Defend / idle position preference: 0 Balanced (indoor and outdoor scored together, fortress bonus), 1 Indoor, 2 Outdoor. |

The last two sit near the end of the file, after Commander Spawner, but belong to this category.

## CQB

| Field | Default | Range | Description |
|---|---|---|---|
| `m_bCQBEnabled` | on | | Clear building: a squad that is ATTACKING / FLANKING / in overmatch and sees an enemy inside a building less than 100 m away clears that building (isolate, stack at the door, breach, clear room by room). Also sweeps the building after an objective is captured. |
| `m_iCQBIsolateMin` | 6 | 3 - 12 | Minimum living members needed to split off a support team (isolate, MG preferred). Below this, everyone goes in. |
| `m_fCQBStackWait` | 10 | 0 - 30 s | Maximum time to wait for the team to gather at the door before entering (a RECKLESS leader doesn't wait) |
| `m_iCQBAbortLosses` | 2 | 1 - 6 | If the entry team loses this many (dead / unconscious), it aborts, falls back to the support position and tries again after 2 minutes |
| `m_bCQBCaptureWaits` | on | | Objective capture waits until no building with CONTACT status remains within the objective radius |
| `m_fCQBContactTimeout` | 180 | 30 - 600 s | Time without new contact before a CONTACT building is automatically considered clear (so capture doesn't stall) |

## Night

| Field | Default | Range | Description |
|---|---|---|---|
| `m_bNightEnabled` | on | | Night behaviour: squad flares, light discipline, defensive ROE with long engagement ranges shortened, illumination before an assault |
| `m_fNightFlareCooldown` | 75 | 30 - 300 s | Flare cooldown per squad |
| `m_fNightEngageMul` | 0.6 | 0.2 - 1 | Multiplier on the engagement range of defending groups (Defend / Garrison) and on the long-range effective distance at night |
| `m_bNightLightDiscipline` | on | | Light discipline: AI flashlights are off outdoors at night, on only for CQB entry teams inside buildings. Vehicle headlights are turned off if an enemy is less than 800 m away. |

## Contact Report

| Field | Default | Range | Description |
|---|---|---|---|
| `m_iContactReportMaxPerScan` | 0 | 0 - 10 | Maximum contact reports per scan per group (global override). 0 = use the group prefab's value (default 3). |
| `m_fContactReportScanInterval` | 0 | 0 - 60 s | Contact report scan interval (global override). 0 = use the group prefab's value (default 15). |

## Commander Spawner

| Field | Default | Range | Description |
|---|---|---|---|
| `m_iCommanderStockDefault` | -1 | | Initial reinforcement stock (soldiers) for all commanders. -1 = use each commander's own value. |
| `m_iSpawnerFactionAICap` | 0 | | Total AI limit per faction for the Commander Spawner. When reached, spawning is delayed. 0 = no limit. |

`m_iMaxAttackGroupsDefault` (default -1) sits between these two in the file but has no attribute, so there is
no description.

## Contact Sharing

| Field | Default | Range | Description |
|---|---|---|---|
| `m_bShareEnabled` | on | | AI squads share enemy contacts with each other (voice / short-range radio) without going through the commander |
| `m_fShareVoiceRange` | 100 | 20 - 300 m | Voice range: shouting to friendly squads, no radio needed |
| `m_fShareRadioRange` | 400 | 100 - 1500 m | Short-range radio range |
| `m_fShareNoisePer100m` | 8 | 0 - 50 m | Base position noise per 100 m of distance from the observer to the contact |
| `m_fShareVoiceNoise` | 1.5 | 0.5 - 5 | Noise multiplier for the voice path (cruder) |
| `m_fShareRadioNoise` | 1 | 0.5 - 5 | Noise multiplier for the short-range radio path |
| `m_bShareRequireRadio` | off | | Radio required: the short-range radio path only works if both sender and receiver carry a radio |
| `m_iShareHops` | 2 | 1 - 2 | Number of hops (1 = direct only, 2 = the receiver may forward once) |
| `m_bShareMapMarkers` | off | | Passed-on contacts are marked on friendly players' maps (vanilla AI enemy markers) |
| `m_bShareToPlayers` | on | | Friendly players in range get a small notification when an AI squad shares a contact |
| `m_bShareDebug` | off | | Log every contact transmission (sender -> receiver, hop, path, noise) |

## Cross-Group Medic

| Field | Default | Range | Description |
|---|---|---|---|
| `m_bMedicCrossGroup` | on | | AI medics may treat friendly casualties outside their own group if the casualty's group has no medic who can come |
| `m_fMedicRadiusBleeding` | 150 | 25 - 500 m | Radius to search for another group's medic for a bleeding casualty |
| `m_fMedicRadiusUnconscious` | 300 | 25 - 800 m | Radius to search for another group's medic for an unconscious casualty |
| `m_bMedicIncludePlayers` | on | | Friendly players are also treated by AI medics |
| `m_bMedicUnderFire` | on | | UNCONSCIOUS casualties may be recovered under fire (the medic isn't being shot at directly, uses smoke). Bleeding casualties always wait until it's safe. |
| `m_iMedicMaxLentPerGroup` | 1 | 1 - 4 | Maximum medics lent out by one group at a time |
| `m_fMedicTimeout` | 90 | 20 - 300 s | Time before a medic loan is considered failed and another medic is tried |

## Player Awareness

| Field | Default | Range | Description |
|---|---|---|---|
| `m_iOverlayMode` | 1 | 0 - 2 | Map overlay of friendly AI groups for the player. 0 = off, 1 = only squads that are support-bound to the player's group or share the same objective, 2 = all. |
| `m_fOverlayRadius` | 1500 | 200 - 5000 m | Base objective radius in the relevant overlay mode (objectives within 2x this radius are shown) |
| `m_fRadioRadius` | 800 | 100 - 3000 m | Radius of operation radio broadcasts to players around the objective |
| `m_fArtyWarnRadius` | 400 | 100 - 1500 m | Radius of artillery warnings to friendly players around the firing point |

## Investigate

| Field | Default | Range | Description |
|---|---|---|---|
| `m_fInvestigateMaxDist` | 500 | 0 - 1500 m | Maximum distance of an enemy that a group will still investigate |
| `m_fInvestigateChance` | 1 | 0 - 1 | Chance that a group investigates an unidentified contact. Rolled once per cluster. |

## Server Config

| Field | Default | Description |
|---|---|---|
| `m_bUseServerConfigFile` | on | Read config overrides from a JSON file in the server profile folder. If OFF, the values above are used as-is. |
| `m_sServerConfigPath` | `$profile:DCO/DCO_GlobalConfig.json` | Path of the JSON config file. The `$profile:` prefix points to the server's `-profile` folder. |
| `m_bAutoGenerateConfigFile` | on | If the JSON file doesn't exist, automatically create one containing the current Workbench values. If it exists but is missing some keys, the old file is copied to `.bak` and the missing keys are added (admin values are kept). |

The JSON file overrides the Workbench values, so on a server a value changed in Workbench can be silently
ignored. Edit the generated `DCO_GlobalConfig.json` (or delete it to regenerate it) when testing.

---

## Proposed preset: aggressive, pushing bots

**Basis.** This is built from the descriptions above and from the 02.10 survey. I have not read the DCO code
behind these settings and nothing here has been played, so treat it as a first guess to tune, not a result.

**What the survey asked for.**
- Lamentin bots were "za mało agresywne" (not aggressive enough).
- Some bots "po prostu stały i czekały na rozstrzelanie" (just stood and waited to be shot instead of running
  to the objective).
- The opposite complaint came from Powerplant: bots were too accurate for a zerg rush. So this preset changes
  how bots move and commit, and deliberately leaves aim skill alone.

**What the descriptions point at.** Three behaviours read like "standing and waiting": the long-range hold
(`m_bLongRangeHold`), going IDLE when a group has no waypoint, and running for cover on every gunshot (dodge).
Those come first below.

### Highest expected impact

| Field | Default | Proposed | Why |
|---|---|---|---|
| `m_bLongRangeHold` | on | **off** | An out-of-range target makes a squad stay put, prone or crouched, firing slow single shots. That is exactly "stood and waited". |
| `m_fSuperiorRatio` | 2 | **1.2** | A squad advances once it has this many living members per known enemy. 2 means two-to-one before it pushes; against players behind fortifications the bots rarely feel superior. |
| `m_fTacticStallPct` | 15 | **25** | A higher value counts an attack as stalled sooner, so the commander changes tactic instead of letting it grind. |
| `m_fDodgeChance` | 0.6 | **0.3** | Fewer runs for cover on every shot heard. |
| `m_iDodgeShotThreshold` | 1 | **3** | The AI ignores the first couple of shots instead of dodging on the first one. |
| `m_fDodgeCooldown` | 8.0 | **15.0** | A dodging AI loses ground; a longer gap lets it keep moving. |
| `m_fTakeCoverChance` | 0.9 | **0.5** | Less cover-seeking when detected in the open. |

### Personality mix

| Field | Default | Proposed | Why |
|---|---|---|---|
| `m_fPersonalityWeightStandard` | 80 | **50** | The weights are relative, so shifting weight to the aggressive types is what changes the mix. |
| `m_fPersonalityWeightCautious` | 7 | **2** | |
| `m_fPersonalityWeightAggressive` | 12 | **35** | AGGRESSIVE is combat-oriented but disciplined, so it is the safer one to grow. |
| `m_fPersonalityWeightReckless` | 3 | **13** | Reckless leaders also skip the CQB stack wait and take less cover. Keep this one small: it makes bots die faster, which also reads as "easy". |

Resulting shares: 50 / 2 / 35 / 13 out of 100, against the current 80 / 7 / 12 / 3 out of 102.

### Closing with the enemy

| Field | Default | Proposed | Why |
|---|---|---|---|
| `m_fSuppressionEffect` | 1 | **0.6** | Less pinned down by incoming fire. |
| `m_fOvermatchMaxDist` | 250 | **400** | An overmatch assault may start from further out. |
| `m_fIdleEnterTime` | 30 | **120** | A group with no waypoint holds and hides after 30 s. Longer keeps it moving. |
| `m_bIdleLeaveToInvestigate` | off | **on** | An idle group may break cover to check a threat instead of waiting. |
| `m_fIdleOverrunDist` | 15 | **40** | Leaves cover for an enemy at 40 m instead of 15 m. |
| `m_fInvestigateMaxDist` | 500 | **800** | Groups pursue contacts from further away. |
| `m_fStragglerTime` | 15 | **10** | Stuck or lagging members are pulled back to the leader sooner. |
| `m_fStragglerStuckTime` | 30 | **20** | A member that isn't closing the distance is re-pathed sooner. |

### Grenades and smoke

| Field | Default | Proposed | Why |
|---|---|---|---|
| `m_fGrenadeUsage` | 0.5 | **0.7** | More frags thrown at fortified positions. |
| `m_fSmokeUsage` | 0.5 | **0.7** | More cover for pushing across open ground. |

Smoke runs against the survey's complaint about friendly bots throwing smoke on their own position. If that
mattered more, keep smoke at 0.5. The setting is global and I don't know whether it can be split by side.

### CQB

| Field | Default | Proposed | Why |
|---|---|---|---|
| `m_fCQBStackWait` | 10 | **5** | Enters sooner. |
| `m_iCQBAbortLosses` | 2 | **3** | The entry team aborts later. |
| `m_fCQBContactTimeout` | 180 | **90** | A CONTACT building is considered clear sooner, so objective capture stops waiting. |

### One performance setting, tested on its own

| Field | Default | Proposed | Why |
|---|---|---|---|
| `m_eLODMode` | 0 | **1** (PREVENT_MAX_LOD) | If commander groups far from any player are being slowed by the engine, that would look like bots standing still. This keeps them running at distance. It costs performance, so test it by itself rather than in the same run as everything above. The enum order is assumed to be VANILLA, PREVENT_MAX_LOD, FULL_DETAIL; check it in Workbench. |

### Leave alone

- `unitAimSkillAccuracy`, `m_fTimeToMaxAccuracy`, `m_fAiPerception`, `m_eAISkillDefault`: the Powerplant
  feedback says accuracy is already too high for a rush. Pushing harder with the same aim means more damage per
  minute. If it feels too lethal, lower `unitAimSkillAccuracy` slightly instead.
- `m_fVehicleDismountDanger`: the survey mentions bots leaving a BRDM before the zone, and this is the likely
  candidate, but the description doesn't say which direction moves the dismount point. Change it in a separate
  test, one value at a time.
- Contact Sharing, Cross-Group Medic and Night: they already help bots keep moving.
- `m_iBehaviorFlags`: already everything on.

### How to test

1. Back up the current `DCO_GlobalConfig.json`, since it overrides Workbench values on a server.
2. Apply the first two tables (highest impact and personality) and play one stage. Those should change the feel
   the most.
3. Add the rest in groups, and `m_eLODMode` on its own.
4. If bots get too hard to hold, the quickest levers back are `m_fSuperiorRatio` (up), the Reckless weight
   (down) and `m_fDodgeChance` (up).
