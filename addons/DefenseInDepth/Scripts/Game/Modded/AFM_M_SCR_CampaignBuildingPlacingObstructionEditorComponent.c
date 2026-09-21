//------------------------------------------------------------------------------------------------
//! Lets player-built compositions be placed touching each other.
//!
//! Vanilla traces a cylinder of the composition's protection radius around every entity of the ghost
//! and blocks placement on any hit. The radius is half the bounding box diagonal, so for a long piece
//! like a sandbag wall it reaches well past the ends and two of them cannot be put end to end.
//!
//! The trace now ignores entities belonging to other player-built compositions. Houses, vehicles,
//! players and terrain still block placement as before.
//------------------------------------------------------------------------------------------------
modded class SCR_CampaignBuildingPlacingObstructionEditorComponent
{
	//! Scales the radius a composition's buried foundation reserves. Vanilla uses the prefab's authored
	//! protection radius, which is sized to the whole building slot rather than to the composition, so a
	//! guard tower keeps everything clear of its stake outline. 0 drops that check and leaves the
	//! composition's own props to be traced normally.
	protected const float AFM_FOUNDATION_RADIUS_FACTOR = 0;

	//------------------------------------------------------------------------------------------------
	//! Used for every entity of the composition whose centre is below terrain level, meaning foundations
	override float GetPredefineProtectionRadius(ResourceName resName)
	{
		return super.GetPredefineProtectionRadius(resName) * AFM_FOUNDATION_RADIUS_FACTOR;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool TraceEntityOnPosition(vector position, notnull BaseWorld world, float safeZoneRadius)
	{
		// Find the ground under the ghost, ignoring anything already built there
		TraceParam trace = new TraceParam();
		trace.Start = position;
		trace.End = position - m_vTraceOffset;
		trace.Flags = TraceFlags.ENTS | TraceFlags.OCEAN | TraceFlags.WORLD;
		float traceCoef = world.TraceMove(trace, AFM_IgnoreBuiltCompositions);
		position[1] = Math.Max(trace.Start[1] - traceCoef * m_vTraceOffset[1] + 0.01, world.GetSurfaceY(position[0], position[2]) + HEIGHT_ABOVE_GROUND_BUFFER);

		if (AFM_TraceCylinderIgnoringCompositions(position + m_vCylinderVectorOffset, safeZoneRadius, m_fCylinderHeight, world))
			return false;

		m_eBlockingReason = ECantBuildNotificationType.BLOCKED;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Same four diagonal traces as TraceCilinderUtil, with a filter the vanilla one cannot take
	//! \return true when the cylinder is clear
	protected bool AFM_TraceCylinderIgnoringCompositions(vector pos, float radius, float height, notnull BaseWorld world)
	{
		float heightHalf = height * 0.5;

		TraceParam trace = new TraceParam();
		trace.Flags = TraceFlags.ENTS;

		vector directions[4];
		directions[0] = Vector(radius, heightHalf, 0);
		directions[1] = Vector(-radius, heightHalf, 0);
		directions[2] = Vector(0, heightHalf, radius);
		directions[3] = Vector(0, heightHalf, -radius);

		for (int i = 0; i < 4; i++)
		{
			trace.Start = pos + directions[i];
			trace.End = pos - directions[i];

			if (world.TraceMove(trace, AFM_IgnoreBuiltCompositions) < 1)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return false for anything that is part of a composition a player has already built
	protected bool AFM_IgnoreBuiltCompositions(IEntity entity)
	{
		if (!entity)
			return true;

		IEntity root = SCR_EntityHelper.GetMainParent(entity, true);
		if (!root)
			root = entity;

		if (root.FindComponent(SCR_CampaignBuildingCompositionComponent))
			return false;

		return true;
	}
}
