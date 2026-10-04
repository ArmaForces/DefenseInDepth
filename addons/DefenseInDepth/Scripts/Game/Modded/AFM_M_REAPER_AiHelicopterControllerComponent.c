//------------------------------------------------------------------------------------------------
//! Caps how far the AI helicopter engages.
//!
//! The mod builds its engagement range from two private consts, m_fSimMinAttackDistance (180) and
//! m_fSimMaxAttackDistance (360), and attacks anything closer than their sum - a 540 m ceiling that no
//! setter exposes. The consts themselves cannot be reassigned, so the cap is applied in the method that
//! decides whether a target may be attacked at all.
//!
//! REAPER_IsAttackTargetAllowed is the right hook because the mod calls it twice: once when acquiring a
//! target, and again every tick for the target it is already attacking. One override therefore both
//! stops long-range acquisition and breaks off a target that has opened the distance.
//------------------------------------------------------------------------------------------------
modded class REAPER_AiHelicopterControllerComponent
{
	protected float m_fAFM_MaxAttackDistance;	//!< 0 or less leaves the mod's own ceiling alone

	protected bool m_bAFM_HasStrikeArea;
	protected vector m_vAFM_StrikeCenter;
	protected float m_fAFM_StrikeRadius;

	//------------------------------------------------------------------------------------------------
	//! \param meters Furthest the helicopter may engage, or 0 to keep the mod's 540 m
	void AFM_SetMaxAttackDistance(float meters)
	{
		m_fAFM_MaxAttackDistance = meters;
	}

	//------------------------------------------------------------------------------------------------
	//! Point the helicopter at a place: it will only engage targets inside this circle, so the mod's own
	//! selection picks the best target there instead of the best target anywhere.
	//!
	//! It still needs the crew to perceive something at the spot - the mod has no path that fires at
	//! bare ground, every shot comes from a BaseTarget.
	void AFM_SetStrikeArea(vector center, float radius)
	{
		m_vAFM_StrikeCenter = center;
		m_fAFM_StrikeRadius = radius;
		m_bAFM_HasStrikeArea = radius > 0;
	}

	//------------------------------------------------------------------------------------------------
	void AFM_ClearStrikeArea()
	{
		m_bAFM_HasStrikeArea = false;
	}

	//------------------------------------------------------------------------------------------------
	override private bool REAPER_IsAttackTargetAllowed(BaseTarget target)
	{
		if (!super.REAPER_IsAttackTargetAllowed(target))
			return false;

		if (m_fAFM_MaxAttackDistance <= 0 && !m_bAFM_HasStrikeArea)
			return true;

		if (!target || !m_pVehicleOwner)
			return true;

		IEntity targetEntity = target.GetTargetEntity();
		if (!targetEntity)
			return true;

		if (m_bAFM_HasStrikeArea && vector.DistanceXZ(targetEntity.GetOrigin(), m_vAFM_StrikeCenter) > m_fAFM_StrikeRadius)
			return false;

		if (m_fAFM_MaxAttackDistance > 0 && vector.DistanceXZ(m_pVehicleOwner.GetOrigin(), targetEntity.GetOrigin()) > m_fAFM_MaxAttackDistance)
			return false;

		return true;
	}
}
