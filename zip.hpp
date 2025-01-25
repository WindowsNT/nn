// 

struct zip_t;
struct ZZIP_ENTRY
{
	std::string name;
	int isdir = 0;
	unsigned long long size = 0;
	unsigned int xcrc32 = 0;

};
class ZZIP
{
private:
	zip_t* z = 0;
public:

	ZZIP(const char* fn, int w = 1);
	ZZIP(void* b,size_t bs);
	void Enum(std::vector< ZZIP_ENTRY>& ze);
	HRESULT Add(const char* n, void* b, size_t sz);
	HRESULT Extract(const char* n, std::vector<char>& d);
	HRESULT ExtractFile(const char* n,const wchar_t* put);
	~ZZIP();
};
