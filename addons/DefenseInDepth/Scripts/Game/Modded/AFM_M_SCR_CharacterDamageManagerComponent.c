//! Makes player characters immune to damage while the DiD zone is in its prepare (warmup) phase.
//! No enemies are alive during warmup, so this only prevents accidents: falls, fortifications,
//! own explosives, friendly fire and vehicle collisions.
modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		// Let bandages, morphine and regeneration through so players can heal during warmup
		if (!HEALING_DAMAGE_TYPES.Contains(damageContext.damageType) && AFM_IsWarmupProtected())
			return true;

		return super.HijackDamageHandling(damageContext);
	}

	//------------------------------------------------------------------------------------------------
	//! Vehicle collisions and crash ejections knock characters out directly on the resilience hitzone,
	//! which bypasses HijackDamageHandling.
	override void ForceUnconsciousness(float resilienceHealth = 0)
	{
		if (AFM_IsWarmupProtected())
			return;

		super.ForceUnconsciousness(resilienceHealth);
	}

	//------------------------------------------------------------------------------------------------
	protected bool AFM_IsWarmupProtected()
	{
		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gameMode || !gameMode.IsWarmup())
			return false;

		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(GetOwner()) != 0;
	}
}
