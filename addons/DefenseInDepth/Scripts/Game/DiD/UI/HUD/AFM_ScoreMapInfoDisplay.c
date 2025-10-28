//------------------------------------------------------------------------------------------------
class AFM_ScoreMapInfoDisplay : AFM_ScoreInfoDisplay
{
	protected bool m_bMapOpen;
	
	//------------------------------------------------------------------------------------------------
	override void DisplayStartDraw(IEntity owner)
	{
		super.DisplayStartDraw(owner);
		
		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
	}
	
	//------------------------------------------------------------------------------------------------
	override void DisplayStopDraw(IEntity owner)
	{
		super.DisplayStopDraw(owner);
		
		SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
	}
	
	//------------------------------------------------------------------------------------------------
	override void UpdateHUD()
	{
		m_bPeriodicRefresh = false;
		
		if (!m_wRoot || !m_bInitDone)
			return;
		
		if (m_bMapOpen)
			Show(true);
		
		UpdateHUDValues();
	}
	
	//------------------------------------------------------------------------------------------------
	void OnMapOpen(MapConfiguration config)
	{
		m_bMapOpen = true;
		
		if (SCR_DeployMenuMain.GetDeployMenu() == null)
		{
			Show(true);
			UpdateHUD();
		}
		else
		{
			Show(false);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnMapClose(MapConfiguration config)
	{
		m_bMapOpen = false;
		Show(false);
	}
};