#pragma once

#include "Settings.g.h"

namespace winrt::NN::implementation
{
    struct Settings : SettingsT<Settings>
    {
        Settings()
        {
            // Xaml objects should not call InitializeComponent during construction.
            // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
        }


        winrt::event_token PropertyChanged(winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }
        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;

        static winrt::hstring txt(long jx)
        {
            const wchar_t* s(size_t);
            return s(jx);
        }

        void Refresh();
        winrt::hstring PythonLocation();
		void PythonLocation(winrt::hstring const& value);

		void OnPythonLocation(IInspectable const&, IInspectable const&);    

    };
}

namespace winrt::NN::factory_implementation
{
    struct Settings : SettingsT<Settings, implementation::Settings>
    {
    };
}
