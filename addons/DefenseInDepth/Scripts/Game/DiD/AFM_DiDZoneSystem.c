class AFM_DiDZoneSystem: GameSystem
{
	protected ref map<int, AFM_DiDZoneComponent> m_aZones = new map<int, AFM_DiDZoneComponent>();
	protected AFM_DiDZoneComponent m_ActiveZone = null;
	
	
	// How often should the system check zones (in seconds)
	protected const float m_fCheckInterval = 1.0;
	protected float m_fCheckTimer = 0;
	
	protected int m_iAttackersInActiveZone = 0;
	protected int m_iDefendersRemaining = 0;
	
	// Callbacks
	protected ref ScriptInvoker m_OnZoneChanged;
	protected ref ScriptInvoker m_OnZoneUpdate;
	protected ref ScriptInvoker m_OnAllZonesCompleted;
	protected ref ScriptInvoker m_OnZoneHeld;
	
	// Game mode reference
	protected AFM_GameModeDiD m_GameMode;
	protected SCR_FactionManager m_FactionManager;
	protected bool m_bIsSystemActive = false;
	protected bool m_bSkipWarmup = false;
	
	protected const int m_iStartingZoneIndex = 1;
	
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

		// Process the current zone
		ProcessZone();
	}
	
	//------------------------------------------------------------------------------------------------
	override event bool ShouldBePaused()
	{
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	bool RegisterZone(AFM_DiDZoneComponent zone)
	{
		PrintFormat("Registering zone %1 as %2 stage", zone.GetZoneName(), zone.GetZoneIndex());
		
		int zoneIndex = zone.GetZoneIndex();  
		
		if (m_aZones.Contains(zoneIndex))
		{
			PrintFormat("Zone %1 is already present at index %2", zone.GetZoneName(), zoneIndex, level: LogLevel.ERROR);
			return false;
		}
		
		m_aZones.Insert(zoneIndex, zone);
		
		//TODO: Add late init here
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	void StartZoneSystem()
	{
		m_bIsSystemActive = true;
		Enable(true);
		
		PrintFormat("AFM_DiDZoneSystem: Started zone system with %1 zones", m_aZones.Count());
		
		// Activate first zone in prepare phase
		if (m_aZones.Contains(m_iStartingZoneIndex))
		{
			m_ActiveZone = m_aZones[m_iStartingZoneIndex];
			m_ActiveZone.ActivateZone();
			
			if (m_OnZoneChanged)
				m_OnZoneChanged.Invoke(m_iStartingZoneIndex);
		} 
		else
		{
			PrintFormat("AFM_DiDZoneSystem: Zone index %1 is invalid! Zone count: %2", 
				m_iStartingZoneIndex, m_aZones.Count(), level:LogLevel.ERROR
			);
			StopZoneSystem();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Main zone processing method
	//------------------------------------------------------------------------------------------------
	
	protected void ProcessZone()
	{
		if (!m_ActiveZone)
		{	
			PrintFormat("AFM_DiDZoneSystem: Invalid active zone!", level:LogLevel.ERROR);
			return;
		}
		
		// Don't process zones that are already finished
		if (m_ActiveZone.IsZoneFinished())
			return;
		
		EAFMZoneState previousState = m_ActiveZone.GetZoneState();
		EAFMZoneState currentState = m_ActiveZone.Process();
		int zoneIndex = m_ActiveZone.GetZoneIndex();
		
		// Handle state transitions
		if (previousState != currentState)
		{
			OnZoneStateChanged(zoneIndex, previousState, currentState);
		}
		
		if (currentState == EAFMZoneState.FINISHED_HELD)
		{
			PrintFormat("AFM_DiDZoneSystem: Zone %1 defense time expired - defenders held!", zoneIndex);
			if (m_OnZoneHeld)
				m_OnZoneHeld.Invoke();
			StopZoneSystem();
			return;
		}
		
		// Check if zone defense time expired (defenders win this zone)
		if (currentState == EAFMZoneState.FINISHED_FAILED)
		{
			PrintFormat("AFM_DiDZoneSystem: All defenders eliminated in zone %1", zoneIndex);
			ProgressToNextZone();
			return;
		}
		
		int attackersCount = m_ActiveZone.GetAICountInsideZone();
		int defendersCount = m_ActiveZone.GetDefenderCount();
		bool sendUpdate = (attackersCount != m_iAttackersInActiveZone || defendersCount != m_iDefendersRemaining);
		m_iAttackersInActiveZone = attackersCount;
		m_iDefendersRemaining = defendersCount;
		
		
		if (sendUpdate)
		{
			if (m_OnZoneUpdate)
				m_OnZoneUpdate.Invoke();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnZoneStateChanged(int zoneIndex, EAFMZoneState oldState, EAFMZoneState newState)
	{
		PrintFormat("AFM_DiDZoneSystem: Zone %1 state changed from %2 to %3", zoneIndex, oldState, newState);
		
		// Notify game mode when transitioning from PREPARE to ACTIVE
		if (oldState == EAFMZoneState.PREPARE && newState == EAFMZoneState.ACTIVE)
		{
			if (m_OnZoneChanged)
				m_OnZoneChanged.Invoke();
		}
		
		// Notify game mode when timer state changes (ACTIVE <-> FROZEN)
		if ((oldState == EAFMZoneState.ACTIVE && newState == EAFMZoneState.FROZEN) ||
			(oldState == EAFMZoneState.FROZEN && newState == EAFMZoneState.ACTIVE))
		{
			if (m_OnZoneUpdate)
				m_OnZoneUpdate.Invoke();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ProgressToNextZone()
	{
		int newZoneIndex = m_iStartingZoneIndex;
		if (m_ActiveZone)
		{
			newZoneIndex = m_ActiveZone.GetZoneIndex() + 1;
			m_ActiveZone.DeactivateZone();
		}
		
		m_ActiveZone = m_aZones[newZoneIndex];
		
		Print("AFM_DiDZoneSystem: Progressing to zone " + newZoneIndex);
		m_bSkipWarmup = false;
		
		// Check if all zones completed
		if (newZoneIndex > m_aZones.Count())
		{
			PrintFormat("AFM_DiDZoneSystem: All zones completed (max: %1)", m_aZones.Count());
			StopZoneSystem();
			
			if (m_OnAllZonesCompleted)
				m_OnAllZonesCompleted.Invoke();
			return;
		}
		
		// Activate next zone in prepare phase
		if (m_ActiveZone)
		{
			m_ActiveZone.ActivateZone();
			
			if (m_OnZoneChanged)
				m_OnZoneChanged.Invoke();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Public API / Getters
	//------------------------------------------------------------------------------------------------
	
	int GetAICountInCurrentZone()
	{
		if (!m_ActiveZone)
			return -1;
		
		return m_ActiveZone.GetAICountInsideZone();
	}
	
	int GetDefenderCount()
	{
		if (!m_ActiveZone)
			return -1;
		
		return m_ActiveZone.GetDefenderCount();
	}
	
	AFM_PlayerSpawnPointEntity GetCurrentZonePlayerSpawnPoint()
	{
		if (!m_ActiveZone)
			return null;
		
		return m_ActiveZone.GetPlayerSpawnPoint();
	}
	
	int GetCurrentZoneIndex()
	{
		if (m_ActiveZone)
			return m_ActiveZone.GetZoneIndex();
		return -1;
	}
	
	int GetMaxZoneIndex()
	{
		return m_aZones.Count();
	}
	
	bool IsTimerRunning()
	{
		if (!m_ActiveZone)
			return false;
		
		EAFMZoneState state = m_ActiveZone.GetZoneState();
		return (state == EAFMZoneState.ACTIVE || state == EAFMZoneState.PREPARE);
	}
	
	bool IsWarmup()
	{
		if (!m_ActiveZone)
			return false;
		
		return m_ActiveZone.GetZoneState() == EAFMZoneState.PREPARE;
	}
	
	WorldTimestamp GetZoneTimeoutTimestamp()
	{
		if (!m_ActiveZone)
		{
			ChimeraWorld world = GetGame().GetWorld();
			return world.GetServerTimestamp();
		}
		
		return m_ActiveZone.GetZoneEndTime();
	}
	
	void ForceEndPrepareStage()
	{
		if (!m_ActiveZone || m_ActiveZone.GetZoneState() != EAFMZoneState.PREPARE)
			return;
		
		m_ActiveZone.ForceEndPrepareStage();
	}
	
	
	void StopZoneSystem()
	{
		Enable(false);
		m_bIsSystemActive = false;
		// Deactivate all zones
		foreach (AFM_DiDZoneComponent zone : m_aZones)
		{
			if (zone)
				zone.DeactivateZone();
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
}
