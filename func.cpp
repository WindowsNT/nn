#include "pch.h"
#include "en.hpp"

const wchar_t* s(size_t idx)
{
	if (idx > MAX_LANG)
		return L"";
	return z_strings[idx];
}



bool LoadFile(const wchar_t* f, std::vector<unsigned char>& d)
{
	HANDLE hX = CreateFile(f, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (hX == INVALID_HANDLE_VALUE)
		return false;
	LARGE_INTEGER sz = { 0 };
	GetFileSizeEx(hX, &sz);
	d.resize((size_t)(sz.QuadPart / sizeof(char)));
	DWORD A = 0;
	ReadFile(hX, d.data(), (DWORD)sz.QuadPart, &A, 0);
	CloseHandle(hX);
	if (A != sz.QuadPart)
		return false;
	return true;
}


