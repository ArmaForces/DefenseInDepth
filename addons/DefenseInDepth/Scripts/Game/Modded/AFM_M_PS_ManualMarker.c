//------------------------------------------------------------------------------------------------
//! Lets a marker be named at runtime, on every machine.
//!
//! PS_ManualMarker reads m_sDescription when it builds its map widget, and that field is a plain
//! attribute baked into the prefab, so a server-side change never reaches the clients. The name is
//! kept in a replicated property instead: clients copy it into m_sDescription when it arrives, which
//! also covers players who join after the marker was created.
//------------------------------------------------------------------------------------------------
modded class PS_ManualMarker
{
	[RplProp(onRplName: "AFM_OnDescriptionChanged")]
	protected string m_sAFM_Description;

	//------------------------------------------------------------------------------------------------
	//! Server side. Safe to call before or after the marker has replicated.
	void AFM_SetDescription(string description)
	{
		m_sAFM_Description = description;
		AFM_ApplyDescription();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_OnDescriptionChanged()
	{
		AFM_ApplyDescription();
	}

	//------------------------------------------------------------------------------------------------
	//! The widget only picks the description up when it is created, so update it directly when the
	//! marker is already on an open map
	protected void AFM_ApplyDescription()
	{
		m_sDescription = m_sAFM_Description;

		if (m_hManualMarkerComponent)
			m_hManualMarkerComponent.SetDescription(m_sDescription);
	}
}
