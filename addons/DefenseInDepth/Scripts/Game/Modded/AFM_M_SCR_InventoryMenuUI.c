//------------------------------------------------------------------------------------------------
//! Times how long the local player keeps an arsenal open, for the match stats.
//!
//! Vanilla fires SCR_InventoryStorageBaseUI.GetOnArsenalEnter() when an arsenal's contents appear and
//! offers nothing for the other end, so the close is taken from this menu instead. Both live only on the
//! client, which is why the result is handed to AFM_DiDArsenalTimerComponent on the player controller
//! rather than written anywhere directly.
//!
//! The state is static because the invoker is, and because there is only ever one local inventory menu.
//------------------------------------------------------------------------------------------------
modded class SCR_InventoryMenuUI
{
	//! Tick count when an arsenal was first opened during this visit, or 0 when this was not an arsenal
	protected static int s_iAFM_ArsenalOpenedAt;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		s_iAFM_ArsenalOpenedAt = 0;
		SCR_InventoryStorageBaseUI.GetOnArsenalEnter().Insert(AFM_OnArsenalEntered);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		super.OnMenuClose();

		SCR_InventoryStorageBaseUI.GetOnArsenalEnter().Remove(AFM_OnArsenalEntered);
		AFM_ReportArsenalTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Only the first arsenal of a visit starts the clock: browsing several crates in one sitting is one
	//! session, not several
	protected void AFM_OnArsenalEntered()
	{
		if (s_iAFM_ArsenalOpenedAt == 0)
			s_iAFM_ArsenalOpenedAt = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_ReportArsenalTime()
	{
		if (s_iAFM_ArsenalOpenedAt == 0)
			return;

		int seconds = (System.GetTickCount() - s_iAFM_ArsenalOpenedAt) / 1000;
		s_iAFM_ArsenalOpenedAt = 0;

		if (seconds <= 0)
			return;

		AFM_DiDArsenalTimerComponent timer = AFM_DiDArsenalTimerComponent.GetLocal();
		if (timer)
			timer.ReportSeconds(seconds);
	}
}
