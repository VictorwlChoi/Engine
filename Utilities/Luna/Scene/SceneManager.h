#pragma once

#include "API.h"

#include "Asset/LevelAsset.h"
#include "Common/Automation/Event.h"
#include "Editor/DocumentManager.h"
#include "Core/Selection.h"
#include "Undo/QueueManager.h"


namespace Asset
{
  class LevelAsset;
  typedef Nocturnal::SmartPtr< LevelAsset > LevelAssetPtr;
}

namespace Luna
{
  // Forwards
  class SceneEditor;
  class HierarchyNode;

  class Scene;
  typedef Nocturnal::SmartPtr< Luna::Scene > ScenePtr;
  typedef std::map< std::string, ScenePtr > M_SceneSmartPtr;
  typedef std::map< Luna::Scene*, i32 > M_AllocScene;

  class SceneDocument;
  typedef Nocturnal::SmartPtr< SceneDocument > SceneDocumentPtr;

  struct FilePathChangedArgs;
  struct NodeChangeArgs;

  struct SceneChangeArgs
  {
    Luna::Scene* m_Scene;

    SceneChangeArgs (Luna::Scene* scene)
      : m_Scene (scene)
    {

    }
  };

  typedef Nocturnal::Signature< void, const SceneChangeArgs& > SceneChangeSignature;

  /////////////////////////////////////////////////////////////////////////////
  // Tracks all the scenes and their undo queues.
  // 
  class LUNA_SCENE_API SceneManager : public DocumentManager
  {
  private:
    // the root scene (the first one opened)
    ScenePtr m_Root;

    // all loaded scenes by path
    M_SceneSmartPtr m_Scenes;

    // the nested scenes that can be freed
    M_AllocScene m_AllocatedScenes;

    // the current scene
    Luna::Scene* m_CurrentScene;

    // the editor window
    SceneEditor* m_Editor;

    Undo::QueueManager m_UndoManager;

    // level that owns the world we're looking at (as far as we know)
    Asset::LevelAssetPtr m_CurrentLevel; 

  public:
    SceneManager( SceneEditor* editor );
    ~SceneManager();

    SceneEditor* GetEditor();

    Asset::LevelAsset* GetCurrentLevel() const;

    ScenePtr NewScene( bool isRoot, std::string path = "", bool addDoc = true );
    virtual DocumentPtr OpenPath( const std::string& path, std::string& error ) NOC_OVERRIDE;
    ScenePtr OpenZone( const std::string& path, std::string& error );

  public:
    virtual bool Save( DocumentPtr document, std::string& error ) NOC_OVERRIDE;

    void SetRootScene( Luna::Scene* root );
    Luna::Scene* GetRootScene();
    bool IsRoot( Luna::Scene* scene ) const;

    void AddScene( Luna::Scene* scene );
    void RemoveScene( Luna::Scene* scene );
    void RemoveAllScenes();

    const M_SceneSmartPtr& GetScenes() const;
    Luna::Scene* GetScene( const std::string& path ) const;
    Luna::Scene* GetScene( const tuid& id ) const;

    bool IsNestedScene( Luna::Scene* scene ) const;
    Luna::Scene* AllocateNestedScene( const std::string& path, Luna::Scene* parent );
    Luna::Scene* AllocateNestedScene( const tuid& id, Luna::Scene* parent );
    void ReleaseNestedScene( Luna::Scene*& scene );
    
    static std::string GetUniqueFileName();

    bool HasCurrentScene() const;
    bool IsCurrentScene( const Luna::Scene* sceneToCompare ) const;

    Luna::Scene* GetCurrentScene() const;
    void SetCurrentScene( Luna::Scene* scene );

    Luna::Scene* GetLightingScene( bool load = false );

    void FreezeTreeSorting();
    void ThawTreeSorting();

    // Undo/redo support
    bool CanUndo();
    bool CanRedo();
    void Undo();
    void Redo();
    void Push( Undo::Queue* queue );

  private:
    Luna::Scene* FindFirstNonNestedScene() const;
    void DocumentPathChanged( const DocumentPathChangedArgs& args );
    void SceneNodeDeleting( const NodeChangeArgs& args );
    void OnDocumentClosed( const DocumentChangedArgs& args );

  private:
    SceneChangeSignature::Event m_SceneAdded;
  public:
    void AddSceneAddedListener( const SceneChangeSignature::Delegate& listener )
    {
      m_SceneAdded.Add( listener );
    }
    void RemoveSceneAddedListener( const SceneChangeSignature::Delegate& listener )
    {
      m_SceneAdded.Remove( listener );
    }

  private:
    SceneChangeSignature::Event m_SceneRemoving;
  public:
    void AddSceneRemovingListener( const SceneChangeSignature::Delegate& listener )
    {
      m_SceneRemoving.Add( listener );
    }
    void RemoveSceneRemovingListener( const SceneChangeSignature::Delegate& listener )
    {
      m_SceneRemoving.Remove( listener );
    }

  private:
    SceneChangeSignature::Event m_CurrentSceneChanging;
  public:
    void AddCurrentSceneChangingListener( const SceneChangeSignature::Delegate& listener )
    {
      m_CurrentSceneChanging.Add( listener );
    }
    void RemoveCurrentSceneChangingListener( const SceneChangeSignature::Delegate& listener )
    {
      m_CurrentSceneChanging.Remove( listener );
    }

  private:
    SceneChangeSignature::Event m_CurrentSceneChanged;
  public:
    void AddCurrentSceneChangedListener( const SceneChangeSignature::Delegate& listener )
    {
      m_CurrentSceneChanged.Add( listener );
    }
    void RemoveCurrentSceneChangedListener( const SceneChangeSignature::Delegate& listener )
    {
      m_CurrentSceneChanged.Remove( listener );
    }
  };
}