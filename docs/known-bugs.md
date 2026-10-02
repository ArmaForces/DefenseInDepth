# Known bugs

From playtests, newest first. Each one says what is known, what is suspected, and what would settle it -
so the next session can collect the right thing rather than confirming the bug exists again.

---

## 1. Players not moved from spectator into their body at match start — **major**, with crashes

**Reported:** 2026-10-02 session.

Not every player gets moved from spectator to their character when the first stage starts, and **some
players crash** while it happens. Crash logs to follow.

Yesterday's fix (`67b897f`, `AFM_M_PS_SpectatorMenu`) does **not** solve it. That one only closes the
spectator camera once a player already controls a live body, so it addresses the symptom where the HUD and
the camera disagree. It cannot help a player who never got a body in the first place, and it is not a
plausible cause of a crash: it only reads the controlled entity and calls PS's own `SwitchFromObserver`.

Where the body comes from, in order:

1. `AFM_GameModeDiD.PopulateZone` → `SpawnPlayerBody` per player, then `AssignPlayerBody` after
   `ASSIGN_DELAY_MS` (500 ms), retried up to `ASSIGN_MAX_ATTEMPTS` (10).
2. `AssignPlayerBody` → `PS_PlayableManager.ApplyPlayable`, which sets the main entity, sets the faction
   from the body and defers `ChangeGroup` by a frame.
3. `RestorePlayerRank` at +300 ms, `ApplySavedLoadout` at +300 ms, `ClearOldBody` at +2000 ms.

Suspects, roughly in order of how much they would explain:

- **The retry loop gives up.** Ten attempts at 500 ms is five seconds; if a playable is not registered by
  then the player is left in spectator with no further attempt and - worth checking - possibly no log line
  loud enough to notice.
- **A race between `ApplyPlayable` and the deferred `ChangeGroup`**, which is a frame later and touches the
  group manager. A player who disconnects, reconnects or is still loading inside that window is the obvious
  candidate for both a missed body and a null dereference.
- **`ClearOldBody` deleting the wrong body** when two stages overlap, which would take a body away from a
  player who had just been given one.
- The crash may be client side in the spectator or respawn UI rather than in any of this.

**To collect:** the crash logs, and the server log around the stage start - every
`AFM_GameModeDiD: Player %1 ...` line, plus how many players the `Spawning bodies for N players` line
counted against how many actually arrived.

---

## 2. Saved loadouts only partly restored

**Reported:** 2026-10-02 session. Possibly also the cause of the 28.09 complaint, which was assumed fixed.

A respawned player gets part of their saved loadout - reportedly the weapon only - rather than what they
saved at the arsenal.

Ours is `AFM_GameModeDiD.ApplySavedLoadout`, called 300 ms after the body is assigned, which hands the work
to vanilla's `SCR_LoadoutManager`. Two things to check before touching our side:

- **Timing.** The loadout is applied while the body is still settling, and vanilla's own flow applies a
  loadout as part of spawning rather than afterwards. Inventory that is still being populated when the
  loadout arrives could explain "the weapon made it, the rest did not".
- **What is saved.** The arsenal's save type decides what may be stored
  (`SCR_EArsenalSaveType`: `IN_ARSENAL_ITEMS_ONLY`, `FACTION_ITEMS_ONLY`, `NO_RESTRICTIONS`). If our
  arsenals are set to in-arsenal-items-only, anything not in that arsenal's list is silently dropped at
  save time, and the bug is in the prefab rather than in the restore.

**To collect:** one player's saved loadout versus what they got, item by item, and which arsenal they saved
at.

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
