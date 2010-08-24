#pragma once

#include "Application/Inspect/InspectControls.h"

namespace Helium
{
    namespace Editor
    {
        class Widget : public Inspect::Widget
        {
        public:
            Widget( Inspect::Control* control );
            ~Widget();

            Inspect::Control* GetControl()
            {
                return m_Control;
            }

            wxWindow* GetWindow()
            {
                return m_Window;
            }

            int GetStringWidth(const tstring& str);
            bool EllipsizeString(tstring& str, int width);

            // initialize state
            virtual void SetWindow( wxWindow* window );

            // callbacks from m_Control
            virtual void Unrealized( Inspect::Control* control );
            virtual void IsEnabledChanged( const Attribute<bool>::ChangeArgs& args );
            virtual void IsReadOnlyChanged( const Attribute<bool>::ChangeArgs& args );
            virtual void IsFrozenChanged( const Attribute<bool>::ChangeArgs& args );
            virtual void IsHiddenChanged( const Attribute<bool>::ChangeArgs& args );
            virtual void ForegroundColorChanged( const Attribute<u32>::ChangeArgs& args );
            virtual void BackgroundColorChanged( const Attribute<u32>::ChangeArgs& args );
            virtual void ToolTipChanged( const Attribute<tstring>::ChangeArgs& args );

        protected:
            Inspect::Control*   m_Control;
            wxWindow*           m_Window;
        };
    }
}