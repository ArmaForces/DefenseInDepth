//------------------------------------------------------------------------------------------------
//! Adds the match report to the debriefing screen.
//!
//! PS builds that screen as a row of frames - one per faction - created into BodyHorizontalLayout when
//! the menu opens, so ours is simply one more frame in that row. Keeping the mission's own end-of-match
//! flow rather than opening a second screen on top of it.
//!
//! Every mission on this lobby mod gets this class, so the frame only appears when the game mode is
//! ours. The frame's own component does the filling.
//------------------------------------------------------------------------------------------------
modded class PS_DebriefingMenu
{
	protected static const ResourceName AFM_RESULTS_LAYOUT = "{5C2A9E17B4D3068F}UI/layouts/Debriefing/DiD_ResultsFrame.layout";

	//! PS's own voice room panel, the one the lobby and the briefing show
	protected static const ResourceName AFM_VOICE_LAYOUT = "{35DB604900C55B98}UI/VoiceChat/VoiceChatFrame.layout";

	// Right hand side, clear of the report frames in the middle. Same size the briefing gives it.
	protected static const float AFM_VOICE_WIDTH = 480;
	protected static const float AFM_VOICE_HEIGHT = 640;

	protected Widget m_wAFM_VoiceChat;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		AFM_AppendMatchReport();
		AFM_AppendVoiceChat();
		AFM_AddVoiceActions();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		AFM_RemoveVoiceActions();

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows who is in which voice room, and who is talking.
	//!
	//! The same panel PS embeds in its lobby, preview, briefing and spectator layouts, and leaves out of
	//! the debriefing one - so a player pressing the transmit key got no sign that anything was happening.
	//! Created here instead of in a layout of ours, because it belongs to PS's screen rather than to the
	//! match report, and the panel wires itself to the rooms manager as soon as it attaches.
	//------------------------------------------------------------------------------------------------
	protected void AFM_AppendVoiceChat()
	{
		Widget root = GetRootWidget();
		if (!root)
			return;

		// PS may add it upstream one day, and two panels would both be listening
		if (root.FindAnyWidget("VoiceChatFrame"))
			return;

		m_wAFM_VoiceChat = GetGame().GetWorkspace().CreateWidgets(AFM_VOICE_LAYOUT, root);
		if (!m_wAFM_VoiceChat)
			return;

		// Anchored to the middle of the right edge, the way the briefing map anchors it
		FrameSlot.SetAnchorMin(m_wAFM_VoiceChat, 1, 0.5);
		FrameSlot.SetAnchorMax(m_wAFM_VoiceChat, 1, 0.5);
		FrameSlot.SetSize(m_wAFM_VoiceChat, AFM_VOICE_WIDTH, AFM_VOICE_HEIGHT);
		FrameSlot.SetPos(m_wAFM_VoiceChat, -(AFM_VOICE_WIDTH + 10), -(AFM_VOICE_HEIGHT / 2));
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the debriefing screen a transmit key.
	//!
	//! Every other PS menu - the lobby, the preview map, the briefing map, the spectator screen - binds
	//! these three actions to the lobby VoN, and this one binds none of them, which is why nobody can talk
	//! over the debrief. Same actions, same handlers, so the keys are the ones players already use.
	//------------------------------------------------------------------------------------------------
	protected void AFM_AddVoiceActions()
	{
		InputManager input = GetGame().GetInputManager();

		input.AddActionListener("VONDirect", EActionTrigger.DOWN, AFM_Action_VoNOn);
		input.AddActionListener("VONDirect", EActionTrigger.UP, AFM_Action_VoNOff);
		input.AddActionListener("VONChannel", EActionTrigger.DOWN, AFM_Action_VoNFactionOn);
		input.AddActionListener("VONChannel", EActionTrigger.UP, AFM_Action_VoNOff);
		input.AddActionListener("LobbyAdminVON", EActionTrigger.DOWN, AFM_Action_VoNAdminOn);
		input.AddActionListener("LobbyAdminVON", EActionTrigger.UP, AFM_Action_VoNOff);

		// The key that hides the panel elsewhere hides it here too
		input.AddActionListener("SwitchVoiceChat", EActionTrigger.DOWN, AFM_Action_SwitchVoiceChat);
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_RemoveVoiceActions()
	{
		InputManager input = GetGame().GetInputManager();

		input.RemoveActionListener("VONDirect", EActionTrigger.DOWN, AFM_Action_VoNOn);
		input.RemoveActionListener("VONDirect", EActionTrigger.UP, AFM_Action_VoNOff);
		input.RemoveActionListener("VONChannel", EActionTrigger.DOWN, AFM_Action_VoNFactionOn);
		input.RemoveActionListener("VONChannel", EActionTrigger.UP, AFM_Action_VoNOff);
		input.RemoveActionListener("LobbyAdminVON", EActionTrigger.DOWN, AFM_Action_VoNAdminOn);
		input.RemoveActionListener("LobbyAdminVON", EActionTrigger.UP, AFM_Action_VoNOff);
		input.RemoveActionListener("SwitchVoiceChat", EActionTrigger.DOWN, AFM_Action_SwitchVoiceChat);
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_Action_SwitchVoiceChat()
	{
		if (m_wAFM_VoiceChat)
			m_wAFM_VoiceChat.SetVisible(!m_wAFM_VoiceChat.IsVisible());
	}

	//------------------------------------------------------------------------------------------------
	//! PS's own handlers dereference the controller and the VoN component without checking either, and at
	//! this point in a match a player may hold nothing at all - so the lookup is done here and a missing
	//! piece simply means no transmission rather than an exception per key press.
	protected PS_PlayableControllerComponent AFM_GetVoiceController()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return null;

		IEntity controlled = playerController.GetControlledEntity();
		if (!controlled)
			return null;

		// Lives on both the lobby entity and every PS character, and is what carries the transmission
		if (!PS_LobbyVoNComponent.Cast(controlled.FindComponent(PS_LobbyVoNComponent)))
			return null;

		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
			return null;

		// The radio lives on the lobby entity rather than the body, so it outlasts every respawn
		if (!playableController.GetTransceiver(EChannelType.PRIMARY))
			return null;

		return playableController;
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_Action_VoNOn()
	{
		PS_PlayableControllerComponent playableController = AFM_GetVoiceController();
		if (playableController)
			playableController.LobbyVoNEnable();
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_Action_VoNFactionOn()
	{
		PS_PlayableControllerComponent playableController = AFM_GetVoiceController();
		if (playableController)
			playableController.LobbyVoNFactionEnable();
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_Action_VoNAdminOn()
	{
		PS_PlayableControllerComponent playableController = AFM_GetVoiceController();
		if (playableController)
			playableController.LobbyVoNAdminEnable();
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_Action_VoNOff()
	{
		PS_PlayableControllerComponent playableController = AFM_GetVoiceController();
		if (playableController)
			playableController.LobbyVoNDisable();
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_AppendMatchReport()
	{
		if (!AFM_GameModeDiD.Cast(GetGame().GetGameMode()))
			return;

		// super resolves this from the root widget, so a null here means the screen itself changed shape
		if (!m_wBodyHorizontalLayout)
		{
			Print("AFM: Debriefing screen has no BodyHorizontalLayout, the match report was not added", LogLevel.WARNING);
			return;
		}

		GetGame().GetWorkspace().CreateWidgets(AFM_RESULTS_LAYOUT, m_wBodyHorizontalLayout);
	}
}
