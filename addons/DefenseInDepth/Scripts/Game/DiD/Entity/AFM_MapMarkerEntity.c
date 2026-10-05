//------------------------------------------------------------------------------------------------
//! A marker on the players' map, shown through the vanilla marker manager.
//!
//! Place the prefab in the world or spawn it on the server. The server registers a vanilla static
//! marker for it, the game mode's SCR_MapMarkerManagerComponent sends that to every client - those
//! who join later included - and deleting the entity takes the marker off the map again.
//! The entity's yaw turns the icon; it stands upright at a yaw of 90.
//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "DiD", description: "Map marker")]
class AFM_MapMarkerEntityClass: GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_MapMarkerEntity: GenericEntity
{
	[Attribute("0", UIWidgets.ComboBox, "Icon", "", ParamEnumArray.FromEnum(SCR_EScenarioFrameworkMarkerCustom), category: "DiD Marker")]
	protected SCR_EScenarioFrameworkMarkerCustom m_eIcon;

	[Attribute("0", UIWidgets.ComboBox, "Colour. The names are the vanilla enum's, not the palette's: RED is dark red, OPFOR red, INDEPENDENT green, GREEN dark green", "", ParamEnumArray.FromEnum(SCR_EScenarioFrameworkMarkerCustomColor), category: "DiD Marker")]
	protected SCR_EScenarioFrameworkMarkerCustomColor m_eColor;

	[Attribute("", UIWidgets.EditBox, "Text shown when the marker is hovered", category: "DiD Marker")]
	protected string m_sText;

	protected static const int ICON_YAW_OFFSET = -90;	//!< The icons are drawn lying on their side

	protected ref SCR_MapMarkerBase m_Marker;

	//------------------------------------------------------------------------------------------------
	//! Server side. Safe to call before or after the marker is on the map.
	void SetText(string text)
	{
		m_sText = text;

		// A vanilla marker cannot be renamed, only replaced
		if (!m_Marker)
			return;

		RemoveMarker();
		CreateMarker();
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateMarker()
	{
		if (m_Marker || !Replication.IsServer())
			return;

		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager)
		{
			PrintFormat("AFM_MapMarkerEntity: The game mode has no SCR_MapMarkerManagerComponent, the marker will not show", level: LogLevel.WARNING);
			return;
		}

		vector origin = GetOrigin();

		// The rotation travels as two unsigned bytes, so a negative angle would arrive as another one
		int rotation = Math.Round(GetYawPitchRoll()[0]) + ICON_YAW_OFFSET;
		while (rotation < 0)
		{
			rotation += 360;
		}

		m_Marker = new SCR_MapMarkerBase();
		m_Marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		m_Marker.SetIconEntry(m_eIcon);
		m_Marker.SetColorEntry(m_eColor);
		m_Marker.SetRotation(rotation);
		m_Marker.SetWorldPos(origin[0], origin[2]);
		m_Marker.SetCustomText(m_sText);
		m_Marker.SetCanBeRemovedByOwner(false);

		manager.InsertStaticMarker(m_Marker, false, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveMarker()
	{
		if (!m_Marker)
			return;

		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (manager)
			manager.RemoveStaticMarker(m_Marker);

		m_Marker = null;
	}

	//------------------------------------------------------------------------------------------------
	void AFM_MapMarkerEntity(IEntitySource src, IEntity parent)
	{
		if (SCR_Global.IsEditMode())
			return;

		// A frame later: the marker manager and replication are up by then, and whoever spawned this
		// at runtime has had the chance to name it first
		GetGame().GetCallqueue().CallLater(CreateMarker);
	}

	//------------------------------------------------------------------------------------------------
	void ~AFM_MapMarkerEntity()
	{
		ArmaReforgerScripted game = GetGame();
		if (game && game.GetCallqueue())
			game.GetCallqueue().Remove(CreateMarker);

		RemoveMarker();
	}
}
