//------------------------------------------------------------------------------------------------
//! What the spectator camera of Defense In Depth adds to vanilla's manual camera: following a living
//! player, and two lines of text that say who is followed and which keys do what.
//!
//! Flying is vanilla's and is not touched. Following is vanilla's too: SCR_AttachManualCameraComponent
//! hangs the camera under a helper entity of its own that it moves along with the target every frame.
//! The camera is never made a child of the player's body itself - a body that streams out or is deleted
//! would take the camera with it - and when the target does disappear vanilla lets go by itself and the
//! camera stays where it is. This component only chooses the target.
//!
//! The keys are read here, in the camera's own frame, the way vanilla's components read theirs. So there
//! is nothing to bind and unbind: they work while this camera is the one being flown and has input, and
//! not under a menu, in chat or after the camera is gone.
//!
//! Must run before the attach component (a higher priority than its 30), which is what carries a change
//! of target made here into the same frame.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseManualCameraComponentTitle()]
class AFM_DiDSpectatorManualCameraComponent : SCR_BaseManualCameraComponent
{
	[Attribute("{14EAE18AB9985CCB}UI/layouts/HUD/DiD_SpectatorHUD.layout", UIWidgets.ResourceNamePicker, "Shown while spectating: the name of the followed player in the text widget called Target, and the key hints", params: "layout")]
	protected ResourceName m_sLayout;

	// Defined in Configs/System/chimeraInputCommon.conf
	protected static const string ACTION_CONTEXT = "AFM_SpectatorContext";
	protected static const string ACTION_NEXT = "AFM_SpectatorNext";
	protected static const string ACTION_PREVIOUS = "AFM_SpectatorPrevious";
	protected static const string ACTION_FREE = "AFM_SpectatorFree";

	protected static const string TARGET_WIDGET = "Target";
	protected static const string FREE_CAMERA_TEXT = "Free camera";

	// Where the camera is put when it starts following somebody: behind and above them, looking at their chest
	protected static const float FOLLOW_DISTANCE_M = 4;
	protected static const float FOLLOW_HEIGHT_M = 2.5;
	protected static const float FOLLOW_LOOK_HEIGHT_M = 1.2;

	protected AFM_GameModeDiD m_GameMode;
	protected SCR_AttachManualCameraComponent m_Attach;
	protected Widget m_wRoot;
	protected TextWidget m_wTarget;

	// Who the camera follows and the body it is attached to. 0 and null in free flight.
	protected int m_iFollowedPlayerId;
	protected IEntity m_FollowedBody;

	//------------------------------------------------------------------------------------------------
	override bool EOnCameraInit()
	{
		SCR_ManualCamera camera = GetCameraEntity();

		m_GameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		m_Attach = SCR_AttachManualCameraComponent.Cast(camera.FindCameraComponent(SCR_AttachManualCameraComponent));
		if (!m_GameMode || !m_Attach)
		{
			PrintFormat("AFM_DiDSpectatorManualCameraComponent: No AFM_GameModeDiD or no attach component on the camera, players cannot be followed", level: LogLevel.ERROR);
			return false;
		}

		m_Attach.GetOnAttachChange().Insert(OnAttachChange);

		// Through the camera's own frame rather than a menu: a menu on top takes the camera's input away
		m_wRoot = camera.CreateCameraWidget(m_sLayout);
		if (m_wRoot)
			m_wTarget = TextWidget.Cast(m_wRoot.FindAnyWidget(TARGET_WIDGET));

		ShowTarget();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnCameraExit()
	{
		if (m_Attach)
			m_Attach.GetOnAttachChange().Remove(OnAttachChange);

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnCameraFrame(SCR_ManualCameraParam param)
	{
		// The followed player died or holds another body now. A corpse still exists, so vanilla would keep
		// following it: let go, and stay where the camera is.
		if (m_iFollowedPlayerId != 0 && m_GameMode.GetLivingBody(m_iFollowedPlayerId) != m_FollowedBody)
			StopFollowing(param);

		// Under a menu or a dialog the camera takes no input, and neither do these keys. The line that
		// names them goes with them, or it would sit on top of the end screen.
		if (m_wRoot)
			m_wRoot.SetVisible(param.isManualInputEnabled);

		if (!param.isManualInputEnabled)
			return;

		InputManager inputManager = GetInputManager();
		inputManager.ActivateContext(ACTION_CONTEXT);

		if (inputManager.GetActionTriggered(ACTION_NEXT))
			FollowNeighbour(true, param);
		else if (inputManager.GetActionTriggered(ACTION_PREVIOUS))
			FollowNeighbour(false, param);
		else if (inputManager.GetActionTriggered(ACTION_FREE))
			StopFollowing(param);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla's attach component reports every attach and detach, its own included: it lets go by itself
	//! when the target's body is deleted or streams out
	protected void OnAttachChange(bool attached, IEntity target)
	{
		m_iFollowedPlayerId = 0;
		m_FollowedBody = null;

		if (attached && target)
		{
			m_iFollowedPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(target);
			m_FollowedBody = target;
		}

		ShowTarget();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowTarget()
	{
		if (!m_wTarget)
			return;

		if (m_iFollowedPlayerId == 0)
			m_wTarget.SetText(FREE_CAMERA_TEXT);
		else
			m_wTarget.SetText(GetGame().GetPlayerManager().GetPlayerName(m_iFollowedPlayerId));
	}

	//------------------------------------------------------------------------------------------------
	//! Players that can be followed from this machine, in the order of their player ids: alive, and their
	//! body exists here. A body far away from everything this machine is shown may not.
	protected void GetFollowablePlayerIds(notnull array<int> outPlayerIds)
	{
		outPlayerIds.Clear();

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		playerIds.Sort();

		foreach (int playerId : playerIds)
		{
			if (m_GameMode.GetLivingBody(playerId))
				outPlayerIds.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Follow the player after or before the one followed now, wrapping around at either end.
	//!
	//! Found by player id rather than by position in the list, so the order holds when the followed
	//! player is no longer in it. From free flight it is the first player, or the last.
	protected void FollowNeighbour(bool next, SCR_ManualCameraParam param)
	{
		array<int> followable = {};
		GetFollowablePlayerIds(followable);

		if (followable.IsEmpty())
			return;

		int count = followable.Count();
		int index;

		if (next)
		{
			index = 0;
			for (int i = 0; i < count; i++)
			{
				if (followable[i] > m_iFollowedPlayerId)
				{
					index = i;
					break;
				}
			}
		}
		else
		{
			index = count - 1;
			for (int j = count - 1; j >= 0; j--)
			{
				if (followable[j] < m_iFollowedPlayerId)
				{
					index = j;
					break;
				}
			}
		}

		// The only one there is, and followed already
		if (followable[index] == m_iFollowedPlayerId)
			return;

		Follow(m_GameMode.GetLivingBody(followable[index]), param);
	}

	//------------------------------------------------------------------------------------------------
	//! Put the camera behind the player and hand it to vanilla's attach component
	protected void Follow(IEntity body, SCR_ManualCameraParam param)
	{
		if (!body)
			return;

		SCR_ManualCamera camera = GetCameraEntity();

		// Off the previous player first, so that what is set below is a place in the world
		m_Attach.Detach();

		vector bodyTransform[4];
		body.GetWorldTransform(bodyTransform);

		vector position = bodyTransform[3] - bodyTransform[2] * FOLLOW_DISTANCE_M + vector.Up * FOLLOW_HEIGHT_M;
		vector lookAt = bodyTransform[3] + vector.Up * FOLLOW_LOOK_HEIGHT_M;

		vector transform[4];
		Math3D.DirectionAndUpMatrix((lookAt - position).Normalized(), vector.Up, transform);
		transform[3] = position;
		camera.SetWorldTransform(transform);

		if (!m_Attach.AttachTo(body))
			PrintFormat("AFM_DiDSpectatorManualCameraComponent: Vanilla's attach component refused %1, the camera stays free next to them", body, level: LogLevel.WARNING);

		ApplyCameraTransform(param);
	}

	//------------------------------------------------------------------------------------------------
	//! Back to free flight, from where the camera is. Does nothing when nobody is followed.
	protected void StopFollowing(SCR_ManualCameraParam param)
	{
		if (m_iFollowedPlayerId == 0)
			return;

		m_Attach.Detach();
		ApplyCameraTransform(param);
	}

	//------------------------------------------------------------------------------------------------
	//! Tell this frame where the camera is now.
	//!
	//! The frame works on a copy of the camera's transform taken before the components ran, in the space
	//! of whatever the camera hangs under, and writes it back at the end. Attaching, detaching and moving
	//! the camera all change what that copy should be. Vanilla's attach component does the same for itself,
	//! but only in a frame that has input, and a followed player can die while a menu is open.
	protected void ApplyCameraTransform(SCR_ManualCameraParam param)
	{
		SCR_ManualCamera camera = GetCameraEntity();

		camera.GetLocalTransform(param.transform);
		camera.GetLocalTransform(param.transformOriginal);
		param.isDirty = true;
	}
}
