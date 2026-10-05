//------------------------------------------------------------------------------------------------
//! The voice of a player who has no living body: the "carrier" of the one voice channel that the dead,
//! the waiting and everybody on the setup and debrief screens share.
//!
//! Vanilla voice comes out of the VoN component of the body a player controls, so a player without a
//! body has no voice at all. The one exception vanilla makes is the Game Master: the editor manager
//! entity carries a VoN component of its own, which the editor connects by hand when it opens
//! (SCR_EditorManagerEntity.Open: ConnectEditorToVoNSystem) and which speaks from wherever the editor
//! camera is. This component is that same thing on the player controller, which every player has from
//! the moment they join. AFM_DiDVoiceComponent connects it on the player's own machine while they are
//! in the channel and disconnects it when they get a body.
//!
//! The sound project of this component (Filename on AFM_PlayerController_DiD.et) is not vanilla's
//! von.acp but Sounds/VON/AFM_DiDVoice_Flat.acp. A receiving client plays a voice at the position the
//! sender is reported at, and for the channel that is the far away point below, tens of kilometres from
//! every camera. Vanilla's project turns distance into attenuation, air-absorption filtering and
//! radio noise and filters, so a voice from there came out faint under static. The flat project is
//! vanilla's with all of that cut out: no distance attenuation or filtering, no noise, no reverb, 0 dB.
//! It keeps a silent path with a distance range, because the engine is believed to take the range of
//! a voice from the project (the project this mechanism came from does the same). Only this component uses it: bodies and the Game Master keep vanilla's von.acp, and
//! spectators hear living players through those bodies' own components, positionally as before.
//!
//! Who hears whom is decided on the server. For a voice that comes from an editor the engine does not
//! look at bodies: it asks a VoN component three questions (the script events of VoNComponent) -
//! which entity is the editor of player X, is that entity an active editor, and where in the world is
//! it - and delivers direct speech to everybody whose answer lies within earshot of the speaker's. The
//! overrides below answer them for the channel: every member's "editor" is their player controller, it
//! is active, and all of them are at one and the same point far outside the map. Members are therefore
//! at distance zero from one another and hear each other at full volume wherever their cameras are,
//! and no living player is anywhere near that point, so the living never hear a member.
//!
//! Which component is asked about whom: the engine calls these events from native code, so the scripts
//! do not show it. They take a player id or an entity as an argument, so a component is asked about
//! other players and not only about its own. The project this mechanism was taken from found that the
//! component that carries the speaker's voice is the one asked - about the speaker for where the voice
//! comes from, and about every possible listener for where they listen from. The answers here do not
//! depend on that: they are given for the pair "the player of this carrier" and "the player asked
//! about", and are the channel's only when both of them are members. So they are right whether this
//! carrier belongs to the speaker or to the listener, and one member alone never drags a living player
//! or a real Game Master into the channel.
//!
//! In every other case the answer is vanilla's own (super), which leaves voice between living players
//! and the voice of a Game Master with the editor open exactly as they were. That includes a machine
//! that keeps this carrier connected after its player was given a body: the server no longer counts
//! the player as a member, so nothing they say reaches the channel and nothing from it reaches them.
//!
//! The member list exists on the server alone (AFM_DiDVoiceComponent keeps it). On a client the
//! overrides always answer with vanilla's.
//------------------------------------------------------------------------------------------------
class AFM_DiDVoNComponentClass: SCR_VoNComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDVoNComponent: SCR_VoNComponent
{
	// Where every member of the channel is, as far as voice is concerned: far outside any terrain and
	// below the ground, so that no body and no editor camera is ever within earshot of it
	protected static const vector CHANNEL_POSITION = "-50000 -5000 -50000";

	// Whose controller this component sits on. A VoN component cannot ask for its owner, so it is told.
	protected int m_iPlayerId;

	// When this machine last recorded the local player's voice, and last played each other player's.
	// Milliseconds of System.GetTickCount, -1 for never.
	protected int m_iLastCaptureTick = -1;
	protected ref map<int, int> m_mLastReceiveTicks = new map<int, int>();

	//------------------------------------------------------------------------------------------------
	//! Tell the component which player's controller it is on. Called by AFM_DiDVoiceComponent, on the
	//! server and on the player's own machine.
	void SetPlayerId(int playerId)
	{
		m_iPlayerId = playerId;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true while the local player's voice is being recorded and sent, give or take windowMs
	bool IsCapturing(int windowMs)
	{
		if (m_iLastCaptureTick < 0)
			return false;

		return System.GetTickCount() - m_iLastCaptureTick <= windowMs;
	}

	//------------------------------------------------------------------------------------------------
	//! The players whose voice arrived on this machine within the last windowMs
	void GetTalkingPlayerIds(notnull array<int> outPlayerIds, int windowMs)
	{
		outPlayerIds.Clear();

		int now = System.GetTickCount();

		foreach (int playerId, int tick : m_mLastReceiveTicks)
		{
			if (now - tick <= windowMs)
				outPlayerIds.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Forget who was talking, so that nothing from before shows when the channel is joined again
	void ClearTalking()
	{
		m_iLastCaptureTick = -1;
		m_mLastReceiveTicks.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the engine every frame the microphone is being recorded through this component.
	//!
	//! Vanilla's own handler is left out on purpose. It draws the transmission on vanilla's voice HUD,
	//! which lies underneath the setup and debrief screens and would show a second time next to the
	//! channel's own line while spectating.
	override protected event void OnCapture(BaseTransceiver transmitter)
	{
		m_iLastCaptureTick = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the engine when another player's voice arrives for playback through this component.
	//! Vanilla's handler is left out for the same reason as in OnCapture.
	override protected event void OnReceive(int playerId, bool isSenderEditor, BaseTransceiver receiver, int frequency, float quality)
	{
		m_mLastReceiveTicks.Set(playerId, System.GetTickCount());

		// TEMPORARY voice diagnostics (AFM_VoiceDiag, see AFM_M_SCR_VoNComponent)
		if (AFM_VoiceDiagDue(m_iAFM_DiagReceiveTick))
		{
			m_iAFM_DiagReceiveTick = System.GetTickCount();
			PrintFormat("AFM_VoiceDiag: server=%1 carrier of player %2 OnReceive from player %3, sender is editor %4, over radio %5, quality %6", Replication.IsServer(), m_iPlayerId, playerId, isSenderEditor, receiver != null, quality);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "Which entity is the editor of this player?" For two members of the channel it is the asked
	//! player's controller, the entity this very component sits on over there.
	override protected event IEntity GetEditorEntity(int playerId)
	{
		// TEMPORARY voice diagnostics (AFM_VoiceDiag): the carrier is asked, before it answers
		if (AFM_VoiceDiagDue(m_iAFM_DiagEntityTick))
		{
			m_iAFM_DiagEntityTick = System.GetTickCount();
			PrintFormat("AFM_VoiceDiag: server=%1 carrier of player %2 asked GetEditorEntity(player %3), both members %4", Replication.IsServer(), m_iPlayerId, playerId, AFM_DiDVoiceComponent.AreBothMembers(m_iPlayerId, playerId));
		}

		if (AFM_DiDVoiceComponent.AreBothMembers(m_iPlayerId, playerId))
		{
			PlayerController controller = GetGame().GetPlayerManager().GetPlayerController(playerId);
			if (controller)
				return controller;
		}

		return super.GetEditorEntity(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! "Where is the editor camera of this player?" For two members of the channel it is the one point
	//! all of them share, which is what puts them within earshot of each other and of nobody else.
	override protected event vector GetEditorWorldLocation(int playerId)
	{
		// TEMPORARY voice diagnostics (AFM_VoiceDiag): the carrier is asked, before it answers
		if (AFM_VoiceDiagDue(m_iAFM_DiagLocationTick))
		{
			m_iAFM_DiagLocationTick = System.GetTickCount();
			PrintFormat("AFM_VoiceDiag: server=%1 carrier of player %2 asked GetEditorWorldLocation(player %3), both members %4", Replication.IsServer(), m_iPlayerId, playerId, AFM_DiDVoiceComponent.AreBothMembers(m_iPlayerId, playerId));
		}

		if (AFM_DiDVoiceComponent.AreBothMembers(m_iPlayerId, playerId))
			return CHANNEL_POSITION;

		return super.GetEditorWorldLocation(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! "Is this entity an editor that is open?" Yes for the controller of a member, when the player of
	//! this carrier is a member too. Vanilla's answer for a controller is no, and for an editor manager
	//! it is whether the Game Master has it open.
	override bool IsEntityActiveEditor(IEntity entity)
	{
		PlayerController controller = PlayerController.Cast(entity);

		// TEMPORARY voice diagnostics (AFM_VoiceDiag): the carrier is asked, before it answers
		if (AFM_VoiceDiagDue(m_iAFM_DiagActiveTick))
		{
			m_iAFM_DiagActiveTick = System.GetTickCount();
			PrintFormat("AFM_VoiceDiag: server=%1 carrier of player %2 asked IsEntityActiveEditor(%3), is a controller %4", Replication.IsServer(), m_iPlayerId, entity, controller != null);
		}

		if (controller && AFM_DiDVoiceComponent.AreBothMembers(m_iPlayerId, controller.GetPlayerId()))
			return true;

		return super.IsEntityActiveEditor(entity);
	}
}
