//------------------------------------------------------------------------------------------------
//! Extraction helicopter. Flight is handled by REAPER_AiHelicopters; this component decides when the
//! helicopter comes, which landing zone it uses and when the players are out.
//!
//! Sortie: WAITING (call received, helicopter not launched yet) -> INBOUND (move waypoint to the
//! landing zone) -> LANDING (land waypoint) -> HANDOVER (touched down, the AI crew is despawned and
//! the helicopter is left running for the players) -> COMPLETE once they fly it clear.
//!
//! Children: AFM_SpawnPointEntity        = where the helicopter comes from and leaves through
//!           AFM_DiDLandingZoneEntity    = candidate landing zones, one picked at random per call
//------------------------------------------------------------------------------------------------
class AFM_DiDExtractionComponentClass: AFM_DiDSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
enum AFM_EExtractionState
{
	IDLE,		// No extraction called
	WAITING,	// Called, helicopter not launched yet
	INBOUND,	// Flying to the landing zone
	LANDING,	// Coming down on the landing zone
	HANDOVER,	// On the ground, crew gone, the helicopter belongs to the players
	COMPLETE	// Players flew it clear
}

//------------------------------------------------------------------------------------------------
class AFM_DiDExtractionComponent: AFM_DiDSpawnerComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Extraction helicopter, must be supported by REAPER_AiHelicopters. Empty = Mi-8MT", params: "et", category: "DiD Extraction")]
	protected ResourceName m_sHelicopterPrefab;

	[Attribute("{BF78D4AAF08466CD}Prefabs/Groups/BLUFOR/REAPER_US_HelicopterCrew.et", UIWidgets.ResourceNamePicker, desc: "Crew group, moved into pilot, turret and cargo seats in that order", params: "et", category: "DiD Extraction")]
	protected ResourceName m_sCrewGroupPrefab;

	[Attribute("180", UIWidgets.EditBox, "Seconds between the call and the helicopter launching", category: "DiD Extraction")]
	protected int m_iSpawnDelaySeconds;

	[Attribute("1", UIWidgets.EditBox, "Players that must be aboard before the extraction counts as done", category: "DiD Extraction")]
	protected int m_iMinPlayersAboard;

	[Attribute("400", UIWidgets.EditBox, "Distance (meters) the players have to fly the helicopter from the landing zone to get out", category: "DiD Extraction")]
	protected float m_fEscapeDistance;

	[Attribute("5", UIWidgets.EditBox, "Height above terrain (meters) below which the helicopter counts as landed", category: "DiD Extraction")]
	protected float m_fTouchdownHeight;

	[Attribute("900", UIWidgets.EditBox, "Seconds after launch before the extraction counts as done regardless of the helicopter. Keeps a stuck pilot from hanging the match", category: "DiD Extraction")]
	protected int m_iSortieTimeoutSeconds;

	[Attribute("150", UIWidgets.EditBox, "Spawn height above terrain (meters)", category: "DiD Extraction Flight")]
	protected float m_fSpawnHeightAGL;

	[Attribute("140", UIWidgets.EditBox, "Cruise speed (km/h, 20-150)", category: "DiD Extraction Flight")]
	protected float m_fCruiseSpeed;

	[Attribute("80", UIWidgets.EditBox, "Cruise height above terrain (meters)", category: "DiD Extraction Flight")]
	protected float m_fCruiseHeight;

	[Attribute("60", UIWidgets.EditBox, "Approach speed to the landing zone (km/h, 20-150)", category: "DiD Extraction Flight")]
	protected float m_fApproachSpeed;

	[Attribute("300", UIWidgets.EditBox, "A move waypoint counts as reached within this distance (meters). Must be larger than the helicopter's turn radius", category: "DiD Extraction Flight")]
	protected float m_fWaypointReachedRadius;

	[Attribute("3", UIWidgets.EditBox, "How many of the child landing zones are used in a match. They are picked at random, named and marked on the map", category: "DiD Extraction")]
	protected int m_iLandingZoneCount;

	[Attribute("{927BC3E7CC0E3A54}PrefabsEditable/Markers/LZMapMarker.et", UIWidgets.ResourceNamePicker, desc: "Map marker spawned on every picked landing zone", params: "et", category: "DiD Extraction")]
	protected ResourceName m_sLandingZoneMarkerPrefab;

	protected static const ResourceName DEFAULT_HELICOPTER_PREFAB = "{3C6B3ED0C3AC30D5}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed_gunship_HE.et";
	protected static const ResourceName MOVE_WAYPOINT_PREFAB = "{F6FB686B76E77E7D}Prefabs/AI/Waypoints/REAPER_AiHelicopterMoveWaypoint.et";
	protected static const ResourceName LAND_WAYPOINT_PREFAB = "{6D8ADA4DF1A482C6}Prefabs/AI/Waypoints/REAPER_AiHelicopterLandWaypoint.et";

	protected static const int UPDATE_INTERVAL_MS = 1000;
	protected static const int CREW_BOARDING_TIMEOUT_SECONDS = 30;
	protected static const int WAYPOINT_TIMEOUT_SECONDS = 90;
	protected static const int NEVER_TAKE_OFF_PLAYER_COUNT = 9999;
	protected static const float MARKER_YAW = 90;	//!< Matches the yaw the marker prefab is placed with

	//! Named after the NATO alphabet in the order they are picked
	protected static ref array<string> LANDING_ZONE_NAMES = {"LZ Alpha", "LZ Bravo", "LZ Charlie", "LZ Delta", "LZ Echo", "LZ Foxtrot", "LZ Golf", "LZ Hotel"};

	protected ref array<AFM_DiDLandingZoneEntity> m_aLandingZones = {};			// Every candidate placed in the world
	protected ref array<AFM_DiDLandingZoneEntity> m_aSelectedLandingZones = {};	// The ones used this match
	protected ref array<string> m_aSelectedNames = {};
	protected ref array<IEntity> m_aMarkers = {};
	protected ref array<REAPER_AiHelicopterBaseWaypoint> m_aWaypoints = {};
	protected ref array<IEntity> m_aLeftovers = {};	// Wrecks and emptied crew groups, deleted on cleanup

	protected AFM_EExtractionState m_eState = AFM_EExtractionState.IDLE;
	protected AFM_DiDLandingZoneEntity m_LandingZone;
	protected string m_sLandingZoneName;
	protected Vehicle m_Helicopter;
	protected SCR_AIGroup m_CrewGroup;
	protected REAPER_AiHelicopterControllerComponent m_Controller;

	protected REAPER_AiHelicopterBaseWaypoint m_MoveInWaypoint;
	protected REAPER_AiHelicopterBaseWaypoint m_LandWaypoint;

	protected WorldTimestamp m_LaunchTime;
	protected WorldTimestamp m_SortieStartTime;
	protected WorldTimestamp m_StateStartTime;
	protected bool m_bCrewBoarded;
	protected AIWaypoint m_TrackedWaypoint;
	protected WorldTimestamp m_WaypointStartTime;

	//------------------------------------------------------------------------------------------------
	override void Prepare(AFM_DiDZoneComponent owner)
	{
		super.Prepare(owner);

		IEntity child = GetChildren();
		while (child)
		{
			AFM_DiDLandingZoneEntity landingZone = AFM_DiDLandingZoneEntity.Cast(child);
			if (landingZone)
				m_aLandingZones.Insert(landingZone);

			child = child.GetSibling();
		}

		if (m_aLandingZones.IsEmpty())
			PrintFormat("AFM_DiDExtractionComponent: No landing zones found, extraction will not work!", level: LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	override void Process()
	{
		if (!m_Zone)
			return;

		EAFMZoneState zoneState = m_Zone.GetZoneState();
		if (zoneState != EAFMZoneState.ACTIVE && zoneState != EAFMZoneState.FROZEN)
			return;

		if (m_eState != AFM_EExtractionState.WAITING)
			return;

		if (GetCurrentTimestamp().Less(m_LaunchTime))
			return;

		LaunchExtraction();
	}

	//------------------------------------------------------------------------------------------------
	//! Pick the landing zones used this match, name them and mark them on the map.
	//! Called by the zone when it activates; safe to call more than once.
	void SelectLandingZones()
	{
		if (!m_aSelectedLandingZones.IsEmpty())
			return;

		if (m_aLandingZones.IsEmpty())
		{
			PrintFormat("AFM_DiDExtractionComponent: No landing zones to select from", level: LogLevel.ERROR);
			return;
		}

		array<AFM_DiDLandingZoneEntity> candidates = {};
		candidates.Copy(m_aLandingZones);

		int wanted = Math.Min(m_iLandingZoneCount, candidates.Count());
		wanted = Math.Min(wanted, LANDING_ZONE_NAMES.Count());

		for (int i = 0; i < wanted; i++)
		{
			int index = s_AIRandomGenerator.RandInt(0, candidates.Count());
			AFM_DiDLandingZoneEntity landingZone = candidates[index];
			candidates.Remove(index);

			m_aSelectedLandingZones.Insert(landingZone);
			m_aSelectedNames.Insert(LANDING_ZONE_NAMES[i]);
			CreateLandingZoneMarker(landingZone, LANDING_ZONE_NAMES[i]);

			PrintFormat("AFM_DiDExtractionComponent: %1 at %2", LANDING_ZONE_NAMES[i], landingZone.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateLandingZoneMarker(notnull AFM_DiDLandingZoneEntity landingZone, string name)
	{
		if (m_sLandingZoneMarkerPrefab.IsEmpty())
			return;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;

		// Spawning with an identity rotation loses the yaw the marker prefab is authored with
		Math3D.AnglesToMatrix(Vector(MARKER_YAW, 0, 0), params.Transform);
		params.Transform[3] = landingZone.GetOrigin();

		IEntity marker = GetGame().SpawnEntityPrefab(Resource.Load(m_sLandingZoneMarkerPrefab), GetGame().GetWorld(), params);
		if (!marker)
		{
			PrintFormat("AFM_DiDExtractionComponent: Failed to spawn landing zone marker %1", m_sLandingZoneMarkerPrefab, level: LogLevel.ERROR);
			return;
		}

		PS_ManualMarker manualMarker = PS_ManualMarker.Cast(marker);
		if (manualMarker)
			manualMarker.AFM_SetDescription(name);
		else
			PrintFormat("AFM_DiDExtractionComponent: Landing zone marker is not a PS_ManualMarker, it will have no name", level: LogLevel.WARNING);

		m_aMarkers.Insert(marker);
	}

	//------------------------------------------------------------------------------------------------
	//! Called when a player uses the call-for-extraction action
	//! \return false when the call was refused, so the caller can tell the player why
	bool RequestExtraction()
	{
		if (m_eState != AFM_EExtractionState.IDLE)
			return false;

		SelectLandingZones();

		if (m_aSelectedLandingZones.IsEmpty())
		{
			PrintFormat("AFM_DiDExtractionComponent: Extraction called but no landing zones are configured", level: LogLevel.ERROR);
			return false;
		}

		if (m_aSpawnPoints.IsEmpty())
		{
			PrintFormat("AFM_DiDExtractionComponent: Extraction called but no spawn points are configured", level: LogLevel.ERROR);
			return false;
		}

		int index = s_AIRandomGenerator.RandInt(0, m_aSelectedLandingZones.Count());
		m_LandingZone = m_aSelectedLandingZones[index];
		m_sLandingZoneName = m_aSelectedNames[index];
		m_LaunchTime = GetCurrentTimestamp().PlusSeconds(m_iSpawnDelaySeconds);
		m_eState = AFM_EExtractionState.WAITING;

		PrintFormat("AFM_DiDExtractionComponent: Extraction called, launching in %1 s towards %2",
			m_iSpawnDelaySeconds, m_sLandingZoneName);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	AFM_EExtractionState GetExtractionState()
	{
		return m_eState;
	}

	//------------------------------------------------------------------------------------------------
	bool IsExtractionCalled()
	{
		return m_eState != AFM_EExtractionState.IDLE;
	}

	//------------------------------------------------------------------------------------------------
	string GetLandingZoneName()
	{
		return m_sLandingZoneName;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds until the helicopter launches, or 0 when it is already on its way
	int GetSecondsToLaunch()
	{
		if (m_eState != AFM_EExtractionState.WAITING)
			return 0;

		int remaining = m_LaunchTime.DiffSeconds(GetCurrentTimestamp());
		return Math.Max(0, remaining);
	}

	//------------------------------------------------------------------------------------------------
	override void Cleanup()
	{
		super.Cleanup();

		GetGame().GetCallqueue().Remove(UpdateSortie);

		SCR_EntityHelper.DeleteEntityAndChildren(m_Helicopter);
		SCR_EntityHelper.DeleteEntityAndChildren(m_CrewGroup);
		DeleteWaypoints();

		foreach (IEntity leftover : m_aLeftovers)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(leftover);
		}
		m_aLeftovers.Clear();

		foreach (IEntity marker : m_aMarkers)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(marker);
		}
		m_aMarkers.Clear();
		m_aSelectedLandingZones.Clear();
		m_aSelectedNames.Clear();
		m_sLandingZoneName = string.Empty;

		m_Helicopter = null;
		m_CrewGroup = null;
		m_Controller = null;
		m_LandingZone = null;
		m_bCrewBoarded = false;
		m_TrackedWaypoint = null;
		m_eState = AFM_EExtractionState.IDLE;
	}

	//------------------------------------------------------------------------------------------------
	//! The extraction helicopter is not part of the zone's AI budget
	override int GetActiveAICount()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasSpawnWaves()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	// Sortie lifecycle
	//------------------------------------------------------------------------------------------------

	protected void LaunchExtraction()
	{
		ResourceName heliPrefab = DEFAULT_HELICOPTER_PREFAB;
		if (!m_sHelicopterPrefab.IsEmpty())
			heliPrefab = m_sHelicopterPrefab;

		AFM_SpawnPointEntity entry = m_aSpawnPoints.GetRandomElement();
		vector transform[4];
		entry.GetWorldTransform(transform);

		BaseWorld world = GetGame().GetWorld();
		vector groundPos = transform[3];
		groundPos[1] = world.GetSurfaceY(groundPos[0], groundPos[2]);

		// The helicopter spawns in the air, the mod keeps it steady until the pilot takes over
		vector spawnPos = transform[3];
		spawnPos[1] = Math.Max(spawnPos[1], groundPos[1] + m_fSpawnHeightAGL);
		transform[3] = spawnPos;

		EntitySpawnParams heliParams = new EntitySpawnParams();
		heliParams.TransformMode = ETransformMode.WORLD;
		heliParams.Transform = transform;
		m_Helicopter = Vehicle.Cast(GetGame().SpawnEntityPrefab(Resource.Load(heliPrefab), world, heliParams));
		if (!m_Helicopter)
		{
			PrintFormat("AFM_DiDExtractionComponent: Failed to spawn helicopter %1", heliPrefab, level: LogLevel.ERROR);
			AbortExtraction("helicopter could not be spawned");
			return;
		}

		// Spawn the crew on the ground so nobody falls before being moved into the seats
		EntitySpawnParams crewParams = new EntitySpawnParams();
		crewParams.TransformMode = ETransformMode.WORLD;
		crewParams.Transform[3] = groundPos;
		m_CrewGroup = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_sCrewGroupPrefab), world, crewParams));
		if (!m_CrewGroup)
		{
			PrintFormat("AFM_DiDExtractionComponent: Failed to spawn crew %1", m_sCrewGroupPrefab, level: LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Helicopter);
			m_Helicopter = null;
			AbortExtraction("crew could not be spawned");
			return;
		}

		m_CrewGroup.REAPER_TeleportGroupInHelicopter(m_Helicopter, true);

		m_SortieStartTime = GetCurrentTimestamp();
		m_StateStartTime = m_SortieStartTime;
		m_bCrewBoarded = false;
		m_TrackedWaypoint = null;
		m_eState = AFM_EExtractionState.INBOUND;

		BuildWaypoints();

		GetGame().GetCallqueue().CallLater(UpdateSortie, UPDATE_INTERVAL_MS, true);

		PrintFormat("AFM_DiDExtractionComponent: Helicopter launched from %1 towards %2",
			groundPos, m_sLandingZoneName);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateSortie()
	{
		if (!m_Helicopter)
		{
			AbortExtraction("helicopter disappeared");
			return;
		}

		if (!m_Controller)
			ConfigureController();

		WorldTimestamp now = GetCurrentTimestamp();

		DamageManagerComponent heliDamage = m_Helicopter.GetDamageManager();
		if (heliDamage && heliDamage.IsDestroyed())
		{
			AbortExtraction("helicopter destroyed");
			return;
		}

		// A stuck pilot must never hang the match
		if (now.DiffSeconds(m_SortieStartTime) >= m_iSortieTimeoutSeconds)
		{
			PrintFormat("AFM_DiDExtractionComponent: Sortie timed out after %1 s, counting the extraction as done",
				m_iSortieTimeoutSeconds, level: LogLevel.WARNING);
			CompleteExtraction("sortie timed out");
			return;
		}

		if (m_eState == AFM_EExtractionState.HANDOVER)
		{
			UpdateHandover();
			return;
		}

		IEntity pilot = m_Helicopter.GetPilot();
		if (!m_bCrewBoarded)
		{
			if (pilot)
			{
				m_bCrewBoarded = true;
			}
			else if (now.DiffSeconds(m_SortieStartTime) > CREW_BOARDING_TIMEOUT_SECONDS)
			{
				AbortExtraction("crew never boarded");
				return;
			}
			else
			{
				return;
			}
		}

		if (!IsAliveCharacter(pilot))
		{
			AbortExtraction("pilot killed");
			return;
		}

		if (!m_CrewGroup)
		{
			AbortExtraction("crew lost");
			return;
		}

		AIWaypoint current = m_CrewGroup.GetCurrentWaypoint();
		if (!current)
		{
			// The mod completed the landing waypoint anyway. Hand the helicopter over if it is down,
			// otherwise the crew has nothing left to fly to.
			if (IsHelicopterLanded())
				HandOverToPlayers();
			else
				AbortExtraction("lost its waypoints");

			return;
		}

		UpdateStateFromWaypoint(current, now);

		AIWaypoint landWaypoint = m_LandWaypoint;
		if (current == landWaypoint)
		{
			if (IsHelicopterLanded())
				HandOverToPlayers();

			return;
		}

		AdvanceWaypoint(current, now);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateStateFromWaypoint(notnull AIWaypoint current, WorldTimestamp now)
	{
		AIWaypoint landWaypoint = m_LandWaypoint;

		AFM_EExtractionState newState = AFM_EExtractionState.INBOUND;
		if (current == landWaypoint)
			newState = AFM_EExtractionState.LANDING;

		if (newState == m_eState)
			return;

		m_eState = newState;
		m_StateStartTime = now;

		PrintFormat("AFM_DiDExtractionComponent: State %1", typename.EnumToString(AFM_EExtractionState, newState), level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Complete the current move waypoint once the helicopter is close enough or has spent too long on it.
	//! The mod only completes waypoints within about 220 m, but at cruise speed the helicopter's turn
	//! circle is wider than that, so it would orbit the waypoint forever.
	protected void AdvanceWaypoint(notnull AIWaypoint current, WorldTimestamp now)
	{
		if (current != m_TrackedWaypoint)
		{
			m_TrackedWaypoint = current;
			m_WaypointStartTime = now;
		}

		float distance = vector.DistanceXZ(m_Helicopter.GetOrigin(), current.GetOrigin());
		bool reached = distance <= m_fWaypointReachedRadius;
		bool timedOut = now.DiffSeconds(m_WaypointStartTime) >= WAYPOINT_TIMEOUT_SECONDS;
		if (!reached && !timedOut)
			return;

		PrintFormat("AFM_DiDExtractionComponent: Waypoint %1 at %2m (timed out: %3)",
			typename.EnumToString(AFM_EExtractionState, m_eState), distance, timedOut, level: LogLevel.DEBUG);

		// On the exit waypoint this makes the mod delete helicopter and crew
		m_CrewGroup.CompleteWaypoint(current);
	}

	//------------------------------------------------------------------------------------------------
	//! Down on the landing zone, near enough to it and low enough above the ground
	protected bool IsHelicopterLanded()
	{
		vector pos = m_Helicopter.GetOrigin();
		float terrain = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		if (pos[1] - terrain > m_fTouchdownHeight)
			return false;

		return vector.DistanceXZ(pos, m_LandingZone.GetOrigin()) <= m_fWaypointReachedRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Despawn the AI crew and leave the running helicopter to the players
	protected void HandOverToPlayers()
	{
		if (m_CrewGroup)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_CrewGroup);
			m_CrewGroup = null;
		}

		DeleteWaypoints();

		m_eState = AFM_EExtractionState.HANDOVER;
		m_StateStartTime = GetCurrentTimestamp();

		PrintFormat("AFM_DiDExtractionComponent: Helicopter down at %1, crew despawned, it is the players' now",
			m_sLandingZoneName);

		AFM_DiDExtractionZoneComponent zone = AFM_DiDExtractionZoneComponent.Cast(m_Zone);
		if (zone)
			zone.OnHelicopterHandedOver(m_sLandingZoneName);
	}

	//------------------------------------------------------------------------------------------------
	//! Waiting for the players to fly it clear of the landing zone
	protected void UpdateHandover()
	{
		float flown = vector.DistanceXZ(m_Helicopter.GetOrigin(), m_LandingZone.GetOrigin());
		if (flown < m_fEscapeDistance)
			return;

		if (CountPlayersAboard() < m_iMinPlayersAboard)
			return;

		CompleteExtraction("players flew it out");
	}

	//------------------------------------------------------------------------------------------------
	protected int CountPlayersAboard()
	{
		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Helicopter.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return 0;

		array<IEntity> occupants = {};
		compartments.GetOccupants(occupants);

		int count = 0;
		foreach (IEntity occupant : occupants)
		{
			if (EntityUtils.IsPlayer(occupant))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected void CompleteExtraction(string reason)
	{
		GetGame().GetCallqueue().Remove(UpdateSortie);

		PrintFormat("AFM_DiDExtractionComponent: Extraction complete (%1)", reason);

		m_eState = AFM_EExtractionState.COMPLETE;
		StashLeftovers();

		AFM_DiDExtractionZoneComponent zone = AFM_DiDExtractionZoneComponent.Cast(m_Zone);
		if (zone)
			zone.OnExtractionComplete();
	}

	//------------------------------------------------------------------------------------------------
	//! The helicopter is gone but the players are still here, so let them call another one
	protected void AbortExtraction(string reason)
	{
		GetGame().GetCallqueue().Remove(UpdateSortie);

		PrintFormat("AFM_DiDExtractionComponent: Extraction aborted (%1)", reason, level: LogLevel.WARNING);

		StashLeftovers();
		m_LandingZone = null;
		m_sLandingZoneName = string.Empty;
		m_bCrewBoarded = false;
		m_TrackedWaypoint = null;
		m_eState = AFM_EExtractionState.IDLE;

		AFM_DiDExtractionZoneComponent zone = AFM_DiDExtractionZoneComponent.Cast(m_Zone);
		if (zone)
			zone.OnExtractionAborted(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Wrecks and dead crews stay until the zone is cleaned up
	protected void StashLeftovers()
	{
		if (m_Helicopter)
			m_aLeftovers.Insert(m_Helicopter);
		if (m_CrewGroup)
			m_aLeftovers.Insert(m_CrewGroup);

		m_Helicopter = null;
		m_CrewGroup = null;
		m_Controller = null;

		DeleteWaypoints();
	}

	//------------------------------------------------------------------------------------------------
	//! The mod attaches its controller shortly after the helicopter spawns.
	//! Players have to be able to get in, so the vehicle must not be locked for them.
	protected void ConfigureController()
	{
		SCR_HelicopterControllerComponent heliController = SCR_HelicopterControllerComponent.Cast(m_Helicopter.FindComponent(SCR_HelicopterControllerComponent));
		if (!heliController)
			return;

		REAPER_AiHelicopterControllerComponent aiController = heliController.REAPER_GetAiControllerComponent();
		if (!aiController)
			return;

		aiController.REAPER_SetLockVehicleForPlayers_S(false);
		m_Controller = aiController;
	}

	//------------------------------------------------------------------------------------------------
	// Waypoints
	//------------------------------------------------------------------------------------------------

	//! Move in, land and wait for the players, then move out through the spawn point
	protected void BuildWaypoints()
	{
		vector landingPos = m_LandingZone.GetOrigin();

		m_MoveInWaypoint = AddWaypoint(MOVE_WAYPOINT_PREFAB, landingPos, m_fApproachSpeed, m_fCruiseHeight, false);
		m_LandWaypoint = AddWaypoint(LAND_WAYPOINT_PREFAB, landingPos, m_fApproachSpeed, m_fCruiseHeight, false);

		// No waypoint after the landing: the crew is despawned once it is down and the players fly it out

		ConfigureLandWaypoint();

		foreach (REAPER_AiHelicopterBaseWaypoint waypoint : m_aWaypoints)
		{
			if (waypoint && m_CrewGroup)
				m_CrewGroup.AddWaypoint(waypoint);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ConfigureLandWaypoint()
	{
		REAPER_AiHelicopterLandWaypoint landWaypoint = REAPER_AiHelicopterLandWaypoint.Cast(m_LandWaypoint);
		if (!landWaypoint)
		{
			PrintFormat("AFM_DiDExtractionComponent: Land waypoint has the wrong type", level: LogLevel.ERROR);
			return;
		}

		// The mod completes the landing waypoint as soon as its conditions pass, and would then fly on.
		// A count no squad can reach keeps it parked until we despawn the crew ourselves.
		landWaypoint.REAPER_SetWaitPlayersGetIn(true);
		landWaypoint.REAPER_SetWaitPlayersMinimumCount(NEVER_TAKE_OFF_PLAYER_COUNT);
		landWaypoint.REAPER_SetWaitPlayersGetInPercent(0);
		landWaypoint.REAPER_SetWaitPlayersGetOut(false);
		landWaypoint.REAPER_SetWaitAiGroupGetOut(false);
		landWaypoint.REAPER_SetCheckForCloseUnts(false);
	}

	//------------------------------------------------------------------------------------------------
	protected REAPER_AiHelicopterBaseWaypoint AddWaypoint(ResourceName prefab, vector pos, float speedKmh, float heightAGL, bool deleteOnArrival)
	{
		BaseWorld world = GetGame().GetWorld();
		pos[1] = world.GetSurfaceY(pos[0], pos[2]);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;

		REAPER_AiHelicopterBaseWaypoint waypoint = REAPER_AiHelicopterBaseWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), world, params));
		if (!waypoint)
		{
			PrintFormat("AFM_DiDExtractionComponent: Failed to spawn waypoint %1", prefab, level: LogLevel.ERROR);
			return null;
		}

		// Set before the waypoint is assigned: the mod reads them when it becomes current
		waypoint.REAPER_SetMaxSpeed(speedKmh);
		waypoint.REAPER_SetHeightAboveTerrain(heightAGL);
		waypoint.REAPER_SetDeleteHelicopterAndCrew(deleteOnArrival);

		m_aWaypoints.Insert(waypoint);
		return waypoint;
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteWaypoints()
	{
		foreach (REAPER_AiHelicopterBaseWaypoint waypoint : m_aWaypoints)
		{
			if (waypoint)
				SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}

		m_aWaypoints.Clear();
		m_MoveInWaypoint = null;
		m_LandWaypoint = null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAliveCharacter(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		SCR_DamageManagerComponent damageManager = character.GetDamageManager();
		return damageManager && !damageManager.IsDestroyed();
	}
}
