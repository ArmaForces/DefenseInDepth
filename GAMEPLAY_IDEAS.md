# DefenseInDepth - Gameplay Improvement Ideas

## Zone Flow & Tension

### 1. Player ticket pool per zone
Defenders get a limited respawn budget per zone (e.g. 30 tickets for 10 players). Depletion causes zone failure, not just "all alive players dead." This makes each death consequential and prevents the current situation where a single surviving player can hold a zone indefinitely while teammates cycle through respawns.

### 2. "Last stand" critical state
When defenders drop below e.g. 30% of starting count AND the timer is under 25% remaining, enter a `CRITICAL` sub-state: faster AI spawns, red HUD, audio cue. Creates a dramatic finish instead of a silent slow bleed.

### 3. Retreat window before zone failure
When `FINISHED_FAILED` is about to trigger, add a 30-second "extract!" warning phase before actually failing. Players who make it out alive carry over to the next zone's starting count. Those who don't are dead weight. Rewards tactical withdrawal.

---

## Prepare Phase (currently just a timer)

### 4. Meaningful setup phase
Right now `PREPARE` is a dead countdown. It could reward players who actively prepare:
- Destroying enemy scout vehicles reduces first-wave ticket count
- Building fortifications (the mod already has `SCR_CampaignBuildingGadgetToolComponent` modded) could increase defender count threshold
- Finding and destroying an enemy ammo cache reduces `m_iTicketsPerWave` for wave zones

---

## Dynamic Difficulty

### 5. Player-count scaling
`GetSpawnCountForWave()` uses fixed values. It should factor in `GetDefenderCount()` — e.g. `spawnCount = baseCount * (defenderCount / 5.0)`. Otherwise 3 players fighting 20-player-designed waves is unplayable.

### 6. Supply rewards actually matter
Wave zones already award supplies via `m_iSupplyReward` but there's nothing to spend them on. Tying them to a "call support" action (mortar strike on demand, resupply drop, extra respawns) would make wave completion feel rewarding beyond just surviving.

---

## Zone Variety

### 7. Zone type modifiers
The base zone is always "hold a polygon for X seconds." Cheap to add variety with a type enum:
- `TIMER` — current behavior
- `ELIMINATION` — no timer, zone ends when all AI tickets depleted (a wave zone without the wave structure)
- `ESCORT` — defenders must keep a vehicle/VIP alive within the zone

### 8. Inter-zone travel phase
Currently zones activate instantly after failure. A short `TRANSIT` state (60–90s) where no AI spawns but players must physically move to the new zone would add immersion and reward organized teams that fall back together vs stragglers.

---

## Mortar / Fire Support

### 9. Counter-battery gameplay
The mortar spawner is sophisticated (Monte Carlo targeting) but one-sided. Giving defenders a way to locate and neutralize the mortar (e.g. a map marker revealing mortar position after it fires N times) creates an active side-objective with clear payoff.

---

## Quick Wins (smallest effort, good return)

- **Win condition hint**: `FINISHED_HELD` currently ends the game silently. Broadcast `"Zone X held! Defenders win!"` the same way zone failure is now broadcast.
- **Spectator info**: The HUD shows live/ticket counts but spectating dead players can't see which zone is active on the map. `AFM_ScoreMapInfoDisplay` could render the active zone polygon.
- **Wave zone HUD label**: `GetZoneDisplayNumber()` returns the wave number, so the "Zone" counter in the HUD shows wave number instead of zone index. The HUD label should change to "Wave" when the active zone is an `AFM_DiDWaveZoneComponent`.

---

## Priority

1. **Player ticket pool** — fundamentally changes the risk/reward of dying, makes defense feel weighty
2. **Prepare phase activities** — eliminates dead time at zone start, gives teams something to coordinate around
