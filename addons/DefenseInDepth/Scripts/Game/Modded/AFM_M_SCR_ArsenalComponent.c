//------------------------------------------------------------------------------------------------
//! Only the defenders pay at an arsenal.
//!
//! Turning supplies on turned them on for everyone, and an arsenal charges whenever its own entity carries a
//! resource component - which the mortar ammo box does. The attacking side has no pool to pay from, so its
//! mortar crew could not draw rounds and the mortars went silent.
//!
//! An arsenal belonging to anyone but the defending side is therefore free. Nothing is lost by it: the
//! attackers' supplies are not a resource anyone plays with, and the only thing the global flag was ever
//! meant to price is what the players themselves take.
//------------------------------------------------------------------------------------------------
modded class SCR_ArsenalComponent
{
	//------------------------------------------------------------------------------------------------
	override bool IsArsenalUsingSupplies()
	{
		if (!AFM_IsDefenderArsenal())
			return false;

		return super.IsArsenalUsingSupplies();
	}

	//------------------------------------------------------------------------------------------------
	//! The zone hands every one of its props the defending faction when a stage starts, so a player's arsenal
	//! carries that key and an arsenal the attackers brought with them does not. An arsenal with no faction at
	//! all is read as not theirs, which keeps anything unassigned free rather than unusable.
	protected bool AFM_IsDefenderArsenal()
	{
		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return true;

		FactionKey defenderKey = gameMode.GetDefenderFactionKey();
		if (defenderKey.IsEmpty())
			return true;

		SCR_Faction faction = GetAssignedFaction();
		if (!faction)
			return false;

		return faction.GetFactionKey() == defenderKey;
	}
}
