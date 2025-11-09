//All credits go to Til Weimann and his mission framework

modded class SCR_AICombatComponent : ScriptComponent
{
	protected bool m_bNeverDismountTurret = false;

	override bool DismountTurretCondition(inout vector targetPos, bool targetPosProvided)
	{
		if (m_bNeverDismountTurret)
			return false;
		return super.DismountTurretCondition(targetPos, targetPosProvided);
	}
	
	void SetNeverDismountTurret(bool value)
	{
		m_bNeverDismountTurret = value;
	}
}