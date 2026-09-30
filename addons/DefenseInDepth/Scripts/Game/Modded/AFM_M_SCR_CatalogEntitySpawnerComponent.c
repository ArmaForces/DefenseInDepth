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

		if (!AFM_DiDSupplies.IsEnabled())
			return;

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
