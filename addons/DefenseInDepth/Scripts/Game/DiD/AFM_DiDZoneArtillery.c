//------------------------------------------------------------------------------------------------
//! Internal round type classification for AFM_DiDZoneArtillery.
//! Does NOT directly map to SCR_EAIArtilleryAmmoType — the waypoint prefab's default
//! ammo is used by the AI crew. This enum drives targeting strategy, cost, and cooldown.
enum EAFMRoundType
{
	HIGH_EXPLOSIVE,		//! Costs budget; Monte Carlo targeting of defender clusters
	SMOKE,				//! Low cost; perpendicular smoke screen between player blob and mortar
	ILLUMINATION,		//! Night only; targets zone centroid (night detection pending API)
	PRACTICE			//! Free harass; 1 round at best available position
}

//------------------------------------------------------------------------------------------------
//! Artillery capability owned by AFM_DiDAttackerDirector.
//!
//! Manages the mortar lifecycle: free initial spawn → director-triggered fire missions →
//! destruction detection → optional budget-paid respawn (max 2 times, escalating cost).
//!
//! Smoke missions use perpendicular screen targeting: rounds fall in a line perpendicular
//! to the mortar→player axis, positioned 25–50m in front of the player blob.
//!
//! Hierarchy in world editor:
//!   AFM_DiDZoneEntity
//!   ├── AFM_ArtillerySpawnPointEntity   ← 1..N spawn positions (children of zone)
//!   └── AFM_DiDAttackerDirector
//!       └── AFM_DiDZoneArtillery        ← this entity (child of director)
//!
//! No AFM_ArtillerySpawnPointEntity children on the zone = artillery disabled for that zone.
//------------------------------------------------------------------------------------------------
class AFM_DiDZoneArtilleryClass: GenericEntityClass
{}

class AFM_DiDZoneArtillery: GenericEntity
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Mortar vehicle prefab to spawn", params: "et", category: "DiD Artillery")]
	protected ResourceName m_sMortarPrefab;

	[Attribute("", UIWidgets.Object, "Crew configuration for the mortar team", category: "DiD Artillery")]
	protected ref AFM_CrewConfig m_CrewConfig;

	[Attribute("15", UIWidgets.EditBox, "Budget cost to respawn mortar (first respawn)", category: "DiD Artillery")]
	protected int m_iFirstRespawnCost;

	[Attribute("25", UIWidgets.EditBox, "Budget cost to respawn mortar (second respawn)", category: "DiD Artillery")]
	protected int m_iSecondRespawnCost;

	[Attribute("240", UIWidgets.EditBox, "HE mission cooldown in seconds (plan: 4 min)", category: "DiD Artillery")]
	protected int m_iHECooldownSeconds;

	[Attribute("120", UIWidgets.EditBox, "Smoke mission cooldown in seconds (plan: 2 min)", category: "DiD Artillery")]
	protected int m_iSmokeCooldownSeconds;

	[Attribute("300", UIWidgets.EditBox, "Illumination mission cooldown in seconds (plan: 5 min)", category: "DiD Artillery")]
	protected int m_iIllumCooldownSeconds;

	[Attribute("30", UIWidgets.EditBox, "Practice shot cooldown in seconds", category: "DiD Artillery")]
	protected int m_iPracticeCooldownSeconds;

	[Attribute("3", UIWidgets.EditBox, "Budget cost per HE round", category: "DiD Artillery")]
	protected int m_iHECostPerRound;

	[Attribute("1", UIWidgets.EditBox, "Budget cost per smoke round", category: "DiD Artillery")]
	protected int m_iSmokeCostPerRound;

	[Attribute("2", UIWidgets.EditBox, "Budget cost per illumination round", category: "DiD Artillery")]
	protected int m_iIllumCostPerRound;

	[Attribute("2.0", UIWidgets.EditBox, "Min alive defenders to trigger an HE mission (density threshold)", category: "DiD Artillery")]
	protected float m_fHEDensityThreshold;

	[Attribute("10", UIWidgets.EditBox, "Monte Carlo samples for HE target selection", category: "DiD Artillery")]
	protected int m_iMonteCarloSamples;

	[Attribute("50", UIWidgets.EditBox, "Sample radius (meters) around each MC point for defender counting", category: "DiD Artillery")]
	protected float m_fSampleRadius;

	[Attribute("25", UIWidgets.EditBox, "Smoke screen: minimum distance (m) from player blob to screen center", category: "DiD Artillery Smoke")]
	protected float m_fSmokeMinDistance;

	[Attribute("50", UIWidgets.EditBox, "Smoke screen: maximum distance (m) from player blob to screen center", category: "DiD Artillery Smoke")]
	protected float m_fSmokeMaxDistance;

	[Attribute("20", UIWidgets.EditBox, "Smoke screen: perpendicular spread (m) to each side of center", category: "DiD Artillery Smoke")]
	protected float m_fSmokeScreenSpread;

	// Runtime state
	protected AFM_DiDZoneComponent m_pZone;
	protected ref array<AFM_ArtillerySpawnPointEntity> m_aSpawnPoints = {};

	protected IEntity m_pSpawnedMortar;
	protected AIGroup m_pMortarCrew;
	protected ref array<SCR_AIWaypointArtillerySupport> m_aActiveWaypoints = {};
	protected AFM_ArtillerySpawnPointEntity m_pLastSpawnPoint;

	protected bool m_bMortarActive;
	protected bool m_bInitialSpawnPending;
	protected int m_iRespawnCount;
	protected int m_iMissionsThisLife;

	// Per-type cooldown timestamps
	protected WorldTimestamp m_fHELastFired;
	protected WorldTimestamp m_fSmokeLastFired;
	protected WorldTimestamp m_fIllumLastFired;
	protected WorldTimestamp m_fPracticeLastFired;

	// Polygon cache for MC sampling (built once from zone polyline)
	protected ref array<float> m_aPolylinePoints2D = null;

	//------------------------------------------------------------------------------------------------
	//! Called by AFM_DiDAttackerDirector.Init() after all spawners and spawn points are collected.
	void Initialize(AFM_DiDZoneComponent zone, array<AFM_ArtillerySpawnPointEntity> spawnPoints)
	{
		m_pZone = zone;
		foreach (AFM_ArtillerySpawnPointEntity sp : spawnPoints)
			m_aSpawnPoints.Insert(sp);

		// Push cooldowns far into the past so first decision cycle can fire immediately
		WorldTimestamp farPast = GetCurrentTimestamp().PlusSeconds(-3600);
		m_fHELastFired = farPast;
		m_fSmokeLastFired = farPast;
		m_fIllumLastFired = farPast;
		m_fPracticeLastFired = farPast;

		if (m_aSpawnPoints.IsEmpty())
		{
			PrintFormat("AFM_DiDZoneArtillery: No spawn points — artillery disabled for this zone", level: LogLevel.DEBUG);
			return;
		}

		if (m_sMortarPrefab.IsEmpty())
		{
			PrintFormat("AFM_DiDZoneArtillery: No mortar prefab configured — artillery disabled!", level: LogLevel.WARNING);
			return;
		}

		if (!m_CrewConfig)
		{
			PrintFormat("AFM_DiDZoneArtillery: No crew config — artillery disabled!", level: LogLevel.WARNING);
			return;
		}

		// Mortar spawns when the zone activates, not at init time (see TriggerInitialSpawn)
		m_bInitialSpawnPending = true;
		PrintFormat("AFM_DiDZoneArtillery: Initialized with %1 spawn points — mortar pending zone activation", m_aSpawnPoints.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Polls mortar health — called every second from AFM_DiDAttackerDirector.Process().
	//! Preferred over damage callbacks for reliability across vehicle damage manager types.
	void CheckMortarAlive()
	{
		if (!m_bMortarActive || !m_pSpawnedMortar)
			return;

		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(
			m_pSpawnedMortar.FindComponent(SCR_DamageManagerComponent)
		);

		if (!dmg)
			return;

		if (dmg.GetState() == EDamageState.DESTROYED)
			HandleMortarDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the mortar entity is alive and can receive fire missions.
	bool IsMortarActive()
	{
		return m_bMortarActive && m_pSpawnedMortar != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the mortar has not yet been spawned for this zone activation.
	bool IsInitialSpawnPending()
	{
		return m_bInitialSpawnPending;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn the mortar for the first time. Called by the director on the first active process tick.
	//! The initial spawn is free — no budget is consumed.
	void TriggerInitialSpawn()
	{
		m_bInitialSpawnPending = false;

		if (m_aSpawnPoints.IsEmpty())
			return;

		SpawnMortarTeam(m_aSpawnPoints.GetRandomElement());
		PrintFormat("AFM_DiDZoneArtillery: Initial mortar spawn triggered by zone activation");
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when another respawn is allowed (max 2 paid respawns per zone).
	bool CanRespawn()
	{
		return m_iRespawnCount < 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Budget cost for the next respawn — escalates with each use.
	int GetRespawnCost()
	{
		switch (m_iRespawnCount)
		{
			case 0: return m_iFirstRespawnCost;
			case 1: return m_iSecondRespawnCost;
			default: return int.MAX;
		}
		return int.MAX;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when at least one round type is available (off cooldown).
	bool IsReadyForMission(WorldTimestamp now)
	{
		if (!m_bMortarActive)
			return false;

		return !IsOnCooldown(EAFMRoundType.PRACTICE, now)
			|| !IsOnCooldown(EAFMRoundType.HIGH_EXPLOSIVE, now)
			|| !IsOnCooldown(EAFMRoundType.SMOKE, now)
			|| !IsOnCooldown(EAFMRoundType.ILLUMINATION, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Score how valuable a fire mission is right now. Called by director each decision cycle.
	//! Returns 0 if no mission makes sense.
	float ScoreArtilleryMission(AFM_DiDBattlefieldState state)
	{
		if (!m_bMortarActive)
			return 0;

		WorldTimestamp now = GetCurrentTimestamp();

		// HE: valuable when defenders are clustered and budget allows
		if (!IsOnCooldown(EAFMRoundType.HIGH_EXPLOSIVE, now)
			&& state.m_fAliveDefenders >= m_fHEDensityThreshold
			&& state.m_fBudgetRatio > 0.2)
			return 1.5;

		// Smoke: worthwhile during ASSAULT/FINAL to disrupt defenders
		if (!IsOnCooldown(EAFMRoundType.SMOKE, now)
			&& state.m_ePhase != EAFMAttackPhase.PROBE)
			return 1.0;

		// Illumination: night only
		if (!IsOnCooldown(EAFMRoundType.ILLUMINATION, now)
			&& state.m_bIsNight
			&& state.m_fBudgetRatio > 0.3)
			return 1.2;

		// Practice: free harass shot, low priority
		if (!IsOnCooldown(EAFMRoundType.PRACTICE, now))
			return 0.5;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Score how valuable it is to respawn the mortar now. Called when mortar is not active.
	float ScoreRespawn(AFM_DiDBattlefieldState state)
	{
		if (m_bMortarActive || !CanRespawn())
			return 0;

		// Only respawn during ASSAULT or FINAL when defenders present
		if (state.m_ePhase == EAFMAttackPhase.PROBE || state.m_iDefenderCount == 0)
			return 0;

		return 1.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Execute a fire mission: select round type → find target → consume budget → assign waypoint.
	void TriggerMission(AFM_DiDBattlefieldState state, WorldTimestamp now)
	{
		if (!m_bMortarActive || !m_pMortarCrew)
			return;

		EAFMRoundType roundType = SelectRoundType(state, now);
		int shotCount = GetShotCount(roundType);
		int cost = GetMissionCost(roundType, shotCount);

		// Budget check and consumption
		AFM_DiDAttackerBudget budget = m_pZone.GetBudget();
		if (budget && cost > 0)
		{
			if (!budget.CanAfford(cost))
			{
				PrintFormat("AFM_DiDZoneArtillery: Can't afford %1 mission (cost %2 pts)", RoundTypeToString(roundType), cost, level: LogLevel.DEBUG);
				return;
			}
			budget.Consume(cost);
		}

		SetLastFired(roundType, now);
		m_iMissionsThisLife++;

		if (roundType == EAFMRoundType.SMOKE)
		{
			// Smoke uses perpendicular screen pattern — no single target position needed
			AssignSmokescreenMissions(shotCount);
		}
		else
		{
			vector targetPos = SelectTargetPosition(roundType);
			if (targetPos == vector.Zero)
			{
				PrintFormat("AFM_DiDZoneArtillery: No valid target for %1 — aborting", RoundTypeToString(roundType), level: LogLevel.DEBUG);
				if (budget && cost > 0)
					budget.AddBonus(cost);
				return;
			}

			AssignFireMission(targetPos, shotCount, ConvertRoundType(roundType));

			// Notify players when HE rounds are inbound
			if (roundType == EAFMRoundType.HIGH_EXPLOSIVE)
			{
				AFM_GameModeDiD gamemode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
				if (gamemode)
					gamemode.NotifyMortarFiring();
			}
		}

		PrintFormat("AFM_DiDZoneArtillery: %1 mission fired — %2 rounds (cost %3 pts)",
			RoundTypeToString(roundType), shotCount, cost);
	}

	//------------------------------------------------------------------------------------------------
	//! Respawn mortar at a new position. Budget consumption is handled by the director.
	void TriggerRespawn(WorldTimestamp now)
	{
		if (!CanRespawn() || m_aSpawnPoints.IsEmpty())
			return;

		AFM_ArtillerySpawnPointEntity spawnPoint = GetNextSpawnPoint();
		SpawnMortarTeam(spawnPoint);
		m_iRespawnCount++;
		// Deliberately no broadcast — defenders are NOT notified of respawn
		PrintFormat("AFM_DiDZoneArtillery: Mortar respawned (respawn #%1)", m_iRespawnCount);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns live agent count in the mortar crew — added to director's GetActiveAICount().
	int GetCrewCount()
	{
		if (!m_pMortarCrew)
			return 0;
		return m_pMortarCrew.GetAgentsCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Clean up mortar and waypoint entities when the zone ends.
	void Cleanup()
	{
		ClearCurrentWaypoint();

		if (m_pSpawnedMortar)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_pSpawnedMortar);
			m_pSpawnedMortar = null;
		}

		m_pMortarCrew = null;
		m_bMortarActive = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnMortarTeam(AFM_ArtillerySpawnPointEntity spawnPoint)
	{
		if (!spawnPoint)
		{
			PrintFormat("AFM_DiDZoneArtillery: No spawn point provided!", level: LogLevel.ERROR);
			return;
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		vector mat[4];
		spawnPoint.GetWorldTransform(mat);
		spawnParams.Transform = mat;

		m_pSpawnedMortar = GetGame().SpawnEntityPrefab(Resource.Load(m_sMortarPrefab), GetGame().GetWorld(), spawnParams);
		if (!m_pSpawnedMortar)
		{
			PrintFormat("AFM_DiDZoneArtillery: Failed to spawn mortar!", level: LogLevel.ERROR);
			return;
		}

		SCR_BaseCompartmentManagerComponent cm = SCR_BaseCompartmentManagerComponent.Cast(
			m_pSpawnedMortar.FindComponent(SCR_BaseCompartmentManagerComponent)
		);
		if (!cm)
		{
			PrintFormat("AFM_DiDZoneArtillery: Mortar has no compartment manager!", level: LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(m_pSpawnedMortar);
			m_pSpawnedMortar = null;
			return;
		}

		m_pMortarCrew = m_CrewConfig.SpawnCrew(cm, null);
		if (!m_pMortarCrew)
		{
			PrintFormat("AFM_DiDZoneArtillery: Failed to spawn mortar crew!", level: LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(m_pSpawnedMortar);
			m_pSpawnedMortar = null;
			return;
		}

		m_pLastSpawnPoint = spawnPoint;
		m_bMortarActive = true;
		m_iMissionsThisLife = 0;
		// Invalidate polygon cache on respawn (position hasn't moved but be safe)
		m_aPolylinePoints2D = null;

		PrintFormat("AFM_DiDZoneArtillery: Mortar spawned at %1", m_pSpawnedMortar.GetOrigin().ToString(), level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleMortarDestroyed()
	{
		m_bMortarActive = false;
		int missionsFired = m_iMissionsThisLife;

		// Clean up waypoint — crew can no longer execute it
		ClearCurrentWaypoint();
		m_pMortarCrew = null;

		// Notify defenders: mortar is destroyed (they can stop counter-battery)
		AFM_GameModeDiD gamemode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (gamemode)
			gamemode.NotifyMortarDestroyed();

		PrintFormat("AFM_DiDZoneArtillery: Mortar destroyed after %1 missions", missionsFired);
	}

	//------------------------------------------------------------------------------------------------
	protected EAFMRoundType SelectRoundType(AFM_DiDBattlefieldState state, WorldTimestamp now)
	{
		// Night → illumination (IsSunSet() drives m_bIsNight via director's BuildBattlefieldState)
		if (state.m_bIsNight
			&& !IsOnCooldown(EAFMRoundType.ILLUMINATION, now)
			&& state.m_fBudgetRatio > 0.3)
			return EAFMRoundType.ILLUMINATION;

		// Dense defenders + budget available → HE
		if (!IsOnCooldown(EAFMRoundType.HIGH_EXPLOSIVE, now)
			&& state.m_fAliveDefenders >= m_fHEDensityThreshold
			&& state.m_fBudgetRatio > 0.2)
			return EAFMRoundType.HIGH_EXPLOSIVE;

		// ASSAULT/FINAL → smoke screen to disrupt defender positions
		if (!IsOnCooldown(EAFMRoundType.SMOKE, now)
			&& state.m_ePhase != EAFMAttackPhase.PROBE)
			return EAFMRoundType.SMOKE;

		// Free harass
		return EAFMRoundType.PRACTICE;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns a representative target position for non-smoke round types.
	//! Smoke targeting is handled entirely inside AssignSmokescreenMissions().
	protected vector SelectTargetPosition(EAFMRoundType roundType)
	{
		switch (roundType)
		{
			case EAFMRoundType.HIGH_EXPLOSIVE:
			case EAFMRoundType.PRACTICE:
				return FindBestHETarget();
			case EAFMRoundType.ILLUMINATION:
				return GetZoneCentroid();
		}
		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetShotCount(EAFMRoundType roundType)
	{
		switch (roundType)
		{
			case EAFMRoundType.HIGH_EXPLOSIVE: return s_AIRandomGenerator.RandInt(2, 8);
			case EAFMRoundType.SMOKE:          return s_AIRandomGenerator.RandInt(4, 6);
			case EAFMRoundType.ILLUMINATION:   return s_AIRandomGenerator.RandInt(2, 3);
			case EAFMRoundType.PRACTICE:       return 1;
		}
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetMissionCost(EAFMRoundType roundType, int shotCount)
	{
		switch (roundType)
		{
			case EAFMRoundType.HIGH_EXPLOSIVE: return shotCount * m_iHECostPerRound;
			case EAFMRoundType.SMOKE:          return shotCount * m_iSmokeCostPerRound;
			case EAFMRoundType.ILLUMINATION:   return shotCount * m_iIllumCostPerRound;
			case EAFMRoundType.PRACTICE:       return 0;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Fire a single-target mission. Clears all active waypoints first.
	protected void AssignFireMission(vector targetPos, int shotCount, SCR_EAIArtilleryAmmoType ammoType = SCR_EAIArtilleryAmmoType.HIGH_EXPLOSIVE)
	{
		ClearCurrentWaypoint();
		AddFireWaypoint(targetPos, shotCount, ammoType);
	}

	//------------------------------------------------------------------------------------------------
	//! Fire a perpendicular smoke screen: 3 waypoints spread along the axis that is
	//! perpendicular to the mortar→player direction, centered m_fSmokeMinDistance–
	//! m_fSmokeMaxDistance in front of the player blob (between blob and mortar).
	//!
	//! Falls back to zone centroid when no alive defenders are found.
	protected void AssignSmokescreenMissions(int totalRounds)
	{
		ClearCurrentWaypoint();

		// Find player blob centroid
		vector blobPos = FindPlayerBlob();
		if (blobPos == vector.Zero)
			blobPos = GetZoneCentroid();

		if (blobPos == vector.Zero)
			return;

		// Mortar position — use current spawn point origin as reference
		vector mortarPos = vector.Zero;
		if (m_pSpawnedMortar)
			mortarPos = m_pSpawnedMortar.GetOrigin();

		// Forward direction: mortar → player blob (XZ plane only)
		float dx = blobPos[0] - mortarPos[0];
		float dz = blobPos[2] - mortarPos[2];
		float len = Math.Sqrt(dx * dx + dz * dz);

		// Normalised forward; default to arbitrary axis when mortar is on top of blob
		float ndx = 1.0, ndz = 0.0;
		if (len >= 1.0)
		{
			ndx = dx / len;
			ndz = dz / len;
		}

		// Perpendicular direction (rotate forward 90° in XZ)
		float perpX = -ndz;
		float perpZ = ndx;

		// Screen center: 25–50 m from players, towards mortar
		float offset = s_AIRandomGenerator.RandFloatXY(m_fSmokeMinDistance, m_fSmokeMaxDistance);

		vector center;
		center[0] = blobPos[0] - ndx * offset;
		center[2] = blobPos[2] - ndz * offset;
		center[1] = GetGame().GetWorld().GetSurfaceY(center[0], center[2]);

		// Spread points along perpendicular axis
		vector leftFlank;
		leftFlank[0] = center[0] + perpX * m_fSmokeScreenSpread;
		leftFlank[2] = center[2] + perpZ * m_fSmokeScreenSpread;
		leftFlank[1] = GetGame().GetWorld().GetSurfaceY(leftFlank[0], leftFlank[2]);

		vector rightFlank;
		rightFlank[0] = center[0] - perpX * m_fSmokeScreenSpread;
		rightFlank[2] = center[2] - perpZ * m_fSmokeScreenSpread;
		rightFlank[1] = GetGame().GetWorld().GetSurfaceY(rightFlank[0], rightFlank[2]);

		// Distribute rounds: center receives half, flanks split the rest
		int centerRounds = Math.Max(1, totalRounds / 2);
		int sideRounds = Math.Max(1, (totalRounds - centerRounds) / 2);

		AddFireWaypoint(center, centerRounds, SCR_EAIArtilleryAmmoType.SMOKE);
		AddFireWaypoint(leftFlank, sideRounds, SCR_EAIArtilleryAmmoType.SMOKE);
		AddFireWaypoint(rightFlank, sideRounds, SCR_EAIArtilleryAmmoType.SMOKE);

		PrintFormat("AFM_DiDZoneArtillery: Smoke screen — center %1, spread +/-%2m perp, blob at %3 (offset %4m)",
			center.ToString(), m_fSmokeScreenSpread, blobPos.ToString(), offset);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn and queue a single artillery waypoint. Does NOT clear existing waypoints.
	//! Used internally to build multi-point smoke screens.
	protected void AddFireWaypoint(vector targetPos, int shotCount, SCR_EAIArtilleryAmmoType ammoType)
	{
		Resource wpResource = Resource.Load("{C524700A27CFECDD}Prefabs/AI/Waypoints/AIWaypoint_ArtillerySupport.et");
		if (!wpResource || !wpResource.IsValid())
		{
			PrintFormat("AFM_DiDZoneArtillery: Failed to load artillery waypoint prefab!", level: LogLevel.ERROR);
			return;
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		targetPos[1] = GetGame().GetWorld().GetSurfaceY(targetPos[0], targetPos[2]);
		spawnParams.Transform[3] = targetPos;

		IEntity wpEntity = GetGame().SpawnEntityPrefab(wpResource, GetGame().GetWorld(), spawnParams);
		if (!wpEntity)
		{
			PrintFormat("AFM_DiDZoneArtillery: Failed to spawn waypoint!", level: LogLevel.ERROR);
			return;
		}

		SCR_AIWaypointArtillerySupport wp = SCR_AIWaypointArtillerySupport.Cast(wpEntity);
		if (!wp)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(wpEntity);
			return;
		}

		wp.SetTargetShotCount(shotCount);
		wp.SetAmmoType(ammoType);

		if (m_pMortarCrew)
			m_pMortarCrew.AddWaypoint(wp);

		m_aActiveWaypoints.Insert(wp);
	}

	//------------------------------------------------------------------------------------------------
	//! Remove and delete all active waypoints.
	protected void ClearCurrentWaypoint()
	{
		foreach (SCR_AIWaypointArtillerySupport wp : m_aActiveWaypoints)
		{
			if (!wp)
				continue;
			if (m_pMortarCrew)
				m_pMortarCrew.RemoveWaypoint(wp);
			SCR_EntityHelper.DeleteEntityAndChildren(wp);
		}
		m_aActiveWaypoints.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the centroid of all alive defenders. Returns vector.Zero when none are found.
	protected vector FindPlayerBlob()
	{
		if (!m_pZone)
			return vector.Zero;

		SCR_Faction defFaction = m_pZone.GetDefenderFaction();
		if (!defFaction)
			return vector.Zero;

		array<int> playerIds = {};
		defFaction.GetPlayersInFaction(playerIds);

		float sumX = 0, sumZ = 0;
		int count = 0;

		foreach (int pid : playerIds)
		{
			PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(pid);
			if (!pc)
				continue;

			IEntity ent = pc.GetControlledEntity();
			if (!ent)
				continue;

			SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(ent);
			if (!ch)
				continue;

			SCR_DamageManagerComponent dmg = ch.GetDamageManager();
			if (!dmg || dmg.GetState() == EDamageState.DESTROYED)
				continue;

			vector pos = ch.GetOrigin();
			sumX += pos[0];
			sumZ += pos[2];
			count++;
		}

		if (count == 0)
			return vector.Zero;

		vector blob;
		blob[0] = sumX / count;
		blob[2] = sumZ / count;
		blob[1] = GetGame().GetWorld().GetSurfaceY(blob[0], blob[2]);
		return blob;
	}

	//------------------------------------------------------------------------------------------------
	//! Monte Carlo sampling — finds the position with the highest defender concentration.
	//! Falls back to zone centroid if no defenders are found.
	protected vector FindBestHETarget()
	{
		if (!m_pZone)
			return vector.Zero;

		PolylineShapeEntity polyline = m_pZone.GetPolylineEntity();
		if (!polyline)
			return vector.Zero;

		array<vector> points = {};
		polyline.GetPointsPositions(points);
		if (points.Count() < 3)
			return vector.Zero;

		vector zoneOrigin = polyline.GetOrigin();

		// Build polygon cache once per zone life
		if (!m_aPolylinePoints2D)
		{
			m_aPolylinePoints2D = new array<float>();
			foreach (vector p : points)
			{
				m_aPolylinePoints2D.Insert(zoneOrigin[0] + p[0]);
				m_aPolylinePoints2D.Insert(zoneOrigin[2] + p[2]);
			}
		}

		// Compute bounding box for random sampling
		float minX = zoneOrigin[0] + points[0][0];
		float minZ = zoneOrigin[2] + points[0][2];
		float maxX = minX, maxZ = minZ;
		foreach (vector p : points)
		{
			float wx = zoneOrigin[0] + p[0];
			float wz = zoneOrigin[2] + p[2];
			if (wx < minX) minX = wx;
			if (wx > maxX) maxX = wx;
			if (wz < minZ) minZ = wz;
			if (wz > maxZ) maxZ = wz;
		}

		vector bestPos = vector.Zero;
		int maxCount = -1;

		for (int i = 0; i < m_iMonteCarloSamples; i++)
		{
			vector sample;
			sample[0] = s_AIRandomGenerator.RandFloatXY(minX, maxX);
			sample[2] = s_AIRandomGenerator.RandFloatXY(minZ, maxZ);

			if (!Math2D.IsPointInPolygon(m_aPolylinePoints2D, sample[0], sample[2]))
				continue;

			sample[1] = GetGame().GetWorld().GetSurfaceY(sample[0], sample[2]);

			int count = CountDefendersInRadius(sample);
			if (count > maxCount)
			{
				maxCount = count;
				bestPos = sample;
			}
		}

		// Fall back to centroid if no defenders found in any sample
		if (maxCount <= 0)
			return GetZoneCentroid();

		return bestPos;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountDefendersInRadius(vector center)
	{
		if (!m_pZone)
			return 0;

		SCR_Faction defFaction = m_pZone.GetDefenderFaction();
		if (!defFaction)
			return 0;

		array<int> playerIds = {};
		defFaction.GetPlayersInFaction(playerIds);

		float radSq = m_fSampleRadius * m_fSampleRadius;
		int count = 0;

		foreach (int pid : playerIds)
		{
			PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(pid);
			if (!pc)
				continue;

			IEntity ent = pc.GetControlledEntity();
			if (!ent)
				continue;

			SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(ent);
			if (!ch)
				continue;

			SCR_DamageManagerComponent dmg = ch.GetDamageManager();
			if (!dmg || dmg.GetState() == EDamageState.DESTROYED)
				continue;

			if (vector.DistanceSq(center, ch.GetOrigin()) <= radSq)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetZoneCentroid()
	{
		PolylineShapeEntity polyline = m_pZone.GetPolylineEntity();
		if (!polyline)
			return vector.Zero;

		array<vector> points = {};
		polyline.GetPointsPositions(points);
		if (points.IsEmpty())
			return vector.Zero;

		vector zoneOrigin = polyline.GetOrigin();
		float sumX = 0, sumZ = 0;
		foreach (vector p : points)
		{
			sumX += zoneOrigin[0] + p[0];
			sumZ += zoneOrigin[2] + p[2];
		}

		vector centroid;
		centroid[0] = sumX / points.Count();
		centroid[2] = sumZ / points.Count();
		centroid[1] = GetGame().GetWorld().GetSurfaceY(centroid[0], centroid[2]);
		return centroid;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns a spawn point different from the last used one (if possible).
	protected AFM_ArtillerySpawnPointEntity GetNextSpawnPoint()
	{
		if (m_aSpawnPoints.Count() == 1)
			return m_aSpawnPoints[0];

		// Try to avoid the position used last time
		AFM_ArtillerySpawnPointEntity candidate;
		for (int attempt = 0; attempt < 5; attempt++)
		{
			candidate = m_aSpawnPoints.GetRandomElement();
			if (candidate != m_pLastSpawnPoint)
				return candidate;
		}
		return candidate;
	}

	//------------------------------------------------------------------------------------------------
	bool IsOnCooldown(EAFMRoundType roundType, WorldTimestamp now)
	{
		int cooldown;
		WorldTimestamp lastFired;

		switch (roundType)
		{
			case EAFMRoundType.HIGH_EXPLOSIVE:
				cooldown = m_iHECooldownSeconds;
				lastFired = m_fHELastFired;
				break;
			case EAFMRoundType.SMOKE:
				cooldown = m_iSmokeCooldownSeconds;
				lastFired = m_fSmokeLastFired;
				break;
			case EAFMRoundType.ILLUMINATION:
				cooldown = m_iIllumCooldownSeconds;
				lastFired = m_fIllumLastFired;
				break;
			case EAFMRoundType.PRACTICE:
				cooldown = m_iPracticeCooldownSeconds;
				lastFired = m_fPracticeLastFired;
				break;
			default:
				return true;
		}

		return Math.AbsInt(now.DiffSeconds(lastFired)) < cooldown;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetLastFired(EAFMRoundType roundType, WorldTimestamp now)
	{
		switch (roundType)
		{
			case EAFMRoundType.HIGH_EXPLOSIVE: m_fHELastFired = now; break;
			case EAFMRoundType.SMOKE:          m_fSmokeLastFired = now; break;
			case EAFMRoundType.ILLUMINATION:   m_fIllumLastFired = now; break;
			case EAFMRoundType.PRACTICE:       m_fPracticeLastFired = now; break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string RoundTypeToString(EAFMRoundType roundType)
	{
		switch (roundType)
		{
			case EAFMRoundType.HIGH_EXPLOSIVE: return "HE";
			case EAFMRoundType.SMOKE:          return "SMOKE";
			case EAFMRoundType.ILLUMINATION:   return "ILLUM";
			case EAFMRoundType.PRACTICE:       return "PRACTICE";
		}
		return "UNKNOWN";
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_EAIArtilleryAmmoType ConvertRoundType(EAFMRoundType roundType)
	{
		switch (roundType)
		{
			case EAFMRoundType.HIGH_EXPLOSIVE: return SCR_EAIArtilleryAmmoType.HIGH_EXPLOSIVE;
			case EAFMRoundType.SMOKE:          return SCR_EAIArtilleryAmmoType.SMOKE;
			case EAFMRoundType.ILLUMINATION:   return SCR_EAIArtilleryAmmoType.ILLUMINATION;
			case EAFMRoundType.PRACTICE:       return SCR_EAIArtilleryAmmoType.PRACTICE;
		}
		return SCR_EAIArtilleryAmmoType.PRACTICE;
	}

	//------------------------------------------------------------------------------------------------
	protected WorldTimestamp GetCurrentTimestamp()
	{
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp();
	}
}
