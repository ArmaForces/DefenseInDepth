//------------------------------------------------------------------------------------------------
//! Carries finished arsenal sessions from a client to the authority.
//!
//! Time in the arsenal cannot be measured on the server: the only signal is
//! SCR_InventoryStorageBaseUI.GetOnArsenalEnter(), a static invoker on a UI class, so it exists only on
//! the machine with the menu open. The modded inventory menu does the timing and hands the result here,
//! and this lives on the player controller because that is an entity the client owns - which is what
//! lets the RPC arrive on the authority already knowing whose it is.
//!
//! Which also means the number is as trustworthy as the client. It is capped on both sides so a hung or
//! meddling client cannot claim the title with an implausible figure, and the award it feeds is a joke
//! award rather than a scoreboard.
//------------------------------------------------------------------------------------------------
class AFM_DiDArsenalTimerComponentClass: ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDArsenalTimerComponent: ScriptComponent
{
	//! Half an hour in one visit is already absurd; beyond that it is a stuck client, not dedication
	protected static const int MAX_SESSION_SECONDS = 1800;

	//------------------------------------------------------------------------------------------------
	//! The component on the local player's controller. Menus have no owner of their own, so this is how
	//! one reaches something that can talk to the server.
	static AFM_DiDArsenalTimerComponent GetLocal()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return AFM_DiDArsenalTimerComponent.Cast(controller.FindComponent(AFM_DiDArsenalTimerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Client side. Called when an arsenal session ends.
	void ReportSeconds(int seconds)
	{
		if (seconds <= 0)
			return;

		Rpc(RPC_ReportArsenalSeconds, Math.ClampInt(seconds, 0, MAX_SESSION_SECONDS));
	}

	//------------------------------------------------------------------------------------------------
	//! Authority side. The owner of this component is the player controller that sent it, so nothing has
	//! to be taken on trust except the number itself.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_ReportArsenalSeconds(int seconds)
	{
		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return;

		int playerId = controller.GetPlayerId();
		if (playerId <= 0)
			return;

		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return;

		AFM_DiDStatsTracker stats = gameMode.GetStats();
		if (!stats)
			return;

		stats.AddArsenalSeconds(playerId, Math.ClampInt(seconds, 0, MAX_SESSION_SECONDS));
	}
}
