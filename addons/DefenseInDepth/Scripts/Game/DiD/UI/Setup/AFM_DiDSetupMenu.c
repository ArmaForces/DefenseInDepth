//------------------------------------------------------------------------------------------------
modded enum ChimeraMenuPreset
{
	AFM_DiDSetupMenu
}

//------------------------------------------------------------------------------------------------
//! The screen everybody waits on while the match is in the pre-game.
//!
//! One screen for everyone: it shows the scenario, what the match is currently set to and the chat.
//! An admin has the same controls enabled and a Start button on top of that. Who is an admin can change
//! while the screen is up - somebody logs in - so it is asked again about once a second instead of
//! decided when the screen opens.
//!
//! The screen holds no state of its own. What it shows is AFM_DiDSetupComponent's replicated values,
//! and a change an admin makes goes to the authority through AFM_DiDSetupPlayerComponent and comes back
//! the same way it reaches everyone else. AFM_DiDSetupComponent opens and closes the screen.
//------------------------------------------------------------------------------------------------
class AFM_DiDSetupMenu: ChimeraMenuBase
{
	protected static const float ADMIN_CHECK_INTERVAL_S = 1.0;

	// What the spin boxes step through. A value the match is already set to and that is not among these
	// is added to its list, so a scenario's own number is never rounded away.
	protected static const ref array<int> PHASE_TIME_STEPS_S = {-1, 0, 30, 60, 90, 120, 180, 240, 300, 420, 600, 900, 1200, 1800, 2700, 3600};
	protected static const ref array<int> SPAWN_WARMUP_STEPS_S = {0, 10, 15, 20, 30, 45, 60, 90, 120};

	protected static const string TEXT_ZONES_OWN = "As set in each zone";
	protected static const string TEXT_NONE = "Off";
	protected static const string TEXT_WAITING = "Waiting for an admin or Game Master to start the match";
	protected static const string TEXT_ADMIN = "You can set the match up: change what you like and press Start";
	protected static const string TEXT_PLAYERS = "%1 players connected";
	protected static const string TEXT_NO_ADMIN = "No admin or Game Master is connected. Volunteer to become Game Master";
	protected static const string TEXT_VOTE = "%1 wants to be Game Master: %2/%3 votes, %4 s left";

	protected AFM_DiDSetupComponent m_Setup;

	protected TextWidget m_wTitle;
	protected TextWidget m_wStatus;
	protected TextWidget m_wPlayers;
	protected TextWidget m_wVoteStatus;

	// The buttons are shown and hidden through the boxes that size them, so a hidden one leaves no gap
	protected Widget m_wVolunteerSize;
	protected Widget m_wVoteYesSize;
	protected Widget m_wVoteNoSize;

	protected SCR_SpinBoxComponent m_DefenderSpin;
	protected SCR_SpinBoxComponent m_AttackerSpin;
	protected SCR_SpinBoxComponent m_PrepareSpin;
	protected SCR_SpinBoxComponent m_DefenseSpin;
	protected SCR_SpinBoxComponent m_FailureSpin;
	protected SCR_SpinBoxComponent m_WarmupSpin;

	protected SCR_ButtonTextComponent m_StartButton;
	protected SCR_ButtonTextComponent m_VolunteerButton;
	protected SCR_ButtonTextComponent m_VoteYesButton;
	protected SCR_ButtonTextComponent m_VoteNoButton;
	protected SCR_InputButtonComponent m_PauseButton;
	protected SCR_InputButtonComponent m_ChatButton;
	protected SCR_ChatPanel m_ChatPanel;

	// What each line of a spin box stands for, in the order it lists them: an index into the setup
	// component's sides, or a number of seconds
	protected ref array<int> m_aDefenderSides = {};
	protected ref array<int> m_aAttackerSides = {};
	protected ref array<int> m_aPrepareSeconds = {};
	protected ref array<int> m_aDefenseSeconds = {};
	protected ref array<int> m_aFailureSeconds = {};
	protected ref array<int> m_aWarmupSeconds = {};

	protected bool m_bIsAdmin;
	protected float m_fAdminCheckTimer;
	protected SCR_VotingManagerComponent m_Voting;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		Widget root = GetRootWidget();

		m_wTitle = TextWidget.Cast(root.FindAnyWidget("Title"));
		m_wStatus = TextWidget.Cast(root.FindAnyWidget("Status"));
		m_wPlayers = TextWidget.Cast(root.FindAnyWidget("Players"));
		m_wVoteStatus = TextWidget.Cast(root.FindAnyWidget("VoteStatus"));
		m_wVolunteerSize = root.FindAnyWidget("VolunteerButtonSize");
		m_wVoteYesSize = root.FindAnyWidget("VoteYesButtonSize");
		m_wVoteNoSize = root.FindAnyWidget("VoteNoButtonSize");

		m_DefenderSpin = SCR_SpinBoxComponent.GetSpinBoxComponent("DefenderSide", root);
		m_AttackerSpin = SCR_SpinBoxComponent.GetSpinBoxComponent("AttackerSide", root);
		m_PrepareSpin = SCR_SpinBoxComponent.GetSpinBoxComponent("PrepareTime", root);
		m_DefenseSpin = SCR_SpinBoxComponent.GetSpinBoxComponent("DefenseTime", root);
		m_FailureSpin = SCR_SpinBoxComponent.GetSpinBoxComponent("FailureTime", root);
		m_WarmupSpin = SCR_SpinBoxComponent.GetSpinBoxComponent("SpawnWarmup", root);

		if (m_DefenderSpin)
			m_DefenderSpin.m_OnChanged.Insert(OnSideChanged);

		if (m_AttackerSpin)
			m_AttackerSpin.m_OnChanged.Insert(OnSideChanged);

		if (m_PrepareSpin)
			m_PrepareSpin.m_OnChanged.Insert(OnTimeChanged);

		if (m_DefenseSpin)
			m_DefenseSpin.m_OnChanged.Insert(OnTimeChanged);

		if (m_FailureSpin)
			m_FailureSpin.m_OnChanged.Insert(OnTimeChanged);

		if (m_WarmupSpin)
			m_WarmupSpin.m_OnChanged.Insert(OnTimeChanged);

		m_StartButton = SCR_ButtonTextComponent.GetButtonText("StartButton", root);
		if (m_StartButton)
			m_StartButton.m_OnClicked.Insert(OnStartClicked);

		m_VolunteerButton = SCR_ButtonTextComponent.GetButtonText("VolunteerButton", root);
		if (m_VolunteerButton)
			m_VolunteerButton.m_OnClicked.Insert(OnVolunteerClicked);

		m_VoteYesButton = SCR_ButtonTextComponent.GetButtonText("VoteYesButton", root);
		if (m_VoteYesButton)
			m_VoteYesButton.m_OnClicked.Insert(OnVoteYesClicked);

		m_VoteNoButton = SCR_ButtonTextComponent.GetButtonText("VoteNoButton", root);
		if (m_VoteNoButton)
			m_VoteNoButton.m_OnClicked.Insert(OnVoteNoClicked);

		m_PauseButton = SCR_InputButtonComponent.GetInputButtonComponent("PauseButton", root);
		if (m_PauseButton)
			m_PauseButton.m_OnActivated.Insert(OnPauseMenu);

		// The chat itself is the HUD's own panel, which the HUD manager moves into this menu's chat slot
		Widget chat = root.FindAnyWidget("ChatPanel");
		if (chat)
			m_ChatPanel = SCR_ChatPanel.Cast(chat.FindHandler(SCR_ChatPanel));

		m_ChatButton = SCR_InputButtonComponent.GetInputButtonComponent("ChatButton", root);
		if (m_ChatButton)
			m_ChatButton.m_OnActivated.Insert(OnChatToggle);

		m_Setup = AFM_DiDSetupComponent.GetInstance();
		if (m_Setup)
			m_Setup.GetOnSetupChanged().Insert(RefreshSettings);

		// The vote is vanilla's and so is everything about it; the screen only listens
		m_Voting = SCR_VotingManagerComponent.GetInstance();
		if (m_Voting)
		{
			m_Voting.GetOnVotingStart().Insert(OnVotingStartOrLocal);
			m_Voting.GetOnVotingEnd().Insert(OnVotingEnd);
			m_Voting.GetOnVoteCountChanged().Insert(OnVoteCountChanged);
			m_Voting.GetOnVoteLocal().Insert(OnVotingStartOrLocal);
			m_Voting.GetOnAbstainVoteLocal().Insert(OnVotingStartOrLocal);
		}

		RefreshSettings();
		RefreshAdmin(true);
		RefreshVote();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (m_Setup)
			m_Setup.GetOnSetupChanged().Remove(RefreshSettings);

		if (m_Voting)
		{
			m_Voting.GetOnVotingStart().Remove(OnVotingStartOrLocal);
			m_Voting.GetOnVotingEnd().Remove(OnVotingEnd);
			m_Voting.GetOnVoteCountChanged().Remove(OnVoteCountChanged);
			m_Voting.GetOnVoteLocal().Remove(OnVotingStartOrLocal);
			m_Voting.GetOnAbstainVoteLocal().Remove(OnVotingStartOrLocal);
		}

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		if (m_ChatPanel)
			m_ChatPanel.OnUpdateChat(tDelta);

		m_fAdminCheckTimer += tDelta;
		if (m_fAdminCheckTimer < ADMIN_CHECK_INTERVAL_S)
			return;

		m_fAdminCheckTimer = 0;
		RefreshAdmin(false);

		// Also what ticks the seconds left of a running vote down
		RefreshVote();
	}

	//------------------------------------------------------------------------------------------------
	// Showing
	//------------------------------------------------------------------------------------------------

	//! Put what the match is set to on the screen. Rebuilt from scratch rather than patched: it happens
	//! when an admin changes something, which is rare, and what a spin box may list depends on the rest -
	//! a side leaves one list the moment the other role takes it.
	protected void RefreshSettings()
	{
		if (!m_Setup)
			return;

		if (m_wTitle)
			m_wTitle.SetText(m_Setup.GetScenarioTitle());

		FillSideSpin(m_DefenderSpin, m_aDefenderSides, false);
		FillSideSpin(m_AttackerSpin, m_aAttackerSides, true);

		FillTimeSpin(m_PrepareSpin, m_aPrepareSeconds, PHASE_TIME_STEPS_S, AFM_EDiDSetupTime.PREPARE);
		FillTimeSpin(m_DefenseSpin, m_aDefenseSeconds, PHASE_TIME_STEPS_S, AFM_EDiDSetupTime.DEFENSE);
		FillTimeSpin(m_FailureSpin, m_aFailureSeconds, PHASE_TIME_STEPS_S, AFM_EDiDSetupTime.FAILURE);
		FillTimeSpin(m_WarmupSpin, m_aWarmupSeconds, SPAWN_WARMUP_STEPS_S, AFM_EDiDSetupTime.SPAWN_WARMUP);
	}

	//------------------------------------------------------------------------------------------------
	//! List the sides that can take the role, with the one that has it selected. That one is listed
	//! whether or not it could be picked again, so the screen always says what the match is set to.
	protected void FillSideSpin(SCR_SpinBoxComponent spin, notnull array<int> sides, bool attacker)
	{
		if (!spin)
			return;

		sides.Clear();
		spin.ClearAll();

		int currentSide = m_Setup.GetCurrentSideIndex(attacker);
		int selected = 0;

		for (int sideIndex = 0, count = m_Setup.GetSideCount(); sideIndex < count; sideIndex++)
		{
			if (sideIndex != currentSide && !m_Setup.IsSidePickable(attacker, sideIndex))
				continue;

			if (sideIndex == currentSide)
				selected = sides.Count();

			sides.Insert(sideIndex);
			spin.AddItem(m_Setup.GetSideLabel(sideIndex));
		}

		// Without telling OnSideChanged: this is the screen catching up, not the admin choosing
		spin.SetCurrentItem(selected, false, false, false);
	}

	//------------------------------------------------------------------------------------------------
	//! List the steps of one of the times, with the current value selected and added when it is not one
	//! of the steps
	protected void FillTimeSpin(SCR_SpinBoxComponent spin, notnull array<int> seconds, notnull array<int> steps, AFM_EDiDSetupTime time)
	{
		if (!spin)
			return;

		int current = m_Setup.GetTime(time);

		seconds.Copy(steps);
		if (!seconds.Contains(current))
		{
			seconds.Insert(current);
			seconds.Sort();
		}

		spin.ClearAll();
		foreach (int value : seconds)
		{
			spin.AddItem(FormatTime(time, value));
		}

		spin.SetCurrentItem(seconds.Find(current), false, false, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds as minutes and seconds, with the two values that mean something else spelled out
	protected string FormatTime(AFM_EDiDSetupTime time, int seconds)
	{
		if (seconds < 0)
			return TEXT_ZONES_OWN;

		// A zone that allows no contested time cannot be lost by being held, and no warm-up is no wait.
		// Zero seconds to prepare or to defend is just a very short phase.
		if (seconds == 0 && (time == AFM_EDiDSetupTime.FAILURE || time == AFM_EDiDSetupTime.SPAWN_WARMUP))
			return TEXT_NONE;

		int minutes = seconds / 60;
		int rest = seconds % 60;
		return string.Format("%1:%2 min", minutes, rest.ToString(2));
	}

	//------------------------------------------------------------------------------------------------
	//! Ask again whether the local player is an admin, and offer the controls accordingly
	protected void RefreshAdmin(bool force)
	{
		if (m_wPlayers)
			m_wPlayers.SetText(string.Format(TEXT_PLAYERS, GetGame().GetPlayerManager().GetPlayerCount()));

		bool isAdmin = AFM_DiDSetupComponent.IsSetupAdmin(SCR_PlayerController.GetLocalPlayerId());
		if (isAdmin == m_bIsAdmin && !force)
			return;

		m_bIsAdmin = isAdmin;

		SetSpinEnabled(m_DefenderSpin, isAdmin);
		SetSpinEnabled(m_AttackerSpin, isAdmin);
		SetSpinEnabled(m_PrepareSpin, isAdmin);
		SetSpinEnabled(m_DefenseSpin, isAdmin);
		SetSpinEnabled(m_FailureSpin, isAdmin);
		SetSpinEnabled(m_WarmupSpin, isAdmin);

		if (m_StartButton)
		{
			m_StartButton.SetEnabled(isAdmin, false);
			m_StartButton.SetVisible(isAdmin, false);
		}

		if (!m_wStatus)
			return;

		if (isAdmin)
			m_wStatus.SetText(TEXT_ADMIN);
		else
			m_wStatus.SetText(TEXT_WAITING);
	}

	//------------------------------------------------------------------------------------------------
	//! Show where the vote for a Game Master stands and offer what the local player may do about it.
	//! Nothing is kept here: it is read from the voting manager each time, on the one second tick and
	//! whenever the manager says something changed. A line with nothing to say is hidden: someone can
	//! start the match and no vote runs.
	protected void RefreshVote()
	{
		if (!m_Voting)
			m_Voting = SCR_VotingManagerComponent.GetInstance();

		SCR_VoterComponent voter = SCR_VoterComponent.GetInstance();
		int localId = SCR_PlayerController.GetLocalPlayerId();

		bool showVolunteer;
		bool showVote;
		string text;

		if (m_Voting && voter)
		{
			int nominee = GetNominee();
			if (nominee == -1)
			{
				// Nobody is up for the role. The vote can only be started while nobody can start the match.
				if (!AFM_DiDSetupComponent.IsAnyoneAbleToStart())
				{
					text = TEXT_NO_ADMIN;
					showVolunteer = m_Voting.IsVotingAvailable(EVotingType.EDITOR_IN, localId);
				}
			}
			else
			{
				int current, required;
				m_Voting.GetVoteCounts(EVotingType.EDITOR_IN, nominee, current, required);

				int secondsLeft = Math.Ceil(m_Voting.GetRemainingDurationOfVote(EVotingType.EDITOR_IN, nominee));
				text = string.Format(TEXT_VOTE, GetGame().GetPlayerManager().GetPlayerName(nominee), current, required, secondsLeft);

				// Whoever put themselves up has voted by doing so, and a vote cast or declined stays so
				showVote = nominee != localId && !m_Voting.IsLocalVote(EVotingType.EDITOR_IN, nominee) && !m_Voting.HasAbstainedLocally(EVotingType.EDITOR_IN, nominee);
			}
		}

		if (m_wVoteStatus)
		{
			m_wVoteStatus.SetText(text);
			m_wVoteStatus.SetVisible(!text.IsEmpty());
		}

		SetVoteButton(m_wVolunteerSize, m_VolunteerButton, showVolunteer);
		SetVoteButton(m_wVoteYesSize, m_VoteYesButton, showVote);
		SetVoteButton(m_wVoteNoSize, m_VoteNoButton, showVote);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetVoteButton(Widget size, SCR_ButtonTextComponent button, bool show)
	{
		if (size)
			size.SetVisible(show);

		if (button)
			button.SetEnabled(show, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetSpinEnabled(SCR_SpinBoxComponent spin, bool enabled)
	{
		if (spin)
			spin.SetEnabled(enabled, false);
	}

	//------------------------------------------------------------------------------------------------
	// Acting
	//------------------------------------------------------------------------------------------------

	//! An admin moved one of the two side spin boxes
	protected void OnSideChanged(SCR_SpinBoxComponent spin, int index)
	{
		bool attacker = spin == m_AttackerSpin;

		array<int> sides = m_aDefenderSides;
		if (attacker)
			sides = m_aAttackerSides;

		AFM_DiDSetupPlayerComponent player = AFM_DiDSetupPlayerComponent.GetLocal();
		if (!m_bIsAdmin || !m_Setup || !player || !sides.IsIndexValid(index))
		{
			// Not theirs to change: put the screen back on what the match is set to
			RefreshSettings();
			return;
		}

		player.SetSide(attacker, m_Setup.GetSidePath(sides[index]));
	}

	//------------------------------------------------------------------------------------------------
	//! An admin moved one of the time spin boxes
	protected void OnTimeChanged(SCR_SpinBoxComponent spin, int index)
	{
		AFM_EDiDSetupTime time = AFM_EDiDSetupTime.PREPARE;
		array<int> seconds = m_aPrepareSeconds;

		if (spin == m_DefenseSpin)
		{
			time = AFM_EDiDSetupTime.DEFENSE;
			seconds = m_aDefenseSeconds;
		}
		else if (spin == m_FailureSpin)
		{
			time = AFM_EDiDSetupTime.FAILURE;
			seconds = m_aFailureSeconds;
		}
		else if (spin == m_WarmupSpin)
		{
			time = AFM_EDiDSetupTime.SPAWN_WARMUP;
			seconds = m_aWarmupSeconds;
		}

		AFM_DiDSetupPlayerComponent player = AFM_DiDSetupPlayerComponent.GetLocal();
		if (!m_bIsAdmin || !player || !seconds.IsIndexValid(index))
		{
			RefreshSettings();
			return;
		}

		player.SetTime(time, seconds[index]);
	}

	//------------------------------------------------------------------------------------------------
	//! The screen stays up until the authority has started the match and says so, which closes it for
	//! everyone at once
	protected void OnStartClicked()
	{
		AFM_DiDSetupPlayerComponent player = AFM_DiDSetupPlayerComponent.GetLocal();
		if (!m_bIsAdmin || !player)
			return;

		player.StartMatch();
	}

	//------------------------------------------------------------------------------------------------
	//! Put the local player up for Game Master. The vote counts that as their own first yes.
	protected void OnVolunteerClicked()
	{
		SCR_VoterComponent voter = SCR_VoterComponent.GetInstance();
		if (voter)
			voter.Vote(EVotingType.EDITOR_IN, SCR_PlayerController.GetLocalPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Vote yes on the running vote for a Game Master
	protected void OnVoteYesClicked()
	{
		SCR_VoterComponent voter = SCR_VoterComponent.GetInstance();
		int nominee = GetNominee();
		if (voter && nominee != -1)
			voter.Vote(EVotingType.EDITOR_IN, nominee);
	}

	//------------------------------------------------------------------------------------------------
	//! Vote no. Vanilla has no vote against: declining is only remembered locally, so the buttons go
	//! away, and the vote simply gets no yes from this player.
	protected void OnVoteNoClicked()
	{
		SCR_VoterComponent voter = SCR_VoterComponent.GetInstance();
		int nominee = GetNominee();
		if (voter && nominee != -1)
			voter.AbstainVote(EVotingType.EDITOR_IN, nominee);
	}

	//------------------------------------------------------------------------------------------------
	//! Who the running vote is for, or -1 when no vote runs. Vanilla allows several at once; the local
	//! player's own comes first, so they are not offered to volunteer again or to vote on somebody else's.
	protected int GetNominee()
	{
		if (!m_Voting)
			return -1;

		array<int> values = {};
		m_Voting.GetAllVotingValues(EVotingType.EDITOR_IN, values, false, false);
		if (values.IsEmpty())
			return -1;

		if (values.Contains(SCR_PlayerController.GetLocalPlayerId()))
			return SCR_PlayerController.GetLocalPlayerId();

		return values[0];
	}

	//------------------------------------------------------------------------------------------------
	// The voting manager's invokers. Each only asks the screen to read the vote again.
	protected void OnVotingStartOrLocal(EVotingType type, int value)
	{
		if (type == EVotingType.EDITOR_IN)
			RefreshVote();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnVotingEnd(EVotingType type, int value, int winner)
	{
		if (type == EVotingType.EDITOR_IN)
			RefreshVote();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnVoteCountChanged(EVotingType type, int value, int voteCount)
	{
		if (type == EVotingType.EDITOR_IN)
			RefreshVote();
	}

	//------------------------------------------------------------------------------------------------
	//! The way out: the pause menu, opened on top of this screen the way the deploy screens do it
	protected void OnPauseMenu()
	{
		MenuBase menu = GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.PauseMenu, 0, true, false);

		PauseMenuUI pauseMenu = PauseMenuUI.Cast(menu);
		if (pauseMenu)
		{
			pauseMenu.FadeBackground(true, true);
			pauseMenu.DisableSettings();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnChatToggle()
	{
		// The panel may not have been in this menu's slot yet when the menu opened
		if (!m_ChatPanel)
		{
			Widget chat = GetRootWidget().FindAnyWidget("ChatPanel");
			if (chat)
				m_ChatPanel = SCR_ChatPanel.Cast(chat.FindHandler(SCR_ChatPanel));
		}

		if (!m_ChatPanel || m_ChatPanel.IsOpen())
			return;

		SCR_ChatPanelManager chatPanelManager = SCR_ChatPanelManager.GetInstance();
		if (chatPanelManager)
			chatPanelManager.ToggleChatPanel(m_ChatPanel);
	}
}
