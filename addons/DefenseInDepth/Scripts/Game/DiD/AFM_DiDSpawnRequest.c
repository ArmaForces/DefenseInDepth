//------------------------------------------------------------------------------------------------
//! A scored spawn candidate produced by AFM_DiDSpawnerComponent.ScoreRequest().
//! Collected by AFM_DiDAttackerDirector for weighted random selection each decision cycle.
class AFM_DiDSpawnRequest
{
	AFM_DiDSpawnerComponent m_Spawner;		//! The spawner that produced this request
	float m_fScore;							//! Desirability score — higher = more likely selected
	int m_iCost;							//! Estimated budget cost for this spawn

	void AFM_DiDSpawnRequest(AFM_DiDSpawnerComponent spawner, float score, int cost)
	{
		m_Spawner = spawner;
		m_fScore = score;
		m_iCost = cost;
	}
}
