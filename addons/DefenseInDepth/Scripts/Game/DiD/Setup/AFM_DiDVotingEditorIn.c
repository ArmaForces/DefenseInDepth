//------------------------------------------------------------------------------------------------
//! The vote that makes a volunteer Game Master when nobody connected can set the match up.
//! Vanilla offers this vote at any time and trusts every client. Here it exists only while the
//! setup is open and nobody (admin, Game Master, listen host) is able to start the match, and the
//! server enforces that itself because vanilla never asks IsAvailable on the server.
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(EVotingType, "m_Type")]
class AFM_DiDVotingEditorIn : SCR_VotingEditorIn
{
	//------------------------------------------------------------------------------------------------
	//! The vote makes sense only while the setup is open and nobody is able to start the match
	protected bool AFM_IsVoteAllowed()
	{
		AFM_DiDSetupComponent setup = AFM_DiDSetupComponent.GetInstance();
		if (!setup || !setup.IsSetupOpen())
			return false;

		return !AFM_DiDSetupComponent.IsAnyoneAbleToStart();
	}

	//------------------------------------------------------------------------------------------------
	override bool IsAvailable(int value, bool isOngoing)
	{
		if (!AFM_IsVoteAllowed())
			return false;

		return super.IsAvailable(value, isOngoing);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla divides two ints here, so the share of votes is 0 or 1 and from two players up the vote
	//! would have to be unanimous. Float division makes the threshold mean what its tooltip says.
	override protected float GetRatio()
	{
		return m_aPlayerIDs.Count() / (float)GetPlayerCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Same int division as in GetRatio, here in the relative participation limit
	override protected bool EvaluateParticipation(int voteCount)
	{
		return voteCount >= Math.Min(m_iMinVotes, GetPlayerCount()) && voteCount / (float)GetPlayerCount() >= m_iMinParticipation;
	}

	//------------------------------------------------------------------------------------------------
	//! Server side enforcement: a running vote fails the moment someone else can start the match, and
	//! also when its author is not the nominee. Vanilla lets only the local player nominate themselves
	//! but checks that on the client alone, so the server has to check it too.
	override bool Evaluate(out EVotingOutcome outcome)
	{
		if (!AFM_IsVoteAllowed() || GetAuthorId() != GetValue())
		{
			outcome = EVotingOutcome.FORCE_FAIL;
			return true;
		}

		return super.Evaluate(outcome);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla dereferences the winner's editor manager without a check, so a winner who just left would crash the grant
	override void OnVotingEnd(int value = DEFAULT_VALUE, int winner = DEFAULT_VALUE)
	{
		if (winner == DEFAULT_VALUE)
			return;

		SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (!core || !core.GetEditorManager(winner))
		{
			Print("AFM_DiDVotingEditorIn: winner " + winner + " has no editor manager, Game Master not granted", LogLevel.WARNING);
			return;
		}

		if (!AFM_IsVoteAllowed())
		{
			Print("AFM_DiDVotingEditorIn: someone can already start the match, Game Master not granted to " + winner, LogLevel.WARNING);
			return;
		}

		super.OnVotingEnd(value, winner);
	}
}
