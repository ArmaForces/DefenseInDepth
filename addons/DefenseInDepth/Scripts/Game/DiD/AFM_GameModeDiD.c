class AFM_GameModeDiDClass: SCR_BaseGameModeClass
{
}

class AFM_GameModeDiD: SCR_BaseGameMode
{
	[Attribute("{2FF4CFE9D80F6F76}Configs/Factions/DiD_Side_US.conf", UIWidgets.ResourceNamePicker, "The side the players defend as: its faction, its extraction helicopter and that helicopter's crew", params: "conf class=AFM_DiDSideConfig", category: "DiD")]
	protected ResourceName m_sDefenderConfigPath;

	[Attribute("{CB5CAE38E68AEBE5}Configs/Factions/DiD_Side_USSR.conf", UIWidgets.ResourceNamePicker, "The side that attacks: its faction, infantry groups, vehicles, mortars, helicopters and COWABUNGA squad", params: "conf class=AFM_DiDSideConfig", category: "DiD")]
	protected ResourceName m_sAttackerConfigPath;

	[Attribute("{9D1C4E7A3B052F68}Configs/Awards/DiD_Awards.conf", UIWidgets.ResourceNamePicker, "Titles handed out when the match ends. Read on the authority only", params: "conf class=AFM_DiDAwardConfig", category: "DiD")]
	protected ResourceName m_sAwardConfigPath;

	[Attribute("1", UIWidgets.CheckBox, "Supplies as one currency for building, the arsenal and support. Unticking it leaves building free, which is what the mode did before the economy existed", category: "DiD")]
	protected bool m_bSupplyEconomy;

	[Attribute("{6B8D4F20E13C957A}Configs/Supplies/DiD_Supplies_Medium.conf", UIWidgets.ResourceNamePicker, "What each stage starts with, what carries over and what dismantling refunds. Easy, Medium and Hard files ship in Configs/Supplies", params: "conf class=AFM_DiDSupplyConfig", category: "DiD")]
	protected ResourceName m_sSupplyConfigPath;

	// Loaded at EOnInit, well before any zone initialises and reads them
	protected ref AFM_DiDSideConfig m_DefenderConfig;
	protected ref AFM_DiDSideConfig m_AttackerConfig;
	protected ref AFM_DiDAwardConfig m_AwardConfig;
	protected ref AFM_DiDSupplyConfig m_SupplyConfig;
	protected ref AFM_DiDSupplyBudget m_SupplyBudget;
	protected ref AFM_DiDSupplyIncome m_SupplyIncome;
	protected ref AFM_DiDArsenalSpending m_ArsenalSpending;

	// Replicated, because a client cannot be relied on to have resolved the configs the same way: the
	// scenario may name the sides in its header, which is captured on the authority. The HUD needs the
	// keys to draw the two flags, so the authority states them.
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected string m_sDefenderFactionKeyRpl;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected string m_sAttackerFactionKeyRpl;
	
	[Attribute("1", UIWidgets.CheckBox, "Re-equip the loadout a player saved at an arsenal when they respawn", category: "DiD")]
	protected bool m_bApplySavedLoadouts;

	[Attribute("30", UIWidgets.EditBox, "Seconds players wait without a body at the start of the first stage before they get one, so the first bodies are not handed out in the same moment the match and its first zone start. The first stage's prepare phase is extended by the same amount. 0 = bodies straight away", category: "DiD")]
	protected int m_iSpawnWarmupSeconds;

	[Attribute("0", UIWidgets.CheckBox, "Start the match by itself as soon as the first player has joined, with the sides and timings of the scenario. Left unticked, everyone waits on the setup screen until an admin presses Start. A scenario can also turn it on in its mission header, which is how a server nobody attends is run", category: "DiD")]
	protected bool m_bAutoStart;

	// The spawn warm-up at match start. Not m_bIsWarmup, which is the prepare phase.
	protected bool m_bSpawnWarmupActive;

	// The corpse a player leaves behind is removed this long after they took the new body. Deleting the
	// entity a client controlled a moment ago, in the same frame it is handed another one, is what
	// docs/known-bugs.md (bug 1) points at for the client crash, so the two are kept apart on purpose.
	protected static const int RESPAWN_FINALIZE_DELAY_MS = 2000;

	// Players arriving in a new zone are placed on rings around its spawn point
	protected static const int SPAWN_RING_SIZE = 6;
	protected static const float SPAWN_SPACING_M = 3.0;

	// Let the new zone settle before moving anyone into it
	protected static const int ZONE_TRANSFER_DELAY_MS = 5000;

	// Launch parameter that starts the match by itself, written -afmDidAutoStart on the command line
	protected static const string AUTO_START_CLI_PARAM = "afmDidAutoStart";

	// Zone the last transfer was made for, so survivors are only moved when the stage actually changes
	protected int m_iLastTransferZoneIndex = -1;

	// What this match has recorded about each player. Authority only.
	protected ref AFM_DiDStatsTracker m_Stats;

	// The finished table, on every machine once the match has ended
	protected ref AFM_DiDMatchResults m_MatchResults;
	protected ref ScriptInvokerVoid m_OnMatchResults;

	// Players whose body has been asked for from the respawn system and has not arrived yet, each with
	// the body they held when it was asked for. Being in here is what stops a second request for the same
	// player while the first is still in flight. Authority only.
	protected ref map<int, IEntity> m_mBodyRequests = new map<int, IEntity>();

	// How many bodies have been handed out, which is also the next index into the side config's list
	protected int m_iBodiesHandedOut;

	// A body's faction affiliation lands a moment after the switch, and the saved loadout cannot be read before it
	protected static const int LOADOUT_RETRY_MS = 300;
	protected static const int LOADOUT_MAX_ATTEMPTS = 10;

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

	// The stage's pool, sent the same way as every other figure on the status line. The container itself only
	// reaches a client that has subscribed to it, which the arsenal and the build menu do while they are open
	// and the HUD never does.
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iSupplies;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bIsContested = false;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iContestedSecondsLeft = -1;	// -1 when the zone cannot be lost by being held

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bHasNextSpawnWave = false;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected WorldTimestamp m_NextSpawnWaveTimestamp;

	// Where a player without a body starts watching from when they have no corpse to rise from: above the
	// current stage's player spawn point. Replicated, because the zones exist on the authority alone.
	// Zero until the first stage starts.
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected vector m_vSpectatorAnchor;

	protected static const float SPECTATOR_ANCHOR_HEIGHT_M = 10;

	// A player whose body was deleted is given a place in the world to be streamed around this long after,
	// once the deletion has gone through and vanilla has taken away the observer that followed the body
	protected static const int SPECTATOR_OBSERVER_DELAY_MS = 1000;

	// Players are taken out of their bodies this long after the match has ended. By then the stage has
	// been cleaned up - which removes an extraction helicopter together with whoever sits in it - and the
	// game-over screen is up on every machine, so nobody watches their own body go.
	protected static const int DEBRIEF_BODY_REMOVAL_DELAY_MS = 3000;

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
		{
			// Skipping the prepare phase skips the wait for bodies with it
			EndSpawnWarmup("the prepare phase was skipped");
			m_ZoneSystem.ForceEndPrepareStage();
		}
		else //no zone system - assume we are a proxy
			Rpc(RPC_DoForceEndPrepareStage);
	}
	
	//! Who may skip is decided in AFM_VoteSkipWarmupAction.PerformAction on the authority. A client cannot
	//! reach this RPC anyway: it does not own the game mode, and only an owner can call a server RPC.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_DoForceEndPrepareStage()
	{
		if (!m_ZoneSystem)
			return;
		EndSpawnWarmup("the prepare phase was skipped");
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

	//------------------------------------------------------------------------------------------------
	//! Every death in the match, AI included, arrives here on the authority. The context says who killed
	//! whom and what each of them was, so the tracker needs nothing else wired up.
	override void OnControllableDestroyedEx(notnull SCR_InstigatorContextData instigatorContextData)
	{
		super.OnControllableDestroyedEx(instigatorContextData);

		if (!IsMaster())
			return;

		if (m_Stats)
			m_Stats.OnControllableDestroyed(instigatorContextData);

		if (m_SupplyIncome)
			m_SupplyIncome.OnControllableDestroyed(instigatorContextData);

		// A player who died watches the rest of the stage
		int victimId = instigatorContextData.GetVictimPlayerID();
		if (victimId > 0)
			OnPlayerLostBody(victimId);
	}

	//------------------------------------------------------------------------------------------------
	//! Credit everyone still holding a body when a stage ends. Called as the next zone opens, so the
	//! survivors are exactly the players the stage did not kill.
	protected void CreditZoneSurvivors()
	{
		if (!m_Stats)
			return;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			if (HasLivingBody(playerId))
				m_Stats.OnZoneSurvived(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! What this match has recorded so far, or null off the authority
	AFM_DiDStatsTracker GetStats()
	{
		return m_Stats;
	}

	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		if (SCR_Global.IsEditMode())
			return;
		
		LoadSideConfigs();

		// Every machine: the prices it stamps on the spawn catalogs have to match what the authority charges,
		// and the HUD reads the pool locally
		LoadSupplyConfig();

		// Authority only: every death is reported here, and a client is told the finished table instead
		if (IsMaster())
		{
			m_Stats = new AFM_DiDStatsTracker();
			LoadAwardConfig();
			StartSupplyEconomy();

			// Whatever an admin chose on the setup screen of the match before is not this match's
			AFM_DiDScenarioSettings.ClearMatchTimings();
		}

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
	
	
	//------------------------------------------------------------------------------------------------
	//! Runs on every machine. The match itself is the vanilla GAME state: PREGAME is the wait before it
	//! and POSTGAME is everything after GameEnd.
	override protected void OnGameStateChanged()
	{
		super.OnGameStateChanged();

		if (GetState() != SCR_EGameModeState.GAME)
			return;

		m_bIsGameRunning = true;
		m_bShowUI = true;

		if (m_ZoneSystem)
			m_ZoneSystem.StartZoneSystem();

		OnMatchSituationChanged();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Takes the match out of the pre-game and into the first stage.
	//!
	//! The pre-game has no time limit of its own (SCR_PreGameGameModeStateComponent with a duration of 0),
	//! so nothing starts the match unless this is called. Calling it again, or after the match is over,
	//! does nothing.
	void StartMatch()
	{
		if (!IsMaster())
			return;

		if (GetState() != SCR_EGameModeState.PREGAME)
			return;

		PrintFormat("AFM_GameModeDiD: Match started with %1 players connected", GetGame().GetPlayerManager().GetPlayerCount());
		StartGameMode();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the match starts by itself with the first player instead of waiting on the setup
	//! screen: the game mode says so, the scenario does in its mission header, or the server was launched
	//! with -afmDidAutoStart. The launch parameter is for a server started on a bare world (-server), which
	//! has no mission header to say it in.
	bool IsAutoStart()
	{
		return m_bAutoStart || AFM_DiDScenarioSettings.IsAutoStart() || System.IsCLIParam(AUTO_START_CLI_PARAM);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only, and only while the match waits in the pre-game. Takes over what the admin chose on the
	//! setup screen, right before StartMatch.
	//!
	//! Nothing has read the sides for good at this point: the zones and their spawners hold that back until
	//! the match starts (AFM_DiDZoneComponent.PrepareForMatch), so swapping the two configs here is all it
	//! takes. The players are the exception. They were put on the defending side as they joined, and
	//! whoever is on the side that was the default is moved to the one that was picked.
	void ApplySetup(ResourceName defenderConfigPath, ResourceName attackerConfigPath, int spawnWarmupSeconds)
	{
		if (!IsMaster())
			return;

		if (GetState() != SCR_EGameModeState.PREGAME)
			return;

		m_iSpawnWarmupSeconds = Math.Max(0, spawnWarmupSeconds);

		SetSideConfigs(defenderConfigPath, attackerConfigPath);

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			// Not through the audit yet: OnPlayerAudited puts them on the defending side when they are
			if (!SCR_FactionManager.SGetPlayerFaction(playerId))
				continue;

			SetPlayerFaction(playerId, GetDefenderFactionKey());
		}

		OnMatchSituationChanged();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! The side config the players defend as unless an admin picks another: the scenario's when its
	//! header names one, otherwise the game mode's own. Good from the moment the entity exists.
	ResourceName GetDefaultDefenderConfigPath()
	{
		ResourceName scenarioPath = AFM_DiDScenarioSettings.GetDefenderConfig();
		if (!scenarioPath.IsEmpty())
			return scenarioPath;

		return m_sDefenderConfigPath;
	}

	//------------------------------------------------------------------------------------------------
	//! The side config that attacks unless an admin picks another, resolved like the defending one
	ResourceName GetDefaultAttackerConfigPath()
	{
		ResourceName scenarioPath = AFM_DiDScenarioSettings.GetAttackerConfig();
		if (!scenarioPath.IsEmpty())
			return scenarioPath;

		return m_sAttackerConfigPath;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds players wait without a body at the start of the first stage, 0 for none
	int GetSpawnWarmupSeconds()
	{
		return m_iSpawnWarmupSeconds;
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
	//! Turns the economy on or off for the whole match, and loads the numbers behind it.
	//!
	//! The switch is vanilla's own global supply flag rather than a flag of ours. Everything that spends
	//! supplies already honours it - composition budgets resolve to unlimited without it, arsenal items
	//! stop costing anything, and the building UI hides its supply bar - and it replicates, so clients
	//! agree without being told separately. The game mode prefab ships with SUPPLIES disabled, which is
	//! why building has been free.
	//------------------------------------------------------------------------------------------------
	protected void LoadSupplyConfig()
	{
		m_SupplyConfig = SCR_ConfigHelperT<AFM_DiDSupplyConfig>.GetConfigObject(m_sSupplyConfigPath);

		if (!m_SupplyConfig)
			PrintFormat("AFM_GameModeDiD: Supply config '%1' could not be loaded", m_sSupplyConfigPath, level: LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartSupplyEconomy()
	{
		SetResourceTypeEnabled(m_bSupplyEconomy, EResourceType.SUPPLIES);

		if (!m_bSupplyEconomy)
		{
			Print("AFM_GameModeDiD: Supply economy is off, building and the arsenal are free");
			return;
		}

		if (!m_SupplyConfig)
		{
			Print("AFM_GameModeDiD: No supply config, turning the economy back off rather than leaving stages unfunded", LogLevel.ERROR);

			m_bSupplyEconomy = false;
			SetResourceTypeEnabled(false, EResourceType.SUPPLIES);
			return;
		}

		// Vanilla charges for compositions only in Conflict, so the charging is ours
		m_SupplyBudget = new AFM_DiDSupplyBudget();
		m_SupplyBudget.Start();

		m_SupplyIncome = new AFM_DiDSupplyIncome();

		// The arsenal charges through vanilla's own path, so its spending is followed rather than taken
		m_ArsenalSpending = new AFM_DiDArsenalSpending();
		m_ArsenalSpending.Start();

		PrintFormat("AFM_GameModeDiD: Supply economy on - stage 1 starts with %1, %2%% carries over, %3%% refunded on dismantle",
			m_SupplyConfig.GetStartingSupplies(1), Math.Round(m_SupplyConfig.m_fCarryOverFraction * 100), m_SupplyConfig.m_iCompositionRefundPercentage);
	}

	//------------------------------------------------------------------------------------------------
	//! Null when the economy is off, which is also how the rest of the mode tells
	AFM_DiDSupplyConfig GetSupplyConfig()
	{
		return m_SupplyConfig;
	}

	//------------------------------------------------------------------------------------------------
	//! Unlike the side configs this is wanted on the authority alone: it decides the winners there, and
	//! what crosses the wire is the finished list of titles rather than the rules for them
	protected void LoadAwardConfig()
	{
		m_AwardConfig = SCR_ConfigHelperT<AFM_DiDAwardConfig>.GetConfigObject(m_sAwardConfigPath);

		if (!m_AwardConfig)
			PrintFormat("AFM_GameModeDiD: Award config '%1' could not be loaded, the match will end with stats but no titles",
				m_sAwardConfigPath, level: LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void LoadSideConfigs()
	{
		// A scenario may name the two sides itself, which is how one game mode prefab serves every pairing
		ResourceName defenderPath = GetDefaultDefenderConfigPath();
		if (defenderPath != m_sDefenderConfigPath)
			PrintFormat("AFM_GameModeDiD: Defending side comes from the scenario: %1", defenderPath);

		ResourceName attackerPath = GetDefaultAttackerConfigPath();
		if (attackerPath != m_sAttackerConfigPath)
			PrintFormat("AFM_GameModeDiD: Attacking side comes from the scenario: %1", attackerPath);

		SetSideConfigs(defenderPath, attackerPath);
	}

	//------------------------------------------------------------------------------------------------
	//! Load the two side configs and, on the authority, state their faction keys for everyone. Runs when
	//! the game mode initialises, and once more on the authority when an admin has picked other sides on
	//! the setup screen.
	protected void SetSideConfigs(ResourceName defenderPath, ResourceName attackerPath)
	{
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

		if (!Replication.IsServer())
			return;

		if (m_DefenderConfig)
			m_sDefenderFactionKeyRpl = m_DefenderConfig.m_sFactionKey;

		if (m_AttackerConfig)
			m_sAttackerFactionKeyRpl = m_AttackerConfig.m_sFactionKey;
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
	//! The authority's answer wins. A client may have loaded a different config, or none.
	FactionKey GetDefenderFactionKey()
	{
		if (!m_sDefenderFactionKeyRpl.IsEmpty())
			return m_sDefenderFactionKeyRpl;

		if (!m_DefenderConfig)
			return string.Empty;

		return m_DefenderConfig.m_sFactionKey;
	}

	//------------------------------------------------------------------------------------------------
	FactionKey GetAttackerFactionKey()
	{
		if (!m_sAttackerFactionKeyRpl.IsEmpty())
			return m_sAttackerFactionKeyRpl;

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
		UpdateSpectatorAnchor();

		// OnZoneChanged also fires when the same zone goes from prepare to active and when a wave is
		// cleared. Dead players are respawned every time, but survivors are only moved when the match
		// has actually progressed to a different zone - otherwise everyone is teleported to the spawn
		// point moments after the first zone starts.
		bool zoneProgressed = m_iLastTransferZoneIndex >= 0 && m_iZoneNumber != m_iLastTransferZoneIndex;
		bool isFirstStageStart = m_iLastTransferZoneIndex < 0;
		m_iLastTransferZoneIndex = m_iZoneNumber;

		// Each stage earns on its own account
		if (m_SupplyIncome && zoneProgressed)
			m_SupplyIncome.OnZoneChanged();

		// A snapshot per stage, so a long match does not have to be read back from one dump at the end
		if (zoneProgressed && m_Stats)
		{
			CreditZoneSurvivors();
			m_Stats.Dump(string.Format("end of stage %1", m_iZoneNumber - 1));
		}

		// The very first bodies wait for the warm-up, which hands them out itself when it ends. Anything else
		// that changes the zone meanwhile leaves that to it.
		if (isFirstStageStart && m_iSpawnWarmupSeconds > 0 && IsMaster())
			StartSpawnWarmup();
		else if (!m_bSpawnWarmupActive)
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

		if (m_SupplyIncome)
			m_SupplyIncome.OnZoneUpdate();
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

		// Read from the pool itself, which is only authoritative here
		if (IsMaster())
			m_iSupplies = AFM_DiDSupplies.GetStored();

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
	//! Connected players without a living body: dead, or controlling nothing at all, which is what every
	//! player is until the first stage hands out bodies.
	//!
	//! A dead player keeps controlling their corpse until they are given a new body, so the corpse is
	//! what tells them apart. It is cleared in ClearOldBody once its owner holds the new body.
	void GetBodilessPlayerIds(notnull array<int> outPlayerIds)
	{
		outPlayerIds.Clear();

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			if (!HasLivingBody(playerId))
				outPlayerIds.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the player controls a character that is alive
	bool HasLivingBody(int playerId)
	{
		return GetLivingBody(playerId) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the living character the player controls, or null when they are dead or hold nothing.
	//! On a client also null for a player whose body does not exist on that machine.
	SCR_ChimeraCharacter GetLivingBody(int playerId)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (!character)
			return null;

		SCR_DamageManagerComponent damageManager = character.GetDamageManager();
		if (!damageManager || damageManager.IsDestroyed())
			return null;

		return character;
	}

	//------------------------------------------------------------------------------------------------
	// Spectators
	//
	// A player without a living body watches through a camera of their own (AFM_DiDSpectatorComponent).
	// That is all on their machine; what follows is what the authority does for it. A machine is only sent
	// what is near the places replication knows its player to be, so there are two things to see to: the
	// players worth following have to exist on the spectator's machine wherever they are, and a spectator
	// who controls nothing has to be given a place at all.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Where a new spectator camera starts when its player has no corpse to rise from. Zero until the
	//! first stage has started.
	vector GetSpectatorAnchor()
	{
		return m_vSpectatorAnchor;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Put the anchor above the active stage's player spawn point, and move everyone who
	//! controls nothing there with it.
	protected void UpdateSpectatorAnchor()
	{
		if (!m_ZoneSystem)
			return;

		AFM_DiDZoneComponent zone = m_ZoneSystem.GetActiveZone();
		if (!zone)
			return;

		// A stage without a spawn point is still watched, from above the zone itself
		vector anchor = zone.GetOwner().GetOrigin();
		AFM_PlayerSpawnPointEntity spawnPoint = zone.GetPlayerSpawnPoint();
		if (spawnPoint)
			anchor = spawnPoint.GetOrigin();

		anchor[1] = anchor[1] + SPECTATOR_ANCHOR_HEIGHT_M;

		// The same stage again: its attack started, or a wave was cleared
		if (anchor == m_vSpectatorAnchor)
			return;

		m_vSpectatorAnchor = anchor;
		Replication.BumpMe();

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			PlaceSpectatorObserver(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. The player has no living body from now on: they died, the body was deleted under
	//! them, or they have joined and wait for their first.
	protected void OnPlayerLostBody(int playerId)
	{
		StreamLivingBodiesTo(playerId, true);

		// Not now: a body being deleted is still there, and vanilla has yet to remove its observer
		GetGame().GetCallqueue().CallLater(PlaceSpectatorObserver, SPECTATOR_OBSERVER_DELAY_MS, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. The player holds a living body again, which the remaining spectators can follow.
	protected void OnPlayerGotBody(int playerId, IEntity body)
	{
		// A Game Master with the editor open is sent everything by vanilla, which takes it all back by
		// itself when the editor closes. Taking the players back here would leave holes in what they see.
		SCR_EditorManagerEntity editorManager;
		SCR_EditorManagerCore editorCore = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (editorCore)
			editorManager = editorCore.GetEditorManager(playerId);

		if (!editorManager || !editorManager.IsOpened())
			StreamLivingBodiesTo(playerId, false);

		array<int> bodiless = {};
		GetBodilessPlayerIds(bodiless);

		foreach (int spectatorId : bodiless)
		{
			if (spectatorId != playerId)
				SetBodyStreamedTo(body, GetStreamingIdentity(spectatorId), true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Keep the living body of every other player on one player's machine however far away
	//! it is, or go back to sending it by distance.
	protected void StreamLivingBodiesTo(int spectatorId, bool keepStreamed)
	{
		RplIdentity identity = GetStreamingIdentity(spectatorId);
		if (!identity.IsValid())
			return;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			if (playerId != spectatorId)
				SetBodyStreamedTo(GetLivingBody(playerId), identity, keepStreamed);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. What vanilla's editor does for a Game Master (SCR_DynamicSimulationEditorComponent):
	//! the body is taken out of the rules that stream it in and out for that one connection, so it is
	//! always there. The engine's argument is "streaming enabled", hence the inversion. It only has an
	//! effect on a server with network dynamic simulation on, which is the default.
	//!
	//! A body sitting in a vehicle is replicated as part of the vehicle and goes where the vehicle goes.
	protected void SetBodyStreamedTo(IEntity body, RplIdentity identity, bool keepStreamed)
	{
		if (!body || !identity.IsValid())
			return;

		RplComponent rplComponent = RplComponent.Cast(body.FindComponent(RplComponent));
		if (rplComponent)
			rplComponent.EnableStreamingConNode(identity, !keepStreamed);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the connection a player's streaming is set for, or an invalid one when there is nothing to
	//! set: the player is gone, or is the host of a listen server and has the whole world already.
	protected RplIdentity GetStreamingIdentity(int playerId)
	{
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
			return RplIdentity.Invalid();

		RplIdentity identity = playerController.GetRplIdentity();
		if (identity == RplIdentity.Local())
			return RplIdentity.Invalid();

		return identity;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Give a player who controls nothing a place in the world: the spectator anchor.
	//!
	//! Vanilla keeps one observer per player and has it follow the controlled body
	//! (SCR_SpawnRequestComponent.UpdateObserverMP_S), so a player who died is still sent what goes on
	//! around the corpse. A player who never had a body has no observer, and one whose body was deleted
	//! loses theirs: their camera would look at a stage without attackers or vehicles in it. A fixed
	//! observer is what vanilla itself uses to preload a spawn position for a player who is not there yet.
	//!
	//! Nothing has to take it away again. It is the same observer vanilla moves onto the next body the
	//! player is given, and removes when they leave.
	protected void PlaceSpectatorObserver(int playerId)
	{
		if (m_vSpectatorAnchor == vector.Zero)
			return;

		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController || playerController.GetControlledEntity())
			return;

		RplIdentity identity = GetStreamingIdentity(playerId);
		if (!identity.IsValid())
			return;

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;

		ObserversSystem observersSystem = ObserversSystem.Cast(world.FindSystem(ObserversSystem));
		if (observersSystem)
			observersSystem.InsertObserverMP(identity, m_vSpectatorAnchor[0], m_vSpectatorAnchor[2], null);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla calls this just before a body somebody controls is deleted, whoever deletes it: the end of
	//! a COWABUNGA squad, a Game Master, the garbage collection of a corpse
	override protected void OnPlayerDeleted(int playerId, IEntity player)
	{
		super.OnPlayerDeleted(playerId, player);

		if (IsMaster())
			OnPlayerLostBody(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Hand a world prop to a faction on every machine.
	//!
	//! Faction affiliation does not replicate: SetAffiliatedFactionByKey is a plain engine call, so doing
	//! it on the server alone leaves every client thinking the prop belongs to whoever placed it. The
	//! arsenal hid that, because RefreshArsenal broadcasts its item list separately, but a building
	//! provider does not - SCR_CampaignBuildingStartUserAction decides whether to show the build action on
	//! the client, comparing the player against the provider's faction as that machine sees it. Vanilla's
	//! own base capture replicates its faction and applies it per machine; this is the same idea.
	void SetPropFaction(RplId propId, FactionKey factionKey)
	{
		if (!propId.IsValid() || factionKey.IsEmpty())
			return;

		RPC_DoSetPropFaction(propId, factionKey);
		Rpc(RPC_DoSetPropFaction, propId, factionKey);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoSetPropFaction(RplId propId, FactionKey factionKey)
	{
		RplComponent rplComponent = RplComponent.Cast(Replication.FindItem(propId));
		if (!rplComponent)
			return;

		IEntity prop = rplComponent.GetEntity();
		if (!prop)
			return;

		SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(prop.FindComponent(SCR_FactionAffiliationComponent));
		if (!affiliation)
			return;

		// Only when it actually changes hands. Setting the same faction again would throw anyone currently
		// building at this provider out of build mode, and this is re-sent whenever a player joins.
		if (affiliation.GetAffiliatedFactionKey() == factionKey)
			return;

		affiliation.SetAffiliatedFactionByKey(factionKey);
	}

	//------------------------------------------------------------------------------------------------
	//! A player who joins mid-match missed the broadcast, so the zone says it again for them
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);

		if (!m_ZoneSystem)
			return;

		AFM_DiDZoneComponent zone = m_ZoneSystem.GetActiveZone();
		if (zone)
			zone.ApplyDefenderFactionToProps();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only, called by AFM_DiDSpawnLogic once a joining player has passed the audit.
	//!
	//! Nobody is given a body here: those come with the stages, and a player who joins in the middle of
	//! one waits for the next hand-out like everyone who died in it. The player is put on the defending
	//! side straight away though, because the zones count their players by faction - someone who has
	//! joined and is waiting for a body is a defender the ticket pool should be sized for.
	void OnPlayerAudited(int playerId)
	{
		if (!SCR_FactionManager.SGetPlayerFaction(playerId))
			SetPlayerFaction(playerId, GetDefenderFactionKey());

		// Until that hand-out they watch
		OnPlayerLostBody(playerId);

		if (IsAutoStart())
			StartMatch();
	}

	//------------------------------------------------------------------------------------------------
	//! A body that was on its way to a player who has left is not waited for any longer
	override protected void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);

		m_mBodyRequests.Remove(playerId);

		// Whatever was kept on their machine for watching is let go of with them
		if (IsMaster())
			StreamLivingBodiesTo(playerId, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. The respawn system asks this before it acts on any spawn request.
	//!
	//! Bodies are handed out by the game mode alone, but the request that carries one is vanilla's own and
	//! a client can send it by itself: a free spawn names a prefab and a position and nothing else. So a
	//! request is only let through for a player the game mode is waiting on a body for. RequestPlayerBody
	//! puts the player on that list before it sends the request, which is how its own free spawns and the
	//! COWABUNGA possessions pass.
	override bool CanPlayerSpawn_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnHandlerComponent handlerComponent, SCR_SpawnData data, out SCR_ESpawnResult result = SCR_ESpawnResult.SPAWN_NOT_ALLOWED)
	{
		int playerId = requestComponent.GetPlayerId();
		if (!m_mBodyRequests.Contains(playerId))
		{
			PrintFormat("AFM_GameModeDiD: Refused a spawn request for player %1 that the game mode did not ask for",
				playerId, level: LogLevel.WARNING);

			result = SCR_ESpawnResult.SPAWN_NOT_ALLOWED;
			return false;
		}

		return super.CanPlayerSpawn_S(requestComponent, handlerComponent, data, result);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn warm-up: at match start, everyone waits without a body before the first ones are handed out.
	//!
	//! Taking a body is when a client has the most to replicate at once - the stage area, its arsenals and
	//! building menus, the character's equipment and radios - and the client crash on the first spawn
	//! (docs/known-bugs.md, bug 1) has only ever come then, never on a later respawn. The wait keeps the
	//! first bodies away from the moment the match starts, when the zone and everything in it is being
	//! created as well.
	//!
	//! Only the first stage start. The first stage's prepare phase is extended by the same time, so the
	//! wait does not cost preparation. A player who joins during the warm-up gets a body when it ends,
	//! along with the rest.
	protected void StartSpawnWarmup()
	{
		m_bSpawnWarmupActive = true;

		if (m_ZoneSystem)
			m_ZoneSystem.ExtendPrepareStage(m_iSpawnWarmupSeconds);

		ShowHint(string.Format("Loading the first stage. You get your body in %1 seconds.", m_iSpawnWarmupSeconds), 10);
		PrintFormat("AFM_GameModeDiD: Spawn warm-up - %1 players wait %2 s before getting bodies",
			GetGame().GetPlayerManager().GetPlayerCount(), m_iSpawnWarmupSeconds);

		GetGame().GetCallqueue().CallLater(EndSpawnWarmup, m_iSpawnWarmupSeconds * 1000, false, "time is up");
	}

	//------------------------------------------------------------------------------------------------
	//! Hand out the first stage's bodies. Does nothing unless the warm-up is running.
	protected void EndSpawnWarmup(string reason)
	{
		if (!m_bSpawnWarmupActive)
			return;

		m_bSpawnWarmupActive = false;
		GetGame().GetCallqueue().Remove(EndSpawnWarmup);

		PrintFormat("AFM_GameModeDiD: Spawn warm-up over (%1), handing out bodies", reason);
		PopulateZone(false);
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
	//! re-sided. The bodies themselves are made and handed over by the vanilla respawn system: this only
	//! decides who gets one, which one and where, and asks for it.
	protected void PopulateZone(bool moveSurvivors = false)
	{
		if (!m_ZoneSystem)
			return;

		// A hand-out is scheduled a few seconds ahead, and the match can be over by the time it is due
		if (GetState() != SCR_EGameModeState.GAME)
			return;

		AFM_PlayerSpawnPointEntity currentSpawnPoint = m_ZoneSystem.GetCurrentZonePlayerSpawnPoint();
		int spawnIndex = 0;

		if (moveSurvivors)
			spawnIndex = MoveSurvivors(currentSpawnPoint, spawnIndex);

		array<int> bodiless = {};
		GetBodilessPlayerIds(bodiless);

		foreach (int playerId : bodiless)
		{
			if (SpawnPlayerBody(playerId, GetSpawnPosition(currentSpawnPoint, spawnIndex)))
				spawnIndex++;
		}

		if (!bodiless.IsEmpty())
			PrintFormat("AFM_GameModeDiD: Spawning bodies for %1 players at the stage's spawn point", bodiless.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Move everyone who lived through the stage to the new spawn point.
	//! Returns the next free position in the spawn ring.
	protected int MoveSurvivors(AFM_PlayerSpawnPointEntity spawnPoint, int spawnIndex)
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			// Only bodies somebody is alive in: a corpse stays where it fell until its owner has a new body
			SCR_ChimeraCharacter character = GetLivingBody(playerId);
			if (!character)
				continue;

			if (MoveSurvivorToSpawnPoint(character, GetSpawnPosition(spawnPoint, spawnIndex)))
				spawnIndex++;
		}

		return spawnIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! Ask for one body from the defending side's config for the player.
	//! Returns false when nothing was asked for.
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

		// No spawn point to stand on: the stage has none, or it has not resolved its children yet
		if (spawnPos == vector.Zero)
		{
			PrintFormat("AFM_GameModeDiD: The stage has no player spawn point, player %1 waits for the next hand-out",
				playerId, level: LogLevel.WARNING);
			return false;
		}

		SCR_FreeSpawnData spawnData = new SCR_FreeSpawnData(prefab, spawnPos);
		if (!RequestPlayerBody(playerId, spawnData, GetDefenderFactionKey()))
			return false;

		m_iBodiesHandedOut++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Ask the vanilla respawn system to put a player into a body.
	//!
	//! The spawn data says which body: SCR_FreeSpawnData makes a new one from a prefab at a position,
	//! SCR_PossessSpawnData hands over a character that already stands in the world. Either way the
	//! request goes through the player's own SCR_RespawnComponent, the way their machine would send it, so
	//! everything vanilla hangs on a spawn happens for these bodies too - the area is preloaded on the
	//! client before control is passed, the editor and the group manager are told, the radio is tuned.
	//!
	//! The player is put on the faction of the body first, and into that faction's player group. The
	//! order matters: a group only takes players of its own faction, and vanilla tunes the radio to the
	//! group's frequency at the moment the body arrives.
	//!
	//! Returns false when nothing was asked for: no such player, a request for them already in flight, or
	//! the respawn system turning it down on the spot. A body that is on its way ends in OnPlayerBodySpawned
	//! or OnPlayerBodyRequestFailed.
	bool RequestPlayerBody(int playerId, notnull SCR_SpawnData spawnData, FactionKey factionKey)
	{
		if (!IsMaster())
			return false;

		// Bodies belong to the match: there are none before it starts and none once it is over, whoever asks
		if (GetState() != SCR_EGameModeState.GAME)
			return false;

		// Asked for a moment ago and still on its way
		if (m_mBodyRequests.Contains(playerId))
			return false;

		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
			return false;

		SCR_RespawnComponent respawnComponent = SCR_RespawnComponent.Cast(playerController.GetRespawnComponent());
		if (!respawnComponent)
		{
			PrintFormat("AFM_GameModeDiD: Player %1 has no SCR_RespawnComponent on their controller, they cannot be given a body",
				playerId, level: LogLevel.ERROR);
			return false;
		}

		if (!SetPlayerFaction(playerId, factionKey))
			return false;

		JoinPlayerGroup(playerId);

		// What this player holds now is what they leave behind: a corpse, or nothing at all
		m_mBodyRequests.Set(playerId, playerController.GetControlledEntity());

		if (!respawnComponent.RequestSpawn(spawnData))
		{
			m_mBodyRequests.Remove(playerId);
			PrintFormat("AFM_GameModeDiD: The body request for player %1 could not be sent, they wait for the next hand-out",
				playerId, level: LogLevel.WARNING);
			return false;
		}

		// A request the respawn system refuses is answered before RequestSpawn returns, and
		// OnPlayerBodyRequestFailed has already taken it off the list by now
		return m_mBodyRequests.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Put a player on a faction through vanilla's own player faction component, which is
	//! what the faction manager, the group manager and the zones' player counts all read.
	//!
	//! A player's faction is always the faction of the body they were last given: the defenders' for a
	//! normal body, the attackers' while they play in the COWABUNGA squad. Dying does not change it, and a
	//! player who has joined and never had a body is a defender.
	//!
	//! Does nothing when the player is on that faction already, so nobody is told about a change of sides
	//! that did not happen - the group manager answers one by taking the player out of their group.
	//! Returns false when the player could not be put on the faction.
	protected bool SetPlayerFaction(int playerId, FactionKey factionKey)
	{
		if (!m_FactionManager)
			return false;

		Faction faction = m_FactionManager.GetFactionByKey(factionKey);
		if (!faction)
		{
			PrintFormat("AFM_GameModeDiD: The faction manager has no faction '%1', player %2 cannot be put on it",
				factionKey, playerId, level: LogLevel.ERROR);
			return false;
		}

		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
			return false;

		SCR_PlayerFactionAffiliationComponent factionComponent = SCR_PlayerFactionAffiliationComponent.Cast(playerController.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!factionComponent)
		{
			PrintFormat("AFM_GameModeDiD: Player %1 has no SCR_PlayerFactionAffiliationComponent on their controller, they cannot be put on a faction",
				playerId, level: LogLevel.ERROR);
			return false;
		}

		if (factionComponent.GetAffiliatedFaction() == faction)
			return true;

		// The authority's own setter rather than RequestFaction, which is the entry a client's request comes
		// in by: that one turns down a faction that is closed to players, and the attackers may well be
		if (!factionComponent.SetFaction_S(faction))
		{
			PrintFormat("AFM_GameModeDiD: Player %1 could not be put on faction '%2'", playerId, factionKey, level: LogLevel.WARNING);
			return false;
		}

		PrintFormat("AFM_GameModeDiD: Player %1 is on faction '%2'", playerId, factionKey);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Put the player in the one group the players of their faction share.
	//!
	//! One group keeps the whole team on one set of map markers and one radio frequency, which is what a
	//! defence of this shape wants. It is an ordinary playable group of SCR_GroupsManagerComponent - the
	//! first one the faction has, made if there is none - with its size limit taken off so that the whole
	//! server fits in.
	//!
	//! A player the group could not take still gets a body, so nothing in here stops the spawn.
	protected void JoinPlayerGroup(int playerId)
	{
		Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
		if (!faction)
			return;

		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		SCR_PlayerControllerGroupComponent groupComponent = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(playerId);
		if (!groupsManager || !groupComponent)
		{
			PrintFormat("AFM_GameModeDiD: No group manager or no group component for player %1, they stay without a group",
				playerId, level: LogLevel.WARNING);
			return;
		}

		SCR_AIGroup group = groupsManager.GetFirstNotFullForFaction(faction);
		if (!group)
			group = groupsManager.CreateNewPlayableGroup(faction);

		if (!group)
		{
			PrintFormat("AFM_GameModeDiD: No player group could be made for faction '%1', player %2 stays without a group",
				faction.GetFactionKey(), playerId, level: LogLevel.WARNING);
			return;
		}

		// 0 means no limit: the whole server goes in here
		group.SetMaxMembers(0);

		if (groupComponent.GetGroupID() == group.GetGroupID())
			return;

		// Runs here and now: a request addressed to the server, sent on the server
		groupComponent.RequestJoinGroup(group.GetGroupID());
	}

	//------------------------------------------------------------------------------------------------
	//! Server only, called by AFM_DiDSpawnLogic when the respawn system has put a player into a body.
	//!
	//! This is the end of vanilla's own spawn sequence: the body exists, the player controls it and
	//! vanilla's listeners have had their turn. What is left is what vanilla does for a body that came
	//! from a loadout and not for one that came from a side config.
	void OnPlayerBodySpawned(int playerId, IEntity body)
	{
		// However the body came about, its player stops watching and can be watched
		OnPlayerGotBody(playerId, body);

		// Asked for just before the match ended and handed over after it. It goes the way every other body
		// went, a moment later rather than in the frame of the hand-over.
		if (GetState() == SCR_EGameModeState.POSTGAME)
		{
			m_mBodyRequests.Remove(playerId);
			GetGame().GetCallqueue().CallLater(RemovePlayerBodies, RESPAWN_FINALIZE_DELAY_MS, false);
			return;
		}

		// Not asked for here, e.g. a Game Master taking over a character
		IEntity oldBody;
		if (!m_mBodyRequests.Find(playerId, oldBody))
			return;

		m_mBodyRequests.Remove(playerId);

		// The attacker squad has no rank or loadout to carry over, and it keeps the corpses of the
		// defenders it was made from: it arms itself from the fallen
		Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
		if (!faction || faction.GetFactionKey() != GetDefenderFactionKey())
			return;

		// Their rank goes with them: a fresh body starts at whatever its prefab says
		SCR_ECharacterRank previousRank = SCR_CharacterRankComponent.GetCharacterRank(oldBody);

		RestorePlayerRank(playerId, body, previousRank);
		ApplySavedLoadout(playerId, 0);
		GetGame().GetCallqueue().CallLater(ClearOldBody, RESPAWN_FINALIZE_DELAY_MS, false, playerId, oldBody, previousRank);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only, called by AFM_DiDSpawnLogic when the respawn system has turned a request down.
	//! The player stays without a body and is asked for again at the next hand-out.
	void OnPlayerBodyRequestFailed(int playerId, SCR_ESpawnResult reason)
	{
		if (!m_mBodyRequests.Contains(playerId))
			return;

		m_mBodyRequests.Remove(playerId);

		PrintFormat("AFM_GameModeDiD: The respawn system refused a body for player %1 (%2), they wait for the next hand-out",
			playerId, typename.EnumToString(SCR_ESpawnResult, reason), level: LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Restore the rank once more in case something overwrote it since, and remove the corpse the player
	//! left. Not done in the same breath as the hand-over: see RESPAWN_FINALIZE_DELAY_MS.
	protected void ClearOldBody(int playerId, IEntity oldBody, SCR_ECharacterRank previousRank)
	{
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		RestorePlayerRank(playerId, body, previousRank);

		if (!oldBody)
			return;

		// Never the body they are holding right now
		if (oldBody == body)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(oldBody);
	}

	//------------------------------------------------------------------------------------------------
	//! Teleport a player who lived through the stage to the next zone, keeping body, loadout and rank.
	//! Survivors are spread around the spawn point so they do not land on top of each other.
	//! Returns true when the player was moved.
	protected bool MoveSurvivorToSpawnPoint(notnull SCR_ChimeraCharacter character, vector spawnPos)
	{
		if (spawnPos == vector.Zero)
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
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(character.GetDamageManager());
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
	//! Vanilla applies a saved loadout when a body spawns by calling OnLoadoutSpawned on the loadout the
	//! player picked in SCR_PlayerLoadoutComponent. Nobody picks a loadout here - the body is whatever the
	//! side config says is next - so nothing applies the save. Calling the arsenal loadout's own applier
	//! is enough: it reads the stored string from SCR_ArsenalManagerComponent itself.
	//!
	//! The applier compares the save's faction with the body's FactionAffiliationComponent and throws if
	//! that component has no faction yet, before a single item is applied. A body that has just been
	//! handed over can still be without one, so this tries again until it has.
	protected void ApplySavedLoadout(int playerId, int attempt)
	{
		if (!m_bApplySavedLoadouts)
			return;

		// The living body only: until the hand-over has gone through, what the player controls is the corpse
		SCR_ChimeraCharacter character = GetLivingBody(playerId);

		Faction faction;
		if (character)
		{
			FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
			if (factionComponent)
				faction = factionComponent.GetAffiliatedFaction();
		}

		if (!faction)
		{
			if (attempt < LOADOUT_MAX_ATTEMPTS)
			{
				GetGame().GetCallqueue().CallLater(ApplySavedLoadout, LOADOUT_RETRY_MS, false, playerId, attempt + 1);
				return;
			}

			if (!character)
				PrintFormat("AFM_GameModeDiD: No body for player %1, saved loadout not applied", playerId, level: LogLevel.WARNING);
			else
				PrintFormat("AFM_GameModeDiD: Body of player %1 never got a faction, saved loadout not applied", playerId, level: LogLevel.WARNING);

			return;
		}

		// COWABUNGA puts players on the attacking side. OnLoadoutSpawned erases a saved loadout whose
		// faction does not match the body, so it must never run for an attacker.
		if (faction.GetFactionKey() != GetDefenderFactionKey())
			return;

		SCR_ArsenalManagerComponent arsenalManager;
		if (!SCR_ArsenalManagerComponent.GetArsenalManager(arsenalManager))
			return;

		SCR_ArsenalPlayerLoadout saved;
		if (!arsenalManager.GetPlayerArsenalLoadout(SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId), saved))
			return;

		if (!saved || saved.loadout.IsEmpty())
			return;

		// Logged before the call: the applier reports nothing back, so this cannot claim it worked
		PrintFormat("AFM_GameModeDiD: Restoring saved arsenal loadout for player %1 (body ready after %2 retries)", playerId, attempt);

		SCR_PlayerArsenalLoadout loadout = new SCR_PlayerArsenalLoadout();
		loadout.OnLoadoutSpawned(character, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! A respawned character starts at the rank of its prefab. Carry the rank of the previous body over,
	//! then let the XP handler have its say, as vanilla does for every body that spawns.
	protected void RestorePlayerRank(int playerId, IEntity character, SCR_ECharacterRank previousRank)
	{
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
		{
			PrintFormat("AFM_GameModeDiD: No player controller for player %1, rank not restored", playerId, level: LogLevel.WARNING);
			return;
		}

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
	//! Server side. Ends the match with winningFactionKey's victory.
	//!
	//! Vanilla's own ending: the game mode goes to POSTGAME and every machine shows the game-over screen,
	//! which is the debrief. The reason is one of the two this mode adds to EGameOverTypes, and the screen
	//! shows what Configs/GameOverScreen/BaseGameOverScreensConfig.conf says for it: who won, and the match
	//! report. A player who joins after the end gets the same screen, because the state and the reason
	//! are replicated.
	//!
	//! The results go out first, so the table is on its way to a client before its state changes. The
	//! players' bodies go last, a few seconds on: see RemovePlayerBodies.
	//------------------------------------------------------------------------------------------------
	protected void GameEnd(FactionKey winningFactionKey)
	{
		// A held zone and a completed extraction can both report a win for the same match
		if (!m_bIsGameRunning)
			return;

		m_bIsGameRunning = false;

		// The log keeps the full table; the page gets it over the wire
		if (m_Stats)
		{
			m_Stats.Dump("match over");
			BroadcastMatchResults(winningFactionKey);
		}

		if (GetState() != SCR_EGameModeState.GAME)
		{
			PrintFormat("AFM_GameModeDiD: Match ended while in state %1, leaving it to the admins", GetState(), level: LogLevel.WARNING);
			return;
		}

		// -1 names no winning faction
		int winningFactionIndex = -1;
		if (m_FactionManager)
		{
			Faction winningFaction = m_FactionManager.GetFactionByKey(winningFactionKey);
			if (winningFaction)
				winningFactionIndex = m_FactionManager.GetFactionIndex(winningFaction);
		}

		EGameOverTypes reason = EGameOverTypes.AFM_DID_ATTACKERS_WIN;
		if (winningFactionKey == GetDefenderFactionKey())
			reason = EGameOverTypes.AFM_DID_DEFENDERS_WIN;

		EndGameMode(SCR_GameModeEndData.CreateSimple(reason, -1, winningFactionIndex));

		GetGame().GetCallqueue().CallLater(RemovePlayerBodies, DEBRIEF_BODY_REMOVAL_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Takes every player out of their body once the match is over, so that everyone is in
	//! the same situation for the debrief: no body, the spectator camera behind the game-over screen, and
	//! the one voice channel that players without a living body share.
	//!
	//! A body is deleted the way the COWABUNGA squad's are, which is also how vanilla removes the body of
	//! a player who leaves - wherever it is: on foot, unconscious or sitting in a vehicle. Vanilla reports
	//! each one through OnPlayerDeleted, which sees to what its player is sent from then on. A dead player
	//! keeps the corpse they control, as they do during the match.
	//!
	//! Nobody is given a body after the match (RequestPlayerBody), so a player who joins later is in the
	//! same situation without anything being done for them. Does nothing while the match runs, and can be
	//! called again: it only ever finds the bodies that are still there.
	protected void RemovePlayerBodies()
	{
		if (!IsMaster() || GetState() != SCR_EGameModeState.POSTGAME)
			return;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		int removed = 0;
		foreach (int playerId : playerIds)
		{
			SCR_ChimeraCharacter body = GetLivingBody(playerId);
			if (!body)
				continue;

			SCR_EntityHelper.DeleteEntityAndChildren(body);
			removed++;
		}

		if (removed > 0)
			PrintFormat("AFM_GameModeDiD: The match is over, %1 players were taken out of their bodies for the debrief", removed);
	}

	//------------------------------------------------------------------------------------------------
	//! What the results page says above the table. Built here because the sides only have names on the
	//! authority - a client has the faction keys, not the configs behind them.
	protected string BuildResultsHeadline(FactionKey winningFactionKey)
	{
		string defenders = "Defenders";
		if (m_DefenderConfig)
			defenders = m_DefenderConfig.GetLabel();

		string attackers = "Attackers";
		if (m_AttackerConfig)
			attackers = m_AttackerConfig.GetLabel();

		if (winningFactionKey == GetDefenderFactionKey())
			return defenders + " held the line";

		return attackers + " broke through";
	}

	//------------------------------------------------------------------------------------------------
	//! Send the finished table out once, at the end of the match.
	//!
	//! Row by row rather than as one payload, because an RPC takes plain values: each row is its name,
	//! its numbers joined in enum order, and the titles it took. Nothing here is replicated state - the
	//! live table stays on the authority for the whole match and only its conclusion travels.
	//!
	//! Each call runs locally too, so a listen server's own client assembles its copy the same way a
	//! remote one does, rather than reading the server's records directly.
	//------------------------------------------------------------------------------------------------
	protected void BroadcastMatchResults(FactionKey winningFactionKey)
	{
		AFM_DiDMatchResults results = m_Stats.BuildResults(m_AwardConfig);
		if (!results || results.IsEmpty())
			return;

		string headline = BuildResultsHeadline(winningFactionKey);

		RPC_DoResultsBegin(headline);
		Rpc(RPC_DoResultsBegin, headline);

		foreach (AFM_DiDPlayerStats row : results.GetRows())
		{
			string name = row.GetName();
			string values = row.EncodeValues();
			string titles = string.Join(AFM_DiDMatchResults.TITLE_SEPARATOR, row.GetTitles(), true);
			string rank = row.GetRankInsignia();

			RPC_DoResultsRow(name, values, titles, rank);
			Rpc(RPC_DoResultsRow, name, values, titles, rank);
		}

		foreach (AFM_DiDAwardResult award : results.GetAwards())
		{
			string title = award.GetTitle();
			string winners = award.GetWinners();
			string value = award.GetValue();

			PrintFormat("AFM_GameModeDiD: Award '%1' goes to %2 with %3", title, winners, value);

			RPC_DoResultsAward(title, winners, value);
			Rpc(RPC_DoResultsAward, title, winners, value);
		}

		RPC_DoResultsEnd();
		Rpc(RPC_DoResultsEnd);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoResultsBegin(string headline)
	{
		m_MatchResults = new AFM_DiDMatchResults();
		m_MatchResults.SetHeadline(headline);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoResultsRow(string name, string values, string titles, string rankInsignia)
	{
		if (!m_MatchResults)
			return;

		AFM_DiDPlayerStats row = new AFM_DiDPlayerStats(string.Empty, name);
		row.DecodeValues(values);
		row.SetRankInsignia(rankInsignia);

		array<string> titleList = {};
		titles.Split(AFM_DiDMatchResults.TITLE_SEPARATOR, titleList, true);

		foreach (string title : titleList)
		{
			row.AddTitle(title);
		}

		m_MatchResults.AddRow(row);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoResultsAward(string title, string winners, string value)
	{
		if (!m_MatchResults)
			return;

		m_MatchResults.AddAward(title, winners, value);
	}

	//------------------------------------------------------------------------------------------------
	//! The table is whole from here on, which is what a page already on screen waits for
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoResultsEnd()
	{
		if (m_OnMatchResults)
			m_OnMatchResults.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Null until the match has ended
	AFM_DiDMatchResults GetMatchResults()
	{
		return m_MatchResults;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires on every machine once the whole table has arrived. The debriefing menu may well open
	//! before the last row does, so a page reads GetMatchResults on open and listens to this as well.
	ScriptInvokerVoid GetOnMatchResults()
	{
		if (!m_OnMatchResults)
			m_OnMatchResults = new ScriptInvokerVoid();

		return m_OnMatchResults;
	}

	//------------------------------------------------------------------------------------------------
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

	//------------------------------------------------------------------------------------------------
	//! What the stage has left to spend, as the authority last saw it
	int GetSupplies()
	{
		return m_iSupplies;
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
