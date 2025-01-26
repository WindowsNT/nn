#include "pch.h"
#include "Network.xaml.h"
#if __has_include("Network.g.cpp")
#include "Network.g.cpp"
#endif



using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

extern bool TrainCancel;
extern std::wstring fil;


#include "nn.h"
void NNToPythonOnnx(::NN&,const wchar_t*);
void NNToPythonPTH(::NN&, const wchar_t*);

extern int CurrentBatch;
extern int NumEpochsX;
extern PROJECT project;

void LoadNetworkFile();
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

    long  Network::IndexOfAct()
    {
        for (size_t i = 0; i < project.nn.Layers.size(); i++)
        {
            if (project.nn.Layers[i].Sel)
                return project.nn.Layers[i].ActType;
        }
        return 0;
    }
    void Network::IndexOfAct(long l)
    {
        for (size_t i = 0; i < project.nn.Layers.size(); i++)
        {
            if (project.nn.Layers[i].Sel)
                project.nn.Layers[i].ActType = l;
        }
    }

    winrt::Windows::Foundation::Collections::IObservableVector<winrt::NN::Item> Network::ActList()
    {
        auto children = single_threaded_observable_vector<NN::Item>();
        auto item = winrt::make<NN::implementation::Item>();
        item.Name1(L"Sigmoid");
        children.Append(item);
        item = winrt::make<NN::implementation::Item>();
        item.Name1(L"Relu");
        children.Append(item);
        return children;
    }


    winrt::Windows::Foundation::Collections::IObservableVector<winrt::NN::Item> Network::LayerList()
    {
        auto children = single_threaded_observable_vector<NN::Item>();
        for (size_t i = 0; i < project.nn.Layers.size(); i++)
        {
			auto item = winrt::make<NN::implementation::Item>();
            if (i == 0)
				item.Name1(std::wstring(s(14)) + std::wstring(L" ") + std::wstring(s(17)));
            else
            if (i == (project.nn.Layers.size() - 1))
                item.Name1(std::wstring(s(15)) + std::wstring(L" ") + std::wstring(s(17)));
            else
                item.Name1(std::wstring(s(16)) + std::wstring(L" ") + std::to_wstring(i));
			children.Append(item);
        }
		return children;
    }

    double Network::BatchNumber()
    {
        return (double)CurrentBatch;
    }
    void Network::BatchNumber(double v)
    {
		CurrentBatch = (int)v;
    }
    double Network::NumEpochs()
    {
        return (double)NumEpochsX;
    }
    void Network::NumEpochs(double v)
    {
        NumEpochsX = (int)v;
    }

    bool Network::InputsVisible()
    {
		for (size_t i = 0; i < project.nn.Layers.size() && i < 1 ; i++)
		{
			if (project.nn.Layers[i].Sel)
				return 1;
		}
		return 0;
    }
    bool Network::CountVisible()
    {
        for (size_t i = 1; i < (project.nn.Layers.size() - 1); i++)
        {
            if (project.nn.Layers[i].Sel)
                return 1;
        }
        return 0;
    }
    bool Network::OutputsVisible()
    {
        for (size_t i = 0; i < project.nn.Layers.size(); i++)
        {
            if (i == (project.nn.Layers.size() - 1) && project.nn.Layers[i].Sel)
                return 1;
        }
        return 0;
    }

    double Network::NumNeurons()
    {
        for (size_t i = 0; i < project.nn.Layers.size(); i++)
        {
            if (project.nn.Layers[i].Sel)
            {
				if (i == 0)
					return (double)project.nn.Layers[i + 1].weights.rows();
				if (i == (project.nn.Layers.size() - 1))
					return (double)project.nn.Layers[i].biases.cols();
				return (double)project.nn.Layers[i].weights.cols();
            }
        }
        return 0;
    }
    void Network::NumNeurons(double n)
    {
        for (size_t i = 0; i < project.nn.Layers.size() ; i++)
        {
            if (project.nn.Layers[i].Sel)
            {
                if (i == 0) // input
                {
					if (project.nn.Layers[i + 1].weights.rows() != (int)n)
                    {
                        project.nn.Layers[i + 1].weights.init((int)n, project.nn.Layers[i + 1].weights.cols());
                        project.nn.RecalculateWB();
                        Refresh({ L"WeightsText",L"BiasesText" });
                    }
                }
                else
                if (i == (project.nn.Layers.size() - 1)) // output
                {
                    if (project.nn.Layers[i].biases.cols() != n)
                    {
                        project.nn.Layers[i].weights.init(project.nn.Layers[i].weights.rows(), (int)n);
                        project.nn.Layers[i].biases.init(1, (int)n);
                        project.nn.RecalculateWB();
                        Refresh({ L"WeightsText",L"BiasesText" });
                    }
                }
                else
                {
                    if (project.nn.Layers[i].weights.rows() != (int)n)
                    {
                        project.nn.Layers[i].weights.init(project.nn.Layers[i].weights.rows(), (int)n);
                        project.nn.Layers[i].biases.init(1, (int)n);
                        project.nn.RecalculateWB();
                        Refresh({ L"WeightsText",L"BiasesText" });
                    }
                }
                break;
            }
        }
    }


    winrt::hstring Network::WeightsText()
    {
		std::vector<wchar_t> s(10000);
        for (size_t i = 1; i < project.nn.Layers.size(); i++)
        {
            if (project.nn.Layers[i].Sel)
            {
				swprintf_s(s.data(), 10000, L"%ix%i", project.nn.Layers[i].weights.rows(), project.nn.Layers[i].weights.cols());
                break;
            }
        }
        return s.data();
    }

    winrt::hstring Network::BiasesText()
    {
        std::vector<wchar_t> s(10000);
        for (size_t i = 1; i < project.nn.Layers.size(); i++)
        {
            if (project.nn.Layers[i].Sel)
            {
                swprintf_s(s.data(), 10000, L"%ix%i", project.nn.Layers[i].biases.rows(), project.nn.Layers[i].biases.cols());
                break;
            }
        }
        return s.data();
    }

    void Network::AddHiddenAfter(IInspectable, IInspectable)
    {
        /*
            mNIST: input # 784
		             hidden : 128 neurons * 784 weights each
					 output : 10 neurons * 128 weights each
                     */
        for (size_t i = 0; i < project.nn.Layers.size() - 1 ; i++)
        {
            if (project.nn.Layers[i].Sel)
            {
				auto lr = project.nn.Layers[i].lr;
				if (lr <= 0.000f)
					lr = 0.01f;
                int num_neurons = project.nn.Layers[i + 1].weights.cols();
                int num_weights_per_neuron = project.nn.Layers[i].output.cols();
                if (i == 0)
                    num_weights_per_neuron = 128;
				Layer l(lr, 1, num_neurons, num_weights_per_neuron);

				project.nn.Layers.insert(project.nn.Layers.begin() + i + 1, l);

                project.nn.RecalculateWB();
                Refresh();
                break;
            }
        }
    }


    double Network::LearningRate()
    {
        for (size_t i = 1; i < project.nn.Layers.size(); i++)
        {
            if (project.nn.Layers[i].Sel)
                return project.nn.Layers[i].lr;

        }
        return 0;
//        return std::numeric_limits<double>::quiet_NaN();;
    }

    void Network::LearningRate(double v)
    {
        for (size_t i = 1; i < project.nn.Layers.size(); i++)
        {
            if (project.nn.Layers[i].Sel)
				project.nn.Layers[i].lr = v;
        }
        return;
    }


	long Network::IndexOfLayer()
	{
		for (size_t i = 0; i < project.nn.Layers.size(); i++)
		{
			if (project.nn.Layers[i].Sel)
				return (long)i;
		}
        if (project.nn.Layers.empty())
            return 0;
        project.nn.Layers[0].Sel = 1;
		return 0;
	}
	void Network::IndexOfLayer(long idx)
	{
        for (size_t i = 0; i < project.nn.Layers.size(); i++)
        {
			if (i == idx)
				project.nn.Layers[i].Sel = 1;
			else
				project.nn.Layers[i].Sel = 0;
        }
        Refresh({L"LearningRate",L"LearningRateVisible",L"ActFuncVisible",L"IndexOfAct",L"InputsVisible",L"CountVisible",L"OutputsVisible",L"NumNeurons",L"WeightsText",L"BiasesText"});
	}


	bool Network::LearningRateVisible()
	{
		for (size_t i = 1; i < project.nn.Layers.size(); i++)
		{
			if (project.nn.Layers[i].Sel)
				return 1;
		}
		return 0;
	}

    bool Network::ActFuncVisible()
    {
        for (size_t i = 1; i < project.nn.Layers.size(); i++)
        {
            if (project.nn.Layers[i].Sel)
                return 1;
        }
        return 0;
    }

    void Network::OnOpen(IInspectable, IInspectable)
    {
//        f:\wuitools\nn\mnist.nn

		OPENFILENAME of = { 0 };
		of.lStructSize = sizeof(of);
		of.hwndOwner = (HWND)0;
		of.lpstrFilter = L"*.nn\0*.nn\0\0";
		std::vector<wchar_t> fnx(10000);
		of.lpstrFile = fnx.data();
		of.nMaxFile = 10000;
		of.lpstrDefExt = L"nn";
		of.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		if (!GetOpenFileName(&of))
			return;
        fil = fnx.data();
        LoadNetworkFile();
    	Refresh();
    }

    void Network::OnExportPTH(IInspectable, IInspectable)
    {
        OPENFILENAME of = { 0 };
        of.lStructSize = sizeof(of);
        of.hwndOwner = (HWND)0;
        of.lpstrFilter = L"*.pth\0*.pth\0\0";
        std::vector<wchar_t> fnx(10000);
        of.lpstrFile = fnx.data();
        of.nMaxFile = 10000;
        of.lpstrDefExt = L"pth";
        of.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        if (!GetSaveFileName(&of))
            return;

        NNToPythonPTH(project.nn,fnx.data());

    }

    void Network::OnExportONNX(IInspectable, IInspectable)
    {
        OPENFILENAME of = { 0 };
        of.lStructSize = sizeof(of);
        of.hwndOwner = (HWND)0;
        of.lpstrFilter = L"*.onnx\0*.onnx\0\0";
        std::vector<wchar_t> fnx(10000);
        of.lpstrFile = fnx.data();
        of.nMaxFile = 10000;
        of.lpstrDefExt = L"onnx";
        of.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        if (!GetSaveFileName(&of))
            return;

		NNToPythonOnnx(project.nn,fnx.data());
    }

     void Network::OnNew(IInspectable, IInspectable)
    {
         std::vector<wchar_t> a(10000);
         GetModuleFileName(0, a.data(), 10000);
         ShellExecute(0, L"open", a.data(), 0, 0, SW_SHOW);
         return;
    }
    void Network::OnSave(IInspectable a, IInspectable b)
    {
        if (fil.empty())
            OnSaveAs(a, b);
        else
        {
            void SaveProjectToFile(const wchar_t* f);
			SaveProjectToFile(fil.c_str());
        }
    }
    void Network::OnSaveAs(IInspectable a, IInspectable b)
    {
        OPENFILENAME of = { 0 };
        of.lStructSize = sizeof(of);
        of.hwndOwner = (HWND)0;
        of.lpstrFilter = L"*.nn\0*.nn\0\0";
        std::vector<wchar_t> fnx(10000);
        of.lpstrFile = fnx.data();
        of.nMaxFile = 10000;
        of.lpstrDefExt = L"nn";
        of.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        if (!GetSaveFileName(&of))
            return;

        fil = fnx.data();
        OnSave(a, b);
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
