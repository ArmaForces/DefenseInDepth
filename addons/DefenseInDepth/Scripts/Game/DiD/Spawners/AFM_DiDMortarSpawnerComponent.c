//------------------------------------------------------------------------------------------------
//! Mortar fire support spawner - spawns a mortar team that shells the densest group of players,
//! walking its fire in over consecutive salvos
//------------------------------------------------------------------------------------------------
class AFM_DiDMortarSpawnerComponentClass: AFM_DiDSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDMortarSpawnerComponent: AFM_DiDSpawnerComponent
{
	// The attacking side's mortar composition and the crew that mans it
	protected ResourceName m_MortarPrefab;
	protected ref AFM_CrewConfig m_crewConfig;
	
	[Attribute("30", UIWidgets.EditBox, "Fire mission update interval (seconds)", category: "DiD Mortar Spawner")]
	protected int m_iFireMissionUpdateInterval;

	[Attribute("300", UIWidgets.EditBox, "Seconds before a destroyed mortar team is replaced. Silencing the mortars should be worth the trip out to them", category: "DiD Mortar Spawner")]
	protected int m_iRespawnDelaySeconds;
	
	[Attribute("10", UIWidgets.EditBox, "Attempts to find a random spot in the zone for harassing fire when no player can be targeted", category: "DiD Mortar Spawner")]
	protected int m_iMonteCarloSamples;

	[Attribute("40", UIWidgets.EditBox, "Players within this distance (meters) of each other count as one group; the mortar aims at the centre of the largest one", category: "DiD Mortar Spawner")]
	protected float m_fTargetGroupRadius;
	
	[Attribute("100", UIWidgets.EditBox, "Minimum distance from mortar to target (meters)", category: "DiD Mortar Spawner")]
	protected float m_fMinTargetDistance;
	
	[Attribute("800", UIWidgets.EditBox, "Maximum distance from mortar to target (meters)", category: "DiD Mortar Spawner")]
	protected float m_fMaxTargetDistance;
	
	[Attribute("60", UIWidgets.EditBox, "Scatter (meters) of the first salvo on a new target area. Rounds land between half and full scatter from the aim point", category: "DiD Mortar Accuracy")]
	protected float m_fInitialDispersion;

	[Attribute("12", UIWidgets.EditBox, "Scatter (meters) once fire has walked in. Rounds land anywhere within this radius", category: "DiD Mortar Accuracy")]
	protected float m_fMinDispersion;

	[Attribute("0.5", UIWidgets.EditBox, "Scatter multiplier for each consecutive salvo on the same target area", category: "DiD Mortar Accuracy")]
	protected float m_fDispersionStep;

	[Attribute("75", UIWidgets.EditBox, "Aim points closer than this (meters) to the previous aim point count as the same target area", category: "DiD Mortar Accuracy")]
	protected float m_fSameTargetRadius;

	[Attribute("30", UIWidgets.EditBox, "Rounds never land closer than this (meters) to attacker AI", category: "DiD Mortar Accuracy")]
	protected float m_fFriendlyFireRadius;

	// How many times to re-roll a scattered impact point that lands too close to attacker AI
	protected const int SCATTER_ATTEMPTS = 5;

	// Runtime data
	protected IEntity m_SpawnedMortar;
	protected ref map<IEntity, ref MortarFireMissionData> m_mFireMissions = new map<IEntity, ref MortarFireMissionData>();
	protected WorldTimestamp m_fLastTargetUpdate;

	// A mortar has been spawned this activation, and when the last one was lost
	protected bool m_bMortarSpawned;
	protected bool m_bMortarLost;
	protected WorldTimestamp m_fMortarLostAt;
	
	//------------------------------------------------------------------------------------------------
	override void Prepare(AFM_DiDZoneComponent owner)
	{
		super.Prepare(owner);
		
		ChimeraWorld world = GetGame().GetWorld();
		m_fLastTargetUpdate = world.GetServerTimestamp();
		
		PrintFormat("AFM_DiDMortarSpawnerComponent: Mortar spawner initialized, target group radius %1m",
			m_fTargetGroupRadius, level: LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	override protected void ResolveFactionContent()
	{
		AFM_DiDSideConfig side = GetAttackerConfig();
		if (!side)
			return;

		m_MortarPrefab = side.m_sMortarComposition;
		m_crewConfig = side.m_MortarCrew;

		// Not every side fields mortars, so this is worth saying without calling it an error
		if (m_MortarPrefab.IsEmpty() || !m_crewConfig)
			PrintFormat("AFM_DiDMortarSpawnerComponent: The attacking side (%1) has no mortar composition or no mortar crew, this spawner will do nothing",
				side.GetLabel(), level: LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	override void Process()
	{
		if (!m_Zone)
			return;
		
		// Only operate during ACTIVE state
		EAFMZoneState state = m_Zone.GetZoneState();
		if (state != EAFMZoneState.ACTIVE && state != EAFMZoneState.FROZEN)
			return;
		
		if (!m_SpawnedMortar)
		{
			if (m_bMortarSpawned)
				HandleMortarLost();
			else
				SpawnSingleGroup();
			
			return;
		}
		
		// Update fire missions periodically
		WorldTimestamp now = GetCurrentTimestamp();
		float secondsSinceUpdate = now.DiffSeconds(m_fLastTargetUpdate);
		if (secondsSinceUpdate < m_iFireMissionUpdateInterval)
			return;

		// Let the current salvo finish first, unless it has been stuck for a whole extra interval
		bool stuck = secondsSinceUpdate >= m_iFireMissionUpdateInterval * 2;
		if (!stuck && AnyMissionHasPendingShots())
			return;

		m_fLastTargetUpdate = now;
		UpdateAllFireMissions();
	}

	//------------------------------------------------------------------------------------------------
	override void Cleanup()
	{
		super.Cleanup();
		SCR_EntityHelper.DeleteEntityAndChildren(m_SpawnedMortar);
		m_SpawnedMortar = null;

		foreach (IEntity mortar, MortarFireMissionData fireMission : m_mFireMissions)
		{
			if (fireMission)
				ClearFireMissionWaypoints(fireMission);
		}
		m_mFireMissions.Clear();
		
		m_bMortarSpawned = false;
		m_bMortarLost = false;
	}

	//------------------------------------------------------------------------------------------------
	//! A destroyed mortar team is replaced only after a delay. Killing one used to buy the players about
	//! a second, which made going after them pointless.
	protected void HandleMortarLost()
	{
		WorldTimestamp now = GetCurrentTimestamp();
		
		if (!m_bMortarLost)
		{
			m_bMortarLost = true;
			m_fMortarLostAt = now;
			
			// The dead crew's salvo waypoints are nobody's now, and the map entry keyed on the destroyed
			// mortar would keep them alive until the zone ends
			DropLostFireMissions();
			
			PrintFormat("AFM_DiDMortarSpawnerComponent: Mortar lost, the next one arrives in %1 s", m_iRespawnDelaySeconds);
			return;
		}
		
		if (now.DiffSeconds(m_fMortarLostAt) < m_iRespawnDelaySeconds)
			return;
		
		m_bMortarLost = false;
		SpawnSingleGroup();
	}

	//------------------------------------------------------------------------------------------------
	//! Forget the fire missions of mortars that no longer exist, deleting the salvo waypoints they left
	protected void DropLostFireMissions()
	{
		ref map<IEntity, ref MortarFireMissionData> alive = new map<IEntity, ref MortarFireMissionData>();
		
		foreach (IEntity mortar, MortarFireMissionData fireMission : m_mFireMissions)
		{
			if (mortar)
			{
				alive.Set(mortar, fireMission);
				continue;
			}
			
			if (fireMission)
				ClearFireMissionWaypoints(fireMission);
		}
		
		m_mFireMissions = alive;
	}
	
	//------------------------------------------------------------------------------------------------
	override protected int GetSpawnCountForWave()
	{
		// Mortars spawn individually
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasSpawnWaves()
	{
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	override protected void SpawnSingleGroup()
	{
		if (m_aSpawnPoints.Count() == 0 || m_MortarPrefab.IsEmpty())
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: No spawn points or mortar prefabs configured!", level: LogLevel.WARNING);
			return;
		}

		if (!m_crewConfig)
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: No crew config defined!", level: LogLevel.ERROR);
			return;
		}
		
		// Spawn mortar vehicle
		AFM_SpawnPointEntity spawnPoint = m_aSpawnPoints.GetRandomElement();
		
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		vector mat[4];
		spawnPoint.GetWorldTransform(mat);
		spawnParams.Transform = mat;
		
		m_SpawnedMortar = GetGame().SpawnEntityPrefab(Resource.Load(m_MortarPrefab), GetGame().GetWorld(), spawnParams);
		if (!m_SpawnedMortar)
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: Failed to spawn mortar!", level: LogLevel.ERROR);
			return;
		}

		// Get compartment manager and crew the mortar
		SCR_BaseCompartmentManagerComponent cm = SCR_BaseCompartmentManagerComponent.Cast(
			m_SpawnedMortar.FindComponent(SCR_BaseCompartmentManagerComponent)
		);

		if (!cm)
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: Mortar has no compartment manager!", level: LogLevel.ERROR);
			return;
		}
		
		// Create initial fire mission data
		MortarFireMissionData fireMission = new MortarFireMissionData();
		fireMission.m_Mortar = m_SpawnedMortar;
		fireMission.m_SpawnPosition = m_SpawnedMortar.GetOrigin();
		
		// Crew the mortar (gunner only, no waypoint yet)
		AIGroup crew = m_crewConfig.SpawnCrew(cm, null);
		if (!crew)
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: Failed to spawn mortar crew!", level: LogLevel.ERROR);
			return;
		}

		fireMission.m_CrewGroup = crew;
		m_mFireMissions.Set(m_SpawnedMortar, fireMission);
		m_bMortarSpawned = true;

		// Create initial fire mission. Restart the update timer so it isn't immediately replaced on the next tick.
		UpdateFireMission(fireMission);
		m_fLastTargetUpdate = GetCurrentTimestamp();

		PrintFormat("AFM_DiDMortarSpawnerComponent: Spawned mortar at %1", m_SpawnedMortar.GetOrigin(), level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Update fire missions for all spawned mortars
	//------------------------------------------------------------------------------------------------
	protected void UpdateAllFireMissions()
	{
		foreach (IEntity mortar, MortarFireMissionData fireMission : m_mFireMissions)
		{
			if (mortar && fireMission)
				UpdateFireMission(fireMission);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True while any crew still has rounds of its current salvo to fire
	protected bool AnyMissionHasPendingShots()
	{
		foreach (IEntity mortar, MortarFireMissionData fireMission : m_mFireMissions)
		{
			if (mortar && fireMission && fireMission.m_CrewGroup && HasPendingShots(fireMission))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Plan a new salvo for a specific mortar, replacing any rounds
	//! not fired yet. Each round gets its own single-shot waypoint scattered around the aim point.
	//! Consecutive salvos on the same area walk in from m_fInitialDispersion towards m_fMinDispersion.
	//! \return true if a new salvo was assigned
	//------------------------------------------------------------------------------------------------
	protected bool UpdateFireMission(MortarFireMissionData fireMission)
	{
		if (!fireMission || !fireMission.m_Mortar || !fireMission.m_CrewGroup)
			return false;

		array<vector> attackerPositions = {};
		GetAttackerPositions(attackerPositions);

		// Aim at the densest group of players
		vector aimPoint = FindBestTargetPosition(fireMission.m_SpawnPosition, attackerPositions);

		if (aimPoint == vector.Zero)
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: No valid target found for mortar", level: LogLevel.DEBUG);
			return false;
		}

		// Walk fire in while the target stays in the same area, otherwise start bracketing again
		float dispersion = m_fInitialDispersion;
		if (fireMission.m_bHasAimPoint && vector.DistanceXZ(aimPoint, fireMission.m_TargetPosition) <= m_fSameTargetRadius)
			dispersion = Math.Max(m_fMinDispersion, fireMission.m_fDispersion * m_fDispersionStep);

		ClearFireMissionWaypoints(fireMission);

		//TODO: Add different fire mission types and mortar count
		int shotCount = s_AIRandomGenerator.RandInt(1, 6);
		for (int i = 0; i < shotCount; i++)
		{
			vector impactPoint;
			if (!GetScatteredImpactPoint(aimPoint, dispersion, attackerPositions, impactPoint))
				continue;

			SCR_AIWaypointArtillerySupport fireWaypoint = CreateFirePositionWaypoint(impactPoint, fireMission);
			if (!fireWaypoint)
			{
				PrintFormat("AFM_DiDMortarSpawnerComponent: Failed to create fire waypoint!", level: LogLevel.ERROR);
				continue;
			}

			fireWaypoint.SetTargetShotCount(1);
			fireMission.m_CrewGroup.AddWaypoint(fireWaypoint);
			fireMission.m_aWaypoints.Insert(fireWaypoint);
		}

		if (fireMission.m_aWaypoints.IsEmpty())
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: No safe impact point around %1, holding fire", aimPoint, level: LogLevel.DEBUG);
			return false;
		}

		fireMission.m_TargetPosition = aimPoint;
		fireMission.m_bHasAimPoint = true;
		fireMission.m_fDispersion = dispersion;
		fireMission.m_LastUpdateTime = GetCurrentTimestamp();

		PrintFormat("AFM_DiDMortarSpawnerComponent: New salvo of %1 rounds at %2 (%3 targets, %4m scatter)",
			fireMission.m_aWaypoints.Count(), aimPoint, fireMission.m_LastTargetCount, dispersion, level: LogLevel.DEBUG);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True while the crew still has rounds of the current salvo to fire
	protected bool HasPendingShots(notnull MortarFireMissionData fireMission)
	{
		array<AIWaypoint> waypoints = {};
		fireMission.m_CrewGroup.GetWaypoints(waypoints);
		return !waypoints.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Remove all salvo waypoints from the crew and delete them, including ones already completed
	protected void ClearFireMissionWaypoints(notnull MortarFireMissionData fireMission)
	{
		if (fireMission.m_CrewGroup)
		{
			array<AIWaypoint> existingWaypoints = {};
			fireMission.m_CrewGroup.GetWaypoints(existingWaypoints);
			foreach (AIWaypoint wp : existingWaypoints)
			{
				fireMission.m_CrewGroup.RemoveWaypoint(wp);
			}
		}

		foreach (SCR_AIWaypointArtillerySupport wp : fireMission.m_aWaypoints)
		{
			if (wp)
				SCR_EntityHelper.DeleteEntityAndChildren(wp);
		}
		fireMission.m_aWaypoints.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Pick where a single round lands. Above m_fMinDispersion rounds land in a ring between half and full
	//! dispersion, so bracketing salvos are clear near misses; at m_fMinDispersion anywhere inside the circle.
	//! \return false if every attempt landed too close to attacker AI
	protected bool GetScatteredImpactPoint(vector aimPoint, float dispersion, notnull array<vector> attackerPositions, out vector impactPoint)
	{
		float minRadius = 0;
		if (dispersion > m_fMinDispersion)
			minRadius = dispersion * 0.5;

		for (int i = 0; i < SCATTER_ATTEMPTS; i++)
		{
			vector candidate = s_AIRandomGenerator.GenerateRandomPointInRadius(minRadius, dispersion, aimPoint);
			if (AFM_DiDTargetingHelper.IsNearAnyPosition(candidate, attackerPositions, m_fFriendlyFireRadius))
				continue;

			candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);
			impactPoint = candidate;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Positions of all living attacker characters, used to avoid shelling own troops
	protected void GetAttackerPositions(notnull array<vector> outPositions)
	{
		outPositions.Clear();
		if (!m_Zone)
			return;

		SCR_Faction attackerFaction = m_Zone.GetAttackerFaction();
		if (!attackerFaction)
			return;

		// Helicopter crews overhead shouldn't block shelling the ground below them
		AFM_DiDTargetingHelper.GetAIPositions(attackerFaction, outPositions, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Aim at the centre of the densest group of players, so consecutive salvos keep the same aim point
	//! and the bracketing can walk in. Falls back to a random point in the zone when no players qualify.
	//------------------------------------------------------------------------------------------------
	protected vector FindBestTargetPosition(vector mortarPos, notnull array<vector> attackerPositions)
	{
		if (!m_Zone)
			return vector.Zero;

		// Players the mortar is allowed to fire at
		array<vector> targets = {};
		SCR_Faction defenderFaction = m_Zone.GetDefenderFaction();
		if (defenderFaction)
		{
			array<vector> playerPositions = {};
			AFM_DiDTargetingHelper.GetPlayerPositions(defenderFaction, playerPositions);

			foreach (vector playerPos : playerPositions)
			{
				if (IsValidTargetPosition(playerPos, mortarPos, attackerPositions))
					targets.Insert(playerPos);
			}
		}

		int groupSize;
		vector groupCenter = AFM_DiDTargetingHelper.FindDensestGroupCenter(targets, m_fTargetGroupRadius, groupSize);
		SetLastTargetCount(mortarPos, groupSize);

		if (groupSize > 0)
		{
			groupCenter[1] = GetGame().GetWorld().GetSurfaceY(groupCenter[0], groupCenter[2]);

			// The centre of a spread out group can sit on top of own troops; then shell one of them directly
			if (IsValidTargetPosition(groupCenter, mortarPos, attackerPositions))
				return groupCenter;

			return targets[0];
		}

		return FindRandomTargetPosition(mortarPos, attackerPositions);
	}

	//------------------------------------------------------------------------------------------------
	//! Harassing fire when no player can be targeted
	protected vector FindRandomTargetPosition(vector mortarPos, notnull array<vector> attackerPositions)
	{
		array<vector> polylinePoints = {};
		vector polylineOrigin;
		if (!GetZonePolyline(polylinePoints, polylineOrigin))
			return vector.Zero;

		vector minBounds, maxBounds;
		CalculateZoneBounds(polylinePoints, minBounds, maxBounds, polylineOrigin);

		for (int i = 0; i < m_iMonteCarloSamples; i++)
		{
			vector samplePos = GenerateRandomPointInBounds(minBounds, maxBounds);

			if (IsValidTargetPosition(samplePos, mortarPos, attackerPositions))
				return samplePos;
		}

		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	//! Inside the zone, within the mortar's range and clear of own troops
	protected bool IsValidTargetPosition(vector pos, vector mortarPos, notnull array<vector> attackerPositions)
	{
		if (!m_Zone.IsPointInsideZone(pos))
			return false;

		float distToMortar = vector.DistanceXZ(mortarPos, pos);
		if (distToMortar < m_fMinTargetDistance || distToMortar > m_fMaxTargetDistance)
			return false;

		return !AFM_DiDTargetingHelper.IsNearAnyPosition(pos, attackerPositions, m_fFriendlyFireRadius);
	}

	//------------------------------------------------------------------------------------------------
	protected bool GetZonePolyline(notnull array<vector> outPoints, out vector outOrigin)
	{
		PolylineShapeEntity polyline = m_Zone.GetPolylineEntity();
		if (!polyline)
			return false;

		polyline.GetPointsPositions(outPoints);
		outOrigin = polyline.GetOrigin();
		return outPoints.Count() >= 3;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetLastTargetCount(vector mortarPos, int targetCount)
	{
		foreach (IEntity mortar, MortarFireMissionData fireMission : m_mFireMissions)
		{
			if (fireMission && fireMission.m_SpawnPosition == mortarPos)
			{
				fireMission.m_LastTargetCount = targetCount;
				return;
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Create fire position waypoint at target location
	//------------------------------------------------------------------------------------------------
	protected SCR_AIWaypointArtillerySupport CreateFirePositionWaypoint(vector targetPos, MortarFireMissionData fireMission)
	{
		Resource wpResource = Resource.Load("{C524700A27CFECDD}Prefabs/AI/Waypoints/AIWaypoint_ArtillerySupport.et");
		if (!wpResource || !wpResource.IsValid())
			return null;
		
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		targetPos[1] = GetGame().GetWorld().GetSurfaceY(targetPos[0], targetPos[2]);
		spawnParams.Transform[3] = targetPos;
		
		IEntity wpEntity = GetGame().SpawnEntityPrefab(wpResource, GetGame().GetWorld(), spawnParams);
		if (!wpEntity)
			return null;
		
		return SCR_AIWaypointArtillerySupport.Cast(wpEntity);
	}
	
	//------------------------------------------------------------------------------------------------
	// Helper methods
	//------------------------------------------------------------------------------------------------
	
	protected void CalculateZoneBounds(array<vector> points, out vector minBounds, out vector maxBounds, vector polylineOrigin)
	{
		minBounds = polylineOrigin + points[0];
		maxBounds = polylineOrigin + points[0];
		
		foreach (vector point : points)
		{
			minBounds[0] = Math.Min(minBounds[0], polylineOrigin[0] + point[0]);
			minBounds[2] = Math.Min(minBounds[2], polylineOrigin[2] + point[2]);
			maxBounds[0] = Math.Max(maxBounds[0], polylineOrigin[0] + point[0]);
			maxBounds[2] = Math.Max(maxBounds[2], polylineOrigin[2] + point[2]);
		}
	}
	
	protected vector GenerateRandomPointInBounds(vector minBounds, vector maxBounds)
	{
		vector point;
		point[0] = s_AIRandomGenerator.RandFloatXY(minBounds[0], maxBounds[0]);
		point[2] = s_AIRandomGenerator.RandFloatXY(minBounds[2], maxBounds[2]);
		point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
		return point;
	}
	
}

//------------------------------------------------------------------------------------------------
//! Data container for mortar fire mission tracking
//------------------------------------------------------------------------------------------------
class MortarFireMissionData
{
	IEntity m_Mortar;
	AIGroup m_CrewGroup;
	ref array<SCR_AIWaypointArtillerySupport> m_aWaypoints = {};	// One single-shot waypoint per round of the current salvo
	vector m_SpawnPosition;
	vector m_TargetPosition;		// Aim point of the current salvo, before scatter
	bool m_bHasAimPoint;
	float m_fDispersion;			// Scatter used for the current salvo
	WorldTimestamp m_LastUpdateTime;
	int m_LastTargetCount;
}
