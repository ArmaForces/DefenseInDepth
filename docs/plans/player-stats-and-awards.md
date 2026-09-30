# Player stats and end-of-match awards

Goal: a results page at the end of a match that shows what each player did and hands out titles -
most bots killed, most RPGs to the face, most time spent building, most time in the arsenal.
Written 2026-09-29 from reading the vanilla data collection subsystem and the hooks a bespoke
tracker would need. **Phases 1-4 are implemented and playtested; phase 5 is the page.**

---

## Why not the vanilla component

`SCR_DataCollectorComponent` with `SCR_PlayerData` is a **career profile** system, not a scoreboard:

- `SCR_PlayerData` is a `JsonApiStruct` holding `BackendCallback m_CharacterDataCallback` and
  `m_StoringCallback` (`SCR_PlayerData.c:61-62`) - its storage is BI's backend, keyed to the player's
  identity, and its writes need online privileges. Our own compile warnings already show the API it
  leans on is deprecated: `SCR_DataCollectorComponent.c:481,483` call `GetStorage` and
  `GetOnlineWritePrivilege`, both flagged obsolete. On a private dedicated server that is exactly where
  it gets unreliable.
- The stats are lifetime totals: `SCR_EDataStats` (`SCR_PlayerData.c:918-958`) is RANK,
  LEVEL_EXPERIENCE, SESSION_DURATION, specialization points, WARCRIMES, KILLS, AI_KILLS, DEATHS,
  distances, medical actions, and kick/ban streaks. There is no notion of *this match*, so "top scorer
  of the match" cannot be expressed without diffing snapshots.
- Nothing resembles time spent building or in an arsenal, and nothing is weapon-specific.

So: keep our own per-match table. Vanilla stays available if career context is ever wanted, but it is
not the thing to extend.

---

## Where the numbers come from

### Kills - one hook covers most of it

`SCR_BaseGameMode.OnControllableDestroyedEx(SCR_InstigatorContextData)` fires for **every** controllable,
AI included, on the authority. The context carries what a tracker needs without any guessing
(`SCR_InstigatorContextData.c:3-17`):

- victim and killer player IDs, and both entities
- `SCR_ECharacterControlType` for each - AI, player, possessed AI, GM
- `SCR_ECharacterDeathStatusRelations` - enemy kill, friendly fire, suicide

"Most bots killed" is then killer control type player, victim control type AI. Friendly fire and
suicides separate themselves. Note the game mode has no kill override at the moment: `OnPlayerKilled`
went away with the body-keeping workaround, so this is new surface rather than an edit.

### Weapon-specific kills - the one place we have to approximate

The context has no weapon. The cheap route is to read the killer's current weapon at the moment of the
kill and classify by prefab; the exact route is hooking damage on the victim and reading the ammo type,
which costs a per-character hook. Start cheap: "RPG to the face" only has to be *fun*, not audit-grade.

### Structures built - already tracked

`AFM_M_SCR_CampaignBuildingCompositionComponent` already reads `m_iBuilderId` and registers each
finished composition with the active zone. Counting per builder there is a couple of lines.

### Time in build mode - server-side, via the provider

`SCR_CampaignBuildingManagerComponent.SetEditorMode` calls `providerComponent.AddNewActiveUser(playerID)`
(`:460`) on the authority, and the provider drops users in `RemoveActiveUsers`. A modded provider can
timestamp both and accumulate seconds per player. `GetOnPlaceEntityServer()` is there too if placement
events turn out to be a better proxy than wall-clock time.

### Time in the arsenal - client-side only

The single event is `SCR_InventoryStorageBaseUI.GetOnArsenalEnter()`, a **static invoker on a UI class**
(`SCR_InventoryStorageBaseUI.c:67-76`), so it exists only on the machine with the menu open. Options,
in order of honesty:

1. Measure on the client, report seconds to the server by RPC when the menu closes. Simple, and as
   trustworthy as the client - which for an award like this is fine.
2. Approximate on the server: the player is near an arsenal and their inventory is open.
3. Drop the metric.

This is the only one of the four that cannot be done purely on the authority, and it is worth saying so
before it turns into a bug report.

### Zone and objective stats - ours already

Zones held, contested seconds survived, whether the player was aboard the extraction: the zone system
and extraction component already know all of it.

---

## Shape

**`AFM_DiDStatsComponent`** on the game mode, authority-side, holding
`map<int, ref AFM_DiDPlayerStats>`. Each record is an enum-indexed `array<float>` - vanilla's own trick,
cheap to extend - plus the player's identity id, since `SCR_PlayerIdentityUtils.GetPlayerIdentityId` is
what survives a reconnect and we already use it for arsenal loadouts. Player ids key the live table;
identity ids key anything that has to outlive a disconnect.

**Awards from config, not code.** `AFM_DiDAwardConfig` as `[BaseContainerProps(configRoot: true)]`,
a list of entries each naming: the stat, the title to show, whether the winner is the highest or lowest,
a minimum value so nobody is crowned "Most RPGs to the face: 0", and what to do with ties. Same pattern
as the side configs, so a new title is a config edit.

**One broadcast at the end.** The live table stays on the server; when the match ends, the results the
page needs go over the wire once - player ids, the handful of stat values shown, and the awarded titles.
Nothing per-tick, nothing replicated continuously.

**The page itself.** `PS_DebriefingMenu` (`ChimeraMenuBase`) already builds faction report frames from a
layout into `BodyHorizontalLayout` at `OnMenuOpen` (`PS_DebriefingMenu.c:24-88`), so a modded class can
append an awards frame next to the existing ones, using a layout of ours. That keeps the mission's
existing end-of-match flow rather than bolting a second screen onto it.

---

## Risks and things to get right

1. **AI killing AI** would otherwise inflate everything: filter on killer control type being a player.
2. **COWABUNGA flips players to the attacking side.** Kills made while turned should probably be their
   own stat rather than counted as friendly fire; decide before the award list is written.
3. **Players change bodies every stage**, so stats must key on the player, never the entity or playable.
4. **Reconnects**: a player who drops and returns gets a new player id. Fold on identity id at match end
   or their match splits in two.
5. **Arsenal time is client-reported** (above). Cap it at something sane so a hung client cannot claim
   the title with nine hours.

---

## Phases

**Phase 1 - the table and kills. Done.** `AFM_DiDStatsComponent`, the stat enum, `OnControllableDestroyedEx`
wired to bot kills, player kills, friendly fire, deaths. A console dump so the numbers can be read in
the log before any UI exists.

**Phase 2 - the things we already know. Done.** Structures built, build-mode seconds, zones survived,
extraction. All authority-side.

**Phase 3 - weapon flavour and arsenal time. Done.** Killer's weapon at kill time; client-reported arsenal
seconds with a cap.

**Phase 4 - awards. Done.** `AFM_DiDAwardConfig` with an entry per title (stat, unit, minimum,
highest-or-lowest, tie handling), resolved on the authority in `AFM_DiDStatsTracker.BuildResults` and
broadcast row by row from `AFM_GameModeDiD.BroadcastMatchResults`. Default titles in
`Configs/Awards/DiD_Awards.conf`. Every machine assembles its own `AFM_DiDMatchResults`.

**Phase 5 - the page.** A layout and a modded `PS_DebriefingMenu` that appends it, reading
`AFM_GameModeDiD.GetMatchResults` on open and `GetOnMatchResults` for rows that arrive after it.

Phase 1 is worth playtesting on its own: if the kill numbers in the log look right after a match, the
rest is presentation.
