//------------------------------------------------------------------------------------------------
//! Spectators hear the living near their camera: the half of it that runs on the spectator's machine.
//!
//! This is a piece of its own, apart from the voice channel of the players without a body. Its other
//! half is the modded SCR_VoNComponent (Scripts/Game/Modded/AFM_M_SCR_VoNComponent.c), and the switch
//! for both is "Spectators Hear Living" on AFM_DiDVoiceComponent. With the switch off the channel
//! works as before. To take the piece out altogether, remove this file, the modded one and the parts
//! of AFM_DiDVoiceComponent marked "Spectators hear the living".
//!
//! How vanilla lets a Game Master hear the players around the editor camera: every player has an
//! editor manager entity, owned by their own machine. While the editor is open, vanilla moves that
//! entity to the editor camera every frame (SCR_CameraEditorComponent.EOnFrame) and the entity's
//! network movement component carries the position to the server. When somebody speaks, the server
//! asks the speaker's VoN component whether a listener's editor manager is an open editor, and if so
//! takes the position of that entity as the place the listener hears from.
//!
//! A spectator's camera is not the editor's, so nothing moves the editor manager for it. This class
//! does: it writes the transform of the spectator camera into the local player's editor manager, the
//! same two calls vanilla makes. The modded SCR_VoNComponent then answers "open editor" on the server
//! for the editor manager of a spectating player, and the living are heard from the camera with
//! vanilla's own fall-off of direct speech. Radio is not heard this way.
//!
//! Written only when the camera has moved a little and not more often than a few times a second: the
//! server needs the position to within earshot, not to the frame. The network simulation is switched
//! on again with every write. Switched on once, the server was seen to keep the first position it was
//! sent (the project this was taken from measured that), and switching on what is on costs nothing.
//!
//! Known weaknesses, accepted: the position is whatever the client reports, so a modified client can
//! listen anywhere on the map; and what the spectator hears lags behind the camera by the time the
//! position takes to reach the server, which is in the order of a second after a fast move.
//------------------------------------------------------------------------------------------------
class AFM_DiDSpectatorCameraFeed
{
	// The camera has to travel this far, and this much time has to pass, before its position is sent again
	protected static const float MIN_TRAVEL_M = 2;
	protected static const int MIN_INTERVAL_MS = 100;

	protected bool m_bFeeding;
	protected vector m_vLastPosition;
	protected int m_iLastWriteTick;
	protected int m_iWrites;

	//------------------------------------------------------------------------------------------------
	//! Call every frame on the machine of a player who may be spectating. Does nothing unless the
	//! spectator camera of Defense In Depth is the one being flown.
	void Update()
	{
		SCR_ManualCamera camera = AFM_DiDSpectatorComponent.GetLocalCamera();
		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();

		if (!camera || !editorManager || !IsMatchRunning())
		{
			Stop();
			return;
		}

		// The real editor moves its manager itself, from the moment it starts opening to the moment it has closed
		if (editorManager.IsOpened() || editorManager.IsInTransition())
		{
			Stop();
			return;
		}

		vector transform[4];
		camera.GetWorldTransform(transform);

		int now = System.GetTickCount();

		if (m_bFeeding)
		{
			if (now - m_iLastWriteTick < MIN_INTERVAL_MS)
				return;

			if (vector.DistanceSq(transform[3], m_vLastPosition) < MIN_TRAVEL_M * MIN_TRAVEL_M)
				return;
		}
		else
		{
			m_bFeeding = true;
			m_iWrites = 0;
			PrintFormat("AFM_DiDSpectatorCameraFeed: Telling the server where the spectator camera is, starting at %1, so that the living near it are heard", transform[3]);
		}

		// What SCR_CameraEditorComponent does for the editor camera
		editorManager.EnableCameraNwkSimulation(true);
		editorManager.SetWorldTransform(transform);

		m_vLastPosition = transform[3];
		m_iLastWriteTick = now;
		m_iWrites++;
	}

	//------------------------------------------------------------------------------------------------
	//! Stop sending the camera position. Does nothing when it is not being sent.
	void Stop()
	{
		if (!m_bFeeding)
			return;

		m_bFeeding = false;

		// Given back the way vanilla leaves it for a closed editor. Not while the real editor is opening
		// or open: from then on the switch is vanilla's, and it has just turned it on for itself.
		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (editorManager && !editorManager.IsOpened() && !editorManager.IsInTransition())
			editorManager.EnableCameraNwkSimulation(false);

		PrintFormat("AFM_DiDSpectatorCameraFeed: Stopped telling the server where the spectator camera is, after %1 position updates, the last one at %2", m_iWrites, m_vLastPosition);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true while the match is being played. There is nobody alive to hear before it, and after
	//! it everybody has been taken out of their body.
	protected bool IsMatchRunning()
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		return gameMode && gameMode.GetState() == SCR_EGameModeState.GAME;
	}
}
