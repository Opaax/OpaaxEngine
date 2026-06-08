#include "Box2DPhysicsWorld.h"

#include <box2d/box2d.h>

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
        if (lGeo.Type == EColliderShape::Circle)
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

} // namespace Opaax
