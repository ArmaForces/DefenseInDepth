# Supplies economy – investigation and plan

Goal: use the vanilla supplies system as one currency for **loadouts**, **buildings** and **support**, so players trade the same pool between staying alive, digging in and calling things in.

## What we have today

| Piece | State |
| --- | --- |
| Supply cache (`AFM_SupplyCache_Large`) | `SCR_ResourceComponent`, starts at **100** of **2000**, consumer range 20 m, generator range 200 m, no decay |
| Building manager (game mode) | Budget type **CAMPAIGN** (supplies), composition refund **100%** |
| Building provider (`DiD_BulidingService_US`) | No `SCR_ResourceComponent` on the composition |
| Arsenal manager (game mode) | Military Supply Allocation **off** |
| Wave zone | Can award supplies on wave clear (`m_iSupplyReward`, off by default, and no world uses wave zones) |

**Consequence:** building is effectively free. The budget component resolves supplies through `SCR_CampaignBuildingProviderComponent.GetResourceComponent()`, which looks on the provider's own entity (`SCR_CampaignBuildingProviderComponent.c:1313-1318`). Ours has none, and a missing resource component makes the CAMPAIGN budget return -1, meaning unlimited (`SCR_CampaignBuildingBudgetEditorComponent.c:245-252`). The cache in the zone is decoration right now.

## What vanilla gives us

- **Buildings:** each composition carries its own CAMPAIGN budget cost, balanced for Conflict. Wire a resource component to the provider and those costs apply, including the refund percentage on dismantling.
- **Loadouts, two separate mechanisms:**
  - **Arsenal item cost:** every arsenal item has a supply cost (`SCR_ArsenalItem.m_iSupplyCost`), with alternative costs per situation, including a distinct `RESPAWN_COST` for what a player spawns with.
  - **Military Supply Allocation:** a per-player allowance sized by rank (`SCR_MilitarySupplyAllocationConfig`), replenished on a timer (600 s by default) and by supply-delivery XP. The game mode already points at a config; it's just disabled.
- **Support:** no vanilla feature, but spending is one call: `SCR_ResourceSystemHelper.ConsumeResources(resourceComponent, amount, failIfNotEnough)`, with `GetStoredResources` for display.

## Proposed design

### One pool per zone
The zone's supply cache is the team's budget for that stage. It pays for buildings, arsenal re-equips and support. What's left over when a zone falls carries over at a percentage, so holding well makes the next stage easier. That covers Seweryn's "earlier results should affect later stages".

### a) Loadouts
- **Spawning stays free.** Death is already punishing enough, and charging for respawns makes a losing stage unrecoverable.
- **The arsenal costs supplies** from the zone pool. Note that measured vanilla item costs are small next to buildings (a kit is 20–60, a bunker is 100), so this is a mild tax rather than a real choice unless the costs are raised.
- **Keep Military Supply Allocation off** for now. It's rank-gated, and the group voted 5–3 against rank-gated gear. Reconsider only if arsenal spam becomes a problem; it would then cap each player rather than the team.

### b) Buildings
- Give the building provider a resource component fed by the zone cache, so vanilla composition costs apply.
- **Drop the refund from 100% to 50%.** At 100% players can rearrange endlessly at no cost, and placement decisions carry no weight.
- AA emplacements already have vanilla prices (200 for the US M2HB, 250 for the USSR NSV), so nothing needs hand-pricing.
- Decide what to do with vanilla's rank requirements on bunkers, MG nests, AA, mortars and helipads.

### c) Support
New user actions at the building service, each consuming from the same pool:

| Support | Cost | Notes |
| --- | --- | --- |
| Ammo resupply crate | 150 | Keeps a long defence going |
| Light vehicle (UAZ/transport) | 200 | Mobility between positions |
| AI reinforcement squad | 250 | 4 AI defenders under player command |
| Artillery strike | 300 | One-off, a few rounds on a marked spot |

Costs are anchored to measured composition prices; see below.

3 of 4 people in the survey wanted support vehicles, and air strikes and artillery each got 2. Stalker suggested picking two support types per round, which this covers naturally through cost.

## Vanilla costs (measured)

Read out of the game paks, so these are the numbers the game will actually charge.

**Compositions** (128 have a supply cost; US/USSR equivalents match):

| Cost | Composition | Rank required |
| --- | --- | --- |
| 10 | Barbed wire, hedgehog, dragon's teeth | – |
| 15–35 | Sandbag long / round / high variants | – |
| 40–60 | Sandbag position (small), camo nets | – |
| 50–55 | Sandbag wall, solid wall | – |
| 80–85 | MG emplacement (M60/PKM), MG nest | Corporal |
| 100 | Bunker, large barricade, sandbag position 01 | Corporal (bunker) |
| 150 | Heavy MG emplacement (M2HB) | Corporal |
| 200–250 | **AA MG emplacement**, MG nest 02 | Corporal |
| 240 | Guard tower | – |
| 250–300 | Field hospital | Corporal |
| 325 | Ammo storage, player hub | Corporal / Sergeant |
| 400 | Mortar placement | Sergeant |
| 425–500 | Headquarters | – |
| 560–750 | Helipad | Sergeant |
| 175–3050 | Supply cache compositions | – |

**Arsenal items** (176 priced for US, range 0–250): M16 carbine 12, M72 launcher 30, M249 40, M60 50, radio backpack 50, ART II optic 75, M2 gun part 80, mortar parts 90 each (three needed), helicopter rocket pod 250, grenade 3. A normal infantry kit lands around 20–60 supplies.

**Vehicles and AI groups carry no vanilla supply price,** so support costs are entirely our choice.

### What this changed in the plan
- My earlier guesses were mostly too cheap. Real bunkers are 100 (I said 100), AA is 200 (I said 200), but field hospitals are 250–300 (I said 150) and ammo storage is 325 (I priced an ammo crate at 100).
- **Arsenal items are cheap compared to buildings.** Ten players re-kitting costs roughly 400, which is about one AA emplacement. Charging for the arsenal is therefore a minor tax, not a real trade-off; the pool is mostly about building and support.
- **Vanilla compositions already carry rank requirements**: bunkers, MG nests, AA and field hospitals need Corporal; mortars, player hubs and helipads need Sergeant. That's the rank-locked building menu you already have. It's worth a decision, because the group rejected rank-gated *gear* 5–3, and this is rank-gated *building*.

## Proposed budgets

Based on the measured costs, for about 10 players.

| Zone | Starting supplies | Carried over | Rationale |
| --- | --- | --- | --- |
| 1 | 1600 | – | 900 s prep, the main fortification stage |
| 2 | 1000 | + 50% of what's left | 600 s prep, players arrive with leftovers |
| 3 | 700 | + 50% of what's left | Last stand, 300 s defence, mostly fighting |

A plausible stage 1 spend, using real prices:

| Item | Cost | Count | Spend |
| --- | --- | --- | --- |
| Barbed wire / hedgehogs | 10 | 5 | 50 |
| Sandbag walls and positions | 30–55 | 6 | ~250 |
| MG nest | 85 | 2 | 170 |
| Bunker | 100 | 1 | 100 |
| AA emplacement | 200 | 1 | 200 |
| Arsenal re-kits | ~40 | 10 | ~400 |
| One support call | 150–300 | 1 | ~250 |

That totals about 1420 of 1600. Ammo storage (325) or a mortar placement (400) would mean giving up the AA gun or most of the fortifications, which is the kind of choice worth having.

Income during a stage:
- **+150 per repelled wave** in wave zones (existing reward, currently off).
- **+250 for downing a helicopter** (existing setting, currently 0). That's one AA emplacement paying for itself.
- No supply trucks: 4 of 6 asked to keep logistics simple.

Support costs, anchored to composition prices rather than my earlier guesses:

| Support | Cost | Anchor |
| --- | --- | --- |
| Ammo resupply crate | 150 | Half an ammo storage composition |
| Light vehicle | 200 | One AA emplacement |
| AI reinforcement squad | 250 | Field hospital |
| Artillery strike | 300 | Most of a stage's spare budget |

## Reward sources (paid in supplies)

Supplies are earned into the active zone's cache. Income arrives mostly during the fight, when building is hard, so it naturally pays for rebuilding and support calls rather than for the initial fortification.

| Event | Supplies | Where it hooks | Notes |
| --- | --- | --- | --- |
| Enemy infantry killed | 2 | `OnControllableDestroyed` on the game mode: victim is an attacker-faction character | The bread and butter. A busy stage kills 150–250 AI, so roughly 300–500 supplies |
| Enemy vehicle destroyed | 50 | Same hook, victim is a `Vehicle` of the attacker faction | About 6 vehicles a stage at most, so up to 300 |
| Helicopter shot down | 250 | Already implemented: `AFM_DiDHeliSpawnerComponent.m_iSupplyRewardOnKill`, currently 0 | One AA emplacement pays for itself |
| Enemy mortar destroyed | 150 | Mortar spawner, when its mortar or crew dies | Gives players a reason to hunt the mortar, which the survey asked for |
| Wave cleared | 150 | Already implemented: `AFM_DiDWaveZoneComponent.m_iSupplyReward`, currently off | Wave zones only |
| Zone held without ever being contested | 200 | Zone finishing in FINISHED_HELD having never frozen | Rewards keeping attackers out entirely |
| Holding the zone, per minute | 25 | Zone tick while ACTIVE and not FROZEN | Small drip that stops while attackers hold the zone, so pushing them out pays |
| COWABUNGA squad wiped out | 100 | Cowabunga component, on its "squad wiped out" end reason | Compensates defenders for an unfair fight |

### Anti-farming rules
- **Only count AI this zone spawned.** Spawners already track their groups, and vehicle crews are tracked since `6385b12`.
- **Cap kill income per stage** at about 600 supplies, so a long grind can't fund everything.
- **No income while the zone is contested**, matching the timer freeze. That makes the freeze hurt twice and rewards clearing the zone.
- **Ignore friendly fire and suicides.** The kill context reports the victim-killer relation, so these are easy to filter.

### Sanity check against the budgets
Stage 1 starts at 1600. A hard-fought stage adds roughly 300–500 from kills, up to 300 from vehicles, and 250 if a helicopter is downed, so about 2200–2600 total. Since most of it arrives mid-fight, it buys support calls, rebuilt sandbags and a replacement MG nest rather than a second AA gun before the attack starts.

### Implementation sketch
One component on the game mode, `AFM_DiDSupplyRewardsComponent`, with a value per event, awarding into the active zone's cache through `SCR_ResourceSystemHelper.ConsumeResources`'s counterpart (adding to the container like `AwardSupplyRewards` already does). It would:
- override `OnControllableDestroyed` for kills, deciding infantry vs vehicle from the victim entity;
- subscribe to the helicopter and mortar spawners for their own events;
- tick with the zone for the hold income.

Existing settings (`m_iSupplyRewardOnKill`, `m_iSupplyReward`) stay where they are, since they already live next to the thing that triggers them.

## Work involved

1. **Wire the cache to the provider:** a resource component on the building service composition, or move the provider onto the cache. Confirm the cache's 200 m generator range covers the whole defended area.
2. **Set the cache's starting value per zone** (2000 / 1200 / 800) and raise the maximum above the starting value.
3. **Carry-over:** on zone change, move a percentage of the old zone's remaining supplies into the new zone's cache. Small addition to the zone system.
4. **Refund:** set composition refund to 50%.
5. **Arsenal:** place arsenals in each zone with supply costs enabled, drawing from the cache.
6. **Support actions:** one component with a list of options, each with a cost, a cooldown and a spawn action. Reuse the spawner patterns we already have.
7. **Rewards:** the component above, plus turning on the two settings that already exist.
8. **HUD:** show the pool. The status line has room, and the supplies are already replicated.

## Open questions

- Should spawning cost supplies at all in later stages, as a soft respawn limit?
- Should the arsenal be free during prep and cost supplies only once the attack starts?
- Carry-over percentage: 50% rewards holding, 100% makes stage 1 dominate the whole match.
