//------------------------------------------------------------------------------------------------
//! Wave-aware spawner component that consumes tickets from wave zone
//! Simply enables ticket system by default - all logic is in base class
//------------------------------------------------------------------------------------------------
class AFM_DiDWaveSpawnerComponentClass: AFM_DiDSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDWaveSpawnerComponent: AFM_DiDSpawnerComponent
{
	[Attribute("-1", UIWidgets.EditBox, "Start spawning AIs only after Nth wave", category: "DiD")]
	protected int m_iMinWaveNumber;
	
	[Attribute("-1", UIWidgets.EditBox, "Stop spawning AIs after Nth wave", category: "DiD")]
	protected int m_iMaxWaveNumber;
	
	override int GetSpawnCountForWave()
	{
		AFM_DiDWaveZoneComponent zone = AFM_DiDWaveZoneComponent.Cast(m_Zone);
		if (!zone || !IsInActiveRange(zone.GetCurrentWave()))
			return 0;
		
		return super.GetSpawnCountForWave();	
	}
	
	override bool IsActive()
	{
		AFM_DiDWaveZoneComponent zone = AFM_DiDWaveZoneComponent.Cast(m_Zone);
		return zone && IsInActiveRange(zone.GetCurrentWave());
	}
	
	bool IsInActiveRange(int value)
	{
		return value >= m_iMinWaveNumber && 
			(m_iMaxWaveNumber < 0 || value <= m_iMaxWaveNumber);
	}
}
