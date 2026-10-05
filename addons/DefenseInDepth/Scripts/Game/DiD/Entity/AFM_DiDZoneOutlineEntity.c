//------------------------------------------------------------------------------------------------
//! Draws the outline of a zone on the map.
//!
//! Place as a child of the zone's PolylineShapeEntity. Nothing is replicated: the shape is part of the
//! world, so every machine reads its points and draws the line itself, with the engine's map links -
//! the way Conflict joins its bases. The map sets its layers up anew every time it opens, so the
//! outline is built on open and taken down on close.
//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "DiD", description: "Zone outline on the map")]
class AFM_DiDZoneOutlineEntityClass: GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDZoneOutlineEntity: GenericEntity
{
	[Attribute("1 1 1 1", UIWidgets.ColorPicker, "Colour of the line", category: "DiD Zone Outline")]
	protected ref Color m_Color;

	protected static const float LINE_WIDTH = 3;

	protected ShapeEntity m_Shape;
	protected ref array<ref MapItem> m_aCorners = {};

	//------------------------------------------------------------------------------------------------
	protected void OnMapOpen(MapConfiguration config)
	{
		RemoveOutline();

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !m_Shape)
			return;

		array<vector> points = {};
		m_Shape.GetPointsPositions(points);
		if (points.Count() < 2)
			return;

		vector shapeTransform[4];
		m_Shape.GetWorldTransform(shapeTransform);

		foreach (vector point : points)
		{
			vector position = point.Multiply4(shapeTransform);

			MapItem corner = mapEntity.CreateCustomMapItem();
			corner.SetBaseType(EMapDescriptorType.MDT_RADIO);
			corner.SetPos(position[0], position[2]);

			// A corner is only there to hang the line on: no icon, no text, never merged with its neighbours
			MapDescriptorProps props = corner.GetProps();
			props.SetIconVisible(false);
			props.SetTextVisible(false);
			props.Activate(true);
			props.SetGroupType(EMapDescriptorGroup.MDG_SEPARATE);
			corner.SetProps(props);

			m_aCorners.Insert(corner);
		}

		// An open shape has no line from its last point back to its first
		int cornerCount = m_aCorners.Count();
		int lineCount = cornerCount - 1;
		if (m_Shape.IsClosed())
			lineCount = cornerCount;

		for (int i = 0; i < lineCount; i++)
		{
			MapLink link = m_aCorners[i].LinkTo(m_aCorners[(i + 1) % cornerCount]);
			if (!link)
				continue;

			MapLinkProps linkProps = link.GetMapLinkProps();
			if (!linkProps)
				continue;

			linkProps.SetLineWidth(LINE_WIDTH);
			linkProps.SetLineColor(m_Color);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapClose(MapConfiguration config)
	{
		RemoveOutline();
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveOutline()
	{
		foreach (MapItem corner : m_aCorners)
		{
			corner.ClearLinks();
			corner.Recycle();
		}

		m_aCorners.Clear();
	}

	//------------------------------------------------------------------------------------------------
	void AFM_DiDZoneOutlineEntity(IEntitySource src, IEntity parent)
	{
		// Nobody looks at a map in the editor or on a dedicated server
		if (SCR_Global.IsEditMode() || System.IsConsoleApp())
			return;

		m_Shape = ShapeEntity.Cast(parent);
		if (!m_Shape)
		{
			PrintFormat("AFM_DiDZoneOutlineEntity: Not placed as a child of a shape, there is no outline to draw", level: LogLevel.WARNING);
			return;
		}

		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
	}

	//------------------------------------------------------------------------------------------------
	void ~AFM_DiDZoneOutlineEntity()
	{
		SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);

		RemoveOutline();
	}
}
