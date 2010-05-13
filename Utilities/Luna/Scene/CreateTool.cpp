#include "Precompile.h"
#include "CreateTool.h"

#include "Mesh.h"
#include "Scene.h"
#include "SceneEditor.h"
#include "SceneManager.h"
#include "ToolTimer.h"
#include "Transform.h"
#include "TreeSortTimer.h"

#include "Pick.h"

#include "Math/AngleAxis.h"
#include "File/Manager.h"
#include "Editor/Orientation.h"

#include "PrimitiveCircle.h"

using namespace Math;
using namespace Luna;

LUNA_DEFINE_TYPE(Luna::CreateTool);

IntersectionPlane CreateTool::s_PlaneSnap = IntersectionPlanes::Ground;

bool CreateTool::s_LiveObjectsOnly = false;
bool CreateTool::s_SurfaceSnap = false;
bool CreateTool::s_ObjectSnap = false;
bool CreateTool::s_NormalSnap = false;

bool CreateTool::s_RandomizeAzimuth = false;
float CreateTool::s_AzimuthMin = 0.f;
float CreateTool::s_AzimuthMax = 15.f;

bool CreateTool::s_RandomizeDirection = false;
float CreateTool::s_DirectionMin = 0.f;
float CreateTool::s_DirectionMax = 15.f;

bool CreateTool::s_RandomizeScale = false;
float CreateTool::s_ScaleMin = 0.8f;
float CreateTool::s_ScaleMax = 1.2f;

bool CreateTool::s_PaintMode = false;
bool CreateTool::s_PaintPreventAnyOverlap = false;
PlacementStyle CreateTool::s_PaintPlacementStyle = PlacementStyles::Radial;
DistributionStyle CreateTool::s_PaintDistributionStyle = DistributionStyles::Normal;
float CreateTool::s_PaintRadius = 10.0f;
int CreateTool::s_PaintSpeed = 10;
float CreateTool::s_PaintDensity = 1.0f;
float CreateTool::s_PaintJitter = 1.0f;

void CreateTool::InitializeType()
{
  Reflect::RegisterClass< Luna::CreateTool >( "Luna::CreateTool" );
}

void CreateTool::CleanupType()
{
  Reflect::UnregisterClass< Luna::CreateTool >();
}

CreateTool::CreateTool(Luna::Scene* scene, Enumerator* enumerator)
: Luna::SceneTool (scene, enumerator)
, m_Created (false)
, m_InstanceUpdateOffsets (false)
, m_Instance (NULL)
, m_AzimuthMin (NULL)
, m_AzimuthMax (NULL)
, m_DirectionMin (NULL)
, m_DirectionMax (NULL)
, m_ScaleMin (NULL)
, m_ScaleMax (NULL)
, m_PaintPreventAnyOverlap (NULL)
, m_PaintPlacementStyle (NULL)
, m_PaintDistributionStyle (NULL)
, m_PaintRadius (NULL)
, m_PaintSpeed (NULL)
, m_PaintDensity (NULL)
, m_PaintJitter (NULL)
{
  m_Scene->AddNodeAddedListener( NodeChangeSignature::Delegate ( this, &CreateTool::SceneNodeAdded ) );
  m_Scene->AddNodeRemovedListener( NodeChangeSignature::Delegate ( this, &CreateTool::SceneNodeRemoved ) );
  
  m_PaintTimer.AddTickListener( TickSignature::Delegate( this, &CreateTool::TimerCallback ) );
}

CreateTool::~CreateTool()
{
  m_Scene->GetManager()->GetEditor()->GetTreeSortTimer().Stop();

  m_PaintTimer.RemoveTickListener( TickSignature::Delegate( this, &CreateTool::TimerCallback ) );

  m_Scene->RemoveNodeAddedListener( NodeChangeSignature::Delegate ( this, &CreateTool::SceneNodeAdded ) );
  m_Scene->RemoveNodeRemovedListener( NodeChangeSignature::Delegate ( this, &CreateTool::SceneNodeRemoved ) );

  if (m_Instance.ReferencesObject())
  {
    // remove temp reference
    m_Scene->RemoveObject( m_Instance );
  }

  m_Scene->Push( m_Scene->GetSelection().SetItems( m_Selection ) );
}

void CreateTool::Place(const Math::Matrix4& position)
{
  if (m_Instance.ReferencesObject())
  {
    // remove temp reference
    m_Scene->RemoveObject( m_Instance );
  }

  //
  // Create Instance Object
  //

  m_Instance = CreateNode();

  if (m_Instance.ReferencesObject())
  {
    m_Instance->SetSelected( true );

    m_Instance->SetTransient( true );

    m_Instance->SetObjectTransform(position);

    m_Scene->AddObject( m_Instance );

    m_Instance->Evaluate( GraphDirections::Downstream );
  }
}

void CreateTool::DetermineTranslationAndNormal( int x, int y, Math::Vector3& t, Math::Vector3& n )
{
  FrustumLinePickVisitor pick( m_Scene->GetView()->GetCamera(), x, y );
  if ( !DetermineTranslationAndNormal( pick, t, n ) )
  {
    // place the object on the selected plane
    m_Scene->GetView()->GetCamera()->ViewportToPlaneVertex( x, y, s_PlaneSnap, t );
  }
}

bool CreateTool::DetermineTranslationAndNormal( PickVisitor& pick, Math::Vector3& t, Math::Vector3& n )
{
  // pick in the world
  m_Scene->GetManager()->GetRootScene()->Pick( &pick );

  bool set = false;
  if ( s_SurfaceSnap || s_ObjectSnap )
  {
    // process results
    V_PickHitSmartPtr sorted;
    PickHit::Sort( m_Scene->GetView()->GetCamera(), pick.GetHits(), sorted, PickSortTypes::Intersection );

    V_PickHitSmartPtr::const_iterator itr = sorted.begin();
    V_PickHitSmartPtr::const_iterator end = sorted.end();
    for ( ; itr != end; ++itr )
    {
      Luna::HierarchyNode* node = Reflect::ObjectCast<Luna::HierarchyNode>( (*itr)->GetObject() );

      // don't snap against what we are placing
      if ( node == m_Instance )
      {
        continue;
      }

      // eliminate nodes that are not live when we are live snapping
      if ( s_LiveObjectsOnly && !node->IsLive() )
      {
        continue;
      }

      // don't snap if we are surface snapping with no normal
      if ( s_SurfaceSnap && !(*itr)->HasNormal() )
      {
        continue;
      }

      // don't snap if we don't have an intersection
      if ( !(*itr)->HasIntersection() )
      {
        continue;
      }

      if ( s_ObjectSnap )
      {
        Vector4 v = node->GetTransform()->GetGlobalTransform().t;
        t.x = v.x;
        t.y = v.y;
        t.z = v.z;
        set = true;
      }
      else
      {
        t = (*itr)->GetIntersection();
      }

      if ( s_NormalSnap && (*itr)->HasNormal() )
      {
        n = (*itr)->GetNormal();
      }
      
      set = true;
      break;
    }
  }
  
  return set;
}

void CreateTool::GenerateInstanceOffsets( PlacementStyle style, float radius, float instanceRadius, Math::V_Vector3& positions )
{
  switch ( style )
  {
    case PlacementStyles::Grid:
    {
      float radiusSquared = radius * radius;
      int numInstances = MAX( 2, (int) sqrt( radiusSquared / ( instanceRadius * instanceRadius ) ) );
      float delta = radius * 2.0f / numInstances;

      for ( float x = -radius; x <= radius; x += delta )
      {
        for ( float y = -radius; y <= radius; y += delta )
        {
          if ( x * x + y * y < radiusSquared )
          {
            Math::Vector3 v = ( Luna::UpVector * x ) + ( Luna::OutVector * y );
            positions.push_back( v );
          }
        }
      }
      break;
    }
      
    case PlacementStyles::Radial:
    default:
    {
      float currentRadius = 0.0f;
      while ( currentRadius < radius )
      {
        float circumference = 2.0f * Math::Pi * currentRadius;
        int numInstances = MAX( 1, (int) ( circumference / ( 2.0f * instanceRadius ) ) );

        float deltaAngle = 2.0f * Math::Pi / numInstances;
        float currentAngle = 2.0f * Math::Pi * rand() / ( (float) RAND_MAX + 1.0f );

        for ( int i = 0; i < numInstances; ++i )
        {
          float x = currentRadius * cos( currentAngle );
          float y = currentRadius * sin( currentAngle );
          Math::Vector3 v = ( Luna::UpVector * x ) + ( Luna::OutVector * y );
          positions.push_back( v );

          currentAngle += deltaAngle;
          while ( currentAngle > ( 2.0f * Math::Pi ) )
          {
            currentAngle -= ( 2.0f * Math::Pi );
          }
        }

        currentRadius += instanceRadius + instanceRadius;
      }
      break;
    }
  }
}

void CreateTool::SelectInstanceOffsets( DistributionStyle style, float radius, Math::V_Vector3& offsets )
{
  Math::V_Vector3 selectedOffsets;
  selectedOffsets.reserve( offsets.size() );
  
  Math::V_Vector3::iterator itr = offsets.begin();
  Math::V_Vector3::iterator end = offsets.end();
  for ( ; itr != end; ++itr )
  {
    switch ( style )
    {
      case DistributionStyles::Uniform:
      {
        float randomNumber = rand() / ( (float) RAND_MAX + 1.0f );
        if ( randomNumber <= 0.5f )
        {
          selectedOffsets.push_back( *itr );
        }
        break;
      }

      case DistributionStyles::Linear:
      {
        float radiusPercent = (*itr).Length() / radius;
        float randomNumber = rand() / ( (float) RAND_MAX + 1.0f );
        float testNumber = 1.0f - radiusPercent;
        if ( randomNumber <= testNumber )
        {
          selectedOffsets.push_back( *itr );
        }
        break;
      }
        
      case DistributionStyles::Normal:
      {
        float radiusPercent = (*itr).Length() / radius;
        float randomNumber = rand() / ( (float) RAND_MAX + 1.0f );
        float testNumber = GetNormalProbabilityFromPercent( radiusPercent );
        if ( randomNumber <= testNumber )
        {
          selectedOffsets.push_back( *itr );
        }
        break;
      }

      case DistributionStyles::Constant:
      default:
        selectedOffsets.push_back( *itr );
        break;
    }
  }

  offsets.clear();
  offsets.reserve( selectedOffsets.size() );
  itr = selectedOffsets.begin();
  end = selectedOffsets.end();
  for ( ; itr != end; ++itr )
  {
    offsets.push_back( *itr );
  }
}

void CreateTool::JitterInstanceOffsets( float instanceRadius, float maxJitter, Math::V_Vector3& offsets )
{
  Math::V_Vector3 jitterVectors;
  jitterVectors.push_back( Luna::UpVector );
  jitterVectors.push_back( Luna::OutVector );

  Math::V_Vector3::iterator itr = offsets.begin();
  Math::V_Vector3::iterator end = offsets.end();
  for ( ; itr != end; ++itr )
  {
    Math::V_Vector3::const_iterator jitterItr = jitterVectors.begin();
    Math::V_Vector3::const_iterator jitterEnd = jitterVectors.end();
    for ( ; jitterItr != jitterEnd; ++jitterItr )
    {
      int searchTries = 10;
      while ( searchTries > 0 )
      {
        --searchTries;
        float jitter = ( rand() / ( (float) RAND_MAX + 1.0f ) ) * 2.0f - 1.0f;
        float randomNumber = rand() / ( (float) RAND_MAX + 1.0f );
        float testNumber = GetNormalProbabilityFromPercent( jitter );
        if ( randomNumber <= testNumber )
        {
          (*itr) += (*jitterItr) * jitter * maxJitter;
          searchTries = 0;
        }
      }
    }
  }
}

void CreateTool::RandomizeInstanceOffsets( Math::V_Vector3& offsets )
{
  Math::V_Vector3 newOffsets;
  newOffsets.reserve( offsets.size() );
  
  while ( offsets.size() )
  {
    Math::V_Vector3::iterator itr = offsets.begin() + ( rand() % offsets.size() );
    newOffsets.push_back( *itr );
    offsets.erase( itr );
  }
  
  Math::V_Vector3::iterator itr = newOffsets.begin();
  Math::V_Vector3::iterator end = newOffsets.end();
  for ( ; itr != end; ++itr )
  {
    offsets.push_back( *itr );
  }
}

void CreateTool::FinalizeOrientation(Math::Matrix4& position, const Math::Vector3& t, const Math::Vector3& n)
{
  // randomize the scale
  if ( s_RandomizeScale )
  {
    // random float b/t 0.0f and 1.0f
    f32 scale = rand()/(float(RAND_MAX)+1);

    // multiply by the range and add the base to set the bounds
    scale = (scale * (s_ScaleMax - s_ScaleMin)) + s_ScaleMin;

    // scale
    position *= Math::Matrix4 ( Math::Scale ( scale, scale, scale ) );
  }

  // seed the normal
  if ( n != Math::Vector3::Zero )
  {
    // rotate by the normal (we are currently orthogonal)
    position *= Math::Matrix4 ( Math::AngleAxis::Rotation( Luna::UpVector, n ) );
  }

  // randomize the rotation
  if ( s_RandomizeDirection )
  {
    // random float b/t -1.0f and 1.0f
    f32 angle = rand()/(float(RAND_MAX)+1) * 2.0f - 1.0f;

    // multiply by the range
    angle = (angle * (s_DirectionMax - s_DirectionMin));

    // add/subract the base depending on sign
    if (angle < 0.f)
    {
      angle -= s_DirectionMin;
    }
    else
    {
      angle += s_DirectionMin;
    }

    // convert to radians
    angle *= Math::DegToRad;

    // pick a random vector
    Math::Vector3 axis (rand()/(float(RAND_MAX)+1) - 0.5f, rand()/(float(RAND_MAX)+1) - 0.5f, rand()/(float(RAND_MAX)+1) - 0.5f);

    // make sure its 1.0f in length
    axis.Normalize();

    // rotate
    position *= Math::Matrix4 ( Math::AngleAxis ( angle, axis ) );
  }

  // randomize the rotation
  if ( s_RandomizeAzimuth )
  {
    // random float b/t -1.0f and 1.0f
    f32 angle = rand()/(float(RAND_MAX)+1) * 2.0f - 1.0f;

    // multiply by the range
    angle = (angle * (s_AzimuthMax - s_AzimuthMin));

    // add/subract the base depending on sign
    if (angle < 0.f)
    {
      angle -= s_AzimuthMin;
    }
    else
    {
      angle += s_AzimuthMin;
    }

    // convert to radians
    angle *= Math::DegToRad;

    // rotate
    position = Math::Matrix4 ( Math::AngleAxis ( angle, UpVector ) ) * position;
  }
  
  // set the translation
  position.t = t;
}

bool CreateTool::ValidPosition( const Math::AlignedBox& bounds, const Math::Vector3& translation, float minDistance )
{
  Luna::HierarchyNode* node = Reflect::ObjectCast<Luna::HierarchyNode>( m_Instance );
  
  FrustumPickVisitor frustumPick( m_Scene->GetView()->GetCamera(), Math::Frustum( bounds ) );
  m_Scene->GetManager()->GetCurrentScene()->Pick( &frustumPick );

  V_PickHitSmartPtr::const_iterator resultsItr = frustumPick.GetHits().begin();
  V_PickHitSmartPtr::const_iterator resultsEnd = frustumPick.GetHits().end();
  for ( ; resultsItr != resultsEnd; ++resultsItr )
  {
    Luna::HierarchyNode* currentNode = Reflect::ObjectCast<Luna::HierarchyNode>( (*resultsItr)->GetObject() );
    if ( !currentNode->IsTransient() && ( s_PaintPreventAnyOverlap || node->IsSimilar( currentNode ) ) )
    {
      const Luna::Transform* transform = currentNode->GetTransform();
      if ( !transform )
      {
        return false;
      }

      Math::Vector3 position( transform->GetGlobalTransform().t.x, transform->GetGlobalTransform().t.y, transform->GetGlobalTransform().t.z );
      Math::Vector3 differenceVector = translation - position;
      if ( differenceVector.Length() < minDistance )
      {
        return false;
      }
    }
  }

  return true;
}

void CreateTool::CalculateInstanceRadiusAndBounds( f32& instanceRadius, Math::AlignedBox& bounds )
{
  Luna::HierarchyNode* node = Reflect::ObjectCast<Luna::HierarchyNode>( m_Instance );
  bounds = node->GetObjectBounds();
  
  Math::Vector3 boundVector = bounds.maximum - bounds.minimum;
  Math::Vector3 out = boundVector * Luna::OutVector;
  Math::Vector3 side = boundVector * Luna::SideVector;
  instanceRadius = MAX( out.Length(), side.Length() ) / 2.0f;
}

void CreateTool::RefreshInstance( void )
{
  Math::Matrix4 orientation;
  FinalizeOrientation( orientation, m_InstanceTranslation, m_InstanceNormal );
  Place( orientation );
}

float CreateTool::GetNormalProbabilityFromPercent( float value )
{
  static float scale = 2.2f;
  float probability = exp( ( scale * scale * value * value ) / -2.0f );
  return probability;
}

void CreateTool::AddToScene()
{
  if (!m_Instance.ReferencesObject())
  {
    return;
  }

  if (!m_Scene->IsEditable())
  {
    return;
  }

  {
    LUNA_SCENE_SCOPE_TIMER( ("Remove Transient Instance") );

    m_Scene->RemoveObject( m_Instance );
  }

  Undo::BatchCommandPtr batch = new Undo::BatchCommand ();

  if (!m_Created)
  {
    batch->Push( m_Scene->GetSelection().Clear() );
    m_Created = true;
  }

  m_Instance->SetTransient( false );

  {
    LUNA_SCENE_SCOPE_TIMER( ("Push Undo Command Adding Instance Into Batch") );

    batch->Push( new SceneNodeExistenceCommand( Undo::ExistenceActions::Add, m_Scene, m_Instance ) );
  }

  {
    LUNA_SCENE_SCOPE_TIMER( ("Initialize Instance") );

    m_Instance->Initialize();
  }

  {
    LUNA_SCENE_SCOPE_TIMER( ("Append To Selection") );

    m_Selection.Append( m_Instance );
  }

  {
    LUNA_SCENE_SCOPE_TIMER( ("Push Undo Batch Into Scene") );

    m_Scene->Push( batch );
  }

  {
    LUNA_SCENE_SCOPE_TIMER( ("Place New Instance At Origin") );

    m_Instance = NULL;
    m_Scene->GetManager()->GetEditor()->GetTreeSortTimer().Start( TreeSortTimer::DefaultMilliseconds, wxTIMER_ONE_SHOT );
    Place( Math::Matrix4::Identity );
  }
}

void CreateTool::SceneNodeAdded( const NodeChangeArgs& args )
{
  m_Selection.Append( args.m_Node );
}

void CreateTool::SceneNodeRemoved( const NodeChangeArgs& args )
{
  m_Selection.Remove( args.m_Node );
}

void CreateTool::Draw( DrawArgs* args )
{
  if ( !s_PaintMode )
  {
    return;
  }
  
  Math::Matrix4 ringTransform;

  if ( m_InstanceNormal != Math::Vector3::Zero )
  {
    ringTransform *= Math::Matrix4( Math::AngleAxis::Rotation( Luna::SideVector, m_InstanceNormal ) );
  }
  else
  {
    ringTransform *= Math::Matrix4( Math::AngleAxis::Rotation( Luna::SideVector, Luna::UpVector ) );
  }

  ringTransform.t.x = m_InstanceTranslation.x;
  ringTransform.t.y = m_InstanceTranslation.y;
  ringTransform.t.z = m_InstanceTranslation.z;

  m_View->GetDevice()->SetTransform(D3DTS_WORLD, (D3DMATRIX*)(&ringTransform));

  Luna::PrimitiveCircle ring (m_View->GetResources());
  ring.m_RadiusSteps = 360;
  ring.m_Radius = s_PaintRadius;
  ring.Update();
  ring.Draw(args);
}

bool CreateTool::AllowSelection()
{
  return false;
}

bool CreateTool::MouseDown(wxMouseEvent& e)
{
  if ( e.MiddleDown() )
  {
    Math::Vector3 translation;
    Math::Vector3 normal;

    {
      LUNA_SCENE_SCOPE_TIMER( ( "Pick Location For Instance" ) );
      DetermineTranslationAndNormal( e.GetX(), e.GetY(), translation, normal );
    }

    CreateSingleObject( translation, normal );

    if ( s_PaintMode )
    {
      m_InstanceUpdateOffsets = true;
      CalculateInstanceRadiusAndBounds( m_InstanceRadius, m_InstanceBounds );
      if ( wxIsShiftDown() )
      {
        CreateMultipleObjects( true );
      }
      if ( !m_PaintTimer.IsRunning() )
      {
        m_PaintTimer.Start( 1000 / s_PaintSpeed );
      }
    }

    {
      LUNA_SCENE_SCOPE_TIMER( ( "Execute Scene" ) );

      m_Scene->Execute(true);
    }
  }
  
  return __super::MouseDown( e );
}

void CreateTool::MouseMove( wxMouseEvent& e )
{
  if ( !m_Instance.ReferencesObject() )
  {
    return;
  }
  
  if ( m_PaintTimer.IsRunning() )
  {
    m_InstanceUpdateOffsets = true;
    m_PaintTimer.CheckUpdate();
  }
  
  // get position
  Math::Vector3 translation;
  Math::Vector3 normal;
  DetermineTranslationAndNormal( e.GetX(), e.GetY(), translation, normal );

  Math::Matrix4 position;
  FinalizeOrientation( position, translation, normal );

  m_InstanceTranslation = translation;
  m_InstanceNormal = ( normal == Math::Vector3::Zero ) ? Luna::UpVector : normal;
  
  // hide the temporary object when painting and moving
  if ( m_PaintTimer.IsRunning() )
  {
    position *= Math::Matrix4( Math::Scale( 0.0f, 0.0f, 0.0f ) );
  }

  // set position
  m_Instance->SetObjectTransform( position );

  // evaluate
  m_Instance->Evaluate( GraphDirections::Downstream );

  // render
  m_Scene->Execute(true);

  __super::MouseMove(e);
}

void CreateTool::MouseUp(wxMouseEvent& e)
{
  if ( !e.MiddleDown() )
  {
    m_PaintTimer.Stop();
  }

  __super::MouseUp(e);
}

void CreateTool::KeyPress( wxKeyEvent& e )
{
  if (e.KeyCode() == WXK_RETURN)
  {
    AddToScene();

    // create new at position
    Place(Math::Matrix4::Identity);

    // render
    m_Scene->Execute(true);
  }
  else
  {
    e.Skip();
  }
}

void CreateTool::CreateProperties()
{
  __super::CreateProperties();

  Place(Math::Matrix4::Identity);

  m_Enumerator->PushPanel("Create", true);
  {
    m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Plane Snap");
      Inspect::Choice* choice = m_Enumerator->AddChoice<int>( new Nocturnal::MemberProperty<Luna::CreateTool, int> (this, &CreateTool::GetPlaneSnap, &CreateTool::SetPlaneSnap) );
      choice->SetDropDown( true );
      Inspect::V_Item items;

      {
        std::ostringstream str;
        str << IntersectionPlanes::View;
        items.push_back( Inspect::Item( "View", str.str() ) );
      }

      {
        std::ostringstream str;
        str << IntersectionPlanes::Ground;
        items.push_back( Inspect::Item( "Ground", str.str() ) );
      }

      choice->SetItems( items );
    }
    m_Enumerator->Pop();

    m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Snap to live objects only");
      m_Enumerator->AddCheckBox<bool>( new Nocturnal::MemberProperty<Luna::CreateTool, bool> (this, &CreateTool::GetLiveObjectsOnly, &CreateTool::SetLiveObjectsOnly) );
    }
    m_Enumerator->Pop();

    m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Surface Snap");
      m_Enumerator->AddCheckBox<bool>( new Nocturnal::MemberProperty<Luna::CreateTool, bool> (this, &CreateTool::GetSurfaceSnap, &CreateTool::SetSurfaceSnap) );
    }
    m_Enumerator->Pop();

    m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Object Snap");
      m_Enumerator->AddCheckBox<bool>( new Nocturnal::MemberProperty<Luna::CreateTool, bool> (this, &CreateTool::GetObjectSnap, &CreateTool::SetObjectSnap) );
    }
    m_Enumerator->Pop();

    m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Normal Snap");
      m_Enumerator->AddCheckBox<bool>( new Nocturnal::MemberProperty<Luna::CreateTool, bool> (this, &CreateTool::GetNormalSnap, &CreateTool::SetNormalSnap) );
    }
    m_Enumerator->Pop();

    m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Randomize Azimuth");
      m_Enumerator->AddCheckBox<bool>( new Nocturnal::MemberProperty<Luna::CreateTool, bool> (this, &CreateTool::GetRandomizeAzimuth, &CreateTool::SetRandomizeAzimuth) );
    }
    m_Enumerator->Pop();

    m_AzimuthMin = m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Azimuth Lower Bound");
      Inspect::Slider* slider = m_Enumerator->AddSlider<float>( new Nocturnal::MemberProperty<Luna::CreateTool, float> (this, &CreateTool::GetAzimuthMin, &CreateTool::SetAzimuthMin) );
      slider->SetRangeMin( 0.f );
      slider->SetRangeMax( 180.f );

      Inspect::ValuePtr textBox = m_Enumerator->GetContainer()->GetCanvas()->Create<Inspect::Value>( m_Enumerator );
      textBox->Bind( slider->GetData() );
      m_Enumerator->Add( textBox );
    }
    m_Enumerator->Pop();

    m_AzimuthMax = m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Azimuth Upper Bound");
      Inspect::Slider* slider = m_Enumerator->AddSlider<float>( new Nocturnal::MemberProperty<Luna::CreateTool, float> (this, &CreateTool::GetAzimuthMax, &CreateTool::SetAzimuthMax) );
      slider->SetRangeMin( 0.f );
      slider->SetRangeMax( 180.f );

      Inspect::ValuePtr textBox = m_Enumerator->GetContainer()->GetCanvas()->Create<Inspect::Value>( m_Enumerator );
      textBox->Bind( slider->GetData() );
      m_Enumerator->Add( textBox );
    }
    m_Enumerator->Pop();

    m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Randomize Direction");
      m_Enumerator->AddCheckBox<bool>( new Nocturnal::MemberProperty<Luna::CreateTool, bool> (this, &CreateTool::GetRandomizeDirection, &CreateTool::SetRandomizeDirection) );
    }
    m_Enumerator->Pop();

    m_DirectionMin = m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Direction Lower Bound");
      Inspect::Slider* slider = m_Enumerator->AddSlider<float>( new Nocturnal::MemberProperty<Luna::CreateTool, float> (this, &CreateTool::GetDirectionMin, &CreateTool::SetDirectionMin) );
      slider->SetRangeMin( 0.f );
      slider->SetRangeMax( 180.f );

      Inspect::ValuePtr textBox = m_Enumerator->GetContainer()->GetCanvas()->Create<Inspect::Value>( m_Enumerator );
      textBox->Bind( slider->GetData() );
      m_Enumerator->Add( textBox );
    }
    m_Enumerator->Pop();

    m_DirectionMax = m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Direction Upper Bound");
      Inspect::Slider* slider = m_Enumerator->AddSlider<float>( new Nocturnal::MemberProperty<Luna::CreateTool, float> (this, &CreateTool::GetDirectionMax, &CreateTool::SetDirectionMax) );
      slider->SetRangeMin( 0.f );
      slider->SetRangeMax( 180.f );

      Inspect::ValuePtr textBox = m_Enumerator->GetContainer()->GetCanvas()->Create<Inspect::Value>( m_Enumerator );
      textBox->Bind( slider->GetData() );
      m_Enumerator->Add( textBox );
    }
    m_Enumerator->Pop();

    m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Randomize Scale");
      m_Enumerator->AddCheckBox<bool>( new Nocturnal::MemberProperty<Luna::CreateTool, bool> (this, &CreateTool::GetRandomizeScale, &CreateTool::SetRandomizeScale) );
    }
    m_Enumerator->Pop();

    m_ScaleMin = m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Scale Lower Bound");
      Inspect::Slider* slider = m_Enumerator->AddSlider<float>( new Nocturnal::MemberProperty<Luna::CreateTool, float> (this, &CreateTool::GetScaleMin, &CreateTool::SetScaleMin) );
      slider->SetRangeMin( 0.05f );
      slider->SetRangeMax( 5.f );

      Inspect::ValuePtr textBox = m_Enumerator->GetContainer()->GetCanvas()->Create<Inspect::Value>( m_Enumerator );
      textBox->Bind( slider->GetData() );
      m_Enumerator->Add( textBox );
    }
    m_Enumerator->Pop();

    m_ScaleMax = m_Enumerator->PushContainer();
    {
      m_Enumerator->AddLabel("Scale Upper Bound");
      Inspect::Slider* slider = m_Enumerator->AddSlider<float>( new Nocturnal::MemberProperty<Luna::CreateTool, float> (this, &CreateTool::GetScaleMax, &CreateTool::SetScaleMax) );
      slider->SetRangeMin( 0.05f );
      slider->SetRangeMax( 5.f );

      Inspect::ValuePtr textBox = m_Enumerator->GetContainer()->GetCanvas()->Create<Inspect::Value>( m_Enumerator );
      textBox->Bind( slider->GetData() );
      m_Enumerator->Add( textBox );
    }
    m_Enumerator->Pop();

    m_Enumerator->PushPanel("Paint");
    {
      m_Enumerator->PushContainer();
      {
        m_Enumerator->AddLabel("Enable Instance Painting");
        m_Enumerator->AddCheckBox<bool>( new Nocturnal::MemberProperty<Luna::CreateTool, bool> (this, &CreateTool::GetPaintMode, &CreateTool::SetPaintMode) );
      }
      m_Enumerator->Pop();

      m_PaintPreventAnyOverlap = m_Enumerator->PushContainer();
      {
        m_Enumerator->AddLabel("Prevent Overlap");
        m_Enumerator->AddCheckBox<bool>( new Nocturnal::MemberProperty<Luna::CreateTool, bool> (this, &CreateTool::GetPaintPreventAnyOverlap, &CreateTool::SetPaintPreventAnyOverlap) );
      }
      m_Enumerator->Pop();

      m_PaintPlacementStyle = m_Enumerator->PushContainer();
      {
        m_Enumerator->AddLabel("Placement Style");

        Inspect::Choice* choice = m_Enumerator->AddChoice<int>( new Nocturnal::MemberProperty<Luna::CreateTool, int> (this, &CreateTool::GetPaintPlacementStyle, &CreateTool::SetPaintPlacementStyle ) );
        choice->SetDropDown( true );
        Inspect::V_Item items;

        {
          std::ostringstream str;
          str << PlacementStyles::Grid;
          items.push_back( Inspect::Item( "Grid", str.str() ) );
        }

        {
          std::ostringstream str;
          str << PlacementStyles::Radial;
          items.push_back( Inspect::Item( "Radial", str.str() ) );
        }

        choice->SetItems( items );
      }
      m_Enumerator->Pop();

      m_PaintDistributionStyle = m_Enumerator->PushContainer();
      {
        m_Enumerator->AddLabel("Distribution Style");

        Inspect::Choice* choice = m_Enumerator->AddChoice<int>( new Nocturnal::MemberProperty<Luna::CreateTool, int> (this, &CreateTool::GetPaintDistributionStyle, &CreateTool::SetPaintDistributionStyle ) );
        choice->SetDropDown( true );
        Inspect::V_Item items;

        {
          std::ostringstream str;
          str << DistributionStyles::Constant;
          items.push_back( Inspect::Item( "Constant", str.str() ) );
        }

        {
          std::ostringstream str;
          str << DistributionStyles::Uniform;
          items.push_back( Inspect::Item( "Uniform", str.str() ) );
        }

        {
          std::ostringstream str;
          str << DistributionStyles::Linear;
          items.push_back( Inspect::Item( "Linear", str.str() ) );
        }

        {
          std::ostringstream str;
          str << DistributionStyles::Normal;
          items.push_back( Inspect::Item( "Normal", str.str() ) );
        }

        choice->SetItems( items );
      }
      m_Enumerator->Pop();

      m_PaintRadius = m_Enumerator->PushContainer();
      {
        m_Enumerator->AddLabel("Radius");
        Inspect::Slider* slider = m_Enumerator->AddSlider<float>( new Nocturnal::MemberProperty<Luna::CreateTool, float> (this, &CreateTool::GetPaintRadius, &CreateTool::SetPaintRadius) );
        slider->SetRangeMin( 0.1f );
        slider->SetRangeMax( 30.0f );
        slider->SetValue( s_PaintRadius );

        Inspect::ValuePtr textBox = m_Enumerator->GetContainer()->GetCanvas()->Create<Inspect::Value>( m_Enumerator );
        textBox->Bind( slider->GetData() );
        m_Enumerator->Add( textBox );
      }
      m_Enumerator->Pop();

      m_PaintSpeed = m_Enumerator->PushContainer();
      {
        m_Enumerator->AddLabel("Speed");
        Inspect::Slider* slider = m_Enumerator->AddSlider<int>( new Nocturnal::MemberProperty<Luna::CreateTool, int> (this, &CreateTool::GetPaintSpeed, &CreateTool::SetPaintSpeed) );
        slider->SetRangeMin( 1 );
        slider->SetRangeMax( 10 );
        slider->SetValue( s_PaintSpeed );

        Inspect::ValuePtr textBox = m_Enumerator->GetContainer()->GetCanvas()->Create<Inspect::Value>( m_Enumerator );
        textBox->Bind( slider->GetData() );
        m_Enumerator->Add( textBox );
      }
      m_Enumerator->Pop();

      m_PaintDensity = m_Enumerator->PushContainer();
      {
        m_Enumerator->AddLabel("Density");
        Inspect::Slider* slider = m_Enumerator->AddSlider<float>( new Nocturnal::MemberProperty<Luna::CreateTool, float> (this, &CreateTool::GetPaintDensity, &CreateTool::SetPaintDensity) );
        slider->SetRangeMin( 0.0f );
        slider->SetRangeMax( 2.0f );
        slider->SetValue( s_PaintDensity );

        Inspect::ValuePtr textBox = m_Enumerator->GetContainer()->GetCanvas()->Create<Inspect::Value>( m_Enumerator );
        textBox->Bind( slider->GetData() );
        m_Enumerator->Add( textBox );
      }
      m_Enumerator->Pop();
      
      m_PaintJitter = m_Enumerator->PushContainer();
      {
        m_Enumerator->AddLabel("Jitter");
        Inspect::Slider* slider = m_Enumerator->AddSlider<float>( new Nocturnal::MemberProperty<Luna::CreateTool, float> (this, &CreateTool::GetPaintJitter, &CreateTool::SetPaintJitter) );
        slider->SetRangeMin( 0.0f );
        slider->SetRangeMax( 1.0f );
        slider->SetValue( s_PaintJitter );

        Inspect::ValuePtr textBox = m_Enumerator->GetContainer()->GetCanvas()->Create<Inspect::Value>( m_Enumerator );
        textBox->Bind( slider->GetData() );
        m_Enumerator->Add( textBox );
      }
      m_Enumerator->Pop();
    }
    m_Enumerator->Pop();
  }
  m_Enumerator->Pop();

  m_AzimuthMin->SetEnabled(s_RandomizeAzimuth);
  m_AzimuthMax->SetEnabled(s_RandomizeAzimuth);

  m_DirectionMin->SetEnabled(s_RandomizeDirection);
  m_DirectionMax->SetEnabled(s_RandomizeDirection);

  m_ScaleMin->SetEnabled(s_RandomizeScale);
  m_ScaleMax->SetEnabled(s_RandomizeScale);

  m_PaintPreventAnyOverlap->SetEnabled(s_PaintMode);
  m_PaintPlacementStyle->SetEnabled(s_PaintMode);
  m_PaintDistributionStyle->SetEnabled(s_PaintMode);
  m_PaintRadius->SetEnabled(s_PaintMode);
  m_PaintSpeed->SetEnabled(s_PaintMode);
  m_PaintDensity->SetEnabled(s_PaintMode);
  m_PaintJitter->SetEnabled(s_PaintMode);
}

void CreateTool::SetupInstanceOffsets( float instanceRadius, Math::V_Vector3& instanceOffsets )
{
  instanceOffsets.clear();
  instanceOffsets.reserve( 256 );

  float adjustedInstanceRadius = instanceRadius / s_PaintDensity;

  GenerateInstanceOffsets( s_PaintPlacementStyle, s_PaintRadius, adjustedInstanceRadius, instanceOffsets );
  SelectInstanceOffsets( s_PaintDistributionStyle, s_PaintRadius, instanceOffsets );
  JitterInstanceOffsets( instanceRadius, s_PaintJitter, instanceOffsets );
  RandomizeInstanceOffsets( instanceOffsets );
}

void CreateTool::CreateSingleObject( const Math::Vector3& translation, const Math::Vector3& normal, bool checkValid )
{
  Math::Matrix4 orientation;

  {
    LUNA_SCENE_SCOPE_TIMER( ( "Finalize Instance Orientation" ) );
    FinalizeOrientation( orientation, translation, normal );
  }

  if ( checkValid )
  {
    float adjustedInstanceRadius = m_InstanceRadius / s_PaintDensity;
    float minDistance = 2.0f * adjustedInstanceRadius;
    float scale = minDistance / m_InstanceRadius;
    Math::Matrix4 scaledOrientation( Math::Scale( minDistance, minDistance, minDistance ) );
    scaledOrientation *= orientation;

    Math::AlignedBox instanceBounds = m_InstanceBounds;
    instanceBounds.Transform( scaledOrientation );

    if ( !ValidPosition( instanceBounds, translation, minDistance ) )
    {
      return;
    }
  }
  
  if ( m_Instance.ReferencesObject() )
  {
    LUNA_SCENE_SCOPE_TIMER( ( "Update Temporary Instance At Location" ) );
    m_Instance->SetObjectTransform( orientation );
    m_Instance->Evaluate( GraphDirections::Downstream );
  }
  else
  {
    LUNA_SCENE_SCOPE_TIMER( ( "Place Temporary Instance At Location" ) );
    Place( orientation );
  }

  {
    LUNA_SCENE_SCOPE_TIMER( ( "Add Instance To Scene" ) );
    AddToScene();
  }

  if ( m_Instance.ReferencesObject() )
  {
    LUNA_SCENE_SCOPE_TIMER( ( "Update Temporary Instance At Location" ) );
    if ( m_PaintTimer.IsRunning() )
    {
      orientation *= Math::Matrix4( Math::Scale( 0.0f, 0.0f, 0.0f ) );
    }
    
    m_Instance->SetObjectTransform( orientation );
    m_Instance->Evaluate( GraphDirections::Downstream );
  }
}

void CreateTool::CreateMultipleObjects( bool stamp )
{
  LUNA_SCENE_SCOPE_TIMER( ("Place Multiple Instances At Location") );

  if ( m_InstanceRadius <= 0.0f )
  {
    return;
  }
  
  if ( m_InstanceOffsets.empty() || m_InstanceUpdateOffsets )
  {
    m_InstanceOffsets.clear();
    SetupInstanceOffsets( m_InstanceRadius, m_InstanceOffsets );
    m_InstanceUpdateOffsets = false;
  }
  
  f32 maxTime = 100.0f;
  Profile::Timer instanceTimer;
  Math::Vector3 instanceNormalOffset = m_InstanceNormal.Normalize() * 2.0f * s_PaintRadius;

  while ( m_InstanceOffsets.size() && ( stamp || ( instanceTimer.Elapsed() < maxTime ) ) )
  {
    Math::V_Vector3::iterator itr = m_InstanceOffsets.begin();

    Math::Matrix4 instanceTransform;
    instanceTransform.t = Math::Vector4( *itr );
    instanceTransform *= Math::Matrix4( Math::AngleAxis::Rotation( Luna::SideVector, m_InstanceNormal ) );
    instanceTransform.t += Math::Vector4( m_InstanceTranslation );

    Math::Vector3 point = Math::Vector3( instanceTransform.t.x, instanceTransform.t.y, instanceTransform.t.z );
    LinePickVisitor pick( m_Scene->GetView()->GetCamera(), Math::Line( point + instanceNormalOffset, point - instanceNormalOffset ) );

    Math::Vector3 instanceTranslation;
    Math::Vector3 instanceNormal;
    if ( DetermineTranslationAndNormal( pick, instanceTranslation, instanceNormal ) )
    {
      point = Math::Vector3( instanceTranslation.x - m_InstanceTranslation.x, instanceTranslation.y - m_InstanceTranslation.y, instanceTranslation.z - m_InstanceTranslation.z );
      if ( point.Length() <= s_PaintRadius )
      {
        CreateSingleObject( instanceTranslation, instanceNormal, true );
      }
    }

    m_InstanceOffsets.erase( itr );
  }
}

void CreateTool::TimerCallback( const TickArgs& args )
{
  CreateMultipleObjects();
  m_Scene->Execute(true);
}

int CreateTool::GetPlaneSnap() const
{
  return s_PlaneSnap;
}

void CreateTool::SetPlaneSnap(int snap)
{
  s_PlaneSnap = (IntersectionPlane)snap;

  m_Scene->Execute(false);
}

bool CreateTool::GetSurfaceSnap() const
{
  return s_SurfaceSnap;
}

void CreateTool::SetSurfaceSnap(bool snap)
{
  s_SurfaceSnap = snap;

  if (s_SurfaceSnap)
  {
    s_ObjectSnap = false;
    m_Enumerator->GetContainer()->Read();
  }

  m_Scene->Execute(false);
}

bool CreateTool::GetLiveObjectsOnly() const
{
  return s_LiveObjectsOnly;
}

void CreateTool::SetLiveObjectsOnly(bool snap)
{
  s_LiveObjectsOnly = snap;
}

bool CreateTool::GetObjectSnap() const
{
  return s_ObjectSnap;
}

void CreateTool::SetObjectSnap(bool snap)
{
  s_ObjectSnap = snap;

  if (s_ObjectSnap)
  {
    s_SurfaceSnap = false;
    m_Enumerator->GetContainer()->Read();
  }

  m_Scene->Execute(false);
}

bool CreateTool::GetNormalSnap() const
{
  return s_NormalSnap;
}

void CreateTool::SetNormalSnap(bool snap)
{
  s_NormalSnap = snap;

  m_Scene->Execute(false);
}

bool CreateTool::GetRandomizeAzimuth() const
{
  return s_RandomizeAzimuth;
}

void CreateTool::SetRandomizeAzimuth(bool value)
{
  s_RandomizeAzimuth = value;
  m_AzimuthMin->SetEnabled(value);
  m_AzimuthMax->SetEnabled(value);
}

float CreateTool::GetAzimuthMin() const
{
  return s_AzimuthMin;
}

void CreateTool::SetAzimuthMin(float value)
{
  s_AzimuthMin = value;
}

float CreateTool::GetAzimuthMax() const
{
  return s_AzimuthMax;
}

void CreateTool::SetAzimuthMax(float value)
{
  s_AzimuthMax = value;
}

bool CreateTool::GetRandomizeDirection() const
{
  return s_RandomizeDirection;
}

void CreateTool::SetRandomizeDirection(bool value)
{
  s_RandomizeDirection = value;
  m_DirectionMin->SetEnabled(value);
  m_DirectionMax->SetEnabled(value);
}

float CreateTool::GetDirectionMin() const
{
  return s_DirectionMin;
}

void CreateTool::SetDirectionMin(float value)
{
  s_DirectionMin = value;
}

float CreateTool::GetDirectionMax() const
{
  return s_DirectionMax;
}

void CreateTool::SetDirectionMax(float value)
{
  s_DirectionMax = value;
}

bool CreateTool::GetRandomizeScale() const
{
  return s_RandomizeScale;
}

void CreateTool::SetRandomizeScale(bool value)
{
  s_RandomizeScale = value;
  m_ScaleMin->SetEnabled(value);
  m_ScaleMax->SetEnabled(value);
}

float CreateTool::GetScaleMin() const
{
  return s_ScaleMin;
}

void CreateTool::SetScaleMin(float value)
{
  s_ScaleMin = value;
}

float CreateTool::GetScaleMax() const
{
  return s_ScaleMax;
}

void CreateTool::SetScaleMax(float value)
{
  s_ScaleMax = value;
}

bool CreateTool::GetPaintMode() const
{
  return s_PaintMode;
}

void CreateTool::SetPaintMode(bool value)
{
  s_PaintMode = value;
  m_PaintPreventAnyOverlap->SetEnabled(s_PaintMode);
  m_PaintPlacementStyle->SetEnabled(s_PaintMode);
  m_PaintDistributionStyle->SetEnabled(s_PaintMode);
  m_PaintRadius->SetEnabled(s_PaintMode);
  m_PaintSpeed->SetEnabled(s_PaintMode);
  m_PaintDensity->SetEnabled(s_PaintMode);
  m_PaintJitter->SetEnabled(s_PaintMode);
}

bool CreateTool::GetPaintPreventAnyOverlap() const
{
  return s_PaintPreventAnyOverlap;
}

void CreateTool::SetPaintPreventAnyOverlap(bool value)
{
  s_PaintPreventAnyOverlap = value;
}

float CreateTool::GetPaintRadius() const
{
  return s_PaintRadius;
}

void CreateTool::SetPaintRadius(float value)
{
  s_PaintRadius = value;
}

int CreateTool::GetPaintSpeed() const
{
  return s_PaintSpeed;
}

void CreateTool::SetPaintSpeed(int value)
{
  s_PaintSpeed = value;
}

float CreateTool::GetPaintDensity() const
{
  return s_PaintDensity;
}

void CreateTool::SetPaintDensity(float value)
{
  s_PaintDensity = value;
}

float CreateTool::GetPaintJitter() const
{
  return s_PaintJitter;
}

void CreateTool::SetPaintJitter(float value)
{
  s_PaintJitter = value;
}

int CreateTool::GetPaintPlacementStyle() const
{
  return s_PaintPlacementStyle;
}

void CreateTool::SetPaintPlacementStyle( int style )
{
  s_PaintPlacementStyle = (PlacementStyle) style;
}

int CreateTool::GetPaintDistributionStyle() const
{
  return s_PaintDistributionStyle;
}

void CreateTool::SetPaintDistributionStyle( int style )
{
  s_PaintDistributionStyle = (DistributionStyle) style;
}
