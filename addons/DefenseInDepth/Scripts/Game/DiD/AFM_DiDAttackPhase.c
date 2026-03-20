//------------------------------------------------------------------------------------------------
//! Attack phase driven by remaining budget ratio.
//! PROBE → ASSAULT → FINAL as the attacker spends down their points pool.
enum EAFMAttackPhase
{
	PROBE,		//! Budget > 75% — infantry probe, test defender positions
	ASSAULT,	//! Budget 75%–25% — all options active, peak pressure
	FINAL		//! Budget < 25% — spend aggressively, bonus to all options
}

//------------------------------------------------------------------------------------------------
//! Snapshot of battlefield conditions built by AFM_DiDAttackerDirector each decision cycle.
//! Passed read-only to AFM_DiDSpawnerComponent.ScoreRequest() and AFM_DiDZoneArtillery scoring.
class AFM_DiDBattlefieldState
{
	int m_iDefenderCount;		//! Alive defenders (blufor)
	int m_iAICountInZone;		//! Enemy AI currently inside the zone boundary
	int m_iTotalActiveAI;		//! Total AI tracked by all spawners (includes outside zone)
	float m_fBudgetRatio;		//! Remaining budget / total (1.0 when no budget system is active)
	float m_fTimeRatio;			//! Remaining time / total defense time (1.0 = just started, 0.0 = expired)
	float m_fDefenderDensity;	//! Alive defender count as a float — used by artillery to decide HE missions
	EAFMAttackPhase m_ePhase;	//! Attack phase derived from budget ratio
	bool m_bIsNight;			//! True during low-visibility conditions — pending day/night API verification
}
