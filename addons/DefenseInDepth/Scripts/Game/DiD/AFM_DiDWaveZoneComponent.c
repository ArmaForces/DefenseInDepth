//------------------------------------------------------------------------------------------------
//! Wave defense zone component - players fight against waves of increasing difficulty
//! Each wave has limited tickets (spawn budget), depleting tickets and killing all AI progresses to next wave
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
class AFM_DiDWaveZoneComponentClass: AFM_DiDZoneComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDWaveZoneComponent: AFM_DiDZoneComponent
{
	[Attribute("5", UIWidgets.EditBox, "Number of waves to complete", category: "DiD Wave Zone")]
	protected int m_iTotalWaves;
	
	[Attribute("20", UIWidgets.EditBox, "Base ticket count for wave 1", category: "DiD Wave Zone")]
	protected int m_iBaseTickets;
	
	[Attribute("10", UIWidgets.EditBox, "Additional tickets per wave level", category: "DiD Wave Zone")]
	protected int m_iTicketsPerWave;
	
	[Attribute("1.2", UIWidgets.EditBox, "Spawn frequency multiplier per wave (higher = faster spawns)", category: "DiD Wave Zone")]
	protected float m_fSpawnFrequencyMultiplier;
	
	[Attribute("1.5", UIWidgets.EditBox, "Spawn count multiplier per wave (higher = more AI per spawn)", category: "DiD Wave Zone")]
	protected float m_fSpawnCountMultiplier;
	
	[Attribute("15", UIWidgets.EditBox, "Time to wait before next wave starts (seconds)", category: "DiD Wave Zone")]
	protected int m_iWaveTransitionTime;
	
	[Attribute("-1", UIWidgets.EditBox, "Supply reward on wave finish. Requires supply cache as a child entity", category: "DiD")]
	protected int m_iSupplyReward;
	
	
	// Wave-specific state
	protected int m_iCurrentWave = 1;

	
	//------------------------------------------------------------------------------------------------
	override protected void LateInit()
	{
		super.LateInit();
		
		PrintFormat("AFM_DiDWaveZoneComponent %1: Initialized with %2 waves, %3 spawners", 
			m_sZoneName, m_iTotalWaves, m_aSpawners.Count());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Activate the zone and start first wave
	//------------------------------------------------------------------------------------------------
	override void ActivateZone()
	{
		StartWave(1);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Deactivate zone and cleanup
	//------------------------------------------------------------------------------------------------
	override void DeactivateZone()
	{
		m_eZoneState = EAFMZoneState.INACTIVE;
		Cleanup();
		PrintFormat("AFM_DiDWaveZoneComponent %1: Deactivated", m_sZoneName);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Main processing loop called periodically - overrides base zone logic
	//------------------------------------------------------------------------------------------------
	override EAFMZoneState Process()
	{
		switch (m_eZoneState)
		{
			case EAFMZoneState.INACTIVE:
			case EAFMZoneState.FINISHED_HELD:
			case EAFMZoneState.FINISHED_FAILED:
				return m_eZoneState;
				
			case EAFMZoneState.PREPARE:
				return HandleWavePrepareState();
				
			case EAFMZoneState.ACTIVE:
				return HandleWaveActiveState();
				
			case EAFMZoneState.WAVE_COMPLETE:
				return HandleWaveCompleteState();
		}
		
		return m_eZoneState;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle preparation state (countdown before wave starts)
	//------------------------------------------------------------------------------------------------
	protected EAFMZoneState HandleWavePrepareState()
	{
		WorldTimestamp now = GetCurrentTimestamp();
		
		// Check if preparation time is over
		if (now.GreaterEqual(m_fZoneEndTime))
		{
			// Transition to active wave
			m_eZoneState = EAFMZoneState.ACTIVE;
			m_fZoneStartTime = now;
			
			PrintFormat("AFM_DiDWaveZoneComponent %1: Wave %2 STARTED! Tickets: %3", 
				m_sZoneName, m_iCurrentWave, GetRemainingTickets());
		}
		
		return m_eZoneState;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle active wave state (spawning and combat)
	//------------------------------------------------------------------------------------------------
	protected EAFMZoneState HandleWaveActiveState()
	{
		// Check if all defenders are dead
		int defenderCount = GetDefenderCount();
		if (defenderCount == 0)
		{
			m_eZoneState = EAFMZoneState.FINISHED_FAILED;
			PrintFormat("AFM_DiDWaveZoneComponent %1: FINISHED_FAILED - All defenders eliminated!", m_sZoneName);
			
			return m_eZoneState;
		}
		
		// Process spawners (they will consume tickets)
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner && spawner.IsActive())
				spawner.Process();
		}
		
		// Check if wave is complete (no tickets left and no AI alive)
		if (GetRemainingTickets() <= 0)
		{
			int activeAI = GetActiveAICount();
			if (activeAI == 0)
			{
				CompleteWave();
			}
		}
		
		return m_eZoneState;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle wave complete state (transition between waves)
	//------------------------------------------------------------------------------------------------
	protected EAFMZoneState HandleWaveCompleteState()
	{
		if (m_iCurrentWave >= m_iTotalWaves)
		{
			m_eZoneState = EAFMZoneState.FINISHED_HELD;
			PrintFormat("AFM_DiDWaveZoneComponent %1: FINISHED_HELD - All %2 waves completed!", 
				m_sZoneName, m_iTotalWaves);
			return m_eZoneState;
		}
		
		StartWave(m_iCurrentWave + 1);
		return m_eZoneState;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Start a new wave
	//------------------------------------------------------------------------------------------------
	protected void StartWave(int waveNumber)
	{
		m_iCurrentWave = waveNumber;
		m_eZoneState = EAFMZoneState.PREPARE;
		
		// Calculate tickets for this wave
		int ticketCount = CalculateWaveTickets(waveNumber);
		
		// Initialize tickets for spawners that use them
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner.IsActive())
				spawner.SetRemainingTickets(ticketCount);
		}
		
		// Set up preparation timer
		WorldTimestamp now = GetCurrentTimestamp();
		m_fZoneStartTime = now;
		m_fZoneEndTime = now.PlusSeconds(m_iPrepareTimeSeconds);
		
		// Apply wave difficulty to spawners
		ApplyWaveDifficulty(waveNumber);
		
		PrintFormat("AFM_DiDWaveZoneComponent %1: Preparing wave %2/%3 with %4 tickets...", 
			m_sZoneName, waveNumber, m_iTotalWaves, ticketCount);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Complete current wave and prepare for next
	//------------------------------------------------------------------------------------------------
	protected void CompleteWave()
	{
		m_eZoneState = EAFMZoneState.WAVE_COMPLETE;
		
		WorldTimestamp now = GetCurrentTimestamp();
		m_fZoneStartTime = now;
		m_fZoneEndTime = now.PlusSeconds(m_iWaveTransitionTime);
		
		PrintFormat("AFM_DiDWaveZoneComponent %1: Wave %2 COMPLETE!", m_sZoneName, m_iCurrentWave);
		
		AwardSupplyRewards();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculate ticket count for a wave
	//------------------------------------------------------------------------------------------------
	protected int CalculateWaveTickets(int waveNumber)
	{
		return m_iBaseTickets + ((waveNumber - 1) * m_iTicketsPerWave);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Apply difficulty scaling to spawners for current wave
	//------------------------------------------------------------------------------------------------
	protected void ApplyWaveDifficulty(int waveNumber)
	{
		float frequencyScale = Math.Pow(m_fSpawnFrequencyMultiplier, waveNumber - 1);
		float countScale = Math.Pow(m_fSpawnCountMultiplier, waveNumber - 1);
		
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner || !spawner.IsActive())
				continue;
			
			// Decrease spawn interval (faster spawns)
			int baseInterval = spawner.GetWaveInterval();
			spawner.SetWaveInterval(Math.Max(10, Math.Floor(baseInterval / frequencyScale)));
			
			// Increase spawn count
			int baseCount = spawner.GetSpawnCount();
			spawner.SetSpawnCount(Math.Max(1, Math.Ceil(baseCount * countScale)));
			
			PrintFormat("AFM_DiDWaveZoneComponent %1: Wave %2 spawner scaled - Interval: %3s, Count: %4", 
				m_sZoneName, waveNumber, spawner.GetWaveInterval(), spawner.GetSpawnCount(), 
				LogLevel.DEBUG);
		}
	}

	protected void AwardSupplyRewards()
	{
		if (!m_SupplyCache || m_iSupplyReward < 0)
			return;
		
		SCR_ResourceContainer container = m_SupplyCache.GetContainer(EResourceType.SUPPLIES);
		if (!container)
			return;
		
		if (!container.SetResourceValue(container.GetResourceValue() + m_iSupplyReward))
		{
			PrintFormat("AFM_DiDWaveZoneComponent %1: Failed to update container resource values",
				m_sZoneName, level:LogLevel.WARNING);
		}
	}	

	
	//------------------------------------------------------------------------------------------------
	// Getters (wave-specific)
	//------------------------------------------------------------------------------------------------
	
	int GetCurrentWave()
	{ 
		return m_iCurrentWave;
	}
	
	int GetTotalWaves()
	{ 
		return m_iTotalWaves; 
	}
	
	override int GetZoneDisplayNumber()
	{
		return GetCurrentWave();
	}
	
	int GetRemainingTickets()
	{
		int tickets = 0;
		foreach(AFM_DiDSpawnerComponent spawner: m_aSpawners)
		{
			if (spawner && spawner.IsActive())
				tickets = tickets + spawner.GetRemainingTickets();
		}
		
		return tickets;
	}
	
	override int GetRedforScore()
	{
		int tickets = GetRemainingTickets();
		if (tickets < 0)
			return 0;
		
		return tickets;
	}
	
	override WorldTimestamp GetZoneEndTime()
	{
		if (m_eZoneState != EAFMZoneState.ACTIVE)
			return super.GetZoneEndTime();
		
		//TODO: Fix me - dirty hack
		WorldTimestamp t = GetCurrentTimestamp().PlusSeconds(1000);
		
		foreach(AFM_DiDSpawnerComponent spawner: m_aSpawners)
		{
			WorldTimestamp spawnTimestamp = spawner.GetNextSpawnTime();
			if (spawner.IsActive() && t.Greater(spawnTimestamp))
				t = spawnTimestamp;
		}
		return t;
	}
}
