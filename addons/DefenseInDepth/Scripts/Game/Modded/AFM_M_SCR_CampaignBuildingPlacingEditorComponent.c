//------------------------------------------------------------------------------------------------
//! Charges for a composition when its ghost is placed, not when it is finished.
//!
//! SetProviderAndBuilder is the first moment on the authority where a composition is known to be a
//! player's: it runs from OnEntityCreatedServer and is what stamps the builder onto it. Everything earlier
//! - the entity core's budget event - fires while the composition is still being created, when a player's
//! sandbags and the fortifications a mission ships with are indistinguishable.
//!
//! Charging here rather than on completion matters for two reasons players notice. A ghost that costs
//! nothing lets the pool look affordable until the shovelling starts, and a refund on a never-built ghost
//! is free supplies.
//------------------------------------------------------------------------------------------------
modded class SCR_CampaignBuildingPlacingEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void SetProviderAndBuilder(notnull SCR_CampaignBuildingCompositionComponent compositionComponent)
	{
		super.SetProviderAndBuilder(compositionComponent);

		compositionComponent.AFM_ChargeSuppliesOnce();
	}

	//------------------------------------------------------------------------------------------------
	//! Charges for soldiers, groups and vehicles, and hands them to the stage.
	//!
	//! These do not come from the service point's catalog spawner at all - they are build menu entries,
	//! filtered by the provider's own traits, and they arrive here like any other placement. So the catalog
	//! spawner's prices and hooks never see them, which is why they were free and why nothing cleared them
	//! at the end of a stage.
	//!
	//! Compositions are skipped: SetProviderAndBuilder above already charges those, and charging here too
	//! would bill them twice.
	//------------------------------------------------------------------------------------------------
	override protected void OnEntityCreatedServer(array<SCR_EditableEntityComponent> entities)
	{
		super.OnEntityCreatedServer(entities);

		if (!entities || !AFM_DiDSupplies.IsEnabled())
			return;

		foreach (SCR_EditableEntityComponent editable : entities)
		{
			AFM_ChargeAndTrack(editable);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_ChargeAndTrack(SCR_EditableEntityComponent editable)
	{
		if (!editable)
			return;

		IEntity owner = editable.GetOwnerScripted();
		if (!owner || owner.FindComponent(SCR_CampaignBuildingCompositionComponent))
			return;

		EEditableEntityType type = editable.GetEntityType();

		AFM_DiDSupplyBudget.Charge(AFM_GetPlacementCost(editable, type));

		// Bought AI belongs to the stage that paid for it. Vehicles are left alone, as they are elsewhere.
		if (type == EEditableEntityType.GROUP || type == EEditableEntityType.CHARACTER)
			AFM_TrackForZone(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! The entity's own CAMPAIGN budget if it carries one, and our configured price for its kind otherwise -
	//! vehicle and group prefabs are not priced in supplies by the game
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
	protected void AFM_TrackForZone(notnull IEntity spawned)
	{
		AFM_DiDZoneSystem zoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!zoneSystem)
			return;

		AFM_DiDZoneComponent zone = zoneSystem.GetActiveZone();
		if (zone)
			zone.RegisterServiceSpawn(spawned);
	}
}
