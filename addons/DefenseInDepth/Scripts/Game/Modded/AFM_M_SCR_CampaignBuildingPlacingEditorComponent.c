//------------------------------------------------------------------------------------------------
//! Charges for everything players place, and hands the AI among it to the stage.
//!
//! One hook for the lot. Fortifications, soldiers, groups and vehicles all reach the world the same way -
//! as build menu entries, filtered by the provider's traits, placed through the editor - and this is where
//! that lands on the authority. Nothing comes from the service point's catalog spawner, which is why its
//! prices never applied to any of it.
//!
//! It is also the right moment rather than merely a convenient one: a ghost is charged for as soon as it is
//! placed, so the pool stops looking affordable before the shovelling starts, and a ghost that is deleted
//! again refunds exactly what it took.
//------------------------------------------------------------------------------------------------
modded class SCR_CampaignBuildingPlacingEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnEntityCreatedServer(array<SCR_EditableEntityComponent> entities)
	{
		super.OnEntityCreatedServer(entities);

		if (!entities || !AFM_DiDSupplies.IsEnabled())
			return;

		// Whoever has this editor open is the one spending
		int playerId = 0;
		SCR_EditorManagerEntity manager = GetManager();
		if (manager)
			playerId = manager.GetPlayerID();

		foreach (SCR_EditableEntityComponent editable : entities)
		{
			AFM_ChargeAndTrack(editable, playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_ChargeAndTrack(SCR_EditableEntityComponent editable, int playerId)
	{
		if (!editable)
			return;

		IEntity owner = editable.GetOwnerScripted();
		if (!owner)
			return;

		EEditableEntityType type = editable.GetEntityType();

		int cost = AFM_GetPlacementCost(editable, type);
		AFM_DiDSupplyBudget.Charge(cost);

		// For the results page. Deliberately counted at placement, so a ghost that is deleted again and the
		// supplies returned still leaves the commitment on the record - it was spent, then refunded.
		AFM_DiDStatsTracker stats = AFM_DiDSupplies.GetStats();
		if (stats)
			stats.OnSuppliesSpent(playerId, cost);

		// Bought AI belongs to the stage that paid for it. Vehicles are left alone, as they are elsewhere.
		if (type == EEditableEntityType.GROUP || type == EEditableEntityType.CHARACTER)
			AFM_TrackForZone(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! The entity's own CAMPAIGN budget if it carries one - which is how compositions are priced - and our
	//! configured price for its kind otherwise, since vehicle and group prefabs carry no supply price
	protected int AFM_GetPlacementCost(notnull SCR_EditableEntityComponent editable, EEditableEntityType type)
	{
		int budgetCost = AFM_DiDSupplyBudget.GetSupplyCost(editable.GetOwnerScripted());
		if (budgetCost > 0)
			return budgetCost;

		AFM_DiDSupplyConfig config = AFM_DiDSupplies.GetConfig();
		if (!config)
			return 0;

		switch (type)
		{
			case EEditableEntityType.CHARACTER:
				return config.m_iCostPerCharacter;

			case EEditableEntityType.GROUP:
				return config.m_iCostPerGroupMember * Math.Max(1, AFM_CountGroupMembers(editable.GetOwnerScripted()));

			case EEditableEntityType.VEHICLE:
				return config.m_iCostPerVehicle;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected int AFM_CountGroupMembers(IEntity owner)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(owner);
		if (!group)
			return 1;

		// Members trickle in over several frames, so the prefab's own count is the honest figure at this
		// moment; it falls back to whatever has arrived already
		int planned = group.GetMaxMembers();
		if (planned > 0)
			return planned;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		return agents.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_TrackForZone(notnull IEntity unit)
	{
		AFM_DiDZoneSystem zoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!zoneSystem)
			return;

		AFM_DiDZoneComponent zone = zoneSystem.GetActiveZone();
		if (zone)
			zone.RegisterBoughtUnit(unit);
	}
}
