//------------------------------------------------------------------------------------------------
//! Mortar fire support spawner - spawns mortar teams with intelligent target selection
//! Uses Monte Carlo sampling to find optimal fire positions within the zone
//------------------------------------------------------------------------------------------------
class AFM_DiDMortarSpawnerComponentClass: AFM_DiDSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDMortarSpawnerComponent: AFM_DiDSpawnerComponent
{
	[Attribute("", UIWidgets.Object, desc: "Crew configuration for mortar", category: "DiD Mortar Spawner")]
	protected ref AFM_CrewConfig m_crewConfig;
	
	[Attribute("", UIWidgets.Auto, desc: "Mortar vehicle prefabs to spawn", category: "DiD Mortar Spawner")]
	protected ResourceName m_MortarPrefab;
	
	[Attribute("30", UIWidgets.EditBox, "Fire mission update interval (seconds)", category: "DiD Mortar Spawner")]
	protected int m_iFireMissionUpdateInterval;
	
	[Attribute("10", UIWidgets.EditBox, "Number of sample points for Monte Carlo targeting (higher = more accurate, slower)", category: "DiD Mortar Spawner")]
	protected int m_iMonteCarloSamples;
	
	[Attribute("50", UIWidgets.EditBox, "Radius (meters) around each sample point to check for targets", category: "DiD Mortar Spawner")]
	protected float m_fSampleRadius;
	
	[Attribute("100", UIWidgets.EditBox, "Minimum distance from mortar to target (meters)", category: "DiD Mortar Spawner")]
	protected float m_fMinTargetDistance;
	
	[Attribute("800", UIWidgets.EditBox, "Maximum distance from mortar to target (meters)", category: "DiD Mortar Spawner")]
	protected float m_fMaxTargetDistance;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable debug visualization of sample points", category: "DiD Mortar Spawner")]
	protected bool m_bDebugVisualization;
	
	// Runtime data
	protected IEntity m_SpawnedMortar;
	protected ref map<IEntity, ref MortarFireMissionData> m_mFireMissions = new map<IEntity, ref MortarFireMissionData>();
	protected WorldTimestamp m_fLastTargetUpdate;
	protected ref array<Shape> m_aDebugShapes = {};
	
	//calculate only once
	protected ref array<float> m_aPolylinePoints2D = null;
	
	//------------------------------------------------------------------------------------------------
	override void Prepare(AFM_DiDZoneComponent owner)
	{
		super.Prepare(owner);
		
		ChimeraWorld world = GetGame().GetWorld();
		m_fLastTargetUpdate = world.GetServerTimestamp();
		
		PrintFormat("AFM_DiDMortarSpawnerComponent: Mortar spawner initialized with %1 MC samples, %2m radius", 
			m_iMonteCarloSamples, m_fSampleRadius, LogLevel.DEBUG);
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
			SpawnSingleGroup();
			return;
		}
		
		// Update fire missions periodically
		WorldTimestamp now = GetCurrentTimestamp();
		
		if (now.DiffSeconds(m_fLastTargetUpdate) >= m_iFireMissionUpdateInterval)
		{
			m_fLastTargetUpdate = now;
			UpdateAllFireMissions();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void Cleanup()
	{
		super.Cleanup();
		SCR_EntityHelper.DeleteEntityAndChildren(m_SpawnedMortar);
		
		m_mFireMissions.Clear();
	}
	
	//------------------------------------------------------------------------------------------------
	override protected int GetSpawnCountForWave()
	{
		// Mortars spawn individually
		return 1;
	}
	
	//------------------------------------------------------------------------------------------------
	override protected void SpawnSingleGroup()
	{
		if (m_aSpawnPoints.Count() == 0 || m_MortarPrefab.IsEmpty())
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: No spawn points or mortar prefabs configured!", LogLevel.WARNING);
			return;
		}
		
		if (!m_crewConfig)
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: No crew config defined!", LogLevel.ERROR);
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
			PrintFormat("AFM_DiDMortarSpawnerComponent: Failed to spawn mortar!", LogLevel.ERROR);
			return;
		}
		
		// Get compartment manager and crew the mortar
		SCR_BaseCompartmentManagerComponent cm = SCR_BaseCompartmentManagerComponent.Cast(
			m_SpawnedMortar.FindComponent(SCR_BaseCompartmentManagerComponent)
		);
		
		if (!cm)
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: Mortar has no compartment manager!", LogLevel.ERROR);
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
			PrintFormat("AFM_DiDMortarSpawnerComponent: Failed to spawn mortar crew!", LogLevel.ERROR);
			return;
		}
		
		fireMission.m_CrewGroup = crew;
		m_mFireMissions.Set(m_SpawnedMortar, fireMission);
		
		// Create initial fire mission
		UpdateFireMission(fireMission);
		
		PrintFormat("AFM_DiDMortarSpawnerComponent: Spawned mortar at %1", m_SpawnedMortar.GetOrigin(), LogLevel.DEBUG);
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
	//! Update fire mission for a specific mortar using Monte Carlo target selection
	//------------------------------------------------------------------------------------------------
	protected void UpdateFireMission(MortarFireMissionData fireMission)
	{
		if (!fireMission || !fireMission.m_Mortar || !fireMission.m_CrewGroup)
			return;
		
		// Find best target position using Monte Carlo sampling
		vector targetPos = FindBestTargetPosition(fireMission.m_SpawnPosition);
		
		if (targetPos == vector.Zero)
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: No valid target found for mortar", LogLevel.DEBUG);
			return;
		}
		
		// Create or update fire position waypoint
		SCR_AIWaypointArtillerySupport fireWaypoint = CreateFirePositionWaypoint(targetPos, fireMission);
		
		if (!fireWaypoint)
		{
			PrintFormat("AFM_DiDMortarSpawnerComponent: Failed to create fire waypoint!", LogLevel.ERROR);
			return;
		}
		
		//TODO: Add different fire mission types and mortar count
		fireWaypoint.SetTargetShotCount(s_AIRandomGenerator.RandInt(1,6));
		
		// Clear existing waypoints and assign new one
		array<AIWaypoint> existingWaypoints = {};
		fireMission.m_CrewGroup.GetWaypoints(existingWaypoints);
		
		foreach (AIWaypoint wp : existingWaypoints)
		{
			fireMission.m_CrewGroup.RemoveWaypoint(wp);
			// Clean up old dynamic waypoint
			if (fireMission.m_CurrentWaypoint == wp)
				SCR_EntityHelper.DeleteEntityAndChildren(wp);
		}
		
		fireMission.m_CrewGroup.AddWaypoint(fireWaypoint);
		fireMission.m_CurrentWaypoint = fireWaypoint;
		fireMission.m_TargetPosition = targetPos;
		fireMission.m_LastUpdateTime = GetCurrentTimestamp();
		
		PrintFormat("AFM_DiDMortarSpawnerComponent: Updated fire mission to %1 (%2 targets)", 
			targetPos.ToString(), fireMission.m_LastTargetCount, LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Monte Carlo sampling to find best target position
	//! Returns position with most defender units within sample radius
	//------------------------------------------------------------------------------------------------
	protected vector FindBestTargetPosition(vector mortarPos)
	{
		if (!m_Zone)
			return vector.Zero;
		
		//TODO: Move below calculations to init (they need to happen only once)
		// Get zone boundary for sampling
		PolylineShapeEntity polyline = m_Zone.GetPolylineEntity();
		if (!polyline)
			return vector.Zero;
		
		array<vector> polylinePoints = {};
		polyline.GetPointsPositions(polylinePoints);
		
		if (polylinePoints.Count() < 3)
			return vector.Zero;
		
		// Calculate zone bounds
		vector minBounds, maxBounds;
		CalculateZoneBounds(polylinePoints, minBounds, maxBounds, polyline.GetOrigin());
		
		// Monte Carlo sampling
		vector bestPosition = vector.Zero;
		
		//TODO: remove me - this is to make mortar fire at anything
		int maxTargetCount = -1;
		WorldTimestamp tStart = GetCurrentTimestamp();
		for (int i = 0; i < m_iMonteCarloSamples; i++)
		{
			// Generate random point within zone bounds
			vector samplePos = GenerateRandomPointInBounds(minBounds, maxBounds);
			
			// Debug visualization
			if (m_bDebugVisualization)
				DebugDrawSamplePoint(samplePos, 0, 0);
			
			// Check if point is actually inside the zone polygon
			if (!IsPointInZone(samplePos, polylinePoints, polyline.GetOrigin()))
				continue;
			
			// Check if within valid range from mortar
			//float distToMortar = vector.Distance(mortarPos, samplePos);
			float distToMortar = Math.Sqrt(Math.Pow(mortarPos[0] - samplePos[0],2) + Math.Pow(mortarPos[2] - samplePos[2], 2));
			if (distToMortar < m_fMinTargetDistance || distToMortar > m_fMaxTargetDistance)
				continue;
			
			// Count targets around this sample point
			int targetCount = CountDefendersInRadius(samplePos, m_fSampleRadius);
			
			// Debug visualization
			if (m_bDebugVisualization)
				DebugDrawSamplePoint(samplePos, targetCount, maxTargetCount);
			
			// Update best position if this sample has more targets
			if (targetCount > maxTargetCount)
			{
				maxTargetCount = targetCount;
				bestPosition = samplePos;
			}
		}
		
		// Store for reference
		if (m_mFireMissions.Count() > 0)
		{
			// Find the fire mission we're updating (hacky, but works for now)
			foreach (IEntity mortar, MortarFireMissionData fm : m_mFireMissions)
			{
				if (fm.m_SpawnPosition == mortarPos)
				{
					fm.m_LastTargetCount = maxTargetCount;
					break;
				}
			}
		}
		WorldTimestamp end = GetCurrentTimestamp();
		PrintFormat("AFM_DiDMortarSpawnerComponent: MC simulation took %1 ms", end.DiffMilliseconds(tStart));
		return bestPosition;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Count defender units within radius of position
	//------------------------------------------------------------------------------------------------
	protected int CountDefendersInRadius(vector centerPos, float radius)
	{
		if (!m_Zone)
			return 0;
		
		SCR_Faction defenderFaction = m_Zone.GetDefenderFaction();
		if (!defenderFaction)
			return 0;
		
		array<int> playerIds = {};
		defenderFaction.GetPlayersInFaction(playerIds);
		
		int count = 0;
		float radiusSq = radius * radius;
		
		foreach (int playerId : playerIds)
		{
			PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
			if (!pc)
				continue;
			
			IEntity playerEntity = pc.GetControlledEntity();
			if (!playerEntity)
				continue;
			
			// Check if player is alive
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerEntity);
			if (!character)
				continue;
			
			SCR_DamageManagerComponent damageManager = character.GetDamageManager();
			if (!damageManager || damageManager.GetState() == EDamageState.DESTROYED)
				continue;
			
			// Check distance (using squared distance for performance)
			vector playerPos = playerEntity.GetOrigin();
			float distSq = vector.DistanceSq(centerPos, playerPos);
			
			if (distSq <= radiusSq)
				count++;
		}
		
		return count;
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
	
	protected bool IsPointInZone(vector point, array<vector> polylinePoints, vector polylineOrigin)
	{
		//init polyline point array once
		if (!m_aPolylinePoints2D)
		{
			m_aPolylinePoints2D = new array<float>();
			foreach (vector p : polylinePoints)
			{
				m_aPolylinePoints2D.Insert(polylineOrigin[0] + p[0]);
				m_aPolylinePoints2D.Insert(polylineOrigin[2] + p[2]);
			}
		}
		
		return Math2D.IsPointInPolygon(m_aPolylinePoints2D, point[0], point[2]);
	}
	
	protected void DebugDrawSamplePoint(vector pos, int targetCount, int maxCount)
	{
		Color color = Color.Yellow;
		if (targetCount == maxCount && targetCount > 0)
			color = Color.Red;
		else if (targetCount > 0)
			color = Color.Orange;
		
		// Draw sphere at sample point
		Shape s = Shape.CreateSphere(color.PackToInt(), ShapeFlags.VISIBLE, pos, m_fSampleRadius);
	
		m_aDebugShapes.Insert(s);
	}
}

//------------------------------------------------------------------------------------------------
//! Data container for mortar fire mission tracking
//------------------------------------------------------------------------------------------------
class MortarFireMissionData
{
	IEntity m_Mortar;
	AIGroup m_CrewGroup;
	SCR_AIWaypoint m_CurrentWaypoint;
	vector m_SpawnPosition;
	vector m_TargetPosition;
	WorldTimestamp m_LastUpdateTime;
	int m_LastTargetCount;
}
