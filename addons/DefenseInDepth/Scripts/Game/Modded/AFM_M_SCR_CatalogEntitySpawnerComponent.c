//------------------------------------------------------------------------------------------------
//! Lets a service point buy from the stage's supply pool, and hands what it spawns to the stage.
//!
//! The spawner charges through its own entity's resource component, and finds it by looking for a
//! Conflict military base within 150 m before falling back to the component on itself. DiD has no bases,
//! so it always uses its own - which knows nothing about the stage's cache until the pool is registered
//! with it. That is hooked on the two methods the spawn menu asks on the machine that is about to act:
//! the affordability check and the figure it displays.
//!
//! Whatever it spawns belongs to the stage that paid for it. A group left standing walks into the next
//! fight with no orders and an abandoned vehicle is free cover for the attackers, so the zone is told
//! about each one and clears them when the stage ends.
//------------------------------------------------------------------------------------------------
modded class SCR_CatalogEntitySpawnerComponent
{
	//------------------------------------------------------------------------------------------------
	protected override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		// Subscribed unconditionally: the game mode may not have switched the economy on yet when a
		// component initialises, and the handler checks for a stage rather than trusting a flag read here
		GetOnEntitySpawned().Insert(AFM_OnEntitySpawned);
	}

	//------------------------------------------------------------------------------------------------
	//! The invoker also carries the user, the faction and the spawner; only the entity matters here
	protected void AFM_OnEntitySpawned(IEntity spawned, IEntity user, SCR_Faction faction, SCR_CatalogEntitySpawnerComponent spawner)
	{
		AFM_DiDZoneSystem zoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!zoneSystem)
			return;

		AFM_DiDZoneComponent zone = zoneSystem.GetActiveZone();
		if (zone)
			zone.RegisterServiceSpawn(spawned);
	}

	//------------------------------------------------------------------------------------------------
	//! Prices the entries as they are collected.
	//!
	//! The catalog's own cost is always zero in this version - the field has no Attribute and nothing
	//! assigns it - so a price has to come from somewhere, and this is the one place every entry passes
	//! through. Stamped onto the entry data rather than charged separately, so the menu shows the figure and
	//! vanilla's own affordability check and charge work on it unchanged.
	//------------------------------------------------------------------------------------------------
	protected override void AddAssetsFromCatalog(notnull SCR_EntityCatalog entityCatalog, bool overwriteOld = false)
	{
		super.AddAssetsFromCatalog(entityCatalog, overwriteOld);

		AFM_DiDSupplyConfig config = AFM_DiDSupplies.GetConfig();
		if (!config || !m_aAssetList)
			return;

		foreach (SCR_EntityCatalogEntry entry : m_aAssetList)
		{
			AFM_PriceEntry(entry, config);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_PriceEntry(SCR_EntityCatalogEntry entry, notnull AFM_DiDSupplyConfig config)
	{
		if (!entry)
			return;

		SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		if (!data)
			return;

		SCR_EntityCatalog parent = entry.GetCatalogParent();
		if (!parent)
			return;

		int cost;
		switch (parent.GetCatalogType())
		{
			case EEntityCatalogType.CHARACTER:
				cost = config.m_iCostPerCharacter;
				break;

			case EEntityCatalogType.GROUP:
				// Entity count is how many the prefab brings, so a rifle squad costs more than a fire team
				cost = config.m_iCostPerGroupMember * Math.Max(1, data.GetEntityCount());
				break;

			case EEntityCatalogType.VEHICLE:
				cost = config.m_iCostPerVehicle;
				break;
		}

		if (cost > 0)
			data.AFM_SetSupplyCost(cost);
	}

	//------------------------------------------------------------------------------------------------
	//! Where the supplies actually move, and the one hook that has to be right: the figure players see is
	//! read on their own machine, the charge happens on the authority, and the pool has to be registered on
	//! whichever one is acting. Linking only where the menu asks left spawning free.
	override void AddSpawnerSupplies(float supplies)
	{
		if (AFM_DiDSupplies.LinkSpender(m_ResourceComponent))
		{
			super.AddSpawnerSupplies(supplies);
			return;
		}

		// No consumer to work through, so the stage is charged directly rather than letting it through free
		if (supplies < 0)
			AFM_DiDSupplies.Spend(Math.Round(-supplies));
		else
			AFM_DiDSupplies.Award(Math.Round(supplies));
	}

	//------------------------------------------------------------------------------------------------
	override SCR_EEntityRequestStatus GetRequestState(notnull SCR_EntityCatalogEntry entityEntry, IEntity user = null)
	{
		AFM_DiDSupplies.LinkSpender(m_ResourceComponent);

		return super.GetRequestState(entityEntry, user);
	}

	//------------------------------------------------------------------------------------------------
	override float GetSpawnerResourceValue()
	{
		AFM_DiDSupplies.LinkSpender(m_ResourceComponent);

		return super.GetSpawnerResourceValue();
	}
}
