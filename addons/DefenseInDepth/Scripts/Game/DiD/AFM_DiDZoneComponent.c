//------------------------------------------------------------------------------------------------
enum EAFMZoneState
{
	INACTIVE,			// Zone is not yet active
	PREPARE,			// Zone is in preparation phase (warmup)
	ACTIVE,				// Zone is active and being defended (or wave is active)
	FROZEN,				// Zone timer is frozen (too many attackers)
	FINISHED_HELD,		// Zone successfully defended — timer expired
	FINISHED_FAILED,	// Zone lost — all defenders eliminated
	FINISHED_REPELLED,	// Zone successfully defended — attacker budget exhausted and all AI cleared

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

	[Attribute("1", UIWidgets.Auto, desc: "Stop the timer when redfor presence is higher than blufor?", category: "DiD")]
	protected bool m_bStopTimerOnRedforSuperiority;

	[Attribute("1", UIWidgets.Auto, desc: "Stop the AI spawners when redfor presence is higher than blufor?", category: "DiD")]
	protected bool m_bStopSpawnersOnRedforSuperiority;

	[Attribute("1", UIWidgets.EditBox, "Zone index (1 to N), 1 is played first, N is the last zone", category: "DiD")]
	protected int m_iZoneIndex;

	[Attribute("300", UIWidgets.EditBox, "Time in seconds to prepare", category: "DiD")]
	protected int m_iPrepareTimeSeconds;

	[Attribute("600", UIWidgets.EditBox, "Time in seconds to defend zone", category: "DiD")]
	protected int m_iDefenseTimeSeconds;

	[Attribute("50", UIWidgets.EditBox, "Max number of AI groups", category: "DiD")]
	protected int m_iMaxAICount;

	[Attribute("0", UIWidgets.EditBox, "Attacker points budget (0 = unlimited, no budget system)", category: "DiD Budget")]
	protected int m_iPointsBudget;

	[Attribute("0.5", UIWidgets.EditBox, "Fraction of remaining budget rolled over to next zone on failure (0.0-1.0)", category: "DiD Budget")]
	protected float m_fBudgetRolloverFraction;

	protected PolylineShapeEntity m_PolylineEntity;
	protected AFM_PlayerSpawnPointEntity m_PlayerSpawnPoint;
	// Legacy: spawners are direct children of the zone (used when no director is present)
	protected ref array<AFM_DiDSpawnerComponent> m_aSpawners = {};
	// Director mode: central coordinator that owns its own spawner children
	protected AFM_DiDAttackerDirector m_Director;
	protected SCR_ResourceComponent m_SupplyCache;

	// Cached 2D polyline points for zone boundary checks (world-space X/Z pairs)
	protected ref array<float> m_aZonePolylinePoints2D = null;

	// Zone state management
	protected EAFMZoneState m_eZoneState = EAFMZoneState.INACTIVE;
	protected WorldTimestamp m_fZoneStartTime;
	protected WorldTimestamp m_fZoneEndTime;
	protected int m_iRemainingTimeSeconds;

	// Budget — null if m_iPointsBudget == 0 (system disabled)
	protected ref AFM_DiDAttackerBudget m_Budget;

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
				// Director mode: central spawner coordinator — owns its own spawner children
				case AFM_DiDAttackerDirector:
					m_Director = AFM_DiDAttackerDirector.Cast(e);
					m_Director.Init(this);
					break;
				// Legacy mode: spawners are direct children of the zone
				case AFM_DiDMechanizedSpawnerComponent:
				case AFM_DiDInfantrySpawnerComponent:
				case AFM_DiDMortarSpawnerComponent:
				case AFM_DiDWaveSpawnerComponent:
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
			PrintFormat("AFM_DiDZoneComponent %1: Missing polyline component, zone wont work properly!", m_sZoneName, level: LogLevel.ERROR);
		if (!m_PlayerSpawnPoint)
			PrintFormat("AFM_DiDZoneComponent %1: Missing player spawnpoint, zone wont work properly!", m_sZoneName, level: LogLevel.ERROR);

		// In director mode the director owns the spawners — no direct spawners on zone is expected
		bool hasSpawners = m_Director || m_aSpawners.Count() > 0;
		if (!hasSpawners)
			PrintFormat("AFM_DiDZoneComponent %1: No spawner components found, AI will not spawn!", m_sZoneName, level: LogLevel.WARNING);

		// Legacy mode: initialize spawners directly
		foreach (AFM_DiDSpawnerComponent s : m_aSpawners)
		{
			s.Prepare(this);
		}

		if (!AFM_DiDZoneSystem.GetInstance().RegisterZone(this))
			PrintFormat("AFM_DiDZoneComponent %1: Failed to register zone!", m_sZoneName, LogLevel.ERROR);
		else
			PrintFormat("AFM_DiDZoneComponent %1: Zone registered", m_sZoneName);

		if (m_Director)
			PrintFormat("AFM_DiDZoneComponent %1: Director mode active", m_sZoneName);
		else
			PrintFormat("AFM_DiDZoneComponent %1: Legacy spawner mode active (%2 spawners)", m_sZoneName, m_aSpawners.Count());

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

		foreach (int id : playerIds)
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

	int GetAICountInsideZone()
	{
		WorldTimestamp timeStart = GetCurrentTimestamp();

		if (!m_PolylineEntity)
			return -1;

		// Build 2D polygon cache once — polyline shape does not move at runtime
		if (!m_aZonePolylinePoints2D)
		{
			m_aZonePolylinePoints2D = new array<float>();
			vector zonePos = m_PolylineEntity.GetOrigin();
			array<vector> points3d = {};
			m_PolylineEntity.GetPointsPositions(points3d);
			foreach (vector p : points3d)
			{
				m_aZonePolylinePoints2D.Insert(p[0] + zonePos[0]);
				m_aZonePolylinePoints2D.Insert(p[2] + zonePos[2]);
			}
		}

		array<AIAgent> agents = {};
		GetGame().GetAIWorld().GetAIAgents(agents);

		int count = 0;
		int totalAgentCount = 0;

		foreach (AIAgent agent : agents)
		{
			if (!agent.IsInherited(SCR_ChimeraAIAgent) || !agent.IsInherited(ChimeraAIAgent))
				continue;

			IEntity agentEntity = agent.GetControlledEntity();
			if (!agentEntity)
				continue;

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agentEntity);
			if (!character || character.GetFactionKey() != m_RedforFaction.GetFactionKey())
				continue;

			vector pos = character.GetOrigin();
			if (Math2D.IsPointInPolygon(m_aZonePolylinePoints2D, pos[0], pos[2]))
				count++;
			totalAgentCount++;
		}

		WorldTimestamp end = GetCurrentTimestamp();
		PrintFormat("AFM_DiDZoneComponent %1: Found %2/%3 AIs inside zone. Took %4ms",
			m_sZoneName, count, totalAgentCount, end.DiffMilliseconds(timeStart).ToString(), level: LogLevel.DEBUG);
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Cleanup all spawned AI through director or direct spawner components
	//------------------------------------------------------------------------------------------------
	protected void Cleanup()
	{
		if (m_Director)
		{
			m_Director.Cleanup();
			return;
		}

		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner)
				spawner.Cleanup();
		}
	}

	protected void FinishZoneHeld()
	{
		m_eZoneState = EAFMZoneState.FINISHED_HELD;
		PrintFormat("AFM_DiDZoneComponent %1: FINISHED_HELD - Timer expired, defenders held!", m_sZoneName);
	}

	protected void FinishZoneFailed()
	{
		m_eZoneState = EAFMZoneState.FINISHED_FAILED;
		PrintFormat("AFM_DiDZoneComponent %1: FINISHED_FAILED - Defenders eliminated!", m_sZoneName);
	}

	protected void FinishZoneRepelled()
	{
		m_eZoneState = EAFMZoneState.FINISHED_REPELLED;
		PrintFormat("AFM_DiDZoneComponent %1: FINISHED_REPELLED - Attacker budget exhausted, assault repelled!", m_sZoneName);
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

		// Budget exhausted and zone cleared = defenders repelled the assault
		if (m_Budget && m_Budget.IsExhausted() && attackerCount == 0 && GetActiveAICount() == 0)
		{
			FinishZoneRepelled();
			return m_eZoneState;
		}

		// Freeze/unfreeze zone timer
		if (m_bStopTimerOnRedforSuperiority)
		{
			if (attackerCount > defenderCount && m_eZoneState == EAFMZoneState.ACTIVE)
			{
				FreezeZone();
			}
			else if (attackerCount <= defenderCount && m_eZoneState == EAFMZoneState.FROZEN)
			{
				UnfreezeZone();
			}
		}

		// Director mode: central decision cycle
		// Legacy mode: each spawner manages its own timing
		if (m_Director)
		{
			m_Director.Process();
		}
		else
		{
			foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
			{
				if (spawner)
					spawner.Process();
			}
		}

		return m_eZoneState;
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
			case EAFMZoneState.FINISHED_REPELLED:
				return m_eZoneState;
			case EAFMZoneState.PREPARE:
				return HandlePrepareLogic();
			case EAFMZoneState.ACTIVE:
			case EAFMZoneState.FROZEN:
				return HandleActiveZoneLogic();
			default:
				PrintFormat("AFM_DiDZoneComponent %1: Unknown zone state %2", m_sZoneName, m_eZoneState, level: LogLevel.ERROR);
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
		return m_eZoneState == EAFMZoneState.FINISHED_HELD
			|| m_eZoneState == EAFMZoneState.FINISHED_FAILED
			|| m_eZoneState == EAFMZoneState.FINISHED_REPELLED;
	}

	void ActivateZone()
	{
		// Initialize budget if configured
		if (m_iPointsBudget > 0)
		{
			m_Budget = new AFM_DiDAttackerBudget(m_iPointsBudget);
			PrintFormat("AFM_DiDZoneComponent %1: Budget initialized with %2 pts", m_sZoneName, m_iPointsBudget);
		}

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

	//! Returns remaining budget if active, otherwise AI count inside zone
	int GetRedforScore()
	{
		if (m_Budget)
			return m_Budget.GetRemaining();
		return GetAICountInsideZone();
	}

	int GetZoneDisplayNumber()
	{
		return GetZoneIndex();
	}

	//------------------------------------------------------------------------------------------------
	//! Get total active AI count across director or all direct spawners
	//------------------------------------------------------------------------------------------------
	int GetActiveAICount()
	{
		if (m_Director)
			return m_Director.GetActiveAICount();

		int totalCount = 0;
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner)
				totalCount += spawner.GetActiveAICount();
		}
		return totalCount;
	}

	//------------------------------------------------------------------------------------------------
	// Budget API
	//------------------------------------------------------------------------------------------------

	//! Returns the budget instance, or null if budget system is disabled for this zone
	AFM_DiDAttackerBudget GetBudget()
	{
		return m_Budget;
	}

	int GetRemainingBudget()
	{
		if (!m_Budget)
			return 0;
		return m_Budget.GetRemaining();
	}

	float GetBudgetRolloverFraction()
	{
		return m_fBudgetRolloverFraction;
	}

	//! Total defense time in seconds — used by director for time ratio calculation
	int GetTotalDefenseSeconds()
	{
		return m_iDefenseTimeSeconds;
	}

	//------------------------------------------------------------------------------------------------
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

	protected int GetZoneAILimit()
	{
		return m_iMaxAICount;
	}
}
