
#include "BulletPch.h"
#include "Bullet/BulletUtilities.h"
#include "Bullet/BulletBody.h"
#include "Bullet/BulletBodyDefinition.h"
#include "Bullet/BulletShapes.h"
#include "Bullet/BulletWorld.h"

#include "Framework/FrameworkDataDeduction.h"

using namespace Helium;

namespace Helium
{
    struct BulletMotionState : public btMotionState
    {
        BulletMotionState(const btTransform &worldTrans)
            : m_Transform(worldTrans)
        {

        }

        virtual void getWorldTransform( btTransform& worldTrans ) const
        {
            worldTrans = m_Transform;
        }

        virtual void setWorldTransform( const btTransform& worldTrans ) 
        {
            m_Transform = worldTrans;
        }

        btTransform m_Transform;
    };
}

void BulletBody::Initialize( BulletWorld &rWorld, const BulletBodyDefinition &rBodyDefinition, const Helium::Simd::Vector3 &rInitialPosition, const Helium::Simd::Quat &rInitialRotation )
{
    HELIUM_ASSERT(rWorld.GetBulletWorld());

    if (rBodyDefinition.m_Shapes.IsEmpty())
    {
        HELIUM_TRACE( TraceLevels::Warning, "BulletBody::Initialize - Tried to create a bullet body with definition %s, but definition has 0 shapes.", 
            *rBodyDefinition.GetPath().ToString());

        return;
    }

    btCollisionShape *pFinalShape = 0;
    btVector3 finalInertia;
    float finalMass;

    if (rBodyDefinition.m_Shapes.GetSize() > 1)
    {
        //m_Shapes.Reserve(rBodyDefinition->m_Shapes.GetSize());
        //for (DynamicArray<BulletShape *>::ConstIterator iter = rBodyDefinition->m_Shapes.Begin(); 
        //    iter != rBodyDefinition->m_Shapes.End(); ++iter)
        //{
        //    const BulletShape *pShape = *iter;
        //    HELIUM_ASSERT(pShape);

        //    btCollisionShape *pBulletShape = pShape->CreateShape();
        //    HELIUM_ASSERT(pBulletShape);

        //    m_Shapes.Push(pBulletShape);
        //}

        // Now what?
    }
    else
    {
        BulletShape *pShape = rBodyDefinition.m_Shapes[0];
        btCollisionShape *pBulletShape = pShape->CreateShape();
        
        if (pShape->m_Mass != 0.0f)
        {
            pBulletShape->calculateLocalInertia(pShape->m_Mass, finalInertia);
        }

        pFinalShape = pBulletShape;
        finalMass = pShape->m_Mass;
    }

    btVector3 origin;
    ConvertToBullet(rInitialPosition, origin);

    btQuaternion rotation;
    ConvertToBullet(rInitialRotation, rotation);
    
	btTransform startTransform;
	startTransform.setIdentity();
    startTransform.setRotation(rotation);
    startTransform.setOrigin(origin);

    m_MotionState = new BulletMotionState(startTransform);
    m_Body = new btRigidBody(finalMass, m_MotionState, pFinalShape, finalInertia);
    m_Body->setRestitution(rBodyDefinition.m_Restitution);
    
    rWorld.GetBulletWorld()->addRigidBody(m_Body);
}

void Helium::BulletBody::GetPosition( Helium::Simd::Vector3 &rPosition )
{
    rPosition.SetSimdVector(m_MotionState->m_Transform.getOrigin().get128());
}

void Helium::BulletBody::GetRotation( Helium::Simd::Quat &rRotation )
{
    rRotation.SetSimdVector(m_MotionState->m_Transform.getRotation().get128());
}
