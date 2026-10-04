//------------------------------------------------------------------------------------------------
//! Calls the extraction helicopter. Put on any prefab the players can reach in an extraction zone.
//! The zone decides whether the call is accepted, so using this anywhere else does nothing.
//------------------------------------------------------------------------------------------------
class AFM_CallExtractionAction: SCR_ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		AFM_GameModeDiD gamemode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gamemode)
			return;

		gamemode.CallExtraction();
	}
}
