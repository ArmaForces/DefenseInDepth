//------------------------------------------------------------------------------------------------
//! Attack helicopter spawner. Flight and weapons are handled by REAPER_AiHelicopters; this spawner
//! decides when a helicopter comes, where it flies and when it leaves.
//!
//! Sortie: SPAWN -> INBOUND (to the holding point) -> ATTACK (move waypoints around the players; the mod
//! attacks what the crew sees) -> HOLD (hover at the holding point) -> ATTACK ... -> EGRESS (back to the
//! entry point, where the mod deletes helicopter and crew).
//!
//! Children: AFM_SpawnPointEntity = entry points (spawned in the air above them, also used to leave).
//! Optional SCR_AIWaypoint children = holding points; without them the helicopter holds between the
//! players and its entry point.
//------------------------------------------------------------------------------------------------
class AFM_DiDHeliSpawnerComponentClass: AFM_DiDAirSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
enum AFM_EHeliSortieState
{
	SPAWNING,	// Waiting for the crew to board
	INBOUND,	// Flying to the holding point
	ATTACK,		// Flying attack waypoints around the players
	HOLD,		// Hovering at the holding point
	EGRESS		// Leaving through the entry point
}

//------------------------------------------------------------------------------------------------
class AFM_DiDHeliSpawnerComponent: AFM_DiDAirSpawnerComponent
{
	// The attacking side's helicopters and the crew group that flies them
	protected ref array<ResourceName> m_aHelicopterPrefabs = {};
	protected ResourceName m_sCrewGroupPrefab;

	[Attribute("120", UIWidgets.EditBox, "Seconds after the zone becomes active before the first sortie", category: "DiD Heli Spawner")]
	protected int m_iFirstSortieDelay;

	[Attribute("300", UIWidgets.EditBox, "Seconds between the end of a sortie and the next one", category: "DiD Heli Spawner")]
	protected int m_iSortieCooldown;

	[Attribute("2", UIWidgets.EditBox, "Maximum sorties per zone activation", category: "DiD Heli Spawner")]
	protected int m_iMaxSorties;

	[Attribute("1", UIWidgets.Slider, "Chance that a sortie launches when one is due. On failure it is retried a minute later", params: "0 1 0.05", category: "DiD Heli Spawner")]
	protected float m_fSortieChance;

	[Attribute("-1", UIWidgets.EditBox, "Wave zones: first wave with sorties (-1 = any)", category: "DiD Heli Spawner")]
	protected int m_iMinWaveNumber;

	[Attribute("-1", UIWidgets.EditBox, "Wave zones: last wave with sorties (-1 = any)", category: "DiD Heli Spawner")]
	protected int m_iMaxWaveNumber;

	[Attribute("100", UIWidgets.EditBox, "Speed on attack waypoints (km/h, 20-150)", category: "DiD Heli Flight")]
	protected float m_fAttackSpeed;

	[Attribute("60", UIWidgets.EditBox, "Height above terrain on attack waypoints (meters)", category: "DiD Heli Flight")]
	protected float m_fAttackHeight;

	[Attribute("500", UIWidgets.EditBox, "Distance (meters) of attack waypoints from the densest player group", category: "DiD Heli Flight")]
	protected float m_fAttackRadius;

	[Attribute("3", UIWidgets.EditBox, "Attack waypoints per attack, spread over an arc facing the helicopter's entry point", category: "DiD Heli Flight")]
	protected int m_iAttackWaypoints;

	[Attribute("120", UIWidgets.EditBox, "Width (degrees) of the arc the attack waypoints are spread over", category: "DiD Heli Flight")]
	protected float m_fAttackArc;

	[Attribute("45", UIWidgets.EditBox, "Seconds to hover at the holding point between attacks", category: "DiD Heli Flight")]
	protected int m_iHoldSeconds;

	[Attribute("300", UIWidgets.EditBox, "Seconds after spawning before the helicopter leaves", category: "DiD Heli Flight")]
	protected int m_iTimeOnStationSeconds;

	[Attribute("0.5", UIWidgets.Slider, "Leave when helicopter health drops below this fraction", params: "0 1 0.05", category: "DiD Heli Flight")]
	protected float m_fBreakOffHealth;

	[Attribute("1", UIWidgets.CheckBox, "Leave when any crew member is killed", category: "DiD Heli Flight")]
	protected bool m_bBreakOffOnCrewLoss;

	[Attribute("1", UIWidgets.CheckBox, "Allow rocket pods", category: "DiD Heli Weapons")]
	protected bool m_bRocketsAllowed;

	[Attribute("1", UIWidgets.CheckBox, "Only attack players, not AI or player-built structures", category: "DiD Heli Weapons")]
	protected bool m_bPlayersOnly;

	[Attribute("0", UIWidgets.CheckBox, "Only fire rockets at vehicles", category: "DiD Heli Weapons")]
	protected bool m_bVehiclesOnly;

	[Attribute("30", UIWidgets.EditBox, "How long to stay on the same target (mod ticks, 30-120)", category: "DiD Heli Weapons")]
	protected int m_iTargetTimeout;

	[Attribute("0", UIWidgets.CheckBox, "Only attack when close to the current waypoint", category: "DiD Heli Weapons")]
	protected bool m_bOnlyAttackCloseToWaypoint;

	[Attribute("600", UIWidgets.EditBox, "Range (meters) from the waypoint for the option above (300-1000)", category: "DiD Heli Weapons")]
	protected int m_iAttackRangeFromWaypoint;

	[Attribute("0", UIWidgets.EditBox, "Furthest the helicopter engages (meters). 0 = the mod's own 540 m ceiling, which lets it fire from where it is barely visible", category: "DiD Heli Weapons")]
	protected float m_fMaxAttackDistance;

	[Attribute("150", UIWidgets.EditBox, "The helicopter only engages targets within this distance (meters) of the aim point it was sent to. 0 = engage anything it sees", category: "DiD Heli Weapons")]
	protected float m_fStrikeAreaRadius;

	[Attribute("40", UIWidgets.EditBox, "Players within this distance (meters) of each other count as one group; the helicopter is sent at the centre of the largest one", category: "DiD Heli Targeting")]
	protected float m_fTargetGroupRadius;

	[Attribute("50", UIWidgets.EditBox, "Never aim within this distance (meters) of attacker AI", category: "DiD Heli Targeting")]
	protected float m_fFriendlyFireRadius;

	[Attribute("0", UIWidgets.EditBox, "Supplies added to the zone's supply cache when the helicopter is shot down", category: "DiD Heli Spawner")]
	protected int m_iSupplyRewardOnKill;

	protected static const ResourceName HOVER_WAYPOINT_PREFAB = "{471EDCB44D26C193}Prefabs/AI/Waypoints/REAPER_AiHelicopterHoverWaypoint.et";

	protected static const int SORTIE_RETRY_SECONDS = 60;
	protected static const int EGRESS_TIMEOUT_SECONDS = 240;

	protected ref AFM_HeliSortie m_Sortie;
	protected ref array<IEntity> m_aLeftovers = {};	// Wrecks and emptied crew groups, deleted on cleanup
	protected int m_iSortiesFlown;
	protected bool m_bZoneActiveSeen;
	protected WorldTimestamp m_NextSortieTime;

	//------------------------------------------------------------------------------------------------
	override void Process()
	{
		if (!m_Zone)
			return;

		EAFMZoneState state = m_Zone.GetZoneState();
		if (state != EAFMZoneState.ACTIVE && state != EAFMZoneState.FROZEN)
			return;

		WorldTimestamp now = GetCurrentTimestamp();
		if (!m_bZoneActiveSeen)
		{
			m_bZoneActiveSeen = true;
			m_NextSortieTime = now.PlusSeconds(m_iFirstSortieDelay);
		}

		if (m_Sortie || m_iSortiesFlown >= m_iMaxSorties || now.Less(m_NextSortieTime))
			return;

		if (s_AIRandomGenerator.RandFloat01() > m_fSortieChance)
		{
			m_NextSortieTime = now.PlusSeconds(SORTIE_RETRY_SECONDS);
			return;
		}

		StartSortie();
	}

	//------------------------------------------------------------------------------------------------
	override protected void ResolveFactionContent()
	{
		AFM_DiDSideConfig side = GetAttackerConfig();
		if (!side)
			return;

		if (side.m_aHelicopters)
			m_aHelicopterPrefabs = side.m_aHelicopters;

		m_sCrewGroupPrefab = side.m_sHelicopterCrewGroup;

		// Nothing to fly: stop the spawner rather than let it retry a sortie it cannot start
		if (m_aHelicopterPrefabs.IsEmpty() || m_sCrewGroupPrefab.IsEmpty())
		{
			PrintFormat("AFM_DiDHeliSpawnerComponent: The attacking side (%1) has no helicopters or no crew group, no sorties will fly",
				side.GetLabel(), level: LogLevel.WARNING);
			m_iSortiesFlown = m_iMaxSorties;
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Cleanup()
	{
		super.Cleanup();

		GetGame().GetCallqueue().Remove(UpdateSortie);
		if (m_Sortie)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Sortie.m_Helicopter);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Sortie.m_CrewGroup);
			DeleteWaypoints(m_Sortie.m_aWaypoints);
			m_Sortie = null;
		}

		foreach (IEntity leftover : m_aLeftovers)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(leftover);
		}
		m_aLeftovers.Clear();

		m_iSortiesFlown = 0;
		m_bZoneActiveSeen = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Wave zones only process spawners inside the configured wave range
	override bool IsActive()
	{
		AFM_DiDWaveZoneComponent waveZone = AFM_DiDWaveZoneComponent.Cast(m_Zone);
		if (!waveZone)
			return true;

		int wave = waveZone.GetCurrentWave();
		if (m_iMinWaveNumber >= 0 && wave < m_iMinWaveNumber)
			return false;

		return m_iMaxWaveNumber < 0 || wave <= m_iMaxWaveNumber;
	}

	//------------------------------------------------------------------------------------------------
	// Sortie lifecycle
	//------------------------------------------------------------------------------------------------

	protected void StartSortie()
	{
		if (m_aSpawnPoints.IsEmpty())
		{
			PrintFormat("AFM_DiDHeliSpawnerComponent: No entry points configured!", level: LogLevel.WARNING);
			m_iSortiesFlown = m_iMaxSorties;
			return;
		}

		ResourceName heliPrefab = m_aHelicopterPrefabs.GetRandomElement();

		Vehicle helicopter;
		SCR_AIGroup crew;
		vector groundPos;
		if (!SpawnHelicopterWithCrew(heliPrefab, m_sCrewGroupPrefab, helicopter, crew, groundPos))
		{
			m_NextSortieTime = GetCurrentTimestamp().PlusSeconds(m_iSortieCooldown);
			return;
		}

		m_Sortie = new AFM_HeliSortie();
		m_Sortie.m_Helicopter = helicopter;
		m_Sortie.m_CrewGroup = crew;
		m_Sortie.m_vEntryPoint = groundPos;
		m_Sortie.m_StartTime = GetCurrentTimestamp();
		m_Sortie.m_StateStartTime = m_Sortie.m_StartTime;
		m_Sortie.m_eState = AFM_EHeliSortieState.SPAWNING;
		m_iSortiesFlown++;

		// The pilot picks up the current waypoint once seated
		m_Sortie.m_vHoldPoint = PickHoldPoint(groundPos);
		SetState(AFM_EHeliSortieState.INBOUND);

		GetGame().GetCallqueue().CallLater(UpdateSortie, UPDATE_INTERVAL_MS, true);

		PrintFormat("AFM_DiDHeliSpawnerComponent: Sortie %1/%2 launched from %3", m_iSortiesFlown, m_iMaxSorties, groundPos, level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateSortie()
	{
		if (!m_Sortie)
		{
			GetGame().GetCallqueue().Remove(UpdateSortie);
			return;
		}

		Vehicle helicopter = m_Sortie.m_Helicopter;

		// Deleted by the mod at the exit point, or by someone else
		if (!helicopter)
		{
			EndSortie("left the area");
			return;
		}

		if (!m_Sortie.m_Controller)
			ConfigureController();

		WorldTimestamp now = GetCurrentTimestamp();

		DamageManagerComponent heliDamage = helicopter.GetDamageManager();
		if (heliDamage && heliDamage.IsDestroyed())
		{
			AwardShootDownReward();
			EndSortie("shot down");
			return;
		}

		IEntity pilot = helicopter.GetPilot();
		if (!m_Sortie.m_bCrewBoarded)
		{
			if (pilot)
			{
				m_Sortie.m_bCrewBoarded = true;
			}
			else if (now.DiffSeconds(m_Sortie.m_StartTime) > BOARDING_TIMEOUT_SECONDS)
			{
				PrintFormat("AFM_DiDHeliSpawnerComponent: Crew failed to board, removing helicopter", level: LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(helicopter);
				EndSortie("crew never boarded");
				return;
			}
			else
			{
				return;
			}
		}

		if (!IsAliveCharacter(pilot))
		{
			EndSortie("pilot killed");
			return;
		}

		if (m_Sortie.m_eState != AFM_EHeliSortieState.HOLD)
			AdvanceWaypoint(now);

		if (m_Sortie.m_eState == AFM_EHeliSortieState.EGRESS)
		{
			if (now.DiffSeconds(m_Sortie.m_StateStartTime) > EGRESS_TIMEOUT_SECONDS)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(helicopter);
				EndSortie("egress timed out");
			}
			return;
		}

		string breakOffReason = GetBreakOffReason(now);
		if (!breakOffReason.IsEmpty())
		{
			PrintFormat("AFM_DiDHeliSpawnerComponent: Breaking off (%1)", breakOffReason, level: LogLevel.DEBUG);
			SetState(AFM_EHeliSortieState.EGRESS);
			return;
		}

		switch (m_Sortie.m_eState)
		{
			case AFM_EHeliSortieState.INBOUND:
				if (!HasPendingWaypoints())
					SetState(AFM_EHeliSortieState.ATTACK);
				break;

			case AFM_EHeliSortieState.ATTACK:
				if (!HasPendingWaypoints())
					SetState(AFM_EHeliSortieState.HOLD);
				break;

			case AFM_EHeliSortieState.HOLD:
				if (now.DiffSeconds(m_Sortie.m_StateStartTime) >= m_iHoldSeconds)
					SetState(AFM_EHeliSortieState.ATTACK);
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Switch state and give the crew the matching waypoints
	protected void SetState(AFM_EHeliSortieState newState)
	{
		array<REAPER_AiHelicopterBaseWaypoint> waypoints = {};

		switch (newState)
		{
			case AFM_EHeliSortieState.INBOUND:
				AddWaypoint(waypoints, MOVE_WAYPOINT_PREFAB, m_Sortie.m_vHoldPoint, m_fCruiseSpeed, m_fCruiseHeight, false);
				break;

			case AFM_EHeliSortieState.ATTACK:
				CreateAttackWaypoints(waypoints);
				break;

			case AFM_EHeliSortieState.HOLD:
				AddWaypoint(waypoints, HOVER_WAYPOINT_PREFAB, m_Sortie.m_vHoldPoint, m_fCruiseSpeed, m_fCruiseHeight, false);
				break;

			case AFM_EHeliSortieState.EGRESS:
				// Stop shooting on the way out, and let the mod delete helicopter and crew on arrival
				if (m_Sortie.m_Controller)
					m_Sortie.m_Controller.AFM_ClearStrikeArea();

				AddWaypoint(waypoints, MOVE_WAYPOINT_PREFAB, m_Sortie.m_vEntryPoint, m_fCruiseSpeed, m_fCruiseHeight, true);
				break;
		}

		AssignWaypoints(waypoints);

		m_Sortie.m_eState = newState;
		m_Sortie.m_StateStartTime = GetCurrentTimestamp();

		PrintFormat("AFM_DiDHeliSpawnerComponent: Sortie state %1", typename.EnumToString(AFM_EHeliSortieState, newState), level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Complete the current move waypoint once the helicopter is close enough or has spent too long on it.
	//! The mod only completes waypoints within about 220 m, but at cruise speed the helicopter's turn
	//! circle is wider than that, so it would orbit the waypoint forever.
	protected void AdvanceWaypoint(WorldTimestamp now)
	{
		SCR_AIGroup crew = m_Sortie.m_CrewGroup;
		if (!crew)
			return;

		AIWaypoint current = crew.GetCurrentWaypoint();
		if (!current)
			return;

		if (current != m_Sortie.m_TrackedWaypoint)
		{
			m_Sortie.m_TrackedWaypoint = current;
			m_Sortie.m_WaypointStartTime = now;
		}

		float distance = vector.DistanceXZ(m_Sortie.m_Helicopter.GetOrigin(), current.GetOrigin());
		bool reached = distance <= m_fWaypointReachedRadius;
		bool timedOut = now.DiffSeconds(m_Sortie.m_WaypointStartTime) >= WAYPOINT_TIMEOUT_SECONDS;
		if (!reached && !timedOut)
			return;

		PrintFormat("AFM_DiDHeliSpawnerComponent: Waypoint %1 at %2m (timed out: %3)",
			typename.EnumToString(AFM_EHeliSortieState, m_Sortie.m_eState), distance, timedOut, level: LogLevel.DEBUG);

		// On the egress waypoint this makes the mod delete helicopter and crew
		crew.CompleteWaypoint(current);
	}

	//------------------------------------------------------------------------------------------------
	//! \return why the helicopter should leave now, or an empty string to keep fighting
	protected string GetBreakOffReason(WorldTimestamp now)
	{
		EAFMZoneState zoneState = m_Zone.GetZoneState();
		if (zoneState != EAFMZoneState.ACTIVE && zoneState != EAFMZoneState.FROZEN)
			return "zone no longer active";

		if (now.DiffSeconds(m_Sortie.m_StartTime) >= m_iTimeOnStationSeconds)
			return "time on station used up";

		DamageManagerComponent heliDamage = m_Sortie.m_Helicopter.GetDamageManager();
		if (heliDamage && heliDamage.GetHealthScaled() < m_fBreakOffHealth)
			return "damaged";

		if (m_bBreakOffOnCrewLoss)
		{
			int aliveCrew = CountAliveCrew();
			if (aliveCrew > m_Sortie.m_iMaxCrewSeen)
				m_Sortie.m_iMaxCrewSeen = aliveCrew;
			else if (aliveCrew < m_Sortie.m_iMaxCrewSeen)
				return "crew member killed";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected void EndSortie(string reason)
	{
		GetGame().GetCallqueue().Remove(UpdateSortie);
		if (!m_Sortie)
			return;

		PrintFormat("AFM_DiDHeliSpawnerComponent: Sortie ended (%1)", reason, level: LogLevel.DEBUG);

		// Wrecks and dead crews stay until the zone is cleaned up
		if (m_Sortie.m_Helicopter)
			m_aLeftovers.Insert(m_Sortie.m_Helicopter);
		if (m_Sortie.m_CrewGroup)
			m_aLeftovers.Insert(m_Sortie.m_CrewGroup);

		DeleteWaypoints(m_Sortie.m_aWaypoints);
		m_Sortie = null;
		m_NextSortieTime = GetCurrentTimestamp().PlusSeconds(m_iSortieCooldown);
	}

	//------------------------------------------------------------------------------------------------
	//! The mod attaches its controller shortly after the helicopter spawns
	protected void ConfigureController()
	{
		SCR_HelicopterControllerComponent heliController = SCR_HelicopterControllerComponent.Cast(m_Sortie.m_Helicopter.FindComponent(SCR_HelicopterControllerComponent));
		if (!heliController)
			return;

		REAPER_AiHelicopterControllerComponent aiController = heliController.REAPER_GetAiControllerComponent();
		if (!aiController)
			return;

		aiController.REAPER_SetWeaponProperties(m_bRocketsAllowed, m_bPlayersOnly, m_bVehiclesOnly, m_iTargetTimeout, true, 20);
		if (m_bOnlyAttackCloseToWaypoint)
			aiController.REAPER_SetOnlyAttackCloseToWaypoint(true, m_iAttackRangeFromWaypoint);
		aiController.REAPER_SetLockVehicleForPlayers_S(true);
		aiController.AFM_SetMaxAttackDistance(m_fMaxAttackDistance);

		m_Sortie.m_Controller = aiController;
	}

	//------------------------------------------------------------------------------------------------
	// Waypoints
	//------------------------------------------------------------------------------------------------

	//! Move waypoints on an arc around the densest player group, facing the helicopter's own side
	protected void CreateAttackWaypoints(notnull array<REAPER_AiHelicopterBaseWaypoint> outWaypoints)
	{
		vector center = FindAttackCenter();
		ApplyStrikeArea(center);

		vector toEntry = m_Sortie.m_vEntryPoint - center;
		float baseYaw = Math.Atan2(toEntry[0], toEntry[2]);
		float arc = m_fAttackArc * Math.DEG2RAD;

		// Sweep left to right or right to left at random
		float direction = 1;
		if (s_AIRandomGenerator.RandFloat01() < 0.5)
			direction = -1;

		int count = Math.Max(1, m_iAttackWaypoints);
		for (int i = 0; i < count; i++)
		{
			float offset = 0;
			if (count > 1)
				offset = (-0.5 + i / (count - 1.0)) * arc * direction;

			float yaw = baseYaw + offset;
			vector pos = center + Vector(Math.Sin(yaw), 0, Math.Cos(yaw)) * m_fAttackRadius;
			AddWaypoint(outWaypoints, MOVE_WAYPOINT_PREFAB, pos, m_fAttackSpeed, m_fAttackHeight, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AddWaypoint(notnull array<REAPER_AiHelicopterBaseWaypoint> outWaypoints, ResourceName prefab, vector pos, float speedKmh, float heightAGL, bool deleteOnArrival)
	{
		REAPER_AiHelicopterBaseWaypoint waypoint = CreateWaypoint(prefab, pos, speedKmh, heightAGL, deleteOnArrival);
		if (waypoint)
			outWaypoints.Insert(waypoint);
	}

	//------------------------------------------------------------------------------------------------
	//! Replace the crew's waypoints. New ones are added before old ones are deleted, because the mod keeps
	//! flying towards its last waypoint until a new one becomes current.
	protected void AssignWaypoints(notnull array<REAPER_AiHelicopterBaseWaypoint> waypoints)
	{
		SCR_AIGroup crew = m_Sortie.m_CrewGroup;

		array<AIWaypoint> previous = {};
		if (crew)
			crew.GetWaypoints(previous);

		foreach (REAPER_AiHelicopterBaseWaypoint waypoint : waypoints)
		{
			if (crew)
				crew.AddWaypoint(waypoint);
		}

		if (crew)
		{
			foreach (AIWaypoint waypoint : previous)
			{
				crew.RemoveWaypoint(waypoint);
			}
		}

		DeleteWaypoints(m_Sortie.m_aWaypoints);
		m_Sortie.m_aWaypoints.Copy(waypoints);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteWaypoints(notnull array<REAPER_AiHelicopterBaseWaypoint> waypoints)
	{
		foreach (REAPER_AiHelicopterBaseWaypoint waypoint : waypoints)
		{
			if (waypoint)
				SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}
		waypoints.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasPendingWaypoints()
	{
		if (!m_Sortie.m_CrewGroup)
			return false;

		array<AIWaypoint> waypoints = {};
		m_Sortie.m_CrewGroup.GetWaypoints(waypoints);
		return !waypoints.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	// Targeting
	//------------------------------------------------------------------------------------------------

	//! Aim point for the attack, picked the way the mortar spawner picks its fire mission: the centre of
	//! the densest group of players that are inside the zone and clear of the attackers' own troops.
	//! Falls back to the zone centre when no player qualifies.
	protected vector FindAttackCenter()
	{
		if (!m_Zone)
			return GetZoneCenter();

		// Positions the helicopter must not drop rockets on top of
		array<vector> attackerPositions = {};
		SCR_Faction attackers = m_Zone.GetAttackerFaction();
		if (attackers)
			AFM_DiDTargetingHelper.GetAIPositions(attackers, attackerPositions, true);

		array<vector> targets = {};
		SCR_Faction defenders = m_Zone.GetDefenderFaction();
		if (defenders)
		{
			array<vector> playerPositions = {};
			AFM_DiDTargetingHelper.GetPlayerPositions(defenders, playerPositions);

			foreach (vector playerPos : playerPositions)
			{
				if (IsValidTargetPosition(playerPos, attackerPositions))
					targets.Insert(playerPos);
			}
		}

		int groupSize;
		vector groupCenter = AFM_DiDTargetingHelper.FindDensestGroupCenter(targets, m_fTargetGroupRadius, groupSize);
		if (groupSize > 0)
		{
			// The centre of a spread out group can sit on top of own troops; then go for one player directly
			if (IsValidTargetPosition(groupCenter, attackerPositions))
				return groupCenter;

			return targets[0];
		}

		return GetZoneCenter();
	}

	//------------------------------------------------------------------------------------------------
	//! Inside the zone and clear of the attackers' own troops
	protected bool IsValidTargetPosition(vector pos, notnull array<vector> attackerPositions)
	{
		if (!m_Zone.IsPointInsideZone(pos))
			return false;

		return !AFM_DiDTargetingHelper.IsNearAnyPosition(pos, attackerPositions, m_fFriendlyFireRadius);
	}

	//------------------------------------------------------------------------------------------------
	//! Confine the helicopter's weapons to the aim point it was sent to, so it engages what is there
	//! instead of the best target anywhere in its 540 m reach
	protected void ApplyStrikeArea(vector center)
	{
		if (!m_Sortie || !m_Sortie.m_Controller)
			return;

		m_Sortie.m_Controller.AFM_SetStrikeArea(center, m_fStrikeAreaRadius);
	}

	//------------------------------------------------------------------------------------------------
	//! A placed holding point, or a point between the zone and the entry point at twice the attack radius,
	//! at most halfway to the entry point so the helicopter does not hold where it spawned
	protected vector PickHoldPoint(vector entryPoint)
	{
		if (!m_aAIWaypoints.IsEmpty())
			return m_aAIWaypoints.GetRandomElement().GetOrigin();

		vector zoneCenter = GetZoneCenter();
		vector toEntry = entryPoint - zoneCenter;
		toEntry[1] = 0;

		float distance = toEntry.Length();
		float holdDistance = Math.Min(m_fAttackRadius * 2, distance * 0.5);
		if (distance < 1)
			return entryPoint;

		return zoneCenter + toEntry.Normalized() * holdDistance;
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetZoneCenter()
	{
		PolylineShapeEntity polyline = m_Zone.GetPolylineEntity();
		if (polyline)
			return AFM_DiDTargetingHelper.GetPolylineCenter(polyline);

		return m_Zone.GetOwner().GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	// Helpers
	//------------------------------------------------------------------------------------------------

	protected int CountAliveCrew()
	{
		if (!m_Sortie.m_CrewGroup)
			return 0;

		array<AIAgent> agents = {};
		m_Sortie.m_CrewGroup.GetAgents(agents);

		int alive = 0;
		foreach (AIAgent agent : agents)
		{
			if (IsAliveCharacter(agent.GetControlledEntity()))
				alive++;
		}

		return alive;
	}

	//------------------------------------------------------------------------------------------------
	protected void AwardShootDownReward()
	{
		if (m_iSupplyRewardOnKill <= 0)
			return;

		SCR_ResourceComponent supplyCache = m_Zone.GetSupplyCache();
		if (!supplyCache)
			return;

		SCR_ResourceContainer container = supplyCache.GetContainer(EResourceType.SUPPLIES);
		if (!container)
			return;

		if (!container.SetResourceValue(container.GetResourceValue() + m_iSupplyRewardOnKill))
			PrintFormat("AFM_DiDHeliSpawnerComponent: Failed to add shoot-down reward to the supply cache", level: LogLevel.WARNING);
	}
}

//------------------------------------------------------------------------------------------------
//! Runtime data of one helicopter sortie
//------------------------------------------------------------------------------------------------
class AFM_HeliSortie
{
	Vehicle m_Helicopter;
	SCR_AIGroup m_CrewGroup;
	REAPER_AiHelicopterControllerComponent m_Controller;
	ref array<REAPER_AiHelicopterBaseWaypoint> m_aWaypoints = {};
	AFM_EHeliSortieState m_eState;
	vector m_vEntryPoint;
	vector m_vHoldPoint;
	WorldTimestamp m_StartTime;
	WorldTimestamp m_StateStartTime;
	bool m_bCrewBoarded;
	int m_iMaxCrewSeen;
	AIWaypoint m_TrackedWaypoint;
	WorldTimestamp m_WaypointStartTime;
}
