//------------------------------------------------------------------------------------------------
//! What a match records about one player.
//!
//! Deliberately not the vanilla data collector: SCR_PlayerData is a career profile persisted to the
//! backend, its stats are lifetime totals, and it has no notion of a match. This lives and dies with
//! one game.
//------------------------------------------------------------------------------------------------
enum AFM_EDiDStat
{
	BOT_KILLS,			//!< Attacking AI killed
	PLAYER_KILLS,		//!< Enemy players killed, which in this game mode means the COWABUNGA squad
	FRIENDLY_KILLS,		//!< Own side killed, player or AI
	DEATHS,				//!< Times this player's body was destroyed
	SUICIDES,			//!< Deaths this player brought on themselves

	LAUNCHER_KILLS,		//!< Kills with a rocket launcher in hand
	GRENADE_KILLS,		//!< Kills with a grenade or grenade launcher in hand
	ARSENAL_SECONDS,	//!< Seconds with an arsenal open, as reported by that player's client

	STRUCTURES_BUILT,	//!< Compositions finished by this player
	BUILD_SECONDS,		//!< Seconds spent in build mode
	ZONES_SURVIVED,		//!< Stages this player reached the end of with a body
	EXTRACTED,			//!< Aboard the helicopter when it flew out

	COUNT				//!< Keep last: the size of every stat record
}

//------------------------------------------------------------------------------------------------
class AFM_DiDPlayerStats
{
	//! Values travel joined into one string, so that this is not a character any of them can contain
	static const string VALUE_SEPARATOR = "|";

	// The player id changes across a reconnect; the identity does not, and is what a result page
	// should fold on so one player's match does not end up split in two
	protected string m_sIdentityId;
	protected string m_sName;

	protected ref array<float> m_aValues = {};

	// Titles this player took, filled when the awards are resolved at the end of the match
	protected ref array<string> m_aTitles = {};

	//------------------------------------------------------------------------------------------------
	void AFM_DiDPlayerStats(string identityId, string name)
	{
		m_sIdentityId = identityId;
		m_sName = name;

		for (int i = 0; i < AFM_EDiDStat.COUNT; i++)
		{
			m_aValues.Insert(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	void Add(AFM_EDiDStat stat, float amount = 1)
	{
		if (!m_aValues.IsIndexValid(stat))
			return;

		m_aValues[stat] = m_aValues[stat] + amount;
	}

	//------------------------------------------------------------------------------------------------
	float Get(AFM_EDiDStat stat)
	{
		if (!m_aValues.IsIndexValid(stat))
			return 0;

		return m_aValues[stat];
	}

	//------------------------------------------------------------------------------------------------
	//! Every stat here counts whole things, so the log does not need six decimal places of them
	string GetText(AFM_EDiDStat stat)
	{
		return Math.Round(Get(stat)).ToString();
	}

	//------------------------------------------------------------------------------------------------
	void AddTitle(string title)
	{
		if (title.IsEmpty())
			return;

		m_aTitles.Insert(title);
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetTitles()
	{
		return m_aTitles;
	}

	//------------------------------------------------------------------------------------------------
	//! Every value in enum order, as one string. Whole numbers throughout, so this is both the wire
	//! form and readable.
	string EncodeValues()
	{
		array<string> parts = {};
		for (int i = 0; i < AFM_EDiDStat.COUNT; i++)
		{
			parts.Insert(GetText(i));
		}

		return string.Join(VALUE_SEPARATOR, parts, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The other end of EncodeValues. A payload from an older build with fewer stats fills what it can
	//! and leaves the rest at zero.
	void DecodeValues(string encoded)
	{
		array<string> parts = {};
		encoded.Split(VALUE_SEPARATOR, parts, false);

		for (int i = 0; i < AFM_EDiDStat.COUNT; i++)
		{
			if (!parts.IsIndexValid(i))
				return;

			m_aValues[i] = parts[i].ToFloat();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Names are read once when the player is first seen, so a player who has since left still appears
	string GetName()
	{
		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	string GetIdentityId()
	{
		return m_sIdentityId;
	}
}
