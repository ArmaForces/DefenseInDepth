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

	[Attribute(desc: "Defense in Depth: the side the players defend as. Overrides the game mode's own choice, so one game mode prefab serves every pairing", params: "conf class=AFM_DiDSideConfig")]
	protected ResourceName m_sAFM_DiDDefenderConfig;

	[Attribute(desc: "Defense in Depth: the side that attacks. Overrides the game mode's own choice", params: "conf class=AFM_DiDSideConfig")]
	protected ResourceName m_sAFM_DiDAttackerConfig;

	[Attribute("0", UIWidgets.CheckBox, desc: "Defense in Depth: start the match by itself as soon as the first player has joined, with the settings of this header and of the game mode, instead of waiting on the setup screen for an admin to press Start. For servers nobody attends")]
	protected bool m_bAFM_DiDAutoStart;

	//------------------------------------------------------------------------------------------------
	//! Hand the settings to AFM_DiDScenarioSettings while this header still holds them
	void AFM_CaptureDiDSettings()
	{
		if (!m_AFM_DiDDefaultPhase && !m_aAFM_DiDPhaseOverrides
			&& m_sAFM_DiDDefenderConfig.IsEmpty() && m_sAFM_DiDAttackerConfig.IsEmpty()
			&& !m_bAFM_DiDAutoStart)
			return;

		AFM_DiDScenarioSettings.Capture(m_AFM_DiDDefaultPhase, m_aAFM_DiDPhaseOverrides, m_sName,
			m_sAFM_DiDDefenderConfig, m_sAFM_DiDAttackerConfig, m_bAFM_DiDAutoStart);
	}
}
