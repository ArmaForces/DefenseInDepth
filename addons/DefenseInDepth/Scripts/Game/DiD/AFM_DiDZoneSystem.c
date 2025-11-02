class AFM_DiDZoneSystem: GameSystem
{
	protected ref map <int, AFM_DiDZoneComponent> m_aZones = new map<int, AFM_DiDZoneComponent>();
	
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		outInfo
			.SetAbstract(false)
			.AddPoint(ESystemPoint.FixedFrame);
	}
	
	static AFM_DiDZoneSystem GetInstance()
	{
		World world = GetGame().GetWorld();
		
		if (!world)
			return null;
		
		return AFM_DiDZoneSystem.Cast(world.FindSystem(AFM_DiDZoneSystem));
	}
	
	bool RegisterZone(AFM_DiDZoneComponent zone)
	{
		PrintFormat("Registering zone %1 as %2 stage", zone.GetZoneName(), zone.GetZoneIndex());
		
		if (m_aZones.Get(zone.GetZoneIndex()) != null)
		{
			PrintFormat("Zone %1 is already present", level: LogLevel.ERROR);
			return false;
		}
		
		m_aZones.Set(zone.GetZoneIndex(), zone);
		return true;
	}
}
