//------------------------------------------------------------------------------------------------
//! Adds the Defense in Depth per-phase settings to every mission header, the way ACE Anvil adds its own.
//!
//! Modding the header rather than subclassing it means any scenario can carry these settings, including
//! ones whose .conf was written before the mod existed. A header that leaves them empty changes nothing.
//------------------------------------------------------------------------------------------------
modded class SCR_MissionHeader : MissionHeader
{
	[Attribute(desc: "Defense in Depth: applied to every zone")]
	protected ref AFM_DiDPhaseSettings m_AFM_DiDDefaultPhase;

	[Attribute(desc: "Defense in Depth: per zone, by zone index, layered on top of the default block")]
	protected ref array<ref AFM_DiDPhaseOverride> m_aAFM_DiDPhaseOverrides;

	//------------------------------------------------------------------------------------------------
	//! Hand the settings to AFM_DiDScenarioSettings while this header still holds them
	void AFM_CaptureDiDSettings()
	{
		if (!m_AFM_DiDDefaultPhase && !m_aAFM_DiDPhaseOverrides)
			return;

		AFM_DiDScenarioSettings.Capture(m_AFM_DiDDefaultPhase, m_aAFM_DiDPhaseOverrides, m_sName);
	}
}
