#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif


#include "Settings.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;


extern std::wstring fil;
// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::NN::implementation
{

	void MainWindow::ItemInvoked(IInspectable, NavigationViewItemInvokedEventArgs ar)
	{
		auto topnv = Content().as<NavigationView>();
		if (!topnv)
			return;
		Frame fr = topnv.FindName(L"contentFrame").as<Frame>();
		if (!fr)
			return;
		if (ar.IsSettingsInvoked())
		{
			fr.Navigate(winrt::xaml_typename<winrt::NN::Settings>());
			return;
		}
		auto it = ar.InvokedItemContainer().as<NavigationViewItem>();
		if (!it)
			return;
		auto n = it.Name();
		if (n == L"ViewNetwork")
			fr.Navigate(winrt::xaml_typename<winrt::NN::Network>());
		/*		if (n == L"ViewAudio")
			fr.Navigate(winrt::xaml_typename<winrt::tsed::Audio>());
		if (n == L"ViewLinks")
			fr.Navigate(winrt::xaml_typename<winrt::tsed::Links>());
			*/
	}

	void MainWindow::OnLoad(IInspectable, IInspectable)
	{
		void LoadNetworkFile();
		LoadNetworkFile();
		auto topnv = Content().as<NavigationView>();
		if (topnv)
		{
			Frame fr = topnv.FindName(L"contentFrame").as<Frame>();
			if (fr)
			{
				fr.Navigate(winrt::xaml_typename<winrt::NN::Network>(), winrt::box_value(fil.c_str()));
//				fr.Content().as<winrt::NN::Network>().wnd((wnd()));
			}
		}
	}


	void MainWindow::Resize()
	{
		auto nv = Content().as<NavigationView>();
		if (nv)
		{
			auto fr = nv.Content().as<Frame>();
			if (fr)
			{
				auto n = fr.Content().as<Page>();
				if (n)
				{
					auto s = n.as<winrt::NN::Network>();
					if (s)
					{
						s.Resize();
					}
				}
			}
		}
	}

	void MainWindow::Refresh()
	{
		auto topnv = Content().as<NavigationView>();
		if (topnv)
		{
			Frame fr = topnv.FindName(L"contentFrame").as<Frame>();
			if (fr)
			{
				auto co = fr.Content();
//				auto sc = co.try_as<winrt::tsed::Score>();
//				if (sc)
//					sc.Paint();
			}
		}
	}


}
