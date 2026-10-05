//------------------------------------------------------------------------------------------------
//! The spectator of Defense In Depth: a free camera for a player who has no living body.
//!
//! A player who dies, joins in the middle of a stage or loses a COWABUNGA body waits for the next
//! hand-out of bodies. Until then they watch through vanilla's own manual camera - the one the Game
//! Master flies - made locally on their machine and never replicated. This component decides one thing
//! only: whether that camera exists.
//!
//! It exists exactly while all of this holds: the match is running or over, the local player has no
//! living body, and the Game Master or build editor is not open. The answer is worked out from scratch
//! every time something that could change it happens, rather than followed from one state to the next,
//! so a missed or doubled event cannot leave the camera in the wrong state for long.
//!
//! An unconscious player is alive and keeps vanilla's own view of it.
//!
//! Lives on the player controller, which a client only has for itself. A listen server holds everyone's,
//! and there only the host's own does anything.
//------------------------------------------------------------------------------------------------
class AFM_DiDSpectatorComponentClass: ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDSpectatorComponent: ScriptComponent
{
	[Attribute("{496FBC94D0849040}Prefabs/Editor/Camera/AFM_SpectatorCamera.et", UIWidgets.ResourceNamePicker, "The camera a player without a living body watches the match through. A vanilla manual camera, made locally on that player's machine", params: "et class=SCR_ManualCamera", category: "DiD Spectator")]
	protected ResourceName m_sCameraPrefab;

	// Everything that changes the answer has an event, except two things: the local body being deleted
	// under the player (a COWABUNGA squad being removed), for which vanilla promises no event on the
	// client, and the local editor manager, which reaches a client some time after the controller does.
	// Both are picked up by asking again this often.
	protected static const int CHECK_INTERVAL_MS = 1000;

	// A new camera looks slightly down, from this far above the body its player has just lost
	protected static const float START_PITCH_DEG = -20;
	protected static const float START_HEIGHT_ABOVE_BODY_M = 2;

	protected SCR_ManualCamera m_Camera;

	// The editor manager whose opening and closing is listened to
	protected SCR_EditorManagerEntity m_EditorManager;

	protected bool m_bListening;

	//------------------------------------------------------------------------------------------------
	//! The component on the local player's controller, or null where there is no local player
	static AFM_DiDSpectatorComponent GetLocal()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return AFM_DiDSpectatorComponent.Cast(controller.FindComponent(AFM_DiDSpectatorComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! The camera the local player is spectating through, or null when they are not spectating
	static SCR_ManualCamera GetLocalCamera()
	{
		AFM_DiDSpectatorComponent spectator = GetLocal();
		if (!spectator)
			return null;

		return spectator.m_Camera;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true while the local player watches through the spectator camera
	static bool IsLocalSpectating()
	{
		return GetLocalCamera() != null;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		// Nobody watches on a dedicated server
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		GetGame().GetCallqueue().CallLater(Refresh, CHECK_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Refresh);

		// The controller's own invokers go with it; these two outlive it
		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (m_bListening && gameMode)
			gameMode.GetOnMatchSituationChanged().Remove(QueueRefresh);

		if (m_EditorManager)
		{
			m_EditorManager.GetOnPreActivate().Remove(QueueRefresh);
			m_EditorManager.GetOnClosed().Remove(QueueRefresh);
		}

		Close();

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Make the camera exist or not exist, according to how things stand right now
	protected void Refresh()
	{
		// Only for the player sitting at this machine
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetOwner());
		if (!controller || controller != GetGame().GetPlayerController())
			return;

		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return;

		Listen(controller, gameMode);

		if (ShouldSpectate(controller, gameMode))
			Open(controller, gameMode);
		else
			Close();
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
	//! Subscribe to everything that changes the answer. The controller and the game mode once, the
	//! editor manager whenever a new one turns up.
	protected void Listen(notnull SCR_PlayerController controller, notnull AFM_GameModeDiD gameMode)
	{
		if (!m_bListening)
		{
			m_bListening = true;

			// A body arrives, or is taken away
			controller.m_OnControlledEntityChanged.Insert(OnControlledEntityChanged);

			// The body dies
			controller.m_OnDestroyed.Insert(OnControlledEntityDestroyed);

			// The match starts, and the stage says where it is watched from
			gameMode.GetOnMatchSituationChanged().Insert(QueueRefresh);
		}

		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (!editorManager || editorManager == m_EditorManager)
			return;

		m_EditorManager = editorManager;

		// The editor brings its own camera, and gives the view back when it closes
		editorManager.GetOnPreActivate().Insert(QueueRefresh);
		editorManager.GetOnClosed().Insert(QueueRefresh);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the local player should be watching through the spectator camera right now
	protected bool ShouldSpectate(notnull SCR_PlayerController controller, notnull AFM_GameModeDiD gameMode)
	{
		// Nothing to watch before the match, and after it everyone is taken out of their body for the debrief
		SCR_EGameModeState state = gameMode.GetState();
		if (state != SCR_EGameModeState.GAME && state != SCR_EGameModeState.POSTGAME)
			return false;

		// The game mode's own rule, the one the stages hand out bodies by
		if (gameMode.HasLivingBody(controller.GetPlayerId()))
			return false;

		// IsOpened only turns once the editor has finished opening or closing. Its camera is there for the
		// whole of both, so the time in between counts as open.
		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (editorManager && (editorManager.IsOpened() || editorManager.IsInTransition()))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Make the camera, the way vanilla makes its debug camera: a prefab spawned on this machine alone.
	//! Does nothing when it is already there, other than taking the view back if something else took it.
	protected void Open(notnull SCR_PlayerController controller, notnull AFM_GameModeDiD gameMode)
	{
		if (m_Camera)
		{
			CameraManager cameraManager = GetGame().GetCameraManager();
			if (cameraManager && cameraManager.CurrentCamera() != m_Camera)
				cameraManager.SetCamera(m_Camera);

			return;
		}

		// Nowhere to start from yet: the stage has not told this machine where it is. Asked again when it does.
		vector transform[4];
		if (!GetStartTransform(controller, gameMode, transform))
			return;

		Resource resource = Resource.Load(m_sCameraPrefab);
		if (!resource || !resource.IsValid())
		{
			PrintFormat("AFM_DiDSpectatorComponent: Camera prefab '%1' could not be loaded, there is no spectator", m_sCameraPrefab, level: LogLevel.ERROR);
			return;
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform = transform;

		// The camera takes the view for itself as it initialises
		m_Camera = SCR_ManualCamera.Cast(GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), spawnParams));
		if (!m_Camera)
		{
			PrintFormat("AFM_DiDSpectatorComponent: Camera prefab '%1' is not an SCR_ManualCamera, there is no spectator", m_sCameraPrefab, level: LogLevel.ERROR);
			return;
		}

		PrintFormat("AFM_DiDSpectatorComponent: Spectator camera opened at %1", transform[3], level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Where a new camera starts. Returns false when there is no such place yet.
	//!
	//! A player who has just died still controls the body they died in, and rises from it facing the way
	//! they were looking. Anybody else - joined in the middle of a stage, or their body was deleted - starts
	//! above the stage's spawn point, which the authority replicates on the game mode.
	protected bool GetStartTransform(notnull SCR_PlayerController controller, notnull AFM_GameModeDiD gameMode, out vector transform[4])
	{
		vector angles = Vector(0, START_PITCH_DEG, 0);
		vector position;

		IEntity body = controller.GetControlledEntity();
		if (body)
		{
			vector view[4];
			GetGame().GetWorld().GetCurrentCamera(view);
			vector viewAngles = Math3D.MatrixToAngles(view);

			angles[0] = viewAngles[0];
			position = body.GetOrigin() + vector.Up * START_HEIGHT_ABOVE_BODY_M;
		}
		else
		{
			position = gameMode.GetSpectatorAnchor();
			if (position == vector.Zero)
				return false;
		}

		Math3D.AnglesToMatrix(angles, transform);
		transform[3] = position;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Remove the camera in the order vanilla's editor removes its own (SCR_CameraEditorComponent): the
	//! camera's components are shut down, the view goes back to the camera of the controlled body, and
	//! only then is the entity deleted. That is what puts a player back behind their own eyes when a
	//! body arrives.
	protected void Close()
	{
		if (!m_Camera)
			return;

		// A camera following somebody hangs under vanilla's attach helper, which is deleted as the
		// components shut down. Let go first, so that the camera is not deleted along with it.
		SCR_AttachManualCameraComponent attach = SCR_AttachManualCameraComponent.Cast(m_Camera.FindCameraComponent(SCR_AttachManualCameraComponent));
		if (attach)
			attach.Detach();

		// Vanilla makes this frame for the widgets of the camera's components and never removes it
		Widget cameraWidget = m_Camera.GetWidget();

		m_Camera.Terminate();

		if (m_Camera)
		{
			m_Camera.TrySwitchToControlledEntityCamera();
			delete m_Camera;
		}

		if (cameraWidget)
			cameraWidget.RemoveFromHierarchy();

		PrintFormat("AFM_DiDSpectatorComponent: Spectator camera closed", level: LogLevel.DEBUG);
	}
}
