HRESULT dmltrain2(std::vector<DataSet> data, int numEpochs)
{
	if (Layers.size() < 3)
		return E_FAIL;

	std::vector<char> temp;
	CoInitialize(0);
	ML ml;
	HRESULT hr = 0;
	hr = ml.InitializeDirect3D12();
	hr = ml.CreateDML();


	dml::Expression T_InputOp2;
	dml::Graph graph1(ml.dmlDevice);
	dml::Graph graph2(ml.dmlDevice);
	int EnableBP = 2;
	dml::Graph graph3(ml.dmlDevice);
	//		dml::Graph& graph3 = graph1;


	int nexttid = 0;


	MLBUFFER B_InputOp1;
	MLBUFFER B_Label;

	std::vector<DML_BINDING_DESC> all_bindings_in;
	std::vector<DML_BINDING_DESC> all_bindings_out;
	std::vector<dml::Expression> all_outputs;


	// Forward Propagation
	if (1)
	{
		std::vector<DML_BINDING_DESC> bindings_in;
		std::vector<DML_BINDING_DESC> bindings_out;
		std::vector<dml::Expression> outputs;

		// Input Tensor (flattened)
		B_InputOp1.Create(ml.d3D12Device, dml::InputTensor(graph1, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, { 1,1,1,(unsigned int)data[0].input.cols() * data[0].input.rows()} }));
		bindings_in.push_back(B_InputOp1.BindingDesc());

		// If MNIST, [1,1,1,784], Byte Size = 3136


		// Hidden Layers
		for (size_t i = 1; i < Layers.size() && Layers.size() >= 3; i++)
		{
			auto& layer = Layers[i];
			layer.B_Weights.Create(ml.d3D12Device, (dml::InputTensor(graph1, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, { 1,1,(unsigned int)layer.weights.rows(),(unsigned int)layer.weights.cols()} })));
			bindings_in.push_back(layer.B_Weights.BindingDesc());

			// If MNIST Hidden, [1,1,784,128] 401408
			// If MNIST Output, [1,1,128,10] 5120

			layer.B_Biases.Create(ml.d3D12Device, (dml::InputTensor(graph1, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, { 1,1,1,(unsigned int)layer.biases.cols()} })));
			bindings_in.push_back(layer.B_Biases.BindingDesc());

			// If MNIST Hidden, [1,1,1,128] 512
			// If MNIST Output, [1,1,1,10] 40
		}

		// Create Forward Propagation Operators
		for (int i = 0; i < Layers.size() && Layers.size() >= 3; i++)
		{
			auto& layer = Layers[i];
			if (i == 0) // input layer -> first hidden
			{
				layer.B_Outputs.Create(ml.d3D12Device, dml::Identity(B_InputOp1.ee));
				if (EnableBP != 1)
					bindings_out.push_back(layer.B_Outputs.BindingDesc());
				// If MNIST, [1,1,1,784] 3136
			}
			else
			{
				auto& pl = Layers[i - 1];
				auto mul1 = dml::Gemm(pl.B_Outputs.ee, layer.B_Weights.ee);  // 1,1,1,128 if mnist hidden, 1,1,1,10 if mnist output
				auto add1 = dml::Add(mul1, layer.B_Biases.ee);
				if (layer.ActType == 1) // Relu
					layer.B_Outputs.Create(ml.d3D12Device, dml::ActivationRelu(add1));
				if (layer.ActType == 0) // sigmoid
					layer.B_Outputs.Create(ml.d3D12Device, dml::ActivationSigmoid(add1));
				if (layer.B_Outputs.b && EnableBP != 1)
					bindings_out.push_back(layer.B_Outputs.BindingDesc());
				// If MNIST Hidden, [1,1,1,128] 512
				// If MNIST Output, [1,1,1,10] 40
			}
			// We want the outputs as output
			outputs.push_back(layer.B_Outputs.ee);
		}

		// Create the operator
		if (EnableBP != 1)
		{
			auto OutputCompiledOperator2 = graph1.Compile(DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION_COMPUTATION, outputs);
			MLOP op2;
			op2.dmlCompiledOperator.Attach(OutputCompiledOperator2.Detach());
			op2.bindings_out = bindings_out;
			op2.bindings_in = bindings_in;
			ml.ops.push_back(op2);
		}
		if (EnableBP == 1)
		{
			for (auto& i : bindings_in)
				all_bindings_in.push_back(i);
			for (auto& i : bindings_out)
				all_bindings_out.push_back(i);

			// No outputs, we don't need them here
		}
	}

	// Create the backpropagation tensors and operators
	MLBUFFER B_Final;
	if (EnableBP)
	{
		std::vector<DML_BINDING_DESC> bindings_in;
		std::vector<DML_BINDING_DESC> bindings_out;
		std::vector<dml::Expression> outputs;
		if (EnableBP == 2)
			nexttid = 0;

		// Input Tensor
		if (EnableBP == 2)
		{
			T_InputOp2 = dml::InputTensor(graph3, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, Layers.back().B_Outputs.GetOutputDesc().sizes });
			bindings_in.push_back(Layers.back().B_Outputs.BindingDesc());
		}

		// If MNIST, [1,1,1,10]

		// Label Tensor
		B_Label.Create(ml.d3D12Device, dml::InputTensor(graph3, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, Layers.back().B_Outputs.GetOutputDesc().sizes }));
		bindings_in.push_back(B_Label.BindingDesc());

		// If MNIST, [1,1,1,10]

		// Weights
		// If MNIST Hidden, [1,1,784,128] 401408
		// If MNIST Output, [1,1,128,10] 5120

		// Biases
		// If MNIST Hidden, [1,1,1,128] 512
		// If MNIST Output, [1,1,1,10] 40

		Layers.back().T_Delta = dml::Subtract(EnableBP == 2 ? T_InputOp2 : Layers.back().B_Outputs.ee, B_Label.ee);
		// If MNIST, [1,1,1,10]
		for (int i = (int)(Layers.size() - 1); i >= 0; i--)
		{
			Layer& curr = Layers[i];
			if (i == 0)
			{
				B_Final.Create(ml.d3D12Device, dml::Identity(curr.T_Delta));
				bindings_out.push_back(B_Final.BindingDesc());
				break;
			}

			Layer& prev = Layers[i - 1];

			// biased += σ'(z)
			// curr.biases += delta * ((float)-curr.lr);

			curr.bp_mincurrlr = ml.ConstantValueTensor(graph3, (float)-curr.lr, curr.T_Delta.GetOutputDesc().sizes);
			curr.bp_mul2 = dml::Multiply(curr.T_Delta, curr.bp_mincurrlr);


			if (EnableBP == 2)
			{
				if (curr.T_BiasesBPIn.Impl() == 0)
				{
					curr.T_BiasesBPIn = (dml::InputTensor(graph3, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, curr.B_Biases.ee.GetOutputDesc().sizes }));
					bindings_in.push_back(curr.B_Biases.BindingDesc());
				}
				curr.B_BiasesOut.Create(ml.d3D12Device, dml::Add(curr.T_BiasesBPIn, curr.bp_mul2));
			}
			else
				curr.B_BiasesOut.Create(ml.d3D12Device, dml::Add(curr.B_Biases.ee, curr.bp_mul2));

			bindings_out.push_back(curr.B_BiasesOut.BindingDesc());


			// curr.weights += gradient * ((float)-curr.lr); // Gradient will be first  128x10,  second 784x128 for MNIST
			// 1,1,1,10, 1,1,1,128
			// Matrix gradient = (prev.output.transpose() * delta); // 128x10, 784x128


			if (EnableBP == 2)
			{
				if (prev.T_OutputsBPIn.Impl() == 0)
				{
					prev.T_OutputsBPIn = (dml::InputTensor(graph3, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, prev.B_Outputs.GetOutputDesc().sizes }));
					bindings_in.push_back(prev.B_Outputs.BindingDesc());
				}
				curr.bp_PrevOutputTranspose = dml::Reinterpret(prev.T_OutputsBPIn, DML_TENSOR_DATA_TYPE_FLOAT32, { 1,1,(unsigned int)prev.T_OutputsBPIn.GetOutputDesc().sizes[3],(unsigned int)prev.T_OutputsBPIn.GetOutputDesc().sizes[2] }, dml::NullOpt);
			}
			else
				curr.bp_PrevOutputTranspose = dml::Reinterpret(prev.B_Outputs.ee, DML_TENSOR_DATA_TYPE_FLOAT32, { 1,1,(unsigned int)prev.B_Outputs.GetOutputDesc().sizes[3],(unsigned int)prev.B_Outputs.GetOutputDesc().sizes[2] }, dml::NullOpt);

			curr.bp_gradient = dml::Gemm(curr.bp_PrevOutputTranspose, curr.T_Delta); // 128x10, 784x128 for MNIST

			curr.bp_mincurrlr2 = ml.ConstantValueTensor(graph3, (float)-curr.lr, curr.bp_gradient.GetOutputDesc().sizes);
			curr.bp_mul3 = dml::Multiply(curr.bp_gradient, curr.bp_mincurrlr2);

			if (EnableBP == 2)
			{
				if (curr.T_WeightsBPIn.Impl() == 0)
				{
					curr.T_WeightsBPIn = (dml::InputTensor(graph3, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32,curr.B_Weights.ee.GetOutputDesc().sizes }));
					bindings_in.push_back(curr.B_Weights.BindingDesc());
				}
			}


			if (EnableBP == 2)
				curr.B_WeightsOut.Create(ml.d3D12Device, dml::Add(curr.T_WeightsBPIn, curr.bp_mul3));
			else
				curr.B_WeightsOut.Create(ml.d3D12Device, dml::Add(curr.B_Weights.ee, curr.bp_mul3));
			bindings_out.push_back(curr.B_WeightsOut.BindingDesc());

			// 1,1,128,10, 1,1,784,128

			//					curr.bp_one = ml.ConstantValueTensor(graph3, 1.0f, { 1,1,prev.T_OutputsBP.GetOutputDesc().sizes[2],prev.T_OutputsBP.GetOutputDesc().sizes[3] });
			if (EnableBP == 2)
				curr.bp_der1 = dml::Identity(prev.T_OutputsBPIn);
			else
				curr.bp_der1 = dml::Identity(prev.B_Outputs.ee);

			if (prev.ActType == 0)
			{
				// Sigmoid Derivative
				curr.bp_dx = dml::ActivationSigmoid(curr.bp_der1);
				curr.bp_one1 = ml.ConstantValueTensor(graph3, 1.0f, curr.bp_dx.GetOutputDesc().sizes);
				curr.bp_oneMinusSigmoid = dml::Subtract(curr.bp_one1, curr.bp_dx);
				curr.bp_der2 = dml::Multiply(curr.bp_dx, curr.bp_oneMinusSigmoid);
			}
			else
			{
				// Relu derivative
				curr.bp_dx = dml::ActivationRelu(curr.bp_der1);
				curr.bp_zeroTensor = ml.ConstantValueTensor(graph3, 0.0f, curr.bp_dx.GetOutputDesc().sizes);
				curr.bp_reluMask = dml::GreaterThan(curr.bp_dx, curr.bp_zeroTensor);
				curr.bp_der2 = dml::Cast(curr.bp_reluMask, DML_TENSOR_DATA_TYPE_FLOAT32);
			}

			// Calculate delta again
			curr.bp_CurrWT = dml::Reinterpret(curr.B_WeightsOut.ee, DML_TENSOR_DATA_TYPE_FLOAT32, { 1,1,(unsigned int)curr.B_WeightsOut.ee.GetOutputDesc().sizes[3],(unsigned int)curr.B_WeightsOut.ee.GetOutputDesc().sizes[2] }, dml::NullOpt);
			curr.bp_Delta2 = dml::Gemm(curr.T_Delta, curr.bp_CurrWT);
			curr.bp_Delta3 = dml::Multiply(curr.bp_Delta2, curr.bp_der2);
			prev.T_Delta = dml::Identity(curr.bp_Delta3);

		}

		if (EnableBP == 1)
		{
			for (auto& i : bindings_in)
				all_bindings_in.push_back(i);
			for (auto& i : bindings_out)
				all_bindings_out.push_back(i);

			// Order here doesn't matter, it matters when we first declare them, so watch it later!
			for (int i = (int)(Layers.size() - 1); i >= 0; i--)
			{
				auto& l = Layers[i];
				if (l.B_BiasesOut.b)
					outputs.push_back(l.B_BiasesOut.ee);
				if (l.B_WeightsOut.b)
					outputs.push_back(l.B_WeightsOut.ee);
			}
			outputs.push_back(B_Final.ee);

			// Add another output test
			auto OutputCompiledOperator2 = graph3.Compile(DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION_COMPUTATION, outputs);
			MLOP op2;
			op2.dmlCompiledOperator.Attach(OutputCompiledOperator2.Detach());
			op2.bindings_out = all_bindings_out;
			op2.bindings_in = all_bindings_in;
			ml.ops.push_back(op2);
		}

		if (EnableBP == 2)
		{
			// Order here doesn't matter, it matters when we first declare them, so watch it later!
			for (int i = (int)(Layers.size() - 1); i >= 0; i--)
			{
				auto& l = Layers[i];
				if (l.B_BiasesOut.b)
					outputs.push_back(l.B_BiasesOut.ee);
				if (l.B_WeightsOut.b)
					outputs.push_back(l.B_WeightsOut.ee);
			}
			outputs.push_back(B_Final.ee);
			auto OutputCompiledOperator2 = graph3.Compile(DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION_COMPUTATION, outputs);
			MLOP op2;
			op2.dmlCompiledOperator.Attach(OutputCompiledOperator2.Detach());
			op2.bindings_out = bindings_out;
			op2.bindings_in = bindings_in;
			ml.ops.push_back(op2);
		}

	}


	// Initialize
	for (auto& op : ml.ops)
		op.CreateInitializer(ml.dmlDevice);

	// https://learn.microsoft.com/en-us/windows/ai/directml/dml-binding
	// Query the operator for the required size (in descriptors) of its binding table.
	ml.CreateHeap();

	// Create a binding table over the descriptor heap we just created.
	for (auto& op : ml.ops)
		op.CreateBindingTable(ml.dmlDevice, ml.descriptorHeap);

	// Resources
	for (auto& op : ml.ops)
	{
		auto bp = op.dmlOperatorInitializer->GetBindingProperties();
		if (bp.TemporaryResourceSize)
		{
			op.tri.Create2(ml.d3D12Device, bp.TemporaryResourceSize, true);
			auto bu = op.tri.BindingDesc();
			op.dmlBindingTable->BindTemporaryResource(&bu);

		}
		if (bp.PersistentResourceSize)
		{
			op.pri.Create2(ml.d3D12Device, bp.PersistentResourceSize, true);
			auto bu = op.pri.BindingDesc();
			op.dmlBindingTable->BindPersistentResource(&bu);
		}
	}


	// The command recorder is a stateless object that records Dispatches into an existing Direct3D 12 command list.
	hr = ml.CreateCommandRecorder();

	// Record execution of the operator initializer.
	ml.Record(0);

	// Close the Direct3D 12 command list, and submit it for execution as you would any other command list. You could
	// in principle record the execution into the same command list as the initialization, but you need only to Initialize
	// once, and typically you want to Execute an operator more frequently than that.
	ml.CloseExecuteResetWait();

	bool MustBind = 1;


	// Upload initial weights/biases to all tensors
	if (1)
	{
		for (auto& l : Layers)
		{
			if (l.B_Biases.b)
				l.B_Biases.Upload(ml.d3D12Device, ml.commandList, l.biases.data().data(), l.biases.data().size() * 4,&ml);
			if (l.B_Weights.b)
				l.B_Weights.Upload(ml.d3D12Device, ml.commandList, l.weights.data().data(), l.weights.data().size() * 4,&ml);
		}
	}

	auto rng = std::default_random_engine{};
	for (int iEpoch = 0; iEpoch < numEpochs; iEpoch++)
	{
#ifndef ML_DEBUG_1
		std::shuffle(std::begin(data), std::end(data), rng);
#endif
		if (iEpoch > 0)
			accuracy = track_accuracy(data);

		float total_loss = 0;
		for (size_t iData = 0; iData < data.size(); iData++)
		{
			// callback
			if (iData > 0)
				total_loss += error(Layers.back().output, data[iData].label);
			if (cbf)
				hr = cbf((int)iEpoch, (int)iData, 1, total_loss, accuracy, paramcbf);
			if (FAILED(hr))
				return E_ABORT;



			// Bind and execute the operator on the GPU.
			ml.SetDescriptorHeaps();


			// Reset the binding table to bind for the operator we want to execute (it was previously used to bind for the initializer).
			if (MustBind)
			{
				for (auto& op : ml.ops)
					op.ResetToExecute();

				// And temporary/persistent resources
				for (auto& op : ml.ops)
				{
					auto bp = op.dmlCompiledOperator->GetBindingProperties();
					if (bp.TemporaryResourceSize)
					{
						op.tre.Create2(ml.d3D12Device, bp.TemporaryResourceSize, true);
						auto bu = op.tre.BindingDesc();
						op.dmlBindingTable->BindTemporaryResource(&bu);

					}
					if (bp.PersistentResourceSize)
					{
						op.pre.Create2(ml.d3D12Device, bp.PersistentResourceSize, true);
						auto bu = op.pre.BindingDesc();
						op.dmlBindingTable->BindPersistentResource(&bu);
					}
				}

				// Start binding
				for (size_t i1 = 0; i1 < ml.ops.size(); i1++)
				{
					auto& op = ml.ops[i1];
					op.dmlBindingTable->BindInputs((UINT)op.bindings_in.size(), op.bindings_in.data());
					op.dmlBindingTable->BindOutputs((UINT)op.bindings_out.size(), op.bindings_out.data());
				}


				MustBind = 0;
			}



			// Input Tensor
			B_InputOp1.Upload(ml.d3D12Device, ml.commandList, data[iData].input.data().data(), data[iData].input.data().size() * 4,&ml);
			Layers[0].B_Outputs.Upload(ml.d3D12Device, ml.commandList, data[iData].input.data().data(), data[iData].input.data().size() * 4,&ml);
			if (B_Label.b)
				B_Label.Upload(ml.d3D12Device, ml.commandList, data[iData].label.data().data(), data[iData].label.data().size() * 4,&ml);

			// Transition of the buffers
			for (auto& op : ml.ops)
				op.TransitionBindings(ml.commandList);

			// Run it
			float* pt = 0;

			// Test before execution
/*
				// TEST
				for (size_t i = 0; i < Layers.size(); i++)
				{
					auto& l = Layers[i];
					l.B_Outputs.Download(ml.d3D12Device, ml.commandList, (size_t)-1, &ml, temp);
					pt = (float*)temp.data();
					l.B_Biases.Download(ml.d3D12Device, ml.commandList, (size_t)-1, &ml, temp);
					pt = (float*)temp.data();
					l.B_Weights.Download(ml.d3D12Device, ml.commandList, (size_t)-1, &ml, temp);
					pt = (float*)temp.data();
				}
*/

			ml.Record(1);
			ml.CloseExecuteResetWait();



			// Copy new weights and biases back
			for (size_t ii = 0; ii < Layers.size(); ii++)
			{
				if (ii == 0)
					continue;
				auto& layer = Layers[ii];
				if (layer.B_BiasesOut.b)
				{
					layer.B_BiasesOut.Download(ml.d3D12Device, ml.commandList, layer.B_BiasesOut.ls, &ml, temp);
					pt = (float*)temp.data();
					layer.B_Biases.Upload(ml.d3D12Device, ml.commandList, (float*)temp.data(), temp.size(),&ml);
				}
				if (layer.B_WeightsOut.b)
				{
					layer.B_WeightsOut.Download(ml.d3D12Device, ml.commandList, layer.B_WeightsOut.ls, &ml, temp);
					pt = (float*)temp.data();
					layer.B_Weights.Upload(ml.d3D12Device, ml.commandList, (float*)temp.data(), temp.size(),&ml);
				}
			}
			

			// TEST after execution
#ifdef DEBUGML
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

		// End of epoch, update all
		for (auto& l : Layers)
		{
			// Outputs 
			if (l.B_Outputs.b)
			{
				l.B_Outputs.Download(ml.d3D12Device, ml.commandList, l.B_Outputs.ls, &ml, temp);
				memcpy(l.output.data().data(), temp.data(), l.output.data().size() * 4);
			}

			// Biases
			if (l.B_BiasesOut.b)
			{
				l.B_BiasesOut.Download(ml.d3D12Device, ml.commandList, l.B_Biases.ls, &ml, temp);
				memcpy(l.biases.data().data(), temp.data(), l.biases.data().size() * 4);
			}

			// Weights
			if (l.B_WeightsOut.b)
			{
				l.B_Weights.Download(ml.d3D12Device, ml.commandList, l.B_Weights.ls, &ml, temp);
				memcpy(l.weights.data().data(), temp.data(), l.weights.data().size() * 4);
			}

		}

	}


	return S_OK;
}
//




/*
		// Run loop
		for (int iEpoch = 0; iEpoch < numEpochs; iEpoch++)
		{
#ifndef ML_DEBUG_1
			std::shuffle(std::begin(data), std::end(data), rng);
#endif

			float total_loss = 0;
			for (size_t iData = 0; iData < data.size(); iData += batch)
			{
				// callback
				if (iData > 0)
					total_loss += error(Layers.back().output, data[iData].label);
				if (cbf)
					hr = cbf((int)iEpoch, (int)iData, 1, total_loss, accuracy, paramcbf);
				if (FAILED(hr))
					return E_ABORT;

				// Bind and execute the operator on the GPU.
				ml.SetDescriptorHeaps();

				if (MustBind)
				{
					for (auto& op : ml.ops)
						op.ResetToExecute();

					// Start binding
					for (size_t i1 = 0; i1 < ml.ops.size(); i1++)
					{
						auto& op = ml.ops[i1];
						op.dmlBindingTable->BindInputs((UINT)op.bindings_in.size(), op.bindings_in.data());
						op.dmlBindingTable->BindOutputs((UINT)op.bindings_out.size(), op.bindings_out.data());
					}

					// And temporary/persistent resources
					ml.DoTempAndPersistentBindingExecute();

					MustBind = 0;
				}

				// Input + Data uploading
				B_InputOp1.Upload(ml.d3D12Device, ml.commandList, data[iData].input.data().data(), data[iData].input.data().size() * 4,&ml);
				if (B_Label.b)
					B_Label.Upload(ml.d3D12Device, ml.commandList, data[iData].label.data().data(), data[iData].label.data().size() * 4,&ml);

				// Transition of the buffers
				for (auto& op : ml.ops)
					op.TransitionBindings(ml.commandList);

				// Run it
				ml.Record(1);
				ml.CloseExecuteResetWait();

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
						layer.B_Biases.Upload(ml.d3D12Device, ml.commandList, (float*)temp.data(), temp.size(),&ml);
					}
					if (layer.B_WeightsOut.b)
					{
						layer.B_WeightsOut.Download(ml.d3D12Device, ml.commandList, layer.B_WeightsOut.ls, &ml, temp);
						pt = (float*)temp.data();
						layer.B_Weights.Upload(ml.d3D12Device, ml.commandList, (float*)temp.data(), temp.size(),&ml);
					}
				}


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


		}
		*/


/*


Tiled Output Forward with Batch

/*
						auto tiledInput = dml::Tile(pl.B_Outputs.ee, {batch, 1, 1, 1});

							// Perform matrix multiplication with weights
							auto mul1 = dml::Gemm(tiledInput, layer.B_Weights.ee);

							// Add biases (broadcasted across the batch)
							auto add1 = dml::Add(mul1, layer.B_Biases.ee);

							// Apply activation function
							auto output = dml::ActivationRelu(add1);
							*/


							// Reinterpret input to flatten the batch into the rows dimension: [16, 1, 1, 784] -> [1, 16, 784]
							// Doesn't work
	/*						auto reshapedInput = dml::Reinterpret(
								pl.B_Outputs.ee,
								{ 1, batch, pl.B_Outputs.ee.GetOutputDesc().sizes[3]},  // Flattened shape
								dml::NullOpt   // Strides are automatically inferred
							);

							auto mul1 = dml::Gemm(reshapedInput, layer.B_Weights.ee);
							auto add1 = mul1 + layer.B_Biases.ee;
							if (layer.ActType == 1) // Relu
								layer.B_Outputs.Create(ml.d3D12Device, dml::ActivationRelu(add1));
							if (layer.ActType == 0) // sigmoid
								layer.B_Outputs.Create(ml.d3D12Device, dml::ActivationSigmoid(add1));
								*/


*/

/*



	/*

	Explanation of Changes:
Time Dependency:

Added a loop to iterate through time steps in reverse order (Backpropagation Through Time or BPTT).
Separate Gradients:

Gradients for input weights (weights_input), recurrent weights (weights_recurrent), and biases are calculated and updated separately.
Hidden State Propagation:

Hidden states from previous time steps are used to compute recurrent gradients.
Delta Update:

The gradient for each layer at each time step includes the contribution from both the loss and the future deltas via the recurrent connection.
Gradient Clipping:

Gradients for weights and recurrent weights are clipped to avoid exploding gradients, which is a common issue in RNNs.
Biases Update:

Biases are updated for each time step using the computed deltas.
Additional Notes:
Initialization:

Make sure the RNN layer has weights_recurrent and maintains hidden_states for each time step.
Efficiency:

For large sequences, consider truncated backpropagation through time (TBPTT) to limit the length of the sequence during backpropagation.
Advanced Models:

This implementation is for a simple RNN. For GRUs or LSTMs, include gate-specific updates (e.g., forget, update, or output gates).
*/
/*
void BackPropagationRNN(std::vector<Matrix> sequence_labels)
{
	// Initialize delta for each time step (starting from the last time step)
	std::vector<Matrix> deltas(sequence_labels.size());

	// Initialize gradients for weights and recurrent weights
	std::vector<Matrix> gradients_input(Layers.size());
	std::vector<Matrix> gradients_recurrent(Layers.size());
	std::vector<Matrix> biases_gradients(Layers.size());

	// Initialize deltas and hidden state derivatives for all layers
	std::vector<Matrix> hidden_deltas(Layers.size());

	// Iterate over the sequence in reverse order (backpropagation through time)
	for (int t = (int)sequence_labels.size() - 1; t >= 0; t--)
	{
		// Get the label for the current time step
		Matrix label = sequence_labels[t];
		Matrix delta;

		// Iterate through layers backward
		for (int i = (int)(Layers.size() - 1); i > 0; i--)
		{
			auto& curr = Layers[i];
			auto& prev = Layers[i - 1];

			if (t == (int)sequence_labels.size() - 1) // At the last time step
			{
				// Calculate delta for the output layer
				auto OurOutput = curr.output; // Output at time t
				delta = OurOutput - label;    // Loss gradient
			}
			else
			{
				// Add contribution from future deltas (recurrent connection)
				delta += hidden_deltas[i] * curr.weights_recurrent.transpose();
			}

			// Update biases gradient
			curr.biases += delta * ((float)-curr.lr);

			// Calculate input weight gradient
			Matrix input_gradient = prev.output.transpose() * delta;

			// Clip gradient to avoid exploding gradients
			float clip_value = 5.0f;
			for (auto& value : input_gradient.data())
			{
				value = std::max(-clip_value, std::min(value, clip_value));
			}

			// Update input weights
			curr.weights += input_gradient * ((float)-curr.lr);

			// Calculate recurrent weight gradient
			if (t > 0) // Only calculate if not the first time step
			{
				auto& prev_hidden = Layers[i].hidden_states[t - 1]; // Hidden state at t-1
				Matrix recurrent_gradient = prev_hidden.transpose() * delta;

				// Clip gradient
				for (auto& value : recurrent_gradient.data())
				{
					value = std::max(-clip_value, std::min(value, clip_value));
				}

				// Update recurrent weights
				curr.weights_recurrent += recurrent_gradient * ((float)-curr.lr);
			}

			// Calculate delta for the previous layer
			Matrix der = prev.output; // Activation derivative
			if (prev.ActType == 0)
				der.sigmoid_derivative();
			else
				der.relu_derivative();

			// Combine chain rule contributions
			delta = (delta * curr.weights.transpose()).multiply_inplace(der);

			// Store hidden delta for future time steps
			hidden_deltas[i] = delta;
		}
	}
}
*/




/*
		Derivation of the Mean Squared Error function
		∂MSE/∂y = 2(y - y')
	*/
	/*
	 Derivation of the sigmoid function
	 σ(x) = 1 / (1 + e^-x)

	*/

	/*
	*
	Compute gradients for weights and biases based on the batch.
	For each input in the batch:
	Calculate the error (difference between prediction and label).
	Propagate this error backward through the network.
	Gradients for weights and biases are accumulated across the batch.
	The average gradient (sum divided by batch size) is used to update the weights and biases.


	*
		void BackPropagationBatch(std::vector<Matrix>& batchInputs, std::vector<Matrix>& batchLabels) {
			// Ensure batch size consistency
			if (batchInputs.size() != batchLabels.size()) {
				throw std::invalid_argument("Batch size mismatch between inputs and labels.");
			}

			int batchSize = static_cast<int>(batchInputs.size());

			// Initialize deltas for each layer in the batch
			std::vector<Matrix> deltas(batchSize);

			// Compute deltas for the output layer
			for (int b = 0; b < batchSize; b++) {
				auto& OurOutput = Layers.back().output[b]; // Access output for this sample
				deltas[b] = OurOutput - batchLabels[b]; // Compute delta for this sample
			}

			// Backpropagate through layers
			for (int i = (int)(Layers.size() - 1); i > 0; i--) {
				Layer& curr = Layers[i];
				Layer& prev = Layers[i - 1];

				// Initialize gradient accumulators for weights and biases
				Matrix accumulatedBiasGradient(curr.biases.rows(), curr.biases.cols(), 0.0f);
				Matrix accumulatedWeightGradient(curr.weights.rows(), curr.weights.cols(), 0.0f);

				// Process each sample in the batch
				for (int b = 0; b < batchSize; b++) {
					// Gradient for biases
					accumulatedBiasGradient += deltas[b] * ((float)-curr.lr);

					// Gradient for weights
					Matrix weightGradient = (prev.output[b].transpose() * deltas[b]);

					// Gradient clipping
					float clip_value = 5.0f;
					for (auto& value : weightGradient.data()) {
						value = std::max(-clip_value, std::min(value, clip_value));
					}

					accumulatedWeightGradient += weightGradient;

					// Compute delta for the previous layer
					if (i > 1) { // Skip first layer
						Matrix der = prev.output[b]; // Activation derivatives
						if (prev.ActType == 0)
							der.sigmoid_derivative();
						else
							der.relu_derivative();

						deltas[b] = (deltas[b] * curr.weights.transpose()).multiply_inplace(der);
					}
				}

				// Apply averaged gradients to weights and biases
				curr.biases += (accumulatedBiasGradient * (1.0f / batchSize));
				curr.weights += (accumulatedWeightGradient * (1.0f / batchSize));
			}
		}
		*/


/*
		// Some test 
if (0)
{
	dml::Graph& graph2 = *g2;

	// Input Tensor
	B_InputOp2.ee = dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, Layers.back().B_Outputs.GetOutputDesc().sizes });
	bindings_in.push_back(Layers.back().B_Outputs.BindingDesc());

	// Label Tensor
	B_Label.Create(ml.d3D12Device, dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, Layers.back().B_Outputs.GetOutputDesc().sizes }));
	bindings_in.push_back(B_Label.BindingDesc());

	B_Final.Create(ml.d3D12Device, B_InputOp2.ee + B_Label.ee);
	bindings_out.push_back(B_Final.BindingDesc());
	outputs.push_back(B_Final.ee);


	auto OutputCompiledOperator2 = graph2.Compile(DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION_COMPUTATION, outputs);
	MLOP op2;
	op2.dmlCompiledOperator.Attach(OutputCompiledOperator2.Detach());
	op2.bindings_out = bindings_out;
	op2.bindings_in = bindings_in;
	return op2;
}
*/





// Batch (Not Working)
/*

In back propagation without batches, I updated biases based on the biases += delta * -learningrate, caclulate a gradient from tranpose of previous  output  * delta,
then update weights  by the formula weigts += gradient * -learningrate. Then I calculate new delta with the formula
delta = (delta * transpose(curr.weights)) * derivative;


In backpropagation with batches, biases are updated by averaging the deltas across the batch: biases += averaged_delta * -learning_rate.
Gradients for weights are calculated by accumulating gradients for each batch sample, computed as the transpose of the previous outputs * deltas, and averaging them:
weights += averaged_gradient * -learning_rate.
Finally, deltas are updated for each batch sample with the formula: delta = (delta * transpose(curr.weights)) * derivative, and these are accumulated and averaged to proceed through the layers.

*/
// Backward Propagation
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

	std::vector<dml::Expression> batchGradients(Layers.size());
	dml::Expression averagedDelta;
	for (unsigned int iBatch = 0; iBatch < batch; iBatch++)
	{
		auto T_Delta1 = (batch == 1) ? T_DeltaFull : dml::Slice(T_DeltaFull, { iBatch, 0, 0, 0 }, { 1, 1, 1, T_DeltaFull.GetOutputDesc().sizes[3] }, { 1, 1, 1, 1 });

		for (int i = (int)(Layers.size() - 1); i >= 0; i--)
		{
			Layer& curr = Layers[i];
			if (i == 0)
				break;
			Layer& prev = Layers[i - 1];
			// Matrix gradient = (prev.output.transpose() * delta); // 128x10, 784x128
			if (prev.T_OutputsBPIn.Impl() == 0)
			{
				prev.T_OutputsBPIn = (dml::InputTensor(graph2, nexttid++, { DML_TENSOR_DATA_TYPE_FLOAT32, prev.B_Outputs.GetOutputDesc().sizes }));
				bindings_in.push_back(prev.B_Outputs.BindingDesc());
			}
			auto bp_gradientunclipped = dml::Gemm(prev.T_OutputsBPIn, T_Delta1, {}, DML_MATRIX_TRANSFORM_TRANSPOSE, DML_MATRIX_TRANSFORM_NONE); // 128x10, 784x128 for MNIST

			// Clip the gradient
			/*float clip_value = 5.0f; 				for (auto& value : gradient.data()) { 						value = std::max(-clip_value, std::min(value, clip_value)); } 	*/
			auto bp_clippedgradient = dml::Clip(bp_gradientunclipped, -5.0f, 5.0f);

			if (iBatch == 0) batchGradients[i] = bp_clippedgradient;
			else batchGradients[i] = batchGradients[i] + bp_clippedgradient;


			// 1,1,128,10, 1,1,784,128
			// Matrix der = prev.output; // 1x128, 1x784
			auto bp_der1 = (batch == 1) ? dml::Identity(prev.T_OutputsBPIn)
				: dml::Slice(prev.T_OutputsBPIn, { iBatch, 0, 0, 0 },
					{ 1, 1, prev.T_OutputsBPIn.GetOutputDesc().sizes[2], prev.T_OutputsBPIn.GetOutputDesc().sizes[3] },
					{ 1, 1, 1, 1 });

			dml::Expression bp_der2;
			if (prev.ActType == 0)
			{
				// Sigmoid Derivative
				/*
				for (size_t i = 0; i < _data.size(); i++) {
						_data[i] = _data[i] * (1 - _data[i]);
					}
				*/
				auto bp_dx = dml::ActivationSigmoid(bp_der1);
				auto bp_one1 = ml.ConstantValueTensor(graph2, 1.0f, bp_dx.GetOutputDesc().sizes);
				auto bp_oneMinusSigmoid = bp_one1 - bp_dx;
				bp_der2 = bp_dx * bp_oneMinusSigmoid;
			}
			else
			{
				// Relu derivative
				/*for (size_t i = 0; i < _data.size(); i++) {
					_data[i] = _data[i] > 0 ? (matrix_t)1 : (matrix_t)0;
				}*/

				auto bp_dx = dml::ActivationRelu(bp_der1);
				auto bp_zeroTensor = ml.ConstantValueTensor(graph2, 0.0f, bp_dx.GetOutputDesc().sizes);
				auto bp_reluMask = dml::GreaterThan(bp_dx, bp_zeroTensor);
				bp_der2 = dml::Cast(bp_reluMask, DML_TENSOR_DATA_TYPE_FLOAT32);
			}

			// Calculate delta again
			// delta = (delta * curr.weights.transpose()).multiply_inplace(der);
			auto bp_CurrWT = dml::Reinterpret(curr.B_WeightsOut.ee, DML_TENSOR_DATA_TYPE_FLOAT32, { 1,1,(unsigned int)curr.B_WeightsOut.ee.GetOutputDesc().sizes[3],(unsigned int)curr.B_WeightsOut.ee.GetOutputDesc().sizes[2] }, dml::NullOpt);
			auto bp_Delta2 = dml::Gemm(T_Delta1, bp_CurrWT);
			auto bp_Delta3 = bp_Delta2 * bp_der2;
			T_Delta1 = dml::Identity(bp_Delta3);


		}
		averagedDelta = T_Delta1 * (1.0f / batch);
	}
	if (batch > 1)
	{
		for (int i = 1; i < Layers.size(); i++) {
			batchGradients[i] = batchGradients[i] * (1.0f / batch);
		}
	}


	auto T_Delta1 = dml::Identity(averagedDelta);
	for (int i = (int)(Layers.size() - 1); i >= 0; i--)
	{
		Layer& curr = Layers[i];
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

	auto OutputCompiledOperator2 = graph2.Compile(DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION_COMPUTATION, outputs);
	MLOP op2;
	op2.dmlCompiledOperator.Attach(OutputCompiledOperator2.Detach());
	op2.bindings_out = bindings_out;
	op2.bindings_in = bindings_in;
	return op2;




}




/*
/*	Matrix ForwardPropagationRNN(std::vector<Matrix> inputs)
	{
		// Initialize hidden state for all layers
		std::vector<Matrix> hidden_states(Layers.size());
		for (size_t i = 1; i < Layers.size(); i++)
		{
			hidden_states[i] = Matrix(Layers[i].hidden_size, 1); // Initialize to zeros
		}

		// Iterate through the time steps
		for (size_t t = 0; t < inputs.size(); t++)
		{
			// First layer: Input layer, flatten the input
			Layers[0].output = inputs[t].flatten();

			// Forward propagate through all layers
			for (size_t i = 1; i < Layers.size(); i++)
			{
				auto& current = Layers[i];
				auto& prev = Layers[i - 1];

				// Compute input to the current layer (combine input and recurrent weights)
				Matrix input_part = prev.output * current.weights; // W_x * x[t]
				Matrix hidden_part = hidden_states[i] * current.weights_recurrent; // W_h * h[t-1]

				// Update hidden state
				hidden_states[i] = input_part + hidden_part + current.biases;

				// Apply activation function
				if (current.ActType == 0)
					hidden_states[i].sigmoid();
				else
					hidden_states[i].relu();

				// Update current layer's output (for potential future layers)
				current.output = hidden_states[i];
			}
		}

		// Return the output of the last layer at the final time step
		return Layers.back().output;
	}
	*/

	/*
		Matrix ForwardPropagationBatch(const Matrix& inputBatch) {
			// First layer: Assign the input batch to the output of the input layer
			Layers[0].output = inputBatch;

			// Forward propagate through the rest of the layers
			for (size_t i = 1; i < Layers.size(); ++i) {
				auto& current = Layers[i];
				auto& prev = Layers[i - 1];

				// Matrix multiplication: (batch_size x prev_features) * (prev_features x curr_features)
				current.output = prev.output * current.weights;

				// Add biases: Broadcast bias across batch dimension
				current.output += current.biases.broadcast_to(current.output.rows());

				// Apply activation function
				if (current.ActType == 0) {
					current.output.sigmoid();
				}
				else {
					current.output.relu();
				}
			}

			// Return the output of the last layer
			return Layers.back().output;
		}
	void BackPropagationBatch(const std::vector<Matrix>& batchInputs, const std::vector<Matrix>& batchLabels)
	{
		size_t batchSize = batchInputs.size();
		std::vector<Matrix> deltas(batchSize); // To store delta for each sample in the batch
		std::vector<std::vector<Matrix>> gradients(Layers.size(), std::vector<Matrix>(batchSize)); // Store gradients for each layer and sample

		// Initialize outputs for the first layer (input layer)
		Layers[0].output = Matrix::stack(batchInputs); // Stack inputs along batch dimension

		// Calculate deltas for each sample in the batch
		for (size_t b = 0; b < batchSize; b++) {
			deltas[b] = Layers.back().output.row(b) - batchLabels[b]; // Row-wise delta
		}

		// Loop through layers in reverse for backpropagation
		for (int i = (int)(Layers.size() - 1); i > 0; i--)
		{
			Layer& curr = Layers[i];
			Layer& prev = Layers[i - 1];

			// Initialize accumulated gradients for weights and biases
			Matrix accumulatedWeightGradient(curr.weights.rows(), curr.weights.cols(), 0.0f);
			Matrix accumulatedBiasGradient(curr.biases.rows(), curr.biases.cols(), 0.0f);

			// Calculate gradients for each sample in the batch
			for (size_t b = 0; b < batchSize; b++) {
				// Bias update: curr.biases += delta * -learningRate
				accumulatedBiasGradient += deltas[b] * ((float)-curr.lr);

				// Weight gradient: prev.output.T * delta
				Matrix gradient = prev.output.row(b).transpose() * deltas[b];

				// Clip gradients to avoid exploding gradients
				float clip_value = 5.0f;
				for (auto& value : gradient.data()) {
					value = std::max(-clip_value, std::min(value, clip_value));
				}

				// Accumulate gradients across the batch
				accumulatedWeightGradient += gradient;

				// Calculate new delta for backpropagation
				if (i > 1) { // Skip delta calculation for the first layer
					Matrix derivative = prev.output.row(b);
					if (prev.ActType == 0)
						derivative.sigmoid_derivative();
					else
						derivative.relu_derivative();

					deltas[b] = (deltas[b] * curr.weights.transpose()).multiply_inplace(derivative);
				}
			}

			// Average the accumulated gradients over the batch
			accumulatedWeightGradient *= (1.0f / batchSize);
			accumulatedBiasGradient *= (1.0f / batchSize);

			// Update weights and biases
			curr.weights += accumulatedWeightGradient * ((float)-curr.lr);
			curr.biases += accumulatedBiasGradient;
		}
	}
	*/


