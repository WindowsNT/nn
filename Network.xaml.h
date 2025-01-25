#pragma once

#include "Network.g.h"
#include "Item.h"

namespace winrt::NN::implementation
{
    struct Network : NetworkT<Network>
    {
        Network()
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

        void Refresh(std::vector<std::wstring> strs);
        void Refresh(const wchar_t* s = L"");
        static winrt::hstring txt(long jx)
        {
            const wchar_t* s(size_t);
            return s(jx);
        }


        long  IndexOfLayer();
        void IndexOfLayer(long);
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::NN::Item> LayerList();



        // Menu
        void OnOpen(IInspectable, IInspectable);
        void OnNew(IInspectable, IInspectable);
        void OnSave(IInspectable, IInspectable);
        void OnSaveAs(IInspectable, IInspectable);
        void OnTrainCPU(IInspectable, IInspectable);
        void OnTrainGPU(IInspectable, IInspectable);
        void Train_Cancel(IInspectable, IInspectable);
        void OnDebugGenMnist(IInspectable, IInspectable);
        
    };
}

namespace winrt::NN::factory_implementation
{
    struct Network : NetworkT<Network, implementation::Network>
    {
    };
}
