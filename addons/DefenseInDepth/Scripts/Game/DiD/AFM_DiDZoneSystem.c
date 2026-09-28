class AFM_DiDZoneSystem: GameSystem
{
	protected ref map<int, AFM_DiDZoneComponent> m_aZones = new map<int, AFM_DiDZoneComponent>();
	protected AFM_DiDZoneComponent m_ActiveZone = null;
	
	
	// How often should the system check zones (in seconds)
	protected const float m_fCheckInterval = 1.0;
	protected float m_fCheckTimer = 0;
	
	// Callbacks
	protected ref ScriptInvoker m_OnZoneChanged;
	protected ref ScriptInvoker m_OnZoneUpdate;
	protected ref ScriptInvoker m_OnAllZonesCompleted;
	protected ref ScriptInvoker m_OnZoneHeld;
	protected ref ScriptInvoker m_OnZoneFailed;		// Invoked with (int zoneIndex) when defenders are eliminated
	protected ref ScriptInvoker m_OnWaveCompleted;	// Invoked with (int wave, int totalWaves) when a wave is cleared
	
	// Game mode reference
	protected AFM_GameModeDiD m_GameMode;
	protected SCR_FactionManager m_FactionManager;
	protected bool m_bIsSystemActive = false;
	protected bool m_bSkipWarmup = false;

	// The starting zone has run at least once, so a missing active zone means the run is over
	protected bool m_bStartingZoneActivated;
	protected bool m_bZonesValidated;
	
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
		
		// Activate first zone in prepare phase. Zones register themselves as they initialise, so the
		// first one may not be there yet; ProcessZone picks it up as soon as it registers.
		if (!ActivateStartingZone())
			PrintFormat("AFM_DiDZoneSystem: Zone %1 has not registered yet (%2 known), waiting for it",
				m_iStartingZoneIndex, m_aZones.Count(), level: LogLevel.WARNING);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Return false when the starting zone has not registered itself yet
	protected bool ActivateStartingZone()
	{
		if (!m_aZones.Contains(m_iStartingZoneIndex))
			return false;
		
		m_ActiveZone = m_aZones[m_iStartingZoneIndex];
		m_ActiveZone.ActivateZone();
		m_bStartingZoneActivated = true;
		
		if (m_OnZoneChanged)
			m_OnZoneChanged.Invoke();
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Main zone processing method
	//------------------------------------------------------------------------------------------------
	
	protected void ProcessZone()
	{
		// The starting zone registered after the system started
		if (!m_ActiveZone)
		{
			// Only while the first zone has never run. Once the system has progressed past it, having no
			// active zone means the run is over, and starting zone 1 again would loop the mission forever
			if (m_bStartingZoneActivated)
				return;
			
			if (!ActivateStartingZone())
				return;
		}
		
		// Don't process zones that are already finished
		if (m_ActiveZone.IsZoneFinished())
			return;
		
		// Children and spawners are resolved a few seconds after the zone registers
		if (!m_ActiveZone.IsInitialized())
			return;
		
		// Every zone has registered by the time the first one has finished initialising
		if (!m_bZonesValidated)
		{
			m_bZonesValidated = true;
			ValidateZoneIndices();
		}
		
		EAFMZoneState previousState = m_ActiveZone.GetZoneState();
		EAFMZoneState currentState = m_ActiveZone.Process();
		int zoneIndex = m_ActiveZone.GetZoneIndex();
		
		// Handle state transitions
		if (previousState != currentState)
		{
			OnZoneStateChanged(zoneIndex, previousState, currentState);
		}
		
		// Handle zone completion states (both regular zones and wave zones use FINISHED_HELD)
		if (currentState == EAFMZoneState.FINISHED_HELD)
		{
			PrintFormat("AFM_DiDZoneSystem: Zone %1 completed - defenders held!", zoneIndex);
			if (m_OnZoneHeld)
				m_OnZoneHeld.Invoke();
			StopZoneSystem();
			return;
		}
		
		// Handle zone failure (all defenders eliminated) - fire event with old index before progressing
		if (currentState == EAFMZoneState.FINISHED_FAILED)
		{
			PrintFormat("AFM_DiDZoneSystem: All defenders eliminated in zone %1", zoneIndex);
			if (m_OnZoneFailed)
				m_OnZoneFailed.Invoke(zoneIndex);
			ProgressToNextZone();
			return;
		}
		
		if (m_OnZoneUpdate)
			m_OnZoneUpdate.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Zone indices must run 1..N with no gaps: progression counts upwards and stops at the first index
	//! nothing is registered at, so anything numbered past a gap can never be played.
	protected void ValidateZoneIndices()
	{
		int count = m_aZones.Count();
		int lastIndex = m_iStartingZoneIndex + count - 1;
		
		for (int index = m_iStartingZoneIndex; index <= lastIndex; index++)
		{
			if (m_aZones.Contains(index))
				continue;
			
			PrintFormat("AFM_DiDZoneSystem: %1 zones are registered, so their indices must run %2 to %3 without gaps, but nothing has index %4. Renumber the zones - the ones past the gap will never be played",
				count, m_iStartingZoneIndex, lastIndex, index, level: LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Progression found no zone at the next index. Anything registered above it is stranded, which is
	//! worth saying out loud rather than ending the match as if every zone had been played.
	protected void ReportStrandedZones(int missingIndex)
	{
		foreach (int index, AFM_DiDZoneComponent zone : m_aZones)
		{
			if (index <= missingIndex || !zone)
				continue;
			
			PrintFormat("AFM_DiDZoneSystem: Zone %1 ('%2') was never played - nothing is registered at index %3 and progression stops there",
				index, zone.GetZoneName(), missingIndex, level: LogLevel.ERROR);
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
		
		// Notify when wave completes (for wave zones)
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
	protected void ProgressToNextZone()
	{
		int newZoneIndex = m_iStartingZoneIndex;
		int carryOverTickets = 0;
		if (m_ActiveZone)
		{
			newZoneIndex = m_ActiveZone.GetZoneIndex() + 1;
			
			// Read before deactivating: the zone still knows whether it was lost and what it had left
			carryOverTickets = m_ActiveZone.GetCarryOverTickets();
			m_ActiveZone.DeactivateZone();
		}
		
		Print("AFM_DiDZoneSystem: Progressing to zone " + newZoneIndex);
		m_bSkipWarmup = false;
		
		// Looked up rather than indexed: a missing index used to hand back nothing and leave the system
		// running with no active zone, which restarted the first one on the next tick
		AFM_DiDZoneComponent nextZone;
		if (!m_aZones.Find(newZoneIndex, nextZone) || !nextZone)
		{
			ReportStrandedZones(newZoneIndex);
			
			PrintFormat("AFM_DiDZoneSystem: All zones completed (%1 registered)", m_aZones.Count());
			m_ActiveZone = null;
			StopZoneSystem();
			
			if (m_OnAllZonesCompleted)
				m_OnAllZonesCompleted.Invoke();
			return;
		}
		
		// Activate next zone in prepare phase
		m_ActiveZone = nextZone;
		m_ActiveZone.ActivateZone();
		
		// After activation, which is where the new pool is sized and clamped
		m_ActiveZone.AddTickets(carryOverTickets);
		
		if (m_OnZoneChanged)
			m_OnZoneChanged.Invoke();
	}
	
	//------------------------------------------------------------------------------------------------
	// Public API / Getters
	//------------------------------------------------------------------------------------------------
	int GetBluforScore()
	{
		if (!m_ActiveZone)
			return -1;
		return m_ActiveZone.GetBluforScore();
	}
	
	int GetRedforScore()
	{
		if (!m_ActiveZone)
			return -1;
		return m_ActiveZone.GetRedforScore();
	}
	
	
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
		if (!m_ActiveZone)
			return -1;
		
		return m_ActiveZone.GetZoneDisplayNumber();
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
		return (
			state == EAFMZoneState.ACTIVE ||
		 	state == EAFMZoneState.PREPARE || 
		    state == EAFMZoneState.WAVE_COMPLETE
		);
	}
	
	//! Attackers hold the majority inside the zone. Not the same as a stopped timer: a zone may be
	//! configured to keep counting down while contested.
	bool IsContested()
	{
		if (!m_ActiveZone)
			return false;

		return m_ActiveZone.IsContested();
	}

	AFM_DiDZoneComponent GetActiveZone()
	{
		return m_ActiveZone;
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
			return GetCurrentTimestamp();
		
		return m_ActiveZone.GetZoneEndTime();
	}
	
	void ForceEndPrepareStage()
	{
		if (!m_ActiveZone || m_ActiveZone.GetZoneState() != EAFMZoneState.PREPARE)
			return;
		
		m_ActiveZone.ForceEndPrepareStage();
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
	
	//! Fired with (int zoneIndex) when all defenders in a zone are eliminated
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
