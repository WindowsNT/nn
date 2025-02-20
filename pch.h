#define DISABLE_XAML_GENERATED_MAIN
#pragma once
#include <windows.h>
#include <unknwn.h>
#include <wininet.h>
#include <restrictederrorinfo.h>
#include <hstring.h>
#include <queue>
#include <any>
#include <optional>
#include <stack>
#include <mutex>
#include <vector>
#include <set>
#include <functional>
#include <memory>
#include <vector>
#include <shlobj.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <random>
#include <dwmapi.h>

#include <wincodec.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1_2.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>
#include <optional>
#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <atlbase.h>
#include "d3dx12.h"
#include ".\\packages\\Microsoft.AI.DirectML.1.15.4\\include\\DirectML.h"
#include "DirectMLX.h"

#include "ystring.h"

#include <random>
#undef min
#undef max
#define SINGLE_SOURCE_IMPL
#include "matrix2.hpp"


// Undefine GetCurrentTime macro to prevent
// conflict with Storyboard::GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <microsoft.ui.xaml.media.dxinterop.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.Windows.AppNotifications.h>
#include <winrt/Microsoft.Windows.AppNotifications.Builder.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <wil/cppwinrt_helpers.h>
#include <appmodel.h>

#include "xml3all.h"

#include <tclap/Arg.h>


inline std::wstring datafolder;
inline std::shared_ptr<XML3::XML> settings;

template <typename T = char>
inline bool PutFile(const wchar_t* f, std::vector<T>& d, bool Fw = false)
{
	HANDLE hX = CreateFile(f, GENERIC_WRITE, 0, 0, Fw ? CREATE_ALWAYS : CREATE_NEW, 0, 0);
	if (hX == INVALID_HANDLE_VALUE)
		return false;
	DWORD A = 0;
	WriteFile(hX, d.data(), (DWORD)(d.size() * sizeof(T)), &A, 0);
	CloseHandle(hX);
	if (A != d.size())
		return false;
	return true;
}

#include "d2d.hpp"
