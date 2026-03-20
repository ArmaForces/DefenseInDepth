//------------------------------------------------------------------------------------------------
//! Mechanized spawner - spawns vehicle groups with different timing and logic
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

	protected ref array<IEntity> m_aSpawnedVehicles = {};

	//------------------------------------------------------------------------------------------------
	override void Prepare(AFM_DiDZoneComponent owner)
	{
		super.Prepare(owner);

		m_iWaveIntervalSeconds = Math.Ceil(m_iWaveIntervalSeconds * m_fDelayMultiplier);

		PrintFormat("AFM_DiDMechanizedSpawnerComponent: Mechanized spawner initialized with %1s interval",
			m_iWaveIntervalSeconds, LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	override protected void SpawnWave()
	{
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

		// Early exit if budget can't cover at least one vehicle
		if (!CanSpendBudget(m_iPointCostPerUnit))
		{
			PrintFormat("AFM_DiDMechanizedSpawnerComponent: Budget exhausted, skipping spawn", LogLevel.DEBUG);
			return;
		}

		int spawnCount = GetSpawnCountForWave();
		PrintFormat("AFM_DiDMechanizedSpawnerComponent: Spawning %1 vehicle(s)", spawnCount, LogLevel.DEBUG);

		for (int i = 0; i < spawnCount; i++)
		{
			if (m_Zone.GetActiveAICount() >= m_iMaxAICount)
				break;

			SpawnSingleGroup();
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void Cleanup()
	{
		super.Cleanup();
		foreach (IEntity entity : m_aSpawnedVehicles)
		{
			if (!entity)
				continue;
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected int GetSpawnCountForWave()
	{
		if (!m_Zone)
			return 1;

		int zoneIndex = m_Zone.GetZoneIndex();

		if (m_bCoordinatedSpawn && zoneIndex >= 3)
			return s_AIRandomGenerator.RandInt(2, 3);

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! m_iPointCostPerUnit represents cost per whole vehicle group (vehicle + crew)
	//------------------------------------------------------------------------------------------------
	override protected void SpawnSingleGroup()
	{
		if (m_aSpawnPoints.Count() == 0 || m_aAIWaypoints.Count() == 0 || m_aVehiclePrefabs.Count() == 0)
			return;

		if (!m_crewConfig)
			return;

		// Budget check — cost is per vehicle, deducted immediately on successful spawn
		if (!CanSpendBudget(m_iPointCostPerUnit))
			return;

		IEntity vehicle = SpawnPrefab(m_aVehiclePrefabs.GetRandomElement(), m_aSpawnPoints.GetRandomElement());
		if (!vehicle)
			return;

		m_aSpawnedVehicles.Insert(vehicle);

		SCR_BaseCompartmentManagerComponent cm = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent)
		);
		if (!cm)
			return;

		AIGroup crew = m_crewConfig.SpawnCrew(cm, m_aAIWaypoints.GetRandomElement());

		// Consume budget immediately — vehicle spawned, cost is committed
		if (crew && m_Zone && m_Zone.GetBudget())
			m_Zone.GetBudget().Consume(m_iPointCostPerUnit);
	}
}
