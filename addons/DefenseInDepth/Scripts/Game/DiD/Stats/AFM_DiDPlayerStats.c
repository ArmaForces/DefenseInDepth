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

	STRUCTURES_BUILT,	//!< Compositions finished by this player
	BUILD_SECONDS,		//!< Seconds spent in build mode
	ZONES_SURVIVED,		//!< Stages this player reached the end of with a body
	EXTRACTED,			//!< Aboard the helicopter when it flew out

	COUNT				//!< Keep last: the size of every stat record
}

//------------------------------------------------------------------------------------------------
class AFM_DiDPlayerStats
{
	// The player id changes across a reconnect; the identity does not, and is what a result page
	// should fold on so one player's match does not end up split in two
	protected string m_sIdentityId;
	protected string m_sName;

	protected ref array<float> m_aValues = {};

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
