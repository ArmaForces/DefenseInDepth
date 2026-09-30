//------------------------------------------------------------------------------------------------
//! Charges the stage's pool for what players build, and pays part of it back when they take it down.
//!
//! Vanilla does this in SCR_CampaignBuildingManagerComponent.OnEntityCoreBudgetUpdated, which returns
//! early unless SCR_GameModeCampaign.GetInstance() is non-null - so in any game mode that is not Conflict
//! the transaction simply never happens, however the resource components are wired. That is why building
//! showed a price, and a pool, and charged nothing.
//!
//! So this listens to the same event the vanilla handler listens to, and does the part that matters here.
//! The budget change is the composition's supply cost: positive when something was placed, negative when
//! it was removed.
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
		if (entityBudget != SUPPLY_BUDGET || budgetChange == 0 || !entity)
			return;

		// Content authored into the world is not on the players' bill. Taking it down still pays out, which
		// is vanilla's own behaviour and gives a use for the sandbags a mission starts with.
		IEntity owner = entity.GetOwner();
		if (owner && owner.IsLoaded() && budgetChange > 0)
			return;

		if (budgetChange > 0)
		{
			Charge(budgetChange);
			return;
		}

		Refund(-budgetChange);
	}

	//------------------------------------------------------------------------------------------------
	protected void Charge(int cost)
	{
		if (AFM_DiDSupplies.Spend(cost))
			return;

		// The pool ran out between the check the player's machine made and this charge. Taking what is
		// left is better than letting it through for free.
		int taken = AFM_DiDSupplies.GetStored();
		AFM_DiDSupplies.Spend(taken);

		PrintFormat("AFM_DiDSupplyBudget: A composition costing %1 was placed with %2 in the pool", cost, taken, level: LogLevel.WARNING);
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
}
