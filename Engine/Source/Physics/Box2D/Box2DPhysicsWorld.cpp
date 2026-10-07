#include "Physics/Box2D/Box2DPhysicsWorld.h"

#include <box2d/box2d.h>
#include <cfloat>

#include "Core/Log/Logger.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Box2DPhysics);

    namespace
    {
        b2Vec2   ToB2(Vector2F InVector) noexcept { return b2Vec2{ InVector.x, InVector.y }; }
        Vector2F ToVec2(b2Vec2 InVector) noexcept { return Vector2F{ InVector.x, InVector.y }; }

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

        // False for an invalid handle or a body already destroyed.
        bool LoadLiveBody(const BodyHandle InBody, b2BodyId& OutId) noexcept
        {
            if (!InBody.IsValid()) { return false; }

            OutId = b2LoadBodyId(InBody.Id);
            return b2Body_IsValid(OutId);
        }

        // 0 when the shape is stale (an end-touch event may name a destroyed shape).
        Uint64 EntityBitsFromShape(b2ShapeId InShape) noexcept
        {
            if (!b2Shape_IsValid(InShape)) { return 0; }

            const b2BodyId lBody = b2Shape_GetBody(InShape);
            return static_cast<Uint64>(reinterpret_cast<uintptr_t>(b2Body_GetUserData(lBody)));
        }

        bool OverlapCollect(b2ShapeId InShape, void* InContext)
        {
            auto* lOut = static_cast<TDynArray<Uint64>*>(InContext);
            lOut->push_back(EntityBitsFromShape(InShape));
            return true;
        }

        // Fixed buffer, so the mover allocates nothing; 8 planes is plenty in 2D.
        constexpr int kMaxMoverPlanes = 8;

        struct MoverPlaneContext
        {
            b2CollisionPlane Planes[kMaxMoverPlanes] = {};
            int              Count                   = 0;
            Uint64           IgnoreUserData          = 0;
        };

        // FLT_MAX pushLimit = a hard surface; clipVelocity = true so b2SolvePlanes clips against it.
        bool MoverPlaneFcn(b2ShapeId InShape, const b2PlaneResult* InPlane, void* InContext)
        {
            if (!InPlane->hit) { return true; }

            auto* lCtx = static_cast<MoverPlaneContext*>(InContext);

            // The mover is a kinematic body itself: skip its own capsule.
            if (lCtx->IgnoreUserData != 0 && EntityBitsFromShape(InShape) == lCtx->IgnoreUserData)
            {
                return true;
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
        // World units per metre (sleep thresholds, speculative margins).
        b2SetLengthUnitsPerMeter(InDesc.LengthUnitsPerMeter);

        b2WorldDef lWorldDef = b2DefaultWorldDef();
        lWorldDef.gravity    = ToB2(InDesc.Gravity);
        m_WorldId            = b2CreateWorld(&lWorldDef);

        OPAAX_LOG(LogBox2DPhysics, Info, "World created (gravity={},{} units/m={} substeps={})",
                  InDesc.Gravity.x, InDesc.Gravity.y, InDesc.LengthUnitsPerMeter, InDesc.SubStepCount);
    }

    Box2DPhysicsWorld::~Box2DPhysicsWorld()
    {
        b2DestroyWorld(m_WorldId);
        OPAAX_LOG(LogBox2DPhysics, Info, "World destroyed");
    }

    // =============================================================================
    // Simulation
    // =============================================================================
    void Box2DPhysicsWorld::Step(float InDeltaTime, int InSubStepCount)
    {
        b2World_Step(m_WorldId, InDeltaTime, InSubStepCount);
    }

    void Box2DPhysicsWorld::SetGravity(Vector2F InGravity)
    {
        b2World_SetGravity(m_WorldId, ToB2(InGravity));
    }

    Vector2F Box2DPhysicsWorld::GetGravity() const
    {
        return ToVec2(b2World_GetGravity(m_WorldId));
    }

    float Box2DPhysicsWorld::GetMaxLinearSpeed() const
    {
        return b2World_GetMaximumLinearSpeed(m_WorldId);
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
        lDef.motionLocks.angularZ = InDesc.bFixedRotation;
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
        lDef.isSensor             = InShape.bIsSensor;
        lDef.filter.categoryBits  = InShape.CategoryBits;
        lDef.filter.maskBits      = InShape.MaskBits;

        // Sensor events on every shape (Box2D 3.2 needs the visitor to opt in too).
        // Contact events on solid shapes only.
        lDef.enableSensorEvents  = true;
        lDef.enableContactEvents = !InShape.bIsSensor;

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
        const b2BodyId    lId = b2LoadBodyId(InBody.Id);
        const b2Transform lTarget{ ToB2(InPosition), b2MakeRot(InRotation) };

        // Swept toward the target, so it creates contacts instead of teleporting.
        b2Body_SetTargetTransform(lId, lTarget, InDeltaTime, true);
    }

    // =============================================================================
    // Body motion
    // =============================================================================
    Vector2F Box2DPhysicsWorld::GetLinearVelocity(BodyHandle InBody) const
    {
        b2BodyId lId = b2_nullBodyId;
        return LoadLiveBody(InBody, lId) ? ToVec2(b2Body_GetLinearVelocity(lId)) : Vector2F{ 0.f, 0.f };
    }

    void Box2DPhysicsWorld::SetLinearVelocity(BodyHandle InBody, Vector2F InVelocity)
    {
        b2BodyId lId = b2_nullBodyId;
        if (LoadLiveBody(InBody, lId)) { b2Body_SetLinearVelocity(lId, ToB2(InVelocity)); }
    }

    float Box2DPhysicsWorld::GetAngularVelocity(BodyHandle InBody) const
    {
        b2BodyId lId = b2_nullBodyId;
        return LoadLiveBody(InBody, lId) ? b2Body_GetAngularVelocity(lId) : 0.f;
    }

    void Box2DPhysicsWorld::SetAngularVelocity(BodyHandle InBody, float InRadiansPerSecond)
    {
        b2BodyId lId = b2_nullBodyId;
        if (LoadLiveBody(InBody, lId)) { b2Body_SetAngularVelocity(lId, InRadiansPerSecond); }
    }

    void Box2DPhysicsWorld::ApplyForce(BodyHandle InBody, Vector2F InForce)
    {
        b2BodyId lId = b2_nullBodyId;
        if (LoadLiveBody(InBody, lId)) { b2Body_ApplyForceToCenter(lId, ToB2(InForce), /*wake*/true); }
    }

    void Box2DPhysicsWorld::ApplyLinearImpulse(BodyHandle InBody, Vector2F InImpulse)
    {
        b2BodyId lId = b2_nullBodyId;
        if (LoadLiveBody(InBody, lId)) { b2Body_ApplyLinearImpulseToCenter(lId, ToB2(InImpulse), /*wake*/true); }
    }

    void Box2DPhysicsWorld::ApplyTorque(BodyHandle InBody, float InTorque)
    {
        b2BodyId lId = b2_nullBodyId;
        if (LoadLiveBody(InBody, lId)) { b2Body_ApplyTorque(lId, InTorque, /*wake*/true); }
    }

    void Box2DPhysicsWorld::ApplyAngularImpulse(BodyHandle InBody, float InImpulse)
    {
        b2BodyId lId = b2_nullBodyId;
        if (LoadLiveBody(InBody, lId)) { b2Body_ApplyAngularImpulse(lId, InImpulse, /*wake*/true); }
    }

    float Box2DPhysicsWorld::GetMass(BodyHandle InBody) const
    {
        b2BodyId lId = b2_nullBodyId;
        return LoadLiveBody(InBody, lId) ? b2Body_GetMass(lId) : 0.f;
    }

    // =============================================================================
    // Events
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
            OutBegan.emplace_back(EntityBitsFromShape(lEvent.sensorShapeId),
                                  EntityBitsFromShape(lEvent.visitorShapeId));
        }

        for (int i = 0; i < lEvents.endCount; ++i)
        {
            const b2SensorEndTouchEvent& lEvent = lEvents.endEvents[i];
            OutEnded.emplace_back(EntityBitsFromShape(lEvent.sensorShapeId),
                                  EntityBitsFromShape(lEvent.visitorShapeId));
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
            OutBegan.emplace_back(EntityBitsFromShape(lEvent.shapeIdA),
                                  EntityBitsFromShape(lEvent.shapeIdB));
        }

        for (int i = 0; i < lEvents.endCount; ++i)
        {
            const b2ContactEndTouchEvent& lEvent = lEvents.endEvents[i];
            OutEnded.emplace_back(EntityBitsFromShape(lEvent.shapeIdA),
                                  EntityBitsFromShape(lEvent.shapeIdB));
        }
    }

    // =============================================================================
    // Queries
    // =============================================================================
    PhysicsRayHit Box2DPhysicsWorld::RayCastClosest(Vector2F InOrigin, Vector2F InDirection,
                                                    float InDistance, Uint64 InChannelMask)
    {
        // A zero direction gives a zero-length ray (no hit).
        const b2Vec2 lTranslation = b2MulSV(InDistance, b2Normalize(ToB2(InDirection)));

        b2QueryFilter lFilter;
        lFilter.categoryBits = ~0ull;          // the query belongs to every category...
        lFilter.maskBits     = InChannelMask;  // ...and accepts the requested channels

        const b2RayResult lResult = b2World_CastRayClosest(m_WorldId, ToB2(InOrigin), lTranslation, lFilter);

        PhysicsRayHit lHit;
        lHit.bHit     = lResult.hit;
        lHit.Point    = ToVec2(lResult.point);
        lHit.Normal   = ToVec2(lResult.normal);
        lHit.Fraction = lResult.fraction;
        lHit.UserData = lResult.hit ? EntityBitsFromShape(lResult.shapeId) : 0;
        return lHit;
    }

    void Box2DPhysicsWorld::OverlapAABB(Vector2F InMin, Vector2F InMax, Uint64 InChannelMask,
                                        TDynArray<Uint64>& OutUserData)
    {
        OutUserData.clear();

        b2QueryFilter lFilter;
        lFilter.categoryBits = ~0ull;
        lFilter.maskBits     = InChannelMask;

        const b2AABB lAABB{ ToB2(InMin), ToB2(InMax) };
        b2World_OverlapAABB(m_WorldId, b2Pos_zero, lAABB, lFilter, &OverlapCollect, &OutUserData);
    }

    // =============================================================================
    // Geometric mover
    // =============================================================================
    MoveCapsuleResult Box2DPhysicsWorld::MoveCapsule(const MoveCapsuleInput& InInput)
    {
        // The mover is a query, not a shape: category ~0, mask = solid channels.
        b2QueryFilter lFilter;
        lFilter.categoryBits = ~0ull;
        lFilter.maskBits     = InInput.ChannelMask;

        b2Vec2       lPos    = ToB2(InInput.Position);
        const b2Vec2 lVel    = ToB2(InInput.Velocity);
        const b2Vec2 lTarget = b2MulAdd(lPos, InInput.DeltaTime, lVel);

        // Half a world unit: an iteration that barely moves means we have settled.
        constexpr float lToleranceSq = 0.5f * 0.5f;
        const int       lMaxIter     = InInput.MaxIterations > 0 ? InInput.MaxIterations : 1;

        MoverPlaneContext lCtx;
        lCtx.IgnoreUserData = InInput.IgnoreUserData;

        for (int lIter = 0; lIter < lMaxIter; ++lIter)
        {
            lCtx.Count = 0;

            b2Capsule lMover;
            lMover.center1 = b2Add(lPos, ToB2(InInput.Capsule.Center1));
            lMover.center2 = b2Add(lPos, ToB2(InInput.Capsule.Center2));
            lMover.radius  = InInput.Capsule.Radius;

            b2World_CollideMover(m_WorldId, b2Pos_zero, &lMover, lFilter, MoverPlaneFcn, &lCtx);
            const b2PlaneSolverResult lSolve = b2SolvePlanes(b2Sub(lTarget, lPos), lCtx.Planes, lCtx.Count);

            // Anti-tunnelling: never go further than a shape cast allows.
            const float  lFraction = b2World_CastMover(m_WorldId, b2Pos_zero, &lMover, lSolve.delta, lFilter);
            const b2Vec2 lDelta    = b2MulSV(lFraction, lSolve.delta);

            lPos = b2Add(lPos, lDelta);

            if (b2LengthSquared(lDelta) < lToleranceSq) { break; }
        }

        // Clipped against the touched planes, so the mover stops pushing into walls.
        const b2Vec2 lClipped = b2ClipVector(lVel, lCtx.Planes, lCtx.Count);

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
        lResult.bGrounded    = lGrounded;
        lResult.GroundNormal = ToVec2(lGroundNormal);
        return lResult;
    }
}
