//------------------------------------------------------------------------------------------------
//! One title as it was actually awarded: who took it and with what.
//!
//! Resolved on the authority and sent as three strings, so the page has nothing left to work out and
//! a client never needs the award config.
//------------------------------------------------------------------------------------------------
class AFM_DiDAwardResult
{
	protected string m_sTitle;
	protected string m_sWinners;
	protected string m_sValue;

	//------------------------------------------------------------------------------------------------
	void AFM_DiDAwardResult(string title, string winners, string value)
	{
		m_sTitle = title;
		m_sWinners = winners;
		m_sValue = value;
	}

	//------------------------------------------------------------------------------------------------
	string GetTitle()
	{
		return m_sTitle;
	}

	//------------------------------------------------------------------------------------------------
	//! One name, or several joined, when a title was shared
	string GetWinners()
	{
		return m_sWinners;
	}

	//------------------------------------------------------------------------------------------------
	//! Already formatted, unit included
	string GetValue()
	{
		return m_sValue;
	}
}

//------------------------------------------------------------------------------------------------
//! What a finished match has to say: a row per player and the titles that were handed out.
//!
//! The same class on both sides of the wire. The authority builds it from the live stats table, sends
//! it row by row, and every machine assembles its own copy - which is what the results page reads,
//! including on the machine that also happens to be the server.
//------------------------------------------------------------------------------------------------
class AFM_DiDMatchResults
{
	//! Titles travel joined into one string, so a title must not contain this
	static const string TITLE_SEPARATOR = ";";

	protected ref array<ref AFM_DiDPlayerStats> m_aRows = {};
	protected ref array<ref AFM_DiDAwardResult> m_aAwards = {};

	//------------------------------------------------------------------------------------------------
	void AddRow(notnull AFM_DiDPlayerStats row)
	{
		m_aRows.Insert(row);
	}

	//------------------------------------------------------------------------------------------------
	array<ref AFM_DiDPlayerStats> GetRows()
	{
		return m_aRows;
	}

	//------------------------------------------------------------------------------------------------
	void AddAward(string title, string winners, string value)
	{
		m_aAwards.Insert(new AFM_DiDAwardResult(title, winners, value));
	}

	//------------------------------------------------------------------------------------------------
	array<ref AFM_DiDAwardResult> GetAwards()
	{
		return m_aAwards;
	}

	//------------------------------------------------------------------------------------------------
	//! A match nobody played leaves nothing to show
	bool IsEmpty()
	{
		return m_aRows.IsEmpty();
	}
}
