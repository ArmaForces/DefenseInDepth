//------------------------------------------------------------------------------------------------
//! Lets a spawner buy from the stage's supply pool.
//!
//! The spawner charges through its own entity's resource component, and finds it by looking for a
//! Conflict military base within 150 m before falling back to the component on itself. DiD has no bases,
//! so it always uses its own - which knows nothing about the stage's cache until the pool is registered
//! with it.
//!
//! Hooked on the two methods the spawn menu asks on the machine that is about to act: the affordability
//! check and the figure it displays. That covers a client deciding what it can afford and the authority
//! taking the supplies, without linking anything on machines that never open the menu.
//------------------------------------------------------------------------------------------------
modded class SCR_CatalogEntitySpawnerComponent
{
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
