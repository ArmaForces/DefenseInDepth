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
		return stats;
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
