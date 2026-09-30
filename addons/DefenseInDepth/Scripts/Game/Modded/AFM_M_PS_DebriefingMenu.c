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
