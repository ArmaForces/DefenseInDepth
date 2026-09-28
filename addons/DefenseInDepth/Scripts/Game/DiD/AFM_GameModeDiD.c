class AFM_GameModeDiDClass: PS_GameModeCoopClass
{
}

class AFM_GameModeDiD: PS_GameModeCoop
{
	[Attribute("{2FF4CFE9D80F6F76}Configs/Factions/DiD_Side_US.conf", UIWidgets.ResourceNamePicker, "The side the players defend as: its faction, its extraction helicopter and that helicopter's crew", params: "conf class=AFM_DiDSideConfig", category: "DiD")]
	protected ResourceName m_sDefenderConfigPath;

	[Attribute("{CB5CAE38E68AEBE5}Configs/Factions/DiD_Side_USSR.conf", UIWidgets.ResourceNamePicker, "The side that attacks: its faction, infantry groups, vehicles, mortars, helicopters and COWABUNGA squad", params: "conf class=AFM_DiDSideConfig", category: "DiD")]
	protected ResourceName m_sAttackerConfigPath;

	// Loaded at EOnInit, well before any zone initialises and reads them
	protected ref AFM_DiDSideConfig m_DefenderConfig;
	protected ref AFM_DiDSideConfig m_AttackerConfig;
	
	[Attribute("1", UIWidgets.CheckBox, "Re-equip the loadout a player saved at an arsenal when they respawn", category: "DiD")]
	protected bool m_bApplySavedLoadouts;
	
	// PS switches the player into the new body four frames after the respawn request
	protected static const int RANK_RESTORE_FIRST_DELAY_MS = 300;
	protected static const int RESPAWN_FINALIZE_DELAY_MS = 2000;

	// Players arriving in a new zone are placed on rings around its spawn point
	protected static const int SPAWN_RING_SIZE = 6;
	protected static const float SPAWN_SPACING_M = 3.0;

	// Let the new zone settle before moving anyone into it
	protected static const int ZONE_TRANSFER_DELAY_MS = 5000;

	// Zone the last transfer was made for, so survivors are only moved when the stage actually changes
	protected int m_iLastTransferZoneIndex = -1;

	// The one group every player belongs to, made with the first body of the match
	protected SCR_AIGroup m_PlayerGroup;

	// How many bodies have been handed out, which is also the next index into the side config's list
	protected int m_iBodiesHandedOut;

	// A freshly spawned playable needs a moment to register before a player can be put in it
	protected static const int ASSIGN_DELAY_MS = 500;
	protected static const int ASSIGN_MAX_ATTEMPTS = 10;

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
	protected int m_iContestedSecondsLeft = -1;	// -1 when the zone cannot be lost by being held

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
	
	//! The admin check behind this lives in AFM_VoteSkipWarmupAction.CanBeShownScript, which only hides
	//! the action locally - a client can still send this RPC and cut the prepare phase short. Left as it
	//! is on purpose: the worst case is a stage starting early on a server whose players we know.
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
		
		LoadSideConfigs();
		
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
	//! Who the two sides are comes from a config file per side rather than from attributes here, so a
	//! scenario can be re-sided without re-authoring it: the zones, spawn points and timings do not care
	//! who is attacking.
	//!
	//! Loaded on server and client alike. These are files, identical on every machine, and the HUD needs
	//! the faction keys as much as the spawners need the prefabs - loading them only on the authority
	//! would leave clients with no flags.
	//------------------------------------------------------------------------------------------------
	protected void LoadSideConfigs()
	{
		// A scenario may name the two sides itself, which is how one game mode prefab serves every pairing
		ResourceName defenderPath = AFM_DiDScenarioSettings.GetDefenderConfig();
		if (defenderPath.IsEmpty())
			defenderPath = m_sDefenderConfigPath;
		else
			PrintFormat("AFM_GameModeDiD: Defending side comes from the scenario: %1", defenderPath);

		ResourceName attackerPath = AFM_DiDScenarioSettings.GetAttackerConfig();
		if (attackerPath.IsEmpty())
			attackerPath = m_sAttackerConfigPath;
		else
			PrintFormat("AFM_GameModeDiD: Attacking side comes from the scenario: %1", attackerPath);

		m_DefenderConfig = SCR_ConfigHelperT<AFM_DiDSideConfig>.GetConfigObject(defenderPath);
		m_AttackerConfig = SCR_ConfigHelperT<AFM_DiDSideConfig>.GetConfigObject(attackerPath);

		if (!m_DefenderConfig)
			PrintFormat("AFM_GameModeDiD: Defender side config '%1' could not be loaded, the defending side will not work",
				defenderPath, level: LogLevel.ERROR);
		else
			m_DefenderConfig.ValidateFactionKey("defender");

		if (!m_AttackerConfig)
			PrintFormat("AFM_GameModeDiD: Attacker side config '%1' could not be loaded, nothing will attack",
				attackerPath, level: LogLevel.ERROR);
		else
			m_AttackerConfig.ValidateFactionKey("attacker");

		if (m_DefenderConfig && m_AttackerConfig)
			PrintFormat("AFM_GameModeDiD: %1 defending against %2",
				m_DefenderConfig.GetLabel(), m_AttackerConfig.GetLabel());
	}

	//------------------------------------------------------------------------------------------------
	AFM_DiDSideConfig GetDefenderConfig()
	{
		return m_DefenderConfig;
	}

	//------------------------------------------------------------------------------------------------
	AFM_DiDSideConfig GetAttackerConfig()
	{
		return m_AttackerConfig;
	}

	//------------------------------------------------------------------------------------------------
	FactionKey GetDefenderFactionKey()
	{
		if (!m_DefenderConfig)
			return string.Empty;

		return m_DefenderConfig.m_sFactionKey;
	}

	//------------------------------------------------------------------------------------------------
	FactionKey GetAttackerFactionKey()
	{
		if (!m_AttackerConfig)
			return string.Empty;

		return m_AttackerConfig.m_sFactionKey;
	}

	//------------------------------------------------------------------------------------------------
	// Zone system callbacks
	//------------------------------------------------------------------------------------------------
	
	//! Fired when a zone enters its prepare phase, when its attack starts and when a wave is cleared
	protected void OnZoneChanged()
	{
		UpdateLocalGameState();

		// OnZoneChanged also fires when the same zone goes from prepare to active and when a wave is
		// cleared. Dead players are respawned every time, but survivors are only moved when the match
		// has actually progressed to a different zone - otherwise everyone is teleported to the spawn
		// point moments after the first zone starts.
		bool zoneProgressed = m_iLastTransferZoneIndex >= 0 && m_iZoneNumber != m_iLastTransferZoneIndex;
		m_iLastTransferZoneIndex = m_iZoneNumber;

		GetGame().GetCallqueue().CallLater(PopulateZone, ZONE_TRANSFER_DELAY_MS, false, zoneProgressed);

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
		m_iContestedSecondsLeft = zone.GetRemainingFailureSeconds();

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
	//! Players without a living body: dead, or holding no playable at all.
	//!
	//! A player with no playable at all counts here too, which is what every player is at the start of a
	//! match now that the world holds no player prefabs.
	//!
	//! Corpses are cleared in ClearOldBody once their owner holds a new body. They no longer have to
	//! survive until then - this list follows player ids, not playables - but the garbage collector stays
	//! off for the game mode, because a body collected from under a player mid-stage is worse.
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
	//! Everyone who has no living body gets one at the stage's spawn point; survivors keep theirs and are
	//! moved there, so nobody is left behind in the stage that just ended.
	//!
	//! The world holds no player prefabs at all. What the players are is content like anything else, so it
	//! comes from the defending side's config, and a mission does not have to be re-authored to be
	//! re-sided. Bodies are spawned and handed over the way AFM_DiDCowabungaComponent does it for the
	//! attackers, rather than through PS_GameModeCoop.Respawn, which needs a placed playable to respawn
	//! from.
	protected void PopulateZone(bool moveSurvivors = false)
	{
		if (!m_ZoneSystem)
			return;

		AFM_PlayerSpawnPointEntity currentSpawnPoint = m_ZoneSystem.GetCurrentZonePlayerSpawnPoint();
		int spawnIndex = 0;

		if (moveSurvivors)
			spawnIndex = MoveSurvivors(currentSpawnPoint, spawnIndex);

		array<int> spectators = {};
		GetSpectatorPlayerIds(spectators);

		foreach (int playerId : spectators)
		{
			if (SpawnPlayerBody(playerId, GetSpawnPosition(currentSpawnPoint, spawnIndex)))
				spawnIndex++;
		}

		if (!spectators.IsEmpty())
			PrintFormat("AFM_GameModeDiD: Spawning bodies for %1 players at the stage's spawn point", spectators.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Move everyone who lived through the stage to the new spawn point
	//! \return the next free position in the spawn ring
	protected int MoveSurvivors(AFM_PlayerSpawnPointEntity spawnPoint, int spawnIndex)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager)
			return spawnIndex;

		array<PS_PlayableContainer> playableContainers = playableManager.GetPlayablesSorted();
		foreach (PS_PlayableContainer container : playableContainers)
		{
			if (!container || container.GetDamageState() == EDamageState.DESTROYED)
				continue;

			PS_PlayableComponent pcomp = container.GetPlayableComponent();
			if (!pcomp)
				continue;

			// Only bodies somebody is actually holding: the rest are corpses waiting to be cleared
			if (playableManager.GetPlayerByPlayable(pcomp.GetRplId()) <= 0)
				continue;

			if (MoveSurvivorToSpawnPoint(pcomp, GetSpawnPosition(spawnPoint, spawnIndex)))
				spawnIndex++;
		}

		return spawnIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn one body from the defending side's config and put the player in it
	//! \return false when nothing could be spawned
	protected bool SpawnPlayerBody(int playerId, vector spawnPos)
	{
		if (!m_DefenderConfig)
		{
			PrintFormat("AFM_GameModeDiD: No defender side config, players cannot be given bodies", level: LogLevel.ERROR);
			return false;
		}

		ResourceName prefab = m_DefenderConfig.GetPlayerCharacter(m_iBodiesHandedOut);
		if (prefab.IsEmpty())
		{
			PrintFormat("AFM_GameModeDiD: The defending side (%1) has no player characters, nobody can spawn",
				m_DefenderConfig.GetLabel(), level: LogLevel.ERROR);
			return false;
		}

		if (!EnsurePlayerGroup(spawnPos))
			return false;

		vector spawnTransform[4];
		Math3D.MatrixIdentity4(spawnTransform);
		spawnTransform[3] = spawnPos;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform = spawnTransform;

		IEntity body = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		if (!body)
		{
			PrintFormat("AFM_GameModeDiD: Failed to spawn player body %1", prefab, level: LogLevel.ERROR);
			return false;
		}

		PS_PlayableComponent playable = PS_PlayableComponent.Cast(body.FindComponent(PS_PlayableComponent));
		if (!playable)
		{
			PrintFormat("AFM_GameModeDiD: %1 is not playable - the side config needs the PlayableSelector _P prefabs",
				prefab, level: LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(body);
			return false;
		}

		m_PlayerGroup.AddAIEntityToGroup(body);
		playable.SetPlayable(true);
		m_iBodiesHandedOut++;

		// The corpse this player is leaving behind, cleared once they hold the new body. Their rank goes
		// with them: a fresh body starts at whatever its prefab says.
		IEntity oldBody = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		SCR_ECharacterRank previousRank = SCR_CharacterRankComponent.GetCharacterRank(oldBody);

		GetGame().GetCallqueue().CallLater(AssignPlayerBody, ASSIGN_DELAY_MS, false, playerId, body, previousRank, 0);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every player is in one group, made the first time anyone needs a body. One group keeps the whole
	//! team on one map marker set and in one voice room, which is what a defence of this shape wants.
	//! \return false when the side config has no group to make
	protected bool EnsurePlayerGroup(vector spawnPos)
	{
		if (m_PlayerGroup)
			return true;

		ResourceName groupPrefab = m_DefenderConfig.m_sPlayerGroup;
		if (groupPrefab.IsEmpty())
		{
			PrintFormat("AFM_GameModeDiD: The defending side (%1) has no player group, nobody can spawn",
				m_DefenderConfig.GetLabel(), level: LogLevel.ERROR);
			return false;
		}

		vector groupTransform[4];
		Math3D.MatrixIdentity4(groupTransform);
		groupTransform[3] = spawnPos;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform = groupTransform;

		m_PlayerGroup = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(groupPrefab), GetGame().GetWorld(), spawnParams));
		if (!m_PlayerGroup)
		{
			PrintFormat("AFM_GameModeDiD: Failed to spawn the player group %1", groupPrefab, level: LogLevel.ERROR);
			return false;
		}

		// 0 means no limit: the whole server goes in here
		m_PlayerGroup.SetMaxMembers(0);

		PrintFormat("AFM_GameModeDiD: Player group %1 created for the %2 side",
			groupPrefab, m_DefenderConfig.GetLabel());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Hand a spawned body to its player. The playable registers a moment after it spawns, so this retries
	//! until it has an id to switch to.
	protected void AssignPlayerBody(int playerId, IEntity body, SCR_ECharacterRank previousRank, int attempt)
	{
		if (!body)
			return;

		PS_PlayableComponent playable = PS_PlayableComponent.Cast(body.FindComponent(PS_PlayableComponent));
		if (!playable)
			return;

		RplId playableId = playable.GetRplId();
		if (!playableId.IsValid())
		{
			if (attempt < ASSIGN_MAX_ATTEMPTS)
				GetGame().GetCallqueue().CallLater(AssignPlayerBody, ASSIGN_DELAY_MS, false, playerId, body, previousRank, attempt + 1);
			else
				PrintFormat("AFM_GameModeDiD: Body for player %1 never registered as playable", playerId, level: LogLevel.ERROR);

			return;
		}

		IEntity oldBody = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		SwitchPlayerToPlayable(playerId, playableId);

		GetGame().GetCallqueue().CallLater(RestorePlayerRank, RANK_RESTORE_FIRST_DELAY_MS, false, playerId, previousRank);
		GetGame().GetCallqueue().CallLater(ApplySavedLoadout, RANK_RESTORE_FIRST_DELAY_MS, false, playerId);
		GetGame().GetCallqueue().CallLater(ClearOldBody, RESPAWN_FINALIZE_DELAY_MS, false, playerId, oldBody, previousRank);
		GetGame().GetCallqueue().CallLater(EnsurePlayerFaction, RESPAWN_FINALIZE_DELAY_MS, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Make sure the player counts as a defender, without setting the faction when it is already right.
	//!
	//! PS_PlayableManager.ApplyPlayable derives the player's faction from the body it puts them in, which
	//! is this config's prefab, so normally there is nothing to do here. Setting it eagerly instead - before
	//! the player held the body - sent SCR_GroupsManagerComponent.OnPlayerFactionChanged through a group
	//! they had not joined yet and threw inside SCR_MapMarkerEntrySquadLeader. This only steps in if PS did
	//! not get there, because a player whose faction is unset is invisible to the zone's defender count.
	protected void EnsurePlayerFaction(int playerId)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager)
			return;

		FactionKey wanted = GetDefenderFactionKey();
		if (wanted.IsEmpty())
			return;

		FactionKey current = playableManager.GetPlayerFactionKey(playerId);
		if (current == wanted)
			return;

		PrintFormat("AFM_GameModeDiD: Player %1 was on faction '%2', setting it to '%3'", playerId, current, wanted);
		playableManager.SetPlayerFactionKey(playerId, wanted);
	}

	//------------------------------------------------------------------------------------------------
	//! Restore the rank once more in case the respawn overwrote it, and remove the corpse the player left.
	//! Corpses used to have to survive for the respawn to find the player; now the player id is what is
	//! followed, so they can go as soon as their owner is elsewhere.
	protected void ClearOldBody(int playerId, IEntity oldBody, SCR_ECharacterRank previousRank)
	{
		RestorePlayerRank(playerId, previousRank);

		if (!oldBody)
			return;

		// Never the body they are holding right now
		if (oldBody == GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId))
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(oldBody);
	}

	//------------------------------------------------------------------------------------------------
	//! Teleport a player who lived through the stage to the next zone, keeping body, loadout and rank.
	//! Survivors are spread around the spawn point so they do not land on top of each other.
	//! \return true when the player was moved
	protected bool MoveSurvivorToSpawnPoint(notnull PS_PlayableComponent playableComponent, vector spawnPos)
	{
		if (spawnPos == vector.Zero)
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playableComponent.GetOwner());
		if (!character)
			return false;

		// Riding a vehicle into the next zone would drag the vehicle's occupants apart from it
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.GetCompartmentAccessComponent());
		if (access && character.IsInVehicle())
			access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.INVALID, false);

		vector transform[4];
		character.GetTransform(transform);
		transform[3] = spawnPos;

		character.Teleport(transform);

		// Surviving a stage should not mean starting the next one wounded or unconscious
		SCR_CharacterDamageManagerComponent damageManager = playableComponent.GetCharacterDamageManagerComponent();
		if (damageManager)
			damageManager.FullHeal();

		PrintFormat("AFM_GameModeDiD: Moved and healed survivor at %1", transform[3], level: LogLevel.DEBUG);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rings around the spawn point, widening every SPAWN_RING_SIZE players, so nobody lands on top of
	//! anybody else. Used for respawned players and moved survivors alike, from one shared counter.
	protected vector GetSpawnPosition(AFM_PlayerSpawnPointEntity spawnPoint, int spawnIndex)
	{
		if (!spawnPoint)
			return vector.Zero;

		vector center = spawnPoint.GetOrigin();
		if (spawnIndex <= 0)
			return center;

		int ring = 1 + spawnIndex / SPAWN_RING_SIZE;
		int indexInRing = spawnIndex % SPAWN_RING_SIZE;
		float angle = indexInRing * (Math.PI2 / SPAWN_RING_SIZE);
		float radius = ring * SPAWN_SPACING_M;

		vector pos = center + Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		return pos;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-equip whatever the player last saved at an arsenal.
	//!
	//! Vanilla applies saved loadouts through SCR_LoadoutManager, which calls OnLoadoutSpawned on the
	//! chosen SCR_BasePlayerLoadout. The PS framework spawns a prefab directly and never goes near that
	//! pipeline, so nothing applies the save. Calling the arsenal loadout's own applier is enough: it
	//! reads the stored string from SCR_ArsenalManagerComponent itself.
	protected void ApplySavedLoadout(int playerId)
	{
		if (!m_bApplySavedLoadouts)
			return;
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (!character)
		{
			PrintFormat("AFM_GameModeDiD: No body for player %1, saved loadout not applied", playerId, level: LogLevel.WARNING);
			return;
		}
		
		// COWABUNGA puts players on the attacking side. OnLoadoutSpawned erases a saved loadout whose
		// faction does not match the body, so it must never run for an attacker.
		if (character.GetFactionKey() != GetDefenderFactionKey())
			return;
		
		SCR_ArsenalManagerComponent arsenalManager;
		if (!SCR_ArsenalManagerComponent.GetArsenalManager(arsenalManager))
			return;
		
		SCR_ArsenalPlayerLoadout saved;
		if (!arsenalManager.GetPlayerArsenalLoadout(SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId), saved))
			return;
		
		if (!saved || saved.loadout.IsEmpty())
			return;
		
		SCR_PlayerArsenalLoadout loadout = new SCR_PlayerArsenalLoadout();
		loadout.OnLoadoutSpawned(character, playerId);
		
		PrintFormat("AFM_GameModeDiD: Applied saved arsenal loadout to player %1", playerId);
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
		GameEnd(GetDefenderFactionKey());
	}

	protected void GameEndAttackersWin()
	{
		Print("Attackers win!");
		GameEnd(GetAttackerFactionKey());
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
	
	//! Contested seconds left before the zone falls, or -1 when it cannot be lost this way
	int GetContestedSecondsLeft()
	{
		return m_iContestedSecondsLeft;
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
		return SCR_Faction.Cast(m_FactionManager.GetFactionByKey(GetDefenderFactionKey()));
	}
	
	SCR_Faction GetRedforFaction()
	{
		return SCR_Faction.Cast(m_FactionManager.GetFactionByKey(GetAttackerFactionKey()));
	}
	
	
}
