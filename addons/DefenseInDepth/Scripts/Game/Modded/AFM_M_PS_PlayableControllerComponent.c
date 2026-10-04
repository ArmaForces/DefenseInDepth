//------------------------------------------------------------------------------------------------
//! Leaves the observer when a player is handed a body whose observer is already gone.
//!
//! PS takes a player out of spectator in OnControlledEntityChanged: whenever the new entity is not the
//! observer (no PS_LobbyVoNComponent), it calls SwitchFromObserver. That handler opens with
//! "if (!from && !m_bAfterInitialSwitch) return;", meant to skip the very first change on joining, when the
//! player goes from nothing to the observer.
//!
//! When a stage hands out bodies, PS's ApplyPlayable gives the player the body and deletes their observer on
//! the next frame. Both reach the client by replication with nothing ordering them, and when the deletion is
//! processed first the handover arrives with no "from". A player who has spent the stage start in spectator
//! has never had a change with both ends set, so the guard swallows the handover too: the HUD follows the body
//! while the spectator camera and menu stay up. Client logs from the 02.10 playtest show it on every stuck
//! join and on none of the good ones.
//!
//! So after PS has had its turn, this finishes the job PS would have done. It only acts when PS did not:
//! once PS has left spectator there is no camera left to remove.
//------------------------------------------------------------------------------------------------
modded class PS_PlayableControllerComponent
{
	// The spawn warm-up's camera move can arrive before the spectator camera exists, so it waits for it
	protected static const int AFM_SPECTATOR_MOVE_RETRY_MS = 250;
	protected static const int AFM_SPECTATOR_MOVE_MAX_ATTEMPTS = 60;

	protected vector m_vAFM_SpectatorTarget;
	protected int m_iAFM_SpectatorMoveAttempts;

	//------------------------------------------------------------------------------------------------
	//! Server: move this player's spectator camera to a position.
	//!
	//! While the spectator menu is open, PS teleports the observer entity to the camera every 333 ms, and the
	//! observer is what the server streams the world around. So this is also how a spectator gets an area
	//! loaded before they are put into a body in it.
	//!
	//! PS has RPC_SetCameraPosition for this, but it dereferences the camera without a check, and at the start
	//! of the match the move can arrive before the client has made its camera.
	void AFM_MoveSpectatorTo(vector position)
	{
		Rpc(AFM_RPC_MoveSpectatorTo, position);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AFM_RPC_MoveSpectatorTo(vector position)
	{
		m_vAFM_SpectatorTarget = position;
		m_iAFM_SpectatorMoveAttempts = 0;

		GetGame().GetCallqueue().Remove(AFM_ApplySpectatorMove);
		AFM_ApplySpectatorMove();
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_ApplySpectatorMove()
	{
		if (!m_Camera)
		{
			m_iAFM_SpectatorMoveAttempts++;
			if (m_iAFM_SpectatorMoveAttempts < AFM_SPECTATOR_MOVE_MAX_ATTEMPTS)
				GetGame().GetCallqueue().CallLater(AFM_ApplySpectatorMove, AFM_SPECTATOR_MOVE_RETRY_MS, false);
			else
				Print("AFM_PlayableController: No spectator camera to move to the first zone", LogLevel.WARNING);

			return;
		}

		m_Camera.SetOrigin(m_vAFM_SpectatorTarget);
		PrintFormat("AFM_PlayableController: Spectator camera moved to %1", m_vAFM_SpectatorTarget);
	}

	//------------------------------------------------------------------------------------------------
	override private void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		AFM_LeaveObserverForBody(from, to);
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_LeaveObserverForBody(IEntity from, IEntity to)
	{
		// Still spectating is the whole condition: PS clears the camera when it leaves spectator itself
		if (!to || !m_Camera)
			return;

		// The camera belongs to the machine that owns this controller, as in PS's own handler
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (!rpl || !rpl.IsOwner())
			return;

		// Going to the observer is going into spectator, not out of it
		if (to.FindComponent(PS_LobbyVoNComponent))
			return;

		if (!AFM_IsAliveCharacter(to))
			return;

		PrintFormat("AFM_PlayableController: Left spectator for the new body that PS skipped (previous entity: %1)", from);
		SwitchFromObserver();
	}

	//------------------------------------------------------------------------------------------------
	protected bool AFM_IsAliveCharacter(notnull IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		SCR_DamageManagerComponent damageManager = character.GetDamageManager();
		return damageManager && !damageManager.IsDestroyed();
	}
}
