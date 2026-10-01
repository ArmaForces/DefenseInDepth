//------------------------------------------------------------------------------------------------
//! Records what each player spends at an arsenal.
//!
//! The arsenal charges through vanilla's own path, so nothing of ours is involved in the transaction - but
//! SCR_ResourcePlayerControllerInventoryComponent announces every successful purchase on a static invoker,
//! on the authority, carrying the player controller that asked and the cost that was taken. That is exactly
//! what the results page needs, and it means the spending can be followed without touching the purchase.
//!
//! Selling an item back is not announced the same way - there is no refund invoker - so a refund does not
//! come off the figure. That matches how placements are counted: what a player committed stays on the
//! record.
//------------------------------------------------------------------------------------------------
class AFM_DiDArsenalSpending
{
	//------------------------------------------------------------------------------------------------
	//! Authority only: the purchase is processed there, and the stats table lives there
	void Start()
	{
		SCR_ResourcePlayerControllerInventoryComponent.GetOnArsenalItemRequested().Insert(OnArsenalItemBought);
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		SCR_ResourcePlayerControllerInventoryComponent.GetOnArsenalItemRequested().Remove(OnArsenalItemBought);
	}

	//------------------------------------------------------------------------------------------------
	//! The invoker also carries the arsenal's resource component, the item, the storage it went into and the
	//! resource type; what matters here is who paid and how much
	protected void OnArsenalItemBought(SCR_ResourceComponent resourceComponent, ResourceName resourceName, IEntity requester, BaseInventoryStorageComponent storage, EResourceType resourceType, int cost)
	{
		if (resourceType != EResourceType.SUPPLIES || cost <= 0)
			return;

		PlayerController controller = PlayerController.Cast(requester);
		if (!controller)
			return;

		AFM_DiDStatsTracker stats = AFM_DiDSupplies.GetStats();
		if (stats)
			stats.OnArsenalSuppliesSpent(controller.GetPlayerId(), cost);
	}
}
