//------------------------------------------------------------------------------------------------
//! Vehicle hull-down overwatch position marker. Place as a child of AFM_ApproachEntity (not the zone directly).
//! Mechanized groups hold this position and provide suppressive fire before the final push into the zone.
//! Extends GenericEntity — gamecode reads GetOrigin() and spawns a vanilla Suppress waypoint at that position.
//! Optional — routes without a VehicleOverwatch entity are infantry-only.
//------------------------------------------------------------------------------------------------
class AFM_VehicleOverwatchEntityClass: GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_VehicleOverwatchEntity: GenericEntity
{
}
