class AFM_GameModeDiDClass: PS_GameModeCoopClass
{
}

class AFM_GameModeDiD: PS_GameModeCoop
{
	[Attribute("US", UIWidgets.EditBox, "Defenders faction key", category: "DiD")]
	protected FactionKey m_sDefenderFactionKey;
	
	[Attribute("USSR", UIWidgets.EditBox, "Attackers faction key", category: "DiD")]
	protected FactionKey m_sAttackerFactionKey;	
	
	[Attribute("300", UIWidgets.EditBox, "Time in seconds to prepare", category: "DiD")]
	int m_iZonePrepareTimeSeconds;
	
	[Attribute("600", UIWidgets.EditBox, "Time in seconds to defend zone", category: "DiD")]
	int m_iZoneDefenseTimeSeconds;
	
	[Attribute("90", UIWidgets.EditBox, "Base time between AI spawns", category: "DiD")]
	int m_iBaseWaveTime;
	
	[Attribute("5", UIWidgets.EditBox, "Base AI spawn count", category: "DiD")]
	int m_iBaseAiSpawnCount;
		
	[Attribute("", UIWidgets.Auto, desc: "AI Group prefabs", category: "DiD")]
	protected ref array<ResourceName> m_aAIGroupPrefabs;
	
	protected SCR_FactionManager m_FactionManager;
	protected AFM_DiDZoneSystem m_ZoneSystem;
	protected ref ScriptInvoker m_OnMatchSituationChanged;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bIsGameRunning = false;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected WorldTimestamp m_fStartTimestamp;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iDefendersRemaining = 0;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iAttackersRemaining = 0;
	
	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnMatchSituationChanged()
	{
		if (!m_OnMatchSituationChanged)
			m_OnMatchSituationChanged = new ScriptInvoker();

		return m_OnMatchSituationChanged;
	}
	
	void OnMatchSituationChanged()
	{
		if (m_OnMatchSituationChanged)
			m_OnMatchSituationChanged.Invoke();
	}
	
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		if (SCR_Global.IsEditMode())
			return;
		
		m_FactionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!m_FactionManager)
		{
			Print("Faction manager component is missing!", LogLevel.ERROR);
		}
		
		m_ZoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!m_ZoneSystem)
		{
			Print("AFM_DiDZoneSystem is missing!", LogLevel.ERROR);
		}
		else
		{
			m_ZoneSystem.Initialize(this, m_iZonePrepareTimeSeconds, m_iZoneDefenseTimeSeconds, m_iBaseWaveTime, m_iBaseAiSpawnCount, m_sAttackerFactionKey, m_aAIGroupPrefabs);
			m_ZoneSystem.GetOnZoneChanged().Insert(OnZoneChanged);
			m_ZoneSystem.GetOnTimerStateChanged().Insert(OnTimerStateChanged);
		}
	}
	
	
	override void OnGameStateChanged()
	{
		super.OnGameStateChanged();
		
		if (GetState() != SCR_EGameModeState.GAME)
			return;
		
		ChimeraWorld world = GetGame().GetWorld();
		m_fStartTimestamp = world.GetServerTimestamp().PlusSeconds(m_iZonePrepareTimeSeconds);
		m_bIsGameRunning = true;
		
		if (m_ZoneSystem)
			m_ZoneSystem.StartZoneSystem();
		
		//start the main game loop with 1Hz frequency
		GetGame().GetCallqueue().CallLater(MainGameLoop, 1000, true);
		
		OnMatchSituationChanged();
		Replication.BumpMe();
	}
	
	
	protected void MainGameLoop()
	{
		if (!IsMaster() || !m_bIsGameRunning || !m_ZoneSystem)
			return;
		
		if (m_ZoneSystem.IsWarmup())
			return;
		
		int remainingDefenders = GetFactionRemainingPlayersCount(m_sDefenderFactionKey);
		int attackersInZone = m_ZoneSystem.GetAICountInCurrentZone();
		
		if (remainingDefenders != m_iDefendersRemaining)
		{
			OnMatchSituationChanged();
			Replication.BumpMe();
		} 
		
		if (remainingDefenders == 0)
		{
			Print("All defenders are dead, progress to next stage");
			m_ZoneSystem.ProgressToNextZone();
		}
		
		if (attackersInZone != m_iAttackersRemaining)
		{
			if (attackersInZone > remainingDefenders)
			{
				m_ZoneSystem.FreezeTimer();
			} else
			{
				m_ZoneSystem.UnfreezeTimer();
			}
			OnMatchSituationChanged();
			Replication.BumpMe();
		}
		
		// Update zone system logic (handles AI spawning)
		m_ZoneSystem.UpdateZoneLogic();
		
		m_iDefendersRemaining = remainingDefenders;
		m_iAttackersRemaining = attackersInZone;
	}
	
	//------------------------------------------------------------------------------------------------
	// Zone system callbacks
	//------------------------------------------------------------------------------------------------
	
	protected void OnZoneChanged(int zoneIndex)
	{
		PrintFormat("AFM_GameModeDiD: Zone changed to %1", zoneIndex);
		
		if (!m_ZoneSystem.IsWarmup())
		{
			// Zone defense phase started
			GetGame().GetCallqueue().CallLater(RespawnAllSpectators, 1000);
			GetGame().GetCallqueue().CallLater(GameEndDefendersWin, m_ZoneSystem.GetZoneDefenseTimeSeconds() * 1000);
		}
		else
		{
			// Zone warmup/preparation phase
			GetGame().GetCallqueue().Remove(GameEndDefendersWin);
			GetGame().GetCallqueue().CallLater(RespawnAllSpectators, 1000 * 5);
		}
		
		RPC_DoProgressToNextZone();
		Rpc(RPC_DoProgressToNextZone);
		OnMatchSituationChanged();
		Replication.BumpMe();
	}
	
	protected void OnTimerStateChanged()
	{
		if (m_ZoneSystem.IsTimerRunning())
		{
			// Timer unfrozen
			GetGame().GetCallqueue().CallLater(GameEndDefendersWin, m_ZoneSystem.GetZoneDefenseTimeSeconds() * 1000);
		}
		else
		{
			// Timer frozen
			GetGame().GetCallqueue().Remove(GameEndDefendersWin);
		}
		
		OnMatchSituationChanged();
		Replication.BumpMe();
	}
	
	void OnAllZonesCompleted()
	{
		Print("All zones completed, attackers win!");
		GameEndAttackersWin();
	}

	
	protected void RespawnAllSpectators()
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		array<PS_PlayableContainer> playableContainers = playableManager.GetPlayablesSorted();
		foreach (PS_PlayableContainer container : playableContainers)
		{
			PS_PlayableComponent pcomp = container.GetPlayableComponent();
			SCR_CharacterDamageManagerComponent damageManager = pcomp.GetCharacterDamageManagerComponent();
			EDamageState damageState = damageManager.GetState();
			if (damageState == EDamageState.DESTROYED)
			{
				int playerId = playableManager.GetPlayerByPlayableRemembered(pcomp.GetRplId());
				if (playerId == -1)
					continue;
				RespawnPlayer(playerId, pcomp);
			}
		}
	}
	
	protected void RespawnPlayer(int playerId, PS_PlayableComponent playableComponent)
	{
		if (playableComponent)
		{
			ResourceName prefabToSpawn = playableComponent.GetNextRespawn(false);
			if (prefabToSpawn != "")
			{
				PS_RespawnData respawnData = new PS_RespawnData(playableComponent, prefabToSpawn);
				Respawn(playerId, respawnData);
				return;
			}
		}

		SwitchToInitialEntity(playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoProgressToNextZone()
	{
		SCR_HintManagerComponent.GetInstance().ShowCustom("Zone progressing", "", 10, false);
		
	}
	
	//------------------------------------------------------------------------------------------------
	// Server side method to end game with winningFactionKey faction victory
	protected void GameEnd(FactionKey winningFactionKey)
	{
		Faction faction = m_FactionManager.GetFactionByKey(winningFactionKey);
		int factionId = m_FactionManager.GetFactionIndex(faction);
		SCR_GameModeEndData endData = SCR_GameModeEndData.CreateSimple(EGameOverTypes.ENDREASON_SCORELIMIT, winnerFactionId:factionId);
		EndGameMode(endData);
		m_bIsGameRunning = false;
		
		GetGame().GetCallqueue().Remove(MainGameLoop);
	}
	
	protected void GameEndDefendersWin()
	{
		Print("Defenders win!");
		GameEnd(m_sDefenderFactionKey);
	}

	protected void GameEndAttackersWin()
	{
		Print("Attackers win!");
		GameEnd(m_sAttackerFactionKey);
	}
	
	//------------------------------------------------------------------------------------------------
	protected int GetFactionRemainingPlayersCount(FactionKey fKey)
	{
		SCR_Faction faction = SCR_Faction.Cast(m_FactionManager.GetFactionByKey(fKey));
		if (!faction)
		{
			PrintFormat("Could not find faction %1", fKey, level:LogLevel.WARNING);
			return 0;
		}
		array<int> playerIds = new array<int>;
		
		faction.GetPlayersInFaction(playerIds);
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
				if (!damageManager.IsDestroyed()) {
					remainingPlayers++;
				}
			}
		}
		PrintFormat("Found %1 players in faction %2", remainingPlayers, fKey, level: LogLevel.SPAM);
		return remainingPlayers;
	}
	
	//------------------------------------------------------------------------------------------------
	// Public getters
	//------------------------------------------------------------------------------------------------
	
	int GetRedforScore()
	{
		return m_iAttackersRemaining;
	}
	
	int GetBluforScore()
	{
		return m_iDefendersRemaining;
	}
	
	int GetCurrentZone()
	{
		if (m_ZoneSystem)
			return m_ZoneSystem.GetCurrentZone();
		return -1;
	}
	
	bool IsGameRunning()
	{
		return m_bIsGameRunning;
	}
	
	bool IsTimerRunning()
	{
		if (m_ZoneSystem)
			return m_ZoneSystem.IsTimerRunning();
		return true;
	}
	
	bool IsWarmup()
	{
		if (m_ZoneSystem)
			return m_ZoneSystem.IsWarmup();
		return true;
	}
	
	WorldTimestamp GetVictoryTimestamp()
	{
		if (m_ZoneSystem)
			return m_ZoneSystem.GetZoneTimeoutTimestamp();
		
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp();
	}
	
	WorldTimestamp GetGameStartTimestamp()
	{
		return m_fStartTimestamp;
	}
	
	SCR_Faction GetBluforFaction()
	{
		return SCR_Faction.Cast(m_FactionManager.GetFactionByKey(m_sDefenderFactionKey));
	}
	
	SCR_Faction GetRedforFaction()
	{
		return SCR_Faction.Cast(m_FactionManager.GetFactionByKey(m_sAttackerFactionKey));
	}
	
	
}