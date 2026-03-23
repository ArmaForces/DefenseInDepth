class AFM_DiDZoneSystem: GameSystem
{
	protected ref map<int, AFM_DiDStage> m_aStages = new map<int, AFM_DiDStage>();
	protected AFM_DiDStage m_ActiveStage = null;
	protected EAFMZoneState m_eActiveStageState = EAFMZoneState.INACTIVE;

	// How often should the system check zones (in seconds)
	protected const float m_fCheckInterval = 1.0;
	protected float m_fCheckTimer = 0;

	// Callbacks
	protected ref ScriptInvoker m_OnZoneChanged;
	protected ref ScriptInvoker m_OnZoneUpdate;
	protected ref ScriptInvoker m_OnAllZonesCompleted;
	protected ref ScriptInvoker m_OnZoneHeld;
	protected ref ScriptInvoker m_OnZoneRepelled;	// Invoked when attacker budget exhausted + zone cleared
	protected ref ScriptInvoker m_OnZoneFailed;		// Invoked with (int stageIndex) when defenders are eliminated
	protected ref ScriptInvoker m_OnZoneCaptured;	// Invoked with (int stageIndex) when AI holds capture progress to 1.0
	protected ref ScriptInvoker m_OnWaveCompleted;	// Invoked with (int wave, int totalWaves) when a wave is cleared

	// Game mode reference
	protected AFM_GameModeDiD m_GameMode;
	protected SCR_FactionManager m_FactionManager;
	protected bool m_bIsSystemActive = false;
	protected bool m_bSkipWarmup = false;

	protected const int m_iStartingStageIndex = 1;

	//------------------------------------------------------------------------------------------------
	void AFM_DiDZoneSystem()
	{
		m_FactionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		m_GameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
	}

	//------------------------------------------------------------------------------------------------
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		outInfo
			.SetAbstract(false)
			.SetUnique(true)
			.SetLocation(ESystemLocation.Server)
			.AddPoint(ESystemPoint.FixedFrame);
	}

	//------------------------------------------------------------------------------------------------
	static AFM_DiDZoneSystem GetInstance()
	{
		World world = GetGame().GetWorld();

		if (!world)
			return null;

		return AFM_DiDZoneSystem.Cast(world.FindSystem(AFM_DiDZoneSystem));
	}

	//------------------------------------------------------------------------------------------------
	override event protected void OnUpdatePoint(WorldUpdatePointArgs args)
	{
		if (!m_bIsSystemActive)
			return;

		m_fCheckTimer += args.GetTimeSliceSeconds();
		if (m_fCheckTimer < m_fCheckInterval)
			return;

		m_fCheckTimer = 0;

		ProcessStage();
	}

	//------------------------------------------------------------------------------------------------
	override event bool ShouldBePaused()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool RegisterStage(AFM_DiDStage stage)
	{
		int stageIndex = stage.GetStageIndex();

		if (m_aStages.Contains(stageIndex))
		{
			PrintFormat("AFM_DiDZoneSystem: Stage index %1 already registered!", stageIndex, level: LogLevel.ERROR);
			return false;
		}

		m_aStages.Insert(stageIndex, stage);
		PrintFormat("AFM_DiDZoneSystem: Registered stage %1", stageIndex);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void StartZoneSystem()
	{
		m_bIsSystemActive = true;
		Enable(true);

		PrintFormat("AFM_DiDZoneSystem: Started with %1 stage(s)", m_aStages.Count());

		if (m_aStages.Contains(m_iStartingStageIndex))
		{
			m_ActiveStage = m_aStages[m_iStartingStageIndex];
			m_ActiveStage.ActivateStage();
			m_eActiveStageState = EAFMZoneState.PREPARE;

			if (m_OnZoneChanged)
				m_OnZoneChanged.Invoke();
		}
		else
		{
			PrintFormat("AFM_DiDZoneSystem: Stage index %1 not found! Count: %2",
				m_iStartingStageIndex, m_aStages.Count(), level: LogLevel.ERROR);
			StopZoneSystem();
		}
	}

	//------------------------------------------------------------------------------------------------
	// Main stage processing method
	//------------------------------------------------------------------------------------------------

	protected void ProcessStage()
	{
		WorldTimestamp tStart = GetCurrentTimestamp();
		if (!m_ActiveStage)
		{
			PrintFormat("AFM_DiDZoneSystem: No active stage!", level: LogLevel.ERROR);
			return;
		}

		EAFMZoneState previousState = m_eActiveStageState;
		EAFMZoneState currentState = m_ActiveStage.ProcessZones();
		m_eActiveStageState = currentState;
		int stageIndex = m_ActiveStage.GetStageIndex();

		if (previousState != currentState)
			OnStageStateChanged(stageIndex, previousState, currentState);

		// Defenders held — timer expired or budget exhausted
		if (currentState == EAFMZoneState.FINISHED_HELD)
		{
			PrintFormat("AFM_DiDZoneSystem: Stage %1 — defenders held!", stageIndex);
			if (m_OnZoneHeld)
				m_OnZoneHeld.Invoke();
			StopZoneSystem();
			return;
		}

		if (currentState == EAFMZoneState.FINISHED_REPELLED)
		{
			PrintFormat("AFM_DiDZoneSystem: Stage %1 — assault repelled! (budget exhausted)", stageIndex);
			if (m_OnZoneRepelled)
				m_OnZoneRepelled.Invoke();
			StopZoneSystem();
			return;
		}

		// Stage failed — advance to next stage
		if (currentState == EAFMZoneState.FINISHED_FAILED)
		{
			PrintFormat("AFM_DiDZoneSystem: All defenders eliminated in stage %1", stageIndex);
			if (m_OnZoneFailed)
				m_OnZoneFailed.Invoke(stageIndex);
			ProgressToNextStage();
			return;
		}

		// Stage captured — AI held progressive capture
		if (currentState == EAFMZoneState.FINISHED_CAPTURED)
		{
			PrintFormat("AFM_DiDZoneSystem: Stage %1 captured by AI", stageIndex);
			if (m_OnZoneCaptured)
				m_OnZoneCaptured.Invoke(stageIndex);
			ProgressToNextStage();
			return;
		}

		if (m_OnZoneUpdate)
			m_OnZoneUpdate.Invoke();

		WorldTimestamp tEnd = GetCurrentTimestamp();
		float diff = tEnd.DiffMilliseconds(tStart);
		if (diff > 5)
			PrintFormat("AFM_DiDZoneSystem: ProcessStage took %1 ms", diff, level: LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStageStateChanged(int stageIndex, EAFMZoneState oldState, EAFMZoneState newState)
	{
		PrintFormat("AFM_DiDZoneSystem: Stage %1 state changed from %2 to %3", stageIndex, oldState, newState);

		if (oldState == EAFMZoneState.PREPARE && newState == EAFMZoneState.ACTIVE)
		{
			if (m_OnZoneChanged)
				m_OnZoneChanged.Invoke();
		}

		if ((oldState == EAFMZoneState.ACTIVE && newState == EAFMZoneState.FROZEN) ||
			(oldState == EAFMZoneState.FROZEN && newState == EAFMZoneState.ACTIVE))
		{
			if (m_OnZoneUpdate)
				m_OnZoneUpdate.Invoke();
		}

		if (newState == EAFMZoneState.WAVE_COMPLETE)
		{
			if (m_OnZoneUpdate)
				m_OnZoneUpdate.Invoke();
			if (m_OnZoneChanged)
				m_OnZoneChanged.Invoke();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called by AFM_DiDWaveZoneComponent when a wave is cleared
	void NotifyWaveCompleted(int currentWave, int totalWaves)
	{
		if (m_OnWaveCompleted)
			m_OnWaveCompleted.Invoke(currentWave, totalWaves);
	}

	//------------------------------------------------------------------------------------------------
	protected void ProgressToNextStage()
	{
		int newStageIndex = m_iStartingStageIndex;
		int rolloverBudget = 0;

		if (m_ActiveStage)
		{
			// Capture rollover before deactivating
			int remaining = m_ActiveStage.GetRemainingBudget();
			if (remaining > 0)
				rolloverBudget = Math.Floor(remaining * m_ActiveStage.GetBudgetRolloverFraction());

			newStageIndex = m_ActiveStage.GetStageIndex() + 1;
			m_ActiveStage.DeactivateStage();
		}

		Print("AFM_DiDZoneSystem: Progressing to stage " + newStageIndex);
		m_bSkipWarmup = false;

		if (newStageIndex > m_aStages.Count())
		{
			PrintFormat("AFM_DiDZoneSystem: All stages completed (max: %1)", m_aStages.Count());
			StopZoneSystem();

			if (m_OnAllZonesCompleted)
				m_OnAllZonesCompleted.Invoke();
			return;
		}

		m_ActiveStage = m_aStages[newStageIndex];
		m_eActiveStageState = EAFMZoneState.PREPARE;

		if (m_ActiveStage)
		{
			m_ActiveStage.ActivateStage();

			// Apply rollover after ActivateStage initialises the new budget
			if (rolloverBudget > 0 && m_ActiveStage.GetBudget())
			{
				m_ActiveStage.GetBudget().AddBonus(rolloverBudget);
				PrintFormat("AFM_DiDZoneSystem: Rolled over %1 pts to stage %2", rolloverBudget, newStageIndex);
			}

			if (m_OnZoneChanged)
				m_OnZoneChanged.Invoke();
		}
	}

	//------------------------------------------------------------------------------------------------
	// Public API / Getters
	//------------------------------------------------------------------------------------------------
	int GetBluforScore()
	{
		if (!m_ActiveStage)
			return -1;

		int total = 0;
		foreach (AFM_DiDZoneComponent zone : m_ActiveStage.GetZones())
			total += zone.GetBluforScore();
		return total;
	}

	int GetRedforScore()
	{
		if (!m_ActiveStage)
			return -1;

		// Return remaining stage budget if active, else total AI in all zones
		AFM_DiDAttackerBudget budget = m_ActiveStage.GetBudget();
		if (budget)
			return budget.GetRemaining();

		int total = 0;
		foreach (AFM_DiDZoneComponent zone : m_ActiveStage.GetZones())
			total += zone.GetAICountInsideZone();
		return total;
	}

	int GetAICountInCurrentZone()
	{
		if (!m_ActiveStage)
			return -1;

		int total = 0;
		foreach (AFM_DiDZoneComponent zone : m_ActiveStage.GetZones())
			total += zone.GetAICountInsideZone();
		return total;
	}

	int GetDefenderCount()
	{
		if (!m_ActiveStage)
			return -1;

		int total = 0;
		foreach (AFM_DiDZoneComponent zone : m_ActiveStage.GetZones())
			total += zone.GetDefenderCount();
		return total;
	}

	AFM_PlayerSpawnPointEntity GetCurrentZonePlayerSpawnPoint()
	{
		if (!m_ActiveStage)
			return null;
		return m_ActiveStage.GetPlayerSpawnPoint();
	}

	int GetCurrentZoneIndex()
	{
		if (!m_ActiveStage)
			return -1;
		return m_ActiveStage.GetStageIndex();
	}

	int GetMaxZoneIndex()
	{
		return m_aStages.Count();
	}

	bool IsTimerRunning()
	{
		if (!m_ActiveStage)
			return false;

		return (
			m_eActiveStageState == EAFMZoneState.ACTIVE ||
			m_eActiveStageState == EAFMZoneState.PREPARE ||
			m_eActiveStageState == EAFMZoneState.WAVE_COMPLETE
		);
	}

	bool IsWarmup()
	{
		return m_eActiveStageState == EAFMZoneState.PREPARE;
	}

	WorldTimestamp GetZoneTimeoutTimestamp()
	{
		if (!m_ActiveStage)
			return GetCurrentTimestamp();

		// Return the latest end time across all active zones in the stage
		WorldTimestamp latest = GetCurrentTimestamp();
		foreach (AFM_DiDZoneComponent zone : m_ActiveStage.GetZones())
		{
			if (zone.IsZoneFinished())
				continue;
			WorldTimestamp t = zone.GetZoneEndTime();
			if (t.GreaterEqual(latest))
				latest = t;
		}
		return latest;
	}

	void ForceEndPrepareStage()
	{
		if (!m_ActiveStage)
			return;

		foreach (AFM_DiDZoneComponent zone : m_ActiveStage.GetZones())
		{
			if (zone && zone.GetZoneState() == EAFMZoneState.PREPARE)
				zone.ForceEndPrepareStage();
		}
	}

	WorldTimestamp GetCurrentTimestamp()
	{
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp();
	}

	void StopZoneSystem()
	{
		Enable(false);
		m_bIsSystemActive = false;
		foreach (AFM_DiDStage stage : m_aStages)
		{
			if (stage)
				stage.DeactivateStage();
		}
	}

	ScriptInvoker GetOnZoneChanged()
	{
		if (!m_OnZoneChanged)
			m_OnZoneChanged = new ScriptInvoker();
		return m_OnZoneChanged;
	}

	ScriptInvoker GetOnZoneUpdate()
	{
		if (!m_OnZoneUpdate)
			m_OnZoneUpdate = new ScriptInvoker();
		return m_OnZoneUpdate;
	}

	ScriptInvoker GetOnAllZonesCompleted()
	{
		if (!m_OnAllZonesCompleted)
			m_OnAllZonesCompleted = new ScriptInvoker();
		return m_OnAllZonesCompleted;
	}

	ScriptInvoker GetOnZoneHeld()
	{
		if (!m_OnZoneHeld)
			m_OnZoneHeld = new ScriptInvoker();
		return m_OnZoneHeld;
	}

	//! Fired when attacker budget exhausted and zone is cleared — defender victory
	ScriptInvoker GetOnZoneRepelled()
	{
		if (!m_OnZoneRepelled)
			m_OnZoneRepelled = new ScriptInvoker();
		return m_OnZoneRepelled;
	}

	//! Fired with (int stageIndex) when AI holds progressive capture to 1.0
	ScriptInvoker GetOnZoneCaptured()
	{
		if (!m_OnZoneCaptured)
			m_OnZoneCaptured = new ScriptInvoker();
		return m_OnZoneCaptured;
	}

	//! Fired with (int stageIndex) when all defenders in a stage are eliminated
	ScriptInvoker GetOnZoneFailed()
	{
		if (!m_OnZoneFailed)
			m_OnZoneFailed = new ScriptInvoker();
		return m_OnZoneFailed;
	}

	//! Fired with (int currentWave, int totalWaves) when a wave zone wave is cleared
	ScriptInvoker GetOnWaveCompleted()
	{
		if (!m_OnWaveCompleted)
			m_OnWaveCompleted = new ScriptInvoker();
		return m_OnWaveCompleted;
	}
}
