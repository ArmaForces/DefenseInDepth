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
		else
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
			Print("Faction manager component is missing!", LogLevel.ERROR);

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
			m_ZoneSystem.GetOnZoneRepelled().Insert(OnZoneRepelled);
			m_ZoneSystem.GetOnZoneFailed().Insert(OnZoneFailed);
			m_ZoneSystem.GetOnWaveCompleted().Insert(OnWaveCompleted);
		}
	}

	override void OnGameStateChanged()
	{
		super.OnGameStateChanged();

		SCR_EGameModeState state = GetState();
		if (state != SCR_EGameModeState.GAME)
			return;

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
		GetGame().GetCallqueue().CallLater(RespawnAllSpectators, 1000 * 5);

		RPC_DoProgressToNextZone(m_iCurrentZone);
		Rpc(RPC_DoProgressToNextZone, m_iCurrentZone);
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
		RPC_DoZoneHeld();
		Rpc(RPC_DoZoneHeld);
		GameEndDefendersWin();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoZoneHeld()
	{
		SCR_ChatComponent.RadioProtocolMessage("Sector secured. Enemy assault has failed. Outstanding work, all units.");
	}

	//! Attacker budget exhausted and zone cleared — defenders repelled the assault
	protected void OnZoneRepelled()
	{
		RPC_DoZoneRepelled();
		Rpc(RPC_DoZoneRepelled);
		GameEndDefendersWin();
	}

	//! All defenders eliminated — zone failed, progressing to next
	protected void OnZoneFailed(int zoneIndex)
	{
		RPC_DoZoneFailed(zoneIndex);
		Rpc(RPC_DoZoneFailed, zoneIndex);
	}

	//! Wave zone wave cleared
	protected void OnWaveCompleted(int currentWave, int totalWaves)
	{
		RPC_DoWaveCompleted(currentWave, totalWaves);
		Rpc(RPC_DoWaveCompleted, currentWave, totalWaves);
	}

	protected void UpdateLocalGameState()
	{
		m_iCurrentZone = m_ZoneSystem.GetCurrentZoneIndex();
		m_bIsWarmup = m_ZoneSystem.IsWarmup();
		m_bIsTimerRunning = m_ZoneSystem.IsTimerRunning();
		m_iAttackersRemaining = m_ZoneSystem.GetRedforScore();
		m_iDefendersRemaining = m_ZoneSystem.GetBluforScore();
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

	//------------------------------------------------------------------------------------------------
	// Commander radio notifications — called on server, broadcast to all clients
	//------------------------------------------------------------------------------------------------

	//! Called when zone transitions from PREPARE to ACTIVE (defend phase starts).
	void NotifyZoneActive(int zoneIndex)
	{
		RPC_DoZoneActive(zoneIndex);
		Rpc(RPC_DoZoneActive, zoneIndex);
	}

	//! Called by director when attack phase changes to ASSAULT or FINAL.
	void NotifyPhaseChanged(EAFMAttackPhase phase)
	{
		RPC_DoPhaseChanged(phase);
		Rpc(RPC_DoPhaseChanged, phase);
	}

	//! Called by director when a combined-arms smoke sequence starts (step 1 fired).
	void NotifySmokeLaunched(bool isArmorPush)
	{
		RPC_DoSmokeLaunched(isArmorPush);
		Rpc(RPC_DoSmokeLaunched, isArmorPush);
	}

	//! Called by zone when all ticket-based spawners run out of tickets.
	void NotifyTicketsExhausted()
	{
		RPC_DoTicketsExhausted();
		Rpc(RPC_DoTicketsExhausted);
	}

	//! Called by artillery when an HE fire mission is triggered.
	void NotifyMortarFiring()
	{
		RPC_DoMortarFiring();
		Rpc(RPC_DoMortarFiring);
	}

	//! Called by AFM_DiDZoneArtillery on server when the mortar is destroyed.
	void NotifyMortarDestroyed()
	{
		RPC_DoMortarDestroyed();
		Rpc(RPC_DoMortarDestroyed);
	}

	//------------------------------------------------------------------------------------------------
	// Broadcast RPCs
	//------------------------------------------------------------------------------------------------

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoZoneActive(int zoneIndex)
	{
		SCR_ChatComponent.RadioProtocolMessage(
			string.Format("Command to all units: sector %1 is now active. Enemy is advancing — hold your positions.", zoneIndex)
		);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoPhaseChanged(EAFMAttackPhase phase)
	{
		if (phase == EAFMAttackPhase.ASSAULT)
			SCR_ChatComponent.RadioProtocolMessage("Attention: enemy has committed their main force. Expect heavy contact.");
		else if (phase == EAFMAttackPhase.FINAL)
			SCR_ChatComponent.RadioProtocolMessage("All units: enemy is expending their last reserves. This is their final push — do not break.");
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoSmokeLaunched(bool isArmorPush)
	{
		if (isArmorPush)
			SCR_ChatComponent.RadioProtocolMessage("SMOKE SCREEN downrange! Enemy armor moving up — AT teams stand by!");
		else
			SCR_ChatComponent.RadioProtocolMessage("SMOKE SCREEN downrange! Enemy infantry assault through smoke — brace for contact!");
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoTicketsExhausted()
	{
		SCR_ChatComponent.RadioProtocolMessage("Intel: enemy reserves are depleted. No further reinforcements inbound.");
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoMortarFiring()
	{
		SCR_ChatComponent.RadioProtocolMessage("INCOMING! Enemy fire support is active. Seek cover and locate the source!");
	}

	//! Enemy mortar destroyed — defenders notified, no position information given
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoMortarDestroyed()
	{
		SCR_ChatComponent.RadioProtocolMessage("Enemy fire support eliminated. Well done.");
	}

	//! Defenders repelled the assault (budget exhausted + zone cleared)
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoZoneRepelled()
	{
		SCR_ChatComponent.RadioProtocolMessage("Enemy force annihilated. The assault has been broken. Hold position.");
	}

	//! Zone failed — all defenders eliminated
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoZoneFailed(int zoneIndex)
	{
		SCR_ChatComponent.RadioProtocolMessage("Position overrun. All units fall back and regroup at the next position.");
	}

	//! New zone is now active
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoProgressToNextZone(int newZoneIndex)
	{
		SCR_ChatComponent.RadioProtocolMessage(string.Format("Moving to zone %1. Prepare your defenses!", newZoneIndex));
	}

	//! Wave zone wave cleared
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoWaveCompleted(int currentWave, int totalWaves)
	{
		string msg;
		if (currentWave >= totalWaves)
			msg = string.Format("All %1 waves defeated! Hold position!", totalWaves);
		else
			msg = string.Format("Wave %1/%2 defeated! Prepare for the next wave!", currentWave, totalWaves);

		SCR_HintManagerComponent.GetInstance().ShowCustom(msg, "", 10, false);
	}

	//------------------------------------------------------------------------------------------------
	// Game end
	//------------------------------------------------------------------------------------------------

	protected void GameEnd(FactionKey winningFactionKey)
	{
		Faction faction = m_FactionManager.GetFactionByKey(winningFactionKey);
		int factionId = m_FactionManager.GetFactionIndex(faction);
		SCR_GameModeEndData endData = SCR_GameModeEndData.CreateSimple(EGameOverTypes.ENDREASON_SCORELIMIT, winnerFactionId: factionId);
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
