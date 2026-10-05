//! Spectators hear the living near their camera: the half of it that runs on the server. The other
//! half, and the whole story, is in AFM_DiDSpectatorCameraFeed.
//!
//! When a living player speaks, the engine asks the VoN component that carries their voice - vanilla's
//! own, on their body - about every possible listener: is the listener's editor manager an open
//! editor? Vanilla says yes only for a Game Master with the editor open, and for such a listener the
//! engine takes the position of the editor manager as the place they hear from. This adds one more
//! yes: the editor manager of a player who is watching the match through the spectator camera, which
//! that player's machine keeps moving to where the camera is.
//!
//! It has to be a modded class because the component asked is the speaker's, and the speaker is an
//! ordinary character. Everything else is answered by vanilla. Without Defense In Depth's voice
//! component on the player controllers nobody is ever on the list this reads, and the answer is
//! always vanilla's.
//!
//! An editor manager still sitting at the world origin has never been moved to a camera: the player
//! is on the list but their spectator camera is not up yet. They would hear whoever stands at the
//! origin, so until the first position arrives the answer stays vanilla's.
modded class SCR_VoNComponent
{
	// TEMPORARY voice diagnostics, remove once living-to-spectator voice is understood. Everything
	// marked AFM_VoiceDiag in this file and in AFM_DiDVoNComponent goes with it.
	protected static const int AFM_VOICE_DIAG_INTERVAL_MS = 2000;
	protected int m_iAFM_DiagActiveTick;
	protected int m_iAFM_DiagEntityTick;
	protected int m_iAFM_DiagLocationTick;
	protected int m_iAFM_DiagUsedTick;
	protected int m_iAFM_DiagReceiveTick;

	//------------------------------------------------------------------------------------------------
	//! TEMPORARY voice diagnostics. Returns true at most once per interval for the tick it is given.
	protected bool AFM_VoiceDiagDue(int lastTick)
	{
		return System.GetTickCount() - lastTick >= AFM_VOICE_DIAG_INTERVAL_MS;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsEntityActiveEditor(IEntity entity)
	{
		bool result;

		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.Cast(entity);
		if (editorManager && editorManager.GetOrigin() != vector.Zero && AFM_DiDVoiceComponent.IsSpectatorListener(editorManager.GetPlayerID()))
			result = true;
		else
			result = super.IsEntityActiveEditor(entity);

		// TEMPORARY voice diagnostics
		if (AFM_VoiceDiagDue(m_iAFM_DiagActiveTick))
		{
			m_iAFM_DiagActiveTick = System.GetTickCount();

			int managerPlayerId = -1;
			if (editorManager)
				managerPlayerId = editorManager.GetPlayerID();

			PrintFormat("AFM_VoiceDiag: server=%1 %2 asked IsEntityActiveEditor(%3, editor manager of player %4) -> %5", Replication.IsServer(), this, entity, managerPlayerId, result);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMPORARY voice diagnostics: vanilla's answer, logged
	override protected event IEntity GetEditorEntity(int playerId)
	{
		IEntity result = super.GetEditorEntity(playerId);

		if (AFM_VoiceDiagDue(m_iAFM_DiagEntityTick))
		{
			m_iAFM_DiagEntityTick = System.GetTickCount();
			PrintFormat("AFM_VoiceDiag: server=%1 %2 asked GetEditorEntity(player %3) -> %4", Replication.IsServer(), this, playerId, result);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMPORARY voice diagnostics: vanilla's answer, logged
	override protected event vector GetEditorWorldLocation(int playerId)
	{
		vector result = super.GetEditorWorldLocation(playerId);

		if (AFM_VoiceDiagDue(m_iAFM_DiagLocationTick))
		{
			m_iAFM_DiagLocationTick = System.GetTickCount();
			PrintFormat("AFM_VoiceDiag: server=%1 %2 asked GetEditorWorldLocation(player %3) -> %4", Replication.IsServer(), this, playerId, result);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMPORARY voice diagnostics: a voice reached the server through this component
	override protected event void OnVoNUsed(int senderId)
	{
		super.OnVoNUsed(senderId);

		if (AFM_VoiceDiagDue(m_iAFM_DiagUsedTick))
		{
			m_iAFM_DiagUsedTick = System.GetTickCount();
			PrintFormat("AFM_VoiceDiag: server=%1 %2 OnVoNUsed, sender is player %3", Replication.IsServer(), this, senderId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! TEMPORARY voice diagnostics: a voice arrived on this machine for playback through this component
	override protected event void OnReceive(int playerId, bool isSenderEditor, BaseTransceiver receiver, int frequency, float quality)
	{
		super.OnReceive(playerId, isSenderEditor, receiver, frequency, quality);

		if (AFM_VoiceDiagDue(m_iAFM_DiagReceiveTick))
		{
			m_iAFM_DiagReceiveTick = System.GetTickCount();
			PrintFormat("AFM_VoiceDiag: server=%1 %2 OnReceive from player %3, sender is editor %4, over radio %5, quality %6", Replication.IsServer(), this, playerId, isSenderEditor, receiver != null, quality);
		}
	}
}
