//------------------------------------------------------------------------------------------------
//! Infantry staging area marker. Place as a child of AFM_ApproachEntity (not the zone directly).
//! Infantry groups regroup here before advancing toward the approach point and into the zone.
//! Extends GenericEntity — gamecode reads GetOrigin() and spawns a vanilla Move waypoint at that position.
//------------------------------------------------------------------------------------------------
class AFM_StagingPointEntityClass: GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_StagingPointEntity: GenericEntity
{
}
