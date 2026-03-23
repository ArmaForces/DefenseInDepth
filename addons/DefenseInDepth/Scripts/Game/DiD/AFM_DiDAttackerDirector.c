//------------------------------------------------------------------------------------------------
//! Central decision-maker for the attacker side — shared across all zones in a stage.
//!
//! Placed as a GenericEntity child of the stage entity in the world editor.
//! Owns spawner entities and an optional AFM_DiDStageArtillery entity as direct children.
//! Coordinates all attackers on each decision cycle.
//!
//! Decision cycle (every m_iDecisionIntervalSeconds):
//!   1. Build battlefield state snapshot (aggregate defender/AI counts, stage budget ratio, phase)
//!   2. Ask each spawner for a score via ScoreRequest()
//!   3. Filter: score > 0, spawner off cooldown, budget covers cost
//!   4. Weighted random pick from top 3 candidates (scores as weights)
//!   5. Trigger the chosen spawner via TriggerSpawn()
//!   6. Independently evaluate and trigger artillery missions / respawns
//!
//! Hierarchy in world editor:
//!   AFM_DiDStage (GenericEntity)
//!   ├── AFM_PlayerSpawnPointEntity
//!   ├── AFM_DiDAttackerDirector             ← this entity
//!   │   ├── AFM_DiDInfantrySpawnerComponent  ← spawner children
//!   │   ├── AFM_DiDMechanizedSpawnerComponent
//!   │   └── AFM_DiDStageArtillery           ← optional artillery child
//!   │       └── AFM_ArtillerySpawnPointEntity
//!   └── Zone entity (1..N)
//!       ├── AFM_DiDZoneComponent [ScriptComponent]
//!       ├── PolylineShapeEntity
//!       ├── AFM_ZoneAssaultWaypointEntity
//!       └── AFM_ApproachEntity
//------------------------------------------------------------------------------------------------
class AFM_DiDAttackerDirectorClass: GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDAttackerDirector: GenericEntity
{
	[Attribute("25", UIWidgets.EditBox, "Decision cycle interval in seconds", category: "DiD Director")]
	protected int m_iDecisionIntervalSeconds;

	[Attribute("0.75", UIWidgets.EditBox, "Aggression 0.0-1.0: scales score of costly options (0=conservative, 1=reckless)", category: "DiD Director")]
	protected float m_fAggression;

	protected AFM_DiDStage m_pStage;
	protected ref array<AFM_DiDSpawnerComponent> m_aSpawners = {};
	protected AFM_DiDStageArtillery m_pArtillery;
	protected WorldTimestamp m_fLastDecisionTime;
	protected bool m_bInitialized = false;
	protected ref array<ref AFM_DiDGroupEntry> m_aGroupRegistry = {};
	protected int m_iDecisionTick = 0;

	//------------------------------------------------------------------------------------------------
	//! Called by AFM_DiDStage.LateInit() after all zone children are collected.
	//! Links this director to its stage; scans own children for spawners and artillery.
	//! Spawners are prepared against the first zone (single-zone stages) or primary zone
	//! (multi-zone stages — future extension point).
	void Init(AFM_DiDStage stage)
	{
		m_pStage = stage;

		array<AFM_DiDZoneComponent> zones = stage.GetZones();
		// Primary zone used as the spawner context (approach routes, assault waypoint, etc.)
		AFM_DiDZoneComponent primaryZone = null;
		if (!zones.IsEmpty())
			primaryZone = zones[0];

		// Scan own children: spawners + optional artillery
		IEntity child = GetChildren();
		while (child)
		{
			AFM_DiDStageArtillery artillery = AFM_DiDStageArtillery.Cast(child);
			if (artillery)
			{
				m_pArtillery = artillery;
				child = child.GetSibling();
				continue;
			}

			AFM_DiDSpawnerComponent spawner = AFM_DiDSpawnerComponent.Cast(child);
			if (spawner && primaryZone)
			{
				m_aSpawners.Insert(spawner);
				spawner.Prepare(primaryZone);
			}

			child = child.GetSibling();
		}

		// Artillery: spawn points are own children of the artillery entity
		if (m_pArtillery && primaryZone)
		{
			m_pArtillery.Initialize(primaryZone);
			PrintFormat("AFM_DiDAttackerDirector: Artillery initialized");
		}

		if (m_aSpawners.Count() == 0)
			PrintFormat("AFM_DiDAttackerDirector: No spawner children found!", level: LogLevel.WARNING);

		// Push last decision back by one interval so the first cycle fires immediately
		ChimeraWorld world = GetGame().GetWorld();
		m_fLastDecisionTime = world.GetServerTimestamp().PlusSeconds(-m_iDecisionIntervalSeconds);
		m_bInitialized = true;

		PrintFormat("AFM_DiDAttackerDirector: Initialized with %1 spawner(s), %2 zone(s), decision every %3s, artillery=%4",
			m_aSpawners.Count(), zones.Count(), m_iDecisionIntervalSeconds, m_pArtillery != null);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by AFM_DiDZoneComponent.HandleActiveZoneLogic() once per second.
	//! Checks artillery health every call; runs full decision cycle on configured interval.
	void Process()
	{
		if (!m_bInitialized || !m_pStage)
			return;

		// First active tick: spawn the mortar now that this zone is actually running
		if (m_pArtillery && m_pArtillery.IsInitialSpawnPending())
			m_pArtillery.TriggerInitialSpawn();

		// Artillery alive check runs every second for responsive destruction detection
		if (m_pArtillery)
			m_pArtillery.CheckMortarAlive();

		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();

		int timeSinceDecision = Math.AbsInt(now.DiffSeconds(m_fLastDecisionTime));
		if (timeSinceDecision < m_iDecisionIntervalSeconds)
			return;

		m_fLastDecisionTime = now;
		RunDecisionCycle(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a freshly spawned group in the central registry.
	//! Called by spawner components immediately after a successful spawn.
	//! routeWaypoints: dynamically spawned vanilla waypoints assigned to this group (staging,
	//! approach, assault). Stored in the entry and cleaned up when the group goes idle or is removed.
	void RegisterGroup(AIGroup group, AFM_DiDZoneComponent zone, AFM_DiDApproachRoute route, EAFMUnitType unitType, array<IEntity> routeWaypoints = null)
	{
		if (!group)
			return;

		AFM_DiDGroupEntry entry = new AFM_DiDGroupEntry();
		entry.m_Group = group;
		entry.m_AssignedZone = zone;
		entry.m_AssignedRoute = route;
		entry.m_eUnitType = unitType;
		entry.m_iSpawnTick = m_iDecisionTick;
		// Agents are not yet available at spawn time — alive count is initialized 600ms later
		// by InitGroupAliveCount(), after the engine has populated the group.
		entry.m_iAliveCount = 0;

		if (routeWaypoints)
		{
			foreach (IEntity wp : routeWaypoints)
				entry.m_aDynamicWaypoints.Insert(wp);
		}

		m_aGroupRegistry.Insert(entry);

		// Defer alive-count initialization until after the engine spawns the agents (~500ms)
		GetGame().GetCallqueue().CallLater(InitGroupAliveCount, 600, false, entry);

		SCR_AIGroup scrGroup = SCR_AIGroup.Cast(group);
		if (scrGroup)
		{
			SCR_AIGroupUtilityComponent utility = scrGroup.GetGroupUtilityComponent();
			if (utility)
			{
				// Agent death tracking — signature: (AIAgent, SCR_AIInfoComponent, IEntity, ECharacterLifeState)
				utility.m_OnAgentLifeStateChanged.Insert(OnAgentLifeStateChanged);

				// Move failure tracking — signature: (int moveResult, IEntity vehicleUsed, bool isWaypointRelated, vector moveLocation)
				utility.GetOnMoveFailed().Insert(OnMoveFailed);

				// Control mode tracking — parameterless, poll mode in callback
				if (utility.m_GroupInfo)
					utility.m_GroupInfo.GetOnControlModeChanged().Insert(OnGroupControlModeChanged);
			}
		}

		PrintFormat("AFM_DiDAttackerDirector: Registered %1 group — tick %2, registry size %3 (alive count pending)",
			typename.EnumToString(EAFMUnitType, unitType),
			m_iDecisionTick, m_aGroupRegistry.Count(), LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Deferred initialization called 600ms after RegisterGroup().
	//! By this point the engine has populated the group with its agents.
	protected void InitGroupAliveCount(AFM_DiDGroupEntry entry)
	{
		if (!entry || !entry.m_Group || entry.m_bPendingRemoval)
			return;

		entry.m_iAliveCount = entry.m_Group.GetAgentsCount();
		PrintFormat("AFM_DiDAttackerDirector: Group alive count initialized — %1 agents", entry.m_iAliveCount, LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns total active AI count: registry alive counts + mortar crew.
	int GetActiveAICount()
	{
		int count = 0;
		foreach (AFM_DiDGroupEntry entry : m_aGroupRegistry)
		{
			if (entry && entry.m_Group)
				count += entry.m_iAliveCount;
		}
		if (m_pArtillery)
			count += m_pArtillery.GetCrewCount();
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns whether all spawners exhausted their tickets (stage budget exhausted)
	bool AreTicketsExhausted()
	{
		if (!m_pStage)
			return true;
		AFM_DiDAttackerBudget budget = m_pStage.GetBudget();
		return budget != null && budget.IsExhausted();
	}

	//------------------------------------------------------------------------------------------------
	//! Cleans up all AI tracked by the registry and the mortar.
	void Cleanup()
	{
		foreach (AFM_DiDGroupEntry entry : m_aGroupRegistry)
		{
			if (!entry || !entry.m_Group)
				continue;

			// Unsubscribe both events before deleting to avoid callbacks firing on dead entries
			SCR_AIGroup scrGroup = SCR_AIGroup.Cast(entry.m_Group);
			if (scrGroup)
			{
				SCR_AIGroupUtilityComponent utility = scrGroup.GetGroupUtilityComponent();
				if (utility)
				{
					utility.m_OnAgentLifeStateChanged.Remove(OnAgentLifeStateChanged);
					utility.GetOnMoveFailed().Remove(OnMoveFailed);
					if (utility.m_GroupInfo)
						utility.m_GroupInfo.GetOnControlModeChanged().Remove(OnGroupControlModeChanged);
				}
			}

			ClearDynamicWaypoints(entry);

			array<AIAgent> agents = {};
			entry.m_Group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;
				IEntity ent = agent.GetControlledEntity();
				if (ent)
					SCR_EntityHelper.DeleteEntityAndChildren(ent);
			}
		}
		m_aGroupRegistry.Clear();

		if (m_pArtillery)
			m_pArtillery.Cleanup();
	}

	//------------------------------------------------------------------------------------------------
	//! Fired by SCR_AIGroupUtilityComponent.m_OnAgentLifeStateChanged when any agent's life state changes.
	//! Decrements alive count on the matching registry entry; flags entry for removal when count hits 0.
	protected void OnAgentLifeStateChanged(AIAgent agent, SCR_AIInfoComponent info, IEntity vehicle, ECharacterLifeState lifeState)
	{
		if (lifeState != ECharacterLifeState.DEAD)
			return;

		AIGroup parentGroup = agent.GetParentGroup();
		if (!parentGroup)
			return;

		foreach (AFM_DiDGroupEntry entry : m_aGroupRegistry)
		{
			if (!entry || entry.m_Group != parentGroup || entry.m_bPendingRemoval)
				continue;

			entry.m_iAliveCount--;
			PrintFormat("AFM_DiDAttackerDirector: Agent died — group alive count now %1", entry.m_iAliveCount, LogLevel.DEBUG);

			if (entry.m_iAliveCount <= 0)
			{
				entry.m_bPendingRemoval = true;
				if (entry.m_AssignedRoute)
					RecordRouteWipe(entry.m_AssignedRoute);
				PrintFormat("AFM_DiDAttackerDirector: Group wiped — flagged for removal", LogLevel.DEBUG);
			}

			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fired by SCR_AIGroupUtilityComponent.GetOnMoveFailed() when a group cannot reach a waypoint.
	//! Uses the failure position to identify the route and records a wipe even if agents survived.
	//! Signature: (int moveResult, IEntity vehicleUsed, bool isWaypointRelated, vector moveLocation)
	protected void OnMoveFailed(int moveResult, IEntity vehicleUsed, bool isWaypointRelated, vector moveLocation)
	{
		if (!isWaypointRelated)
			return;

		// Identify the route by proximity: find the entry whose approach point is nearest
		// to the failed move location (within 100m). Not exact but sufficient for pressure tracking.
		AFM_DiDGroupEntry closest;
		float closestDist = 100;

		foreach (AFM_DiDGroupEntry entry : m_aGroupRegistry)
		{
			if (!entry || !entry.m_AssignedRoute || entry.m_bPendingRemoval)
				continue;

			float dist = vector.Distance(moveLocation, entry.m_AssignedRoute.m_ApproachPoint.GetOrigin());
			if (dist < closestDist)
			{
				closestDist = dist;
				closest = entry;
			}
		}

		if (closest)
		{
			PrintFormat("AFM_DiDAttackerDirector: Move failed near approach route (dist=%1m) — recording route wipe", closestDist, LogLevel.DEBUG);
			RecordRouteWipe(closest.m_AssignedRoute);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Records a wipe event on a route: increments wipe counter, sets cooldown and decay timer.
	//! Cooldown length scales with aggression (high aggression = shorter cooldown).
	protected void RecordRouteWipe(AFM_DiDApproachRoute route)
	{
		route.m_iGroupsWiped++;
		route.m_iWipeDecayTicksRemaining = 10;
		route.m_iCooldownTicksRemaining = Math.Round(Math.Lerp(4, 1, m_fAggression));
		PrintFormat("AFM_DiDAttackerDirector: Route wipe recorded — wiped=%1, cooldown=%2 ticks",
			route.m_iGroupsWiped, route.m_iCooldownTicksRemaining, LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Fired by SCR_AIGroupInfoComponent.GetOnControlModeChanged() for every registered group.
	//! Does not rely on invoker parameters — polls current mode from each registry entry instead.
	protected void OnGroupControlModeChanged()
	{
		foreach (AFM_DiDGroupEntry entry : m_aGroupRegistry)
		{
			if (!entry || !entry.m_Group)
				continue;

			SCR_AIGroup scrGroup = SCR_AIGroup.Cast(entry.m_Group);
			if (!scrGroup)
				continue;

			SCR_AIGroupUtilityComponent utility = scrGroup.GetGroupUtilityComponent();
			if (!utility || !utility.m_GroupInfo)
				continue;

			EGroupControlMode mode = utility.m_GroupInfo.GetGroupControlMode();
			if (mode == EGroupControlMode.IDLE)
			{
				PrintFormat("AFM_DiDAttackerDirector: Group %1 mode → IDLE", entry.m_Group, LogLevel.DEBUG);
				HandleIdleGroup(entry);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called when a registered group transitions to IDLE (no waypoints, not engaging).
	//! Three branches:
	//!   1. Defenders present in zone  → issue patrol waypoints inside zone polygon
	//!   2. Defenders gone, not captured → issue sweep waypoint at zone centroid
	//!   3. Zone captured               → relocate to undercovered zone (stub until E2)
	protected void HandleIdleGroup(AFM_DiDGroupEntry entry)
	{
		AFM_DiDZoneComponent zone = entry.m_AssignedZone;
		if (!zone || !entry.m_Group)
			return;

		// Grace period: newly spawned groups are in IDLE state until the engine processes
		// their initial waypoints (next tick). Ignore IDLE events for 2 ticks after spawn
		// to avoid overwriting the attack plan before the group has started moving.
		if (m_iDecisionTick - entry.m_iSpawnTick < 2)
			return;

		// Clear any previously issued dynamic waypoints before assigning new ones
		ClearDynamicWaypoints(entry);

		if (zone.GetDefenderCount() > 0)
		{
			PrintFormat("AFM_DiDAttackerDirector: IDLE group — defenders present, issuing patrol", LogLevel.DEBUG);
			IssuePatrolWaypoints(entry);
		}
		else if (!zone.IsZoneCaptured())
		{
			PrintFormat("AFM_DiDAttackerDirector: IDLE group — zone not captured, issuing sweep", LogLevel.DEBUG);
			IssueSweepWaypoint(entry);
		}
		else
		{
			AFM_DiDZoneComponent target = FindUndercoveredZone();
			if (target)
			{
				PrintFormat("AFM_DiDAttackerDirector: IDLE group — zone captured, relocating to zone %1", target.GetZoneName(), LogLevel.DEBUG);
				AFM_ZoneAssaultWaypointEntity assaultMarker = target.GetAssaultWaypoint();
				if (assaultMarker)
				{
					SCR_AIWaypoint assaultWP = SpawnAssaultWaypointAt(assaultMarker.GetOrigin());
					if (assaultWP)
					{
						entry.m_aDynamicWaypoints.Insert(assaultWP);
						entry.m_Group.AddWaypoint(assaultWP);
					}
				}
				entry.m_AssignedZone = target;
				entry.m_AssignedRoute = null;
			}
			else
			{
				PrintFormat("AFM_DiDAttackerDirector: IDLE group — all zones finished or no undercovered zone found, no relocation", LogLevel.DEBUG);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Issues 2-3 random patrol waypoints inside the zone polygon and assigns them to the group.
	protected void IssuePatrolWaypoints(AFM_DiDGroupEntry entry)
	{
		AFM_DiDZoneComponent zone = entry.m_AssignedZone;
		int count = 2 + s_AIRandomGenerator.RandInt(0, 1); // 2 or 3

		for (int i = 0; i < count; i++)
		{
			vector pos = zone.GetRandomPointInZone();
			SCR_AIWaypoint wp = SpawnMoveWaypointAt(pos);
			if (!wp)
				return;
			entry.m_aDynamicWaypoints.Insert(wp);
			entry.m_Group.AddWaypoint(wp);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Issues a single sweep waypoint at the zone centroid.
	protected void IssueSweepWaypoint(AFM_DiDGroupEntry entry)
	{
		vector centroid = entry.m_AssignedZone.GetZoneCentroid();
		SCR_AIWaypoint wp = SpawnMoveWaypointAt(centroid);
		if (!wp)
			return;
		entry.m_aDynamicWaypoints.Insert(wp);
		entry.m_Group.AddWaypoint(wp);
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes all dynamically spawned waypoints for this entry and clears the list.
	protected void ClearDynamicWaypoints(AFM_DiDGroupEntry entry)
	{
		foreach (IEntity wp : entry.m_aDynamicWaypoints)
		{
			if (wp)
				SCR_EntityHelper.DeleteEntityAndChildren(wp);
		}
		entry.m_aDynamicWaypoints.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a Move waypoint prefab at pos. Called by spawners for staging/approach positions
	//! and by the director for patrol/sweep orders.
	SCR_AIWaypoint SpawnMoveWaypointAt(vector pos)
	{
		AFM_DiDCommanderConfig config = AFM_DiDCommanderConfig.GetInstance();
		if (!config)
			return null;
		return SpawnWaypointAt(pos, config.m_sMoveWaypointPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns an Attack waypoint prefab at pos. Used for zone assault objectives.
	SCR_AIWaypoint SpawnAssaultWaypointAt(vector pos)
	{
		AFM_DiDCommanderConfig config = AFM_DiDCommanderConfig.GetInstance();
		if (!config)
			return null;
		return SpawnWaypointAt(pos, config.m_sAssaultWaypointPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a Suppress waypoint prefab at pos. Used for mechanized vehicle overwatch positions.
	SCR_AIWaypoint SpawnSuppressWaypointAt(vector pos)
	{
		AFM_DiDCommanderConfig config = AFM_DiDCommanderConfig.GetInstance();
		if (!config)
			return null;
		return SpawnWaypointAt(pos, config.m_sSuppressWaypointPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Core waypoint spawn helper — spawns a prefab at pos and returns it as SCR_AIWaypoint.
	//! Returns null and logs a warning if prefab is empty or spawn fails.
	protected SCR_AIWaypoint SpawnWaypointAt(vector pos, ResourceName prefab)
	{
		if (prefab == string.Empty)
		{
			PrintFormat("AFM_DiDAttackerDirector: Waypoint prefab not configured in AFM_DiDCommanderConfig!", LogLevel.WARNING);
			return null;
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		Math3D.MatrixIdentity4(spawnParams.Transform);
		spawnParams.Transform[3] = pos;

		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		return SCR_AIWaypoint.Cast(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the active (non-finished) zone in the stage with the lowest
	//! (groupsAssigned / max(1, defenderCount)) ratio.
	//! Used to redirect idle groups after their assigned zone is captured.
	//! Returns null when all zones are finished or the stage has only one zone.
	protected AFM_DiDZoneComponent FindUndercoveredZone()
	{
		if (!m_pStage)
			return null;

		AFM_DiDZoneComponent bestZone;
		float bestRatio = 999.0;

		foreach (AFM_DiDZoneComponent zone : m_pStage.GetZones())
		{
			if (zone.IsZoneFinished())
				continue;

			// Count groups currently assigned to this zone (en route or engaging)
			int groupsAssigned = 0;
			foreach (AFM_DiDGroupEntry entry : m_aGroupRegistry)
			{
				if (entry && !entry.m_bPendingRemoval && entry.m_AssignedZone == zone)
					groupsAssigned++;
			}

			int defenders = zone.GetDefenderCount();
			float ratio = groupsAssigned / Math.Max(1.0, defenders);
			if (ratio < bestRatio)
			{
				bestRatio = ratio;
				bestZone = zone;
			}
		}

		return bestZone;
	}

	//------------------------------------------------------------------------------------------------
	//! Ticks route pressure counters: decrements cooldowns and wipe-decay timers.
	//! When a decay timer reaches zero the wipe count is halved (gradual forgetting).
	//! Called once per decision cycle before any spawn logic runs.
	protected void UpdateRoutePressure()
	{
		if (!m_pStage)
			return;

		foreach (AFM_DiDZoneComponent zone : m_pStage.GetZones())
		{
			foreach (AFM_DiDApproachRoute route : zone.GetApproachRoutes())
			{
				if (route.m_iCooldownTicksRemaining > 0)
					route.m_iCooldownTicksRemaining--;

				if (route.m_iWipeDecayTicksRemaining > 0)
				{
					route.m_iWipeDecayTicksRemaining--;
					if (route.m_iWipeDecayTicksRemaining == 0)
					{
						route.m_iGroupsWiped = Math.Max(0, route.m_iGroupsWiped / 2);
						PrintFormat("AFM_DiDAttackerDirector: Route wipe decay — wiped count halved to %1", route.m_iGroupsWiped, LogLevel.DEBUG);
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDecisionCycle(WorldTimestamp now)
	{
		m_iDecisionTick++;

		// Tick route pressure before any spawn decisions
		UpdateRoutePressure();

		// Remove wiped groups flagged by OnAgentLifeStateChanged — iterate in reverse to allow safe removal
		for (int i = m_aGroupRegistry.Count() - 1; i >= 0; i--)
		{
			AFM_DiDGroupEntry entry = m_aGroupRegistry[i];
			if (entry && entry.m_bPendingRemoval)
			{
				ClearDynamicWaypoints(entry);
				m_aGroupRegistry.Remove(i);
				PrintFormat("AFM_DiDAttackerDirector: Removed wiped group from registry (size now %1)", m_aGroupRegistry.Count(), LogLevel.DEBUG);
			}
		}

		AFM_DiDBattlefieldState state = BuildBattlefieldState(now);

		// --- Package path (takes priority over single-spawner pick during stalls) ---
		if (ShouldIssuePackage(state))
		{
			AFM_DiDAssaultPackage package = BuildBestPackage(state, now);
			AFM_DiDAttackerBudget packageBudget = m_pStage.GetBudget();
			if (package && (!packageBudget || packageBudget.CanAfford(package.m_iTotalCost)))
			{
				ExecutePackage(package, state, now);
				return;
			}
		}

		// --- Spawner selection ---
		ref array<ref AFM_DiDSpawnRequest> candidates = {};

		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner || !spawner.CanSpawnNow(now))
				continue;

			float score = spawner.ScoreRequest(state);
			if (score <= 0)
				continue;

			int cost = spawner.GetPointCostPerUnit();

			// Skip if budget can't cover this spawn
			AFM_DiDAttackerBudget budget = m_pStage.GetBudget();
			if (budget && !budget.CanAfford(cost))
				continue;

			// Aggression multiplier: higher aggression boosts the score of expensive options
			if (cost > 1 && m_fAggression > 0)
				score = score * (1.0 + (cost - 1) * m_fAggression * 0.1);

			candidates.Insert(new AFM_DiDSpawnRequest(spawner, score, cost));
		}

		if (candidates.Count() > 0)
		{
			ref AFM_DiDSpawnRequest chosen = WeightedRandomPick(candidates);
			if (chosen)
			{
				int groupCount = ComputeGroupCount(chosen, state);
				PrintFormat("AFM_DiDAttackerDirector: Triggering %1 x%2 groups (score=%3, cost=%4pts, phase=%5)",
					chosen.m_Spawner.Type().ToString(), groupCount, chosen.m_fScore, chosen.m_iCost,
					state.m_ePhase, level: LogLevel.NORMAL);
				chosen.m_Spawner.TriggerSpawn(now, groupCount);
			}
		}
		else
		{
			PrintFormat("AFM_DiDAttackerDirector: No viable spawner candidates (phase=%1, budget=%2%%)",
				state.m_ePhase, state.m_fBudgetRatio * 100, level: LogLevel.WARNING);
		}

		// --- Artillery evaluation (independent of spawner selection) ---
		if (!m_pArtillery)
			return;

		if (m_pArtillery.IsMortarActive() && m_pArtillery.IsReadyForMission(now))
		{
			float artScore = m_pArtillery.ScoreArtilleryMission(state);
			if (artScore > 0)
				m_pArtillery.TriggerMission(state, now);
		}
		else if (!m_pArtillery.IsMortarActive() && m_pArtillery.CanRespawn())
		{
			float respawnScore = m_pArtillery.ScoreRespawn(state);
			if (respawnScore > 0)
			{
				AFM_DiDAttackerBudget budget = m_pStage.GetBudget();
				int respawnCost = m_pArtillery.GetRespawnCost();
				if (!budget || budget.CanAfford(respawnCost))
				{
					if (budget)
						budget.Consume(respawnCost);
					m_pArtillery.TriggerRespawn(now);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Decide how many groups to spawn for the chosen request.
	//! Reads base count and variance from the spawner; battlefield state can adjust the result.
	protected int ComputeGroupCount(AFM_DiDSpawnRequest chosen, AFM_DiDBattlefieldState state)
	{
		int base = chosen.m_Spawner.GetSpawnCount();
		int variance = chosen.m_Spawner.GetSpawnCountVariance();

		int lo = Math.Max(1, base - variance);
		int hi = base + variance;
		int count = s_AIRandomGenerator.RandInt(lo, hi);

		// Final phase: send one extra group to commit remaining budget aggressively
		if (state.m_ePhase == EAFMAttackPhase.FINAL)
			count++;

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Sorts candidates descending by score and picks with weighted random from the top 3.
	protected ref AFM_DiDSpawnRequest WeightedRandomPick(array<ref AFM_DiDSpawnRequest> candidates)
	{
		// Insertion sort — descending by score (typically 2–4 entries)
		int count = candidates.Count();
		for (int i = 1; i < count; i++)
		{
			ref AFM_DiDSpawnRequest key = candidates[i];
			int j = i - 1;
			while (j >= 0 && candidates[j].m_fScore < key.m_fScore)
			{
				candidates[j + 1] = candidates[j];
				j--;
			}
			candidates[j + 1] = key;
		}

		// Weighted random selection from top 3
		int topN = Math.Min(3, count);
		float totalWeight = 0;
		for (int i = 0; i < topN; i++)
			totalWeight += candidates[i].m_fScore;

		if (totalWeight <= 0)
			return candidates[0];

		float r = s_AIRandomGenerator.RandFloat01() * totalWeight;
		float cumulative = 0;
		for (int i = 0; i < topN; i++)
		{
			cumulative += candidates[i].m_fScore;
			if (r <= cumulative)
				return candidates[i];
		}

		return candidates[0];
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when a combined-arms package should replace the normal single-spawner pick.
	//! Conditions: ASSAULT or FINAL phase, zone stalled for 2+ ticks, both spawner types affordable.
	protected bool ShouldIssuePackage(AFM_DiDBattlefieldState state)
	{
		if (state.m_ePhase == EAFMAttackPhase.PROBE)
			return false;

		if (state.m_iZoneStallTicks < 2)
			return false;

		AFM_DiDAttackerBudget budget = m_pStage.GetBudget();
		bool hasInfantry = false;
		bool hasMechanized = false;

		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner)
				continue;

			int cost = spawner.GetPointCostPerUnit();
			if (budget && !budget.CanAfford(cost))
				continue;

			if (AFM_DiDInfantrySpawnerComponent.Cast(spawner))
				hasInfantry = true;
			else if (AFM_DiDMechanizedSpawnerComponent.Cast(spawner))
				hasMechanized = true;

			if (hasInfantry && hasMechanized)
				return true;
		}

		return hasInfantry && hasMechanized;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the best combined-arms package for the current battlefield state.
	//! Spawners are picked randomly from eligible pools; route is picked randomly from all
	//! off-cooldown routes across active zones.
	//! Returns null when a complete package cannot be assembled.
	protected AFM_DiDAssaultPackage BuildBestPackage(AFM_DiDBattlefieldState state, WorldTimestamp now)
	{
		AFM_DiDAttackerBudget budget = m_pStage.GetBudget();

		// Collect eligible spawners into typed pools
		ref array<AFM_DiDInfantrySpawnerComponent> infantryPool = {};
		ref array<AFM_DiDMechanizedSpawnerComponent> mechanizedPool = {};

		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner || !spawner.CanSpawnNow(now))
				continue;

			if (budget && !budget.CanAfford(spawner.GetPointCostPerUnit()))
				continue;

			AFM_DiDInfantrySpawnerComponent inf = AFM_DiDInfantrySpawnerComponent.Cast(spawner);
			if (inf)
			{
				infantryPool.Insert(inf);
				continue;
			}

			AFM_DiDMechanizedSpawnerComponent mech = AFM_DiDMechanizedSpawnerComponent.Cast(spawner);
			if (mech)
				mechanizedPool.Insert(mech);
		}

		if (infantryPool.IsEmpty() || mechanizedPool.IsEmpty())
			return null;

		AFM_DiDInfantrySpawnerComponent infantrySpawner = infantryPool.GetRandomElement();
		AFM_DiDMechanizedSpawnerComponent mechanizedSpawner = mechanizedPool.GetRandomElement();

		// Collect all off-cooldown routes across active zones, then pick one at random
		ref array<AFM_DiDApproachRoute> routePool = {};
		foreach (AFM_DiDZoneComponent zone : m_pStage.GetZones())
		{
			if (zone.IsZoneFinished())
				continue;

			foreach (AFM_DiDApproachRoute r : zone.GetApproachRoutes())
			{
				if (r.m_iCooldownTicksRemaining <= 0)
					routePool.Insert(r);
			}
		}

		if (routePool.IsEmpty())
			return null;

		AFM_DiDApproachRoute route = routePool.GetRandomElement();

		return new AFM_DiDAssaultPackage(
			infantrySpawner,
			infantrySpawner.GetPointCostPerUnit(),
			route,
			route.m_fInfantryTravelTicks,
			mechanizedSpawner,
			mechanizedSpawner.GetPointCostPerUnit(),
			m_pArtillery,
			EAFMRoundType.SMOKE);
	}

	//------------------------------------------------------------------------------------------------
	//! Execute a combined-arms package: consume budget, fire artillery pre-assault, spawn infantry
	//! and mechanized. F3 replaces the mechanized spawn with a staggered-timing delayed call.
	protected void ExecutePackage(AFM_DiDAssaultPackage package, AFM_DiDBattlefieldState state, WorldTimestamp now)
	{
		PrintFormat("AFM_DiDAttackerDirector: Issuing assault package (cost=%1 pts, artillery=%2)",
			package.m_iTotalCost, package.m_Artillery != null);

		AFM_DiDAttackerBudget budget = m_pStage.GetBudget();
		if (budget)
			budget.Consume(package.m_iTotalCost);

		if (package.m_Artillery && package.m_Artillery.IsMortarActive())
			package.m_Artillery.TriggerMission(state, now);

		package.m_InfantrySpawner.TriggerSpawn(now, 1);

		if (package.m_MechanizedSpawner)
			package.m_MechanizedSpawner.TriggerSpawn(now, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected AFM_DiDBattlefieldState BuildBattlefieldState(WorldTimestamp now)
	{
		AFM_DiDBattlefieldState state = new AFM_DiDBattlefieldState();

		// Aggregate defender and AI counts across all active zones in the stage
		array<AFM_DiDZoneComponent> zones = m_pStage.GetZones();
		AFM_DiDZoneComponent primaryZone;
		int totalDefenders = 0;
		int totalAIInZone = 0;
		float maxCaptureProgress = 0.0;
		int maxStallTicks = 0;

		foreach (AFM_DiDZoneComponent zone : zones)
		{
			if (zone.IsZoneFinished())
				continue;

			if (!primaryZone)
				primaryZone = zone;

			totalDefenders += zone.GetDefenderCount();
			totalAIInZone += zone.GetAICountInsideZone();

			float cp = zone.GetCaptureProgress();
			if (cp > maxCaptureProgress)
				maxCaptureProgress = cp;

			int st = zone.GetStallTicks();
			if (st > maxStallTicks)
				maxStallTicks = st;
		}

		state.m_iDefenderCount = totalDefenders;
		state.m_iAICountInZone = totalAIInZone;
		state.m_iTotalActiveAI = GetActiveAICount();
		state.m_fAliveDefenders = totalDefenders;
		state.m_fZoneCaptureProgress = maxCaptureProgress;
		state.m_iZoneStallTicks = maxStallTicks;

		AFM_DiDAttackerBudget budget = m_pStage.GetBudget();
		if (budget)
			state.m_fBudgetRatio = budget.GetRatio();
		else
			state.m_fBudgetRatio = 1.0;

		// Time ratio from primary zone — 1.0 at start, approaching 0.0 at deadline
		if (primaryZone)
		{
			WorldTimestamp zoneEnd = primaryZone.GetZoneEndTime();
			float secondsRemaining = Math.Max(0, zoneEnd.DiffSeconds(now));
			int totalDefenseSeconds = primaryZone.GetTotalDefenseSeconds();
			if (totalDefenseSeconds > 0)
				state.m_fTimeRatio = Math.Clamp(secondsRemaining / totalDefenseSeconds, 0.0, 1.0);
			else
				state.m_fTimeRatio = 1.0;
		}
		else
		{
			state.m_fTimeRatio = 1.0;
		}

		// Derive attack phase from budget ratio
		if (state.m_fBudgetRatio > 0.75)
			state.m_ePhase = EAFMAttackPhase.PROBE;
		else if (state.m_fBudgetRatio > 0.25)
			state.m_ePhase = EAFMAttackPhase.ASSAULT;
		else
			state.m_ePhase = EAFMAttackPhase.FINAL;

		// Day/night via TimeAndWeatherManagerEntity.IsSunSet()
		ChimeraWorld chimeraWorld = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (chimeraWorld)
		{
			TimeAndWeatherManagerEntity timeWeather = chimeraWorld.GetTimeAndWeatherManager();
			state.m_bIsNight = timeWeather && timeWeather.IsSunSet();
		}

		return state;
	}
}
