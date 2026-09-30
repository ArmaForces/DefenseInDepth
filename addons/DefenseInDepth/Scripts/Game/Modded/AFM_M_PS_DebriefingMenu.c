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

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		AFM_AppendMatchReport();
		AFM_AddVoiceActions();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		AFM_RemoveVoiceActions();

		super.OnMenuClose();
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
