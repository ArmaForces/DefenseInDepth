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
	//! Vanilla looks for a resource component on the provider itself and leaves the rest to proximity: a
	//! Conflict building service stands in a base next to its storage. Our stages each have their own
	//! cache, and which one is in play is a matter of which stage is running rather than which is nearest,
	//! so the active zone's cache is named outright.
	//!
	//! Null while the economy is off, which is how vanilla expresses an unlimited budget - so building
	//! stays free, exactly as it was before any of this existed.
	//------------------------------------------------------------------------------------------------
	override SCR_ResourceComponent GetResourceComponent()
	{
		if (!AFM_DiDSupplies.IsEnabled())
			return null;

		SCR_ResourceComponent cache = AFM_DiDSupplies.GetActiveCache();
		if (cache)
			return cache;

		// No stage running, or a stage with no cache placed: fall back to whatever vanilla would find
		return super.GetResourceComponent();
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
