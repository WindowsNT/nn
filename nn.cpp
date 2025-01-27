#include "pch.h"

void THROW_IF_FAILED(HRESULT hr)
{
	if (FAILED(hr)) throw;
}


#include "nn.h"


// Not shuffle for debugging
#ifdef _DEBUG
#define ML_DEBUG_1  // no shuffle
#define ML_DEBUG_2 // 6000 only of the set
//#define CLIP_DEBUG
#endif

//#define SINGLE_BUFFER
#ifdef SINGLE_BUFFER
struct MLBUFFER;
std::shared_ptr<MLBUFFER> global_buffer;
#endif

int CurrentBatch = 16;
int NumEpochsX = 10;

#include "mlbuffer.h"


MLBUFFER debugbuffer;

struct MLOP
{
	CComPtr<IDMLCompiledOperator> dmlCompiledOperator;
	CComPtr<IDMLOperatorInitializer> dmlOperatorInitializer;

	HRESULT CreateInitializer(IDMLDevice* dmlDevice)
	{
		std::vector<IDMLCompiledOperator*> dmlCompiledOperators2;
		dmlCompiledOperators2.push_back(dmlCompiledOperator);
		return dmlDevice->CreateOperatorInitializer((UINT)dmlCompiledOperators2.size(), dmlCompiledOperators2.data(), IID_PPV_ARGS(&dmlOperatorInitializer));
	}


	std::vector<DML_BINDING_DESC> bindings_in;
	std::vector<DML_BINDING_DESC> bindings_out;


	DML_BINDING_TABLE_DESC dmlBindingTableDesc{};
	CComPtr<IDMLBindingTable> dmlBindingTable;
	UINT descriptorCount = 0;


	void tape(ID3D12Device* d3D12Device)
	{
		auto bp = dmlCompiledOperator->GetBindingProperties();
		if (bp.TemporaryResourceSize)
		{
			tre.Create2(d3D12Device, bp.TemporaryResourceSize, true);
			auto bu = tre.BindingDesc();
			dmlBindingTable->BindTemporaryResource(&bu);

		}
		if (bp.PersistentResourceSize)
		{
			pre.Create2(d3D12Device, bp.PersistentResourceSize, true);
			auto bu = pre.BindingDesc();
			dmlBindingTable->BindPersistentResource(&bu);
		}
	}

	void tapi(ID3D12Device* d3D12Device)
	{
		auto bp = dmlOperatorInitializer->GetBindingProperties();
		if (bp.TemporaryResourceSize)
		{
			tri.Create2(d3D12Device, bp.TemporaryResourceSize, true);
			auto bu = tri.BindingDesc();
			dmlBindingTable->BindTemporaryResource(&bu);

		}
		if (bp.PersistentResourceSize)
		{
			pri.Create2(d3D12Device, bp.PersistentResourceSize, true);
			auto bu = pri.BindingDesc();
			dmlBindingTable->BindPersistentResource(&bu);
		}

	}


	void TransitionBindings(ID3D12GraphicsCommandList* commandList)
	{
#ifdef SINGLE_BUFFER
		auto x = CD3DX12_RESOURCE_BARRIER::Transition(global_buffer->b, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &x);
#else
		// Transition of the buffers
			for (auto& bu : bindings_in)
			{
				auto buff = ((DML_BUFFER_BINDING*)bu.Desc)->Buffer;
				
				auto x = CD3DX12_RESOURCE_BARRIER::Transition(buff, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				commandList->ResourceBarrier(1, &x);
			}
			for (auto& bu : bindings_out)
			{
				auto buff = ((DML_BUFFER_BINDING*)bu.Desc)->Buffer;
				auto x = CD3DX12_RESOURCE_BARRIER::Transition(buff, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				commandList->ResourceBarrier(1, &x);
			}
#endif
	}

	std::vector<char> UploadData;

	size_t TotalInputBufferSize = 0;
	std::vector<std::tuple<size_t, size_t>> TotalInputRanges;
	size_t AddInputBuffer(size_t Total)
	{
		TotalInputRanges.push_back({ TotalInputBufferSize,Total });
		TotalInputBufferSize += Total;
		return TotalInputRanges.size() - 1;
	}


	size_t TotalOutputBufferSize = 0;
	std::vector<std::tuple<size_t, size_t>> TotalOutputRanges;
	size_t AddOutputBuffer(size_t Total)
	{
		TotalOutputRanges.push_back({ TotalOutputBufferSize,Total });
		TotalOutputBufferSize += Total;
		return TotalOutputRanges.size() - 1;
	};




	void ResetToExecute()
	{
		dmlBindingTableDesc.Dispatchable = dmlCompiledOperator;
		THROW_IF_FAILED(dmlBindingTable->Reset(&dmlBindingTableDesc));
	}


	UINT FindDC()
	{
		auto  initializeBindingProperties = dmlOperatorInitializer->GetBindingProperties();
		auto executeBindingProperties = dmlCompiledOperator->GetBindingProperties();
		descriptorCount = 0;
		descriptorCount = std::max(
			initializeBindingProperties.RequiredDescriptorCount,
			std::max(descriptorCount, executeBindingProperties.RequiredDescriptorCount));
		return descriptorCount;
	}

	void CreateBindingTable(IDMLDevice* dmlDevice, ID3D12DescriptorHeap* descriptorHeap)
	{
		dmlBindingTableDesc.Dispatchable = dmlOperatorInitializer;
		dmlBindingTableDesc.CPUDescriptorHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		dmlBindingTableDesc.GPUDescriptorHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		dmlBindingTableDesc.SizeInDescriptors = descriptorCount;

		THROW_IF_FAILED(dmlDevice->CreateBindingTable(
			&dmlBindingTableDesc,
			IID_PPV_ARGS(&dmlBindingTable)));

	}


	MLBUFFER tri, tre, pri, pre;





};



//#define DEBUGML

class ML
{
public:
	CComPtr<ID3D12Device> d3D12Device;
	CComPtr<IDMLDevice> dmlDevice;
	CComPtr<ID3D12CommandQueue> commandQueue;
	CComPtr<ID3D12CommandAllocator> commandAllocator;
	CComPtr<ID3D12GraphicsCommandList> commandList;
	CComPtr<IDMLCommandRecorder> dmlCommandRecorder;
	DML_BINDING_PROPERTIES initializeBindingProperties = {}, executeBindingProperties = {};
	CComPtr<ID3D12DescriptorHeap> descriptorHeap;
	std::vector<MLOP> ops;

	HRESULT InitializeDirect3D12()
	{

		// Throws if the D3D12 debug layer is missing - you must install the Graphics Tools optional feature
#if defined (_DEBUG)
#ifdef DEBUGML
		CComPtr<ID3D12Debug> d3D12Debug;
		THROW_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&d3D12Debug)));
		d3D12Debug->EnableDebugLayer();
#endif
#endif

		CComPtr<IDXGIFactory4> dxgiFactory;
		CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

		CComPtr<IDXGIAdapter> dxgiAdapter;
		UINT adapterIndex{};
		HRESULT hr{};
		do
		{
			dxgiAdapter = nullptr;
			dxgiAdapter = 0;
			THROW_IF_FAILED(dxgiFactory->EnumAdapters(adapterIndex, &dxgiAdapter));
			++adapterIndex;

			d3D12Device = 0;
			hr = ::D3D12CreateDevice(
				dxgiAdapter,
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&d3D12Device));
			if (hr == DXGI_ERROR_UNSUPPORTED) continue;
			THROW_IF_FAILED(hr);
		} while (hr != S_OK);

		D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

		commandQueue = 0;
		THROW_IF_FAILED(d3D12Device->CreateCommandQueue(
			&commandQueueDesc,
			IID_PPV_ARGS(&commandQueue)));

		commandAllocator = 0;
		THROW_IF_FAILED(d3D12Device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&commandAllocator)));

		commandList = 0;
		THROW_IF_FAILED(d3D12Device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			commandAllocator,
			nullptr,
			IID_PPV_ARGS(&commandList)));

		return S_OK;
	}




	std::map<size_t, MLBUFFER> AllBuffers;

	HRESULT CreateDML()
	{
		DML_CREATE_DEVICE_FLAGS dmlCreateDeviceFlags = DML_CREATE_DEVICE_FLAG_NONE;
#if defined (_DEBUG)
#ifdef DEBUGML
		dmlCreateDeviceFlags |= DML_CREATE_DEVICE_FLAG_DEBUG;
#endif
#endif
		DMLCreateDevice(d3D12Device, dmlCreateDeviceFlags, IID_PPV_ARGS(&dmlDevice));
		if (!dmlDevice)
			return E_FAIL;

		return S_OK;
	}



	dml::Expression ConstantValueTensor(dml::Graph& graph, float what, dml::TensorDesc::Dimensions outputSizes)
	{
		DML_SCALAR_UNION scalar2;
		scalar2.Float32 = what;
		auto zT = dml::FillValueConstant(
			graph, outputSizes,
			DML_TENSOR_DATA_TYPE_FLOAT32,       // Data type
			scalar2
		);
		return zT;
	}


	HRESULT CreateCommandRecorder()
	{
		dmlCommandRecorder = 0;
		return    dmlDevice->CreateCommandRecorder(
			IID_PPV_ARGS(&dmlCommandRecorder));
	}

	void CloseExecuteResetWait()
	{
		THROW_IF_FAILED(commandList->Close());

		ID3D12CommandList* commandLists[] = { commandList };
		commandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

		CComPtr<ID3D12Fence> d3D12Fence;
		THROW_IF_FAILED(d3D12Device->CreateFence(
			0,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&d3D12Fence)));

		auto hfenceEventHandle = ::CreateEvent(nullptr, true, false, nullptr);

		THROW_IF_FAILED(commandQueue->Signal(d3D12Fence, 1));
		THROW_IF_FAILED(d3D12Fence->SetEventOnCompletion(1, hfenceEventHandle));

		::WaitForSingleObjectEx(hfenceEventHandle, INFINITE, FALSE);

		THROW_IF_FAILED(commandAllocator->Reset());
		THROW_IF_FAILED(commandList->Reset(commandAllocator, nullptr));
		CloseHandle(hfenceEventHandle);
	}




	void SetDescriptorHeaps()
	{
		ID3D12DescriptorHeap* d3D12DescriptorHeaps[] = { descriptorHeap };
		commandList->SetDescriptorHeaps(ARRAYSIZE(d3D12DescriptorHeaps), d3D12DescriptorHeaps);
	}

	void CreateHeap()
	{
		// You need to initialize an operator exactly once before it can be executed, and
		// the two stages require different numbers of descriptors for binding. For simplicity,
		// we create a single descriptor heap that's large enough to satisfy them both.
		UINT descriptorCount = 0;
		for (auto& op : ops)
			descriptorCount += op.FindDC();

		if (descriptorCount == 0)
			descriptorCount = 1;
		// Create descriptor heaps.

		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
		descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptorHeapDesc.NumDescriptors = descriptorCount;
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		descriptorHeap = 0;
		THROW_IF_FAILED(d3D12Device->CreateDescriptorHeap(
			&descriptorHeapDesc,
			IID_PPV_ARGS(&descriptorHeap)));

		// Set the descriptor heap(s).
		SetDescriptorHeaps();
	}


	void DoTempAndPersistentBindingInitializer()
	{
		for (auto& op : ops)
		{
			op.tapi(d3D12Device);
		}
	}


	void DoTempAndPersistentBindingExecute()
	{
		for (auto& op : ops)
		{
			op.tape(d3D12Device);
		}

	}



	void Record(int what)
	{
		if (what == 0)
		{
			for (auto& op : ops)
				dmlCommandRecorder->RecordDispatch(commandList, op.dmlOperatorInitializer, op.dmlBindingTable);
		}
		if (what == 1)
		{
			for (auto& op : ops)
				dmlCommandRecorder->RecordDispatch(commandList, op.dmlCompiledOperator, op.dmlBindingTable);
		}
	}
};


DML_BINDING_DESC MLBUFFER::BindingDesc()
{
#ifndef SINGLE_BUFFER
	if (!b)
		throw;
#endif
	DML_BINDING_DESC dbd = {};
	dmb.Buffer = b;
	dmb.Offset = offset;
	dmb.SizeInBytes = ls;
	dbd.Type = DML_BINDING_TYPE_BUFFER;
	dbd.Desc = &dmb;
	return dbd;
}



HRESULT MLBUFFER::Create2(ID3D12Device* d3D12Device, size_t x, [[maybe_unused]] bool ForceInternal)
{
#ifdef SINGLE_BUFFER
	if (ForceInternal == 0)
	{
		bool NC = 0;
		if (global_buffer == 0)
		{
			NC = 1;
			global_buffer = std::make_shared<MLBUFFER>();
			global_buffer->Create2(d3D12Device, TensorSizeAlign(x), true);
		}
		b = global_buffer->b;
		offset = global_buffer->ls;
		if (NC == 0)
			global_buffer->ls += x;
		ls = x;
		return S_OK;
	}
#endif
	b = 0;
	ls = x;
	auto x1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto x2 = CD3DX12_RESOURCE_DESC::Buffer(ls, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	[[maybe_unused]] auto hr = d3D12Device->CreateCommittedResource(
		&x1,
		D3D12_HEAP_FLAG_NONE,
		&x2,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&b));
	return hr;
}


UINT64 MLBUFFER::Upload(ID3D12Device* d3D12Device, ID3D12GraphicsCommandList* commandList, void* data, size_t by, [[maybe_unused]] class ML* ml)
{
	if (!b)
		return 0;
	std::vector<char> bigger_data;

	if (by != ls)
	{
		if (by > ls)
			nop();
		else
		{
			bigger_data.resize(ls);
			memcpy(bigger_data.data(), data, by);
			data = bigger_data.data();
			by = ls;
		}
	}

	auto x1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto x2 = CD3DX12_RESOURCE_DESC::Buffer(ls, D3D12_RESOURCE_FLAG_NONE);
	if (!uploadBuffer)
	{
#ifdef SINGLE_BUFFER
		if (global_buffer->uploadBuffer == 0)
		{
			x2 = CD3DX12_RESOURCE_DESC::Buffer(global_buffer->ls, D3D12_RESOURCE_FLAG_NONE);
			[[maybe_unused]] auto hr = d3D12Device->CreateCommittedResource(
				&x1,
				D3D12_HEAP_FLAG_NONE,
				&x2,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&global_buffer->uploadBuffer));
		}
		uploadBuffer = global_buffer->uploadBuffer;
		global_buffer->global_upload.resize(global_buffer->ls);
#else
		[[maybe_unused]] auto hr = d3D12Device->CreateCommittedResource(
			&x1,
			D3D12_HEAP_FLAG_NONE,
			&x2,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer));
#endif
	}

	// Transition the destination buffer to COPY_DEST
	auto transitionToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
		b,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, // Or current state
		D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &transitionToCopyDest);

#ifdef SINGLE_BUFFER
	memcpy(global_buffer->global_upload.data() + offset, data, by);
	[[maybe_unused]] float* pdd = (float*)global_buffer->global_upload.data();
	D3D12_SUBRESOURCE_DATA tensorSubresourceData{};
	tensorSubresourceData.pData = global_buffer->global_upload.data();
	tensorSubresourceData.RowPitch = static_cast<LONG_PTR>(global_buffer->global_upload.size());
	tensorSubresourceData.SlicePitch = tensorSubresourceData.RowPitch;

	// Upload the input tensor to the GPU.
	auto rv = ::UpdateSubresources(commandList, b, uploadBuffer, 0, 0, 1, &tensorSubresourceData);

#else

	D3D12_SUBRESOURCE_DATA tensorSubresourceData{};
	tensorSubresourceData.pData = data;
	tensorSubresourceData.RowPitch = static_cast<LONG_PTR>(by);
	tensorSubresourceData.SlicePitch = tensorSubresourceData.RowPitch;

	// Upload the input tensor to the GPU.
	auto rv = ::UpdateSubresources(commandList, b, uploadBuffer, offset, 0, 1, &tensorSubresourceData);
#endif

	if (0)
	{
		std::vector<char> d;
#ifdef SINGLE_BUFFER
		global_buffer->Download(d3D12Device, commandList, (size_t)-1, ml, d);
		auto x = CD3DX12_RESOURCE_BARRIER::Transition(global_buffer->b, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
#else
		Download(d3D12Device, commandList, (size_t)-1, ml, d);
		auto x = CD3DX12_RESOURCE_BARRIER::Transition(b, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
#endif
		commandList->ResourceBarrier(1, &x);

		[[maybe_unused]] float* pd = (float*)d.data();
		nop();
	}

	return rv;
}


HRESULT MLBUFFER::Download(ID3D12Device* d3D12Device, ID3D12GraphicsCommandList* commandList, size_t j,ML* ml,std::vector<char>& out)
{
	out.resize(0);
	CComPtr< ID3D12Resource> outputBuffer = b;
#ifdef SINGLE_BUFFER
	outputBuffer = global_buffer->b;
#endif
	if (!outputBuffer)
		return E_NOINTERFACE;

	if (j == (size_t)-1)
		j = ls;
	CComPtr<ID3D12Resource> readbackBuffer;
	auto x7 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
#ifdef SINGLE_BUFFER
	auto x8 = CD3DX12_RESOURCE_DESC::Buffer(global_buffer->ls);
#else
	auto x8 = CD3DX12_RESOURCE_DESC::Buffer(j);
#endif
	auto hr = d3D12Device->CreateCommittedResource(
		&x7,
		D3D12_HEAP_FLAG_NONE,
		&x8,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&readbackBuffer));
	if (FAILED(hr))
		return hr;


	auto x10 = CD3DX12_RESOURCE_BARRIER::Transition(outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &x10);
	commandList->CopyResource(readbackBuffer, outputBuffer);
	if (ml)
		ml->CloseExecuteResetWait();

	D3D12_RANGE tensorBufferRange{ offset, static_cast<SIZE_T>(j) };
	void* outputBufferData = 0;
	hr = readbackBuffer->Map(0, &tensorBufferRange, &outputBufferData);
	if (FAILED(hr))
		return hr;
	out.resize(j);
	memcpy(out.data(),((char*)outputBufferData + offset), out.size());
	return S_OK;

}



void SaveMatrix(Matrix& x, XML3::XMLElement& e)
{
	e.vv("rows").SetValueInt(x.rows());
	e.vv("cols").SetValueInt(x.cols());
	e.vv("data").SetBinaryValue((char*)x.data().data(), x.rows() * x.cols() * sizeof(float));
}

void LoadMatrix(Matrix& x, XML3::XMLElement& e)
{
	x.init(e.vv("rows").GetValueInt(0), e.vv("cols").GetValueInt(0));
	auto bd = e.vv("data").GetBinaryValue();
	memcpy(x.data().data(), bd.GetD().data(), bd.size());
}



Layer::Layer(double lrate, int ActivationType, int num_neurons, int num_weights_per_neuron)
{
	lr = lrate;
	ActType = ActivationType;

	weights.init(num_weights_per_neuron, num_neurons); // 128 neurons, each has 784 weights
	if (num_neurons > 0)
		biases.init(1, num_neurons);

	//		weights.randomize();
}


void Layer::Save(XML3::XMLElement& e)
{
	e.vv("lr").SetValueDouble(lr);
	e.vv("ActType").SetValueInt(ActType);
	SaveMatrix(weights, e.AddElement("weights"));
	SaveMatrix(biases, e.AddElement("biases"));
}
void Layer::Load(XML3::XMLElement& e)
{
	lr = e.vv("lr").GetValueDouble(0.1);
	ActType = e.vv("ActType").GetValueInt(0);
	LoadMatrix(weights, *e.FindElementZ("weights", true));
	LoadMatrix(biases, *e.FindElementZ("biases", true));
}


void NN::RecalculateWB()
{
	for (size_t i = 0; i < Layers.size(); i++)
	{
		if (i == 0)
			continue;
		else
		{
			int num_neurons = Layers[i].weights.cols();
			int prev_out = Layers[i].weights.rows();
			if (i > 1)
				prev_out = Layers[i - 1].weights.cols();
			Layers[i].biases.init(1,num_neurons);
			Layers[i].weights.init(prev_out, num_neurons);
		}
	}
}

void NN::Save(XML3::XMLElement& e)
{
	for (auto& layer : Layers)
	{
		auto& l = e.AddElement("Layer");
		layer.Save(l);
	}
	e.vv("rnnh").SetValueInt(RNN_hidden_size);
}

void NN::Load(XML3::XMLElement& e)
{
	Layers.clear();
	for (auto& layer : e)
	{

		auto wr = layer["weights"].vv("rows").GetValueInt(0);
		auto wc = layer["weights"].vv("cols").GetValueInt(0);
		int numneurons = wc;
		Layer l(0.5, 0, numneurons, wr);
		l.Load(layer);
		Layers.push_back(l);
	}
	RNN_hidden_size = e.vv("rnnh").GetValueInt(32);
}

Matrix NN::ForwardPropagationRNN(Matrix input_sequence) {



	// Assuming input_sequence is of size (T, input_size)
	int T = input_sequence.rows();  // Number of time steps
	Matrix h_prev;
	h_prev.init(RNN_hidden_size, 1); //= Matrix::Zeros(hidden_size, 1); // Initialize h_0 to zeros
	Matrix outputs; // Store outputs if needed

	for (int t = 0; t < T; t++) 
	{
		Matrix x_t = input_sequence.row(t); // Extract input at time step t

		// Compute the new hidden state
/*		Matrix h_t = activation_function(
			(x_t * W_ih) + (h_prev * W_hh) + b_h
		);

		// Optionally compute the output
		Matrix y_t = output_activation_function(
			(h_t * W_ho) + b_o
		);

		// Save the output and update the hidden state
		outputs.append(y_t);
		h_prev = h_t; // Update h_prev for the next time step

		*/
		}

	return outputs; // Return outputs or final hidden state
}



Matrix NN::ForwardPropagation(Matrix input)
{
	// First layer, put the inputs as flattened
	Layers[0].output = input.flatten();

	// Next layers, forward
	for (size_t i = 1; i < Layers.size(); i++)
	{
		auto& current = Layers[i];
		auto& prev = Layers[i - 1];

		if (prev.output.cols() != current.weights.rows())
			current.output = (prev.output.flatten() * current.weights);
		else
			current.output = (prev.output * current.weights);

		current.output += current.biases;
		if (current.ActType == 0)
			current.output.sigmoid();
		else
			current.output.relu();
	}

	return  Layers.back().output;
}

NN::NN()
{
	Layer inputLayer(0.001f, 0, 0, 0);
	Layer outputLayer(0.001f, 0, 10, 128);   

	outputLayer.weights.rand_he();
	for (auto& j : outputLayer.biases.data())
		j = 0.01f;

	Layers.push_back(inputLayer);  // Input layer
	Layers.push_back(outputLayer);  // Output layer

}

void NN::BackPropagation(Matrix label)
{
	auto OurOutput = Layers.back().output; // 1x10

	// Calculation of the derivation of MSE, 2 is ignored for simplicity because it doesn't affect gradient descent
	auto delta = OurOutput - label; // 1x10
	for (int i = (int)(Layers.size() - 1); i > 0; i--)
	{
		Layer& curr = Layers[i];
		Layer& prev = Layers[i - 1];

		// biased += σ'(z) , delta = 1x10, 
		curr.biases += delta * ((float)-curr.lr);

		// weights += prev.Y.T * σ'(z)
		Matrix gradient = (prev.output.transpose() * delta); // 128x10, 784x128

		float clip_value = 5.0f;
		for (auto& value : gradient.data()) {
			value = std::max(-clip_value, std::min(value, clip_value));
		}
		curr.weights += gradient * ((float)-curr.lr);


		// σ'(z) = σ(z) * (1 - σ(z))

		// Sigmoid Derivative
		Matrix der = prev.output; // 1x128, 1x784
		if (prev.ActType == 0)
			der.sigmoid_derivative();
		else
			der.relu_derivative();

		// delta = (delta * prev.W.T) x σ'(z);
		// This multiplication implements the chain rule, combining the loss gradient with the derivative of the activation function for each layer.
		if (i > 1) // don't calculate for the first layer
		{
			auto fd = (delta * curr.weights.transpose());
#ifdef CLIP_DEBUG
			//				MatrixToClipboard(fd);
#endif

			delta = fd.multiply_inplace(der); // 1x128 second time
		}

	}
}

float NN::track_accuracy(std::vector<DataSet>& dataset)
{
	int correct_predictions = 0;

	for (auto& sample : dataset) {
		// Forward propagate to get the network's output
		Matrix output = ForwardPropagation(sample.input);

		// Get predicted label (index of max output value)
		output = output.flatten();
		int predicted_label = output.argmax_rows()[0];

		// Get true label (index of 1 in one-hot encoded label)
		int true_label = sample.label.argmax_rows()[0];

		// Count correct predictions
		if (predicted_label == true_label) {
			correct_predictions++;
		}
	}

	// Compute and display accuracy
	accuracy = (float)correct_predictions / dataset.size() * 100.0f;
	//		std::cout << "Accuracy: " << accuracy << "%" << std::endl;
	return accuracy;
}

void NN::test_network(std::vector<DataSet>& test_dataset) {
	int correct_predictions = 0;

	for (auto& test_sample : test_dataset) {
		// Forward propagate to get the network's output
		Matrix output = ForwardPropagation(test_sample.input);

		// Get predicted label (index of max output value)
		int predicted_label = output.argmax_rows()[0];

		// Get true label (index of 1 in one-hot encoded label)
		int true_label = test_sample.label.argmax_rows()[0];

		// Count correct predictions
		if (predicted_label == true_label) {
			correct_predictions++;
		}
	}

	// Compute and display test accuracy
	accuracy = (float)correct_predictions / test_dataset.size() * 100.0f;
	std::cout << "Test Accuracy: " << accuracy << "%" << std::endl;
}





void TensorDebug(dml::Expression& e)
{
	auto desc = e.GetOutputDesc();
	auto sizetotal = desc.totalTensorSizeInBytes;
	std::cout << "Shape: ";
	for (auto size : desc.sizes) std::cout << size << " ";
	std::cout << std::endl;
	std::cout << "Total size: " << sizetotal << std::endl;
	auto ty = desc.dataType;
	std::cout << "Type: " << ty << std::endl;
	if (desc.strides.has_value())
	{
		std::cout << "Strides: ";
		for (auto size : desc.strides.value()) std::cout << size << " ";
		std::cout << std::endl;
	}
	std::cout << std::endl;
}


void NN::train(std::vector<DataSet> ds, int numEpochs, bool AndAccuracy)
{
	auto rng = std::default_random_engine{};
	HRESULT hr = S_OK;
	for (int i = 0; i < numEpochs; i++)
	{
#ifndef ML_DEBUG_1
		std::shuffle(std::begin(ds), std::end(ds), rng);
#endif
		if (cbf)
			hr = cbf(i, -1, 0, 0, accuracy, paramcbf);
		if (FAILED(hr))
			return;
		float totalLoss = 0.0;  // Initialize total loss
		for (size_t y = 0; y < ds.size(); y++)
		{
			if (y > 0)
				totalLoss += error(Layers.back().output, ds[y].label);
			if (cbf)
				hr = cbf(i, (int)y, 1, totalLoss, accuracy, paramcbf);
			if (FAILED(hr))
				return;
			ForwardPropagation(ds[y].input);
			if (cbf)
				hr = cbf(i, (int)y, 2, totalLoss, accuracy, paramcbf);
			if (FAILED(hr))
				return;
			BackPropagation(ds[y].label);
#ifdef CLIP_DEBUG
			for (size_t il = 0; il < Layers.size(); il++)
			{
				auto& l = Layers[il];
				MatrixToClipboard(l.weights);
				MatrixToClipboard(l.biases);
				MatrixToClipboard(l.output);
			}
#endif
			if (cbf)
				hr = cbf(i, (int)y, 3, totalLoss, accuracy, paramcbf);
			if (FAILED(hr))
				return;
		}
		if (cbf)
			hr = cbf(i, -1, 4, totalLoss / ds.size(), accuracy, paramcbf);
		if (FAILED(hr))
			return;

		if (AndAccuracy)
		{
			float acc = track_accuracy(ds);
			if (cbf)
				hr = cbf(i, -1, 5, totalLoss / ds.size(), acc, paramcbf);
			if (FAILED(hr))
				return;

		}

		// Print average loss for the epoch
//			std::cout << "Epoch " << i + 1 << " completed, Average Loss: "
//				<< totalLoss / ds.size() << "\n";
			// Track accuracy after each epoch
	}


}


void NN::MatrixToClipboard(ID3D12Device* dev, ID3D12GraphicsCommandList* commandLit, ML* ml, MLBUFFER& m)
{
	std::vector<char> temp;
	m.Download(dev, commandLit, m.ls, ml, temp);
	if (temp.empty())
		return;
	Matrix output;
	output.init(m.GetOutputDesc().sizes[2], m.GetOutputDesc().sizes[3]);

	memcpy(output.data().data(), temp.data(), output.data().size() * 4);
	MatrixToClipboard(output);
}

void NN::MatrixToClipboard(Matrix& m)
{
	std::string str;
	for (int i = 0; i < m.rows(); i++)
	{
		for (int j = 0; j < m.cols(); j++)
		{
			str += std::to_string(m.at(i, j)) + "\t";
		}
		str += "\n";
	}
	// Copy to clipboard
	const size_t len = str.length() + 1;
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
	memcpy(GlobalLock(hMem), str.c_str(), len);
	GlobalUnlock(hMem);
	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CF_TEXT, hMem);
	CloseClipboard();
}



HRESULT NN::dmltrain3(std::vector<DataSet> data, unsigned int batch, int numEpochs)
{
	CoInitialize(0);
	auto rng = std::default_random_engine{};
	std::vector<char> temp;
	ML ml;
	HRESULT hr = 0;
	hr = ml.InitializeDirect3D12();
	hr = ml.CreateDML();




	// Forward and Bckward Propagation Operator
	MLBUFFER B_InputOp1;
	MLBUFFER B_InputOp2;
	MLBUFFER B_Final;
	MLBUFFER B_Label;
	dml::Graph graph1(ml.dmlDevice);
	dml::Graph graph2(ml.dmlDevice);
	int nexttid = 0;

	/*
			// Single operator, Single Graph
			// This doesn't work because output buffers overlap with input and parallelism can't be done

			ml.ops.push_back(GetTrainOperator(ml, batch, nexttid,data,B_InputOp1,B_Label,B_Final,true,true,&graph1,&graph1));
	*/

			// Two Operators, Two graphs
	ml.ops.push_back(GetTrainOperator(ml, batch, nexttid, data, B_InputOp1, B_InputOp2, B_Label, B_Final, true, false, &graph1, 0));
	nexttid = 0;
	ml.ops.push_back(GetTrainOperator(ml, batch, nexttid, data, B_InputOp1, B_InputOp2, B_Label, B_Final, false, true, 0, &graph2));

	/*
			// Two operators, one graph
			ml.ops.push_back(GetTrainOperator(ml, batch, nexttid,data, B_InputOp1, B_Label, B_Final, true, false,&graph1,0));
			ml.ops.push_back(GetTrainOperator(ml, batch, nexttid,data, B_InputOp1, B_Label, B_Final, false, true,0, &graph1));
	*/

	// Initialize
	for (auto& op : ml.ops)
		op.CreateInitializer(ml.dmlDevice);

	// https://learn.microsoft.com/en-us/windows/ai/directml/dml-binding
	// Query the operator for the required size (in descriptors) of its binding table.
	ml.CreateHeap();

	// Create a binding table over the descriptor heap we just created.
	for (auto& op : ml.ops)
		op.CreateBindingTable(ml.dmlDevice, ml.descriptorHeap);

	// Bind Temporary and 
	ml.DoTempAndPersistentBindingInitializer();

	// The command recorder is a stateless object that records Dispatches into an existing Direct3D 12 command list.
	hr = ml.CreateCommandRecorder();

	// Record execution of the operator initializer.
	ml.Record(0);

	// Execute it
	ml.CloseExecuteResetWait();



	bool MustBind = 1;
	if (MustBind)
	{
#ifdef SINGLE_BUFFER
		// Swap all the buffers
		auto globalbs = global_buffer->ls;
		global_buffer = 0;
		global_buffer = std::make_shared<MLBUFFER>();
		global_buffer->Create2(ml.d3D12Device, TensorSizeAlign(globalbs), true);
		for (auto& op : ml.ops)
		{
			for (auto& bi : op.bindings_in)
			{
				auto de = (DML_BUFFER_BINDING*)bi.Desc;
				de->Buffer = global_buffer->b;
			}
			for (auto& bi : op.bindings_out)
			{
				auto de = (DML_BUFFER_BINDING*)bi.Desc;
				de->Buffer = global_buffer->b;
			}
		}
#endif
	}


	// Upload once initial weights/biases/input
	if (1)
	{
		for (auto& l : Layers)
		{
			if (l.B_Biases.b)
				l.B_Biases.Upload(ml.d3D12Device, ml.commandList, l.biases.data().data(), l.biases.data().size() * 4, &ml);
			if (l.B_Weights.b)
				l.B_Weights.Upload(ml.d3D12Device, ml.commandList, l.weights.data().data(), l.weights.data().size() * 4, &ml);
		}
	}


	for (int iEpoch = 0; iEpoch < numEpochs; iEpoch++)
	{
#ifndef ML_DEBUG_1
		std::shuffle(std::begin(data), std::end(data), rng);
#endif

		float totalloss = 0;
		for (size_t iData = 0; iData < data.size(); iData += batch)
		{
			// callback
/*				if (iData > 0)
				{
					auto& bl = Layers.back();
					int rows = bl.B_Outputs.GetOutputDesc().sizes[2];
					int cols = bl.B_Outputs.GetOutputDesc().sizes[3];
					if (bl.output.rows() != rows || bl.output.cols() != cols)
						bl.output.init(rows, cols);
					bl.B_Outputs.Download(ml.d3D12Device, ml.commandList, bl.B_Outputs.ls, &ml, temp);
					bl.output.init(rows, cols);
					int size = rows * cols * 4;
					memcpy(bl.output.data().data(), temp.data(), size);
					totalloss += error(Layers.back().output, data[iData].label);
				}
*/

			if (cbf)
				hr = cbf((int)iEpoch, (int)iData, 1, totalloss, accuracy, paramcbf);
			if (FAILED(hr))
				return E_ABORT;


			// Bind and execute the operator on the GPU.
			ml.SetDescriptorHeaps();

			// Input + Data uploading
			if (batch > 1)
			{
				std::vector<float> fullinput;
				for (unsigned int i = 0; i < batch; i++)
				{
					auto& d = data[iData + i];
					fullinput.insert(fullinput.end(), d.input.data().begin(), d.input.data().end());
				}
				B_InputOp1.Upload(ml.d3D12Device, ml.commandList, fullinput.data(), fullinput.size() * 4, &ml);
				if (B_Label.b)
				{
					std::vector<float> fulllabel;
					for (unsigned int i = 0; i < batch; i++)
					{
						auto& d = data[iData + i];
						fulllabel.insert(fulllabel.end(), d.label.data().begin(), d.label.data().end());
					}
					B_Label.Upload(ml.d3D12Device, ml.commandList, fulllabel.data(), fulllabel.size() * 4, &ml);
				}
			}
			else
			{
				B_InputOp1.Upload(ml.d3D12Device, ml.commandList, data[iData].input.data().data(), data[iData].input.data().size() * 4, &ml);
				if (B_Label.b)
					B_Label.Upload(ml.d3D12Device, ml.commandList, data[iData].label.data().data(), data[iData].label.data().size() * 4, &ml);
			}

			// Operator 1
			if (1)
			{
				auto& op = ml.ops[0];
				op.ResetToExecute();
				op.TransitionBindings(ml.commandList);

				// Binding
				op.dmlBindingTable->BindInputs((UINT)op.bindings_in.size(), op.bindings_in.data());
				op.dmlBindingTable->BindOutputs((UINT)op.bindings_out.size(), op.bindings_out.data());

				// And temporary/persistent resources
				op.tape(ml.d3D12Device);

				// And run it
				ml.dmlCommandRecorder->RecordDispatch(ml.commandList, op.dmlCompiledOperator, op.dmlBindingTable);
				ml.CloseExecuteResetWait();
				ml.SetDescriptorHeaps();
			}


			// Operator 2
			if (1)
			{
				auto& op = ml.ops[1];
				op.ResetToExecute();
				op.TransitionBindings(ml.commandList);

				op.dmlBindingTable->BindInputs((UINT)op.bindings_in.size(), op.bindings_in.data());
				op.dmlBindingTable->BindOutputs((UINT)op.bindings_out.size(), op.bindings_out.data());

				// And temporary/persistent resources
				op.tape(ml.d3D12Device);

				// And run it
				ml.dmlCommandRecorder->RecordDispatch(ml.commandList, op.dmlCompiledOperator, op.dmlBindingTable);
				ml.CloseExecuteResetWait();
				ml.SetDescriptorHeaps();
			}

			MustBind = 0;

#ifdef CLIP_DEBUG
			//				MatrixToClipboard(ml.d3D12Device, ml.commandList, &ml, debugbuffer);
			//				MatrixToClipboard(ml.d3D12Device, ml.commandList, &ml, Layers.back().B_Outputs);
#endif

				// Update Biases/Weights
			float* pt = 0;
			for (size_t ii = 0; ii < Layers.size(); ii++)
			{
				if (ii == 0)
					continue;
				auto& layer = Layers[ii];
				if (layer.B_BiasesOut.b)
				{
					layer.B_BiasesOut.Download(ml.d3D12Device, ml.commandList, layer.B_BiasesOut.ls, &ml, temp);
					pt = (float*)temp.data();
					layer.B_Biases.Upload(ml.d3D12Device, ml.commandList, (float*)temp.data(), temp.size(), &ml);
				}
				if (layer.B_WeightsOut.b)
				{
					layer.B_WeightsOut.Download(ml.d3D12Device, ml.commandList, layer.B_WeightsOut.ls, &ml, temp);
					pt = (float*)temp.data();
					layer.B_Weights.Upload(ml.d3D12Device, ml.commandList, (float*)temp.data(), temp.size(), &ml);
				}
			}


#ifdef CLIP_DEBUG
			for (size_t il = 0; il < Layers.size(); il++)
			{
				auto& l = Layers[il];
				MatrixToClipboard(ml.d3D12Device, ml.commandList, &ml, l.B_Weights);
				MatrixToClipboard(ml.d3D12Device, ml.commandList, &ml, l.B_Biases);
				MatrixToClipboard(ml.d3D12Device, ml.commandList, &ml, l.B_Outputs);
			}
#endif


			// TEST after execution
#ifdef DEBUGML
				//debugbuffer.Download(ml.d3D12Device, ml.commandList, (size_t)-1, &ml, temp);
//				pt = (float*)temp.data();
//				B_Final.Download(ml.d3D12Device, ml.commandList, (size_t)-1, &ml, temp);
//				pt = (float*)temp.data();
			for (size_t i = 0; i < Layers.size(); i++)
			{
				auto& l = Layers[i];
				l.B_Outputs.Download(ml.d3D12Device, ml.commandList, (size_t)-1, &ml, temp);
				pt = (float*)temp.data();
				if (l.B_Biases.b)
					l.B_Biases.Download(ml.d3D12Device, ml.commandList, (size_t)-1, &ml, temp);
				pt = (float*)temp.data();
				if (l.B_Weights.b)
					l.B_Weights.Download(ml.d3D12Device, ml.commandList, (size_t)-1, &ml, temp);
				pt = (float*)temp.data();
			}
#endif
		}



		// Epoch end
		for (auto& l : Layers)
		{
			// Outputs 
			if (l.B_Outputs.b)
			{
				l.B_Outputs.Download(ml.d3D12Device, ml.commandList, l.B_Outputs.ls, &ml, temp);
				if ((unsigned int)l.output.rows() != l.B_Outputs.GetOutputDesc().sizes[2] || (unsigned int)l.output.cols() != l.B_Outputs.GetOutputDesc().sizes[3])
					l.output.init(l.B_Outputs.GetOutputDesc().sizes[2], l.B_Outputs.GetOutputDesc().sizes[3]);
				memcpy(l.output.data().data(), temp.data(), l.output.cols() * l.output.rows() * 4);
			}

			// Biases
			if (l.B_BiasesOut.b)
			{
				l.B_BiasesOut.Download(ml.d3D12Device, ml.commandList, l.B_Biases.ls, &ml, temp);
				if ((unsigned int)l.biases.rows() != l.B_BiasesOut.GetOutputDesc().sizes[2] || (unsigned int)l.biases.cols() != l.B_BiasesOut.GetOutputDesc().sizes[3])
					l.biases.init(l.B_BiasesOut.GetOutputDesc().sizes[2], l.B_BiasesOut.GetOutputDesc().sizes[3]);
				memcpy(l.biases.data().data(), temp.data(), l.biases.cols() * l.biases.rows() * 4);
			}

			// Weights
			if (l.B_WeightsOut.b)
			{
				l.B_Weights.Download(ml.d3D12Device, ml.commandList, l.B_Weights.ls, &ml, temp);
				if ((unsigned int)l.weights.rows() != l.B_Weights.GetOutputDesc().sizes[2] || (unsigned int)l.weights.cols() != l.B_Weights.GetOutputDesc().sizes[3])
					l.weights.init(l.B_Weights.GetOutputDesc().sizes[2], l.B_Weights.GetOutputDesc().sizes[3]);
				memcpy(l.weights.data().data(), temp.data(), l.weights.cols() * l.weights.rows() * 4);
			}

		}


		track_accuracy(data);
	}



	return S_OK;
}


MLOP NN::GetTrainOperator(ML& ml, unsigned int batch, int& starttid, std::vector<DataSet>& data, MLBUFFER& B_InputOp1, MLBUFFER& B_InputOp2, MLBUFFER& B_Label, MLBUFFER& B_Final, bool FP, bool BP, dml::Graph* g1, dml::Graph* g2)
{

	// Forward Propagation Operator
	int& nexttid = starttid;
	std::vector<DML_BINDING_DESC> bindings_in;
	std::vector<DML_BINDING_DESC> bindings_out;
	std::vector<dml::Expression> outputs;


	bool RNN = 0;
	unsigned int hidden_units = 128;
	MLBUFFER B_HiddenState;
	unsigned int time_steps = 28;

	// Input Tensor
	if (FP)
	{
		dml::Graph& graph1 = *g1;
		B_InputOp1.Create(ml.d3D12Device, dml::InputTensor(graph1, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, { batch,1,1,(unsigned int)data[0].input.cols() * data[0].input.rows()} }));
		bindings_in.push_back(B_InputOp1.BindingDesc());

		if (RNN)
		{
			B_HiddenState.Create(ml.d3D12Device, dml::InputTensor(graph1, nexttid++,
				{ DML_TENSOR_DATA_TYPE_FLOAT32, { batch, 1, 1, hidden_units } }));
			bindings_in.push_back(B_HiddenState.BindingDesc());
		}

		// Weights and Biases
		for (size_t i = 1; i < Layers.size() && Layers.size() >= 3; i++)
		{
			auto& layer = Layers[i];
			layer.B_Weights.Create(ml.d3D12Device, (dml::InputTensor(graph1, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, { 1,1,(unsigned int)layer.weights.rows(),(unsigned int)layer.weights.cols()} })));
			bindings_in.push_back(layer.B_Weights.BindingDesc());

			// If MNIST Hidden, [batch,1,784,128] 401408
			// If MNIST Output, [batch,1,128,10] 5120

			layer.B_Biases.Create(ml.d3D12Device, (dml::InputTensor(graph1, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, { 1,1,1,(unsigned int)layer.biases.cols()} })));
			bindings_in.push_back(layer.B_Biases.BindingDesc());

			// If MNIST Hidden, [batch,1,1,128] 512
			// If MNIST Output, [batch,1,1,10] 40
		}

		// Create Forward Propagation Operators
		for (int i = 0; i < Layers.size(); i++)
		{
			auto& layer = Layers[i];
			if (i == 0) // input layer -> first hidden
			{
				layer.B_Outputs.Create(ml.d3D12Device, dml::Identity(B_InputOp1.ee));
				bindings_out.push_back(layer.B_Outputs.BindingDesc());
				// If MNIST, [1,1,1,784] 3136
			}
			else
			{
				auto& pl = Layers[i - 1];
				std::vector<dml::Expression> batchOutputs;

				for (unsigned int b = 0; b < batch; ++b)
				{
					// Extract the b-th sample from the batch
					auto singleInput = dml::Slice(pl.B_Outputs.ee, { b, 0, 0, 0 }, { 1, 1, 1, pl.B_Outputs.ee.GetOutputDesc().sizes[3] }, { 1, 1, 1, 1 });

					// Perform forward propagation for this single sample
					auto mul1 = dml::Gemm(singleInput, layer.B_Weights.ee);
					auto add1 = dml::Add(mul1, layer.B_Biases.ee);
					auto output = layer.ActType == 1 ? dml::ActivationRelu(add1) : dml::ActivationSigmoid(add1);

					batchOutputs.push_back(output);
				}
				if (batch == 1)
					layer.B_Outputs.Create(ml.d3D12Device, batchOutputs[0]);
				else
					layer.B_Outputs.Create(ml.d3D12Device, dml::Join(batchOutputs, 0));

				bindings_out.push_back(layer.B_Outputs.BindingDesc());
				// If MNIST Hidden, [batch,1,1,128] 512
				// If MNIST Output, [batch,1,1,10] 40
			}
			// We want the outputs as output
			outputs.push_back(layer.B_Outputs.ee);
		}

		// Create the operator
		if (BP == 0)
		{
			auto OutputCompiledOperator2 = graph1.Compile(DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION_COMPUTATION, outputs);
			MLOP op2;
			op2.dmlCompiledOperator.Attach(OutputCompiledOperator2.Detach());
			op2.bindings_out = bindings_out;
			op2.bindings_in = bindings_in;
			return op2;
		}
	}


	if (BP)
	{
		dml::Graph& graph2 = *g2;
		auto& OutOutput = B_InputOp2;

		// Input Tensor
		OutOutput.ee = FP ? dml::Identity(Layers.back().B_Outputs.ee) : dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, Layers.back().B_Outputs.GetOutputDesc().sizes });
		if (!FP)
			bindings_in.push_back(Layers.back().B_Outputs.BindingDesc());

		// Label Tensor
		B_Label.Create(ml.d3D12Device, dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, {batch,1,1,10} }));
		bindings_in.push_back(B_Label.BindingDesc());

		// Delta Loop
		auto T_DeltaFull = OutOutput.ee - B_Label.ee;

#if 1
		std::vector<dml::Expression> batchGradients(Layers.size());
		std::vector<dml::Expression> batchDeltas(Layers.size());

		// Pass 0: Create input the tensors
		for (int i = (int)(Layers.size() - 1); i >= 0; i--)
		{
			Layer& curr = Layers[i];
			if (i == 0)
				break;

			Layer& prev = Layers[i - 1];
			if (prev.T_OutputsBPIn.Impl() == 0)
			{
				prev.T_OutputsBPIn = (dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, prev.B_Outputs.GetOutputDesc().sizes }));
				bindings_in.push_back(prev.B_Outputs.BindingDesc());
			}


			if (curr.BiasesBPIn.ee.Impl() == 0)
			{
				curr.BiasesBPIn.ee = (dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, curr.B_Biases.ee.GetOutputDesc().sizes }));
				bindings_in.push_back(curr.B_Biases.BindingDesc());
			}
			if (curr.WeightsBPIn.ee.Impl() == 0)
			{
				curr.WeightsBPIn.ee = (dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32,curr.B_Weights.ee.GetOutputDesc().sizes }));
				bindings_in.push_back(curr.B_Weights.BindingDesc());
			}
		}

		// Extract the slice for the current sample
		for (unsigned int iBatch = 0; iBatch < batch; iBatch++)
		{
			//				auto T_Delta1 = dml::Slice(T_DeltaFull, { iBatch, 0, 0, 0 }, T_DeltaFull.GetOutputDesc().sizes, { 1, 1, 1, 1 });
			auto T_Delta1 = (batch == 1) ? T_DeltaFull : dml::Slice(T_DeltaFull, { iBatch, 0, 0, 0 }, { 1, 1, T_DeltaFull.GetOutputDesc().sizes[2], T_DeltaFull.GetOutputDesc().sizes[3] }, { 1, 1, 1, 1 });


			// Pass 1: Calculate gradients
			for (int i = (int)(Layers.size() - 1); i >= 0; i--)
			{
				Layer& curr = Layers[i];
				if (i == 0)
				{
					if (batchDeltas[i].Impl() == 0)
						batchDeltas[i] = T_Delta1;
					else
						batchDeltas[i] = batchDeltas[i] + T_Delta1;
					break;
				}
				Layer& prev = Layers[i - 1];
				// Matrix gradient = (prev.output.transpose() * delta); // 128x10, 784x128

				auto PrevO = (batch == 1) ? prev.T_OutputsBPIn : dml::Slice(prev.T_OutputsBPIn, { iBatch, 0, 0, 0 }, { 1, 1, prev.T_OutputsBPIn.GetOutputDesc().sizes[2], prev.T_OutputsBPIn.GetOutputDesc().sizes[3] }, { 1, 1, 1, 1 });
				auto bp_gradientunclipped = dml::Gemm(PrevO, T_Delta1, {}, DML_MATRIX_TRANSFORM_TRANSPOSE, DML_MATRIX_TRANSFORM_NONE); // 128x10, 784x128 for MNIST
				// Clip the gradient
				// float clip_value = 5.0f; 				for (auto& value : gradient.data()) { 						value = std::max(-clip_value, std::min(value, clip_value)); } 	
				auto bp_clippedgradient = dml::Clip(bp_gradientunclipped, -5.0f, 5.0f);

				if (batchGradients[i].Impl() == 0)
					batchGradients[i] = bp_clippedgradient;
				else
					batchGradients[i] = batchGradients[i] + bp_clippedgradient;


				// And the derivative
				auto bp_der1 = dml::Identity(PrevO);

				dml::Expression bp_der2;
				if (prev.ActType == 0)
				{
					// Sigmoid Derivative
					//						for (size_t i = 0; i < _data.size(); i++) {							_data[i] = _data[i] * (1 - _data[i]);							}						
					auto bp_dx = dml::ActivationSigmoid(bp_der1);
					auto bp_one1 = ml.ConstantValueTensor(graph2, 1.0f, bp_dx.GetOutputDesc().sizes);
					auto bp_oneMinusSigmoid = bp_one1 - bp_dx;
					bp_der2 = bp_dx * bp_oneMinusSigmoid;
				}
				else
				{
					// Relu derivative
					// for (size_t i = 0; i < _data.size(); i++) {							_data[i] = _data[i] > 0 ? (matrix_t)1 : (matrix_t)0;						}
					auto bp_dx = dml::ActivationRelu(bp_der1);
					auto bp_zeroTensor = ml.ConstantValueTensor(graph2, 0.0f, bp_dx.GetOutputDesc().sizes);
					auto bp_reluMask = dml::GreaterThan(bp_dx, bp_zeroTensor);
					bp_der2 = dml::Cast(bp_reluMask, DML_TENSOR_DATA_TYPE_FLOAT32);
				}

				auto bp_Delta2 = dml::Gemm(T_Delta1, curr.WeightsBPIn.ee, {}, DML_MATRIX_TRANSFORM_NONE, DML_MATRIX_TRANSFORM_TRANSPOSE);
				auto T_Delta2 = bp_Delta2 * bp_der2;
				if (batchDeltas[i].Impl() == 0)
					batchDeltas[i] = T_Delta1;
				else
					batchDeltas[i] = batchDeltas[i] + T_Delta1;
				T_Delta1 = T_Delta2;

			}
		}


		// Accumulate gradients
		if (batch > 1)
		{
			for (int i = 1; i < Layers.size(); i++) {
				batchGradients[i] = batchGradients[i] * (1.0f / batch);
			}
		}
		// and deltas
		if (batch > 1)
		{
			for (int i = 1; i < Layers.size(); i++) {
				batchDeltas[i] = batchDeltas[i] * (1.0f / batch);
			}
		}

		// Phase 2 : Update Biases / Weights
		for (int i = (int)(Layers.size() - 1); i >= 0; i--)
		{
			Layer& curr = Layers[i];
			auto T_Delta1 = batchDeltas[i];

			if (i == 0)
			{
				B_Final.Create(ml.d3D12Device, dml::Identity(T_Delta1));
				bindings_out.push_back(B_Final.BindingDesc());
				break;
			}

			// 	curr.biases += delta * ((float)-curr.lr);
			auto bp_mincurrlr = ml.ConstantValueTensor(graph2, (float)-curr.lr, T_Delta1.GetOutputDesc().sizes);
			auto bp_mul2 = T_Delta1 * bp_mincurrlr;
			if (curr.BiasesBPIn.ee.Impl() == 0)
			{
				curr.BiasesBPIn.ee = (dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, curr.B_Biases.ee.GetOutputDesc().sizes }));
				bindings_in.push_back(curr.B_Biases.BindingDesc());
			}
			curr.B_BiasesOut.Create(ml.d3D12Device, curr.BiasesBPIn.ee + bp_mul2);
			bindings_out.push_back(curr.B_BiasesOut.BindingDesc());


			// curr.weights += gradient * ((float)-curr.lr);
			auto bp_mincurrlr2 = ml.ConstantValueTensor(graph2, (float)-curr.lr, batchGradients[i].GetOutputDesc().sizes);
			auto bp_mul3 = batchGradients[i] * bp_mincurrlr2;
			if (curr.WeightsBPIn.ee.Impl() == 0)
			{
				curr.WeightsBPIn.ee = (dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32,curr.B_Weights.ee.GetOutputDesc().sizes }));
				bindings_in.push_back(curr.B_Weights.BindingDesc());
			}
			curr.B_WeightsOut.Create(ml.d3D12Device, curr.WeightsBPIn.ee + bp_mul3);
			bindings_out.push_back(curr.B_WeightsOut.BindingDesc());
		}

#else
		auto T_Delta1 = T_DeltaFull;
		for (int i = (int)(Layers.size() - 1); i >= 0; i--)
		{
			Layer& curr = Layers[i];
			if (i == 0)
			{
				B_Final.Create(ml.d3D12Device, dml::Identity(T_Delta1));
				bindings_out.push_back(B_Final.BindingDesc());
				break;
			}

			Layer& prev = Layers[i - 1];

			// 	curr.biases += delta * ((float)-curr.lr);
			auto bp_mincurrlr = ml.ConstantValueTensor(graph2, (float)-curr.lr, T_Delta1.GetOutputDesc().sizes);
			auto bp_mul2 = T_Delta1 * bp_mincurrlr;
			if (curr.BiasesBPIn.ee.Impl() == 0)
			{
				curr.BiasesBPIn.ee = (dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, curr.B_Biases.ee.GetOutputDesc().sizes }));
				bindings_in.push_back(curr.B_Biases.BindingDesc());
			}
			curr.B_BiasesOut.Create(ml.d3D12Device, curr.BiasesBPIn.ee + bp_mul2);
			bindings_out.push_back(curr.B_BiasesOut.BindingDesc());


			// Matrix gradient = (prev.output.transpose() * delta); // 128x10, 784x128
			if (prev.T_OutputsBPIn.Impl() == 0)
			{
				prev.T_OutputsBPIn = (dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, prev.B_Outputs.GetOutputDesc().sizes }));
				bindings_in.push_back(prev.B_Outputs.BindingDesc());
			}
			auto bp_gradientunclipped = dml::Gemm(prev.T_OutputsBPIn, T_Delta1, {}, DML_MATRIX_TRANSFORM_TRANSPOSE, DML_MATRIX_TRANSFORM_NONE); // 128x10, 784x128 for MNIST

			// Clip the gradient
			// float clip_value = 5.0f; for (auto& value : gradient.data()) { value = std::max(-clip_value, std::min(value, clip_value));} 
			auto bp_clippedgradient = dml::Clip(bp_gradientunclipped, -5.0f, 5.0f);

			// curr.weights += gradient * ((float)-curr.lr);
			auto bp_mincurrlr2 = ml.ConstantValueTensor(graph2, (float)-curr.lr, bp_clippedgradient.GetOutputDesc().sizes);
			auto bp_mul3 = bp_clippedgradient * bp_mincurrlr2;
			if (curr.WeightsBPIn.ee.Impl() == 0)
			{
				curr.WeightsBPIn.ee = (dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32,curr.B_Weights.ee.GetOutputDesc().sizes }));
				bindings_in.push_back(curr.B_Weights.BindingDesc());
			}
			curr.B_WeightsOut.Create(ml.d3D12Device, curr.WeightsBPIn.ee + bp_mul3);
			bindings_out.push_back(curr.B_WeightsOut.BindingDesc());


			// der = derivative of activation
			// delta = (delta * curr.weights.transpose()).multiply_inplace(der);

			// 1,1,128,10, 1,1,784,128
			auto bp_der1 = dml::Identity(prev.T_OutputsBPIn);

			dml::Expression bp_der2;
			if (prev.ActType == 0)
			{
				// Sigmoid Derivative 						for (size_t i = 0; i < _data.size(); i++) {								_data[i] = _data[i] * (1 - _data[i]);							}
				auto bp_dx = dml::ActivationSigmoid(bp_der1);
				auto bp_one1 = ml.ConstantValueTensor(graph2, 1.0f, bp_dx.GetOutputDesc().sizes);
				auto bp_oneMinusSigmoid = bp_one1 - bp_dx;
				bp_der2 = bp_dx * bp_oneMinusSigmoid;
			}
			else
			{
				// Relu derivative
				// for (size_t i = 0; i < _data.size(); i++) {							_data[i] = _data[i] > 0 ? (matrix_t)1 : (matrix_t)0;						}
				auto bp_dx = dml::ActivationRelu(bp_der1);
				auto bp_zeroTensor = ml.ConstantValueTensor(graph2, 0.0f, bp_dx.GetOutputDesc().sizes);
				auto bp_reluMask = dml::GreaterThan(bp_dx, bp_zeroTensor);
				bp_der2 = dml::Cast(bp_reluMask, DML_TENSOR_DATA_TYPE_FLOAT32);
			}


			// Calculate delta again
			auto bp_Delta2 = dml::Gemm(T_Delta1, curr.B_WeightsOut.ee, {}, DML_MATRIX_TRANSFORM_NONE, DML_MATRIX_TRANSFORM_TRANSPOSE);
			T_Delta1 = bp_Delta2 * bp_der2;
		}
#endif


		// Create the operator
		for (int i = (int)(Layers.size() - 1); i >= 0; i--)
		{
			auto& l = Layers[i];
			if (l.B_BiasesOut.b)
				outputs.push_back(l.B_BiasesOut.ee);
			if (l.B_WeightsOut.b)
				outputs.push_back(l.B_WeightsOut.ee);
		}
		if (B_Final.ee)
			outputs.push_back(B_Final.ee);
		/*
		if (1)
			{
				bindings_out.push_back(debugbuffer.BindingDesc());
				outputs.push_back(debugbuffer.ee);
			}
		*/

		auto OutputCompiledOperator2 = graph2.Compile(DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION_COMPUTATION, outputs);
		MLOP op2;
		op2.dmlCompiledOperator.Attach(OutputCompiledOperator2.Detach());
		op2.bindings_out = bindings_out;
		op2.bindings_in = bindings_in;
		return op2;
	}


	return {};
}



std::vector<DataSet> parse_mnist(const std::vector<unsigned char>& images,
	const std::vector<unsigned char>& labels,
	int numSamples)
{
	const int imageSize = 28 * 28;  // Each image is 28x28 pixels
	const int imageHeaderSize = 16; // Header size for images file
	const int labelHeaderSize = 8;  // Header size for labels file

	std::vector<DataSet> dataset;

	for (int i = 0; i < numSamples; i++)
	{
		// Parse image data (skip header)
		Matrix img(28, 28);  // Assuming Matrix is a class with (rows, cols) constructor
		int imageStart = imageHeaderSize + i * imageSize;

		for (int row = 0; row < 28; row++) {
			for (int col = 0; col < 28; col++) {
				img.set(row, col, images[imageStart + row * 28 + col] / 255.0f);  // Normalize to [0, 1]
			}
		}

		// Parse label data (skip header)
		int label = labels[labelHeaderSize + i];
		Matrix lbl(1, 10, 0);  // One-hot encode label: 1 row, 10 columns, initialized to 0
		lbl.set(0, label, 1.0f);  // Set the correct label index to 1

		// Create a DataSet object and add it to the vector
		DataSet data;
		data.input = img;
		data.label = lbl;
		dataset.push_back(data);
	}

	return dataset;
}
bool LoadFile(const wchar_t* f, std::vector<unsigned char>& d);

extern std::wstring fil;
void PROJECT::Save(XML3::XML& x)
{
	nn.Save(x.GetRootElement()["network"]);
	if (type == "mnist")
	{
		auto& data0 = x.GetRootElement()["externaldata"]["data"];
		data0.vv("type").SetValue("mnist");
		data0.vv("input").SetWideValue(if_mnist[0].c_str());
		data0.vv("inputlabels").SetWideValue(if_mnist[1].c_str());
		data0.vv("test").SetWideValue(if_mnist[2].c_str());
		data0.vv("testlabels").SetWideValue(if_mnist[3].c_str());
	}
}

PROJECT project;

void SaveProjectToFile(const wchar_t* f)
{
	XML3::XML x;
	project.Save(x);
	x.Save(f);
}

bool TrainCancel = 0;
HRESULT ForCallback(int iEpoch, int iDataset, int step, float totalloss, float acc,void* param);


void StartTrain(int m,void* p)
{
	project.nn.cbf = ForCallback;
	project.nn.paramcbf = p;
	TrainCancel = 0;
	if (m == 0)
	{
		std::thread t(&NN::train, (NN*)&project.nn, project.data_set,NumEpochsX, 1);
		t.detach();
	}
	if (m == 1)
	{
		std::thread t(&NN::dmltrain3, (NN*)&project.nn, project.data_set, CurrentBatch, NumEpochsX);
		t.detach();
	}
}


void LoadNetworkFile()
{
	XML3::XML x(fil.c_str());
	if (x.GetRootElement().GetChildrenNum() == 0)
		return;

	project.nn.Load(x.GetRootElement()["network"]);

	auto ed = x.GetRootElement().FindElementZ("externaldata");
	if(ed)
	{
		for (auto& d : *ed)
		{
			if (d.vv("type").GetValue() == "mnist")
			{
				std::array<std::wstring, 4> files;
				files[0] = d.vv("input").GetWideValue();
				files[1] = d.vv("inputlabels").GetWideValue();
				files[2] = d.vv("test").GetWideValue();
				files[3] = d.vv("testlabels").GetWideValue();

				std::vector<unsigned char> idx1ubyte;
				std::vector<unsigned char> idx3ubyte;
				LoadFile(files[0].c_str(), idx3ubyte);
				LoadFile(files[1].c_str(), idx1ubyte);
#ifdef ML_DEBUG_2
				project.data_set = parse_mnist(idx3ubyte, idx1ubyte, 6000);
#else
				project.data_set = parse_mnist(idx3ubyte, idx1ubyte, (int)(idx1ubyte.size() - 8));
#endif

				std::vector<unsigned char> idx1tbyte;
				std::vector<unsigned char> idx3tbyte;
				LoadFile(files[2].c_str(), idx3tbyte);
				LoadFile(files[3].c_str(), idx1tbyte);

#ifdef ML_DEBUG_2
				project.test_set = parse_mnist(idx3tbyte, idx1tbyte, 1000);
#else
				project.test_set = parse_mnist(idx3tbyte, idx1tbyte, (int)(idx1tbyte.size() - 8));
#endif

				project.if_mnist = files;
				project.type = "mnist";

			}
		}
	}

}


void mnist_test(const wchar_t* out)
{
	std::vector<std::wstring> mnist_files = {
	L".\\data\\mnist\\train-images.idx3-ubyte",L".\\data\\mnist\\train-labels.idx1-ubyte",
	L".\\data\\mnist\\t10k-images.idx3-ubyte",L".\\data\\mnist\\t10k-labels.idx1-ubyte",
	};

	std::vector<wchar_t> a(10000);
	GetModuleFileName(0, a.data(), 10000);
	auto a2 = wcsrchr(a.data(), L'\\');
	if (a2)
		*a2 = 0;
	SetCurrentDirectory(a.data());

	std::vector<unsigned char> idx1ubyte;
	std::vector<unsigned char> idx3ubyte;
	LoadFile(mnist_files[0].c_str(), idx3ubyte);
	LoadFile(mnist_files[1].c_str(), idx1ubyte);
	auto dataset = parse_mnist(idx3ubyte, idx1ubyte, 6000);
	//	auto dataset = parse_mnist(idx3ubyte, idx1ubyte, 5000);

	std::vector<unsigned char> idx1tbyte;
	std::vector<unsigned char> idx3tbyte;
	LoadFile(mnist_files[2].c_str(), idx3tbyte);
	LoadFile(mnist_files[3].c_str(), idx1tbyte);
	auto test_dataset = parse_mnist(idx3tbyte, idx1tbyte, 1000);

	if (1)
	{
		// Each neuron has some weights and a bias

		NN nn;
		nn.Layers.clear();

		Layer inputLayer(0.001f,0,0,0); 
		Layer hiddenLayer(0.001f, 1,128,784); // Relu
		Layer outputLayer(0.001f,0,10,128);      // Sigmoid

		// Random weights/biases for layers
		hiddenLayer.weights.rand_he();
		outputLayer.weights.rand_he();

		for (auto& j : hiddenLayer.biases.data())
			j = 0.01f;
		for (auto& j : outputLayer.biases.data())
			j = 0.01f;

		nn.Layers.push_back(inputLayer);  // Input layer
		nn.Layers.push_back(hiddenLayer);  // Input layer
		nn.Layers.push_back(outputLayer);  // Output layer

		DeleteFile(out);
		XML3::XML x(out);
		nn.Save(x.GetRootElement()["network"]);
		auto& data0 = x.GetRootElement()["externaldata"]["data"];
		data0.vv("type").SetValue("mnist");

		for (int i = 0; i < 4; i++)
		{
			wchar_t b[1000];
			GetFullPathName(mnist_files[i].c_str(), 1000, b, 0);	
			mnist_files[i] = b;	
		}
		data0.vv("input").SetWideValue(mnist_files[0].c_str());
		data0.vv("inputlabels").SetWideValue(mnist_files[1].c_str());
		data0.vv("test").SetWideValue(mnist_files[2].c_str());
		data0.vv("testlabels").SetWideValue(mnist_files[3].c_str());
		x.Save();

		// Non random init for debugging
/*		for (size_t i = 1; i < nn.Layers.size(); i++)
		{
			auto& layer = nn.Layers[i];
			for (size_t i = 0; i < layer.weights.data().size(); i++)
				layer.weights.data()[i] = 0.1f;
			for (size_t i = 0; i < layer.biases.data().size(); i++)
				layer.biases.data()[i] = 0.2f;
		}
*/
//		nn.load_weights_and_biases("weights.bin");


		// first dataset all 1 for debugging
	//	for (auto& e : dataset[0].input.data())
		//	e = 1.0f;
//		nn.dmltrain2(dataset, 10);
//		nn.train(dataset, 10);


//		nn.test_network(test_dataset);

		// Save it
//		nn.save_weights_and_biases("weights.bin");
	}
}

