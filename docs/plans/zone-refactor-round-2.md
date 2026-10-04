# Zone refactor, round 2

Three changes: a losing condition for a contested zone, AI tickets everywhere scaled to player count, and a clean-up of player structures when a zone ends.

## 1. Contested zones can now be lost

### Today

`m_bStopTimerOnRedforSuperiority` is the whole feature. When attacker AI inside the zone outnumber defenders inside it, `FreezeZone()` stores the remaining defence time and moves the state to `FROZEN`; `UnfreezeZone()` restores it when defenders retake the majority. The zone can never be lost this way — holding is only ever delayed, which is why one player in a tower can stall a stage indefinitely, and why Command_DDos asked for the zone to be losable.

`m_bStopSpawnersOnRedforSuperiority` additionally pauses infantry and mechanized spawners while frozen, so the pressure *drops* exactly when the attackers are winning.

### Proposed

Freezing stays. On top of it, a **failure timer** accumulates while the zone is contested:

| Property | Behaviour |
| --- | --- |
| Default | 300 s, configurable per zone and from the mission header |
| While contested | Counts down |
| When defenders retake the majority | **Freezes where it is — never resets** |
| Reaching zero | Zone goes to `FINISHED_FAILED` |
| On the HUD | Shown only while contested; hidden when frozen |

Because it never resets, being contested for five separate minutes costs as much as five minutes in a row. That makes repeated pushes cumulative and removes the current stalemate, without punishing a team that clears the zone quickly.

### Implementation

- `m_iFailureTimeSeconds` (default 300) and `m_iRemainingFailureSeconds` on `AFM_DiDZoneComponent`, initialised at `ActivateZone()`.
- `FreezeZone()` starts consuming it; `UnfreezeZone()` stops. Both already exist and are the only two places that need to change.
- `HandleActiveZoneLogic()` checks for expiry alongside its existing `defenderCount == 0` and `IsZoneTimeExpired()` checks, so `FinishZoneFailed()` is reached through a path that already works.
- Replicate the remaining seconds and a "contested" flag through the game mode, which already carries `m_bIsContested`. The HUD then shows the failure countdown in place of the defence countdown while contested — the status line already has a `CONTESTED` marker to hang it on.

### Two interactions to decide

**COWABUNGA fires at 180 s of freeze; failure would land at 300 s.** That ordering is deliberate and worth keeping: the dead return as attackers first, which pushes the zone further toward failing. But COWABUNGA currently measures its own `m_ContestedSince` independently. It should read the zone's failure timer instead, so the two clocks cannot disagree.

**Paused spawners stay paused.** `m_bStopSpawnersOnRedforSuperiority` stays on, and it works better with the failure timer than without it. The loop becomes: contested, so the timer drains and reinforcements stop → players clear the zone → timer freezes where it is and reinforcements resume → attackers build up and contest again, resuming from whatever time is left. The pause paces that cycle rather than blocking it, and because the timer never resets, every cycle is a permanent cost to the players.

Two reasons it is the right default:

- It only stops **new** spawns. AI already inside the zone keep fighting, so a contested zone is a contained engagement players can actually win rather than an endless stream.
- Clearing the zone buys a real lull, which is the breather between waves that the survey asked for.

The stalemate this used to cause came from the timer stopping, not from the spawners stopping. Once the timer is cumulative, the pause has no downside left.

## 2. AI tickets in every zone, scaled to player count

### Today

Tickets exist on `AFM_DiDSpawnerComponent` (`m_bUseTickets`, `m_iMaxTickets`, `ConsumeTickets`) but are off by default and only wave zones use them. `AFM_DiDWaveZoneComponent` has its own `GetRemainingTickets()` that drives wave progression, separate from the `GetRemainingSpawnSpawnTickets()` added for the HUD. Nothing scales with how many people are playing.

Two existing bugs to fix while here:

- `IsAICapReached()` compares the **zone-wide** AI count against `m_iMaxAICount`, which is a **per-spawner** attribute. A spawner set to 8 stops spawning once the zone holds 8 AI from any source.
- Ticket counts are authored per spawner, so a zone's total budget is whatever its spawners happen to add up to.

### Proposed

The zone owns one pool and distributes it; spawners spend from it.

```
tickets = clamp(round(perPlayer * players * headerMultiplier), min, max)
```

| Setting | Default | Where |
| --- | --- | --- |
| Tickets per player | 15 | Zone, per phase |
| Multiplier | 1.0 | **Mission header** |
| Minimum | 60 | Header, guardrail |
| Maximum | 240 | Header, guardrail |

At the default multiplier that is 60 tickets for 4 players, 150 for 10, 240 from 16 up.

**Count connected BLUFOR players, not living ones,** and **lock the number at zone activation.** `GetDefenderCount()` filters on a living character, which is right for the contest check and wrong here: a pool that shrank as players died would make a zone easier exactly when the team is losing, and the HUD number would move on its own. Size from `GetPlayersInFaction()` without the alive filter so spectators waiting to respawn still count.

**Concurrent pressure scales too.** Scaling only the pool makes a zone last longer rather than feel harder, so the zone AI cap scales with player count alongside it — and that means finally fixing `IsAICapReached()` to compare against the zone's own cap.

**Mortars, helicopters and COWABUNGA stay outside the pool.** They are not something players grind down, and counting them would make the displayed "enemies remaining" lie. They are already limited by time on station and activation counts.

### Guardrails

- Clamp after the multiplier, so a header cannot produce an unplayable zone in either direction.
- A multiplier of 0 or less means "use the defaults", matching `SCR_MissionHeaderCampaign`'s `-1` convention.
- Log the resolved pool per zone at activation. Without that, a bad header value is invisible.

### Wave zones

`AFM_DiDWaveZoneComponent` computes per-wave tickets from `m_iBaseTickets` and `m_iTicketsPerWave` and uses ticket depletion to end a wave. Simplest path: scale those two numbers by the same player-count factor and leave the wave logic alone. Folding wave tickets into the zone pool would change what ends a wave, which is out of scope here.

## 3. Remove player structures when a zone ends

### Why

Windows95 reported no vehicle reaching the fight at all in one round, because player-built compositions blocked the pathing — and he proposed exactly this fix. It is also now more likely than it was, since compositions can be placed touching and a solid wall across a road is easy to build.

### Finding them

There is no registry. `SCR_CampaignBuildingProviderComponent` tracks budgets, cooldowns and active users, but keeps no list of what was built, and the editable entity core exposes no enumeration either. Two options:

**A. Track at build time (recommended).** `modded class SCR_CampaignBuildingCompositionComponent` hooking `OnCompositionSpawned()` registers the entity with the zone system. Gives an exact list, no queries, and the registration point is where the cost is already applied. Costs one modded class.

**B. Query at zone end.** `QueryEntitiesBySphere` over the zone, keep entities whose root has `SCR_CampaignBuildingCompositionComponent`, and test against `IsPointInsideZone()`. No new coupling, but the radius has to cover the whole zone and it will also catch compositions authored into the world rather than built by players — which must not be deleted.

Option A avoids that last problem entirely, which is the deciding factor: B cannot tell a player's sandbag wall from the mission's own.

### When

`DeactivateZone()` already runs `Cleanup()` on every spawner when the zone system moves on, so it is the natural place. Worth a short delay or a hint first, so structures do not vanish while players are still looking at them.

### Decisions

- Delete only inside the zone that ended, or everything the team built? Per-zone is more predictable; everything is simpler.
- Wrecks and corpses are a separate problem, handled by the garbage system.
- If the supplies economy lands later, deleting a composition should not refund it.

## Order of work

1. **Failure timer.** Self-contained, touches two existing methods, and gives the clearest gameplay change.
2. **Structure clean-up.** Independent of the rest, fixes a reported bug.
3. **Ticket pool.** Largest, and the only one that needs the mission header, so it lands with the configuration track from the roadmap rather than before it.

## Open questions

- Should the failure timer be visible from the start as a threat, or only appear once the zone is first contested?
    Answer: When zone is contested
- Does the failure timer carry over between zones, or reset each stage? Reset is assumed above.
    Answer: No, failure timer is zone-specific, and it should be possible to disable it (eg in last zone)
- Should a zone lost to the failure timer end the match, or advance to the next zone as `FINISHED_FAILED` already does?
    No, only advance to next zone
