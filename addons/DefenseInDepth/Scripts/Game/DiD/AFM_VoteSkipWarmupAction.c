class AFM_VoteSkipWarmupAction: SCR_ScriptedUserAction
{
	override bool CanBeShownScript(IEntity user)
	{		
		if (SCR_PlayerController.GetLocalControlledEntity() != user)
			return false;

		int userId = SCR_PlayerController.GetLocalPlayerId();
		
		return AFM_DiDSetupComponent.IsSetupAdmin(userId);
	}
	
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// CanBeShownScript only hides the action on the client; the authority decides who may use it
		if (!Replication.IsServer())
			return;

		int userId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (!AFM_DiDSetupComponent.IsSetupAdmin(userId))
			return;

		AFM_GameModeDiD gamemode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gamemode)
			return;
		
		gamemode.ForceEndPrepareStage();
	}
}