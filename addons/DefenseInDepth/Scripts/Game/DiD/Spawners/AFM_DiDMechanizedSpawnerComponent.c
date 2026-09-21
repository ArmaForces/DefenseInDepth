//------------------------------------------------------------------------------------------------
//! Mechanized spawner - spawns vehicle groups with different timing and logic
//!
//! Vehicles attack in two phases instead of driving at one placed waypoint and sitting there:
//!   OVERWATCH - drive to the placed waypoint that best overlooks the players, keeping a standoff
//!               distance so they do not roll into the middle of the defence
//!   ENGAGE    - once there, or after m_iMaxOverwatchSeconds, suppress or push the players' position,
//!               re-tasked as the players move
//------------------------------------------------------------------------------------------------
class AFM_DiDMechanizedSpawnerComponentClass: AFM_DiDSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDMechanizedSpawnerComponent: AFM_DiDSpawnerComponent
{
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

	[Attribute("1", UIWidgets.CheckBox, "Drive to an overwatch position first, then suppress or push the players. Off = hold one placed waypoint", category: "DiD Mechanized Tactics")]
	protected bool m_bOverwatchTactics;

	[Attribute("150", UIWidgets.EditBox, "Preferred distance (meters) from the players for the overwatch position", category: "DiD Mechanized Tactics")]
	protected float m_fOverwatchStandoff;

	[Attribute("60", UIWidgets.EditBox, "The overwatch position counts as reached within this distance (meters)", category: "DiD Mechanized Tactics")]
	protected float m_fOverwatchReachedRadius;

	[Attribute("120", UIWidgets.EditBox, "Engage anyway after this many seconds, in case the vehicle cannot reach its overwatch position", category: "DiD Mechanized Tactics")]
	protected int m_iMaxOverwatchSeconds;

	[Attribute("0.5", UIWidgets.Slider, "Share of groups that suppress the players rather than push onto them", params: "0 1 0.05", category: "DiD Mechanized Tactics")]
	protected float m_fSuppressShare;

	[Attribute("30", UIWidgets.EditBox, "Seconds between re-evaluating every group", category: "DiD Mechanized Tactics")]
	protected int m_iRetaskIntervalSeconds;

	[Attribute("40", UIWidgets.EditBox, "Players within this distance (meters) of each other count as one group", category: "DiD Mechanized Tactics")]
	protected float m_fTargetGroupRadius;

	[Attribute("60", UIWidgets.EditBox, "Only re-task a group once the players have moved this far (meters) from its current target", category: "DiD Mechanized Tactics")]
	protected float m_fRetaskMoveDistance;

	[Attribute("{ED8277F35B46B4AA}Prefabs/AI/Waypoints/AIWaypoint_Suppress.et", UIWidgets.ResourceNamePicker, desc: "Waypoint given to suppressing groups", params: "et", category: "DiD Mechanized Tactics")]
	protected ResourceName m_sSuppressWaypointPrefab;

	[Attribute("{D45195533ADF06E8}Prefabs/AI/Waypoints/DiD_AIWaypoint_SearchAndDestroy.et", UIWidgets.ResourceNamePicker, desc: "Waypoint given to groups that push the players", params: "et", category: "DiD Mechanized Tactics")]
	protected ResourceName m_sAttackWaypointPrefab;

	[Attribute("60", UIWidgets.EditBox, "Completion radius (meters) of the engage waypoint", category: "DiD Mechanized Tactics")]
	protected float m_fEngageWaypointRadius;

	protected ref array<IEntity> m_aSpawnedVehicles = {};
	protected ref map<AIGroup, ref AFM_MechanizedGroup> m_mGroups = new map<AIGroup, ref AFM_MechanizedGroup>();
	protected WorldTimestamp m_fLastRetask;

	//------------------------------------------------------------------------------------------------
	override void Prepare(AFM_DiDZoneComponent owner)
	{
		super.Prepare(owner);

		// Mechanized units spawn less frequently
		m_iWaveIntervalSeconds = Math.Ceil(m_iWaveIntervalSeconds * m_fDelayMultiplier);
		m_fLastRetask = GetCurrentTimestamp();

		PrintFormat("AFM_DiDMechanizedSpawnerComponent: Mechanized spawner initialized with %1s interval",
			m_iWaveIntervalSeconds, LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	override void Process()
	{
		super.Process();

		if (!m_bOverwatchTactics || !m_Zone)
			return;

		EAFMZoneState state = m_Zone.GetZoneState();
		if (state != EAFMZoneState.ACTIVE && state != EAFMZoneState.FROZEN)
			return;

		WorldTimestamp now = GetCurrentTimestamp();
		if (now.DiffSeconds(m_fLastRetask) < m_iRetaskIntervalSeconds)
			return;

		m_fLastRetask = now;
		UpdateGroups(now);
	}
	
	//------------------------------------------------------------------------------------------------
	override protected void SpawnWave()
	{
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
		foreach (AIGroup group, AFM_MechanizedGroup mechanizedGroup : m_mGroups)
		{
			if (mechanizedGroup)
				DeleteEngageWaypoint(mechanizedGroup);
		}
		m_mGroups.Clear();

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

		SCR_AIWaypoint overwatch = PickOverwatchWaypoint();

		// Track the crew so it counts towards the AI cap and is removed on cleanup
		AIGroup crew = m_crewConfig.SpawnCrew(cm, overwatch);
		if (!crew)
			return;

		TrackSpawnedGroup(crew);
		TrackMechanizedGroup(crew, vehicle, overwatch);
	}

	//------------------------------------------------------------------------------------------------
	// Overwatch and engagement
	//------------------------------------------------------------------------------------------------

	//! The placed waypoint that overlooks the players from about m_fOverwatchStandoff away. Falls back
	//! to a random one while no player can be found, which is how the spawner behaved before.
	protected SCR_AIWaypoint PickOverwatchWaypoint()
	{
		if (!m_bOverwatchTactics)
			return m_aAIWaypoints.GetRandomElement();

		vector target = FindPlayerGroupCenter();
		if (target == vector.Zero)
			return m_aAIWaypoints.GetRandomElement();

		SCR_AIWaypoint best;
		float bestScore;

		foreach (SCR_AIWaypoint waypoint : m_aAIWaypoints)
		{
			if (!waypoint)
				continue;

			// How far this position is from the standoff we want, so both too close and too far lose
			float distance = vector.DistanceXZ(waypoint.GetOrigin(), target);
			float score = Math.AbsFloat(distance - m_fOverwatchStandoff);

			if (!best || score < bestScore)
			{
				best = waypoint;
				bestScore = score;
			}
		}

		if (!best)
			return m_aAIWaypoints.GetRandomElement();

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackMechanizedGroup(notnull AIGroup group, IEntity vehicle, SCR_AIWaypoint overwatch)
	{
		AFM_MechanizedGroup mechanizedGroup = new AFM_MechanizedGroup();
		mechanizedGroup.m_Vehicle = vehicle;
		mechanizedGroup.m_OverwatchWaypoint = overwatch;
		mechanizedGroup.m_OverwatchStart = GetCurrentTimestamp();
		mechanizedGroup.m_bSuppresses = s_AIRandomGenerator.RandFloat01() < m_fSuppressShare;

		m_mGroups.Set(group, mechanizedGroup);
	}

	//------------------------------------------------------------------------------------------------
	//! Move groups from overwatch into the fight, and keep engaging groups pointed at the players
	protected void UpdateGroups(WorldTimestamp now)
	{
		vector target = FindPlayerGroupCenter();
		if (target == vector.Zero)
			return;

		foreach (AIGroup group, AFM_MechanizedGroup mechanizedGroup : m_mGroups)
		{
			if (!group || !mechanizedGroup)
				continue;

			if (!mechanizedGroup.m_bEngaging)
			{
				if (!IsOverwatchDone(mechanizedGroup, now))
					continue;

				mechanizedGroup.m_bEngaging = true;
			}
			else if (!NeedsRetasking(group, mechanizedGroup, target))
			{
				continue;
			}

			AssignEngageWaypoint(group, mechanizedGroup, target);
		}

		PruneGroups();
	}

	//------------------------------------------------------------------------------------------------
	//! In position, or it has taken long enough that waiting is pointless
	protected bool IsOverwatchDone(notnull AFM_MechanizedGroup mechanizedGroup, WorldTimestamp now)
	{
		if (now.DiffSeconds(mechanizedGroup.m_OverwatchStart) >= m_iMaxOverwatchSeconds)
			return true;

		if (!mechanizedGroup.m_Vehicle || !mechanizedGroup.m_OverwatchWaypoint)
			return true;

		float distance = vector.DistanceXZ(mechanizedGroup.m_Vehicle.GetOrigin(), mechanizedGroup.m_OverwatchWaypoint.GetOrigin());
		return distance <= m_fOverwatchReachedRadius;
	}

	//------------------------------------------------------------------------------------------------
	protected bool NeedsRetasking(notnull AIGroup group, notnull AFM_MechanizedGroup mechanizedGroup, vector target)
	{
		if (!mechanizedGroup.m_bHasTarget)
			return true;

		if (vector.DistanceXZ(target, mechanizedGroup.m_vTarget) >= m_fRetaskMoveDistance)
			return true;

		array<AIWaypoint> waypoints = {};
		group.GetWaypoints(waypoints);
		return waypoints.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	protected void AssignEngageWaypoint(notnull AIGroup group, notnull AFM_MechanizedGroup mechanizedGroup, vector target)
	{
		ResourceName prefab = m_sAttackWaypointPrefab;
		if (mechanizedGroup.m_bSuppresses)
			prefab = m_sSuppressWaypointPrefab;

		BaseWorld world = GetGame().GetWorld();
		target[1] = world.GetSurfaceY(target[0], target[2]);

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = target;

		SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), world, spawnParams));
		if (!waypoint)
		{
			PrintFormat("AFM_DiDMechanizedSpawnerComponent: Failed to spawn engage waypoint %1", prefab, level: LogLevel.ERROR);
			return;
		}

		waypoint.SetCompletionRadius(m_fEngageWaypointRadius);

		// Drop whatever the group was doing, including its overwatch waypoint. Placed waypoints are
		// shared between groups, so they are completed for this group only and never deleted.
		array<AIWaypoint> previous = {};
		group.GetWaypoints(previous);

		group.AddWaypoint(waypoint);

		foreach (AIWaypoint previousWaypoint : previous)
		{
			group.RemoveWaypoint(previousWaypoint);
		}

		DeleteEngageWaypoint(mechanizedGroup);
		mechanizedGroup.m_Waypoint = waypoint;
		mechanizedGroup.m_vTarget = target;
		mechanizedGroup.m_bHasTarget = true;

		PrintFormat("AFM_DiDMechanizedSpawnerComponent: Group engaging %1 (suppress: %2)",
			target, mechanizedGroup.m_bSuppresses, level: LogLevel.DEBUG);
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
	protected void DeleteEngageWaypoint(notnull AFM_MechanizedGroup mechanizedGroup)
	{
		if (!mechanizedGroup.m_Waypoint)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(mechanizedGroup.m_Waypoint);
		mechanizedGroup.m_Waypoint = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Forget wiped out crews and delete the waypoints they were given
	protected void PruneGroups()
	{
		ref map<AIGroup, ref AFM_MechanizedGroup> aliveGroups = new map<AIGroup, ref AFM_MechanizedGroup>();

		foreach (AIGroup group, AFM_MechanizedGroup mechanizedGroup : m_mGroups)
		{
			if (group)
			{
				aliveGroups.Set(group, mechanizedGroup);
				continue;
			}

			if (mechanizedGroup)
				DeleteEngageWaypoint(mechanizedGroup);
		}

		m_mGroups = aliveGroups;
	}
}

//------------------------------------------------------------------------------------------------
//! Attack state of one spawned vehicle crew
//------------------------------------------------------------------------------------------------
class AFM_MechanizedGroup
{
	IEntity m_Vehicle;						// Used to tell when the overwatch position is reached
	SCR_AIWaypoint m_OverwatchWaypoint;		// Placed waypoint, shared between groups, never deleted
	SCR_AIWaypoint m_Waypoint;				// Waypoint this spawner created, deleted when replaced
	bool m_bEngaging;
	bool m_bSuppresses;						// Decided once: suppress rather than push onto the players
	bool m_bHasTarget;
	vector m_vTarget;
	WorldTimestamp m_OverwatchStart;
}
