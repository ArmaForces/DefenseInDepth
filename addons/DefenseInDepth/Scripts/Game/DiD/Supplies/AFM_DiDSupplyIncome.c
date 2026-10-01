//------------------------------------------------------------------------------------------------
//! What the defenders earn during a stage.
//!
//! Income arrives mostly while the fighting is on, when building is hard, so it pays for rebuilding a
//! wrecked position and re-kitting rather than for the initial fortification. That is the point: the
//! starting pool is for planning, this is for reacting.
//!
//! Three rules keep it from becoming a grind. Only a player's kills count, so the attackers cannot fund
//! the defence by shooting each other. Nothing is earned while the attackers hold the zone, which makes
//! the contested state hurt twice and makes pushing them out worth doing. And kills are capped per stage,
//! so a stage that lasts forever cannot pay for everything.
//------------------------------------------------------------------------------------------------
class AFM_DiDSupplyIncome
{
	protected int m_iKillIncomeThisZone;
	protected WorldTimestamp m_LastDripAt;

	//------------------------------------------------------------------------------------------------
	//! A new stage starts with its own allowance and its own clock
	void OnZoneChanged()
	{
		m_iKillIncomeThisZone = 0;
		m_LastDripAt = GetCurrentTimestamp();
	}

	//------------------------------------------------------------------------------------------------
	//! Authority side, from the game mode's own kill hook
	void OnControllableDestroyed(notnull SCR_InstigatorContextData context)
	{
		AFM_DiDSupplyConfig config = AFM_DiDSupplies.GetConfig();
		if (!config || !EarningIsAllowed())
			return;

		// Only a player's doing. AI killing AI is most of what happens in a match, and none of it is
		// anybody's achievement.
		if (context.GetKillerPlayerID() <= 0)
			return;

		if (context.HasAnyVictimKillerRelation(SCR_ECharacterDeathStatusRelations.KILLED_BY_FRIENDLY_PLAYER))
			return;

		int reward = config.m_iRewardPerInfantryKill;
		if (Vehicle.Cast(context.GetVictimEntity()))
			reward = config.m_iRewardPerVehicleKill;

		AwardKillIncome(reward, config, context.GetKillerPlayerID());
	}

	//------------------------------------------------------------------------------------------------
	//! Authority side, once per zone tick. Pays for each whole minute the stage has been held.
	void OnZoneUpdate()
	{
		AFM_DiDSupplyConfig config = AFM_DiDSupplies.GetConfig();
		if (!config || config.m_iRewardPerMinuteHeld <= 0)
			return;

		if (!EarningIsAllowed())
		{
			// The clock stops with the income rather than carrying the contested minutes over
			m_LastDripAt = GetCurrentTimestamp();
			return;
		}

		WorldTimestamp now = GetCurrentTimestamp();
		int minutes = now.DiffSeconds(m_LastDripAt) / 60;
		if (minutes <= 0)
			return;

		m_LastDripAt = now;
		AFM_DiDSupplies.Award(minutes * config.m_iRewardPerMinuteHeld);
	}

	//------------------------------------------------------------------------------------------------
	//! The stage has to be being fought for, and not lost to the attackers at this moment
	protected bool EarningIsAllowed()
	{
		if (!AFM_DiDSupplies.IsEnabled())
			return false;

		AFM_DiDZoneSystem zoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!zoneSystem)
			return false;

		AFM_DiDZoneComponent zone = zoneSystem.GetActiveZone();
		if (!zone || zone.GetZoneState() != EAFMZoneState.ACTIVE)
			return false;

		return !zone.IsContested();
	}

	//------------------------------------------------------------------------------------------------
	protected void AwardKillIncome(int reward, notnull AFM_DiDSupplyConfig config, int killerId)
	{
		if (reward <= 0)
			return;

		if (config.m_iKillIncomeCapPerZone > 0)
		{
			int remaining = config.m_iKillIncomeCapPerZone - m_iKillIncomeThisZone;
			if (remaining <= 0)
				return;

			reward = Math.Min(reward, remaining);
		}

		m_iKillIncomeThisZone = m_iKillIncomeThisZone + reward;
		AFM_DiDSupplies.Award(reward);

		// Credited after the cap, so the figure on the results page is what the team actually received
		AFM_DiDStatsTracker stats = AFM_DiDSupplies.GetStats();
		if (stats)
			stats.OnSuppliesEarned(killerId, reward);
	}

	//------------------------------------------------------------------------------------------------
	protected WorldTimestamp GetCurrentTimestamp()
	{
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp();
	}
}
