# Faction parametrisation

Goal: one config file per side, holding everything faction-specific, so a scenario can be re-sided
without re-authoring it. Written 2026-09-28 from three explorations of the mod, of vanilla, and of the
PlayableSelector framework. **Plan only - nothing implemented.**

---

## What the exploration established

### The AI side is already concentrated

No world layer overrides a single faction-specific prefab. Every faction value the attackers use is
authored in six spawner prefabs, and the layers only override tuning numbers:

| Prefab | Faction content |
| --- | --- |
| `Prefabs/Spawners/AFM_DidInfantrySpawner.et:10` | 10 `Group_USSR_*` group prefabs |
| `Prefabs/Spawners/AFM_MechanizedSpawner.et:11-16` | UAZ469_PKM + BRDM2, `Character_USSR_Crew`, `Character_USSR_Randomized` |
| `Prefabs/Spawners/AFM_MortarSpawner.et:12-16` | `Character_USSR_Engineer`, `DiD_MortarComposition_USSR` |
| `Prefabs/Spawners/AFM_HeliSpawner.et:10-15` | Mi-8MT gunship + `REAPER_USSR_HelicopterCrew` |
| `Prefabs/Spawners/AFM_ExtractionSpawner.et:9` | UH-1H (defender side) |
| `Prefabs/Spawners/AFM_WaveInantrySpawner.et:8` | 4 `Group_USSR_*` groups (used by the DiD_Zombie addon) |

Those six are instanced by every zone in **six worlds** - Lamentin, Powerplant, Beauregard,
Cain_GreenHill and two dev worlds - so one config reaches all of them.

In script, only four files name a faction: `AFM_GameModeDiD.c:7,10` (the two `FactionKey` attributes),
`AFM_DiDCowabungaComponent.c:15,40-42` (squad group + three unarmed character constants),
`AFM_DiDHeliSpawnerComponent.c:33,114` (crew + Mi-8 fallback) and `AFM_DiDExtractionComponent.c:33,60`
(crew + a fallback that is a *USSR* Mi-8 - a latent bug, see "Incidental fixes").

Everything else already asks the zone or the game mode: `AFM_DiDTargetingHelper` takes an `SCR_Faction`
parameter throughout, the zone caches both factions at `AFM_DiDZoneComponent.c:193-194`, and the HUD
resolves flags from the faction at `AFM_ScoreInfoDisplay.c:58-62`.

### Layers cannot be selected at runtime

Layers and sub-scenes are Workbench-only: everything that manipulates them lives in
`Core/generated/WorkbenchAPI/WorldEditorAPI.c`, and `IEntitySource.GetLayerID()` is an editor-side
source interface. Every `.layer` in a world's `_Layers` folder is merged when the world loads, so a
config field naming a layer would do nothing.

Worlds, however, **inherit**. Every DiD world is a thin wrapper over a vanilla terrain, e.g.
`Worlds/DiD_Linear_Lamentin.ent` in full:

```
SubScene { Parent "{853E92315D1D9EFE}worlds/Eden/Eden.ent" }
```

So the per-faction unit is a *world*, not a layer (see "Player side").

### The player side is 90% free, and the last 10% is a prefab list

PS binds faction to the playable's prefab and gives no way around it:
`PS_PlayableComponent.GetFactionKey()` reads `GetDefaultFactionKey()`, the engine exposes no setter for
a default affiliation, and `SCR_AIGroup.SetFaction` refuses outright on playable groups. PS also has no
slot-generation path - the lobby is built from real entities standing in the world.

What follows the faction for free, with no work at all: lobby faction name / flag / colour, group
names, VoN rooms, faction balance, spectator buttons, respawn prefab (DiD calls
`GetNextRespawn(false)`, i.e. "same prefab you died as"), and the saved-loadout guard at
`AFM_GameModeDiD.c:527`, which is already keyed on `m_sDefenderFactionKey`.

What does not: the player slots themselves. `Players.layer` is the only definition of who the players
are - Lamentin is 16 slots from `Group_US_FireTeam_P` x3 plus one `Group_US_PlatoonHQ_P` whose four
slots are overridden in the layer. PS ships drop-in equivalents for the other vanilla factions
(`Group_USSR_*_P`, `Group_FIA_*_P`, 57 USSR and 20 FIA `Character_*_P` prefabs).

Note for future work: the PS source the mod compiles against is
`C:\Users\nielu\Projects\armaforces\ReforgerLobbyCommunity\PlayableSelector`. The copy under
`Documents\My Games\ArmaReforgerWorkbench\addons\PlayableSelector` is two years stale and lacks
`PS_PlayableContainer` and `PS_RespawnData`.

---

## Design

### A side-car config bundle, one `.conf` per side

`AFM_DiDSideConfig` as `[BaseContainerProps(configRoot: true)]`, the same shape Overthrow uses for
`OVT_Faction`: a per-side content bundle that is *not* a `SCR_Faction` subclass, held by the game mode
and resolved by faction key.

```enscript
[BaseContainerProps(configRoot: true), BaseContainerCustomStringTitleField("m_sFactionKey")]
class AFM_DiDSideConfig
{
    [Attribute("", UIWidgets.EditBox, "Faction key this side plays as")]
    FactionKey m_sFactionKey;

    // --- attackers
    [Attribute("", UIWidgets.ResourceAssignArray, "Infantry group prefabs", params: "et", category: "Infantry")]
    ref array<ResourceName> m_aInfantryGroups;

    [Attribute("", UIWidgets.Object, "Vehicles, each with the crew that mans it", category: "Mechanized")]
    ref array<ref AFM_DiDVehicleEntry> m_aMechanized;

    [Attribute("", UIWidgets.ResourceNamePicker, "Mortar composition", params: "et", category: "Mortars")]
    ResourceName m_sMortarComposition;
    [Attribute("", UIWidgets.Object, "Mortar crew", category: "Mortars")]
    ref AFM_CrewConfig m_MortarCrew;

    [Attribute("", UIWidgets.ResourceAssignArray, "Attack helicopters", params: "et", category: "Air")]
    ref array<ResourceName> m_aHelicopters;
    [Attribute("", UIWidgets.ResourceNamePicker, "Helicopter crew group", params: "et", category: "Air")]
    ResourceName m_sHelicopterCrewGroup;

    [Attribute("", UIWidgets.ResourceNamePicker, "Empty group the COWABUNGA squad joins", params: "et", category: "Cowabunga")]
    ResourceName m_sCowabungaGroup;
    [Attribute("", UIWidgets.ResourceAssignArray, "COWABUNGA squad characters, used in order", params: "et", category: "Cowabunga")]
    ref array<ResourceName> m_aCowabungaCharacters;

    // --- defenders
    [Attribute("", UIWidgets.ResourceNamePicker, "Extraction helicopter", params: "et", category: "Extraction")]
    ResourceName m_sExtractionHelicopter;
    [Attribute("", UIWidgets.ResourceNamePicker, "Extraction crew group", params: "et", category: "Extraction")]
    ResourceName m_sExtractionCrewGroup;
}
```

Plus `AFM_DiDVehicleEntry` (`[BaseContainerProps()]`: vehicle prefab + `ref AFM_CrewConfig`), because
vehicle-to-crew pairing is the one thing vanilla's catalogs cannot express - they are flat per-type
lists.

One class, two files: `Configs/Factions/DiD_Side_US.conf` and `DiD_Side_USSR.conf`. A side used as
defender fills the extraction fields and leaves the attacker ones empty, and vice versa; a file can
fill both and be usable on either side.

### How the game mode gets them

Two `ResourceName` attributes on `AFM_GameModeDiD`, loaded with the vanilla one-liner:

```enscript
[Attribute("{...}Configs/Factions/DiD_Side_US.conf", UIWidgets.ResourceNamePicker,
    "Defending side", params: "conf class=AFM_DiDSideConfig", category: "DiD")]
protected ResourceName m_sDefenderConfig;
...
m_DefenderConfig = SCR_ConfigHelperT<AFM_DiDSideConfig>.GetConfigObject(m_sDefenderConfig);
```

`params: "conf class=AFM_DiDSideConfig"` makes the Workbench picker list only matching configs.

Chosen deliberately over the alternative (a linked `ref` member in the game-mode prefab, Overthrow's
style): a `ResourceName` can be **overridden per scenario from the mission header**, which this mod
already has plumbing for. Two new fields on `modded class SCR_MissionHeader` next to
`m_AFM_DiDDefaultPhase`, captured by `AFM_DiDScenarioSettings` at `OnMissionSet`, and one game-mode
prefab serves every faction pairing:

```
world + Players layer ->  which side the players are
mission .conf         ->  which two side configs to load
server config         ->  same fields again, for a server-side swap
```

`m_sDefenderFactionKey` / `m_sAttackerFactionKey` then **come from the configs** (`m_sFactionKey`) and
the two attributes are deleted - one source of truth per side.

### How spawners read it

Each spawner keeps its existing `[Attribute]` array as an explicit per-spawner override and falls back
to the side config when it is empty:

```enscript
// AFM_DiDSpawnerComponent
protected array<ResourceName> ResolveGroupPrefabs()
{
    if (m_aAIGroupPrefabs && !m_aAIGroupPrefabs.IsEmpty())
        return m_aAIGroupPrefabs;          // this spawner deviates on purpose

    AFM_DiDSideConfig side = m_Zone.GetAttackerConfig();
    ...
}
```

This is what makes the migration safe: the config can land and be tested while the prefabs still hold
their USSR lists, and clearing those lists is a separate, revertible step.

### Arsenal boxes

`SCR_ArsenalComponent.GetAssignedFaction()` reads the box's own `SCR_FactionAffiliationComponent`, and
`OnFactionChanged` calls `RefreshArsenal()`, which broadcasts to clients. So the game mode can
re-faction the placed boxes at init from the defender config and they restock themselves - no layer
edit. Only the crate's model stays visually American, which is cosmetic.

---

## Player side

Per-faction **world wrappers**, since layers cannot be switched:

```
DiD_Linear_Lamentin.ent          SubScene of Eden          zones, props - everything shared
DiD_Linear_Lamentin_US.ent       SubScene of the above  +  Players_US
DiD_Linear_Lamentin_USSR.ent     SubScene of the above  +  Players_USSR
```

The mission `.conf` picks the world and the two side configs together, which is how scenarios already
vary (`DiD_Lamentin_Tuned.conf`). Authoring cost per faction per world is the dozen prefab references
that are in `Players.layer` today.

**Confirmed working.** Inheriting a DiD world from another DiD world was tested in the editor
(2026-09-28) - none of the six worlds did it before, all inheriting vanilla terrains, so it was worth
proving. The shared content comes through and a child world can add its own player groups.

The alternative this avoids, recorded in case it is ever needed: runtime substitution, despawning the
placed playable groups at world init and spawning the config's equivalents at the same transforms
before PS finishes registering them. `AFM_DiDCowabungaComponent` proves every piece (spawn group, spawn
characters, `SetPlayable(true)`, `SwitchPlayerToPlayable`), but PS offers no hook and the work would
have to land inside `PS_PlayableComponent.LateInit -> RegisterPlayable` - EOnInit plus a couple of
callqueue frames. Not needed now.

---

## Phases

**Phase 0 - the world-inheritance test.** Done: it works, so the player side is per-faction world
wrappers.

**Phase 1 - the config, read by the AI side.** New `AFM_DiDSideConfig`, `AFM_DiDVehicleEntry`, two
`.conf` files carrying today's US and USSR content, the two game-mode attributes and loader, getters on
the game mode and pass-throughs on the zone. Spawners resolve config-then-override for: infantry
groups, mechanized vehicles + crew, mortar composition + crew, helicopters + crew, COWABUNGA group +
characters, extraction helicopter + crew. The four hardcoded script fallbacks are deleted.
Verification: mod builds, a match plays identically to today with the prefabs untouched.

**Phase 2 - clear the prefabs.** Empty the faction arrays in the six spawner prefabs so the config is
the only source. Verification: a match still plays identically; then point the attacker config at a
different side and watch the enemy change without touching a world.

**Phase 3 - faction keys from config.** Delete `m_sDefenderFactionKey` / `m_sAttackerFactionKey`, read
`m_sFactionKey` from the two configs, re-faction the arsenal boxes at init.

**Phase 4 - mission-header override.** Two fields on the modded header, captured alongside the phase
settings, so a scenario and a server config can swap sides. Sample config updated.

**Phase 5 - the player side.** Per Phase 0: either split each world into a shared base plus per-faction
wrappers, or build the runtime substitution.

---

## Deliberately not doing

**Vanilla entity catalogs** (`SCR_Faction.m_aEntityCatalogs`, `SCR_EntityCatalog` as `.conf`). They are
the idiomatic answer when you need vanilla interop - GM content browser, arsenal stocking, vehicle
request spawners, Scenario Framework randomisation - and DiD needs none of it: its spawners take plain
`ResourceName`s. The costs are real: every prefab must be an editable entity with correct
`EEditableEntityLabel`s (a closed vanilla enum with no slot for a new faction), only one catalog per
type per faction survives `Init`, `m_aEntityCatalogs` is nulled afterwards, reading before
`SCR_Faction.Init` hard-errors, and catalogs cannot express vehicle-to-crew pairing anyway. If GM build
mode ever needs a side's content, mirror that subset into catalogs then, as a second source.

**`SCR_LoadoutManager` / `SCR_FactionPlayerLoadout`.** This is the vanilla way to make loadouts appear
in the deploy menu, and it does not apply: PS never enters that pipeline, the game mode prefab disables
`SCR_RespawnSystemComponent`, and a playable's loadout *is* its `_P` character prefab. Adding it would
mean maintaining a replicated, index-ordered array that nothing in DiD reads.

**Wave zones.** Out of scope, as in the code review.

---

## Risks and things to get right

1. **Load on both server and client.** The configs are files, identical everywhere, so both sides can
   load them - and the HUD needs the faction keys on clients. Do not gate the load on
   `Replication.IsServer()`. Only a *runtime* choice of config would need replicating; ours comes from
   the mission header, which every machine reads.
2. **`SCR_Global.IsEditMode()` early-outs** on anything touching factions at init, as vanilla and
   Overthrow both do, or the Workbench chokes.
3. **GUID hygiene.** Create the `.conf` files in Workbench; never copy an existing one, which
   duplicates its GUID. ACE and Overthrow already collide this way on `FIA_InventoryItems.conf`.
4. **`AFM_CrewConfig` reuse.** It is already faction-clean (empty prefab attributes, falls back to the
   vehicle's own default occupant) and is `[BaseContainerProps(configRoot: true)]`, so it nests into the
   side config as-is.
5. **Empty-config behaviour.** A side config with an empty list must log an error naming the field and
   the side rather than silently spawning nothing - the current spawners already warn on empty arrays,
   and that path is what a mis-authored config will hit.
6. **The DiD_BritishForces and DiD_Zombie addons** re-skin factions by authoring their own worlds and
   prefabs on top of this addon. Once side configs exist they should become side configs instead; until
   then, do not break their references to `Prefabs/Groups/OPFOR/KLMK/Group_USSR_SniperTeam_KLMK.et` or
   `AFM_SupplyCache_Large.et`.

## Incidental fixes to fold in

- `Prefabs/MP/AFM_DiDZonePrefab.et:5` authors `m_aAIGroupPrefabs` with two USSR groups inside the
  `AFM_DiDZoneComponent` block - a component with no such member. Dead value carried by four zone
  instances. Delete.
- `AFM_DiDExtractionComponent.c:60` - the extraction's `DEFAULT_HELICOPTER_PREFAB` fallback is a USSR
  Mi-8. Never hit today because `AFM_ExtractionSpawner.et:9` sets the UH-1H, but any new extraction
  spawner left empty gets a Soviet gunship flown by a US crew. Phase 1 deletes the constant.
- `Prefabs/Characters/REAPER_USSR_Pilot.et` is referenced nowhere. Delete or wire it up.
- `AFM_DiDZoneSystem.c:34` caches an `SCR_FactionManager` that is never read. Delete.

---

## Where to start

Phase 1, which touches no content: the config classes, the two `.conf` files carrying today's US and
USSR values, and the spawners resolving config-then-override. A match should play identically with the
six spawner prefabs untouched, which is the check that the wiring is right before Phase 2 takes their
values away.
