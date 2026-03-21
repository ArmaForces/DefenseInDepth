//------------------------------------------------------------------------------------------------
//! Infantry spawner — spawns regular infantry groups.
//! Supports both legacy (self-ticking Process) and director (ScoreRequest/TriggerSpawn) modes.
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
	//! Director mode: score how desirable an infantry spawn is right now.
	//!
	//! Scoring logic:
	//!   Base = 1.0 (infantry is always the default option)
	//!   +1.5  if AI in zone < half of defender count  (outnumbered, send reinforcements)
	//!   +0.5  if phase == FINAL                        (spend remaining budget)
	//!   -2.0  if AI in zone > 80% of max cap          (zone saturated, don't pile on)
	//!   -0.5  if budget ratio < 0.15                  (critically low, be conservative)
	//------------------------------------------------------------------------------------------------
	override float ScoreRequest(AFM_DiDBattlefieldState state)
	{
		float score = 1.0;

		// Need more bodies — attackers are outnumbered in zone
		if (state.m_iAICountInZone < state.m_iDefenderCount * 0.5)
			score += 1.5;

		// Final push — spend aggressively
		if (state.m_ePhase == EAFMAttackPhase.FINAL)
			score += 0.5;

		// Zone already saturated — adding more infantry won't help
		if (state.m_iTotalActiveAI > m_iMaxAICount * 0.8)
			score -= 2.0;

		// Budget critically low — hold infantry back
		if (state.m_fBudgetRatio < 0.15)
			score -= 0.5;

		return score;
	}

	//------------------------------------------------------------------------------------------------
	//! Legacy mode: override to add randomised spawn interval variety.
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
			SpawnWave(GetSpawnCountForWave());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Infantry-specific spawn logic with optional sequential spawning.
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
