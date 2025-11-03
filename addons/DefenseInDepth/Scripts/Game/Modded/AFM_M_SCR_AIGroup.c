modded class SCR_AIGroup : ChimeraAIGroup
{
	override void EOnInit(IEntity owner)
	{
		if (SCR_Global.IsEditMode())
			return;
		super.EOnInit(owner);
	}
}