//------------------------------------------------------------------------------------------------
//! What vanilla's game-over screen shows when a Defense In Depth match ends: who won, and the match
//! report underneath.
//!
//! The screen itself is vanilla's, opened on every machine by SCR_GameOverScreenManagerComponent when the
//! game mode goes to POSTGAME. It creates the content layout this component sits on and hands it the
//! title and the subtitle of the reason the match ended with, which the base class puts on the screen.
//! The report is the results frame the layout embeds, filled by its own component.
//!
//! All this adds is the case in which there is no report. The table goes out once, to the players
//! connected when the match ends, so somebody who joins afterwards has none: they get a line saying so
//! in place of an empty table.
//------------------------------------------------------------------------------------------------
class AFM_DiDGameOverContentUIComponent : SCR_GameOverScreenContentUIComponent
{
	protected static const string RESULTS_HOLDER_NAME = "ResultsHolder";
	protected static const string NO_REPORT_NAME = "NoReport";

	protected AFM_GameModeDiD m_GameMode;

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		if (SCR_Global.IsEditMode())
			return;

		// The screen can be up before the last row of the table has arrived
		m_GameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (m_GameMode)
			m_GameMode.GetOnMatchResults().Insert(RefreshReport);

		RefreshReport();
	}

	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		if (m_GameMode)
			m_GameMode.GetOnMatchResults().Remove(RefreshReport);

		super.HandlerDeattached(w);
	}

	//------------------------------------------------------------------------------------------------
	//! Show the report when this machine has one, and the line that says there is none when it has not
	protected void RefreshReport()
	{
		if (!m_wRoot)
			return;

		bool hasReport = m_GameMode && m_GameMode.GetMatchResults() != null;

		Widget results = m_wRoot.FindAnyWidget(RESULTS_HOLDER_NAME);
		if (results)
			results.SetVisible(hasReport);

		Widget noReport = m_wRoot.FindAnyWidget(NO_REPORT_NAME);
		if (noReport)
			noReport.SetVisible(!hasReport);
	}
}
