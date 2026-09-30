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

	// Build sessions that have not ended yet, by player
	protected ref map<int, WorldTimestamp> m_mBuildStarted = new map<int, WorldTimestamp>();

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
		else
			return;

		CreditWeaponFlavour(killer, context.GetKillerEntity());
	}

	//------------------------------------------------------------------------------------------------
	//! What the killer was holding when it happened.
	//!
	//! An approximation, and knowingly so: the kill context carries no weapon, so this reads the weapon
	//! in hand at the moment of death. Someone who fires a rocket and swaps to a rifle before the victim
	//! finishes dying is credited with the rifle. Fine for an award about rockets to the face; anything
	//! that had to be exact would have to hook damage on every character instead.
	protected void CreditWeaponFlavour(notnull AFM_DiDPlayerStats killer, IEntity killerEntity)
	{
		if (!killerEntity)
			return;

		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(killerEntity.FindComponent(BaseWeaponManagerComponent));
		if (!weaponManager)
			return;

		BaseWeaponComponent weapon = weaponManager.GetCurrentWeapon();
		if (!weapon)
			return;

		EWeaponType weaponType = weapon.GetWeaponType();

		if (weaponType == EWeaponType.WT_ROCKETLAUNCHER)
			killer.Add(AFM_EDiDStat.LAUNCHER_KILLS);
		else if (weaponType == EWeaponType.WT_FRAGGRENADE || weaponType == EWeaponType.WT_GRENADELAUNCHER)
			killer.Add(AFM_EDiDStat.GRENADE_KILLS);
	}

	//------------------------------------------------------------------------------------------------
	//! Reported by a client through AFM_DiDArsenalTimerComponent, which is the only place it is knowable
	void AddArsenalSeconds(int playerId, int seconds)
	{
		if (seconds <= 0)
			return;

		AFM_DiDPlayerStats stats = GetOrCreate(playerId);
		if (stats)
			stats.Add(AFM_EDiDStat.ARSENAL_SECONDS, seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! One composition finished. The builder id is the only thing separating a player's work from the
	//! mission's own, and the building component already carries it.
	void OnStructureBuilt(int playerId)
	{
		AFM_DiDPlayerStats stats = GetOrCreate(playerId);
		if (stats)
			stats.Add(AFM_EDiDStat.STRUCTURES_BUILT);
	}

	//------------------------------------------------------------------------------------------------
	//! Build mode is a stretch of time rather than an event. Both of these are idempotent, because the
	//! provider adds and drops active users through several paths - leaving the mode, dying,
	//! disconnecting - and more than one of them can fire for the same visit.
	void BuildModeEntered(int playerId)
	{
		if (playerId <= 0 || m_mBuildStarted.Contains(playerId))
			return;

		m_mBuildStarted.Set(playerId, GetCurrentTimestamp());
	}

	//------------------------------------------------------------------------------------------------
	void BuildModeLeft(int playerId)
	{
		WorldTimestamp started;
		if (!m_mBuildStarted.Find(playerId, started))
			return;

		m_mBuildStarted.Remove(playerId);

		int seconds = GetCurrentTimestamp().DiffSeconds(started);
		if (seconds <= 0)
			return;

		AFM_DiDPlayerStats stats = GetOrCreate(playerId);
		if (stats)
			stats.Add(AFM_EDiDStat.BUILD_SECONDS, seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Close any session still open, so a player who was building when the match ended keeps their time
	void FlushBuildSessions()
	{
		array<int> builders = {};
		foreach (int playerId, WorldTimestamp started : m_mBuildStarted)
		{
			builders.Insert(playerId);
		}

		foreach (int playerId : builders)
		{
			BuildModeLeft(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Reached the end of a stage with a body
	void OnZoneSurvived(int playerId)
	{
		AFM_DiDPlayerStats stats = GetOrCreate(playerId);
		if (stats)
			stats.Add(AFM_EDiDStat.ZONES_SURVIVED);
	}

	//------------------------------------------------------------------------------------------------
	//! Aboard the helicopter when it flew clear
	void OnExtracted(int playerId)
	{
		AFM_DiDPlayerStats stats = GetOrCreate(playerId);
		if (stats)
			stats.Add(AFM_EDiDStat.EXTRACTED);
	}

	//------------------------------------------------------------------------------------------------
	protected WorldTimestamp GetCurrentTimestamp()
	{
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp();
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

		CaptureRank(playerId, stats);
		return stats;
	}

	//------------------------------------------------------------------------------------------------
	//! The higher of the two ranks a player has, resolved to an insignia.
	//!
	//! Two sources, because neither alone is right. The XP handler on the player controller is where a
	//! promotion during the match lands, and it outlives every body - reading the body alone showed the
	//! rank of a prefab that had been replaced since. The body's own rank component can still be the
	//! higher of the two, because DiD carries a rank forward onto each new body itself.
	//!
	//! An empty result changes nothing, so a player in a vehicle, in the respawn screen or long gone keeps
	//! whatever was last read for them.
	//------------------------------------------------------------------------------------------------
	protected void CaptureRank(int playerId, notnull AFM_DiDPlayerStats stats)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		PlayerController controller = playerManager.GetPlayerController(playerId);
		if (!controller)
			return;

		SCR_ECharacterRank rank = SCR_ECharacterRank.INVALID;

		SCR_PlayerXPHandlerComponent xpHandler = SCR_PlayerXPHandlerComponent.Cast(controller.FindComponent(SCR_PlayerXPHandlerComponent));
		if (xpHandler)
			rank = xpHandler.GetPlayerRankByXP();

		// INVALID is the last value of the enum rather than the lowest, so it has to be kept out of the
		// comparison rather than just losing it
		SCR_ECharacterRank bodyRank = SCR_CharacterRankComponent.GetCharacterRank(controller.GetControlledEntity());
		if (bodyRank != SCR_ECharacterRank.INVALID)
		{
			if (rank == SCR_ECharacterRank.INVALID || bodyRank > rank)
				rank = bodyRank;
		}

		if (rank == SCR_ECharacterRank.INVALID)
			return;

		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager)
			return;

		SCR_RankContainer ranks = factionManager.GetFactionRanks(playerId);
		if (!ranks)
			return;

		stats.SetRankInsignia(ranks.GetRankInsignia(rank));
	}

	//------------------------------------------------------------------------------------------------
	//! One player's line, in the order the header names them
	protected string BuildRow(notnull AFM_DiDPlayerStats stats)
	{
		string row = stats.GetName();

		row = row + " | " + stats.GetText(AFM_EDiDStat.BOT_KILLS);
		row = row + " | " + stats.GetText(AFM_EDiDStat.PLAYER_KILLS);
		row = row + " | " + stats.GetText(AFM_EDiDStat.LAUNCHER_KILLS);
		row = row + " | " + stats.GetText(AFM_EDiDStat.GRENADE_KILLS);
		row = row + " | " + stats.GetText(AFM_EDiDStat.FRIENDLY_KILLS);
		row = row + " | " + stats.GetText(AFM_EDiDStat.DEATHS);
		row = row + " | " + stats.GetText(AFM_EDiDStat.SUICIDES);
		row = row + " | " + stats.GetText(AFM_EDiDStat.STRUCTURES_BUILT);
		row = row + " | " + stats.GetText(AFM_EDiDStat.BUILD_SECONDS);
		row = row + " | " + stats.GetText(AFM_EDiDStat.ARSENAL_SECONDS);
		row = row + " | " + stats.GetText(AFM_EDiDStat.ZONES_SURVIVED);
		row = row + " | " + stats.GetText(AFM_EDiDStat.EXTRACTED);

		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns false when nobody has done anything worth recording
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
	//! Everything the results page needs, resolved once at the end of the match.
	//!
	//! The rows are the live records rather than copies - the match is over, nothing else will touch
	//! them - and the titles are written onto those same rows as well as collected into their own list,
	//! so the page can show them either against a player or as a roll of honour.
	//------------------------------------------------------------------------------------------------
	AFM_DiDMatchResults BuildResults(AFM_DiDAwardConfig config)
	{
		FlushBuildSessions();

		AFM_DiDMatchResults results = new AFM_DiDMatchResults();

		array<int> playerIds = {};
		array<AFM_DiDPlayerStats> stats = {};
		if (!GetStats(playerIds, stats))
			return results;

		// Ranks last: a promotion during the match should show, and everyone still connected has a body
		// to read one from right now
		for (int i = 0; i < stats.Count(); i++)
		{
			CaptureRank(playerIds[i], stats[i]);
			results.AddRow(stats[i]);
		}

		if (config)
			ResolveAwards(config, results);

		return results;
	}

	//------------------------------------------------------------------------------------------------
	protected void ResolveAwards(notnull AFM_DiDAwardConfig config, notnull AFM_DiDMatchResults results)
	{
		array<ref AFM_DiDAwardEntry> awards = config.GetAwards();
		if (!awards)
			return;

		foreach (AFM_DiDAwardEntry award : awards)
		{
			if (!award || award.m_sTitle.IsEmpty())
				continue;

			// COUNT is the size of a record, not a stat anyone can win
			if (award.m_eStat == AFM_EDiDStat.COUNT)
			{
				PrintFormat("AFM_DiDStatsTracker: Award '%1' names no real stat and was skipped", award.m_sTitle, level: LogLevel.WARNING);
				continue;
			}

			ResolveAward(award, results);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One title. Everyone below the minimum is out of the running, which is what keeps a match where
	//! nobody fired a rocket from crowning someone with none.
	protected void ResolveAward(notnull AFM_DiDAwardEntry award, notnull AFM_DiDMatchResults results)
	{
		array<AFM_DiDPlayerStats> winners = {};
		float best = 0;

		foreach (AFM_DiDPlayerStats row : results.GetRows())
		{
			float value = row.Get(award.m_eStat);
			if (value < award.m_fMinimum)
				continue;

			if (winners.IsEmpty())
			{
				best = value;
				winners.Insert(row);
				continue;
			}

			if (value == best)
			{
				winners.Insert(row);
				continue;
			}

			bool better = value > best;
			if (!award.m_bHighestWins)
				better = value < best;

			if (!better)
				continue;

			best = value;
			winners.Clear();
			winners.Insert(row);
		}

		if (winners.IsEmpty())
			return;

		if (winners.Count() > 1 && !award.m_bAwardTies)
			return;

		array<string> names = {};
		foreach (AFM_DiDPlayerStats winner : winners)
		{
			winner.AddTitle(award.m_sTitle);
			names.Insert(winner.GetName());
		}

		results.AddAward(award.m_sTitle, string.Join(", ", names, true), FormatValue(best, award.m_sUnit));
	}

	//------------------------------------------------------------------------------------------------
	protected string FormatValue(float value, string unit)
	{
		string text = Math.Round(value).ToString();
		if (unit.IsEmpty())
			return text;

		return text + " " + unit;
	}

	//------------------------------------------------------------------------------------------------
	//! Write the table to the log, best bot killer first. Stands in for the results page until there is
	//! one, and is the way to tell whether the numbers are believable after a match.
	void Dump(string reason)
	{
		FlushBuildSessions();

		array<int> playerIds = {};
		array<AFM_DiDPlayerStats> stats = {};

		if (!GetStats(playerIds, stats))
		{
			PrintFormat("AFM_DiDStatsTracker: No player stats to report (%1)", reason);
			return;
		}

		PrintFormat("AFM_DiDStatsTracker: Player stats (%1) - %2 players", reason, stats.Count());
		PrintFormat("AFM_DiDStatsTracker: player | bots | players | rockets | grenades | friendly | deaths | suicides | built | build s | arsenal s | zones | out");

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

			PrintFormat("AFM_DiDStatsTracker: %1", BuildRow(best));
		}
	}
}
