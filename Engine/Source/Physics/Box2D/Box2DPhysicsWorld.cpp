#include "Box2DPhysicsWorld.h"

#include <box2d/box2d.h>
#include <cfloat>

#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    namespace
    {
        b2Vec2   ToB2(Vector2F InV)   noexcept { return b2Vec2{ InV.x, InV.y }; }
        Vector2F ToVec2(b2Vec2 InV)   noexcept { return Vector2F{ InV.x, InV.y }; }

        b2BodyType ToB2BodyType(EBodyType InType) noexcept
        {
            switch (InType)
            {
                case EBodyType::Static:    return b2_staticBody;
                case EBodyType::Kinematic: return b2_kinematicBody;
                case EBodyType::Dynamic:   return b2_dynamicBody;
            }
            return b2_staticBody;
        }

        // Resolve a shape to its owning entity's packed bits (BodyDesc::UserData). Returns 0 if the
        // shape is stale — end-touch events may reference shapes destroyed since the step.
        Uint64 EntityBitsFromShape(b2ShapeId InShape) noexcept
        {
            if (!b2Shape_IsValid(InShape)) { return 0; }
            const b2BodyId lBody = b2Shape_GetBody(InShape);
            return static_cast<Uint64>(reinterpret_cast<uintptr_t>(b2Body_GetUserData(lBody)));
        }

        // b2OverlapResultFcn for OverlapAABB — context is the out-vector; collect and continue.
        bool OverlapCollect(b2ShapeId InShape, void* InContext)
        {
            auto* lOut = static_cast<TDynArray<Uint64>*>(InContext);
            lOut->push_back(EntityBitsFromShape(InShape));
            return true;
        }

        // Collision-plane sink for b2World_CollideMover. A fixed buffer keeps the per-step
        // work allocation-free; 8 planes is ample for 2D capsule movement (floor + a couple walls).
        constexpr int kMaxMoverPlanes = 8;
        struct MoverPlaneContext
        {
            b2CollisionPlane Planes[kMaxMoverPlanes] = {};
            int              Count                   = 0;
            Uint64           IgnoreUserData          = 0;   // skip the mover's own body
        };

        // b2PlaneResultFcn: turn each hit plane into a rigid b2CollisionPlane (FLT_MAX pushLimit =
        // hard surface, clipVelocity = true) for b2SolvePlanes / b2ClipVector. Skips the mover's own
        // body (now a real kinematic body in the world) via IgnoreUserData.
        bool MoverPlaneFcn(b2ShapeId InShape, const b2PlaneResult* InPlane, void* InContext)
        {
            if (!InPlane->hit) { return true; }

            auto* lCtx = static_cast<MoverPlaneContext*>(InContext);
            if (lCtx->IgnoreUserData != 0 && EntityBitsFromShape(InShape) == lCtx->IgnoreUserData)
            {
                return true;   // self — don't collide with our own capsule body
            }
            if (lCtx->Count < kMaxMoverPlanes)
            {
                lCtx->Planes[lCtx->Count] = { InPlane->plane, FLT_MAX, 0.f, true };
                lCtx->Count += 1;
            }
            return true;
        }
    }

    // =============================================================================
    // CTOR - DTOR
    // =============================================================================
    Box2DPhysicsWorld::Box2DPhysicsWorld(const PhysicsWorldDesc& InDesc)
    {
        // Global tuning: how many world units make a metre (sleep thresholds, margins).
        b2SetLengthUnitsPerMeter(InDesc.LengthUnitsPerMeter);

        b2WorldDef lWorldDef = b2DefaultWorldDef();
        lWorldDef.gravity    = ToB2(InDesc.Gravity);
        m_WorldId            = b2CreateWorld(&lWorldDef);

        OPAAX_CORE_INFO("Box2DPhysicsWorld: created (gravity={},{} units/m={} substeps={})",
                        InDesc.Gravity.x, InDesc.Gravity.y, InDesc.LengthUnitsPerMeter, InDesc.SubStepCount);
    }

    Box2DPhysicsWorld::~Box2DPhysicsWorld()
    {
        b2DestroyWorld(m_WorldId);
        OPAAX_CORE_INFO("Box2DPhysicsWorld: destroyed");
    }

    // =============================================================================
    // IPhysicsWorld Interface
    // =============================================================================
    void Box2DPhysicsWorld::Step(float DeltaTime, int SubStepCount)
    {
        b2World_Step(m_WorldId, DeltaTime, SubStepCount);
    }

    void Box2DPhysicsWorld::SetGravity(Vector2F InGravity)
    {
        b2World_SetGravity(m_WorldId, ToB2(InGravity));
    }

    Vector2F Box2DPhysicsWorld::GetGravity() const
    {
        return ToVec2(b2World_GetGravity(m_WorldId));
    }

    // =============================================================================
    // Bodies + shapes
    // =============================================================================
    BodyHandle Box2DPhysicsWorld::CreateBody(const BodyDesc& InDesc)
    {
        b2BodyDef lDef            = b2DefaultBodyDef();
        lDef.type                 = ToB2BodyType(InDesc.Type);
        lDef.position             = ToB2(InDesc.Position);
        lDef.rotation             = b2MakeRot(InDesc.Rotation);
        lDef.gravityScale         = InDesc.GravityScale;
        lDef.linearDamping        = InDesc.LinearDamping;
        lDef.angularDamping       = InDesc.AngularDamping;
        lDef.motionLocks.angularZ = InDesc.FixedRotation;
        lDef.userData             = reinterpret_cast<void*>(static_cast<uintptr_t>(InDesc.UserData));

        const b2BodyId lId = b2CreateBody(m_WorldId, &lDef);
        return BodyHandle{ b2StoreBodyId(lId) };
    }

    void Box2DPhysicsWorld::DestroyBody(BodyHandle InBody)
    {
        if (!InBody.IsValid()) { return; }

        const b2BodyId lId = b2LoadBodyId(InBody.Id);
        if (b2Body_IsValid(lId))
        {
            b2DestroyBody(lId);
        }
    }

    ShapeHandle Box2DPhysicsWorld::AddShape(BodyHandle InBody, const ShapeDesc& InShape)
    {
        const b2BodyId lBody = b2LoadBodyId(InBody.Id);

        b2ShapeDef lDef           = b2DefaultShapeDef();
        lDef.density              = InShape.Density;
        lDef.material.friction    = InShape.Friction;
        lDef.material.restitution = InShape.Restitution;
        lDef.isSensor             = InShape.IsSensor;
        lDef.filter.categoryBits  = InShape.CategoryBits;
        lDef.filter.maskBits      = InShape.MaskBits;

        // Enable sensor events on every shape: a sensor needs them to report, and a solid
        // visitor needs them to be SEEN by a sensor (Box2D 3.2 requires the visitor to opt in).
        // Solid shapes additionally enable contact events for OnCollisionEnter/Exit.
        lDef.enableSensorEvents  = true;
        lDef.enableContactEvents = !InShape.IsSensor;

        const ShapeGeometry& lGeo = InShape.Geometry;

        b2ShapeId lShape;
        if (lGeo.Type == EColliderShape::Capsule)
        {
            const b2Capsule lCapsule{ ToB2(lGeo.Offset + lGeo.Center1),
                                      ToB2(lGeo.Offset + lGeo.Center2),
                                      lGeo.Radius };
            lShape = b2CreateCapsuleShape(lBody, &lDef, &lCapsule);
        }
        else if (lGeo.Type == EColliderShape::Circle)
        {
            const b2Circle lCircle{ ToB2(lGeo.Offset), lGeo.Radius };
            lShape = b2CreateCircleShape(lBody, &lDef, &lCircle);
        }
        else
        {
            // Geometry carries half-extents directly (the component-side full size is halved upstream).
            const b2Polygon lBox = b2MakeOffsetBox(lGeo.HalfExtents.x, lGeo.HalfExtents.y,
                                                   ToB2(lGeo.Offset), b2MakeRot(0.f));
            lShape = b2CreatePolygonShape(lBody, &lDef, &lBox);
        }

        return ShapeHandle{ b2StoreShapeId(lShape) };
    }

    void Box2DPhysicsWorld::GetBodyTransform(BodyHandle InBody, Vector2F& OutPosition, float& OutRotation) const
    {
        const b2BodyId lId = b2LoadBodyId(InBody.Id);
        OutPosition = ToVec2(b2Body_GetPosition(lId));
        OutRotation = b2Rot_GetAngle(b2Body_GetRotation(lId));
    }

    void Box2DPhysicsWorld::SetBodyTransform(BodyHandle InBody, Vector2F InPosition, float InRotation)
    {
        const b2BodyId lId = b2LoadBodyId(InBody.Id);
        b2Body_SetTransform(lId, ToB2(InPosition), b2MakeRot(InRotation));
    }

    void Box2DPhysicsWorld::SetBodyTargetTransform(BodyHandle InBody, Vector2F InPosition, float InRotation,
                                                   float InDeltaTime)
    {
        const b2BodyId lId = b2LoadBodyId(InBody.Id);
        const b2Transform lTarget{ ToB2(InPosition), b2MakeRot(InRotation) };
        // Moves a kinematic body toward the target over the step (generates contacts); wake = true.
        b2Body_SetTargetTransform(lId, lTarget, InDeltaTime, true);
    }

    // =============================================================================
    // Events (drained after Step)
    // =============================================================================
    void Box2DPhysicsWorld::GetSensorEvents(TDynArray<PhysicsContactPair>& OutBegan,
                                            TDynArray<PhysicsContactPair>& OutEnded)
    {
        OutBegan.clear();
        OutEnded.clear();

        const b2SensorEvents lEvents = b2World_GetSensorEvents(m_WorldId);

        for (int i = 0; i < lEvents.beginCount; ++i)
        {
            const b2SensorBeginTouchEvent& lEvent = lEvents.beginEvents[i];
            OutBegan.push_back({ EntityBitsFromShape(lEvent.sensorShapeId),
                                 EntityBitsFromShape(lEvent.visitorShapeId) });
        }

        for (int i = 0; i < lEvents.endCount; ++i)
        {
            const b2SensorEndTouchEvent& lEvent = lEvents.endEvents[i];
            OutEnded.push_back({ EntityBitsFromShape(lEvent.sensorShapeId),
                                 EntityBitsFromShape(lEvent.visitorShapeId) });
        }
    }

    void Box2DPhysicsWorld::GetContactEvents(TDynArray<PhysicsContactPair>& OutBegan,
                                             TDynArray<PhysicsContactPair>& OutEnded)
    {
        OutBegan.clear();
        OutEnded.clear();

        const b2ContactEvents lEvents = b2World_GetContactEvents(m_WorldId);

        for (int i = 0; i < lEvents.beginCount; ++i)
        {
            const b2ContactBeginTouchEvent& lEvent = lEvents.beginEvents[i];
            OutBegan.push_back({ EntityBitsFromShape(lEvent.shapeIdA),
                                 EntityBitsFromShape(lEvent.shapeIdB) });
        }

        for (int i = 0; i < lEvents.endCount; ++i)
        {
            const b2ContactEndTouchEvent& lEvent = lEvents.endEvents[i];
            OutEnded.push_back({ EntityBitsFromShape(lEvent.shapeIdA),
                                 EntityBitsFromShape(lEvent.shapeIdB) });
        }
    }

    // =============================================================================
    // Queries
    // =============================================================================
    PhysicsRayHit Box2DPhysicsWorld::RayCastClosest(Vector2F Origin, Vector2F Direction, float Distance,
                                                    Uint64 ChannelMask)
    {
        // translation = unit(Direction) * Distance; zero direction => zero-length ray (no hit).
        const b2Vec2 lTranslation = b2MulSV(Distance, b2Normalize(ToB2(Direction)));

        b2QueryFilter lFilter;
        lFilter.categoryBits = ~0ull;        // the query belongs to all categories...
        lFilter.maskBits     = ChannelMask;  // ...and accepts the channels the caller asked for.

        const b2RayResult lResult = b2World_CastRayClosest(m_WorldId, ToB2(Origin), lTranslation, lFilter);

        PhysicsRayHit lHit;
        lHit.bHit     = lResult.hit;
        lHit.Point    = ToVec2(lResult.point);
        lHit.Normal   = ToVec2(lResult.normal);
        lHit.Fraction = lResult.fraction;
        lHit.UserData = lResult.hit ? EntityBitsFromShape(lResult.shapeId) : 0;
        return lHit;
    }

    void Box2DPhysicsWorld::OverlapAABB(Vector2F Min, Vector2F Max, Uint64 ChannelMask,
                                        TDynArray<Uint64>& OutUserData)
    {
        OutUserData.clear();

        b2QueryFilter lFilter;
        lFilter.categoryBits = ~0ull;
        lFilter.maskBits     = ChannelMask;

        const b2AABB lAABB{ ToB2(Min), ToB2(Max) };
        b2World_OverlapAABB(m_WorldId, lAABB, lFilter, &OverlapCollect, &OutUserData);
    }

    // =============================================================================
    // Geometric mover
    // =============================================================================
    MoveCapsuleResult Box2DPhysicsWorld::MoveCapsule(const MoveCapsuleInput& InInput)
    {
        // Mover is a query, not a shape: category ~0 (always queryable), mask = solid channels.
        b2QueryFilter lFilter;
        lFilter.categoryBits = ~0ull;
        lFilter.maskBits     = InInput.ChannelMask;

        b2Vec2       lPos    = ToB2(InInput.Position);
        const b2Vec2 lVel    = ToB2(InInput.Velocity);
        const b2Vec2 lTarget = b2MulAdd(lPos, InInput.DeltaTime, lVel);  // target = pos + dt*velocity

        // Early-out distance — half a world unit; if a solve iteration barely moves, we're settled.
        constexpr float lToleranceSq = 0.5f * 0.5f;
        const int lMaxIter = InInput.MaxIterations > 0 ? InInput.MaxIterations : 1;

        MoverPlaneContext lCtx;
        lCtx.IgnoreUserData = InInput.IgnoreUserData;
        for (int lIter = 0; lIter < lMaxIter; ++lIter)
        {
            lCtx.Count = 0;

            // World-space capsule at the current resolved position.
            b2Capsule lMover;
            lMover.center1 = b2Add(lPos, ToB2(InInput.Capsule.Center1));
            lMover.center2 = b2Add(lPos, ToB2(InInput.Capsule.Center2));
            lMover.radius  = InInput.Capsule.Radius;

            b2World_CollideMover(m_WorldId, &lMover, lFilter, MoverPlaneFcn, &lCtx);
            const b2PlaneSolverResult lSolve = b2SolvePlanes(b2Sub(lTarget, lPos), lCtx.Planes, lCtx.Count);

            // Anti-tunnel: never advance further than a shape cast of the solved translation allows.
            const float  lFraction = b2World_CastMover(m_WorldId, &lMover, lSolve.translation, lFilter);
            const b2Vec2 lDelta    = b2MulSV(lFraction, lSolve.translation);
            lPos = b2Add(lPos, lDelta);

            if (b2LengthSquared(lDelta) < lToleranceSq) { break; }
        }

        // Clip the velocity against the touched planes so the mover stops pushing into walls.
        const b2Vec2 lClipped = b2ClipVector(lVel, lCtx.Planes, lCtx.Count);

        // Grounded = any touched plane whose normal is "up enough" (normal.y >= GroundNormalY).
        bool   lGrounded     = false;
        b2Vec2 lGroundNormal = { 0.f, 0.f };
        for (int i = 0; i < lCtx.Count; ++i)
        {
            if (lCtx.Planes[i].plane.normal.y >= InInput.GroundNormalY)
            {
                lGrounded     = true;
                lGroundNormal = lCtx.Planes[i].plane.normal;
                break;
            }
        }

        MoveCapsuleResult lResult;
        lResult.Position     = ToVec2(lPos);
        lResult.Velocity     = ToVec2(lClipped);
        lResult.Grounded     = lGrounded;
        lResult.GroundNormal = ToVec2(lGroundNormal);
        return lResult;
    }

} // namespace Opaax
