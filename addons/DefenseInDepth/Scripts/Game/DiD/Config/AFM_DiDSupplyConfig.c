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

	[Attribute("50", UIWidgets.Slider, desc: "What dismantling a finished composition gives back. At 100 players rearrange endlessly for free and placement carries no weight. A ghost nobody built always refunds in full", params: "0 100 5", category: "Building")]
	int m_iCompositionRefundPercentage;

	[Attribute("2", UIWidgets.EditBox, desc: "Supplies for killing an attacking soldier. A busy stage is 150-250 kills", category: "Income")]
	int m_iRewardPerInfantryKill;

	[Attribute("50", UIWidgets.EditBox, desc: "Supplies for destroying an attacking vehicle", category: "Income")]
	int m_iRewardPerVehicleKill;

	[Attribute("25", UIWidgets.EditBox, desc: "Supplies per minute the stage is held. Stops while attackers hold the zone, so pushing them out pays", category: "Income")]
	int m_iRewardPerMinuteHeld;

	[Attribute("250", UIWidgets.EditBox, desc: "Supplies for shooting the attack helicopter down. One AA emplacement paying for itself", category: "Income")]
	int m_iRewardPerHelicopterKill;

	[Attribute("150", UIWidgets.EditBox, desc: "Supplies for destroying a mortar team, which is a reason to go looking for one", category: "Income")]
	int m_iRewardPerMortarKill;

	[Attribute("150", UIWidgets.EditBox, desc: "Supplies for clearing a wave. Wave zones only", category: "Income")]
	int m_iRewardPerWaveCleared;

	[Attribute("60", UIWidgets.EditBox, desc: "What a single soldier costs at the service point. The game ships no price for these - the catalog's own field is not settable in this version - so these are ours", category: "Spawning")]
	int m_iCostPerCharacter;

	[Attribute("60", UIWidgets.EditBox, desc: "What each member of a bought group costs, so a bigger group costs more", category: "Spawning")]
	int m_iCostPerGroupMember;

	[Attribute("200", UIWidgets.EditBox, desc: "What a vehicle costs at the service point", category: "Spawning")]
	int m_iCostPerVehicle;

	[Attribute("600", UIWidgets.EditBox, desc: "Most one stage can earn from kills, so a long grind cannot fund everything. Counts soldiers and vehicles; the helicopter, the mortar and waves are outside it. 0 for no cap", category: "Income")]
	int m_iKillIncomeCapPerZone;

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
