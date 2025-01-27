#include "mlbuffer.h"

const wchar_t* s(size_t idx);
inline void nop()
{

}

class Layer
{
public:

	bool Sel = 0;

	int ActType = 0; // 0 sigmoid 1 ralu
	Matrix output;
	double lr = 0.1;
	Matrix weights;
	Matrix weights_recurrent_hidden;
	Matrix weights_recurrent_output;
	Matrix biases_recurrent;
	Matrix biases;
	void Save(XML3::XMLElement& e);
	void Load(XML3::XMLElement& e);


	// DirectML structure tensors

	MLBUFFER WeightsBPIn;
	MLBUFFER BiasesBPIn;


	dml::Expression T_OutputsBPIn;
	MLBUFFER B_Weights;
	MLBUFFER B_WeightsOut;
	MLBUFFER B_Biases;
	MLBUFFER B_BiasesOut;
	MLBUFFER B_Outputs;

	// A Layer is a collection of neurons
	Layer(double lrate, int ActivationType, int num_neurons, int num_weights_per_neuron);




};


class DataSet
{
public:

	Matrix input;
	Matrix label;
};

struct MLOP;

class NN
{
public:
	std::vector<Layer> Layers;

	NN();
	void RecalculateWB();
	void Save(XML3::XMLElement& e);
	void Load(XML3::XMLElement& e);

	int RNN_hidden_size = 32;

	Matrix ForwardPropagation(Matrix input);
	Matrix ForwardPropagationRNN(Matrix input);

	// Mean Squared Error function
	// MSE = 1/n * Σ(y - y')^2
	// y is the output, y' is the expected (label) output, n is the number of samples
	//  It's suitable for regression tasks but not typically used for classification tasks, where cross-entropy loss is more appropriate.
	float error(Matrix& out, Matrix& exp) {
		return (out - exp).square().sum() / out.cols();
	}

	void BackPropagation(Matrix label);

	float accuracy = 0;
	float track_accuracy(std::vector<DataSet>& dataset);

	MLOP GetTrainOperator(ML& ml, unsigned int batch, int& starttid, std::vector<DataSet>& data, MLBUFFER& B_InputOp1, MLBUFFER& B_InputOp2, MLBUFFER& B_Label, MLBUFFER& B_Final, bool FP, bool BP, dml::Graph* g1, dml::Graph* g2);
	HRESULT dmltrain3(std::vector<DataSet> data, unsigned int batch, int numEpochs);
	void MatrixToClipboard(ID3D12Device* dev, ID3D12GraphicsCommandList* commandLit, ML* ml, MLBUFFER& m);
	void MatrixToClipboard(Matrix& m);

	std::function<HRESULT(int iEpoch, int iDataset, int step, float totalloss, float acc, void* param)> cbf;
	void* paramcbf = 0;

	void train(std::vector<DataSet> ds, int numEpochs, bool AndAccuracy);
	void test_network(std::vector<DataSet>& test_dataset);

};


struct PROJECT
{
	std::string type; // "mnist" defined
	std::array<std::wstring, 4> if_mnist;
	std::vector<DataSet> data_set;
	std::vector<DataSet> test_set;
	::NN nn;
	void Save(XML3::XML& x);
};

