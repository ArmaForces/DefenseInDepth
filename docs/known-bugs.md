# Known bugs

From playtests, newest first. Each one says what is known, what is suspected, and what would settle it -
so the next session can collect the right thing rather than confirming the bug exists again.

---

## 1. Players not moved from spectator into their body at match start — stuck camera **fixed**, crash **possibly fixed**

**Reported:** 2026-10-02 session. The survey puts it at "practically every round, either a crash or stuck in
spectator" for one player, and "didn't always spawn at the start" for another.

**Status:** the stuck camera is **fixed** and confirmed in play on the evening of 02.10. The crash is
**possibly fixed**: it was reproduced once more on 02.10 at 21:37, then did not happen in the 03.10
playtest with the spawn warm-up in. Logs analysed: `docs/logs_stuck` (client, 02.10,
Powerplant, four joins, plus `server_console.log`, the server's side of the 18:28 join), `docs/logs_crash`
(client, 01.10, Lamentin Tuned, one join that crashed, plus its server log), and `docs/log_crash` (client
and server, 02.10 evening, six joins with the fix in, the last one crashing). The first two folders have since
been removed from disk; their findings are kept here.

### Stuck in the spectator camera: the body arrives after the observer is already gone

Every join, side by side. "From" is the entity the client was controlling just before the body, as logged by
`PS_VON_DIAG event=CONTROLLED_ENTITY_CHANGED`.

| Join | Body reaches the client | From | Result |
|---|---|---|---|
| 02.10 18:24, stage start | 18:24:15.689, 5.7 s after GAME | `NULL` | stuck |
| 02.10 18:25, rejoin mid-stage | 18:25:12.306 | the observer (`…326`) | correct body |
| 02.10 18:26, stage start | 18:26:44.331, 5.7 s after GAME | `NULL` | stuck |
| 02.10 18:28, stage start | 18:28:11.845, 5.7 s after GAME | `NULL` | stuck |
| 01.10 19:48, stage start | 19:48:10.693, 5.6 s after GAME | the observer (`…320`) | took the body, crashed 181 ms later |

Every stuck join has `from=NULL`; every join that worked has the observer there. The sequence:

1. When GAME starts the player has no playable yet, so PS puts them in its observer: on the client
   `ApplyPlayable` → `SwitchToObserver(null)` opens the spectator menu and camera. No change with both a
   `from` and a `to` has happened yet, so PS's `m_bAfterInitialSwitch` is still false on that client.
2. About 5 s later the zone hands out bodies: `AssignPlayerBody` → `SwitchPlayerToPlayable` → on the server,
   PS's `ApplyPlayable` makes the body the player's main entity and **deletes the observer on the next frame**
   (`PS_PlayableManager.c:262-265`, `m_CallQueue.Call(SCR_EntityHelper.DeleteEntityAndChildren, initialEntity)`).
3. Both reach the client by replication, and nothing orders them. When the deletion is processed first, the
   client's controlled entity drops to nothing and the handover arrives as `from=NULL`. The 18:24 join shows
   it directly: the body was allocated at the freed observer's address (`0x000001A3928107D0`).
4. PS's handler, `PS_PlayableControllerComponent.OnControlledEntityChanged` (`:452`), opens with
   `if (!from && !m_bAfterInitialSwitch) return;`. That guard exists to skip the very first nothing-to-observer
   change on joining. Here it also swallows the observer-to-body change, so `SwitchFromObserver` never runs:
   the menu and camera stay up while the HUD follows the body.

**The server side of the 18:28 join** (`server_console.log`) shows the server did its part on time. Server
clock:

| Server time | Event |
|---|---|
| 18:28:03.431 | GAME |
| 18:28:08.445 | `Spawning bodies for 1 players at the stage's spawn point` |
| ~18:28:08.93 | switch to the body; inferred, the first `Player 1 respawned with rank` line at 09.231 runs 300 ms after it |
| 18:28:10.931 | `ClearOldBody`, 2 s after the switch (the second rank line) |
| 18:28:15.797 | client disconnects |

There are no errors and no `never registered as playable` line, so the body was registered and assigned on
the first attempt. That rules out the retry loop and anything else on the server. The client clock runs about
2.7 s ahead of the server's (GAME at 06.157 against 03.431, disconnect at 18.442 against 15.797), which puts
the client receiving the body at about 18:28:09.15 server time: **roughly 200 ms after the switch**, with the
observer's deletion sent one frame after the switch. The race window is that short, so it does not need a slow
client to lose it. The "5.7 s after GAME" in the table above is our own schedule (bodies at GAME + 5 s, the
switch 0.5 s later), not loading time.

This explains the details too:
- **Rejoining fixes it.** A mid-stage join gets a fresh observer and the body arrives with `from` set, so PS
  takes the normal path (the 18:25 row).
- **Game Master and back fixes it.** Closing the editor calls `SwitchFromObserver` directly.
- **It is intermittent.** It is a race between two messages a frame apart, so some joins win it. The 01.10
  join won (its observer had moved to `<1512, 57, 5890>`, next to the body, while on Powerplant it waited at
  `<0, 5000, 100>`). Whether the observer's distance from the body affects the odds is not shown by these
  logs: one winning join is not enough.

`ClearOldBody` is not involved. At match start the "old body" it records is the observer, which PS has
already deleted by the time it runs 2 s later, so it finds nothing.

**Why `67b897f` didn't catch it.** The modded `PS_SpectatorMenu` is loaded on the client (its compile warning
for line 38 appears on every join), but it logs nothing, so these logs cannot show whether it ran. One
possible reason for the 18:24 join: the body sits at the deleted observer's address, so if PS's
`m_InitialEntity` still held that address, `controlled == GetInitialEntity()` was true and the hook stood
down. That does not cover the 18:26 and 18:28 joins, where the addresses differ. Unexplained.

**Fix (confirmed in play).** `Modded/AFM_M_PS_PlayableControllerComponent.c` overrides PS's private
`OnControlledEntityChanged`. After PS's own handler, it calls `SwitchFromObserver` when this machine owns the
controller, the new entity is a living character without `PS_LobbyVoNComponent`, and the spectator camera is
still up. It only acts when PS didn't: once PS leaves spectator, the camera is gone.

In `docs/log_crash` the race was lost twice (20:58 Powerplant and 21:01 Lamentin, both `from=NULL`). Both
times `AFM_PlayableController: Left spectator for the new body that PS skipped` was logged and the player got
their body. The old menu hook's line (`AFM_SpectatorMenu: Left spectator from the menu`) never appeared, so
the hook in `AFM_M_PS_SpectatorMenu.c` is now redundant and can be removed after one more playtest.

### Crash while taking the body — possibly fixed

Two crashes, both on the client and both on Lamentin Tuned:

| | 01.10 19:48 | 02.10 21:37 |
|---|---|---|
| Crash GUID | `bf4b9a27-7b3b-450c-bec9-b8343afd416d` | `05489dec-bc2e-40e5-b7a6-c2ac19a01f24` |
| Path | PS normal path (`from` = observer) | PS normal path (`from` = observer) |
| FPM exception after the switch | 20 ms | 22 ms |
| Crash after the switch | 181 ms | 152 ms |
| Last line before the crash | `PS_VON_DIAG event=MENU_STATE_ENTER`, 1 ms earlier | the same, 1 ms earlier |

**It is one crash site, not two.** Both are a native access violation, "Illegal read by `0x7ff75bb0fb68`", and
the eleven-frame native stack in `crash.log` is identical, address for address. No script frame is on the
stack, so it needs the dump or Bohemia's analysis to name the function.

**Bohemia's stack trace** (received 03.10), innermost first:

```
enf::rpl::RemoveFromRecvChannel        replication/core/enf_istreams.cpp(699)
enf::rpl::DestroyStream                replication/core/enf_istreams.cpp(478)
enf::rpl::EncodingJob::PostProcess     replication/enf_pip_encoding.cpp(1021)
enf::RplProcessToken::RplProcessToken  replication/enf_mod.cpp(515)
enf::BaseWorld::InternalUpdateEntities baseworld/enf_worldsimulation.cpp(2002)
gamecode::ChimeraWorld::InternalUpdateEntities
gamelib::Game::OnUpdate  ...  WinMain
```

What it says:
- **The crash is in the engine's replication core, not in any script.** At the start of a frame the client
  processes what arrived from the server (`RplProcessToken`), and while finishing that (`EncodingJob::PostProcess`)
  it tears down a replication stream (`DestroyStream`) and takes it off its receive channel
  (`RemoveFromRecvChannel`). That last step reads freed memory.
- **A stream being destroyed means a replicated entity leaving this client**: the server deleted it, or it
  streamed out of range. So the crash happens while the client processes an entity's removal.
- **Nothing in the trace is VoN, radio, map markers, UI or our code.** FPM's exception and the preview
  character drop out as causes: they are script-level and finished frames earlier. They can only matter
  indirectly, by changing what gets deleted when.
- **It fits the lead below.** On PS's normal path the one deletion that reaches the client right after the
  switch is the observer's: PS deletes it on the server one frame after the switch. That is an entity this
  client owned and controlled until a moment ago, whose ownership has just moved to the body. A stream torn
  down right after its channel role changed is a plausible place for an engine bug to bite. This is
  inference: the trace names the operation, not the entity.
- **On "radio related":** the PS observer carries its own radio (the transceivers on 998 and 999 in the
  `PS_VON_DIAG` lines), and `DeleteEntityAndChildren` removes it together with the observer. So a radio
  stream is among those destroyed, which may be why it looks radio-related. Nothing in the trace singles it out.
- **It is not DiD's bug, and that matches the reports from other modes.** Every PS mission switches players
  out of the observer this way, and PS deletes the observer on the next frame in all of them. DiD's part is
  only that it does this at every stage start, for every player at once.

Why it is hard to reproduce: it needs the deletion to land in a narrow window after the hand-over, which
depends on network timing. A local or Workbench session will rarely hit it, and neither faction choice showed
a difference in a handful of tries.

**The server is not involved.** Both server logs are clean at the crash moment and only notice the disconnect
by timeout. Allowing for the clock offsets (client about 1.8 s ahead on 01.10, about 2.9 s on 02.10), the
client crashes about 300 ms after the server's switch both times. That is also exactly when our
`RestorePlayerRank` runs on the server, but that is a coincidence: the rank was already right (1 = 1), so
`SetCharacterRank` sent nothing, and `ApplySavedLoadout` sends nothing either. Ruled out.

**What happens on the client in those ~150 ms** (02.10 21:37, `docs/log_crash/console.log`): the switch
arrives with the observer as `from`; 2 ms later the client spawns a local preview character
(`CharacterBasebody_Asian_02.et`, `ENTITY:5` at 0,0,0); 22 ms later FPM throws in `OnGroupJoined_C`; the
radio entries of the new body are added; VoN moves from the observer's `PS_LobbyVoNComponent` to the body's
`SCR_VoNComponent`; then the crash, 1 ms after the `MENU_STATE_ENTER` diagnostic.

**Every stage-start join with the fix in, side by side** (`docs/log_crash`):

| Join | Path | Preview spawned | FPM exception | Result |
|---|---|---|---|---|
| 20:58 Powerplant | `from=NULL`, our fix | (not checked) | no | ok |
| 21:01 Lamentin | `from=NULL`, our fix | yes | no | ok |
| 21:15 Lamentin | PS normal path | yes | no | ok |
| 21:17 Lamentin | PS normal path | no | yes | ok |
| 21:35 Lamentin | PS normal path | no | no | ok |
| 21:37 Lamentin | PS normal path | yes | yes | **crash** |

What this does and does not show:
- **FPM is not sufficient.** It threw at the 21:17 stage start and at three respawns mid-match (21:04, 21:09,
  21:21) without a crash, and Bohemia's trace has no script in it. It throws on group join, which happens at
  every switch, so it shows up next to the crash without causing it. FPM is a third-party map marker mod; its
  `m_aDynamicMarkers` is null when a group-join RPC arrives.
- **The preview is not sufficient either**, and the 01.10 crash had none.
- **Both crashes took PS's normal path, and none of the five `from=NULL` joins crashed** (18:24, 18:26,
  18:28, 20:58, 21:01). The normal path crashed in two of six joins (01.10 19:48, and on 02.10 the 18:25
  rejoin, 21:15, 21:17, 21:35 and 21:37). The normal path is the one where the observer still exists when the
  client takes the body, so its deletion arrives *afterwards*, while VoN and the radios are being handed from
  the observer to the body. A late deletion of an entity something still reads would produce exactly an
  "illegal read". This is the strongest lead, but numbers this small are not proof.
- **The camera had already switched** (seen in play, 02.10 21:37): the screen froze on the view from the
  spawned body, not the spectator camera. So the spectator camera was gone and the player's own camera was up
  when it crashed. That fits the order in the log: `SwitchFromObserver` ran with the switch, and the crash
  came about 150 ms later. It points away from the spectator camera itself and towards what still refers to
  the observer, or arrives for it, after the hand-over.

**Only the first spawn crashes** (stress test, 03.10): switching players between spectator and a body over
and over, mid-match, never crashed. Only the first switch of the match did. That is the moment the client has
the most to replicate at once: the stage area streamed in for the first time, arsenals and building menus,
the player group (created in the same frame), the character's equipment and radios, plus the GAME transition
itself. It fits replication lag widening the window for the engine bug. The stress test was removed
afterwards.

**Mitigation (implemented, possibly fixed): spawn warm-up.** `m_iSpawnWarmupSeconds` on the game mode
(default 30, 0 = off). At the first stage start only, every spectator's camera is moved 10 m above the
stage's player spawn point, which makes PS move their observer there too, so the server streams the area to
them while they only watch. Bodies are handed out when the time is up. The first stage's prepare phase is
extended by the same time, so the wait costs no preparation. Players joining during the warm-up are moved too
and get a body with everyone else. Skipping the prepare phase ends the warm-up at once. Log lines:
`AFM_GameModeDiD: Spawn warm-up - N players wait 30 s ...` and `Spawn warm-up over (...)` on the server, and
`AFM_PlayableController: Spectator camera moved to ...` on each client.

**Playtest result (03.10): no crash**, including with a player prefab carrying a radio in its equipment.
Marked possibly fixed rather than fixed: the crash came on roughly one first spawn in three before, so one
clean playtest is encouraging but not proof, and the warm-up works around the engine bug rather than removing
it. If it comes back, the options below still apply, and the crash GUIDs above are the reference for
Bohemia.

**Options if the warm-up doesn't cure it (none implemented).** All of them separate the observer's deletion from the ownership hand-over,
which is the one thing the trace and the join table point at together.

1. **In PS, delay the observer's deletion** (`PS_PlayableManager.ApplyPlayable`, `:262-265`) from the next
   frame to a few seconds later. The observer is unowned and out of sight by then, so keeping it briefly
   costs nothing. This fixes it for every PS mission, which matches the reports from outside DiD, so it is
   the better place if the PS fork accepts it.
2. **In DiD, delete the observer *before* the switch**: in `AssignPlayerBody`, delete it on the server a
   moment before switching the player to the body. Every join then takes the `from=NULL` path, which has not
   crashed in five joins and which the stuck-camera fix now handles. The cost is a few hundred milliseconds
   without spectator VoN. Only covers DiD.
3. **Report it to Bohemia** with this trace: a client crash in `rpl::DestroyStream` /
   `RemoveFromRecvChannel` when the server deletes the entity a player controlled one frame after giving
   them control of another (`SetInitialMainEntity` followed by `DeleteEntityAndChildren` of the old one).
   That is their bug whatever we do on our side.

Whichever is tried, the evidence will be slow: the crash hit two of six normal-path joins. A deliberate
stress test (one client switched back and forth between two entities, the old one deleted on the next frame,
dozens of times over a real network) would show far sooner whether an option works than waiting for a
playtest to crash.

**To collect:** which of the two GUIDs Bohemia's trace belongs to. The native addresses in our two
`crash.log` stacks are identical, so it almost certainly covers both.

### Noise in these logs, not this bug

- **The 18:43 crash in `logs_stuck`** (GUID `67dbddb9-17b8-45ca-a208-1390b83cd952`) happened on quitting the
  game from the main menu ("Game destroyed", then the crash), 15 minutes after leaving the server.
- **`PS_PlayableManager.SetPlayerState` exceptions (`m_eGameState`)** each fire about 0.5 s after the client
  disconnects: `PS_PlayableComponent.OnDelete` runs during world teardown, after the game mode is gone, and
  reaches server-only code. Harmless, and a PS bug rather than ours.
- **`Defender side config DiD_Side_UK_1989.conf could not be loaded`** (01.10): that config lives in the
  `DiD_BritishForces` addon, which the client did not have loaded. The side is resolved on the server, so it
  is harmless on a client. A server running a mission with the UK side does need the addon.

Line numbers above are from the local PlayableSelector checkout. The running PS build is newer
(`SetPlayerState` is at line 617 in the crash stacks, line 584 locally), so treat them as approximate.

---

## 2. Saved loadouts only partly restored — **fixed**

**Reported:** 2026-10-02 session, and confirmed in the 02.10 survey ("the loadout didn't save, or saved
halfway"). Possibly also the cause of the 28.09 complaint, which was assumed fixed.

A respawned player gets part of their saved loadout - reportedly the weapon only - rather than what they
saved at the arsenal.

Ours is `AFM_GameModeDiD.ApplySavedLoadout`, called 300 ms after the body is assigned. It checks the body is
on the defending side, then calls vanilla's `SCR_PlayerArsenalLoadout.OnLoadoutSpawned` directly, because
PS spawns bodies outside vanilla's loadout pipeline.

### Found in the 01.10 server log: the restore throws before it applies anything

`logs_crash/server_console.log`, at the stage 2 respawn:

```
19:50:48.079 SCRIPT    (E): Virtual Machine Exception
Reason: NULL pointer to instance. Variable '#return'
scripts/Game/GameMode/Loadout/SCR_PlayerArsenalLoadout.c:147 Function OnLoadoutSpawned
scripts/Game/DiD/AFM_GameModeDiD.c:1118 Function ApplySavedLoadout
19:50:48.088 SCRIPT       : AFM_GameModeDiD: Applied saved arsenal loadout to player 1
```

- **What fails.** In the vanilla source, line 147 of `OnLoadoutSpawned` is
  `factionKey == factionComponent.GetAffiliatedFaction().GetFactionKey()`: the body's
  `FactionAffiliationComponent` has no affiliated faction yet, 300 ms after the switch. Our own guard passed
  just before, because it asks the character (`SCR_ChimeraCharacter.GetFactionKey`) rather than that
  component, so the two disagree at that moment.
- **What it does to the player.** The exception aborts the function before `ApplyLoadoutString`, so
  **nothing** from the save is applied, and the player keeps the body prefab's default kit. That kit has a
  weapon, which could be what "only the weapon was saved" looked like from the inside; that part is a guess.
  It also aborts before vanilla's "delete the save on failure" branch, so the save survives for the next try.
- **Our log lies.** `Applied saved arsenal loadout` is printed unconditionally after the call, so the
  server log reports success for a restore that threw.
- **How often.** This is the only restore of a saved loadout in that log, so 1 of 1. The 02.10 server log
  (`logs_stuck`) has no saved loadout to restore.

**Fix (confirmed in play on the evening of 02.10).** `ApplySavedLoadout` now waits until the body's
`FactionAffiliationComponent.GetAffiliatedFaction()` is set: up to 10 retries 300 ms apart, the way
`AssignPlayerBody` waits for the playable id. It warns if the faction never arrives. The defender check now
reads the same component as vanilla. The old "Applied" line is replaced by
`Restoring saved arsenal loadout for player N (body ready after K retries)`, logged before the call, since
the applier reports nothing back.

**What to look for in the next playtest's server log:** the `Restoring...` line with its retry count, and no
`SCR_PlayerArsenalLoadout` exception after it.

### Still worth checking if the fix doesn't cure it

- **What is saved.** The arsenal's save type decides what may be stored
  (`SCR_EArsenalSaveType`: `IN_ARSENAL_ITEMS_ONLY`, `FACTION_ITEMS_ONLY`, `NO_RESTRICTIONS`). If our
  arsenals are set to in-arsenal-items-only, anything not in that arsenal's list is silently dropped at
  save time, and the bug is in the prefab rather than in the restore.

**To collect, if needed:** one player's saved loadout against what they got, item by item, and which
arsenal they saved at.

---

## 3. Alive players could not hear spectators at the debriefing — **possibly fixed, untested**

**Reported:** 2026-10-02 session. Addressed by `3e74d7d`, not yet played.

Survivors kept talking through their character's radio while everyone already watching was on the lobby
VoN, so the debrief was two conversations. `3e74d7d` moves every player to their observer as the match
advances to `DEBRIEFING`, before the room move, which should put the whole lobby in one room.

**To confirm:** at the debrief, a survivor and a spectator can hear each other, and the voice panel on the
debriefing screen lists everyone in one room.

Note this does **not** cover the same split *during* a stage - a living player on a character radio and a
spectator on the lobby VoN cannot hear each other mid-match by design. Fixing that needs the listening
radio on the observer (defenders' encryption key, player group frequency, receive only) plus PS's camera
position sync, which is scoped but not built.
