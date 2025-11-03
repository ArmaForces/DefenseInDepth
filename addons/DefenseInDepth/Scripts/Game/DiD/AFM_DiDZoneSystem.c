class AFM_DiDZoneSystem: GameSystem
{
	protected ref array<AFM_DiDZoneComponent> m_aZones;
	
	// -1: pre match, 0: zone setup, 1-N zones
	protected int m_iCurrentZone = -1;
	protected int m_iMaxZoneIndex = 0;
	
	// Timer management
	protected bool m_bIsTimerRunning = true;
	protected bool m_bIsWarmup = true;
	protected int m_iZoneRemainingSeconds;
	protected WorldTimestamp m_fZoneTimeoutTimestamp;
	protected WorldTimestamp m_fLastWaveSpawnTime;
	
	// Configuration
	protected int m_iZonePrepareTimeSeconds = 300;
	protected int m_iZoneDefenseTimeSeconds = 600;
	protected int m_iBaseWaveTime = 90;
	protected int m_iBaseAiSpawnCount = 5;
	protected FactionKey m_sAttackerFactionKey = "USSR";
	
	protected ref array<ResourceName> m_aAIGroupPrefabs;
	
	// Callbacks
	protected ref ScriptInvoker m_OnZoneChanged;
	protected ref ScriptInvoker m_OnTimerStateChanged;
	
	// Game mode reference
	protected AFM_GameModeDiD m_GameMode;
	
	//------------------------------------------------------------------------------------------------
	void AFM_DiDZoneSystem()
	{
		m_aZones = new array<AFM_DiDZoneComponent>();
		m_aAIGroupPrefabs = new array<ResourceName>();
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
	bool RegisterZone(AFM_DiDZoneComponent zone)
	{
		PrintFormat("Registering zone %1 as %2 stage", zone.GetZoneName(), zone.GetZoneIndex());
		
		int zoneIndex = zone.GetZoneIndex();
		
		if (m_aZones[zoneIndex] != null)
		{
			PrintFormat("Zone %1 is already present at index %2", zone.GetZoneName(), zoneIndex, level: LogLevel.ERROR);
			return false;
		}
		
		m_aZones[zoneIndex] = zone;
		
		if (zoneIndex > m_iMaxZoneIndex)
			m_iMaxZoneIndex = zoneIndex;
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	void Initialize(AFM_GameModeDiD gameMode, int zonePrepareTime, int zoneDefenseTime, int baseWaveTime, int baseAiSpawnCount, FactionKey attackerFactionKey, array<ResourceName> aiGroupPrefabs)
	{
		m_GameMode = gameMode;
		m_iZonePrepareTimeSeconds = zonePrepareTime;
		m_iZoneDefenseTimeSeconds = zoneDefenseTime;
		m_iBaseWaveTime = baseWaveTime;
		m_iBaseAiSpawnCount = baseAiSpawnCount;
		m_sAttackerFactionKey = attackerFactionKey;
		m_aAIGroupPrefabs = aiGroupPrefabs;
	}
	
	//------------------------------------------------------------------------------------------------
	void StartZoneSystem()
	{
		ChimeraWorld world = GetGame().GetWorld();
		m_iCurrentZone = 0;
		m_bIsWarmup = true;
		
		//ensure spawn at zone timer start
		m_fLastWaveSpawnTime = world.GetServerTimestamp().PlusSeconds(-m_iBaseWaveTime);

		m_fZoneTimeoutTimestamp = world.GetServerTimestamp().PlusSeconds(m_iZonePrepareTimeSeconds);
		
		GetGame().GetCallqueue().CallLater(ProgressToNextZone, m_iZonePrepareTimeSeconds * 1000);
		
		PrintFormat("AFM_DiDZoneSystem: Started zone system with %1 zones", m_iMaxZoneIndex);
	}
	
	//------------------------------------------------------------------------------------------------
	void ProgressToNextZone()
	{
		ChimeraWorld world = GetGame().GetWorld();
		m_iCurrentZone = m_iCurrentZone + 1;
		
		Print("AFM_DiDZoneSystem: Progressing to zone " + m_iCurrentZone);
		
		if (m_iCurrentZone > m_iMaxZoneIndex)
		{
			PrintFormat("AFM_DiDZoneSystem: All zones completed (max: %1)", m_iMaxZoneIndex);
			if (m_GameMode)
				m_GameMode.OnAllZonesCompleted();
			return;
		}
		
		// Setup warmup phase
		m_bIsWarmup = true;
		m_bIsTimerRunning = true;
		m_fZoneTimeoutTimestamp = world.GetServerTimestamp().PlusSeconds(m_iZonePrepareTimeSeconds);
		
		GetGame().GetCallqueue().CallLater(EndWarmup, m_iZonePrepareTimeSeconds * 1000);
		
		if (m_OnZoneChanged)
			m_OnZoneChanged.Invoke(m_iCurrentZone);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void EndWarmup()
	{
		if (!m_bIsWarmup)
			return;
		
		m_bIsWarmup = false;
		ChimeraWorld world = GetGame().GetWorld();
		m_fZoneTimeoutTimestamp = world.GetServerTimestamp().PlusSeconds(m_iZoneDefenseTimeSeconds);
		
		Print("AFM_DiDZoneSystem: Warmup ended for zone " + m_iCurrentZone);
		
		if (m_OnZoneChanged)
			m_OnZoneChanged.Invoke(m_iCurrentZone);
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdateZoneLogic()
	{
		if (m_bIsWarmup || m_iCurrentZone < 1)
			return;
		
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		
		// Handle AI wave spawning
		int diff = Math.AbsFloat(now.DiffSeconds(m_fLastWaveSpawnTime));
		if (diff > m_iBaseWaveTime)
		{
			m_fLastWaveSpawnTime = now;
			SpawnAIWave();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void SpawnAIWave()
	{
		if (!m_aZones.IsIndexValid(m_iCurrentZone))
		{
			PrintFormat("AFM_DiDZoneSystem: Invalid zone index %1", m_iCurrentZone, LogLevel.WARNING);
			return;
		}
		
		AFM_DiDZoneComponent currentZone = m_aZones[m_iCurrentZone];
		if (!currentZone)
		{
			PrintFormat("AFM_DiDZoneSystem: No zone component found for index %1", m_iCurrentZone, LogLevel.WARNING);
			return;
		}
		
		int spawnCount = s_AIRandomGenerator.RandInt(1, m_iCurrentZone + 1) * m_iBaseAiSpawnCount;
		PrintFormat("AFM_DiDZoneSystem: Spawning %1 AI groups for zone %2", spawnCount, m_iCurrentZone);
		
		for(int i = 0; i < spawnCount; i++)
		{
			currentZone.SpawnAI(m_aAIGroupPrefabs.GetRandomElement());
		}
	}
	
	//------------------------------------------------------------------------------------------------
	int GetAICountInCurrentZone()
	{
		if (!m_aZones.IsIndexValid(m_iCurrentZone))
			return 0;
		
		AFM_DiDZoneComponent currentZone = m_aZones[m_iCurrentZone];
		if (!currentZone)
			return 0;
		
		return currentZone.GetAICountInsideZone();
	}
	
	//------------------------------------------------------------------------------------------------
	void FreezeTimer()
	{
		if (!m_bIsTimerRunning)
			return;
		
		m_bIsTimerRunning = false;
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		
		m_iZoneRemainingSeconds = m_fZoneTimeoutTimestamp.DiffSeconds(now);
		Print("AFM_DiDZoneSystem: Zone timer frozen, time remaining " + m_iZoneRemainingSeconds);
		
		if (m_OnTimerStateChanged)
			m_OnTimerStateChanged.Invoke();
	}
	
	//------------------------------------------------------------------------------------------------
	void UnfreezeTimer()
	{
		if (m_bIsTimerRunning)
			return;
		
		m_bIsTimerRunning = true;
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		m_fZoneTimeoutTimestamp = now.PlusSeconds(m_iZoneRemainingSeconds);
		Print("AFM_DiDZoneSystem: Zone timer unfrozen");
		
		if (m_OnTimerStateChanged)
			m_OnTimerStateChanged.Invoke();
	}
	
	//------------------------------------------------------------------------------------------------
	void Reset()
	{
		m_iCurrentZone = -1;
		m_bIsTimerRunning = true;
		m_bIsWarmup = true;
		m_iZoneRemainingSeconds = 0;
		
		GetGame().GetCallqueue().Remove(ProgressToNextZone);
		GetGame().GetCallqueue().Remove(EndWarmup);
	}
	
	//------------------------------------------------------------------------------------------------
	// Getters
	//------------------------------------------------------------------------------------------------
	
	int GetCurrentZone()
	{
		return m_iCurrentZone;
	}
	
	int GetMaxZoneIndex()
	{
		return m_iMaxZoneIndex;
	}
	
	bool IsTimerRunning()
	{
		return m_bIsTimerRunning;
	}
	
	bool IsWarmup()
	{
		return m_bIsWarmup;
	}
	
	WorldTimestamp GetZoneTimeoutTimestamp()
	{
		if (m_bIsTimerRunning)
			return m_fZoneTimeoutTimestamp;
		
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp().PlusSeconds(m_iZoneRemainingSeconds);
	}
	
	int GetZoneDefenseTimeSeconds()
	{
		return m_iZoneDefenseTimeSeconds;
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
}
