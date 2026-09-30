//------------------------------------------------------------------------------------------------
//! The match report on the debriefing screen: the titles that were handed out, and the table behind
//! them.
//!
//! Reads AFM_GameModeDiD.GetMatchResults, which every machine assembles for itself from the
//! authority's broadcast. The order the two arrive in is not fixed - PS opens this menu when the game
//! state changes, which can beat the last row over the wire - so this fills whatever is there on
//! attach and fills again when the table is complete.
//------------------------------------------------------------------------------------------------
class AFM_DiDResultsFrameComponent : SCR_ScriptedWidgetComponent
{
	protected static const ResourceName AWARD_ROW_LAYOUT = "{2E71B4C8A0F5D936}UI/layouts/Debriefing/DiD_AwardRow.layout";
	protected static const ResourceName PLAYER_ROW_LAYOUT = "{8F3D60A25C1B74E9}UI/layouts/Debriefing/DiD_ResultRow.layout";

	protected Widget m_wAwards;
	protected Widget m_wPlayers;

	protected AFM_GameModeDiD m_GameMode;

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wAwards = w.FindAnyWidget("AwardsVerticalLayout");
		m_wPlayers = w.FindAnyWidget("PlayersVerticalLayout");

		m_GameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!m_GameMode)
			return;

		m_GameMode.GetOnMatchResults().Insert(Refresh);
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		if (m_GameMode)
			m_GameMode.GetOnMatchResults().Remove(Refresh);

		super.HandlerDeattached(w);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilt from scratch rather than patched, because this happens twice at most and a stale row is
	//! worse than a rebuilt one
	void Refresh()
	{
		if (!m_wAwards || !m_wPlayers || !m_GameMode)
			return;

		AFM_DiDMatchResults results = m_GameMode.GetMatchResults();
		if (!results)
			return;

		ClearChildren(m_wAwards);
		ClearChildren(m_wPlayers);

		SetHeadline(results.GetHeadline());
		FillAwards(results);
		FillPlayers(results);
	}

	//------------------------------------------------------------------------------------------------
	//! Who won, in words. The authority says it, because only there do the sides have names rather than
	//! faction keys. The layout's own text stands if nothing was sent.
	protected void SetHeadline(string headline)
	{
		if (headline.IsEmpty())
			return;

		TextWidget title = TextWidget.Cast(GetRootWidget().FindAnyWidget("TitleText"));
		if (title)
			title.SetText(headline);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillAwards(notnull AFM_DiDMatchResults results)
	{
		array<ref AFM_DiDAwardResult> awards = results.GetAwards();

		// A quiet match earns nothing, and an empty list looks like a bug rather than a quiet match
		if (!awards || awards.IsEmpty())
		{
			Widget row = GetGame().GetWorkspace().CreateWidgets(AWARD_ROW_LAYOUT, m_wAwards);
			SetText(row, "Label", "Nothing worth a title happened");
			SetText(row, "Value", "");
			return;
		}

		foreach (AFM_DiDAwardResult award : awards)
		{
			Widget row = GetGame().GetWorkspace().CreateWidgets(AWARD_ROW_LAYOUT, m_wAwards);
			if (!row)
				continue;

			SetText(row, "Label", award.GetTitle());
			SetText(row, "Value", award.GetWinners() + " - " + award.GetValue());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Best bot killer first, which is the same order the server writes to the log
	protected void FillPlayers(notnull AFM_DiDMatchResults results)
	{
		Widget header = GetGame().GetWorkspace().CreateWidgets(PLAYER_ROW_LAYOUT, m_wPlayers);
		SetRankIcon(header, string.Empty);
		SetText(header, "Label", "Player");
		SetText(header, "Col1", "Bots");
		SetText(header, "Col2", "Deaths");
		SetText(header, "Col3", "Built");
		SetText(header, "Col4", "Build");
		SetText(header, "Col5", "Arsenal");

		array<AFM_DiDPlayerStats> remaining = {};
		foreach (AFM_DiDPlayerStats row : results.GetRows())
		{
			remaining.Insert(row);
		}

		// Selection sort on a lobby's worth of records, same as the server's own dump does
		while (!remaining.IsEmpty())
		{
			int bestIndex = 0;
			for (int i = 1; i < remaining.Count(); i++)
			{
				if (remaining[i].Get(AFM_EDiDStat.BOT_KILLS) > remaining[bestIndex].Get(AFM_EDiDStat.BOT_KILLS))
					bestIndex = i;
			}

			AFM_DiDPlayerStats best = remaining[bestIndex];
			remaining.Remove(bestIndex);

			AddPlayerRow(best);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AddPlayerRow(notnull AFM_DiDPlayerStats stats)
	{
		Widget row = GetGame().GetWorkspace().CreateWidgets(PLAYER_ROW_LAYOUT, m_wPlayers);
		if (!row)
			return;

		SetRankIcon(row, stats.GetRankInsignia());
		SetText(row, "Label", stats.GetName());
		SetText(row, "Col1", stats.GetText(AFM_EDiDStat.BOT_KILLS));
		SetText(row, "Col2", stats.GetText(AFM_EDiDStat.DEATHS));
		SetText(row, "Col3", stats.GetText(AFM_EDiDStat.STRUCTURES_BUILT));
		SetText(row, "Col4", FormatMinutes(stats.Get(AFM_EDiDStat.BUILD_SECONDS)));
		SetText(row, "Col5", FormatMinutes(stats.Get(AFM_EDiDStat.ARSENAL_SECONDS)));
	}

	//------------------------------------------------------------------------------------------------
	//! Minutes, because a column is too narrow for four digits of seconds and nobody reads them anyway
	protected string FormatMinutes(float seconds)
	{
		if (seconds < 60)
			return "-";

		return Math.Round(seconds / 60).ToString() + "m";
	}

	//------------------------------------------------------------------------------------------------
	//! The insignia is an icon name inside vanilla's nametag imageset, resolved on the authority from
	//! whatever body the player held - the same set the nametags themselves draw from. Hidden rather than
	//! left empty for the header row and for anyone who never held a ranked body.
	protected void SetRankIcon(Widget row, string insignia)
	{
		if (!row)
			return;

		ImageWidget icon = ImageWidget.Cast(row.FindAnyWidget("Rank"));
		if (!icon)
			return;

		if (insignia.IsEmpty())
		{
			icon.SetVisible(false);
			return;
		}

		icon.LoadImageFromSet(0, SCR_XPInfoDisplay.GetRankIconImageSet(), insignia);
		icon.SetColor(Color.FromInt(UIColors.NEUTRAL_INFORMATION.PackToInt()));
		icon.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetText(Widget row, string widgetName, string text)
	{
		if (!row)
			return;

		TextWidget textWidget = TextWidget.Cast(row.FindAnyWidget(widgetName));
		if (textWidget)
			textWidget.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearChildren(notnull Widget parent)
	{
		Widget child = parent.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			parent.RemoveChild(child);
			child = next;
		}
	}
}
