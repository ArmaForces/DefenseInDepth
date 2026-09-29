//------------------------------------------------------------------------------------------------
//! Per-match player stats, kept on the authority.
//!
//! Everything it needs arrives in one event: SCR_BaseGameMode.OnControllableDestroyedEx fires for every
//! controllable that dies, AI included, and the context says who killed whom, whether each was a player
//! or AI, and how the two were related. There is no per-character wiring here as a result.
//------------------------------------------------------------------------------------------------
class AFM_DiDStatsTracker
{
	protected ref map<int, ref AFM_DiDPlayerStats> m_mStats = new map<int, ref AFM_DiDPlayerStats>();

	//------------------------------------------------------------------------------------------------
	//! Count one death. Called for every controllable, so most of the work is deciding what to ignore.
	void OnControllableDestroyed(notnull SCR_InstigatorContextData context)
	{
		int killerId = context.GetKillerPlayerID();
		int victimId = context.GetVictimPlayerID();

		// A player died: their own column, whoever did it
		if (victimId > 0)
		{
			AFM_DiDPlayerStats victim = GetOrCreate(victimId);
			if (victim)
			{
				victim.Add(AFM_EDiDStat.DEATHS);

				if (context.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.SUICIDE))
					victim.Add(AFM_EDiDStat.SUICIDES);
			}
		}

		// Everything below is about the killer, so AI killing AI - most of what happens in a match -
		// stops here
		if (killerId <= 0 || killerId == victimId)
			return;

		AFM_DiDPlayerStats killer = GetOrCreate(killerId);
		if (!killer)
			return;

		if (context.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.KILLED_BY_FRIENDLY_PLAYER))
		{
			killer.Add(AFM_EDiDStat.FRIENDLY_KILLS);
			return;
		}

		// Possessed AI and GM kills count as the player's: they are still that player shooting
		if (context.HasAnyVictimCharacterControlType(SCR_ECharacterControlType.AI | SCR_ECharacterControlType.POSSESSED_AI))
			killer.Add(AFM_EDiDStat.BOT_KILLS);
		else if (context.HasAnyVictimCharacterControlType(SCR_ECharacterControlType.PLAYER))
			killer.Add(AFM_EDiDStat.PLAYER_KILLS);
	}

	//------------------------------------------------------------------------------------------------
	//! \return the player's record, created on first sight with the name they connected under
	AFM_DiDPlayerStats GetOrCreate(int playerId)
	{
		AFM_DiDPlayerStats stats;
		if (m_mStats.Find(playerId, stats))
			return stats;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return null;

		string name = playerManager.GetPlayerName(playerId);
		string identityId = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);

		stats = new AFM_DiDPlayerStats(identityId, name);
		m_mStats.Set(playerId, stats);
		return stats;
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when nobody has done anything worth recording
	bool GetStats(notnull array<int> outPlayerIds, notnull array<AFM_DiDPlayerStats> outStats)
	{
		outPlayerIds.Clear();
		outStats.Clear();

		foreach (int playerId, AFM_DiDPlayerStats stats : m_mStats)
		{
			if (!stats)
				continue;

			outPlayerIds.Insert(playerId);
			outStats.Insert(stats);
		}

		return !outStats.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Write the table to the log, best bot killer first. Stands in for the results page until there is
	//! one, and is the way to tell whether the numbers are believable after a match.
	void Dump(string reason)
	{
		array<int> playerIds = {};
		array<AFM_DiDPlayerStats> stats = {};

		if (!GetStats(playerIds, stats))
		{
			PrintFormat("AFM_DiDStatsTracker: No player stats to report (%1)", reason);
			return;
		}

		PrintFormat("AFM_DiDStatsTracker: Player stats (%1) - %2 players", reason, stats.Count());
		PrintFormat("AFM_DiDStatsTracker: %1 | %2 | %3 | %4 | %5 | %6",
			"player", "bots", "players", "friendly", "deaths", "suicides");

		// Selection sort on a handful of records: a comparator class would be more machinery than this
		// is worth
		array<AFM_DiDPlayerStats> remaining = {};
		remaining.Copy(stats);

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

			PrintFormat("AFM_DiDStatsTracker: %1 | %2 | %3 | %4 | %5 | %6",
				best.GetName(),
				best.Get(AFM_EDiDStat.BOT_KILLS),
				best.Get(AFM_EDiDStat.PLAYER_KILLS),
				best.Get(AFM_EDiDStat.FRIENDLY_KILLS),
				best.Get(AFM_EDiDStat.DEATHS),
				best.Get(AFM_EDiDStat.SUICIDES));
		}
	}
}
