#pragma once

#include "MainWindow.g.h"

namespace winrt::NN::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow()
        {
            // Xaml objects should not call InitializeComponent during construction.
            // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
        }

        static winrt::hstring txt(long jx)
        {
            const wchar_t* s(size_t);
            return s(jx);
        }


        void Resize();
        void Refresh();
        void OnLoad(IInspectable, IInspectable);
        void ItemInvoked(IInspectable, winrt::Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs);

    };
}

namespace winrt::NN::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
