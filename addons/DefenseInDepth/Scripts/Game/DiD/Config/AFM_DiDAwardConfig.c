//------------------------------------------------------------------------------------------------
//! One title and the stat that decides who gets it.
//!
//! A minimum keeps the joke ones honest: nobody should be crowned "Rockets to the face" with zero of
//! them, and in a match where nothing of the sort happened the title is simply not handed out.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), BaseContainerCustomTitleField("m_sTitle")]
class AFM_DiDAwardEntry
{
	[Attribute("1", UIWidgets.CheckBox, desc: "Hand this title out at all. Uncheck to keep the entry without using it")]
	bool m_bEnabled;

	[Attribute("", UIWidgets.EditBox, desc: "Title as the results page shows it")]
	string m_sTitle;

	[Attribute("0", UIWidgets.ComboBox, desc: "Stat that decides the winner", enums: ParamEnumArray.FromEnum(AFM_EDiDStat))]
	AFM_EDiDStat m_eStat;

	[Attribute("", UIWidgets.EditBox, desc: "Unit shown after the number, e.g. 'kills' or 's'. Optional")]
	string m_sUnit;

	[Attribute("1", UIWidgets.CheckBox, desc: "Highest value wins. Uncheck for the lowest, for a title nobody is proud of")]
	bool m_bHighestWins;

	[Attribute("1", UIWidgets.EditBox, desc: "Do not hand it out at all below this value")]
	float m_fMinimum;

	[Attribute("1", UIWidgets.CheckBox, desc: "Share it between everyone level at the top. Uncheck to leave it unawarded on a tie")]
	bool m_bAwardTies;
}

//------------------------------------------------------------------------------------------------
//! Every title a match can hand out, in the order the results page lists them.
//!
//! A file rather than code, for the same reason the side configs are: adding a title should not be a
//! script change. Only the authority reads it - it resolves the winners and sends the finished list -
//! so a client never needs the file to see the results.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomStringTitleField("Award Config")]
class AFM_DiDAwardConfig
{
	[Attribute("", UIWidgets.Object, desc: "Titles handed out at the end of a match")]
	ref array<ref AFM_DiDAwardEntry> m_aAwards;

	//------------------------------------------------------------------------------------------------
	array<ref AFM_DiDAwardEntry> GetAwards()
	{
		return m_aAwards;
	}
}
