//! Vanilla's renegade handling assumes the Campaign game mode and dereferences
//! SCR_GameModeCampaign.GetInstance() without a null check, so every rank change threw a script
//! exception here and aborted whatever triggered it, including XP awards for kills.
modded class SCR_CharacterRankComponent
{
	override protected void SpecialRankHandling(SCR_ECharacterRank newRank, SCR_ECharacterRank prevRank)
	{
		// Renegades only exist in the Campaign game mode
		if (!SCR_GameModeCampaign.GetInstance())
			return;

		super.SpecialRankHandling(newRank, prevRank);
	}
}
