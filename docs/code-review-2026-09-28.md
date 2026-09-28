# DiD code review - 2026-09-28

Full read of `addons/DefenseInDepth/Scripts/Game/DiD/**` (~5.7k lines) and
`addons/DefenseInDepth/Scripts/Game/Modded/**`, at commit `1245a6e`.
`Scripts/WorkbenchGame/EnfusionMCP/**` excluded - tooling, not gamemode.

Nothing below has been playtested against; findings come from reading the code, the world layers
and `Missions/DiD_Lamentin_Tuned.conf`.

Each finding has a **Verdict** line to fill in (agree / wontfix / already handled / needs thought)
and a **Notes** line for the reasoning, so the fixes can be worked through from this file.

---

## Player-visible bugs

### 1. The score HUD is invisible to every client on a dedicated server

`Scripts/Game/DiD/AFM_GameModeDiD.c:37`

`m_bShowUI` is the only piece of HUD state without `[RplProp]` - the other 17 fields have one. It is
set to `true` only on the authority (`AFM_GameModeDiD.c:193`), and `AFM_ScoreInfoDisplay.c:226`
hides the display whenever `ShowUI()` is false, so on a proxy it stays hidden for the whole match.

Masked by `AFM_ScoreMapInfoDisplay.UpdateHUD()` overriding that check, so clients still see the panel
*with the map open* - likely why it went unnoticed.

Fix: add `[RplProp(onRplName: "OnMatchSituationChanged")]`, or drop `m_bShowUI` and gate on the
already-replicated `m_bIsGameRunning`.

- **Verdict:** Disregard: UI is shown to all players, with or without map open
- **Notes:**

### 2. Zone 3 can be won without extracting

`Scripts/Game/DiD/AFM_DiDZoneComponent.c:591`, `AFM_DiDExtractionZoneComponent.c`

The extraction zone inherits `HandleActiveZoneLogic`, so `IsAttackDefeated()` ends it as
`FINISHED_HELD` once the pool is spent and the last paid-for attacker is dead. `Zone_3.layer` leaves
`m_bUseTicketPool` at its default (on) and the tuned conf gives zone 3 x1.25, clamped to 240 - so a
team that grinds the attack down wins the last stage with the helicopter never called.

Fix: override `IsAttackDefeated()` to return false in the extraction zone, or disable its pool.

- **Verdict:** Disregard, decision made on purpose to leave two victory variants
- **Notes:**

### 3. The extraction sortie timeout awards the win

`Scripts/Game/DiD/Spawners/AFM_DiDExtractionComponent.c:407`

`CompleteExtraction("sortie timed out")` fires 900 s after launch **regardless of state**, including
`HANDOVER` with nobody aboard. Players still fighting their way to the LZ get a surprise victory.
Likely root of the earlier "heli despawned, I was moved to spectator, match still active" report.

Fix: in `HANDOVER`, disarm the timeout or route it to `AbortExtraction` instead.

- **Verdict:** Fix
- **Notes:**

### 4. Running out of tickets freezes the attackers that are still alive

`Scripts/Game/DiD/AFM_DiDZoneComponent.c:779` (and `:774` for the contested pause)

The zone skips `spawner.Process()` entirely for pool-funded spawners once tickets are gone - but
`Process()` is also what re-tasks live AI: infantry hunting
(`AFM_DiDInfantrySpawnerComponent.c:78`) and the overwatch -> engage transition
(`AFM_DiDMechanizedSpawnerComponent.c:98`). At the exact moment the win condition needs the last
attackers to push, infantry stops chasing and vehicles park at overwatch forever. Same while the
zone is contested.

Fix: split "spawn a wave" from "update what is already out there" - e.g. an `UpdateTactics()` the
zone always calls, with only the wave spawn gated.

- **Verdict:** Fix
- **Notes:**

### 5. The mortar respawns instantly and leaks its old fire mission

`Scripts/Game/DiD/Spawners/AFM_DiDMortarSpawnerComponent.c:86`, `:208`

A new mortar is spawned on any tick where `m_SpawnedMortar` is null - no cooldown, so killing the
crew buys the players about one second. The dead mortar's entry stays in `m_mFireMissions` keyed on
the destroyed entity and `UpdateAllFireMissions()` skips it, so its artillery waypoints are never
cleared until zone cleanup.

- **Verdict:** Fix
- **Notes:** Make the mortar respawn delayed: It should make sense to players to hunt enemy mortars

---

## Correctness

### 6. A gap in zone indices restarts the mission

`Scripts/Game/DiD/AFM_DiDZoneSystem.c:242`

`m_aZones[newZoneIndex]` is indexed before the completion check at `:248`, which assumes indices are
contiguous `1..Count()`. With zones numbered 1, 2, 4, progression lands on a missing key ->
`m_ActiveZone` null -> `ProcessZone` (`:137`) calls `ActivateStartingZone()` and **zone 1 starts
over**, forever.

Fix: iterate sorted keys, or track a max index instead of `Count()`.

- **Verdict:** Fix, add a validataion check on system init, if zones are not sequential log an error to mission maker
- **Notes:**

### 7. The infantry spawner's own ticket limit does nothing

`Scripts/Game/DiD/Spawners/AFM_DiDInfantrySpawnerComponent.c:66`, `:121`

Both `Process()` and `SpawnSingleGroup()` are overridden without the base's ticket checks
(`AFM_DiDSpawnerComponent.c:107` and `:310`), so `m_bUseTickets` / `m_iMaxTickets` are unenforced and
`ConsumeTickets` drives the counter negative.

Live: zone 3's infantry spawner is authored with `m_bUseTickets 1, m_iMaxTickets 340`
(`Worlds/DiD_Linear_Lamentin_Layers/Zone_3.layer:830`) and that cap is silently ignored - only the
zone pool holds it back.

- **Verdict:** Fix
- **Notes:**

### 8. Failed crew spawn leaks a vehicle and a waypoint

`Scripts/Game/DiD/Spawners/AFM_DiDMechanizedSpawnerComponent.c:238-250`

The vehicle is inserted into `m_aSpawnedVehicles` and the overwatch waypoint is created before
`SpawnCrew`. If `!cm` or `!crew`, the function returns leaving an empty vehicle counting against
`m_iMaxActiveVehicles` and an untracked waypoint nothing ever deletes.

- **Verdict:** Fix
- **Notes:**

### 9. The default COWABUNGA squad is unarmed

`Scripts/Game/DiD/Spawners/AFM_DiDCowabungaComponent.c:18`, `:39-41`

The attribute says "Empty = riflemen with a machine gunner and an AT man", but the three fallbacks
are `Character_USSR_Unarmed`, `..._NI_Unarmed`, `..._Unarmed_KLMK`. Also `DEFAUALT` typo.

- **Verdict:** Unarmed on purpose, fix any incorrect comments and typos
- **Notes:**

### 10. ScriptInvoker arity mismatch

`Scripts/Game/DiD/AFM_DiDZoneSystem.c:123`

`m_OnZoneChanged` is invoked with a zone index here; every other call site passes nothing and the
subscriber `AFM_GameModeDiD.OnZoneChanged()` takes no parameters. Drop the argument.

- **Verdict:** Fix
- **Notes:**

### 11. Two modded classes are no-ops

`Scripts/Game/Modded/AFM_M_SCR_CampaignBuildingGadgetToolComponent.c`,
`Scripts/Game/Modded/SCR_AIDangerReaction_WeaponFired.c`

Both re-declare members that already exist in the base (`m_iConstructionValue`,
`AUDIBLE_DISTANCE_NORMAL` / `_SUPPRESSED`; vanilla has 500/100). A `modded class` derives from the
original, so vanilla methods that read those names still read the base declarations.

To change audible distance, override the method that uses it
(`SCR_AIDangerReaction_WeaponFired.PerformReaction`, vanilla line ~50).

- **Verdict:** Do not fix, this remark is false: Modded class can and does override properties, even when they are private. Save this into your memory
- **Notes:**

### 12. The warmup skip is only checked client-side

`Scripts/Game/DiD/AFM_VoteSkipWarmupAction.c`, `Scripts/Game/DiD/AFM_GameModeDiD.c:115`

`CanBeShownScript` does the admin check, but `RPC_DoForceEndPrepareStage` accepts the call from any
client. Re-check `SCR_Global.IsAdmin` server-side.

- **Verdict:** Note, leave a comment. Do not fix
- **Notes:**

### 13. Wave-zone path (latent)

Unused in the shipped world - only `Prefabs/MP/AFM_DiDZoneWavesPrefab.et` and
`Prefabs/Spawners/AFM_WaveInantrySpawner.et` reference it. If you come back to it:

- `StartWave` hands tickets to *every* active spawner (`AFM_DiDWaveZoneComponent.c:183`), including
  ones with `m_bUseTickets` off, which never consume them - so `GetRemainingTickets()` never reaches
  0 and **the wave can never complete**.
- `ApplyWaveDifficulty` (`:242`) reads the already-scaled interval/count and re-applies
  `pow(mult, wave-1)`, compounding to roughly 57x spawn count by wave 5.
- `HandleWaveActiveState` (`:129`) skips both `IsSpawnerEnabled()` and the contested pause, so
  scenario and server-config toggles do not apply inside wave zones.

- **Verdict:** Do not fix, wave spawners are outside the scope of current refactor
- **Notes:**

---

## Performance & hygiene

### 14. The full AI-world scan runs twice per second

`HandleActiveZoneLogic` calls `GetAICountInsideZone()` (`AFM_DiDZoneComponent.c:280` - iterates every
agent in the world), then `UpdateLocalGameState` calls it again via `GetRedforScore()`
(`AFM_GameModeDiD.c:282`). `GetDefenderCount()` likewise. Cache both per tick in the zone.

- **Verdict:** Fix
- **Notes:**

### 15. The perf instrumentation can never fire

`AFM_DiDZoneSystem.c:186` ("ProcessZone took %1 ms") and the timing in `AFM_DiDZoneComponent.c:282`
diff *server timestamps*, which do not advance within a frame - the result is always 0. Use
`System.GetTickCount()`.

- **Verdict:** Remove the log and time measuring logic, not needed
- **Notes:**

### 16. Mortar debug visualisation defaults to on

`AFM_DiDMortarSpawnerComponent.c:33`, `:545`. `m_aDebugShapes` only clears on cleanup. Default it
to 0.

- **Verdict:** Remove debug visualisation
- **Notes:**

### 17. 14 log calls pass LogLevel positionally

`LogLevel` lands as a format argument instead of the level. Two downgrade real errors to normal -
`AFM_DiDZoneComponent.c:122` ("Failed to register zone!") and `AFM_DiDSpawnerComponent.c:328`
(failed group spawn) - and nine DEBUG lines spam the server log at normal level every wave.

Full list: `AFM_CrewConfig.c:65,74,90`; `AFM_DiDZoneComponent.c:122`;
`AFM_DiDMechanizedSpawnerComponent.c:82`; `AFM_DiDSpawnerComponent.c:57,84,87,281,312,324,328,444,463`.

- **Verdict:** Fix
- **Notes:**

### 18. Config traps

- Turning off `m_bStopTimerOnRedforSuperiority` silently disables the whole failure timer: the zone
  never enters `FROZEN`, so nothing drains `m_iRemainingFailureSeconds`
  (`AFM_DiDZoneComponent.c:741`, `:755`).
- `m_fCruiseSpeed` in `AFM_DiDExtractionComponent.c:55` is never read - both waypoints are built with
  `m_fApproachSpeed` (`:667-668`).

- **Verdict:** Fix
- **Notes:**

### 19. Brittleness / duplication

- `LateInit`'s `switch (e.Type())` (`AFM_DiDZoneComponent.c:144`) matches exact types, so any future
  spawner subclass silently logs "Unknown type" and is dropped. `AFM_DiDSpawnerComponent.Cast(e)`
  alone would cover it.
- `IsPointInZone` implemented twice: `AFM_DiDZoneComponent.c:248` and
  `AFM_DiDMortarSpawnerComponent.c:518`.
- The flight-constant block (spawn height, cruise/approach speed and height, waypoint reached radius)
  is duplicated between `AFM_DiDHeliSpawnerComponent.c:54-73` and
  `AFM_DiDExtractionComponent.c:51-64`.

- **Verdict:** Remove duplicates, make helper class if required
- **Notes:**

### 20. Small things

- `AFM_ScoreInfoDisplay.c:85` is missing its semicolon.
- `KeepBody` (`AFM_GameModeDiD.c:322`) is redundant now that the garbage collector is disabled;
  bodies are only cleaned by `OnPlayerRespawned`.
- `ConsumeTickets` (`AFM_DiDSpawnerComponent.c:438`) can drive `m_iRemainingTickets` negative, which
  then feeds `GetRemainingSpawnTickets()` when a zone has no pool.

- **Verdict:** Fix
- **Notes:**

---

## Clean

- No ternary operators anywhere in the Enscript.
- The ticket-pool, failure-timer and saved-loadout work from this session reads consistently; the
  pool guards (`m_bTicketPoolSized`) are respected by every reader.
- Waypoint ownership in the mechanized and infantry spawners correctly never deletes placed
  waypoints shared between groups.
- Mod hooks that do work: `AFM_M_SCR_MissionHeader` + `AFM_M_ArmaReforgerScripted` (T172515
  workaround), `AFM_M_PS_ManualMarker`, `AFM_M_REAPER_AiHelicopterControllerComponent`,
  `AFM_M_SCR_CampaignBuildingPlacingObstructionEditorComponent`,
  `AFM_M_SCR_CampaignBuildingCompositionComponent`, `AFM_M_SCR_CharacterRankComponent`,
  `AFM_M_SCR_CharacterDamageManagerComponent`, `AFM_M_SCR_AICombatComponent`.
