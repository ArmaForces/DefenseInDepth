# Last stage: extraction ending

Goal: replace the final stage's countdown with an ending the players choose. The stage opens with extraction already available, the attackers are a finite pool, and the decision of when to leave belongs to the team.

## Why the current ending falls flat

The last stage is won when `m_iDefenseTimeSeconds` reaches zero. That is an event no player action produces, so it cannot feel like a climax, and it is the third static defence of the match in a row — the same verb three times.

The survey says the same thing from three directions:

- 5 of 7 want a final objective.
- Seweryn: holding for 5 minutes at the end makes the first two stages feel pointless, and earlier results should carry over.
- Command_DDos: the final zerg rush is bland and needs rethinking.

## The design in one paragraph

The last stage starts with the extraction already callable. The attackers are a **finite pool** of tickets, displayed on the HUD as enemies remaining. Call early and you cross open ground to a landing zone with the whole force still coming. Wait and you grind the pool down, but the attackers occupy your landing zones one by one, so patience costs you options. Those two curves cross at a different moment in every match, and finding that moment is the stage.

## There is no "you may now extract" gate

Any trigger between defending and calling — a timer, a wave count, a task that unlocks the radio — is the countdown this rework is meant to remove. The game would still be telling players when the ending is allowed to begin.

So there is no gate. Extraction is available from the first minute, and three things stop it from being a win button:

1. **The bird is slow.** Three minutes from call to touchdown. Calling is a commitment, not an escape; you still have to survive the inbound and reach the LZ.
2. **The enabling task is doable immediately.** It is the opening beat, not a lock. See below.
3. **Leaving at T+0 stays legal.** A bold team that runs for the LZ straight away with everyone alive should be allowed to try. Permitting the degenerate case is what makes the choice real instead of theatrical.

## The two pressure curves

| | What it costs | Mechanism |
| --- | --- | --- |
| **Leaving early** | The enemy is at full strength on the run to the LZ | Finite ticket pool, spent as the attack continues |
| **Waiting** | Your landing zones are taken one by one | LZ attrition, visible on the HUD |

Holding must not be strictly better, or everyone waits and the countdown is back in disguise. LZ attrition is what stops that, so it has to bite: losing a zone should visibly hurt, not be cosmetic.

## Two ways to win

- **Escape** — call, hold the LZ, get out while the fight is still on.
- **Break them** — grind the pool to zero. The attack is spent, and the helicopter comes in unopposed as the reward.

Losing is unchanged: `EAFMZoneState.FINISHED_FAILED` already covers all defenders eliminated, so that condition is free.

This is also the answer to Seweryn's complaint. What happened in stages 1 and 2 shows up here as how many people are still alive to hold the pool down, and whether the team can afford to wait at all.

## The enemy pool

`AFM_DiDSpawnerComponent` already has the ticket system: `m_bUseTickets`, `m_iMaxTickets`, `ConsumeTickets()` charging `GetPlannedGroupSize()` at spawn time, `GetRemainingTickets()` and `SetRemainingTickets()`. Spawning stops when the pool is empty (`Process()` and the wave check both test it).

What is missing is a zone-wide view:

- The escape zone sets and owns the pool, distributing it across its infantry and mechanized spawners rather than each spawner carrying its own budget.
- Override `GetEnemiesRemaining()` the way `AFM_DiDWaveZoneComponent` does — it returns `GetRedforScore() + GetActiveAICount()`, and the escape zone's version is remaining tickets plus live AI. The base zone returns -1 for unlimited.
- The HUD already replicates and displays this as `m_iEnemiesRemaining`, so the number the decision rests on is on screen today.

**Mortars and the helicopter stay outside the pool.** They are not something players can grind down, and counting them would make the displayed number lie. Limit them by time on station instead, as they are now.

## Scaling the pool to player count

### The pool is a clock, not only a difficulty dial

Pool size decides how long the "break them" win takes, so it cannot be tuned purely for difficulty. Time to deplete is roughly pool size divided by the team's kill rate, and kill rate scales with the number of players. Keeping that time constant therefore means **scaling the pool linearly with player count**. That is the default.

It matters because both win conditions have to stay reachable. A pool too large for the team never empties, and "break them" quietly disappears; a pool too small empties before anyone has considered extracting, and the stage resolves itself.

### Concurrent pressure has to scale too

Scaling only the pool changes how *long* the stage lasts without changing how *hard* it feels — a big team would face the same wave intensity for longer, which is a grind rather than a harder fight. So the concurrent AI cap scales alongside it: the zone's `m_iMaxAICount` (currently 0, meaning no zone-wide limit) and each spawner's `m_iSpawnCountPerWave`. Intensity per player stays roughly constant, and the pool then depletes at a rate proportional to team size, which is what keeps the clock honest.

This also keeps LZ attrition valid without scaling it. Because depletion time is constant across team sizes, the attrition intervals stay in the same relationship to the pool for a 6-player game and a 14-player one.

### What to count, and when

- **Count connected BLUFOR players, not living ones.** `GetDefenderCount()` filters on a living, undestroyed character, which is right for the contest check but wrong here — it drops as people die. Size the pool from `m_BluforFaction.GetPlayersInFaction()` without the alive filter, so spectators waiting to respawn still count.
- **Lock the number at zone activation** and never recompute it. A pool that shrank as players died would make the stage easier exactly when the team is losing, and the HUD number would move on its own, which breaks the read that the entire design rests on.
- **Late joiners and disconnects do not resize it.** Whoever is there when the last stage starts sets the size.
- **COWABUNGA attackers do not consume tickets.** They are players, they are a separate kind of pressure, and charging them to the pool would make the displayed count drop for reasons nobody can see.

### Proposed numbers

`tickets = clamp(15 × players, 60, 240)`

| Players | Tickets | Concurrent cap |
| --- | --- | --- |
| 4 | 60 (floor) | 12 |
| 6 | 90 | 18 |
| 8 | 120 | 24 |
| 10 | 150 | 30 |
| 12 | 180 | 36 |
| 16 | 240 (cap) | 40 (cap) |
| 20 | 240 (cap) | 40 (cap) |

Concurrent cap is `clamp(3 × players, 12, 40)`. The upper bound is a server performance ceiling as much as a design one.

### Why there are a floor and a cap

Linear scaling is right in the middle of the range and wrong at both ends, because team strength is not linear in a defence:

- **Small teams are disproportionately weak.** Below about six players you cannot cover every approach at all, so the position has holes no amount of individual skill closes. Strict linear scaling would still overwhelm them, hence the floor sits above `15 × players` for a very small group only in the sense that it stops the pool becoming meaninglessly thin — the real protection for small teams is the reduced concurrent cap, which keeps them from being flanked from three sides at once.
- **Large teams saturate the frontage.** Past a dozen or so players the marginal defender adds little, because the approaches are already covered. Without a cap the pool would grow past what the stage can deliver in a reasonable time, and the fight becomes long rather than hard.

Both bounds want checking against real sessions rather than reasoning. The group itself has no strong view — the survey split 3 "don't know", 2 yes, 1 no on whether difficulty should scale with player count — so this belongs behind a config switch with scaling on by default, not baked in.

## LZ attrition

Four candidate landing zones, placed 400–800 m from the defended position in different directions, each with its own approach and cover. All surviving zones are selectable at any time.

Every few minutes the attackers occupy one and it goes off the board — announced on the HUD, with the marker turning red. The floor is one, so the team is never locked out of extracting entirely, but the last remaining LZ can be the worst one on the map.

An earlier version of this plan had the game offer 2 of N at random. Attrition makes that unnecessary: it constrains the choice over time while leaving full agency in the moment, which is better on both counts.

## The enabling task

The stage opens with the long-range radio unpowered. The task is to fuel it, and it sits inside the defended position — with the LZ carrying the tension, this beat no longer needs to expose anybody.

| Option | How | Notes |
| --- | --- | --- |
| **Fuel** | `SCR_FuelManagerComponent.SetTotalFuelPercentage(0)` drains the generator at zone start; vanilla jerry cans, `SCR_FuelSupportStationComponent`, `SCR_RefuelAtSupportStationAction` and `SCR_CheckFuelAction` do the rest | Almost no new script, physical and readable, divisible into trips |
| Repair | Pre-damage hitzones, then a toolkit and `SCR_RepairSupportStationComponent` | Needs code to break specific hitzones at spawn; progress is a menu check, not a glance |
| Switch | One `SCR_ScriptedUserAction`, same shape as `AFM_VoteSkipWarmupAction` | Most controllable, least tactile |

Fuel is the pick. Tuning is the can count and the distance, and nothing about it needs new UI.

## The extraction sortie

### What the helicopter mod already provides

Read out of `REAPER_AiHelicopters` `data.pak` (`Scripts/Game/AI/REAPER_AiHelicopterWaypoints.c`). `REAPER_AiHelicopterLandWaypoint` is purpose-built for this:

| Property | What it does |
| --- | --- |
| `REAPER_SetWaitPlayersGetIn(true)` | The helicopter lands and waits for players to board |
| `REAPER_SetWaitPlayersMinimumCount(n)` | Will not complete until n players are aboard |
| `REAPER_SetWaitPlayersGetInPercent(f)` | Or until f of all compartment slots are filled |
| `REAPER_SetCheckForCloseUnts(bool)`, `REAPER_SetCloseUnitsMaxDistance(4-15)` | Will not complete while any player or AI agent is close but not inside |
| `REAPER_SetImportantEntityName(name)` | A specific named entity must be aboard — a VIP extraction for free |
| `REAPER_SnapToLandingSurface()` | The waypoint traces itself onto the ground on init, so LZ placement is forgiving |
| `REAPER_ActionOnLandingSucceeded()` | Fires when the helicopter is actually down |

Prefabs: `Prefabs/AI/Waypoints/REAPER_AiHelicopterLandWaypoint.et`, and `Prefabs/Groups/BLUFOR/REAPER_US_HelicopterCrew.et` for a friendly crew.

Two traps:

- `AFM_DiDHeliSpawnerComponent` calls `REAPER_SetLockVehicleForPlayers_S(true)`. The extraction helicopter must set it **false** or nobody can board.
- The setter really is spelled `REAPER_SetCheckForCloseUnts`. The typo is in the mod.

### The flight

Move → Land → Move-with-delete, which is the state machine `AFM_DiDHeliSpawnerComponent` already runs with a different middle leg. Spawn, crew teleport, waypoint assignment, self-completion and cleanup are all solved there.

- **Go-around.** If the helicopter takes damage on approach, or attackers are within 50 m of the touchdown point, it orbits and tries again a minute later. A second chance is better drama than an instant loss.
- **Set `CheckForCloseUnits` false** and run our own "every living player within radius is aboard" check. Otherwise an unconscious body six metres away blocks takeoff forever, and that gets found during a playtest instead of before one.

### Keep the win out of the AI's hands

The win must not depend on a third-party mod's pilot landing correctly; a slope or a tree would end the match in a bug rather than a defeat.

**Players win when enough living players hold the LZ for a short count.** The helicopter is the theatre, not the arbiter. If it fails to land within a timeout, the players win anyway.

## Fallback: drive out

If the helicopter is destroyed, a vehicle at the defended position becomes the way out, so losing the bird downgrades the ending instead of ending the match.

The route problem, for when that gets built: pre-place 20–30 dormant spawn points on every plausible road and activate those within ~400 m and ahead of the vehicle, plus two authored checkpoints as set-pieces, with the live sites randomised so it does not become a memory test.

**Do not route the escape back through the previous zones.** `AFM_DiDZoneSystem.ProgressToNextZone` calls `DeactivateZone`, which tears down that zone's AI. Re-arming zones 1 and 2 puts AI across the whole map at once, which is what the zone-wide `m_iMaxAICount` exists to prevent, and the old zones are full of the players' own fortifications now facing the wrong way.

## Why not the other two shapes

| | Fix a vehicle and drive out | Fuel a helicopter, a player flies it |
| --- | --- | --- |
| Blocking problem | The route is unpredictable, so enemy placement has no good answer without heavy authoring | Needs one competent pilot; a second lift means that pilot flying back into the fight |
| Replay value | Only if ambush sites randomise | The same two places in every match |

Both keep their value as variations once the LZ version exists; the ground escape is already the fallback branch above.

## Implementation sketch

- **`AFM_DiDEscapeZoneComponent`**, alongside `AFM_DiDWaveZoneComponent`: owns the ticket pool, LZ attrition, the call state and the win checks, in place of a countdown.
- **`AFM_DiDExtractionComponent`**: spawns the friendly helicopter and crew, builds the Move → Land → Move waypoints, watches the landing, handles the go-around and the timeout fallback. Modelled on `AFM_DiDHeliSpawnerComponent`.
- **LZ marker entity**: a child of the escape zone like `AFM_SpawnPointEntity`, one per candidate zone, carrying its name and state.
- **The call action**: `SCR_ScriptedUserAction` on the radio, available once fuelled.
- Attacker retargeting onto the LZ reuses `m_bHuntPlayers` from `AFM_DiDInfantrySpawnerComponent`; `AFM_DiDCowabungaComponent` handles the dead during the run.

## Work involved

1. `AFM_DiDEscapeZoneComponent` with the ticket pool distributed across its spawners, plus `GetEnemiesRemaining()`.
2. Pool and concurrent-cap scaling: count connected BLUFOR players at zone activation, apply the formulas, push the result into each spawner with `SetRemainingTickets()` and into `m_iMaxAICount`. Config switch to turn scaling off.
3. LZ marker entity, attrition timer, and the HUD line for which zones are live.
4. The enabling task: drain the generator at zone start, place cans, detect completion.
5. The call action and the inbound timer.
6. `AFM_DiDExtractionComponent`: spawn, waypoints, landing detection, go-around, timeout fallback.
7. Win check on players holding the LZ, independent of the helicopter; second win check on the pool reaching zero.
8. Ground-vehicle fallback when the helicopter is destroyed.
9. End-of-match summary: time held, enemies killed, who got out. 5 of 6 asked for a summary.

## Tuning, first guesses

For about 10 players. All of these are starting points, not measurements.

| Setting | Value | Reasoning |
| --- | --- | --- |
| Enemy pool | `clamp(15 × players, 60, 240)`, so 150 at 10 players | A busy stage currently kills 150–250 AI, so this is a shorter, finite version of one. See the scaling section |
| Concurrent AI cap | `clamp(3 × players, 12, 40)` | Keeps intensity per player flat so the pool depletes at a rate proportional to team size |
| Candidate LZs | 4 | Enough for attrition to matter without cluttering the map |
| LZ distance | 400–800 m | Far enough to be a move under fire, close enough to reach on foot |
| First LZ lost | 5 minutes in | Gives the team one clean decision window before pressure starts |
| Attrition interval | 4 minutes | Floor of 1 remaining |
| Inbound time | 180 s | Long enough that calling is a commitment |
| Hold to win | 30 s, majority of living players within 30 m | Keeps the win off the pilot AI |
| Go-around trigger | Damage on approach, or attackers within 50 m | Retry after 60 s |
| Landing timeout | 120 s after arrival | Players win on the hold check alone past this |

## Open questions

- Seats against team size, and whether a second lift is allowed or the stragglers lose.
- Is the result measured per player or for the team? What happens to someone left on the ground.
- Does supply carry-over buy anything here — a smoke screen on the LZ, a gunship escort, a second bird?
- Should casualties create their own pressure to leave? Vanilla has `SCR_LoadCasualtySupportStationComponent` if carrying the wounded should be mechanical rather than narrative.
- Do mortars and the helicopter keep running once the pool is empty, or does the stage go quiet as a signal that the attack is broken?

## Needs an editor test before building

None of this has been run in game; it is read from the mod's scripts and the vanilla sources.

1. Can players board and ride an AI-crewed helicopter cleanly, with `REAPER_SetLockVehicleForPlayers_S(false)`?
2. Does the pilot land where the Land waypoint is put, on the kind of ground the candidate LZs sit on?
3. Does `WaitPlayersGetIn` complete reliably, and what does `CheckForCloseUnits` do with an unconscious body nearby?
4. Does draining a generator's fuel and refuelling it from cans behave the way it does for vehicles?
