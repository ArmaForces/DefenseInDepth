//------------------------------------------------------------------------------------------------
//! Charges for a composition when its ghost is placed, not when it is finished.
//!
//! SetProviderAndBuilder is the first moment on the authority where a composition is known to be a
//! player's: it runs from OnEntityCreatedServer and is what stamps the builder onto it. Everything earlier
//! - the entity core's budget event - fires while the composition is still being created, when a player's
//! sandbags and the fortifications a mission ships with are indistinguishable.
//!
//! Charging here rather than on completion matters for two reasons players notice. A ghost that costs
//! nothing lets the pool look affordable until the shovelling starts, and a refund on a never-built ghost
//! is free supplies.
//------------------------------------------------------------------------------------------------
modded class SCR_CampaignBuildingPlacingEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void SetProviderAndBuilder(notnull SCR_CampaignBuildingCompositionComponent compositionComponent)
	{
		super.SetProviderAndBuilder(compositionComponent);

		compositionComponent.AFM_ChargeSuppliesOnce();
	}
}
