//------------------------------------------------------------------------------------------------
//! The supply economy, in one place.
//!
//! Every stage has its own supply cache, and that cache is the team's budget for the stage: vanilla
//! charges composition costs against it through the building provider, and what is left over follows the
//! players to the next stage. This class owns the seeding, the carry-over and the spending, so the zone
//! system and the spawners can ask for a number without knowing where it lives.
//!
//! Whether any of it applies is vanilla's own global supply flag, which the game mode sets from its
//! attribute at startup. With supplies disabled, composition budgets resolve to unlimited and every
//! method here is a no-op - the same state the mode shipped in before this existed.
//------------------------------------------------------------------------------------------------
class AFM_DiDSupplies
{
	//------------------------------------------------------------------------------------------------
	//! Vanilla's flag rather than one of ours: it is replicated, admins can flip it, and every vanilla
	//! consumer of supplies already honours it
	static bool IsEnabled()
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return false;

		return gameMode.IsResourceTypeEnabled(EResourceType.SUPPLIES);
	}

	//------------------------------------------------------------------------------------------------
	static AFM_DiDSupplyConfig GetConfig()
	{
		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return null;

		return gameMode.GetSupplyConfig();
	}

	//------------------------------------------------------------------------------------------------
	//! Set a stage's pool as it begins: its own starting value plus whatever the last stage left behind,
	//! capped at the configured ceiling
	static void SeedZone(AFM_DiDZoneComponent zone, int carryOver)
	{
		if (!zone || !IsEnabled())
			return;

		AFM_DiDSupplyConfig config = GetConfig();
		if (!config)
			return;

		SCR_ResourceContainer container = GetContainer(zone);
		if (!container)
		{
			PrintFormat("AFM_DiDSupplies: Zone %1 has no supply cache, it cannot be funded", zone.GetZoneIndex(), level: LogLevel.WARNING);
			return;
		}

		// Raised first: the current value cannot be set above the maximum, and the prefab's ceiling is the
		// starting value of a Conflict cache rather than a whole stage's budget
		container.SetMaxResourceValue(config.m_iCacheMaximum);

		int starting = config.GetStartingSupplies(zone.GetZoneIndex());
		int total = Math.ClampInt(starting + carryOver, 0, config.m_iCacheMaximum);

		container.SetResourceValue(total);

		PrintFormat("AFM_DiDSupplies: Stage %1 funded with %2 supplies (%3 its own, %4 carried over, ceiling %5)",
			zone.GetZoneIndex(), total, starting, carryOver, config.m_iCacheMaximum);
	}

	//------------------------------------------------------------------------------------------------
	//! What a stage that is ending hands to the next one. Empties the old cache, so a zone the players
	//! walk back into is not still holding a budget.
	static int TakeCarryOver(AFM_DiDZoneComponent zone)
	{
		if (!zone || !IsEnabled())
			return 0;

		AFM_DiDSupplyConfig config = GetConfig();
		if (!config)
			return 0;

		SCR_ResourceContainer container = GetContainer(zone);
		if (!container)
			return 0;

		int remaining = Math.Round(container.GetResourceValue());
		int carryOver = config.GetCarryOver(remaining);

		container.SetResourceValue(0);

		PrintFormat("AFM_DiDSupplies: Stage %1 ended with %2 supplies, %3 carried over", zone.GetZoneIndex(), remaining, carryOver);
		return carryOver;
	}

	//------------------------------------------------------------------------------------------------
	//! \return what the active stage has left, or 0 when there is nothing to spend from
	static int GetStored()
	{
		SCR_ResourceContainer container = GetActiveContainer();
		if (!container)
			return 0;

		return Math.Round(container.GetResourceValue());
	}

	//------------------------------------------------------------------------------------------------
	//! Authority side. Adds to the active stage's pool, clamped by the container's own maximum.
	static void Award(int amount)
	{
		if (amount <= 0 || !IsEnabled())
			return;

		SCR_ResourceContainer container = GetActiveContainer();
		if (!container)
			return;

		container.SetResourceValue(container.GetResourceValue() + amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Authority side. Takes from the active stage's pool.
	//! \return false when there was not enough, in which case nothing is taken
	static bool Spend(int amount)
	{
		if (amount <= 0)
			return true;

		if (!IsEnabled())
			return true;

		SCR_ResourceContainer container = GetActiveContainer();
		if (!container)
			return false;

		if (container.GetResourceValue() < amount)
			return false;

		return container.SetResourceValue(container.GetResourceValue() - amount);
	}

	//------------------------------------------------------------------------------------------------
	//! The cache of the stage being played, which is the pool everything spends from
	static SCR_ResourceComponent GetActiveCache()
	{
		AFM_DiDZoneSystem zoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!zoneSystem)
			return null;

		AFM_DiDZoneComponent zone = zoneSystem.GetActiveZone();
		if (!zone)
			return null;

		return zone.GetSupplyCache();
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_ResourceContainer GetActiveContainer()
	{
		AFM_DiDZoneSystem zoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!zoneSystem)
			return null;

		return GetContainer(zoneSystem.GetActiveZone());
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_ResourceContainer GetContainer(AFM_DiDZoneComponent zone)
	{
		if (!zone)
			return null;

		SCR_ResourceComponent cache = zone.GetSupplyCache();
		if (!cache)
			return null;

		return cache.GetContainer(EResourceType.SUPPLIES);
	}
}
