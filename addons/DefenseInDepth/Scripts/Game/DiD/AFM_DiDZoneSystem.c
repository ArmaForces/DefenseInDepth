class AFM_DiDZoneSystem: GameSystem
{
	protected ref array<AFM_DiDZoneComponent> m_aZones;
	
	// Index of current active zone (changes every zone advance)
	protected int m_iActiveZone = 0;
	protected int m_iMaxZoneIndex = 0;
	
	
	// How often should the system check zones (in seconds)
	protected float m_fCheckInterval = 1.0;
	protected float m_fCheckTimer = 0;
	
	protected FactionKey m_sAttackerFactionKey = "USSR";
	protected FactionKey m_sDefenderFactionKey = "US";
	
	protected int m_iAttackersInActiveZone = 0;
	protected int m_iDefendersRemaining = 0;
	
	// Callbacks
	protected ref ScriptInvoker m_OnZoneChanged;
	protected ref ScriptInvoker m_OnTimerStateChanged;
	protected ref ScriptInvoker m_OnAllZonesCompleted;
	protected ref ScriptInvoker m_OnZoneHeld;
	protected ref ScriptInvoker m_OnFactionCountChange;
	
	// Game mode reference
	protected AFM_GameModeDiD m_GameMode;
	protected SCR_FactionManager m_FactionManager;
	protected bool m_bIsSystemActive = false;
	
	//------------------------------------------------------------------------------------------------
	void AFM_DiDZoneSystem()
	{
		m_aZones = new array<AFM_DiDZoneComponent>();
		m_FactionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
	}
	
	//------------------------------------------------------------------------------------------------
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		outInfo
			.SetAbstract(false)
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
		ProcessZone(m_iActiveZone);
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
		
		//TODO: Refactor me and change m_aZones to map or something
		// Ensure array is large enough
        while (m_aZones.Count() <= zoneIndex)
        {
            m_aZones.Insert(null);
        }
		
		if (m_aZones[zoneIndex] != null)
		{
			PrintFormat("Zone %1 is already present at index %2", zone.GetZoneName(), zoneIndex, level: LogLevel.ERROR);
			return false;
		}
		
		m_aZones[zoneIndex] = zone;
		
		if (zoneIndex > m_iMaxZoneIndex)
			m_iMaxZoneIndex = zoneIndex;
		
		// Initialize factions in the zone
		if (m_FactionManager)
		{
			zone.InitializeFactions(m_sDefenderFactionKey, m_sAttackerFactionKey, m_FactionManager);
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	void Initialize(AFM_GameModeDiD gameMode, FactionKey defenderFactionKey, FactionKey attackerFactionKey, SCR_FactionManager factionManager)
	{
		m_GameMode = gameMode;
		m_sDefenderFactionKey = defenderFactionKey;
		m_sAttackerFactionKey = attackerFactionKey;
		m_FactionManager = factionManager;
	}
	
	//------------------------------------------------------------------------------------------------
	void StartZoneSystem()
	{
		m_bIsSystemActive = true;
		Enable(true);
		m_iActiveZone = 1;
		
		PrintFormat("AFM_DiDZoneSystem: Started zone system with %1 zones", m_iMaxZoneIndex);
		
		// Activate first zone in prepare phase
		if (m_aZones.IsIndexValid(m_iActiveZone) && m_aZones[m_iActiveZone])
		{
			m_aZones[m_iActiveZone].ActivateZone(true);
			
			if (m_OnZoneChanged)
				m_OnZoneChanged.Invoke(m_iActiveZone);
		} 
		else
		{
			PrintFormat("AFM_DiDZoneSystem: Zone index %1 is invalid! Zone count: %2", 
				m_iActiveZone, m_aZones.Count(), level:LogLevel.ERROR
			);
			StopZoneSystem();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Main zone processing method
	//------------------------------------------------------------------------------------------------
	
	protected void ProcessZone(int zoneIndex)
	{
		if (!m_aZones.IsIndexValid(zoneIndex) || !m_aZones[zoneIndex])
		{	
			PrintFormat("AFM_DiDZoneSystem: Zone index %1 is invalid! Zone count: %2", 
				zoneIndex, m_aZones.Count(), level:LogLevel.ERROR
			);
			return;
		}
		AFM_DiDZoneComponent zone = m_aZones[zoneIndex];
		
		// Don't process zones that are already finished
		if (zone.IsZoneFinished())
			return;
		
		EAFMZoneState previousState = zone.GetZoneState();
		EAFMZoneState currentState = zone.Process();
		
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
		
		int attackersCount = zone.GetAICountInsideZone();
		int defendersCount = zone.GetDefenderCount();
		bool sendUpdate = (attackersCount != m_iAttackersInActiveZone || defendersCount != m_iDefendersRemaining);
		m_iAttackersInActiveZone = attackersCount;
		m_iDefendersRemaining = defendersCount;
		
		
		if (sendUpdate)
		{
			if (m_OnFactionCountChange)
				m_OnFactionCountChange.Invoke();
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
			if (m_OnTimerStateChanged)
				m_OnTimerStateChanged.Invoke();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ProgressToNextZone()
	{
		// Deactivate current zone
		if (m_aZones.IsIndexValid(m_iActiveZone) && m_aZones[m_iActiveZone])
		{
			m_aZones[m_iActiveZone].DeactivateZone();
		}
		
		m_iActiveZone++;
		
		Print("AFM_DiDZoneSystem: Progressing to zone " + m_iActiveZone);
		
		// Check if all zones completed
		if (m_iActiveZone > m_iMaxZoneIndex)
		{
			PrintFormat("AFM_DiDZoneSystem: All zones completed (max: %1)", m_iMaxZoneIndex);
			StopZoneSystem();
			m_iActiveZone = -1; // Mark as completed
			
			if (m_OnAllZonesCompleted)
				m_OnAllZonesCompleted.Invoke();
			return;
		}
		
		// Activate next zone in prepare phase
		if (m_aZones.IsIndexValid(m_iActiveZone) && m_aZones[m_iActiveZone])
		{
			m_aZones[m_iActiveZone].ActivateZone(true);
			
			if (m_OnZoneChanged)
				m_OnZoneChanged.Invoke(m_iActiveZone);
		}
	}

	
	//------------------------------------------------------------------------------------------------
	// Public API / Getters
	//------------------------------------------------------------------------------------------------
	
	int GetAICountInCurrentZone()
	{
		if (!m_aZones.IsIndexValid(m_iActiveZone) || m_iActiveZone < 1)
			return -1;
		
		AFM_DiDZoneComponent currentZone = m_aZones[m_iActiveZone];
		if (!currentZone)
			return -1;
		
		return currentZone.GetAICountInsideZone();
	}
	
	int GetDefenderCount()
	{
		if (!m_aZones.IsIndexValid(m_iActiveZone) || m_iActiveZone < 1)
			return -1;
		
		AFM_DiDZoneComponent zone = m_aZones[m_iActiveZone];
		if (!zone)
			return -1;
		
		return zone.GetDefenderCount();
	}
	
	AFM_PlayerSpawnPointEntity GetCurrentZonePlayerSpawnPoint()
	{
		if (!m_aZones.IsIndexValid(m_iActiveZone) || m_iActiveZone < 1)
			return null;
		
		AFM_DiDZoneComponent zone = m_aZones[m_iActiveZone];
		if (!zone)
			return null;
		
		return zone.GetPlayerSpawnPoint();
	}
	
	int GetCurrentZone()
	{
		return m_iActiveZone;
	}
	
	int GetMaxZoneIndex()
	{
		return m_iMaxZoneIndex;
	}
	
	bool IsTimerRunning()
	{
		if (!m_aZones.IsIndexValid(m_iActiveZone) || m_iActiveZone < 1)
			return true;
		
		AFM_DiDZoneComponent zone = m_aZones[m_iActiveZone];
		if (!zone)
			return true;
		
		EAFMZoneState state = zone.GetZoneState();
		return (state == EAFMZoneState.ACTIVE || state == EAFMZoneState.PREPARE);
	}
	
	bool IsWarmup()
	{
		if (!m_aZones.IsIndexValid(m_iActiveZone) || m_iActiveZone < 1)
			return false;
		
		AFM_DiDZoneComponent zone = m_aZones[m_iActiveZone];
		if (!zone)
			return false;
		
		return zone.GetZoneState() == EAFMZoneState.PREPARE;
	}
	
	WorldTimestamp GetZoneTimeoutTimestamp()
	{
		if (!m_aZones.IsIndexValid(m_iActiveZone) || m_iActiveZone < 1)
		{
			ChimeraWorld world = GetGame().GetWorld();
			return world.GetServerTimestamp();
		}
		
		AFM_DiDZoneComponent zone = m_aZones[m_iActiveZone];
		if (!zone)
		{
			ChimeraWorld world = GetGame().GetWorld();
			return world.GetServerTimestamp();
		}
		
		return zone.GetZoneEndTime();
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
	
	ScriptInvoker GetOnTimerStateChanged()
	{
		if (!m_OnTimerStateChanged)
			m_OnTimerStateChanged = new ScriptInvoker();
		
		return m_OnTimerStateChanged;
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
	
	ScriptInvoker GetOnFactionCountChanged()
	{
		if (!m_OnFactionCountChange)
			m_OnFactionCountChange = new ScriptInvoker();
		
		return m_OnFactionCountChange;
	}
}
