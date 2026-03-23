//------------------------------------------------------------------------------------------------
//! Infantry spawner — spawns regular infantry groups.
//! Director mode only: use ScoreRequest()/TriggerSpawn() interface.
//------------------------------------------------------------------------------------------------
class AFM_DiDInfantrySpawnerComponentClass: AFM_DiDSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDInfantrySpawnerComponent: AFM_DiDSpawnerComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Enable random spawn point selection", category: "DiD Infantry Spawner")]
	protected bool m_bRandomSpawnPoints;

	protected int m_iCurrentSpawnPointIndex = 0;

	//------------------------------------------------------------------------------------------------
	override void Prepare(AFM_DiDZoneComponent owner)
	{
		super.Prepare(owner);
		PrintFormat("AFM_DiDInfantrySpawnerComponent: Infantry spawner initialized with %1 spawn points",
			m_aSpawnPoints.Count(), LogLevel.DEBUG);
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
	//! Infantry-specific spawn logic.
	//! Spawns vanilla Move waypoints at each route marker position and chains them:
	//! staging → approach → zone assault (attack waypoint).
	//------------------------------------------------------------------------------------------------
	override protected void SpawnSingleGroup()
	{
		array<ref AFM_DiDApproachRoute> routes = m_Zone.GetApproachRoutes();
		if (m_aSpawnPoints.Count() == 0 || routes.Count() == 0 || m_aAIGroupPrefabs.Count() == 0)
			return;

		AFM_DiDAttackerDirector director = m_Zone.GetDirector();
		if (!director)
			return;

		ResourceName groupPrefab = m_aAIGroupPrefabs.GetRandomElement();

		// Spawn point selection: random or sequential round-robin
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

		AFM_DiDApproachRoute route = routes.GetRandomElement();

		AIGroup group = SpawnAI(groupPrefab, spawnPoint);
		if (!group)
			return;

		// Infantry chain: staging → approach → zone assault
		// Waypoints are spawned at marker GetOrigin() and tracked in the registry entry for cleanup.
		ref array<IEntity> routeWPs = {};

		if (route.m_StagingPoint)
		{
			SCR_AIWaypoint stagingWP = director.SpawnMoveWaypointAt(route.m_StagingPoint.GetOrigin());
			if (stagingWP) { routeWPs.Insert(stagingWP); group.AddWaypoint(stagingWP); }
		}

		SCR_AIWaypoint approachWP = director.SpawnMoveWaypointAt(route.m_ApproachPoint.GetOrigin());
		if (approachWP) { routeWPs.Insert(approachWP); group.AddWaypoint(approachWP); }

		AFM_ZoneAssaultWaypointEntity assaultMarker = m_Zone.GetAssaultWaypoint();
		if (assaultMarker)
		{
			SCR_AIWaypoint assaultWP = director.SpawnAssaultWaypointAt(assaultMarker.GetOrigin());
			if (assaultWP) { routeWPs.Insert(assaultWP); group.AddWaypoint(assaultWP); }
		}

		director.RegisterGroup(group, m_Zone, route, EAFMUnitType.INFANTRY, routeWPs);

		PrintFormat("AFM_DiDInfantrySpawnerComponent: Spawned infantry group %1", groupPrefab, LogLevel.DEBUG);
	}
}
