//------------------------------------------------------------------------------------------------
//! Registers player-built compositions with the zone that was active when they were placed, so the
//! zone can remove them when it ends.
//!
//! Vanilla keeps no usable list. SCR_CampaignBuildingManagerComponent.RegisterComposition only records
//! compositions that sit inside a SCR_CampaignMilitaryBaseComponent radius, which this game mode has
//! none of, so its map stays empty.
//!
//! SetIsCompositionSpawned is the right hook. It runs on the authority for both spawn paths - directly
//! from EOnInit for a composition with no link component, and through the linked-entities-spawned
//! invoker for one built gradually - and by then SCR_CampaignBuildingPlacingEditorComponent has set the
//! builder id, which is what separates a player's work from the mission's own.
//------------------------------------------------------------------------------------------------
modded class SCR_CampaignBuildingCompositionComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void SetIsCompositionSpawned()
	{
		super.SetIsCompositionSpawned();

		// Compositions authored into the world have no builder
		if (m_iBuilderId == INVALID_PLAYER_ID)
			return;

		// The supply cost, taken here rather than on the budget event: that event fires while the
		// composition is still being created, before anyone has set a builder on it, so it cannot tell a
		// player's sandbags from the mission's own
		AFM_DiDSupplyBudget.Charge(AFM_DiDSupplyBudget.GetSupplyCost(GetOwner()));

		AFM_DiDZoneSystem zoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!zoneSystem)
			return;

		AFM_DiDZoneComponent zone = zoneSystem.GetActiveZone();
		if (!zone)
			return;

		zone.RegisterPlayerStructure(GetOwner());

		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return;

		AFM_DiDStatsTracker stats = gameMode.GetStats();
		if (stats)
			stats.OnStructureBuilt(m_iBuilderId);
	}
}
