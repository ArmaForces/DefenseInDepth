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
	// Set the moment this composition is paid for, so neither hook can charge for it twice
	protected bool m_bAFM_SuppliesCharged;

	//------------------------------------------------------------------------------------------------
	//! Takes the composition's supply cost from the stage's pool, once.
	//!
	//! Called when the ghost is placed rather than when it is finished: a player who has committed the
	//! supplies should see them gone, and the build menu should refuse the next piece it cannot afford.
	//! Charging on completion also made the refund an exploit - place ghosts for nothing, dismantle them
	//! for supplies.
	//------------------------------------------------------------------------------------------------
	void AFM_ChargeSuppliesOnce()
	{
		if (m_bAFM_SuppliesCharged || m_iBuilderId == INVALID_PLAYER_ID)
			return;

		m_bAFM_SuppliesCharged = true;
		AFM_DiDSupplyBudget.Charge(AFM_DiDSupplyBudget.GetSupplyCost(GetOwner()));
	}

	//------------------------------------------------------------------------------------------------
	override protected void SetIsCompositionSpawned()
	{
		super.SetIsCompositionSpawned();

		// Compositions authored into the world have no builder
		if (m_iBuilderId == INVALID_PLAYER_ID)
			return;

		// Normally already paid for when the ghost was placed. This catches a composition that reached the
		// world by some other path, and cannot charge twice.
		AFM_ChargeSuppliesOnce();

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
