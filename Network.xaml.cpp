#include "pch.h"
#include "Network.xaml.h"
#if __has_include("Network.g.cpp")
#include "Network.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

extern bool TrainCancel;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::NN::implementation
{
    void Network::Refresh(const wchar_t* s)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ s });
    }
    void Network::Refresh(std::vector<std::wstring> strs)
    {
        if (strs.empty())
            m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"" });
        else
        {
            for (auto& s : strs)
                m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ s });
        }
    }
    void Network::OnDebugGenMnist(IInspectable, IInspectable)
    {
        void mnist_test();
         mnist_test();

    }

    void Network::OnOpen(IInspectable, IInspectable)
    {

    }
     void Network::OnNew(IInspectable, IInspectable)
    {

    }
    void Network::OnSave(IInspectable, IInspectable)
    {

    }
    void Network::OnSaveAs(IInspectable, IInspectable)
    {

    }

    void Network::Train_Cancel(IInspectable, IInspectable)
    {
        TrainCancel = 1;
    }

    void Network::OnTrainGPU(IInspectable, IInspectable)
    {
        auto sp = Content().as<Panel>();
        auto ct = sp.FindName(L"Input1").as<ContentDialog>();
		ct.Title(winrt::box_value(L"Training on GPU"));
        ct.ShowAsync();

        void StartTrain(int m,void*);
        StartTrain(1,this);

    }
    void Network::OnTrainCPU(IInspectable, IInspectable)
    {
        auto sp = Content().as<Panel>();
        auto ct = sp.FindName(L"Input1").as<ContentDialog>();
        ct.Title(winrt::box_value(L"Training on CPU"));
        ct.ShowAsync();

        void StartTrain(int m,void*);
        StartTrain(0,this);
    }


}

extern HWND MainWindow;
// Called in thread
DWORD ptc = 0;
wchar_t msg[1000] = {};
HRESULT ForCallback(int iEpoch, int iDataset, int step, float totalloss, float acc,void* param)
{
    if (TrainCancel)
        return E_ABORT;

    if (step == 0 || step == 4 || step == 5)
    {
        return S_OK;
    }
    auto ctc = GetTickCount();
    if (ctc < (ptc + 1000))
        return S_OK;

    ptc = ctc;
    swprintf_s(msg,1000,L"Epoch %i - Set %i.\r\nCurrent loss: %.1f\r\nCurrent accuracy: %.1f", iEpoch + 1,iDataset,totalloss,acc);
    PostMessage(MainWindow, WM_USER + 1, (WPARAM)param, (LPARAM)msg);

    return S_OK;
}

// From main thread
void CallbackUpdate(WPARAM param, LPARAM msg1)
{
    winrt::NN::implementation::Network* n1 = (winrt::NN::implementation::Network*)param;
    n1->Content().as<Panel>().FindName(L"Input2").as<TextBlock>().Text((const wchar_t*)msg1);

}
