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

		AFM_DiDZoneSystem zoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!zoneSystem)
			return;

		AFM_DiDZoneComponent zone = zoneSystem.GetActiveZone();
		if (!zone)
			return;

		zone.RegisterPlayerStructure(GetOwner());
	}
}
