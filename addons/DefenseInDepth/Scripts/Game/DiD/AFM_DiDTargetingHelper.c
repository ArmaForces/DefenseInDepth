//------------------------------------------------------------------------------------------------
//! Shared targeting queries for fire support and air spawners
//------------------------------------------------------------------------------------------------
class AFM_DiDTargetingHelper
{
	//------------------------------------------------------------------------------------------------
	//! True if the character sits in a helicopter (air crew are ignored for zone control and friendly fire)
	static bool IsInHelicopter(notnull SCR_ChimeraCharacter character)
	{
		if (!character.IsInVehicle())
			return false;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.GetCompartmentAccessComponent());
		if (!access)
			return false;

		IEntity vehicle = access.GetVehicle();
		return vehicle && vehicle.FindComponent(VehicleHelicopterSimulation);
	}

	//------------------------------------------------------------------------------------------------
	//! Positions of all living, player-controlled characters of the faction
	static void GetPlayerPositions(notnull SCR_Faction faction, notnull array<vector> outPositions)
	{
		outPositions.Clear();

		array<int> playerIds = {};
		faction.GetPlayersInFaction(playerIds);

		PlayerManager playerManager = GetGame().GetPlayerManager();
		foreach (int playerId : playerIds)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
			if (!character || !IsAlive(character))
				continue;

			outPositions.Insert(character.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Positions of all living AI characters of the faction
	//! \param skipAirCrew Ignore characters sitting in helicopters
	static void GetAIPositions(notnull SCR_Faction faction, notnull array<vector> outPositions, bool skipAirCrew = true)
	{
		outPositions.Clear();

		FactionKey factionKey = faction.GetFactionKey();

		array<AIAgent> agents = {};
		GetGame().GetAIWorld().GetAIAgents(agents);
		foreach (AIAgent agent : agents)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!character || character.GetFactionKey() != factionKey || !IsAlive(character))
				continue;

			if (skipAirCrew && IsInHelicopter(character))
				continue;

			outPositions.Insert(character.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool IsNearAnyPosition(vector pos, notnull array<vector> positions, float radius)
	{
		float radiusSq = radius * radius;
		foreach (vector other : positions)
		{
			float dx = pos[0] - other[0];
			float dz = pos[2] - other[2];
			if (dx * dx + dz * dz <= radiusSq)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Centre of the largest group of positions: the position with the most others within radius,
	//! averaged with those neighbours. Returns vector.Zero for an empty list.
	static vector FindDensestGroupCenter(notnull array<vector> positions, float radius, out int groupSize)
	{
		groupSize = 0;
		if (positions.IsEmpty())
			return vector.Zero;

		float radiusSq = radius * radius;
		int bestIndex = 0;
		int bestCount = -1;
		foreach (int i, vector anchor : positions)
		{
			int count = 0;
			foreach (vector other : positions)
			{
				if (vector.DistanceSqXZ(anchor, other) <= radiusSq)
					count++;
			}

			if (count > bestCount)
			{
				bestCount = count;
				bestIndex = i;
			}
		}

		vector anchorPos = positions[bestIndex];
		vector sum = vector.Zero;
		foreach (vector other : positions)
		{
			if (vector.DistanceSqXZ(anchorPos, other) <= radiusSq)
				sum += other;
		}

		groupSize = bestCount;
		return sum / bestCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Average of the zone polyline points in world space, on the terrain surface
	static vector GetPolylineCenter(notnull PolylineShapeEntity polyline)
	{
		array<vector> points = {};
		polyline.GetPointsPositions(points);

		vector origin = polyline.GetOrigin();
		if (points.IsEmpty())
			return origin;

		vector sum = vector.Zero;
		foreach (vector p : points)
		{
			sum += p;
		}

		vector center = origin + sum / points.Count();
		center[1] = GetGame().GetWorld().GetSurfaceY(center[0], center[2]);
		return center;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsAlive(notnull SCR_ChimeraCharacter character)
	{
		SCR_DamageManagerComponent damageManager = character.GetDamageManager();
		return damageManager && !damageManager.IsDestroyed();
	}
}
