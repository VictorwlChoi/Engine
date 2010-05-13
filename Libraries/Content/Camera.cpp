#include "Camera.h"
#include "ContentVisitor.h" 

using namespace Reflect;
using namespace Content;

REFLECT_DEFINE_CLASS(Camera)

void Camera::EnumerateClass( Reflect::Compositor<Camera>& comp )
{
  comp.AddField( &Camera::m_View, "m_View" );
  comp.AddField( &Camera::m_FOV, "m_FOV" );
  comp.AddField( &Camera::m_Width, "m_Width" );
  comp.AddField( &Camera::m_Height, "m_Height" );
}

void Camera::Host(ContentVisitor* visitor)
{
  visitor->VisitCamera(this); 
}