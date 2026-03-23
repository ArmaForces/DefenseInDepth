//------------------------------------------------------------------------------------------------
//! Game-mode-level config singleton for the AI Commander system.
//! Attach as a script component on the game mode entity in the editor.
//!
//! Runtime-spawned waypoint prefabs:
//!  - m_sMoveWaypointPrefab:     Move-type, used for staging, approach, patrol and sweep orders
//!  - m_sAssaultWaypointPrefab:  Attack-type, spawned at AFM_ZoneAssaultWaypointEntity position
//!  - m_sSuppressWaypointPrefab: Suppress-type, spawned at AFM_VehicleOverwatchEntity position
//!  - m_sArtillerySupportWaypoint: fire mission target marker, spawned by AFM_DiDStageArtillery
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "DiD", description: "AI Commander configuration singleton")]
class AFM_DiDCommanderConfigClass: ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDCommanderConfig: ScriptComponent
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Move waypoint prefab — used for staging, approach, patrol and sweep orders", params: "et", category: "DiD Commander")]
	ResourceName m_sMoveWaypointPrefab;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Attack waypoint prefab — spawned at zone assault marker position for each group's final objective", params: "et", category: "DiD Commander")]
	ResourceName m_sAssaultWaypointPrefab;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Suppress waypoint prefab — spawned at vehicle overwatch marker position for mechanized groups", params: "et", category: "DiD Commander")]
	ResourceName m_sSuppressWaypointPrefab;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Waypoint prefab spawned at the target position during an artillery fire mission", params: "et", category: "DiD Commander")]
	ResourceName m_sArtillerySupportWaypoint;

	//------------------------------------------------------------------------------------------------
	//! Returns the config instance attached to the game mode entity, or null if not configured.
	//------------------------------------------------------------------------------------------------
	static AFM_DiDCommanderConfig GetInstance()
	{
		BaseGameMode gm = GetGame().GetGameMode();
		if (!gm)
			return null;
		return AFM_DiDCommanderConfig.Cast(gm.FindComponent(AFM_DiDCommanderConfig));
	}
}
