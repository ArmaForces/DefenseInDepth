//------------------------------------------------------------------------------------------------
//! The numbers behind the supply economy: what each stage starts with, what follows the players to the
//! next one, and what dismantling gives back.
//!
//! A file rather than attributes on the game mode, for the same reason the sides are: these want
//! balancing between playtests, and a balance pass should not mean editing a prefab. Read on the
//! authority only - every decision it feeds is made there.
//!
//! Prices themselves are not here. Compositions and arsenal items carry vanilla supply costs already
//! (a bunker is 100, an AA gun 200, an infantry kit 20-60), and those are the numbers the game charges;
//! this config sets how much there is to spend.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomStringTitleField("Supply Config")]
class AFM_DiDSupplyConfig
{
	[Attribute("1600", UIWidgets.EditBox, desc: "Supplies a stage starts with when the list below has no entry for it", category: "Pool")]
	int m_iDefaultStartingSupplies;

	[Attribute("", UIWidgets.EditBox, desc: "Starting supplies per stage, first entry is stage 1. Stages past the end fall back to the default", category: "Pool")]
	ref array<int> m_aStartingSuppliesPerZone;

	[Attribute("3000", UIWidgets.EditBox, desc: "Ceiling for a stage's cache. Above the starting value, so income during the fight has somewhere to go", category: "Pool")]
	int m_iCacheMaximum;

	[Attribute("0.5", UIWidgets.Slider, desc: "How much of what is left over follows the players to the next stage. Holding well should make the next stage easier; 1.0 lets stage 1 fund the whole match", params: "0 1 0.05", category: "Pool")]
	float m_fCarryOverFraction;

	[Attribute("50", UIWidgets.Slider, desc: "What dismantling a composition gives back. At 100 players rearrange endlessly for free and placement carries no weight", params: "0 100 5", category: "Building")]
	int m_iCompositionRefundPercentage;

	//------------------------------------------------------------------------------------------------
	//! \return what stage zoneIndex starts with, counting stages from 1
	int GetStartingSupplies(int zoneIndex)
	{
		int entry = zoneIndex - 1;

		if (m_aStartingSuppliesPerZone && m_aStartingSuppliesPerZone.IsIndexValid(entry))
			return m_aStartingSuppliesPerZone[entry];

		return m_iDefaultStartingSupplies;
	}

	//------------------------------------------------------------------------------------------------
	//! \return what to move from a stage that is ending into the one that follows it
	int GetCarryOver(int remaining)
	{
		if (remaining <= 0)
			return 0;

		return Math.Round(remaining * Math.Clamp(m_fCarryOverFraction, 0, 1));
	}
}
