#include "pch.h"
#include "Settings.xaml.h"
#if __has_include("Settings.g.cpp")
#include "Settings.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
std::wstring PythonFolder();

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.


int CALLBACK BrowseCallbackProc(
	HWND hwnd,
	UINT uMsg,
	LPARAM,
	LPARAM lpData
)
{
	if (uMsg == BFFM_INITIALIZED)
	{
		ITEMIDLIST* sel = (ITEMIDLIST*)lpData;
		if (!sel)
			return 0;
		SendMessage(hwnd, BFFM_SETSELECTION, false, (LPARAM)sel);
	}
	return 0;
}


bool BrowseFolder(HWND hh, const TCHAR* tit, const TCHAR* root, const TCHAR* sel, TCHAR* rv)
{
	IShellFolder* sf = 0;
	SHGetDesktopFolder(&sf);
	if (!sf)
		return false;

	BROWSEINFO bi = { 0 };
	bi.hwndOwner = hh;
	if (root && _tcslen(root))
	{
		PIDLIST_RELATIVE pd = 0;
		wchar_t dn[1000] = { 0 };
		wcscpy_s(dn, 1000, root);
		sf->ParseDisplayName(hh, 0, dn, 0, &pd, 0);
		bi.pidlRoot = pd;
	}
	bi.lpszTitle = tit;
	bi.ulFlags = BIF_EDITBOX | BIF_NEWDIALOGSTYLE;
	bi.lpfn = BrowseCallbackProc;
	if (sel && _tcslen(sel))
	{
		PIDLIST_RELATIVE pd = 0;
		wchar_t dn[1000] = { 0 };
		wcscpy_s(dn, 1000, sel);
		sf->ParseDisplayName(hh, 0, dn, 0, &pd, 0);
		bi.lParam = (LPARAM)pd;
	}

	PIDLIST_ABSOLUTE pd = SHBrowseForFolder(&bi);
	if (!pd)
		return false;

	SHGetPathFromIDList(pd, rv);
	CoTaskMemFree(pd);
	return true;
}




namespace winrt::NN::implementation
{


	void Settings::Refresh()
	{
		m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{L""});
	}
	winrt::hstring Settings::PythonLocation()
	{
		return PythonFolder().c_str();
	}
	
	void Settings::PythonLocation(winrt::hstring const& value)
	{
		settings->GetRootElement().vv("pyf").SetWideValue(value.c_str());
		settings->Save();
		Refresh();
	}

	void Settings::OnPythonLocation(IInspectable const&, IInspectable const&)
	{
		std::vector<wchar_t> rv(10000);
		auto e = PythonFolder();
		SHCreateDirectory(0, e.c_str());
		auto b = BrowseFolder(0, 0, 0, e.c_str(), rv.data());
		if (!b)
			return;
		PythonLocation(rv.data());
	}

}
