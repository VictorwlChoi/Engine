#pragma once

#include "Foundation/Inspect/Container.h"

#include "Editor/Controls/Tree/TreeWndCtrl.h"
#include "Editor/Inspect/Widget.h"

namespace Helium
{
    namespace Editor
    {
        class TreeCanvasWidget;

        class TreeItemData : public wxTreeItemData
        {
        public:
            TreeItemData( TreeCanvasWidget* widget )
                : m_Widget( widget )
            {

            }
            
            TreeCanvasWidget* GetWidget()
            {
                return m_Widget;
            }

        private:
            TreeCanvasWidget* m_Widget;
        };

        class TreeCanvasWidget : public Reflect::ConcreteInheritor<TreeCanvasWidget, Widget>
        {
        public:
            TreeCanvasWidget()
                : m_ContainerControl( NULL )
                , m_TreeWndCtrl( NULL )
                , m_ItemData( NULL )
            {

            }

            TreeCanvasWidget( Inspect::Container* container );

            wxTreeItemId GetId() const
            {
                return m_ItemData.GetId();
            }

            void SetId( wxTreeItemId id )
            {
                m_ItemData.SetId( id );
            }

            TreeWndCtrl* GetTreeWndCtrl()
            {
                return m_TreeWndCtrl;
            }

            void SetTreeWndCtrl(TreeWndCtrl* ctrl)
            {
                m_Window = m_TreeWndCtrl = ctrl;
            }

            virtual void Create( wxWindow* parent ) HELIUM_OVERRIDE;
            virtual void Destroy() HELIUM_OVERRIDE;

            virtual void Read() HELIUM_OVERRIDE {}
            virtual bool Write() HELIUM_OVERRIDE { return true; }

        private:
            Inspect::Container*     m_ContainerControl;
            TreeWndCtrl*            m_TreeWndCtrl;
            TreeItemData            m_ItemData;
        };
    }
}