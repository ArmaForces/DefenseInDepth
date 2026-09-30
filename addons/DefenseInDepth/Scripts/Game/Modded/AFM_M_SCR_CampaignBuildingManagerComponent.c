//------------------------------------------------------------------------------------------------
//! Lets the refund percentage come from the supply config.
//!
//! Vanilla reads m_iCompositionRefundPercentage straight out of the field when a composition is
//! dismantled and offers only a getter, so overriding the getter would not change the refund itself.
//! Writing the field once at startup does, and keeps the number in the config file with the rest of the
//! economy rather than buried in the game mode prefab.
//------------------------------------------------------------------------------------------------
modded class SCR_CampaignBuildingManagerComponent
{
	//------------------------------------------------------------------------------------------------
	void AFM_SetCompositionRefundPercentage(int percentage)
	{
		m_iCompositionRefundPercentage = Math.ClampInt(percentage, 0, 100);
	}
}
