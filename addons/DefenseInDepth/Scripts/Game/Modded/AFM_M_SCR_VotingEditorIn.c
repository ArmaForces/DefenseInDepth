//------------------------------------------------------------------------------------------------
//! Keeps the vanilla Game Master vote out of Defense in Depth.
//!
//! The DiD game mode prefab replaces the voting templates with its own AFM_DiDVotingEditorIn, which only
//! exists while nobody can start the match. If the array were merged with the inherited one instead, a
//! plain vanilla EDITOR_IN template would sit next to it and hand out Game Master at any time. Such a
//! template is failed here on the server. AFM_DiDVotingEditorIn passes through, since the cast succeeds
//! for it, and other vote types are untouched because their type differs.
//------------------------------------------------------------------------------------------------
modded class SCR_VotingEditorIn : SCR_VotingReferendum
{
	protected static bool s_bAFM_WarnedForeignTemplate;

	//------------------------------------------------------------------------------------------------
	override bool Evaluate(out EVotingOutcome outcome)
	{
		if (m_Type == EVotingType.EDITOR_IN && !AFM_DiDVotingEditorIn.Cast(this) && AFM_GameModeDiD.Cast(GetGame().GetGameMode()))
		{
			if (!s_bAFM_WarnedForeignTemplate)
			{
				s_bAFM_WarnedForeignTemplate = true;
				Print("AFM_M_SCR_VotingEditorIn: a vanilla EDITOR_IN template is active, so the DiD prefab's voting templates did not replace vanilla's. Failing it.", LogLevel.WARNING);
			}

			outcome = EVotingOutcome.FORCE_FAIL;
			return true;
		}

		return super.Evaluate(outcome);
	}
}
