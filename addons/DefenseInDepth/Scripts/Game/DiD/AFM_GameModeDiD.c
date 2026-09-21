class AFM_GameModeDiDClass: PS_GameModeCoopClass
{
}

class AFM_GameModeDiD: PS_GameModeCoop
{
	[Attribute("US", UIWidgets.EditBox, "Defenders faction key", category: "DiD")]
	protected FactionKey m_sDefenderFactionKey;
	
	[Attribute("USSR", UIWidgets.EditBox, "Attackers faction key", category: "DiD")]
	protected FactionKey m_sAttackerFactionKey;	
	
	// Dead bodies are inserted into the garbage system on death; withdraw them shortly after
	protected static const int BODY_WITHDRAW_DELAY_MS = 500;

	// PS switches the player into the new body four frames after the respawn request
	protected static const int RANK_RESTORE_FIRST_DELAY_MS = 300;
	protected static const int RESPAWN_FINALIZE_DELAY_MS = 2000;

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

	// HUD status line
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iZoneNumber = 0;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iZoneCount = 0;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iWave = 0;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iWaveCount = 0;	// 0 outside wave zones

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iEnemiesRemaining = -1;	// -1 when spawns are unlimited

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iTicketsRemaining = -1;	// -1 when no spawner of the zone uses tickets

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bIsContested = false;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bHasNextSpawnWave = false;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected WorldTimestamp m_NextSpawnWaveTimestamp;

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

	//! Called by AFM_CallExtractionAction. Ignored unless the active zone is an extraction zone.
	void CallExtraction()
	{
		if (m_ZoneSystem)
			DoCallExtraction();
		else //no zone system - assume we are a proxy
			Rpc(RPC_DoCallExtraction);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_DoCallExtraction()
	{
		DoCallExtraction();
	}

	protected void DoCallExtraction()
	{
		if (!m_ZoneSystem)
			return;

		AFM_DiDExtractionZoneComponent zone = AFM_DiDExtractionZoneComponent.Cast(m_ZoneSystem.GetActiveZone());
		if (!zone)
		{
			PrintFormat("AFM_GameModeDiD: Extraction called but the active zone is not an extraction zone", level: LogLevel.WARNING);
			return;
		}

		zone.RequestExtraction();
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
	
	//! Fired when a zone enters its prepare phase, when its attack starts and when a wave is cleared
	protected void OnZoneChanged()
	{
		UpdateLocalGameState();
		// Respawn dead players when zone changes
		GetGame().GetCallqueue().CallLater(RespawnAllSpectators, 1000 * 5);

		// Wave clears have their own hint (OnWaveCompleted)
		if (m_bIsWarmup)
		{
			RPC_DoProgressToNextZone(m_iZoneNumber);
			Rpc(RPC_DoProgressToNextZone, m_iZoneNumber);
		}
		else if (IsActiveZoneInState(EAFMZoneState.ACTIVE))
		{
			RPC_DoAttackStarted(m_iZoneNumber);
			Rpc(RPC_DoAttackStarted, m_iZoneNumber);
		}

		OnMatchSituationChanged();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsActiveZoneInState(EAFMZoneState state)
	{
		if (!m_ZoneSystem)
			return false;

		AFM_DiDZoneComponent zone = m_ZoneSystem.GetActiveZone();
		return zone && zone.GetZoneState() == state;
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
	
	//! Called when all defenders in a zone are eliminated - fires before zone progression
	protected void OnZoneFailed(int zoneIndex)
	{
		RPC_DoZoneFailed(zoneIndex);
		Rpc(RPC_DoZoneFailed, zoneIndex);
	}
	
	//! Called when a wave zone wave is cleared
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
		m_bIsContested = m_ZoneSystem.IsContested();
		m_iZoneCount = m_ZoneSystem.GetMaxZoneIndex();

		AFM_DiDZoneComponent zone = m_ZoneSystem.GetActiveZone();
		if (!zone)
			return;

		m_iZoneNumber = zone.GetZoneIndex();
		m_iEnemiesRemaining = zone.GetEnemiesRemaining();
		m_iTicketsRemaining = zone.GetRemainingSpawnTickets();

		// Spawners only send waves while the zone is being fought over
		EAFMZoneState state = zone.GetZoneState();
		m_bHasNextSpawnWave = false;
		if (state == EAFMZoneState.ACTIVE || state == EAFMZoneState.FROZEN)
			m_bHasNextSpawnWave = zone.GetNextSpawnWaveTime(m_NextSpawnWaveTimestamp);

		m_iWave = 0;
		m_iWaveCount = 0;
		AFM_DiDWaveZoneComponent waveZone = AFM_DiDWaveZoneComponent.Cast(zone);
		if (waveZone)
		{
			m_iWave = waveZone.GetCurrentWave();
			m_iWaveCount = waveZone.GetTotalWaves();
		}
	}

	
	//------------------------------------------------------------------------------------------------
	//! Keep dead bodies out of the garbage system. Deleting a body unregisters its playable, and the
	//! player would then be missing from the list below and never respawned at the next zone.
	override protected void OnPlayerKilled(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		super.OnPlayerKilled(playerId, playerEntity, killerEntity, killer);

		// The body is inserted into the garbage system on death, so withdraw it right after
		GetGame().GetCallqueue().CallLater(KeepBody, BODY_WITHDRAW_DELAY_MS, false, playerEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void KeepBody(IEntity body)
	{
		if (!body)
			return;

		SCR_GarbageSystem garbageSystem = SCR_GarbageSystem.GetByEntityWorld(body);
		if (garbageSystem)
			garbageSystem.Withdraw(body);
	}

	//------------------------------------------------------------------------------------------------
	//! Players without a living body: dead, or holding no playable at all
	void GetSpectatorPlayerIds(notnull array<int> outPlayerIds)
	{
		outPlayerIds.Clear();

		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager)
			return;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			RplId playableId = playableManager.GetPlayableByPlayer(playerId);
			if (playableId == RplId.Invalid())
			{
				outPlayerIds.Insert(playerId);
				continue;
			}

			PS_PlayableContainer container = playableManager.GetPlayableById(playableId);
			if (!container || container.GetDamageState() == EDamageState.DESTROYED)
				outPlayerIds.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Put a player into a playable, used for respawns and for the attacker squad
	void SwitchPlayerToPlayable(int playerId, RplId playableId)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager)
			return;

		playableManager.SetPlayerPlayable(playerId, playableId);
		playableManager.ForceSwitch(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Broadcast a hint to every player
	void ShowHint(string message, int duration = 10)
	{
		RPC_DoShowHint(message, duration);
		Rpc(RPC_DoShowHint, message, duration);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoShowHint(string message, int duration)
	{
		SCR_HintManagerComponent.GetInstance().ShowCustom(message, "", duration, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void RespawnAllSpectators()
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		array<PS_PlayableContainer> playableContainers = playableManager.GetPlayablesSorted();
		AFM_PlayerSpawnPointEntity currentSpawnPoint = m_ZoneSystem.GetCurrentZonePlayerSpawnPoint();


		foreach (PS_PlayableContainer container : playableContainers)
		{
			if (!container)
				continue;

			// A playable whose entity is already gone can't be respawned, and must not stop the others
			PS_PlayableComponent pcomp = container.GetPlayableComponent();
			if (!pcomp)
				continue;

			SCR_CharacterDamageManagerComponent damageManager = pcomp.GetCharacterDamageManagerComponent();
			if (!damageManager)
				continue;

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
				PS_RespawnData respawnData = new PS_RespawnData(playableComponent, prefabToSpawn, "");

				if (sp)
					respawnData.m_aSpawnTransform[3] = sp.GetOrigin();

				Respawn(playerId, respawnData);

				// The player is switched into the new body a few frames later
				IEntity oldBody = playableComponent.GetOwner();
				SCR_ECharacterRank previousRank = SCR_CharacterRankComponent.GetCharacterRank(oldBody);
				
				// Restore the rank as soon as the player holds the new body, then again in case anything
				// reads or overwrites it while the respawn finishes
				GetGame().GetCallqueue().CallLater(RestorePlayerRank, RANK_RESTORE_FIRST_DELAY_MS, false, playerId, previousRank);
				GetGame().GetCallqueue().CallLater(OnPlayerRespawned, RESPAWN_FINALIZE_DELAY_MS, false, playerId, oldBody, previousRank);
				return;
			}
		}

		SwitchToInitialEntity(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Restore the rank the player earned and remove the body kept for this respawn
	protected void OnPlayerRespawned(int playerId, IEntity oldBody, SCR_ECharacterRank previousRank)
	{
		RestorePlayerRank(playerId, previousRank);

		if (oldBody)
			SCR_EntityHelper.DeleteEntityAndChildren(oldBody);
	}

	//------------------------------------------------------------------------------------------------
	//! A respawned character starts at the rank of its prefab: the PS framework spawns it directly and
	//! skips the vanilla spawn flow that applies the rank matching the player's XP. Carry the rank of the
	//! previous body over, then let the XP handler raise it further if it can.
	protected void RestorePlayerRank(int playerId, SCR_ECharacterRank previousRank)
	{
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
		{
			PrintFormat("AFM_GameModeDiD: No player controller for player %1, rank not restored", playerId, level: LogLevel.WARNING);
			return;
		}

		IEntity character = playerController.GetControlledEntity();
		if (!character)
		{
			PrintFormat("AFM_GameModeDiD: Player %1 controls no entity, rank not restored", playerId, level: LogLevel.WARNING);
			return;
		}

		SCR_CharacterRankComponent rankComponent = SCR_CharacterRankComponent.GetCharacterRankComponent(character);
		if (!rankComponent)
		{
			PrintFormat("AFM_GameModeDiD: Player %1 respawned into an entity without a rank component", playerId, level: LogLevel.WARNING);
			return;
		}

		SCR_ECharacterRank spawnedRank = SCR_CharacterRankComponent.GetCharacterRank(character);
		if (previousRank > spawnedRank)
			rankComponent.SetCharacterRank(previousRank, true);

		SCR_PlayerXPHandlerComponent xpHandler = SCR_PlayerXPHandlerComponent.Cast(playerController.FindComponent(SCR_PlayerXPHandlerComponent));
		if (xpHandler)
			xpHandler.UpdatePlayerRank(false);

		PrintFormat("AFM_GameModeDiD: Player %1 respawned with rank %2 (previous %3, spawned %4, XP handler present: %5)",
			playerId, SCR_CharacterRankComponent.GetCharacterRank(character), previousRank, spawnedRank, xpHandler != null);
	}
	
	//! Broadcast: zone failure - shown before zone progression hint
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoZoneFailed(int zoneIndex)
	{
		SCR_HintManagerComponent.GetInstance().ShowCustom(
			string.Format("Zone %1 lost! Falling back...", zoneIndex),
			"",
			6,
			false
		);
	}
	
	//! Broadcast: next zone is now active
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoProgressToNextZone(int newZoneIndex)
	{
		SCR_HintManagerComponent.GetInstance().ShowCustom(
			string.Format("Moving to zone %1. Prepare your defenses!", newZoneIndex),
			"",
			10,
			false
		);
	}
	
	//! Broadcast: prepare phase is over, the enemy attack begins
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoAttackStarted(int zoneIndex)
	{
		SCR_HintManagerComponent.GetInstance().ShowCustom(
			string.Format("Zone %1: the enemy attack has begun!", zoneIndex),
			"",
			10,
			false
		);
	}

	//! Broadcast: wave cleared in a wave zone
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

	int GetZoneNumber()
	{
		return m_iZoneNumber;
	}

	int GetZoneCount()
	{
		return m_iZoneCount;
	}

	int GetWave()
	{
		return m_iWave;
	}

	//! 0 outside wave zones
	int GetWaveCount()
	{
		return m_iWaveCount;
	}

	//! -1 when spawns are unlimited
	int GetEnemiesRemaining()
	{
		return m_iEnemiesRemaining;
	}

	//! Spawn tickets left in the active zone, or -1 when none of its spawners use tickets
	int GetTicketsRemaining()
	{
		return m_iTicketsRemaining;
	}

	//! Attackers hold the majority inside the zone and the timer is stopped
	bool IsContested()
	{
		return m_bIsContested;
	}

	//! \return false when no timed enemy wave is coming
	bool GetNextSpawnWaveTime(out WorldTimestamp nextTime)
	{
		nextTime = m_NextSpawnWaveTimestamp;
		return m_bHasNextSpawnWave;
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
