//------------------------------------------------------------------------------------------------
//! Charges the stage's pool for what players build, and pays part of it back when they take it down.
//!
//! Vanilla does this in SCR_CampaignBuildingManagerComponent.OnEntityCoreBudgetUpdated, which returns
//! early unless SCR_GameModeCampaign.GetInstance() is non-null - so in any game mode that is not Conflict
//! the transaction simply never happens, however the resource components are wired. That is why building
//! showed a price, and a pool, and charged nothing.
//!
//! So this listens to the same event the vanilla handler listens to, and pays back part of the cost when a
//! composition is removed. Charging for one is not done here: the event fires while the composition is
//! being created, before SCR_CampaignBuildingPlacingEditorComponent.OnEntityCreatedServer has said who
//! built it, so there is no way to tell a player's sandbags from the fortifications a mission ships with -
//! which is how a stage found itself billed for its own headquarters. The charge is taken in
//! SCR_CampaignBuildingCompositionComponent.SetIsCompositionSpawned instead, where the builder is known.
//------------------------------------------------------------------------------------------------
class AFM_DiDSupplyBudget
{
	//! Which budget carries supply costs. The game mode's building manager is set to the same one.
	protected static const EEditableEntityBudget SUPPLY_BUDGET = EEditableEntityBudget.CAMPAIGN;

	protected SCR_EditableEntityCore m_EntityCore;

	//------------------------------------------------------------------------------------------------
	//! Authority only, like vanilla's own subscription: the placement is processed there
	bool Start()
	{
		m_EntityCore = SCR_EditableEntityCore.Cast(SCR_EditableEntityCore.GetInstance(SCR_EditableEntityCore));
		if (!m_EntityCore)
		{
			Print("AFM_DiDSupplyBudget: No editable entity core, building will not cost supplies", LogLevel.ERROR);
			return false;
		}

		m_EntityCore.Event_OnEntityBudgetUpdatedPerEntity.Insert(OnBudgetUpdated);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		if (m_EntityCore)
			m_EntityCore.Event_OnEntityBudgetUpdatedPerEntity.Remove(OnBudgetUpdated);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBudgetUpdated(EEditableEntityBudget entityBudget, int originalBudgetValue, int budgetChange, int updatedBudgetValue, SCR_EditableEntityComponent entity)
	{
		// Removals only. A placement arrives here too early to tell whose it is.
		if (entityBudget != SUPPLY_BUDGET || budgetChange >= 0 || !entity)
			return;

		IEntity owner = entity.GetOwner();
		if (!owner)
			return;

		// Only what a player paid for pays back. The mission's own fortifications were never charged for,
		// so dismantling them is not a source of supplies. Player ids count from 1, and vanilla's own
		// INVALID_PLAYER_ID is protected inside the composition component, so anything at or below zero is
		// read as nobody.
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(owner.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (!composition || composition.GetBuilderId() <= 0)
			return;

		// A ghost nobody has finished building is a decision not yet acted on, so taking it down costs
		// nothing. The haircut is for tearing down something that was actually built.
		if (!composition.IsCompositionSpawned())
		{
			AFM_DiDSupplies.Award(-budgetChange);
			return;
		}

		Refund(-budgetChange);
	}

	//------------------------------------------------------------------------------------------------
	//! Called from the placement hook, where the builder is known
	static void Charge(int cost)
	{
		if (cost <= 0 || AFM_DiDSupplies.Spend(cost))
			return;

		// The pool ran out between the check the player's machine made and this charge. Taking what is
		// left is better than letting it through for free.
		int taken = AFM_DiDSupplies.GetStored();
		AFM_DiDSupplies.Spend(taken);

		PrintFormat("AFM_DiDSupplyBudget: A composition costing %1 was built with %2 in the pool", cost, taken, level: LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void Refund(int cost)
	{
		AFM_DiDSupplyConfig config = AFM_DiDSupplies.GetConfig();
		if (!config)
			return;

		int refund = Math.Round(cost * Math.ClampInt(config.m_iCompositionRefundPercentage, 0, 100) * 0.01);
		AFM_DiDSupplies.Award(refund);
	}

	//------------------------------------------------------------------------------------------------
	//! What a composition and everything under it costs in supplies
	static int GetSupplyCost(IEntity composition)
	{
		if (!composition)
			return 0;

		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(composition.FindComponent(SCR_EditableEntityComponent));
		if (!editable)
			return 0;

		array<ref SCR_EntityBudgetValue> budgets = {};
		editable.GetEntityAndChildrenBudgetCost(budgets);

		foreach (SCR_EntityBudgetValue budget : budgets)
		{
			if (budget.GetBudgetType() == SUPPLY_BUDGET)
				return budget.GetBudgetValue();
		}

		return 0;
	}
}
