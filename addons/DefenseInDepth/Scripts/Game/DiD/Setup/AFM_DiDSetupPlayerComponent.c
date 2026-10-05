//------------------------------------------------------------------------------------------------
//! Carries what an admin does on the setup screen from their machine to the authority.
//!
//! It lives on the player controller because that is an entity the client owns, which is what lets a
//! request arrive on the authority already knowing whose it is: the player is read from the controller
//! this component sits on, never from anything the client sent.
//!
//! Nothing is decided here. Whether the player is an admin, whether the match is still waiting in the
//! pre-game and whether the value makes sense are all for AFM_DiDSetupComponent to say, on the authority.
//! The screen only offers the controls to admins, but that is a courtesy of the screen - anyone can send
//! these requests, and everyone but an admin is ignored.
//------------------------------------------------------------------------------------------------
class AFM_DiDSetupPlayerComponentClass: ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDSetupPlayerComponent: ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	//! The component on the local player's controller. Menus have no owner of their own, so this is how
	//! one reaches something that can talk to the server.
	static AFM_DiDSetupPlayerComponent GetLocal()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return AFM_DiDSetupPlayerComponent.Cast(controller.FindComponent(AFM_DiDSetupPlayerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Client side. Ask for another side in a role, named by its config.
	void SetSide(bool attacker, ResourceName configPath)
	{
		if (configPath.IsEmpty())
			return;

		Rpc(RPC_DoSetSide, attacker, configPath);
	}

	//------------------------------------------------------------------------------------------------
	//! Client side. Ask for one of the times to be set.
	void SetTime(AFM_EDiDSetupTime time, int seconds)
	{
		Rpc(RPC_DoSetTime, time, seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Client side. Ask for the match to start with the setup as it stands.
	void StartMatch()
	{
		Rpc(RPC_DoStartMatch);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_DoSetSide(bool attacker, ResourceName configPath)
	{
		int playerId = GetOwnerPlayerId();
		AFM_DiDSetupComponent setup = AFM_DiDSetupComponent.GetInstance();
		if (playerId <= 0 || !setup)
			return;

		setup.RequestSetSide(playerId, attacker, configPath);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_DoSetTime(AFM_EDiDSetupTime time, int seconds)
	{
		int playerId = GetOwnerPlayerId();
		AFM_DiDSetupComponent setup = AFM_DiDSetupComponent.GetInstance();
		if (playerId <= 0 || !setup)
			return;

		setup.RequestSetTime(playerId, time, seconds);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_DoStartMatch()
	{
		int playerId = GetOwnerPlayerId();
		AFM_DiDSetupComponent setup = AFM_DiDSetupComponent.GetInstance();
		if (playerId <= 0 || !setup)
			return;

		setup.RequestStart(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Authority side. The player whose controller this component is on, which is the player the request
	//! came from. 0 when it is not on a player controller.
	protected int GetOwnerPlayerId()
	{
		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return 0;

		return controller.GetPlayerId();
	}
}
