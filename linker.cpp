#include "pch.h"


#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib,"comsuppw.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "Prntvpt.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "crypt32.lib")


#pragma comment(lib,"packages\\Microsoft.AI.DirectML.1.15.4\\bin\\x64-win\\DirectML.lib")



extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}


#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

