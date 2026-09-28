//------------------------------------------------------------------------------------------------
//! Infantry spawner - spawns regular infantry groups
//! Groups either hold one of the placed waypoints or hunt: they are periodically re-tasked with a
//! search and destroy waypoint on the largest group of players.
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

	[Attribute("1", UIWidgets.CheckBox, "Re-task groups to hunt the largest group of players instead of holding their placed waypoint", category: "DiD Infantry Hunting")]
	protected bool m_bHuntPlayers;

	[Attribute("0.25", UIWidgets.Slider, "Share of groups that keep their placed waypoint instead of hunting", params: "0 1 0.05", category: "DiD Infantry Hunting")]
	protected float m_fDefenderShare;

	[Attribute("45", UIWidgets.EditBox, "Seconds between re-tasking hunting groups", category: "DiD Infantry Hunting")]
	protected int m_iRetaskIntervalSeconds;

	[Attribute("40", UIWidgets.EditBox, "Players within this distance (meters) of each other count as one group", category: "DiD Infantry Hunting")]
	protected float m_fTargetGroupRadius;

	[Attribute("50", UIWidgets.EditBox, "Only re-task a group once the players have moved this far (meters) from its current target", category: "DiD Infantry Hunting")]
	protected float m_fRetaskMoveDistance;

	[Attribute("{D45195533ADF06E8}Prefabs/AI/Waypoints/DiD_AIWaypoint_SearchAndDestroy.et", UIWidgets.ResourceNamePicker, desc: "Waypoint given to hunting groups", params: "et", category: "DiD Infantry Hunting")]
	protected ResourceName m_sHuntWaypointPrefab;

	[Attribute("60", UIWidgets.EditBox, "Radius (meters) hunting groups search around their target", category: "DiD Infantry Hunting")]
	protected float m_fHuntWaypointRadius;

	protected int m_iCurrentSpawnPointIndex = 0;
	protected int m_iCurrentWaypointIndex = 0;

	protected ref map<AIGroup, ref AFM_HuntingGroup> m_mHuntingGroups = new map<AIGroup, ref AFM_HuntingGroup>();
	protected WorldTimestamp m_fLastRetask;

	//------------------------------------------------------------------------------------------------
	override void Prepare(AFM_DiDZoneComponent owner)
	{
		super.Prepare(owner);

		m_fLastRetask = GetCurrentTimestamp();

		PrintFormat("AFM_DiDInfantrySpawnerComponent: Infantry spawner initialized with %1 spawn points and %2 waypoints",
			m_aSpawnPoints.Count(), m_aAIWaypoints.Count(), level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Override to add variety to spawn intervals
	//------------------------------------------------------------------------------------------------
	override void Process()
	{
		if (!CanSpawnNow())
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
	//! Hunting groups are re-tasked here rather than in Process(), so they keep chasing the players even
	//! when the zone has no tickets left to spend or is pausing its reinforcements
	override void UpdateTactics()
	{
		if (!m_bHuntPlayers || !m_Zone)
			return;

		EAFMZoneState state = m_Zone.GetZoneState();
		if (state != EAFMZoneState.ACTIVE && state != EAFMZoneState.FROZEN)
			return;

		WorldTimestamp now = GetCurrentTimestamp();
		if (now.DiffSeconds(m_fLastRetask) < m_iRetaskIntervalSeconds)
			return;

		m_fLastRetask = now;
		RetaskHuntingGroups();
	}

	//------------------------------------------------------------------------------------------------
	//! Set from the scenario header through the zone
	void SetHuntPlayers(bool hunt)
	{
		m_bHuntPlayers = hunt;
		PrintFormat("AFM_DiDInfantrySpawnerComponent: Hunting set to %1", hunt, level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	override void Cleanup()
	{
		foreach (AIGroup group, AFM_HuntingGroup huntingGroup : m_mHuntingGroups)
		{
			if (huntingGroup)
				DeleteHuntWaypoint(huntingGroup);
		}
		m_mHuntingGroups.Clear();

		super.Cleanup();
	}

	//------------------------------------------------------------------------------------------------
	//! Infantry-specific spawn logic with optional sequential spawning
	//------------------------------------------------------------------------------------------------
	override protected void SpawnSingleGroup()
	{
		if (m_aSpawnPoints.Count() == 0 || m_aAIWaypoints.Count() == 0 || m_aAIGroupPrefabs.Count() == 0)
			return;

		// This spawner's own budget, separate from the zone pool
		if (m_bUseTickets && GetRemainingTickets() <= 0)
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
			TrackSpawnedGroup(group);
			TrackHuntingGroup(group);
			PrintFormat("AFM_DiDInfantrySpawnerComponent: Spawned infantry group %1", groupPrefab, level: LogLevel.DEBUG);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Hunting
	//------------------------------------------------------------------------------------------------

	//! Decide once per group whether it holds its placed waypoint or hunts the players
	protected void TrackHuntingGroup(notnull AIGroup group)
	{
		AFM_HuntingGroup huntingGroup = new AFM_HuntingGroup();
		huntingGroup.m_bDefender = s_AIRandomGenerator.RandFloat01() < m_fDefenderShare;
		m_mHuntingGroups.Set(group, huntingGroup);
	}

	//------------------------------------------------------------------------------------------------
	//! Send hunting groups after the largest group of players. Groups keep their placed waypoint while
	//! no player is found, so an empty zone behaves as before.
	protected void RetaskHuntingGroups()
	{
		vector target = FindPlayerGroupCenter();
		if (target == vector.Zero)
			return;

		foreach (AIGroup group, AFM_HuntingGroup huntingGroup : m_mHuntingGroups)
		{
			if (!group || !huntingGroup || huntingGroup.m_bDefender)
				continue;

			if (!NeedsRetasking(group, huntingGroup, target))
				continue;

			AssignHuntWaypoint(group, huntingGroup, target);
		}

		PruneHuntingGroups();
	}

	//------------------------------------------------------------------------------------------------
	//! Re-task a group that has nothing left to do, or whose target has moved far enough
	protected bool NeedsRetasking(notnull AIGroup group, notnull AFM_HuntingGroup huntingGroup, vector target)
	{
		if (!huntingGroup.m_bHasTarget)
			return true;

		if (vector.DistanceXZ(target, huntingGroup.m_vTarget) >= m_fRetaskMoveDistance)
			return true;

		array<AIWaypoint> waypoints = {};
		group.GetWaypoints(waypoints);
		return waypoints.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	protected void AssignHuntWaypoint(notnull AIGroup group, notnull AFM_HuntingGroup huntingGroup, vector target)
	{
		BaseWorld world = GetGame().GetWorld();
		target[1] = world.GetSurfaceY(target[0], target[2]);

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = target;

		SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_sHuntWaypointPrefab), world, spawnParams));
		if (!waypoint)
		{
			PrintFormat("AFM_DiDInfantrySpawnerComponent: Failed to spawn hunt waypoint %1", m_sHuntWaypointPrefab, level: LogLevel.ERROR);
			return;
		}

		waypoint.SetCompletionRadius(m_fHuntWaypointRadius);

		// Drop whatever the group was doing, including its placed waypoint. Placed waypoints are shared
		// between groups, so they are completed for this group only and never deleted.
		array<AIWaypoint> previous = {};
		group.GetWaypoints(previous);

		group.AddWaypoint(waypoint);

		foreach (AIWaypoint previousWaypoint : previous)
		{
			group.RemoveWaypoint(previousWaypoint);
		}

		DeleteHuntWaypoint(huntingGroup);
		huntingGroup.m_Waypoint = waypoint;
		huntingGroup.m_vTarget = target;
		huntingGroup.m_bHasTarget = true;

		PrintFormat("AFM_DiDInfantrySpawnerComponent: Group hunting players at %1", target, level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Centre of the largest group of living players, or zero when none can be found
	protected vector FindPlayerGroupCenter()
	{
		if (!m_Zone)
			return vector.Zero;

		SCR_Faction defenderFaction = m_Zone.GetDefenderFaction();
		if (!defenderFaction)
			return vector.Zero;

		array<vector> playerPositions = {};
		AFM_DiDTargetingHelper.GetPlayerPositions(defenderFaction, playerPositions);

		int groupSize;
		vector center = AFM_DiDTargetingHelper.FindDensestGroupCenter(playerPositions, m_fTargetGroupRadius, groupSize);
		if (groupSize == 0)
			return vector.Zero;

		return center;
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteHuntWaypoint(notnull AFM_HuntingGroup huntingGroup)
	{
		if (!huntingGroup.m_Waypoint)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(huntingGroup.m_Waypoint);
		huntingGroup.m_Waypoint = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Forget wiped out groups and delete the waypoints they were given
	protected void PruneHuntingGroups()
	{
		ref map<AIGroup, ref AFM_HuntingGroup> aliveGroups = new map<AIGroup, ref AFM_HuntingGroup>();

		foreach (AIGroup group, AFM_HuntingGroup huntingGroup : m_mHuntingGroups)
		{
			if (group)
			{
				aliveGroups.Set(group, huntingGroup);
				continue;
			}

			// Group wiped out: drop the waypoint it was given
			if (huntingGroup)
				DeleteHuntWaypoint(huntingGroup);
		}

		m_mHuntingGroups = aliveGroups;
	}
}

//------------------------------------------------------------------------------------------------
//! Hunting state of one spawned group
//------------------------------------------------------------------------------------------------
class AFM_HuntingGroup
{
	bool m_bDefender;			// Holds its placed waypoint instead of hunting
	bool m_bHasTarget;
	vector m_vTarget;
	SCR_AIWaypoint m_Waypoint;	// Waypoint this spawner created, deleted when replaced
}
