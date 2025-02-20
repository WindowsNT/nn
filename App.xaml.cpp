#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

HICON hIcon1 = 0;
WNDPROC wProc = 0;
HWND MainWindow = 0;
std::wstring fil;
const wchar_t* s(size_t idx);
const wchar_t* ttitle = s(1);
winrt::Windows::Foundation::IInspectable thewindow;

void CallbackUpdate(WPARAM param, LPARAM msg);

LRESULT CALLBACK cbx(HWND hh, UINT mm, WPARAM ww, LPARAM ll)
{
    if (mm == WM_SIZE)
    {
        auto i = thewindow;
        if (i)
        {
            auto mw = i.as<winrt::NN::MainWindow>();
            if (mw)
                mw.Resize();
        }
        return 0;

    }

    if (mm == WM_USER + 1)
    {
        // Post update to dialog
        CallbackUpdate(ww, ll);
        return 0;
    }
    return CallWindowProc(wProc, hh, mm, ww, ll);
}

winrt::NN::MainWindow CreateWi()
{
    winrt::NN::MainWindow j;
    thewindow = j;
    j.Activate();
    static int One = 0;

    auto n = j.as<::IWindowNative>();
    if (n)
    {
        HWND hh;
        n->get_WindowHandle(&hh);
        if (hh)
        {
            MainWindow = hh;
            hIcon1 = LoadIcon(GetModuleHandle(0), L"ICON_1");

            wProc = (WNDPROC)GetWindowLongPtr(hh, GWLP_WNDPROC);
            SetWindowLongPtr(hh, GWLP_WNDPROC, (LONG_PTR)cbx);


            SetWindowText(hh, ttitle);
            if (One == 0)
                ShowWindow(hh, SW_SHOWMAXIMIZED);
            One = 1;
#define GCL_HICONSM         (-34)
#define GCL_HICON           (-14)
            SetClassLongPtr(hh, GCL_HICONSM, (LONG_PTR)hIcon1);
            SetClassLongPtr(hh, GCL_HICON, (LONG_PTR)hIcon1);

            if (1)
            {
                BOOL value = true;
                ::DwmSetWindowAttribute(hh, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
            }
        }
    }
    return j;
}


namespace winrt::NN::implementation
{
    /// <summary>
    /// Initializes the singleton application object.  This is the first line of authored code
    /// executed, and as such is the logical equivalent of main() or WinMain().
    /// </summary>
    App::App()
    {
        // Xaml objects should not call InitializeComponent during construction.
        // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
        });
#endif
    }

    /// <summary>
    /// Invoked when the application is launched.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& e)
    {
        window = CreateWi();
#ifdef _DEBUG
#endif

    }
}


int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR t, int)
{
    //    MessageBox(0, 0, 0, 0);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    if (t)
    {
        fil = t;
    }
#ifdef _DEBUG
	SetCurrentDirectory(L"f:\\wuitools\\nn\\x64\\Debug\\NN\\AppX");
#endif

    {
        void (WINAPI * pfnXamlCheckProcessRequirements)();
        auto module = ::LoadLibrary(L"Microsoft.ui.xaml.dll");
        if (module)
        {
            pfnXamlCheckProcessRequirements = reinterpret_cast<decltype(pfnXamlCheckProcessRequirements)>(GetProcAddress(module, "XamlCheckProcessRequirements"));
            if (pfnXamlCheckProcessRequirements)
            {
                (*pfnXamlCheckProcessRequirements)();
            }

            ::FreeLibrary(module);
        }
    }

    PWSTR p = 0;
    SHGetKnownFolderPath(FOLDERID_ProgramData, 0, 0, &p);
    std::wstring de = p;
    CoTaskMemFree(p);

    de += L"\\B7D701B9-F0C7-4771-B8ED-3F53453C1AB8";
	SHCreateDirectory(0, de.c_str());   
    datafolder = de.c_str();
	std::wstring sf = de + L"\\settings.xml";
    settings = std::make_shared<XML3::XML>(sf.c_str());

#ifdef _DEBUG
 //   void ImdbTest();
//    ImdbTest();
#endif

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    ::winrt::Microsoft::UI::Xaml::Application::Start(
        [](auto&&)
        {
            ::winrt::make<::winrt::NN::implementation::App>();
        });

    settings->Save();
    return 0;
}
