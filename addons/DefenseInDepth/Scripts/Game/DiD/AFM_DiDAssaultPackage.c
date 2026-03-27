//------------------------------------------------------------------------------------------------
//! Combined-arms assault package built by AFM_DiDAttackerDirector.BuildBestPackage().
//!
//! Coordinates a pre-assault fire mission, an infantry group, and an optional mechanized
//! group so that all three converge on the zone within the same arrival window.
//!
//! m_iTotalCost is computed at construction time as the sum of all component costs.
//! The director checks budget.CanAfford(m_iTotalCost) before executing.
//!
//! Execution order:
//!   1. Artillery fires immediately (if m_Artillery is set)
//!   2. Infantry spawns immediately
//!   3. Mechanized spawns after a delay so it arrives at the same time as infantry
//------------------------------------------------------------------------------------------------
class AFM_DiDAssaultPackage
{
	//! Artillery entity to fire the pre-assault mission. Null = no fire support.
	AFM_DiDStageArtillery m_Artillery;

	//! Round type used for the pre-assault fire mission.
	EAFMRoundType m_ePreAssaultRound;

	//! Infantry spawner — always present; triggers immediately on execution.
	AFM_DiDSpawnerComponent m_InfantrySpawner;

	//! Mechanized spawner — null for infantry-only packages.
	//! Triggered with a staggered delay so vehicle and infantry arrive together.
	AFM_DiDSpawnerComponent m_MechanizedSpawner;

	//! Approach route shared by both spawners.
	AFM_DiDApproachRoute m_Route;

	//! Desired arrival time in director ticks — used to compute mechanized spawn delay.
	float m_fTargetArrivalTicks;

	//! Total budget cost: infantry cost + mechanized cost (+ 0 for artillery, paid separately).
	int m_iTotalCost;

	//! Attack phase at the time the package was built — forwarded to TriggerSpawn on the delayed
	//! mechanized call so SetMaxAutonomousDistance receives the correct value.
	EAFMAttackPhase m_ePhase;

	//! Director aggression at build time — forwarded alongside m_ePhase.
	float m_fAggression;

	//------------------------------------------------------------------------------------------------
	void AFM_DiDAssaultPackage(
		AFM_DiDSpawnerComponent infantrySpawner,
		int infantryCost,
		AFM_DiDApproachRoute route,
		float targetArrivalTicks,
		AFM_DiDSpawnerComponent mechanizedSpawner,
		int mechanizedCost,
		AFM_DiDStageArtillery artillery,
		EAFMRoundType preAssaultRound)
	{
		m_InfantrySpawner = infantrySpawner;
		m_MechanizedSpawner = mechanizedSpawner;
		m_Artillery = artillery;
		m_ePreAssaultRound = preAssaultRound;
		m_Route = route;
		m_fTargetArrivalTicks = targetArrivalTicks;
		m_iTotalCost = infantryCost + mechanizedCost;
	}
}
