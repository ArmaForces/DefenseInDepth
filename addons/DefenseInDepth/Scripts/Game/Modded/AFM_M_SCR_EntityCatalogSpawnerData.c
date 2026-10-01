//------------------------------------------------------------------------------------------------
//! Lets a catalog entry be given a price.
//!
//! The catalog carries a supply cost field, but in this version of the game it has no Attribute on it and
//! nothing in the whole script base ever assigns it - which is why vanilla's own catalog configs log
//! "Unknown keyword/data 'm_iSupplyCost'" and why GetSupplyCost() answers 0 for every entry. Spawning a
//! squad or a vehicle is therefore free, whatever the spawner is told to charge.
//!
//! So the price is stamped on from our own config instead. Doing it here rather than charging separately
//! keeps vanilla in charge of the rest: the menu shows the figure, the affordability check uses it, and the
//! spawner takes the supplies through the consumer it already has.
//------------------------------------------------------------------------------------------------
modded class SCR_EntityCatalogSpawnerData
{
	//------------------------------------------------------------------------------------------------
	void AFM_SetSupplyCost(int cost)
	{
		m_iSupplyCost = cost;
	}
}
