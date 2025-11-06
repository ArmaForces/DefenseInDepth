class AFM_GameModeDiDClass: PS_GameModeCoopClass
{
}

class AFM_GameModeDiD: PS_GameModeCoop
{
	[Attribute("US", UIWidgets.EditBox, "Defenders faction key", category: "DiD")]
	protected FactionKey m_sDefenderFactionKey;
	
	[Attribute("USSR", UIWidgets.EditBox, "Attackers faction key", category: "DiD")]
	protected FactionKey m_sAttackerFactionKey;	
	
	protected SCR_FactionManager m_FactionManager;
	protected AFM_DiDZoneSystem m_ZoneSystem;
	protected ref ScriptInvoker m_OnMatchSituationChanged;
	
	protected bool m_bShowUI = false;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bIsGameRunning = false;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bIsWarmup = false;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bIsTimerRunning = false;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected WorldTimestamp m_fTimeoutTimestamp;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iDefendersRemaining = 0;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iAttackersRemaining = 0;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iCurrentZone = 0;
	
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
	
	void ForceEndPrepareStage()
	{
		if (m_ZoneSystem)
			m_ZoneSystem.ForceEndPrepareStage();
		else //no zone system - assume we are a proxy
			Rpc(RPC_DoForceEndPrepareStage);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_DoForceEndPrepareStage()
	{
		if (!m_ZoneSystem)
			return;
		m_ZoneSystem.ForceEndPrepareStage();
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
			Print("AFM_DiDZoneSystem is missing", LogLevel.ERROR);
		}
		else
		{
			m_ZoneSystem.GetOnZoneChanged().Insert(OnZoneChanged);
			m_ZoneSystem.GetOnZoneUpdate().Insert(OnZoneUpdate);
			m_ZoneSystem.GetOnAllZonesCompleted().Insert(OnAllZonesCompleted);
			m_ZoneSystem.GetOnZoneHeld().Insert(OnZoneHeld);
		}
	}
	
	
	override void OnGameStateChanged()
	{
		super.OnGameStateChanged();
		
		SCR_EGameModeState state = GetState();
		if (state != SCR_EGameModeState.GAME)
			return;
		
		ChimeraWorld world = GetGame().GetWorld();
		m_bIsGameRunning = true;
		m_bShowUI = true;
		
		if (m_ZoneSystem)
			m_ZoneSystem.StartZoneSystem();
		
		OnMatchSituationChanged();
		Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	// Zone system callbacks
	//------------------------------------------------------------------------------------------------
	
	protected void OnZoneChanged()
	{
		UpdateLocalGameState();
		// Respawn dead players when zone changes
		GetGame().GetCallqueue().CallLater(RespawnAllSpectators, 1000 * 5);

		RPC_DoProgressToNextZone();
		Rpc(RPC_DoProgressToNextZone);
		OnMatchSituationChanged();
		Replication.BumpMe();
	}
	
	protected void OnZoneUpdate()
	{
		UpdateLocalGameState();
		OnMatchSituationChanged();
		Replication.BumpMe();
	}
	
	protected void OnAllZonesCompleted()
	{
		GameEndAttackersWin();
	}
	
	protected void OnZoneHeld()
	{
		GameEndDefendersWin();
	}
	
	protected void UpdateLocalGameState()
	{
		m_iCurrentZone = m_ZoneSystem.GetCurrentZoneIndex();
		m_bIsWarmup = m_ZoneSystem.IsWarmup();
		m_bIsTimerRunning = m_ZoneSystem.IsTimerRunning();
		m_iAttackersRemaining = m_ZoneSystem.GetAICountInCurrentZone();
		m_iDefendersRemaining = m_ZoneSystem.GetDefenderCount();
		m_fTimeoutTimestamp = m_ZoneSystem.GetZoneTimeoutTimestamp();
	}

	
	protected void RespawnAllSpectators()
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		array<PS_PlayableContainer> playableContainers = playableManager.GetPlayablesSorted();
		AFM_PlayerSpawnPointEntity currentSpawnPoint = m_ZoneSystem.GetCurrentZonePlayerSpawnPoint();
		
		
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
				RespawnPlayer(playerId, pcomp, currentSpawnPoint);
			}
		}
	}
	
	protected void RespawnPlayer(int playerId, PS_PlayableComponent playableComponent, AFM_PlayerSpawnPointEntity sp)
	{
		if (playableComponent)
		{
			ResourceName prefabToSpawn = playableComponent.GetNextRespawn(false);
			if (prefabToSpawn != "")
			{
				PS_RespawnData respawnData = new PS_RespawnData(playableComponent, prefabToSpawn);
				
				if (sp)
					respawnData.m_aSpawnTransform[3] = sp.GetOrigin();
				
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
	// Public getters
	//------------------------------------------------------------------------------------------------
	
	int GetAttackersRemaining()
	{
		return m_iAttackersRemaining;
	}
	
	int GetDefendersRemaining()
	{
		return m_iDefendersRemaining;
	}
	
	int GetCurrentZone()
	{
		return m_iCurrentZone;
	}
	
	bool IsGameRunning()
	{
		return m_bIsGameRunning;
	}
	
	bool IsTimerRunning()
	{
		return m_bIsTimerRunning;
	}
	
	bool IsWarmup()
	{
		return m_bIsWarmup;
	}
	
	bool ShowUI()
	{
		return m_bShowUI;
	}
	
	WorldTimestamp GetTimeoutTimestamp()
	{
		return m_fTimeoutTimestamp;
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