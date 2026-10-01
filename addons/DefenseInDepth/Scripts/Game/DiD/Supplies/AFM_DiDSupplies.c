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
	//! How far a spender is allowed to reach for the stage's cache. Generous on purpose: the cache and the
	//! thing spending from it are both somewhere in the same zone, and the grid drops links that fall out
	//! of a consumer's range on its next sweep.
	protected static const float LINK_RANGE_M = 1000;

	//------------------------------------------------------------------------------------------------
	//! Puts the active stage's supplies into a spender's own consumer queue.
	//!
	//! Vanilla pairs consumers with containers by proximity through the resource grid, which is right for
	//! a Conflict base and no use here: our pool is whichever stage is running. So the container is
	//! registered outright, and the consumer's range widened so the grid's next sweep does not unlink it
	//! again for being too far away.
	//!
	//! The consumer has to be the DEFAULT one. A consumer authored without an identifier is
	//! DEFAULT_STORAGE, and everything that spends supplies - the building budget, the arsenal - looks up
	//! DEFAULT, which is why a pool wired only for storage reads as zero.
	//!
	//! Returns true when the spender can see the stage's supplies.
	//------------------------------------------------------------------------------------------------
	static bool LinkSpender(SCR_ResourceComponent spender)
	{
		if (!spender || !IsEnabled())
			return false;

		SCR_ResourceContainer container = GetActiveContainer();
		if (!container)
			return false;

		// Only the consumer. A generator's queue is what it hands resources out FROM, so registering the
		// pool with one offers it to anything nearby that draws supplies - which is how 1400 of a stage's
		// 1600 disappeared between the fund and the zone ending. Refunds come back through our own accounting
		// instead.
		SCR_ResourceConsumer consumer = spender.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!consumer)
			return false;

		// Only the consumer has a range to widen, and it is the one the grid would otherwise unlink
		if (consumer.GetResourceRange() < LINK_RANGE_M)
			consumer.SetResourceRange(LINK_RANGE_M);

		return LinkInteractor(consumer, container);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool LinkInteractor(notnull SCR_ResourceInteractor interactor, notnull SCR_ResourceContainer container)
	{
		if (interactor.FindContainer(container) != SCR_ResourceContainerQueueBase.INVALID_CONTAINER_INDEX)
			return true;

		return interactor.RegisterContainerForced(container);
	}

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

		// Read back rather than reporting what was asked for: a container can refuse or clamp a value, and
		// anything that quietly drains the pool later shows up as a gap between this line and the one the
		// stage ends with
		PrintFormat("AFM_DiDSupplies: Stage %1 funded with %2 supplies, asked for %3 (%4 its own, %5 carried over, ceiling %6)",
			zone.GetZoneIndex(), Math.Round(container.GetResourceValue()), total, starting, carryOver, config.m_iCacheMaximum);
	}

	//------------------------------------------------------------------------------------------------
	//! Empties a cache that is not in play.
	//!
	//! Spenders find supplies through the resource grid, by range, and every stage's cache sits in the same
	//! world. Leaving a stage that has not started yet holding its prefab's supplies would let an arsenal
	//! or a builder quietly draw from it, so only the stage being played holds anything.
	static void DrainZone(AFM_DiDZoneComponent zone)
	{
		if (!zone || !IsEnabled())
			return;

		SCR_ResourceContainer container = GetContainer(zone);
		if (!container || container.GetResourceValue() <= 0)
			return;

		container.SetResourceValue(0);
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
