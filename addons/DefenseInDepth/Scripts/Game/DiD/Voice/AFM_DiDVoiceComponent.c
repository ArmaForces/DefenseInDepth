//------------------------------------------------------------------------------------------------
//! The voice channel of Defense In Depth: one channel for every player who has no living body.
//!
//! The rule is a single one, the same in every state of the match: a player is in the channel exactly
//! while they are connected, have no living body and do not have the Game Master editor open. That
//! gives the three things the mode wants without a rule for each. Before the match nobody has a body,
//! so everyone on the setup screen can talk. During it the dead talk to each other, and to those who
//! joined late and wait for the next hand-out. After it every body is taken away for the debrief, so
//! everyone can talk again. "Living body" is the game mode's own definition (AFM_GameModeDiD.
//! HasLivingBody), the one the stages hand out bodies by: an unconscious player is alive and keeps
//! vanilla voice. A Game Master with the editor open keeps vanilla's Game Master voice.
//!
//! This component sits on the player controller and does two jobs with that rule, on two machines.
//!
//! On the server it keeps the list of members, which is what decides who hears whom: the carrier
//! (AFM_DiDVoNComponent) answers the engine from it. The server holds every player's controller, so
//! each player's membership is worked out by the component on their own controller.
//!
//! On the player's own machine it connects the carrier while the player is a member and disconnects
//! it when they are not, reads the talk key, and shows one line of text that says what the channel is
//! doing. The machine works the rule out for itself rather than being told by the server: everything
//! in the rule is known locally, and a machine that gets it wrong for a moment gains nothing, because
//! the server's list is the one that routes the voice.
//!
//! Like the spectator camera, the answer is worked out from scratch every time something that could
//! change it happens, and once a second besides, so a missed event cannot leave it wrong for long.
//!
//! The second, separate thing in here is that spectators hear the living near their camera. See
//! AFM_DiDSpectatorCameraFeed; everything of it in this file is marked "Spectators hear the living".
//------------------------------------------------------------------------------------------------
class AFM_DiDVoiceComponentClass: ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDVoiceComponent: ScriptComponent
{
	[Attribute("{2C315904D1D86DE4}UI/layouts/HUD/DiD_VoiceIndicator.layout", UIWidgets.ResourceNamePicker, "One line of text shown to a player who is in the voice channel of the players without a body, in the text widget called Text: how to talk, that their voice is being sent, and who is talking. Drawn above the setup and debrief screens", params: "layout", category: "DiD Voice")]
	protected ResourceName m_sIndicatorLayout;

	[Attribute("1", UIWidgets.CheckBox, "During the match, a player watching through the spectator camera also hears the direct speech of living players near that camera, fading with distance the way it does between the living. The living never hear spectators. Unticked, spectators only hear each other. The camera position used for it is reported by the spectator's own machine", category: "DiD Voice")]
	protected bool m_bSpectatorsHearLiving;

	// Most of what changes the answer has an event. What has none, or none that can be relied on - a body
	// deleted under its player, an editor manager that turns up late, the match changing state - is picked
	// up by asking again this often.
	protected static const int CHECK_INTERVAL_MS = 1000;

	// Defined in Configs/System/chimeraInputCommon.conf
	protected static const string ACTION_CONTEXT = "AFM_VoiceContext";
	protected static const string ACTION_TALK = "AFM_VoiceTalk";

	// A voice counts as being heard, or sent, for this long after its last packet
	protected static const int TALKING_WINDOW_MS = 500;

	// Above the menus, below what vanilla draws over everything (SCR_LoadingOverlay, 9001 and up)
	protected static const int INDICATOR_Z_ORDER = 9000;
	protected static const string INDICATOR_TEXT_WIDGET = "Text";

	protected static const string TEXT_IDLE = "Hold [T] to talk to everyone without a body";
	protected static const string TEXT_TRANSMITTING = "Transmitting";
	protected static const string TEXT_TALKING = "Talking: ";
	protected static const string TEXT_SEPARATOR = "   |   ";

	// Server only. The players in the channel, and those of them who also hear the living near their
	// spectator camera. Kept as two plain sets because the engine asks about them for every packet of
	// voice and every possible listener.
	protected static ref set<int> s_MemberIds = new set<int>();
	protected static ref set<int> s_SpectatorListenerIds = new set<int>();

	protected AFM_DiDVoNComponent m_Carrier;
	protected bool m_bCarrierMissingLogged;
	protected bool m_bListening;

	// The editor manager whose opening and closing is listened to
	protected SCR_EditorManagerEntity m_EditorManager;

	// Server: the player this controller was last worked out for, kept for the moment the controller is deleted
	protected int m_iServerPlayerId;

	// The player's own machine
	protected bool m_bConnected;
	protected bool m_bTalkHeld;
	protected bool m_bCaptureRefusedLogged;
	protected SCR_VONController m_VONController;
	protected bool m_bVanillaVoiceDisabled;
	protected Widget m_wIndicator;
	protected TextWidget m_wIndicatorText;
	protected string m_sIndicatorText;
	protected ref AFM_DiDSpectatorCameraFeed m_CameraFeed;

	//------------------------------------------------------------------------------------------------
	//! Server only. Returns true when both players are in the channel, which is the only case in which
	//! the channel has anything to say about how voice travels between them. The same player twice is
	//! the question of whether that one player is a member.
	static bool AreBothMembers(int playerId, int otherPlayerId)
	{
		return s_MemberIds.Contains(playerId) && s_MemberIds.Contains(otherPlayerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Spectators hear the living. Server only. Returns true when the player hears the living from
	//! where their spectator camera is: a member of the channel, during the match, with the switch on.
	static bool IsSpectatorListener(int playerId)
	{
		return s_SpectatorListenerIds.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		GetGame().GetCallqueue().CallLater(Refresh, CHECK_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Refresh);

		// The controller's own invokers go with it; the editor manager outlives it
		StopListeningToEditor();

		Disconnect("their controller is gone");

		// The player has left the server
		if (m_iServerPlayerId > 0)
		{
			if (s_MemberIds.RemoveItem(m_iServerPlayerId))
				PrintFormat("AFM_DiDVoiceComponent: Player %1 left the server and with it the voice channel of players without a body, %2 remain in it", m_iServerPlayerId, s_MemberIds.Count());

			s_SpectatorListenerIds.RemoveItem(m_iServerPlayerId);
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Work out whether this controller's player is in the channel right now, and act on it: on the
	//! server by keeping the member list, on the player's own machine by connecting or disconnecting
	//! the carrier. A listen server's host is both.
	protected void Refresh()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetOwner());
		if (!controller)
			return;

		// Not known yet for a moment after the controller is made
		int playerId = controller.GetPlayerId();
		if (playerId <= 0)
			return;

		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return;

		// A dedicated server has no local controller, a client has no controller but its own
		bool isServer = Replication.IsServer();
		bool isLocal = controller == GetGame().GetPlayerController();
		if (!isServer && !isLocal)
			return;

		if (!FindCarrier(controller, playerId))
			return;

		Listen(controller, playerId, isLocal);

		bool member = IsMemberNow(controller, playerId, isLocal, gameMode);

		if (isServer)
			ApplyOnServer(playerId, member, gameMode);

		if (isLocal)
			ApplyOnOwner(controller, playerId, member);
	}

	//------------------------------------------------------------------------------------------------
	//! The same, a frame later. An event is raised in the middle of the change it announces - the player
	//! manager still names the old body while the controller reports the new one, and a body about to be
	//! deleted still exists - so the answer is only right once the change has gone through.
	protected void QueueRefresh()
	{
		GetGame().GetCallqueue().Call(Refresh);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		QueueRefresh();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnControlledEntityDestroyed(Instigator killer, IEntity killerEntity)
	{
		QueueRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! The rule. Returns true when the player of this controller belongs in the channel right now.
	protected bool IsMemberNow(notnull SCR_PlayerController controller, int playerId, bool isLocal, notnull AFM_GameModeDiD gameMode)
	{
		// The game mode's own rule, the one the stages hand out bodies by and the spectator camera opens by
		if (gameMode.HasLivingBody(playerId))
			return false;

		// A Game Master with the editor open speaks from the editor camera, with vanilla's own voice. The
		// editor connects its own VoN component for that the moment it starts opening, so the time in
		// between counts as open.
		SCR_EditorManagerEntity editorManager = GetEditorManager(playerId, isLocal);
		if (editorManager && (editorManager.IsOpened() || editorManager.IsInTransition()))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The editor manager of this controller's player, or null when it is not there (yet). A client only
	//! has its own, and reaches it differently from how the server reaches everybody's.
	protected SCR_EditorManagerEntity GetEditorManager(int playerId, bool isLocal)
	{
		if (isLocal)
			return SCR_EditorManagerEntity.GetInstance();

		SCR_EditorManagerCore editorCore = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (!editorCore)
			return null;

		return editorCore.GetEditorManager(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Find the carrier on the controller and tell it whose it is. Returns false when the controller
	//! prefab has none, in which case there is no channel for this player.
	protected bool FindCarrier(notnull SCR_PlayerController controller, int playerId)
	{
		if (!m_Carrier)
			m_Carrier = AFM_DiDVoNComponent.Cast(controller.FindComponent(AFM_DiDVoNComponent));

		if (!m_Carrier)
		{
			if (!m_bCarrierMissingLogged)
			{
				m_bCarrierMissingLogged = true;
				PrintFormat("AFM_DiDVoiceComponent: The controller of player %1 has no AFM_DiDVoNComponent, they have no voice while without a body", playerId, level: LogLevel.ERROR);
			}

			return false;
		}

		m_Carrier.SetPlayerId(playerId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribe to everything that changes the answer. The controller once, the editor manager
	//! whenever a new one turns up: it reaches a machine some time after the controller does.
	protected void Listen(notnull SCR_PlayerController controller, int playerId, bool isLocal)
	{
		if (!m_bListening)
		{
			m_bListening = true;

			// A body arrives, or is taken away
			controller.m_OnControlledEntityChanged.Insert(OnControlledEntityChanged);

			// The body dies
			controller.m_OnDestroyed.Insert(OnControlledEntityDestroyed);
		}

		SCR_EditorManagerEntity editorManager = GetEditorManager(playerId, isLocal);
		if (!editorManager || editorManager == m_EditorManager)
			return;

		StopListeningToEditor();
		m_EditorManager = editorManager;

		// On the player's own machine
		editorManager.GetOnPreActivate().Insert(QueueRefresh);
		editorManager.GetOnClosed().Insert(QueueRefresh);

		// On the server
		editorManager.GetOnOpenedServer().Insert(QueueRefresh);
		editorManager.GetOnClosedServer().Insert(QueueRefresh);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopListeningToEditor()
	{
		if (!m_EditorManager)
			return;

		m_EditorManager.GetOnPreActivate().Remove(QueueRefresh);
		m_EditorManager.GetOnClosed().Remove(QueueRefresh);
		m_EditorManager.GetOnOpenedServer().Remove(QueueRefresh);
		m_EditorManager.GetOnClosedServer().Remove(QueueRefresh);
		m_EditorManager = null;
	}

	//------------------------------------------------------------------------------------------------
	// The server
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server only. Put the player on the member list or take them off it. From that moment the carrier
	//! routes their voice into the channel, or leaves it to vanilla.
	protected void ApplyOnServer(int playerId, bool member, notnull AFM_GameModeDiD gameMode)
	{
		m_iServerPlayerId = playerId;

		if (member != s_MemberIds.Contains(playerId))
		{
			if (member)
			{
				s_MemberIds.Insert(playerId);
				PrintFormat("AFM_DiDVoiceComponent: Player %1 (%2) has no living body and is in the voice channel of players without one, %3 in it now",
					playerId, GetGame().GetPlayerManager().GetPlayerName(playerId), s_MemberIds.Count());
			}
			else
			{
				s_MemberIds.RemoveItem(playerId);
				PrintFormat("AFM_DiDVoiceComponent: Player %1 (%2) has a living body or the Game Master editor open and is back on vanilla voice, %3 remain in the voice channel of players without a body",
					playerId, GetGame().GetPlayerManager().GetPlayerName(playerId), s_MemberIds.Count());
			}
		}

		ApplySpectatorListenerOnServer(playerId, member, gameMode);
	}

	//------------------------------------------------------------------------------------------------
	//! Spectators hear the living. Server only. A member hears the living from their spectator camera
	//! during the match, which is the only time there is a spectator camera and somebody alive to hear.
	//! The modded SCR_VoNComponent reads this list.
	protected void ApplySpectatorListenerOnServer(int playerId, bool member, notnull AFM_GameModeDiD gameMode)
	{
		bool listener = m_bSpectatorsHearLiving && member && gameMode.GetState() == SCR_EGameModeState.GAME;
		if (listener == s_SpectatorListenerIds.Contains(playerId))
			return;

		if (listener)
		{
			s_SpectatorListenerIds.Insert(playerId);
			PrintFormat("AFM_DiDVoiceComponent: Player %1 now hears the living near their spectator camera", playerId);
			return;
		}

		s_SpectatorListenerIds.RemoveItem(playerId);

		// Where the server last had the camera says whether the position ever arrived from the player's
		// machine: a camera that never left <0 0 0> was never reported
		vector cameraPosition;
		SCR_EditorManagerEntity editorManager = GetEditorManager(playerId, false);
		if (editorManager)
			cameraPosition = editorManager.GetOrigin();

		PrintFormat("AFM_DiDVoiceComponent: Player %1 no longer hears the living near their spectator camera, which the server last had at %2", playerId, cameraPosition);
	}

	//------------------------------------------------------------------------------------------------
	// The player's own machine
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	protected void ApplyOnOwner(notnull SCR_PlayerController controller, int playerId, bool member)
	{
		if (member == m_bConnected)
			return;

		if (member)
			Connect(controller, playerId);
		else
			Disconnect("they have a living body or the Game Master editor open");
	}

	//------------------------------------------------------------------------------------------------
	//! Join the channel on this machine.
	//!
	//! The carrier is connected the way vanilla connects the VoN component of the Game Master editor
	//! (SCR_EditorManagerEntity.Open): without that the engine neither records through it nor plays
	//! anything back through it. It speaks directly, never over a radio.
	//!
	//! Vanilla's own voice keys are taken out of the way for as long as the channel is joined. A dead
	//! player still controls their corpse, and vanilla's controller still holds the corpse's VoN
	//! component: its talk key would record through the corpse, next to the living. Vanilla does not
	//! offer its keys to the dead anyway (SCR_VONController.Update), this makes sure of it for everyone
	//! in the channel, and the channel has a talk key of its own.
	protected void Connect(notnull SCR_PlayerController controller, int playerId)
	{
		m_Carrier.ClearTalking();
		m_Carrier.ConnectEditorToVoNSystem(playerId);
		m_Carrier.SetCommMethod(ECommMethod.DIRECT);
		m_Carrier.SetTransmitRadio(null);

		m_bConnected = true;
		m_bTalkHeld = false;

		// True only when this call is what disabled it, so that what the pause menu disabled stays the pause menu's to give back
		m_VONController = SCR_VONController.Cast(controller.FindComponent(SCR_VONController));
		if (m_VONController && m_VONController.SetVONDisabled(true))
			m_bVanillaVoiceDisabled = true;

		ShowIndicator();

		// Spectators hear the living
		if (m_bSpectatorsHearLiving)
			m_CameraFeed = new AFM_DiDSpectatorCameraFeed();

		GetGame().GetCallqueue().CallLater(OnVoiceFrame, 0, true);

		PrintFormat("AFM_DiDVoiceComponent: Joined the voice channel of players without a body as player %1, vanilla voice keys are off", playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Leave the channel on this machine and hand voice back to vanilla. Does nothing when not joined.
	//!
	//! Vanilla's controller needs nothing more than its keys back. It takes the VoN component of a new
	//! body by itself when the body arrives (SCR_VONController.OnControlledEntityChanged), and the
	//! Game Master editor sets its own when it opens; the component it holds is never touched here.
	//!
	//! The keys are given back even under an open pause menu, where vanilla would rather have them
	//! off. Vanilla only gives back what it took itself, so keeping them here until the menu closes
	//! would leave them off for good; talking with the pause menu open until then harms nobody.
	protected void Disconnect(string reason)
	{
		if (!m_bConnected)
			return;

		m_bConnected = false;
		m_bTalkHeld = false;

		GetGame().GetCallqueue().Remove(OnVoiceFrame);

		// Spectators hear the living
		if (m_CameraFeed)
		{
			m_CameraFeed.Stop();
			m_CameraFeed = null;
		}

		if (m_Carrier)
		{
			m_Carrier.SetCapture(false);
			m_Carrier.DisconnectEditorFromVoNSystem();
		}

		if (m_bVanillaVoiceDisabled && m_VONController)
			m_VONController.SetVONDisabled(false);

		m_bVanillaVoiceDisabled = false;

		HideIndicator();

		PrintFormat("AFM_DiDVoiceComponent: Left the voice channel of players without a body (%1), vanilla voice keys are back", reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while the channel is joined on this machine: the talk key, the line of text, and the
	//! camera position for the spectators who hear the living.
	protected void OnVoiceFrame()
	{
		if (!m_bConnected || !m_Carrier)
			return;

		// Vanilla turns its keys back on when the pause menu closes, if the channel was joined under it
		if (m_VONController && !m_VONController.IsVONDisabled() && m_VONController.SetVONDisabled(true))
			m_bVanillaVoiceDisabled = true;

		bool talkHeld = IsTalkHeld();
		if (talkHeld != m_bTalkHeld)
		{
			m_bTalkHeld = talkHeld;

			bool accepted = m_Carrier.SetCapture(talkHeld);
			if (talkHeld && !accepted && !m_bCaptureRefusedLogged)
			{
				m_bCaptureRefusedLogged = true;
				PrintFormat("AFM_DiDVoiceComponent: The engine refused to record the microphone for the voice channel of players without a body. No microphone, or voice chat is not permitted for this player", level: LogLevel.WARNING);
			}
		}

		UpdateIndicator();

		// Spectators hear the living
		if (m_CameraFeed)
			m_CameraFeed.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true while the talk key is held down.
	//!
	//! The key is an action of its own in a context of its own, switched on from here every frame the
	//! way vanilla switches on its voice keys (SCR_VONController.Update). That is what makes it work on
	//! the setup screen, in the spectator camera and on the debrief screen alike: none of them has to
	//! know about it. Vanilla's own direct speech action is not used, because vanilla's controller
	//! listens to it and would start recording through the corpse.
	//!
	//! It is asked for every frame rather than listened to. While the chat is open its context shuts
	//! out all others, the action goes quiet and the release of the key is never reported; asking sees
	//! that as the key being up. A focused text field counts as typing too.
	protected bool IsTalkHeld()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return false;

		inputManager.ActivateContext(ACTION_CONTEXT);

		if (!inputManager.IsActionActive(ACTION_TALK) || inputManager.GetActionValue(ACTION_TALK) <= 0)
			return false;

		return !IsTyping();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true while a text field has the keyboard: the chat line, or a field of a menu
	protected bool IsTyping()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		Widget focused = workspace.GetFocusedWidget();
		if (!focused)
			return false;

		return EditBoxWidget.Cast(focused) != null || MultilineEditBoxWidget.Cast(focused) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Put the line of text on screen. Straight into the workspace and above the menus, because the HUD
	//! lies underneath the setup and debrief screens.
	protected void ShowIndicator()
	{
		if (m_wIndicator)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_wIndicator = workspace.CreateWidgets(m_sIndicatorLayout);
		if (!m_wIndicator)
		{
			PrintFormat("AFM_DiDVoiceComponent: Layout '%1' could not be created, the voice channel works but shows nothing", m_sIndicatorLayout, level: LogLevel.ERROR);
			return;
		}

		m_wIndicator.SetZOrder(INDICATOR_Z_ORDER);
		m_wIndicatorText = TextWidget.Cast(m_wIndicator.FindAnyWidget(INDICATOR_TEXT_WIDGET));
		m_sIndicatorText = string.Empty;

		UpdateIndicator();
	}

	//------------------------------------------------------------------------------------------------
	protected void HideIndicator()
	{
		if (m_wIndicator)
			m_wIndicator.RemoveFromHierarchy();

		m_wIndicator = null;
		m_wIndicatorText = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Say what the channel is doing: that the local player's voice is being sent, and whose voice is
	//! being heard. Both come from the engine's own reports of recording and playback on the carrier,
	//! not from the state of the key, so "Transmitting" means the microphone really is being sent.
	protected void UpdateIndicator()
	{
		if (!m_wIndicatorText || !m_Carrier)
			return;

		string text;

		if (m_Carrier.IsCapturing(TALKING_WINDOW_MS))
			text = TEXT_TRANSMITTING;

		array<int> talkingIds = {};
		m_Carrier.GetTalkingPlayerIds(talkingIds, TALKING_WINDOW_MS);

		if (!talkingIds.IsEmpty())
		{
			if (!text.IsEmpty())
				text = text + TEXT_SEPARATOR;

			text = text + TEXT_TALKING;

			PlayerManager playerManager = GetGame().GetPlayerManager();
			foreach (int index, int talkingId : talkingIds)
			{
				if (index > 0)
					text = text + ", ";

				text = text + playerManager.GetPlayerName(talkingId);
			}
		}

		if (text.IsEmpty())
			text = TEXT_IDLE;

		if (text == m_sIndicatorText)
			return;

		m_sIndicatorText = text;
		m_wIndicatorText.SetText(text);
	}
}
