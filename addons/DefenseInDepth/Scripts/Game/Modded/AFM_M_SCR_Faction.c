//------------------------------------------------------------------------------------------------
//! The defending side can always save a loadout at an arsenal.
//!
//! Vanilla lets a faction save only when the loadout manager holds an SCR_PlayerArsenalLoadout with its key.
//! The game mode uses the Conflict loadout manager, which has one for US and USSR and nothing else, so FIA
//! and every modded faction (UK, RHS) had the save action hidden and the save refused on the server.
//!
//! The defender is picked per match from the side catalog, so a fixed list in the loadout manager would
//! never keep up. The attacking side stays as vanilla decides: a COWABUNGA player saving there would replace
//! the defender save, and the re-equip on respawn erases a save whose faction does not match the body.
//------------------------------------------------------------------------------------------------
modded class SCR_Faction
{
	//------------------------------------------------------------------------------------------------
	override bool IsCustomLoadoutSupported()
	{
		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return super.IsCustomLoadoutSupported();

		FactionKey defenderKey = gameMode.GetDefenderFactionKey();
		if (!defenderKey.IsEmpty() && GetFactionKey() == defenderKey)
			return true;

		return super.IsCustomLoadoutSupported();
	}
}
