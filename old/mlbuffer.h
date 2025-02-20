
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




MLOP NN::GetTrainOperator(ML& ml, unsigned int batch, int& starttid, std::vector<DataSet>& data, MLBUFFER& B_InputOp1, MLBUFFER& B_InputOp2, MLBUFFER& B_Label, MLBUFFER& B_Final, bool FP, bool BP, dml::Graph* g1, dml::Graph* g2)
{
	int& nexttid = starttid;
	std::vector<DML_BINDING_DESC> bindings_in;
	std::vector<DML_BINDING_DESC> bindings_out;
	std::vector<dml::Expression> outputs;




	bool RNN = 0;
	unsigned int hidden_units = 128;
	MLBUFFER B_HiddenState;
	//	unsigned int time_steps = 28;

		// Forward Propagation Operator
	if (FP)
	{

		// test 
//		auto opx = GetFPO(ml, batch, data);

		dml::Graph& graph1 = *g1;

		// Input Tensor
		if (data.size() == 0)
			return MLOP(nullptr);
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
			MLOP op2(&ml);
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
			if (l.B_BiasesOut.b.b)
				outputs.push_back(l.B_BiasesOut.ee);
			if (l.B_WeightsOut.b.b)
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
		MLOP op2(&ml);
		op2.dmlCompiledOperator.Attach(OutputCompiledOperator2.Detach());
		op2.bindings_out = bindings_out;
		op2.bindings_in = bindings_in;
		return op2;
	}


	return MLOP(nullptr);
}


