//------------------------------------------------------------------------------------------------
//! Hands the groups a service point garrisons the position with to the stage that paid for them.
//!
//! Same reasoning as the catalog spawner: a defending group belongs to the stage it was bought in, and
//! left standing it wanders into the next one with no orders. The zone clears them when the stage ends.
//------------------------------------------------------------------------------------------------
modded class SCR_DefenderSpawnerComponent
{
	//------------------------------------------------------------------------------------------------
	protected override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		if (!AFM_DiDSupplies.IsEnabled())
			return;

		GetOnDefenderGroupSpawned().Insert(AFM_OnGroupSpawned);
	}

	//------------------------------------------------------------------------------------------------
	//! Same as the catalog spawner: the charge runs on the authority, so the pool is registered there
	override void AddSupplies(float value)
	{
		if (AFM_DiDSupplies.LinkSpender(m_ResourceComponent))
		{
			super.AddSupplies(value);
			return;
		}

		if (value < 0)
			AFM_DiDSupplies.Spend(Math.Round(-value));
		else
			AFM_DiDSupplies.Award(Math.Round(value));
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_OnGroupSpawned(SCR_DefenderSpawnerComponent spawner, SCR_AIGroup group)
	{
		if (!group)
			return;

		AFM_DiDZoneSystem zoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!zoneSystem)
			return;

		AFM_DiDZoneComponent zone = zoneSystem.GetActiveZone();
		if (zone)
			zone.RegisterServiceSpawn(group);
	}
}
