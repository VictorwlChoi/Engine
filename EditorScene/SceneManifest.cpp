#include "EditorScenePch.h"
#include "SceneManifest.h"

#include "Reflect/TranslatorDeduction.h"

HELIUM_DEFINE_CLASS( Helium::Editor::SceneManifest );

using namespace Helium;
using namespace Helium::Editor;

void SceneManifest::PopulateMetaType( Reflect::MetaStruct& comp )
{
	comp.AddField( &SceneManifest::m_BoundingBoxMin, TXT( "m_BoundingBoxMin" ) );
	comp.AddField( &SceneManifest::m_BoundingBoxMax, TXT( "m_BoundingBoxMax" ) );
	comp.AddField( &SceneManifest::m_Assets, TXT( "m_Assets" ) );
}