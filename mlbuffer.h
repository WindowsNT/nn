
#pragma once

struct MLBUFFER
{
	CComPtr<ID3D12Resource> b = 0;
	size_t ls = 0;
	size_t offset = 0;

	unsigned long long TensorSizeAlign(unsigned long long t)
	{
		while (t % DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT)
			t++;
		return t;

	}

	~MLBUFFER()
	{
		if (b)
		{

		}
	}

	auto GetOutputDesc() {
		return ee.GetOutputDesc();
	}

	dml::Expression ee;
	HRESULT Create(ID3D12Device* d, dml::Expression e)
	{
		ee = e;
		auto x = TensorSizeAlign(e.GetOutputDesc().totalTensorSizeInBytes);
		return Create2(d, x,false);
	}
	HRESULT Create2(ID3D12Device* d3D12Device, size_t x, [[maybe_unused]] bool ForceInternal);

	CComPtr<ID3D12Resource> uploadBuffer;
	std::vector<char> global_upload;
	UINT64 Upload(ID3D12Device* d3D12Device, ID3D12GraphicsCommandList* commandList, void* data, size_t by, [[maybe_unused]] class ML* ml);


	DML_BUFFER_BINDING dmb = {};

	DML_BINDING_DESC BindingDesc();
	HRESULT Download(ID3D12Device* d3D12Device, ID3D12GraphicsCommandList* commandList, size_t j, class ML* ml, std::vector<char>& out);


};
