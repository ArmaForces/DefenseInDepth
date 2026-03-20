//------------------------------------------------------------------------------------------------
//! Tracks the attacker's points budget for a zone.
//! Budget is spent when AI is spawned. When exhausted and all AI cleared,
//! the zone transitions to FINISHED_REPELLED (defender victory).
//!
//! Usage:
//!   Before spawn:  if (!budget.CanAfford(cost)) return;
//!   After spawn:   budget.Consume(actualAgentCount * costPerUnit);
//------------------------------------------------------------------------------------------------
class AFM_DiDAttackerBudget
{
	protected int m_iTotal;
	protected int m_iRemaining;

	//------------------------------------------------------------------------------------------------
	void AFM_DiDAttackerBudget(int total)
	{
		m_iTotal = total;
		m_iRemaining = total;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true if at least 'cost' points are available
	bool CanAfford(int cost)
	{
		return m_iRemaining >= cost;
	}

	//------------------------------------------------------------------------------------------------
	//! Deduct points. Clamps to zero — never goes negative.
	void Consume(int cost)
	{
		m_iRemaining = Math.Max(0, m_iRemaining - cost);
		PrintFormat("AFM_DiDAttackerBudget: Consumed %1 pts — %2/%3 remaining",
			cost, m_iRemaining, m_iTotal, level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! Add bonus points (e.g. rollover from a failed previous zone)
	void AddBonus(int bonus)
	{
		m_iRemaining += bonus;
		m_iTotal += bonus;
		PrintFormat("AFM_DiDAttackerBudget: +%1 bonus pts — %2/%3 remaining",
			bonus, m_iRemaining, m_iTotal);
	}

	//------------------------------------------------------------------------------------------------
	bool IsExhausted()
	{
		return m_iRemaining <= 0;
	}

	//------------------------------------------------------------------------------------------------
	int GetRemaining()
	{
		return m_iRemaining;
	}

	int GetTotal()
	{
		return m_iTotal;
	}

	//! 0.0 = exhausted, 1.0 = full budget
	float GetRatio()
	{
		if (m_iTotal == 0)
			return 0;
		return m_iRemaining / (float)m_iTotal;
	}
}
