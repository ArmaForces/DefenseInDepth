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
	
	override int GetSpawnCountForWave()
	{
		AFM_DiDWaveZoneComponent zone = AFM_DiDWaveZoneComponent.Cast(m_Zone);
		if (!zone || zone.GetCurrentWave() < m_iMinWaveNumber)
			return 0;
		
		return super.GetSpawnCountForWave();	
	}
}
