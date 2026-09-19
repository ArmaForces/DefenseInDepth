//------------------------------------------------------------------------------------------------
//! Mechanized spawner - spawns vehicle groups with different timing and logic
//! Example of a more advanced spawner implementation
//------------------------------------------------------------------------------------------------
class AFM_DiDMechanizedSpawnerComponentClass: AFM_DiDSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDMechanizedSpawnerComponent: AFM_DiDSpawnerComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Spawn vehicles only when minimum AI threshold reached", category: "DiD Mechanized Spawner")]
	protected bool m_bRequireMinAI;
	
	[Attribute("10", UIWidgets.EditBox, "Minimum AI count before spawning mechanized groups", category: "DiD Mechanized Spawner")]
	protected int m_iMinAIThreshold;
	
	[Attribute("2", UIWidgets.EditBox, "Delay multiplier for mechanized spawns (slower than infantry)", category: "DiD Mechanized Spawner")]
	protected float m_fDelayMultiplier;
	
	[Attribute("1", UIWidgets.CheckBox, "Spawn in coordinated groups", category: "DiD Mechanized Spawner")]
	protected bool m_bCoordinatedSpawn;
	
	[Attribute("", UIWidgets.Object, desc: "Defines the vehicles crew - you may drag existing configs into here.", category: "DiD Mechanized Spawner")]
	protected ref AFM_CrewConfig m_crewConfig;
	
	[Attribute("", UIWidgets.Auto, desc: "Vehicle prefabs to spawn", category: "DiD Mechanized Spawner")]
	protected ref array<ResourceName> m_aVehiclePrefabs;

	[Attribute("2", UIWidgets.EditBox, "Maximum vehicles from this spawner alive at the same time", category: "DiD Mechanized Spawner")]
	protected int m_iMaxActiveVehicles;

	protected ref array<IEntity> m_aSpawnedVehicles = {};
	
	//------------------------------------------------------------------------------------------------
	override void Prepare(AFM_DiDZoneComponent owner)
	{
		super.Prepare(owner);
		
		// Mechanized units spawn less frequently
		m_iWaveIntervalSeconds = Math.Ceil(m_iWaveIntervalSeconds * m_fDelayMultiplier);
		
		PrintFormat("AFM_DiDMechanizedSpawnerComponent: Mechanized spawner initialized with %1s interval", 
			m_iWaveIntervalSeconds, LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	override protected void SpawnWave()
	{
		// Check if we should wait for minimum AI count
		if (m_bRequireMinAI)
		{
			int currentAI = m_Zone.GetActiveAICount();
			if (currentAI < m_iMinAIThreshold)
			{
				PrintFormat("AFM_DiDMechanizedSpawnerComponent: Waiting for minimum AI threshold (%1/%2)", 
					currentAI, m_iMinAIThreshold, LogLevel.DEBUG);
				return;
			}
		}
		
		int spawnCount = GetSpawnCountForWave();
		PrintFormat("AFM_DiDMechanizedSpawnerComponent: Spawning wave with %1 vehicles", spawnCount, level: LogLevel.DEBUG);

		for (int i = 0; i < spawnCount; i++)
		{
			if (IsAICapReached())
				break;

			if (CountActiveVehicles() >= m_iMaxActiveVehicles)
			{
				PrintFormat("AFM_DiDMechanizedSpawnerComponent: Vehicle limit reached (%1)", m_iMaxActiveVehicles, level: LogLevel.DEBUG);
				break;
			}

			SpawnSingleGroup();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Cleanup()
	{
		// Removes the crews, including those who got out of their vehicle
		super.Cleanup();

		foreach (IEntity entity : m_aSpawnedVehicles)
		{
			if (!entity)
				continue;
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		m_aSpawnedVehicles.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Vehicles from this spawner that still exist and aren't destroyed
	protected int CountActiveVehicles()
	{
		int count = 0;
		for (int i = m_aSpawnedVehicles.Count() - 1; i >= 0; i--)
		{
			Vehicle vehicle = Vehicle.Cast(m_aSpawnedVehicles[i]);
			if (!vehicle)
			{
				// Deleted: forget it. Wrecks stay listed so cleanup removes them.
				if (!m_aSpawnedVehicles[i])
					m_aSpawnedVehicles.Remove(i);
				continue;
			}

			SCR_DamageManagerComponent damageManager = vehicle.GetDamageManager();
			if (damageManager && damageManager.IsDestroyed())
				continue;

			count++;
		}

		return count;
	}
	
	
	//------------------------------------------------------------------------------------------------
	//! Mechanized groups spawn fewer units per wave but potentially with support
	//------------------------------------------------------------------------------------------------
	override protected int GetSpawnCountForWave()
	{
		if (!m_Zone)
			return 1;
		
		int zoneIndex = m_Zone.GetZoneIndex();
		
		// Mechanized spawns scale differently - fewer vehicles but more impactful
		if (m_bCoordinatedSpawn && zoneIndex >= 3)
		{
			// Spawn 2-3 vehicles in coordinated attack for higher zones
			return s_AIRandomGenerator.RandInt(2, 3);
		}
		
		// Single vehicle spawn for lower zones
		return 1;
	}
	
	
	//------------------------------------------------------------------------------------------------
	//! Override spawn logic for special mechanized behavior
	//------------------------------------------------------------------------------------------------
	override protected void SpawnSingleGroup()
	{
		if (m_aSpawnPoints.Count() == 0 || m_aAIWaypoints.Count() == 0 || m_aVehiclePrefabs.Count() == 0)
			return;
		
		if (!m_crewConfig)
			return; 
		
		IEntity vehicle = SpawnPrefab(m_aVehiclePrefabs.GetRandomElement(), m_aSpawnPoints.GetRandomElement());
		if (!vehicle)
			return;
		m_aSpawnedVehicles.Insert(vehicle);
		
		SCR_BaseCompartmentManagerComponent cm = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!cm)
			return;
		
		// Track the crew so it counts towards the AI cap and is removed on cleanup
		AIGroup crew = m_crewConfig.SpawnCrew(cm, m_aAIWaypoints.GetRandomElement());
		if (crew)
			TrackSpawnedGroup(crew);
	}
}
