//------------------------------------------------------------------------------------------------
//! Simplified crew config for spawning AI drivers and gunners in vehicles
//! Credits to Til Weimann and his TilW mission framework: https://github.com/Til-Weimann/tilw-mission-framework
//------------------------------------------------------------------------------------------------

[BaseContainerProps(configRoot: true), BaseContainerCustomStringTitleField("Crew Config")]
class AFM_CrewConfig
{
	[Attribute("", UIWidgets.ResourceAssignArray, desc: "Driver character prefab (if empty, uses vehicle default)", params: "et")]
	protected ResourceName m_sDriverPrefab;
	
	[Attribute("", UIWidgets.ResourceAssignArray, desc: "Gunner character prefab (if empty, uses vehicle default)", params: "et")]
	protected ResourceName m_sGunnerPrefab;
	
	[Attribute("1", UIWidgets.CheckBox, desc: "Spawn driver")]
	protected bool m_bSpawnDriver;
	
	[Attribute("1", UIWidgets.CheckBox, desc: "Spawn gunner")]
	protected bool m_bSpawnGunner;
	
	[Attribute("1", UIWidgets.CheckBox, desc: "Prevent gunner from dismounting turret")]
	protected bool m_bNoTurretDismount;
	
	//------------------------------------------------------------------------------------------------
	//! Main method to spawn crew in a vehicle and assign waypoint
	//! @param cm Compartment manager of the vehicle
	//! @param assignedWaypoint Waypoint to assign to the AI group
	//! @return The spawned AIGroup, or null if spawning failed
	//------------------------------------------------------------------------------------------------
	AIGroup SpawnCrew(SCR_BaseCompartmentManagerComponent cm, AIWaypoint assignedWaypoint)
	{
		if (!cm)
		{
			Print("AFM_CrewConfig: Invalid compartment manager!", LogLevel.ERROR);
			return null;
		}
		
		// Get all compartment slots
		array<BaseCompartmentSlot> compartmentSlots = {};
		cm.GetCompartments(compartmentSlots);
		
		if (compartmentSlots.Count() == 0)
		{
			Print("AFM_CrewConfig: No compartment slots found!", LogLevel.WARNING);
			return null;
		}
		
		// Find driver and gunner slots
		BaseCompartmentSlot driverSlot = FindSlot(compartmentSlots, ECompartmentType.PILOT);
		BaseCompartmentSlot gunnerSlot = FindSlot(compartmentSlots, ECompartmentType.TURRET);
		
		// Create AI group
		AIGroup aiGroup = CreateAIGroup();
		if (!aiGroup)
		{
			Print("AFM_CrewConfig: Failed to create AI group!", LogLevel.ERROR);
			return null;
		}
		
		// Spawn driver
		if (m_bSpawnDriver && driverSlot)
		{
			IEntity driver = SpawnCharacterInSlot(driverSlot, m_sDriverPrefab, aiGroup);
			if (driver)
				PrintFormat("AFM_CrewConfig: Spawned driver in %1", driverSlot.GetCompartmentName(), LogLevel.DEBUG);
		}
		
		// Spawn gunner
		if (m_bSpawnGunner && gunnerSlot)
		{
			IEntity gunner = SpawnCharacterInSlot(gunnerSlot, m_sGunnerPrefab, aiGroup);
			if (gunner)
			{
				PrintFormat("AFM_CrewConfig: Spawned gunner in %1", gunnerSlot.GetCompartmentName(), LogLevel.DEBUG);
				
				// Prevent dismount if configured
				if (m_bNoTurretDismount)
				{
					SCR_AICombatComponent combatComp = SCR_AICombatComponent.Cast(gunner.FindComponent(SCR_AICombatComponent));
					if (combatComp)
						combatComp.SetNeverDismountTurret(true);
				}
			}
		}
		
		// Assign waypoint to group
		if (assignedWaypoint)
		{
			aiGroup.AddWaypoint(assignedWaypoint);
			PrintFormat("AFM_CrewConfig: Assigned waypoint to AI group", LogLevel.DEBUG);
		}
		
		return aiGroup;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Find first available slot of specified type
	//------------------------------------------------------------------------------------------------
	protected BaseCompartmentSlot FindSlot(array<BaseCompartmentSlot> slots, ECompartmentType type)
	{
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;
				
			if (slot.GetType() == type && !slot.IsOccupied() && slot.IsCompartmentAccessible())
			{
				// Check if slot has default character or we have custom prefab
				if (!slot.GetDefaultOccupantPrefab().IsEmpty() || 
					(type == ECompartmentType.PILOT && m_sDriverPrefab != "") ||
					(type == ECompartmentType.TURRET && m_sGunnerPrefab != ""))
				{
					return slot;
				}
			}
		}
		
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Spawn character in compartment slot
	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnCharacterInSlot(BaseCompartmentSlot slot, ResourceName characterPrefab, AIGroup group)
	{
		if (!slot)
			return null;
		
		IEntity character;
		
		// Use custom prefab if provided, otherwise use default
		if (characterPrefab != "")
			character = slot.SpawnCharacterInCompartment(characterPrefab, group);
		else
			character = slot.SpawnDefaultCharacterInCompartment(group);
		
		return character;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Create new AI group
	//------------------------------------------------------------------------------------------------
	protected AIGroup CreateAIGroup()
	{
		Resource groupResource = Resource.Load("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et");
		if (!groupResource || !groupResource.IsValid())
		{
			Print("AFM_CrewConfig: Failed to load AI group resource!", LogLevel.ERROR);
			return null;
		}
		
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		IEntity groupEntity = GetGame().SpawnEntityPrefab(groupResource, GetGame().GetWorld(), spawnParams);
		if (!groupEntity)
		{
			Print("AFM_CrewConfig: Failed to spawn AI group entity!", LogLevel.ERROR);
			return null;
		}
		
		return AIGroup.Cast(groupEntity);
	}
}