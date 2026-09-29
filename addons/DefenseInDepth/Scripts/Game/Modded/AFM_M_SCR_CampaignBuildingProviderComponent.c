//------------------------------------------------------------------------------------------------
//! Times how long each player spends in build mode, for the match stats.
//!
//! The provider is where that is knowable: it keeps the list of players currently building, and adds
//! and drops them through every path that matters - entering the mode, leaving it, dying, teleporting,
//! disconnecting. RemoveActiveUsers, which clears the whole list, goes through RemoveActiveUser for
//! each one, so timing the singular pair catches all of it.
//------------------------------------------------------------------------------------------------
modded class SCR_CampaignBuildingProviderComponent
{
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
