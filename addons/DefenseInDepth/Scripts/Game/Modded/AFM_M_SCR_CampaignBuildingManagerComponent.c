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

	//------------------------------------------------------------------------------------------------
	//! Makes a composition actually cost what it says it does.
	//!
	//! Charging and refunding do not go through the provider's own GetResourceComponent - the manager
	//! resolves the component itself, from the composition's provider entity - so pointing the provider at
	//! the stage's cache only fixed the figure players see. The figure is read on their machine; the
	//! charge happens on the authority, where nothing had linked the pool, so building was free while the
	//! bar showed a healthy 1600.
	//!
	//! This is the same linking, at the moment of the transaction and on whichever machine performs it.
	//------------------------------------------------------------------------------------------------
	override protected bool GetResourceComponent(IEntity owner, out SCR_ResourceComponent component)
	{
		if (!super.GetResourceComponent(owner, component))
			return false;

		AFM_DiDSupplies.LinkSpender(component);
		return true;
	}
}
