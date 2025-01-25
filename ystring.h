// Ultimate Windows Library 
// (C) Michael Chourdakis
#pragma once

	// astring class
	class astring : public std::string
	{
	public:
		astring& Format(const char* f, ...)
		{
			va_list args;
			va_start(args, f);

			int len = _vscprintf(f, args) + 100;
			if (len < 8192)
				len = 8192;
			std::vector<char> b(len);
			vsprintf_s(b.data(), len, f, args);
			assign(b.data());
			va_end(args);
			return *this;
		}

	};


	// ystring class, wstring <-> string wrapper
	class ystring : public std::wstring
		{
		private:
			mutable std::string asc_str_st;
		public:

			// Constructors
			ystring(HWND hh) : std::wstring()
				{
				AssignFromHWND(hh);
				}
			ystring(HWND hh, int ID) : std::wstring()
				{
				AssignFromHWND(GetDlgItem(hh, ID));
				}
			ystring() : std::wstring()
				{
				}
			ystring(const char* v, int CP = CP_UTF8)
				{
				EqChar(v, CP);
				}
			ystring(const std::string& v, int CP = CP_UTF8)
				{
				EqChar(v.c_str(), CP);
				}
			ystring(const std::wstring& v)
				{
				assign(v.c_str());
				}
			ystring(const wchar_t* f)
				{
				if (!f)
					return;
				assign(f);
				}
/*		
			// Constructor and format for sprintf-like
			ystring(const wchar_t* f, ...) : std::wstring()
				{
				va_list args;
				va_start(args, f);

				int len = _vscwprintf(f, args) + 100;
				if (len < 8192)
					len = 8192;
				vector<wchar_t> b(len);
				vswprintf_s(b.data(), len, f, args);
				assign(b.data());
				va_end(args);
				}
*/
			ystring& Format(const wchar_t* f, ...)
				{
				va_list args;
				va_start(args, f);

				int len = _vscwprintf(f, args) + 100;
				if (len < 8192)
					len = 8192;
				std::vector<wchar_t> b(len);
				vswprintf_s(b.data(), len, f, args);
				assign(b.data());
				va_end(args);
				return *this;
				}

			// operator =
			void operator=(const char* v)
				{
				EqChar(v);
				}
			void operator=(const wchar_t* v)
				{
				assign(v);
				}
			void operator=(const std::wstring& v)
				{
				assign(v.c_str());
				}
			void operator=(const ystring& v)
				{
				assign(v.c_str());
				}
			void operator=(const std::string& v)
				{
				EqChar(v.c_str());
				}
			CLSID ToCLSID()
				{
				CLSID a;
				CLSIDFromString(c_str(), &a);
				return a;
				}
			void operator=(CLSID cid)
				{
				wchar_t ad[100] = { 0 };
				StringFromGUID2(cid, ad, 100);
				assign(ad);
				}

			operator const wchar_t*()
				{
				return c_str();
				}

			// asc_str() and a_str() and operator const char*() 
			const std::string& asc_str(int CP = CP_UTF8) const
				{
				const wchar_t* s = c_str();
				int sz = WideCharToMultiByte(CP, 0, s, -1, 0, 0, 0, 0);
				std::vector<char> d(sz + 100);
				WideCharToMultiByte(CP, 0, s, -1, d.data(), sz + 100, 0, 0);
				asc_str_st = d.data();
				return asc_str_st;
				}
			operator const char*() const
				{
				return a_str();
				}
			const char* a_str(int CP = CP_UTF8) const
				{
				asc_str(CP);
				return asc_str_st.c_str();
				}

			long long ll() const
			{
				return atoll(a_str());
			}

			// Internal Convertor
			void EqChar(const char* v, int CP = CP_UTF8)
				{
				clear();
				if (!v)
					return;
				int sz = MultiByteToWideChar(CP, 0, v, -1, 0, 0);
				std::vector<wchar_t> d(sz + 100);
				MultiByteToWideChar(CP, 0, v, -1, d.data(), sz + 100);
				assign(d.data());
				}

			// From HWND
			void AssignFromHWND(HWND hh)
				{
				int zl = GetWindowTextLength(hh);
				std::vector<wchar_t> n(zl + 100);
				GetWindowTextW(hh, n.data(), zl + 100);
				assign(n.data());
				}
		};


	// String Functions
#define MATCH_TRUE 1
#define MATCH_FALSE 0
#define MATCH_ABORT -1

#define NEGATE_CLASS
#define OPTIMIZE_JUST_STAR
#undef MATCH_TAR_PATTERN

		// OPTI
	static char DMtable[256] = { 0 };
	static void PrepareDoMatchTable()
		{
		for (int i = 0; i < 256; ++i)
			DMtable[i] = (char)toupper(static_cast<char>(i));
		}
	inline int DoMatch(const char *text, const char *p, bool IsCaseSensitive = false)
		{
		// DoMatchTable Initialization
		if (DMtable[0x31] != '1')
			PrepareDoMatchTable();

		// probably the MOST DIFFICULT FUNCTION in TurboIRC
		// Thanks to BitchX for copying this function
		//int last;
		int matched;
		//int reverse;
		int pT = 0;
		int pP = 0;

		for (; p[pP] != '\0'; pP++, pT++)
			{
			if (text[pT] == '\0' && p[pP] != '*')
				return MATCH_ABORT;
			switch (p[pP])
				{
				//         case '\\': // Match with following char
				//                pP++;
				// NO BREAK HERE

				default:
					if (IsCaseSensitive)
						{
						if (text[pT] != p[pP])
							return MATCH_FALSE;
						else
							continue;
						}
					//         if (toupper(text[pT]) != toupper(p[pP]))
					if (DMtable[text[pT]] != DMtable[p[pP]])
						return MATCH_FALSE;
					continue;

				case '?':
					continue;

				case '*':
					if (p[pP] == '*')
						pP++;
					if (p[pP] == '\0')
						return MATCH_TRUE;
					while (text[pT])
						{
						matched = DoMatch(text + pT++, p + pP);
						if (matched != MATCH_FALSE)
							return matched;
						}
					return MATCH_ABORT;

				}
			}
#ifdef MATCH_TAR_PATTERN
		if (text[pT] == '/')
			return MATCH_TRUE;
#endif
		return (text[pT] == '\0');
		}

	inline int DoMatch2(const wchar_t* text, const wchar_t* p, bool IsCaseSensitive = false)
	{
		// DoMatchTable Initialization
		if (DMtable[0x31] != '1')
			PrepareDoMatchTable();

		// probably the MOST DIFFICULT FUNCTION in TurboIRC
		// Thanks to BitchX for copying this function
		//int last;
		int matched;
		//int reverse;
		int pT = 0;
		int pP = 0;

		for (; p[pP] != '\0'; pP++, pT++)
		{
			if (text[pT] == '\0' && p[pP] != '*')
				return MATCH_ABORT;
			switch (p[pP])
			{
				//         case '\\': // Match with following char
				//                pP++;
				// NO BREAK HERE

			default:
				if (IsCaseSensitive || text[pT] > 200 || p[pP] > 200)
				{
					if (text[pT] != p[pP])
						return MATCH_FALSE;
					else
						continue;
				}
				//         if (toupper(text[pT]) != toupper(p[pP]))
				if (DMtable[text[pT]] != DMtable[p[pP]])
					return MATCH_FALSE;
				continue;

			case '?':
				continue;

			case '*':
				if (p[pP] == '*')
					pP++;
				if (p[pP] == '\0')
					return MATCH_TRUE;
				while (text[pT])
				{
					matched = DoMatch2(text + pT++, p + pP);
					if (matched != MATCH_FALSE)
						return matched;
				}
				return MATCH_ABORT;

			}
		}
#ifdef MATCH_TAR_PATTERN
		if (text[pT] == '/')
			return MATCH_TRUE;
#endif
		return (text[pT] == '\0');
	}



	// This will be called from the other funcs
	inline	int VMatching(const char *text, const char *p, bool IsCaseSensitive = false)
		{
#ifdef OPTIMIZE_JUST_STAR
		if (p[0] == '*' && p[1] == '\0')
			return MATCH_TRUE;
#endif
		return (DoMatch(text, p, IsCaseSensitive) == MATCH_TRUE);
		}

	// This will be called from the other funcs
	inline	int VMatching2(const wchar_t* text, const wchar_t* p, bool IsCaseSensitive = false)
	{
#ifdef OPTIMIZE_JUST_STAR
		if (p[0] == '*' && p[1] == '\0')
			return MATCH_TRUE;
#endif
		return (DoMatch2(text, p, IsCaseSensitive) == MATCH_TRUE);
	}



	

