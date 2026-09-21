//------------------------------------------------------------------------------------------------
enum EAFMZoneState
{
	INACTIVE,			// Zone is not yet active
	PREPARE,			// Zone is in preparation phase (warmup)
	ACTIVE,				// Zone is active and being defended (or wave is active)
	FROZEN,				// Zone timer is frozen (too many attackers)
	FINISHED_HELD,		// Zone successfully defended by defenders (or all waves completed)
	FINISHED_FAILED,	// Zone lost - all defenders eliminated
	
	// Wave zone specific states
	WAVE_COMPLETE		// Wave cleared, preparing for next wave
}

//------------------------------------------------------------------------------------------------
class AFM_DiDZoneComponentClass: ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDZoneComponent: ScriptComponent
{
	[Attribute("DidZone", UIWidgets.Auto, desc: "Zone name", category: "DiD")]
	protected string m_sZoneName;
	
	[Attribute("1", UIWidgets.Auto, desc: "Stop the timer while attackers outnumber defenders inside the zone?", category: "DiD")]
	protected bool m_bStopTimerOnRedforSuperiority;
	
	[Attribute("1", UIWidgets.Auto, desc: "Pause infantry and mechanized spawners while the zone is contested (timer frozen)? Mortars and helicopters keep operating", category: "DiD")]
	protected bool m_bStopSpawnersOnRedforSuperiority;
	
	[Attribute("1", UIWidgets.EditBox, "Zone index (1 to N), 1 is played first, N is the last zone", category: "DiD")]
	protected int m_iZoneIndex;
	
	[Attribute("300", UIWidgets.EditBox, "Time in seconds to prepare", category: "DiD")]
	protected int m_iPrepareTimeSeconds;
	
	[Attribute("600", UIWidgets.EditBox, "Time in seconds to defend zone", category: "DiD")]
	protected int m_iDefenseTimeSeconds;
	
	[Attribute("0", UIWidgets.EditBox, "Max AI soldiers across all spawners of this zone (0 = no zone-wide limit, only each spawner's own limit applies)", category: "DiD")]
	protected int m_iMaxAICount;
	
	protected PolylineShapeEntity m_PolylineEntity;
	protected AFM_PlayerSpawnPointEntity m_PlayerSpawnPoint;
	protected ref array<AFM_DiDSpawnerComponent> m_aSpawners = {};
	protected SCR_ResourceComponent m_SupplyCache;
	
	// Cached 2D polyline points for zone boundary checks (world-space X/Z pairs)
	protected ref array<float> m_aZonePolylinePoints2D = null;
		
	// Zone state management
	protected EAFMZoneState m_eZoneState = EAFMZoneState.INACTIVE;
	protected WorldTimestamp m_fZoneStartTime;
	protected WorldTimestamp m_fZoneEndTime;
	protected int m_iRemainingTimeSeconds;
	
	// Faction configuration
	protected SCR_Faction m_RedforFaction;
	protected SCR_Faction m_BluforFaction;
	
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode())
			return;
		
		//Only initialize when zone system is available (on authority)
		if (AFM_DiDZoneSystem.GetInstance())
			GetGame().GetCallqueue().CallLater(LateInit, 5000);
	}
	
	protected void LateInit()
	{
		IEntity e = GetOwner().GetChildren();
		if (!e)
			PrintFormat("AFM_DiDZoneComponent %1: No children found!", m_sZoneName, level: LogLevel.ERROR);
		
		while (e)
		{
			switch (e.Type())
			{
				case PolylineShapeEntity:
					m_PolylineEntity = PolylineShapeEntity.Cast(e);
					break;
				case AFM_PlayerSpawnPointEntity:
					m_PlayerSpawnPoint = AFM_PlayerSpawnPointEntity.Cast(e);
					break;
				case AFM_DiDMechanizedSpawnerComponent:
				case AFM_DiDInfantrySpawnerComponent:
				case AFM_DiDMortarSpawnerComponent:
				case AFM_DiDWaveSpawnerComponent:
				case AFM_DiDHeliSpawnerComponent:
				case AFM_DiDCowabungaComponent:
				case AFM_DiDExtractionComponent:
					AFM_DiDSpawnerComponent spawner = AFM_DiDSpawnerComponent.Cast(e);
					m_aSpawners.Insert(spawner);
					break;
				case AFM_SupplyCacheEntity:
					m_SupplyCache = SCR_ResourceComponent.Cast(e.FindComponent(SCR_ResourceComponent));
					break;
				default:
					PrintFormat("AFM_DiDZoneComponent %1: Unknown type %2", m_sZoneName, e.Type().ToString());
			}
			e = e.GetSibling();
		}
		
		if (!m_PolylineEntity)
			PrintFormat("AFM_DiDZoneComponent %1: Missing polyline component, zone wont work properly!", m_sZoneName, level:LogLevel.ERROR);
		if (!m_PlayerSpawnPoint)
			PrintFormat("AFM_DiDZoneComponent %1: Missing player spawnpoint, zone wont work properly!", m_sZoneName, level:LogLevel.ERROR);
		if (m_aSpawners.Count() == 0)
			PrintFormat("AFM_DiDZoneComponent %1: No spawner components found, AI will not spawn!", m_sZoneName, level:LogLevel.WARNING);
		
		// Initialize spawners
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			spawner.Prepare(this);
		}
		
		if (!AFM_DiDZoneSystem.GetInstance().RegisterZone(this))
			PrintFormat("AFM_DiDZoneComponent %1: Failed to register zone!", m_sZoneName, LogLevel.ERROR);
		else
			PrintFormat("AFM_DiDZoneComponent %1: Zone registered", m_sZoneName);
		
		
		AFM_GameModeDiD gamemode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gamemode)
		{
			PrintFormat("AFM_DiDZoneComponent %1: Invalid gamemode!", m_sZoneName, level: LogLevel.ERROR);
			return;
		}
		m_RedforFaction = gamemode.GetRedforFaction();
		m_BluforFaction = gamemode.GetBluforFaction();
	}
	
	//------------------------------------------------------------------------------------------------
	int GetDefenderCount()
	{
		if (!m_BluforFaction)
			return -1;
		
		array<int> playerIds = new array<int>;
		m_BluforFaction.GetPlayersInFaction(playerIds);
		int remainingPlayers = 0;
		
		foreach(int id: playerIds)
		{
			PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
			if (pc)
			{
				SCR_ChimeraCharacter ent = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());		
				if (!ent)
					continue;
				SCR_DamageManagerComponent damageManager = ent.GetDamageManager();
				if (damageManager && !damageManager.IsDestroyed())
					remainingPlayers++;
			}
		}
		
		return remainingPlayers;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Living defender players standing inside the zone polygon
	int GetDefenderCountInsideZone()
	{
		if (!m_BluforFaction || !EnsureZonePolygon())
			return 0;

		array<vector> playerPositions = {};
		AFM_DiDTargetingHelper.GetPlayerPositions(m_BluforFaction, playerPositions);

		int count = 0;
		foreach (vector pos : playerPositions)
		{
			if (Math2D.IsPointInPolygon(m_aZonePolylinePoints2D, pos[0], pos[2]))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Build the 2D polygon cache once - polyline shape does not move at runtime
	protected bool EnsureZonePolygon()
	{
		if (!m_PolylineEntity)
			return false;

		if (m_aZonePolylinePoints2D)
			return true;

		m_aZonePolylinePoints2D = new array<float>();
		vector zonePos = m_PolylineEntity.GetOrigin();
		array<vector> points3d = {};
		m_PolylineEntity.GetPointsPositions(points3d);
		foreach (vector p : points3d)
		{
			m_aZonePolylinePoints2D.Insert(p[0] + zonePos[0]);
			m_aZonePolylinePoints2D.Insert(p[2] + zonePos[2]);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	int GetAICountInsideZone()
	{
		WorldTimestamp timeStart = GetCurrentTimestamp();

		if (!EnsureZonePolygon())
			return -1;

		array<AIAgent> agents = {};
		GetGame().GetAIWorld().GetAIAgents(agents);
		
		int count = 0;
		int totalAgentCount = 0;
		
		foreach(AIAgent agent: agents)
		{
			if (!agent.IsInherited(SCR_ChimeraAIAgent) || !agent.IsInherited(ChimeraAIAgent))
				continue;

			IEntity agentEntity = agent.GetControlledEntity();
			if (!agentEntity)
				continue;
			
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agentEntity);
			if (!character || character.GetFactionKey() != m_RedforFaction.GetFactionKey())
				continue;

			// Helicopter crews flying over the zone don't hold it
			if (AFM_DiDTargetingHelper.IsInHelicopter(character))
				continue;

			vector pos = character.GetOrigin();
			if (Math2D.IsPointInPolygon(m_aZonePolylinePoints2D, pos[0], pos[2]))
				count++;
			totalAgentCount++;
		}
		
		WorldTimestamp end = GetCurrentTimestamp();
		PrintFormat("AFM_DiDZoneComponent %1: Found %2/%3 AIs inside zone. Took %4ms", m_sZoneName, count, totalAgentCount, end.DiffMilliseconds(timeStart).ToString(), level: LogLevel.DEBUG);
		return count;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Cleanup all spawned AI through spawner components
	//------------------------------------------------------------------------------------------------
	protected void Cleanup()
	{
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner)
				spawner.Cleanup();
		}
	}
	
	protected void FinishZoneHeld()
	{
		m_eZoneState = EAFMZoneState.FINISHED_HELD;
		PrintFormat("AFM_DiDZoneComponent %1: Zone FINISHED_HELD - Defenders won!", m_sZoneName);
	}
	
	protected void FinishZoneFailed()
	{
		m_eZoneState = EAFMZoneState.FINISHED_FAILED;
		PrintFormat("AFM_DiDZoneComponent %1: Zone FINISHED_FAILED - Defenders eliminated!", m_sZoneName);
	}
	
	protected void FreezeZone()
	{
		if (m_eZoneState != EAFMZoneState.ACTIVE)
			return;
		
		m_iRemainingTimeSeconds = m_fZoneEndTime.DiffSeconds(GetCurrentTimestamp());
		m_eZoneState = EAFMZoneState.FROZEN;
		
		PrintFormat("AFM_DiDZoneComponent %1: Zone FROZEN with %2 seconds remaining",
		 m_sZoneName, m_iRemainingTimeSeconds);
	}
	
	protected void UnfreezeZone()
	{
		if (m_eZoneState != EAFMZoneState.FROZEN)
			return;
		
		m_fZoneEndTime = GetCurrentTimestamp().PlusSeconds(m_iRemainingTimeSeconds);
		m_eZoneState = EAFMZoneState.ACTIVE;
		
		PrintFormat("AFM_DiDZoneComponent %1: Zone UNFROZEN, resuming with %2 seconds",
		 m_sZoneName, m_iRemainingTimeSeconds);
	}

	protected EAFMZoneState HandlePrepareLogic()
	{
		// Check if preparation time is over
		if (GetCurrentTimestamp().GreaterEqual(m_fZoneEndTime))
		{
			// Transition to ACTIVE state
			m_eZoneState = EAFMZoneState.ACTIVE;
			WorldTimestamp now = GetCurrentTimestamp();
			m_fZoneStartTime = now;
			m_fZoneEndTime = now.PlusSeconds(m_iDefenseTimeSeconds);
			PrintFormat("AFM_DiDZoneComponent %1: PREPARE -> ACTIVE", m_sZoneName);	
		}
		
		return m_eZoneState;
	}
	
	protected EAFMZoneState HandleActiveZoneLogic()
	{
		int defenderCount = GetDefenderCount();
		int attackerCount = GetAICountInsideZone();
		
		if (defenderCount == 0)
		{
			FinishZoneFailed();
			return m_eZoneState;
		}
		
		if (IsZoneTimeExpired())
		{
			FinishZoneHeld();
			return m_eZoneState;
		}
		
		// Freeze the timer only while attackers hold the majority inside the zone
		if (m_bStopTimerOnRedforSuperiority)
		{
			int defendersInside = GetDefenderCountInsideZone();
			if (attackerCount > defendersInside && m_eZoneState == EAFMZoneState.ACTIVE)
			{
				FreezeZone();
			}
			else if (attackerCount <= defendersInside && m_eZoneState == EAFMZoneState.FROZEN)
			{
				UnfreezeZone();
			}
		}
		
		// Delegate spawning to spawner components. While contested, reinforcements pause but fire support continues.
		bool spawnersPaused = AreSpawnersPaused();
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner)
				continue;

			if (spawnersPaused && spawner.HasSpawnWaves())
				continue;

			spawner.Process();
		}

		return m_eZoneState;
	}

	//------------------------------------------------------------------------------------------------
	//! Infantry and mechanized spawners are paused while attackers hold the zone
	bool AreSpawnersPaused()
	{
		return m_bStopSpawnersOnRedforSuperiority && m_eZoneState == EAFMZoneState.FROZEN;
	}
	
	//------------------------------------------------------------------------------------------------
	// Main process method - called periodically by the zone system
	//------------------------------------------------------------------------------------------------
	
	EAFMZoneState Process()
	{
		switch (m_eZoneState)
		{
			case EAFMZoneState.INACTIVE:
			case EAFMZoneState.FINISHED_HELD:
			case EAFMZoneState.FINISHED_FAILED:
				//don't process inactive zones
				return m_eZoneState;
			case EAFMZoneState.PREPARE:
				return HandlePrepareLogic();
			case EAFMZoneState.ACTIVE:
			case EAFMZoneState.FROZEN:
				return HandleActiveZoneLogic();
			default:
				PrintFormat("AFM_DiDZoneComponent %1: Unknown zone state %2", m_sZoneName, m_eZoneState, level:LogLevel.ERROR);
				return m_eZoneState;
		}
		
		//unreachable code but required for parser
		return m_eZoneState;
	}
	
	
	int GetZoneIndex()
	{
		return m_iZoneIndex;
	}
	
	string GetZoneName()
	{
		return m_sZoneName;
	}

	EAFMZoneState GetZoneState()
	{
		return m_eZoneState;
	}
	
	bool IsZoneFinished()
	{
		return m_eZoneState == EAFMZoneState.FINISHED_HELD || m_eZoneState == EAFMZoneState.FINISHED_FAILED;
	}
	
	void ActivateZone()
	{
		WorldTimestamp now = GetCurrentTimestamp();
		m_eZoneState = EAFMZoneState.PREPARE;
		m_fZoneStartTime = now;
		m_fZoneEndTime = now.PlusSeconds(m_iPrepareTimeSeconds);
		PrintFormat("AFM_DiDZoneComponent %1: Entering PREPARE state for %2 seconds",
		 m_sZoneName, m_iPrepareTimeSeconds);
	}
	
	void DeactivateZone()
	{
		m_eZoneState = EAFMZoneState.INACTIVE;
		Cleanup();
		PrintFormat("AFM_DiDZoneComponent %1: Deactivated", m_sZoneName);
	}
	
	void ForceEndPrepareStage()
	{
		if (m_eZoneState != EAFMZoneState.PREPARE)
			return;
		
		m_fZoneEndTime = GetCurrentTimestamp();
	}
	
	WorldTimestamp GetZoneEndTime()
	{
		if (m_eZoneState == EAFMZoneState.FROZEN)
		{
			// Return calculated end time based on remaining seconds
			return GetCurrentTimestamp().PlusSeconds(m_iRemainingTimeSeconds);
		}
		
		return m_fZoneEndTime;
	}
	
	AFM_PlayerSpawnPointEntity GetPlayerSpawnPoint()
	{
		return m_PlayerSpawnPoint;
	}
	
	PolylineShapeEntity GetPolylineEntity()
	{
		return m_PolylineEntity;
	}

	SCR_ResourceComponent GetSupplyCache()
	{
		return m_SupplyCache;
	}
	
	SCR_Faction GetDefenderFaction()
	{
		return m_BluforFaction;
	}
	
	SCR_Faction GetAttackerFaction()
	{
		return m_RedforFaction;
	}
	
	int GetBluforScore()
	{
		return GetDefenderCount();
	}
	
	int GetRedforScore()
	{
		return GetAICountInsideZone();
	}
	
	int GetZoneDisplayNumber()
	{
		return GetZoneIndex();
	}
		
	//------------------------------------------------------------------------------------------------
	//! Earliest upcoming spawn of an active wave spawner (clamped to now when overdue)
	//! \return false when no spawner sends timed waves
	bool GetNextSpawnWaveTime(out WorldTimestamp nextTime)
	{
		if (AreSpawnersPaused())
			return false;

		WorldTimestamp now = GetCurrentTimestamp();
		bool found = false;
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner || !spawner.IsActive() || !spawner.HasSpawnWaves())
				continue;

			WorldTimestamp spawnTime = spawner.GetNextSpawnTime();
			if (spawnTime.Less(now))
				spawnTime = now;

			if (!found || spawnTime.Less(nextTime))
			{
				nextTime = spawnTime;
				found = true;
			}
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Enemies left to fight in this zone, or -1 when spawns are unlimited
	int GetEnemiesRemaining()
	{
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn tickets left across every spawner that uses them, or -1 when none of them do.
	//! Named apart from the wave zone's own GetRemainingTickets, which counts active spawners only
	//! and drives wave progression.
	int GetRemainingSpawnTickets()
	{
		int total = -1;

		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner || !spawner.UsesTickets())
				continue;

			if (total < 0)
				total = 0;

			total += spawner.GetRemainingTickets();
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Get total active AI count across all spawners
	//------------------------------------------------------------------------------------------------
	int GetActiveAICount()
	{
		int totalCount = 0;
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner)
				totalCount += spawner.GetActiveAICount();
		}
		return totalCount;
	}
	
	protected bool IsZoneTimeExpired()
	{
		if (m_eZoneState != EAFMZoneState.ACTIVE)
			return false;
		
		return GetCurrentTimestamp().GreaterEqual(m_fZoneEndTime);
	}
	
	protected WorldTimestamp GetCurrentTimestamp()
	{
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp();
	}
	
	//! Zone-wide AI limit, 0 when only each spawner's own limit applies
	int GetMaxAICount()
	{
		return m_iMaxAICount;
	}
}
