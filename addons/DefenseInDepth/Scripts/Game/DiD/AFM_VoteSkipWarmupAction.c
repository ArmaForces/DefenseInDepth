class AFM_VoteSkipWarmupAction: SCR_ScriptedUserAction
{
	override bool CanBeShownScript(IEntity user)
	{		
		if (SCR_PlayerController.GetLocalControlledEntity() != user)
			return false;

		int userId = SCR_PlayerController.GetLocalPlayerId();
		
		return SCR_Global.IsAdmin(userId);
	}
	
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		AFM_GameModeDiD gamemode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gamemode)
			return;
		
		gamemode.ForceEndPrepareStage();
	}
}