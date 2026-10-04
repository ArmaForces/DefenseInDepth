//------------------------------------------------------------------------------------------------
//! Times how long each player spends in build mode, and points building at the stage's supply pool.
//!
//! The provider is where the timing is knowable: it keeps the list of players currently building, and
//! adds and drops them through every path that matters - entering the mode, leaving it, dying,
//! teleporting, disconnecting. RemoveActiveUsers, which clears the whole list, goes through
//! RemoveActiveUser for each one, so timing the singular pair catches all of it.
//------------------------------------------------------------------------------------------------
modded class SCR_CampaignBuildingProviderComponent
{
	//------------------------------------------------------------------------------------------------
	//! Where the supplies for building come from.
	//!
	//! The provider keeps vanilla's own resource component - its consumer is the DEFAULT one the building
	//! budget looks up, and handing the budget the cache's component instead does not work, because a
	//! consumer authored for storage answers to a different identifier and the budget then finds nothing.
	//! What changes is which supplies that consumer can see: vanilla settles it by proximity, which suits
	//! a Conflict base and not a mission that moves from stage to stage, so the active stage's cache is
	//! registered with it directly.
	//!
	//! Null while the economy is off, which is how vanilla expresses an unlimited budget - so building
	//! stays free, exactly as it was before any of this existed.
	//------------------------------------------------------------------------------------------------
	override SCR_ResourceComponent GetResourceComponent()
	{
		if (!AFM_DiDSupplies.IsEnabled())
			return null;

		SCR_ResourceComponent own = super.GetResourceComponent();
		if (own && AFM_DiDSupplies.LinkSpender(own))
			return own;

		// The provider has no consumer the budget can use. The stage's cache carries one of its own, so the
		// budget can read it there instead.
		SCR_ResourceComponent cache = AFM_DiDSupplies.GetActiveCache();
		if (cache)
			return cache;

		return own;
	}

	//------------------------------------------------------------------------------------------------
	override void AddNewActiveUser(int userID)
	{
		super.AddNewActiveUser(userID);

		AFM_DiDStatsTracker stats = AFM_GetStatsTracker();
		if (stats)
			stats.BuildModeEntered(userID);
	}

	//------------------------------------------------------------------------------------------------
	override void RemoveActiveUser(int userID)
	{
		super.RemoveActiveUser(userID);

		AFM_DiDStatsTracker stats = AFM_GetStatsTracker();
		if (stats)
			stats.BuildModeLeft(userID);
	}

	//------------------------------------------------------------------------------------------------
	//! Null in any other game mode, and on clients, where there is no table to write to
	protected AFM_DiDStatsTracker AFM_GetStatsTracker()
	{
		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return null;

		return gameMode.GetStats();
	}
}
