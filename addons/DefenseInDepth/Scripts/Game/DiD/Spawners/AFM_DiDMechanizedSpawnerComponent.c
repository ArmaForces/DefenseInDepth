//------------------------------------------------------------------------------------------------
//! Mechanized spawner — spawns vehicle groups with crew.
//! Supports both legacy (self-ticking Process) and director (ScoreRequest/TriggerSpawn) modes.
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
		PrintFormat("AFM_DiDMechanizedSpawnerComponent: Mechanized spawner initialized", LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Director mode: score how desirable a mechanized push is right now.
	//!
	//! Scoring logic:
	//!   Base = 0.1 (armor is a special-occasion option, not the default)
	//!   +2.0  if phase == FINAL                        (commit remaining budget)
	//!   +1.0  if budget ratio > 0.6                   (still have plenty — escalate)
	//!   +1.5  if defenders > 5                        (many defenders = armor-favourable)
	//!   -0.1  if m_bRequireMinAI and threshold not met (precondition not satisfied)
	//------------------------------------------------------------------------------------------------
	override float ScoreRequest(AFM_DiDBattlefieldState state)
	{
		// In PROBE phase armor is off the table — attacker is still scouting
		if (state.m_ePhase == EAFMAttackPhase.PROBE)
			return 0;

		float score = 0.1;

		// Final phase: commit whatever is left, armor is a force multiplier
		if (state.m_ePhase == EAFMAttackPhase.FINAL)
			score += 2.0;

		// Plenty of budget remaining — escalate with armor
		if (state.m_fBudgetRatio > 0.6)
			score += 1.0;

		// More defenders = more value from a vehicle push
		if (state.m_iDefenderCount > 5)
			score += 1.5;

		// Precondition check: require minimum infantry in zone
		if (m_bRequireMinAI && state.m_iTotalActiveAI < m_iMinAIThreshold)
			score -= 0.1;

		// Deadlock: zone stalled for 2+ ticks — armor is a good escalation option here
		// (unlike infantry, mechanized gets a smaller penalty since it may break the stall)
		if (state.m_iZoneStallTicks >= 2)
			score -= 0.5;

		return score;
	}

	//------------------------------------------------------------------------------------------------
	override protected void SpawnWave(int count)
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

		PrintFormat("AFM_DiDMechanizedSpawnerComponent: Spawning %1 vehicle(s)", count, LogLevel.DEBUG);

		for (int i = 0; i < count; i++)
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
	//! m_iPointCostPerUnit represents cost per whole vehicle group (vehicle + crew).
	//! Spawns vanilla waypoints at each route marker position and chains them:
	//! approach → suppress (overwatch) → zone assault (attack waypoint).
	//------------------------------------------------------------------------------------------------
	override protected void SpawnSingleGroup()
	{
		if (m_aSpawnPoints.Count() == 0 || m_Zone.GetApproachRoutes().IsEmpty() || m_aVehiclePrefabs.Count() == 0)
			return;

		if (!m_crewConfig)
			return;

		AFM_DiDAttackerDirector director = m_Zone.GetDirector();
		if (!director)
			return;

		// Budget check — cost is per vehicle, deducted immediately on successful spawn
		if (!CanSpendBudget(m_iPointCostPerUnit))
			return;

		AFM_DiDApproachRoute route = SelectRoute();
		if (!route)
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

		// Spawn approach waypoint first — SpawnCrew receives it as the crew's initial order
		// and adds it internally, so we must not call AddWaypoint on it again.
		ref array<IEntity> routeWPs = {};
		SCR_AIWaypoint approachWP = director.SpawnMoveWaypointAt(route.m_ApproachPoint.GetOrigin());
		if (approachWP)
			routeWPs.Insert(approachWP);

		AIGroup crew = m_crewConfig.SpawnCrew(cm, approachWP);
		if (!crew)
			return;

		ApplyAutonomousDistance(crew);

		if (route.m_VehicleOverwatch)
		{
			SCR_AIWaypoint suppressWP = director.SpawnSuppressWaypointAt(route.m_VehicleOverwatch.GetOrigin());
			if (suppressWP) { routeWPs.Insert(suppressWP); crew.AddWaypoint(suppressWP); }
		}

		AFM_ZoneAssaultWaypointEntity assaultMarker = m_Zone.GetAssaultWaypoint();
		if (assaultMarker)
		{
			SCR_AIWaypoint assaultWP = director.SpawnAssaultWaypointAt(assaultMarker.GetOrigin());
			if (assaultWP) { routeWPs.Insert(assaultWP); crew.AddWaypoint(assaultWP); }
		}

		director.RegisterGroup(crew, m_Zone, route, EAFMUnitType.MECHANIZED, routeWPs);

		// Consume budget immediately — vehicle spawned, cost is committed
		if (m_Zone && m_Zone.GetBudget())
			m_Zone.GetBudget().Consume(m_iPointCostPerUnit);
	}
}
