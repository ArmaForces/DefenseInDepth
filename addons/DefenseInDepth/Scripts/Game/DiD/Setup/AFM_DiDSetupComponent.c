//------------------------------------------------------------------------------------------------
//! Which of the times on the setup screen a request is about
//------------------------------------------------------------------------------------------------
enum AFM_EDiDSetupTime
{
	PREPARE,		// Seconds to prepare before the attack
	DEFENSE,		// Seconds a zone must be held
	FAILURE,		// Contested seconds before a zone is lost
	SPAWN_WARMUP	// Seconds players wait for their first body
}

//------------------------------------------------------------------------------------------------
class AFM_DiDSetupComponentClass: SCR_BaseGameModeComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The scenario setup: what an admin or Game Master can still change while the match waits in the
//! pre-game, and the screen everybody waits on meanwhile.
//!
//! The values live here on the authority and are replicated, so every player watches the choices as
//! they are made. Nothing in the match reads them from here. They are handed to the game mode and to
//! AFM_DiDScenarioSettings once, when Start is pressed, which is also the moment the zones read their
//! sides and timings for good.
//!
//! Requests come in through AFM_DiDSetupPlayerComponent on the player controller, which is what tells
//! the authority whose they are. Every one of them is checked here: the match has to be waiting in the
//! pre-game, the player has to be an admin or Game Master as the authority sees it, and the value has to
//! be one the match can run with.
//!
//! A match that starts by itself (AFM_GameModeDiD.IsAutoStart) never opens the setup. It runs on what
//! the scenario and the game mode say, and this component stays out of it.
//------------------------------------------------------------------------------------------------
class AFM_DiDSetupComponent: SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.ResourceAssignArray, desc: "The sides an admin can pick from on the setup screen, for either role. List what this world and its addons have installed. A side is only offered when the faction manager has its faction, as defenders when it has player characters and as attackers when it has infantry groups. The two sides the scenario or the game mode start with are offered whether they are listed here or not", params: "conf class=AFM_DiDSideConfig", category: "DiD Setup")]
	protected ref array<ResourceName> m_aSideCatalog;

	// Bounds for what an admin can ask for. What the scenario or the game mode start with is not held to them.
	protected static const int MAX_PHASE_TIME_SECONDS = 7200;
	protected static const int MAX_SPAWN_WARMUP_SECONDS = 600;

	// How often a machine looks whether the setup screen should be up
	protected static const int SCREEN_CHECK_INTERVAL_MS = 500;

	protected static const string DEFAULT_SCENARIO_TITLE = "Defense In Depth";

	// The sides that can be picked: the catalog, then whichever defaults it did not list. The two arrays
	// run in step. Files are the same on every machine, so each one loads its own copy for the names.
	protected ref array<ResourceName> m_aSidePaths = {};
	protected ref array<ref AFM_DiDSideConfig> m_aSideConfigs = {};

	// True while the authority waits in the pre-game for an admin to start the match. Clients open the
	// screen on this rather than on the game state alone: it is false until the authority's word has
	// arrived, so a player who joins a running match never sees the screen flash up.
	[RplProp(onRplName: "OnSetupChanged")]
	protected bool m_bSetupOpen;

	// Replicated because a server can rename the scenario in its config, which no client gets to see
	[RplProp(onRplName: "OnSetupChanged")]
	protected string m_sScenarioTitle;

	[RplProp(onRplName: "OnSetupChanged")]
	protected string m_sDefenderConfigPath;

	[RplProp(onRplName: "OnSetupChanged")]
	protected string m_sAttackerConfigPath;

	// -1 keeps what each zone was authored with, as in AFM_DiDPhaseSettings
	[RplProp(onRplName: "OnSetupChanged")]
	protected int m_iPrepareTimeSeconds = -1;

	[RplProp(onRplName: "OnSetupChanged")]
	protected int m_iDefenseTimeSeconds = -1;

	[RplProp(onRplName: "OnSetupChanged")]
	protected int m_iFailureTimeSeconds = -1;

	[RplProp(onRplName: "OnSetupChanged")]
	protected int m_iSpawnWarmupSeconds;

	protected ref ScriptInvokerVoid m_OnSetupChanged;

	//------------------------------------------------------------------------------------------------
	//! The setup of the running game mode, or null when its prefab does not carry one
	static AFM_DiDSetupComponent GetInstance()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;

		return AFM_DiDSetupComponent.Cast(gameMode.FindComponent(AFM_DiDSetupComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the player may change the setup and start the match.
	//!
	//! That is an admin, a Game Master, or the host of a session that does not run on a dedicated server.
	//! An admin is whoever vanilla calls one: the server's listed admins and anyone logged in as one. A
	//! Game Master is whoever holds an unlimited editor, which is what a won vote hands out (see
	//! IsGameMaster). The host is added because nothing in the scripts says the engine gives them either
	//! role, and a hosted match with nobody able to press Start would be stuck. The host is the one player
	//! whose controller is local to the authority; on a dedicated server there is no such controller and
	//! the local id is 0, which no player has.
	//!
	//! Asked on the authority it is the answer that counts. Asked on a client it only decides what that
	//! player's screen offers.
	static bool IsSetupAdmin(int playerId)
	{
		if (playerId <= 0)
			return false;

		if (SCR_Global.IsAdmin(playerId))
			return true;

		if (IsGameMaster(playerId))
			return true;

		if (Replication.IsServer())
			return playerId == SCR_PlayerController.GetLocalPlayerId();

		return IsHostPlayerOnClient(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when a client knows the player to be the host of the session.
	//!
	//! The authority cannot tell a client which player is the host any other way than through the voting
	//! manager, which replicates it. Its getter latches the caller's own id when asked before that
	//! replication arrived, and a client is never the host, so an answer equal to the local id is that
	//! latch and is not trusted. On a dedicated server the host id is 0, which no player has.
	static bool IsHostPlayerOnClient(int playerId)
	{
		SCR_VotingManagerComponent votingManager = SCR_VotingManagerComponent.GetInstance();
		if (!votingManager)
			return false;

		int hostId = votingManager.GetHostPlayerID();
		if (hostId <= 0 || hostId == SCR_PlayerController.GetLocalPlayerId())
			return false;

		return hostId == playerId;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the player is a Game Master with an unlimited editor.
	//!
	//! On the authority the answer is the player role alone. The authority sets it only when the editor
	//! really becomes unlimited, so it has no window in which it is wrong. The editor manager's and the
	//! delegate's limited flags cannot be used there: both read false, meaning unlimited, until the first
	//! update has run, which would let a player who has only just joined pass for a Game Master.
	//!
	//! A client reads the role as well, and the player's delegate on top of it, whose limited flag is
	//! replicated. That one can read wrong for a moment on a new player, but it only decides what the
	//! screen offers and the screen looks again every second.
	static bool IsGameMaster(int playerId)
	{
		if (playerId <= 0)
			return false;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager && playerManager.HasPlayerRole(playerId, EPlayerRole.GAME_MASTER))
			return true;

		if (Replication.IsServer())
			return false;

		SCR_PlayerDelegateEditorComponent delegateManager = SCR_PlayerDelegateEditorComponent.Cast(SCR_PlayerDelegateEditorComponent.GetInstance(SCR_PlayerDelegateEditorComponent));
		if (!delegateManager)
			return false;

		SCR_EditablePlayerDelegateComponent playerDelegate = delegateManager.GetDelegate(playerId);
		return playerDelegate && !playerDelegate.HasLimitedEditor();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when somebody connected can change the setup and start the match, as
	//! IsSetupAdmin sees it on this machine
	static bool IsAnyoneAbleToStart()
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (IsSetupAdmin(playerId))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires on every machine when anything on the setup screen has changed
	ScriptInvokerVoid GetOnSetupChanged()
	{
		if (!m_OnSetupChanged)
			m_OnSetupChanged = new ScriptInvokerVoid();

		return m_OnSetupChanged;
	}

	//------------------------------------------------------------------------------------------------
	void OnSetupChanged()
	{
		if (m_OnSetupChanged)
			m_OnSetupChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		// Every machine: the screen names the sides by what their configs say
		if (m_aSideCatalog)
		{
			foreach (ResourceName configPath : m_aSideCatalog)
			{
				FindOrAddSide(configPath);
			}
		}

		if (Replication.IsServer())
			InitSetup();

		// A dedicated server has nobody to show the screen to
		if (RplSession.Mode() != RplMode.Dedicated)
			GetGame().GetCallqueue().CallLater(UpdateScreen, SCREEN_CHECK_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(UpdateScreen);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Authority only. The screen opens on what the scenario and the game mode would have played anyway,
	//! so an admin who changes nothing gets exactly the match a start by itself would have given.
	//!
	//! Everything read here is good from the moment the game mode exists - its attributes and the mission
	//! header captured at OnMissionSet - so it does not matter whether the game mode itself has
	//! initialised yet.
	protected void InitSetup()
	{
		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(m_pGameMode);
		if (!gameMode)
		{
			PrintFormat("AFM_DiDSetupComponent: Not on an AFM_GameModeDiD, there is nothing to set up", level: LogLevel.ERROR);
			return;
		}

		m_sScenarioTitle = AFM_DiDScenarioSettings.GetScenarioName();
		if (m_sScenarioTitle.IsEmpty())
			m_sScenarioTitle = DEFAULT_SCENARIO_TITLE;

		m_sDefenderConfigPath = gameMode.GetDefaultDefenderConfigPath();
		m_sAttackerConfigPath = gameMode.GetDefaultAttackerConfigPath();

		// The sides the match starts with can always be gone back to, listed in the catalog or not
		FindOrAddSide(m_sDefenderConfigPath);
		FindOrAddSide(m_sAttackerConfigPath);

		AFM_DiDPhaseSettings defaultPhase = AFM_DiDScenarioSettings.GetDefaultPhase();
		if (defaultPhase)
		{
			m_iPrepareTimeSeconds = Math.Max(-1, defaultPhase.m_iPrepareTimeSeconds);
			m_iDefenseTimeSeconds = Math.Max(-1, defaultPhase.m_iDefenseTimeSeconds);
			m_iFailureTimeSeconds = Math.Max(-1, defaultPhase.m_iFailureTimeSeconds);
		}

		m_iSpawnWarmupSeconds = Math.Max(0, gameMode.GetSpawnWarmupSeconds());

		m_bSetupOpen = !gameMode.IsAutoStart();
		if (m_bSetupOpen)
			PrintFormat("AFM_DiDSetupComponent: Waiting in the pre-game for an admin to start the match, %1 sides to pick from", m_aSidePaths.Count());
		else
			PrintFormat("AFM_DiDSetupComponent: The match starts by itself, the setup screen stays closed");

		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine. The match has left the pre-game, whoever started it and however.
	override void OnGameStateChanged(SCR_EGameModeState state)
	{
		if (state == SCR_EGameModeState.PREGAME)
			return;

		if (Replication.IsServer() && m_bSetupOpen)
		{
			m_bSetupOpen = false;
			Replication.BumpMe();
		}

		CloseScreen();
	}

	//------------------------------------------------------------------------------------------------
	// The screen
	//------------------------------------------------------------------------------------------------

	//! Runs on every machine with a player on it for as long as the match is in the pre-game, and opens
	//! the setup screen once there is a local player to show it to.
	//!
	//! Polled rather than done on an event because there is no single moment to hook: the local player
	//! controller, the replicated state of this component and the menu manager each become ready on their
	//! own, and the screen needs all three. It stops for good as soon as the pre-game is over, so a player
	//! who joins a running match never gets the screen.
	protected void UpdateScreen()
	{
		if (m_pGameMode.GetState() != SCR_EGameModeState.PREGAME)
		{
			CloseScreen();
			return;
		}

		if (!m_bSetupOpen)
			return;

		// The screen talks to the authority through the local player's controller
		if (!AFM_DiDSetupPlayerComponent.GetLocal())
			return;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		if (menuManager.FindMenuByPreset(ChimeraMenuPreset.AFM_DiDSetupMenu))
			return;

		menuManager.OpenMenu(ChimeraMenuPreset.AFM_DiDSetupMenu);
	}

	//------------------------------------------------------------------------------------------------
	//! Take the screen down on this machine and stop looking whether it should be up
	protected void CloseScreen()
	{
		GetGame().GetCallqueue().Remove(UpdateScreen);

		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
			menuManager.CloseMenuByPreset(ChimeraMenuPreset.AFM_DiDSetupMenu);
	}

	//------------------------------------------------------------------------------------------------
	// Reading
	//------------------------------------------------------------------------------------------------

	//! Returns true while the authority waits in the pre-game for an admin to start the match
	bool IsSetupOpen()
	{
		return m_bSetupOpen;
	}

	//------------------------------------------------------------------------------------------------
	string GetScenarioTitle()
	{
		return m_sScenarioTitle;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds currently set for one of the times. -1 for a phase time keeps what each zone was authored with.
	int GetTime(AFM_EDiDSetupTime time)
	{
		switch (time)
		{
			case AFM_EDiDSetupTime.PREPARE:
				return m_iPrepareTimeSeconds;

			case AFM_EDiDSetupTime.DEFENSE:
				return m_iDefenseTimeSeconds;

			case AFM_EDiDSetupTime.FAILURE:
				return m_iFailureTimeSeconds;

			case AFM_EDiDSetupTime.SPAWN_WARMUP:
				return m_iSpawnWarmupSeconds;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! How many sides this machine knows of. Not all of them can be picked: see IsSidePickable.
	int GetSideCount()
	{
		return m_aSidePaths.Count();
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetSidePath(int sideIndex)
	{
		if (!m_aSidePaths.IsIndexValid(sideIndex))
			return ResourceName.Empty;

		return m_aSidePaths[sideIndex];
	}

	//------------------------------------------------------------------------------------------------
	//! The name the side goes by on the screen
	string GetSideLabel(int sideIndex)
	{
		if (!m_aSideConfigs.IsIndexValid(sideIndex))
			return string.Empty;

		return m_aSideConfigs[sideIndex].GetLabel();
	}

	//------------------------------------------------------------------------------------------------
	//! Where the side currently set for a role sits in this machine's list, or -1 when its config cannot
	//! be loaded here.
	//!
	//! A side the catalog does not list is added on the way. That is the case on a client when the server
	//! named a side in its own config: the client learns of it only now, from the replicated path.
	int GetCurrentSideIndex(bool attacker)
	{
		if (attacker)
			return FindOrAddSide(m_sAttackerConfigPath);

		return FindOrAddSide(m_sDefenderConfigPath);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the side can be put in the role as things stand.
	//!
	//! It needs a faction this world's faction manager knows, and the content its role is about: a side
	//! nobody can play as has no place among the defenders, and one with no infantry sends nothing. And it
	//! cannot be the faction the other role already has - a side does not fight itself, and every count in
	//! the match tells the two apart by faction. To swap the two sides, change one of them to a third first.
	bool IsSidePickable(bool attacker, int sideIndex)
	{
		if (!m_aSideConfigs.IsIndexValid(sideIndex))
			return false;

		AFM_DiDSideConfig side = m_aSideConfigs[sideIndex];
		if (side.m_sFactionKey.IsEmpty())
			return false;

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager || !factionManager.GetFactionByKey(side.m_sFactionKey))
			return false;

		if (attacker)
		{
			if (!side.m_aInfantryGroups || side.m_aInfantryGroups.IsEmpty())
				return false;
		}
		else
		{
			if (!side.m_aPlayerCharacters || side.m_aPlayerCharacters.IsEmpty())
				return false;
		}

		int otherIndex = GetCurrentSideIndex(!attacker);
		if (m_aSideConfigs.IsIndexValid(otherIndex) && m_aSideConfigs[otherIndex].m_sFactionKey == side.m_sFactionKey)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the side sits in the list, loading its config and adding it when it is not there yet.
	//! Returns -1 when there is no such config to load.
	protected int FindOrAddSide(ResourceName configPath)
	{
		if (configPath.IsEmpty())
			return -1;

		int sideIndex = m_aSidePaths.Find(configPath);
		if (sideIndex >= 0)
			return sideIndex;

		AFM_DiDSideConfig side = SCR_ConfigHelperT<AFM_DiDSideConfig>.GetConfigObject(configPath);
		if (!side)
		{
			PrintFormat("AFM_DiDSetupComponent: Side config '%1' could not be loaded, it is left off the setup screen",
				configPath, level: LogLevel.WARNING);
			return -1;
		}

		m_aSideConfigs.Insert(side);
		return m_aSidePaths.Insert(configPath);
	}

	//------------------------------------------------------------------------------------------------
	// Requests, authority only
	//------------------------------------------------------------------------------------------------

	//! Returns true when a request of this player is to be acted on at all
	protected bool CanChangeSetup(int playerId)
	{
		if (!Replication.IsServer())
			return false;

		// Too late, or a match that was never open to setting up
		if (!m_bSetupOpen || m_pGameMode.GetState() != SCR_EGameModeState.PREGAME)
			return false;

		if (!IsSetupAdmin(playerId))
		{
			PrintFormat("AFM_DiDSetupComponent: Player %1 is not an admin or Game Master, their setup request is ignored",
				playerId, level: LogLevel.WARNING);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Put another side in a role. The side is named by its config, because the lists of two machines do
	//! not have to be in the same order; it has to be one this machine lists and can be picked right now.
	void RequestSetSide(int playerId, bool attacker, ResourceName configPath)
	{
		if (!CanChangeSetup(playerId))
			return;

		int sideIndex = m_aSidePaths.Find(configPath);
		if (!IsSidePickable(attacker, sideIndex))
		{
			PrintFormat("AFM_DiDSetupComponent: Player %1 asked for a side that cannot be picked ('%2'), ignored",
				playerId, configPath, level: LogLevel.WARNING);
			return;
		}

		// The authority's own spelling of the path, not the one that came over the wire
		if (attacker)
			m_sAttackerConfigPath = m_aSidePaths[sideIndex];
		else
			m_sDefenderConfigPath = m_aSidePaths[sideIndex];

		OnSetupChanged();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Set one of the times. Whatever is asked for is brought into the range a match can run with.
	void RequestSetTime(int playerId, AFM_EDiDSetupTime time, int seconds)
	{
		if (!CanChangeSetup(playerId))
			return;

		switch (time)
		{
			case AFM_EDiDSetupTime.PREPARE:
				m_iPrepareTimeSeconds = Math.ClampInt(seconds, -1, MAX_PHASE_TIME_SECONDS);
				break;

			case AFM_EDiDSetupTime.DEFENSE:
				m_iDefenseTimeSeconds = Math.ClampInt(seconds, -1, MAX_PHASE_TIME_SECONDS);
				break;

			case AFM_EDiDSetupTime.FAILURE:
				m_iFailureTimeSeconds = Math.ClampInt(seconds, -1, MAX_PHASE_TIME_SECONDS);
				break;

			case AFM_EDiDSetupTime.SPAWN_WARMUP:
				m_iSpawnWarmupSeconds = Math.ClampInt(seconds, 0, MAX_SPAWN_WARMUP_SECONDS);
				break;

			default:
				return;
		}

		OnSetupChanged();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Hand what was chosen to the game mode and to the scenario settings, then start the match. The
	//! zones read both as the match starts, so nothing else has to be told.
	void RequestStart(int playerId)
	{
		if (!CanChangeSetup(playerId))
			return;

		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(m_pGameMode);
		if (!gameMode)
			return;

		PrintFormat("AFM_DiDSetupComponent: Player %1 starts the match - defender '%2', attacker '%3', prepare %4 s, defend %5 s, contested %6 s, spawn warm-up %7 s",
			playerId, m_sDefenderConfigPath, m_sAttackerConfigPath,
			m_iPrepareTimeSeconds, m_iDefenseTimeSeconds, m_iFailureTimeSeconds, m_iSpawnWarmupSeconds);

		AFM_DiDScenarioSettings.SetMatchTimings(m_iPrepareTimeSeconds, m_iDefenseTimeSeconds, m_iFailureTimeSeconds);
		gameMode.ApplySetup(m_sDefenderConfigPath, m_sAttackerConfigPath, m_iSpawnWarmupSeconds);
		gameMode.StartMatch();
	}
}
