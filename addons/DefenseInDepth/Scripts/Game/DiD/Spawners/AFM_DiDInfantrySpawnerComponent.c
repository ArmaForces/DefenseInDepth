//------------------------------------------------------------------------------------------------
//! Infantry spawner - spawns regular infantry groups
//! This is a concrete implementation showing how to extend the base spawner
//------------------------------------------------------------------------------------------------
class AFM_DiDInfantrySpawnerComponentClass: AFM_DiDSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDInfantrySpawnerComponent: AFM_DiDSpawnerComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Enable random spawn point selection", category: "DiD Infantry Spawner")]
	protected bool m_bRandomSpawnPoints;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable random waypoint assignment", category: "DiD Infantry Spawner")]
	protected bool m_bRandomWaypoints;
	
	[Attribute("0.8", UIWidgets.EditBox, "Min spawn interval multiplier for variety", category: "DiD Infantry Spawner")]
	protected float m_fMinIntervalMultiplier;
	
	[Attribute("1.2", UIWidgets.EditBox, "Max spawn interval multiplier for variety", category: "DiD Infantry Spawner")]
	protected float m_fMaxIntervalMultiplier;
	
	protected int m_iCurrentSpawnPointIndex = 0;
	protected int m_iCurrentWaypointIndex = 0;
	
	//------------------------------------------------------------------------------------------------
	override void Prepare(AFM_DiDZoneComponent owner)
	{
		super.Prepare(owner);
		PrintFormat("AFM_DiDInfantrySpawnerComponent: Infantry spawner initialized with %1 spawn points and %2 waypoints", 
			m_aSpawnPoints.Count(), m_aAIWaypoints.Count(), LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Override to add variety to spawn intervals
	//------------------------------------------------------------------------------------------------
	override void Process()
	{
		if (!m_Zone)
			return;
		
		EAFMZoneState state = m_Zone.GetZoneState();
		if (state != EAFMZoneState.ACTIVE && state != EAFMZoneState.FROZEN)
			return;
		
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		
		int timeSinceLastSpawn = Math.AbsInt(now.DiffSeconds(m_fLastSpawnTime));
		
		// Add variety to spawn intervals
		float intervalMultiplier = s_AIRandomGenerator.RandFloatXY(m_fMinIntervalMultiplier, m_fMaxIntervalMultiplier);
		int adjustedInterval = Math.Ceil(m_iWaveIntervalSeconds * intervalMultiplier);
		
		if (timeSinceLastSpawn >= adjustedInterval)
		{
			m_fLastSpawnTime = now;
			SpawnWave();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Infantry-specific spawn logic with optional sequential spawning
	//------------------------------------------------------------------------------------------------
	override protected void SpawnSingleGroup()
	{
		if (m_aSpawnPoints.Count() == 0 || m_aAIWaypoints.Count() == 0 || m_aAIGroupPrefabs.Count() == 0)
			return;
		
		ResourceName groupPrefab = m_aAIGroupPrefabs.GetRandomElement();
		
		// Get spawn point (random or sequential)
		AFM_SpawnPointEntity spawnPoint;
		if (m_bRandomSpawnPoints)
		{
			spawnPoint = m_aSpawnPoints.GetRandomElement();
		}
		else
		{
			spawnPoint = m_aSpawnPoints[m_iCurrentSpawnPointIndex];
			m_iCurrentSpawnPointIndex = (m_iCurrentSpawnPointIndex + 1) % m_aSpawnPoints.Count();
		}
		
		// Get waypoint (random or sequential)
		SCR_AIWaypoint waypoint;
		if (m_bRandomWaypoints)
		{
			waypoint = m_aAIWaypoints.GetRandomElement();
		}
		else
		{
			waypoint = m_aAIWaypoints[m_iCurrentWaypointIndex];
			m_iCurrentWaypointIndex = (m_iCurrentWaypointIndex + 1) % m_aAIWaypoints.Count();
		}
		
		AIGroup group = SpawnAI(groupPrefab, spawnPoint, waypoint);
		if (group)
		{
			m_aSpawnedAIGroups.Insert(group);
			PrintFormat("AFM_DiDInfantrySpawnerComponent: Spawned infantry group %1", groupPrefab, LogLevel.DEBUG);
		}
	}
}
