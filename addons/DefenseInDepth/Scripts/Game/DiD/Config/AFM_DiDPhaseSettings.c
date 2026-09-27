//------------------------------------------------------------------------------------------------
//! Tri-state so "not set" stays distinct from "off". A header that says nothing about helicopters must
//! not silently disable them.
//------------------------------------------------------------------------------------------------
enum AFM_EToggle
{
	DEFAULT,	// Leave the world's own setting alone
	OFF,
	ON
}

//------------------------------------------------------------------------------------------------
//! Settings a scenario can override for a zone, without opening the world in the editor.
//!
//! Two kinds of field, deliberately mixed. Absolute values are replaced, because "prep is 300 s" reads
//! better than "prep is 0.6 of whatever the world says". Difficulty is scaled, because it has to reach
//! every spawner without naming them. Unset means "keep what the zone was authored with": -1 for the
//! absolute values, 0 for the multipliers, DEFAULT for the toggles - the same convention as
//! SCR_MissionHeaderCampaign.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class AFM_DiDPhaseSettings
{
	[Attribute("-1", UIWidgets.EditBox, "Seconds to prepare before the attack (-1 = keep the zone's own)", category: "Timings")]
	int m_iPrepareTimeSeconds;

	[Attribute("-1", UIWidgets.EditBox, "Seconds the zone must be held (-1 = keep the zone's own)", category: "Timings")]
	int m_iDefenseTimeSeconds;

	[Attribute("-1", UIWidgets.EditBox, "Contested seconds before the zone is lost, 0 disables it (-1 = keep the zone's own)", category: "Timings")]
	int m_iFailureTimeSeconds;

	[Attribute("-1", UIWidgets.EditBox, "Attacker tickets per connected player (-1 = keep the zone's own)", category: "Tickets")]
	int m_iTicketsPerPlayer;

	[Attribute("0", UIWidgets.EditBox, "Scales the ticket pool for difficulty (0 = keep the zone's own)", category: "Tickets")]
	float m_fTicketMultiplier;

	[Attribute("-1", UIWidgets.EditBox, "Smallest ticket pool (-1 = keep the zone's own)", category: "Tickets")]
	int m_iMinTickets;

	[Attribute("-1", UIWidgets.EditBox, "Largest ticket pool (-1 = keep the zone's own)", category: "Tickets")]
	int m_iMaxTickets;

	[Attribute("-1", UIWidgets.EditBox, "Max attackers alive across the zone, 0 for no zone-wide limit (-1 = keep the zone's own)", category: "Tickets")]
	int m_iMaxAICount;

	[Attribute("0", UIWidgets.ComboBox, "Enemy helicopter sorties", enums: ParamEnumArray.FromEnum(AFM_EToggle), category: "Spawners")]
	AFM_EToggle m_eHelicopters;

	[Attribute("0", UIWidgets.ComboBox, "Enemy vehicles", enums: ParamEnumArray.FromEnum(AFM_EToggle), category: "Spawners")]
	AFM_EToggle m_eMechanized;

	[Attribute("0", UIWidgets.ComboBox, "Enemy mortars", enums: ParamEnumArray.FromEnum(AFM_EToggle), category: "Spawners")]
	AFM_EToggle m_eMortars;

	[Attribute("0", UIWidgets.ComboBox, "Dead players returning as attackers", enums: ParamEnumArray.FromEnum(AFM_EToggle), category: "Spawners")]
	AFM_EToggle m_eCowabunga;

	[Attribute("0", UIWidgets.ComboBox, "Infantry re-tasking onto the players instead of holding a waypoint", enums: ParamEnumArray.FromEnum(AFM_EToggle), category: "Spawners")]
	AFM_EToggle m_eInfantryHunting;

	//------------------------------------------------------------------------------------------------
	//! Put every field back to "unset". Needed because this class is also created with new() while
	//! resolving, and attribute defaults only apply to instances the config system builds.
	void Reset()
	{
		m_iPrepareTimeSeconds = -1;
		m_iDefenseTimeSeconds = -1;
		m_iFailureTimeSeconds = -1;
		m_iTicketsPerPlayer = -1;
		m_fTicketMultiplier = 0;
		m_iMinTickets = -1;
		m_iMaxTickets = -1;
		m_iMaxAICount = -1;
		m_eHelicopters = AFM_EToggle.DEFAULT;
		m_eMechanized = AFM_EToggle.DEFAULT;
		m_eMortars = AFM_EToggle.DEFAULT;
		m_eCowabunga = AFM_EToggle.DEFAULT;
		m_eInfantryHunting = AFM_EToggle.DEFAULT;
	}

	//------------------------------------------------------------------------------------------------
	//! Copy across everything the other block actually sets, leaving the rest of this one alone.
	//! Used to layer a per-zone override on top of the default block.
	void Override(AFM_DiDPhaseSettings other)
	{
		if (!other)
			return;

		if (other.m_iPrepareTimeSeconds >= 0)
			m_iPrepareTimeSeconds = other.m_iPrepareTimeSeconds;

		if (other.m_iDefenseTimeSeconds >= 0)
			m_iDefenseTimeSeconds = other.m_iDefenseTimeSeconds;

		if (other.m_iFailureTimeSeconds >= 0)
			m_iFailureTimeSeconds = other.m_iFailureTimeSeconds;

		if (other.m_iTicketsPerPlayer >= 0)
			m_iTicketsPerPlayer = other.m_iTicketsPerPlayer;

		if (other.m_fTicketMultiplier > 0)
			m_fTicketMultiplier = other.m_fTicketMultiplier;

		if (other.m_iMinTickets >= 0)
			m_iMinTickets = other.m_iMinTickets;

		if (other.m_iMaxTickets >= 0)
			m_iMaxTickets = other.m_iMaxTickets;

		if (other.m_iMaxAICount >= 0)
			m_iMaxAICount = other.m_iMaxAICount;

		if (other.m_eHelicopters != AFM_EToggle.DEFAULT)
			m_eHelicopters = other.m_eHelicopters;

		if (other.m_eMechanized != AFM_EToggle.DEFAULT)
			m_eMechanized = other.m_eMechanized;

		if (other.m_eMortars != AFM_EToggle.DEFAULT)
			m_eMortars = other.m_eMortars;

		if (other.m_eCowabunga != AFM_EToggle.DEFAULT)
			m_eCowabunga = other.m_eCowabunga;

		if (other.m_eInfantryHunting != AFM_EToggle.DEFAULT)
			m_eInfantryHunting = other.m_eInfantryHunting;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true unless a toggle explicitly says OFF
	static bool IsEnabled(AFM_EToggle toggle)
	{
		return toggle != AFM_EToggle.OFF;
	}
}

//------------------------------------------------------------------------------------------------
//! The same block, aimed at one zone
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class AFM_DiDPhaseOverride : AFM_DiDPhaseSettings
{
	[Attribute("1", UIWidgets.EditBox, "Zone index this block applies to, 1 is played first", category: "Phase")]
	int m_iZoneIndex;
}
