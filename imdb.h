struct IMDB_LINE
{
	std::wstring text;
	bool Positive = 0;
};


Matrix convert_to_matrix(std::vector<std::vector<float>>& embedded_review, int max_time_steps, int embedding_dim) {
	// Initialize matrix of size (max_time_steps, embedding_dim)
	Matrix result;
	result.init(max_time_steps, embedding_dim, 0.0f); // Zero padding by default

	// Copy embedded review into matrix
	int T = std::min((int)embedded_review.size(), max_time_steps); // Ensure we don't exceed max_time_steps
	for (int t = 0; t < T; t++) {
		for (int d = 0; d < embedding_dim; d++) {
			result.set(t, d,embedded_review[t][d]); // Assign embedding values
		}
	}

	return result;
}


int find_max_time_steps(std::vector<std::vector<int>>& tokenized_reviews, float percentile = 0.95) {
	std::vector<int> lengths;
	for (const auto& review : tokenized_reviews) {
		lengths.push_back((int)review.size());
	}
	std::sort(lengths.begin(), lengths.end());
	int index = (int)(percentile * lengths.size());
	return lengths[index]; // Choose 95th percentile length
}

void imdb_processor(const wchar_t* in_csv)
{
	std::vector<IMDB_LINE> imdb_lines;
	std::ifstream file(in_csv);
	std::string line;
	while (std::getline(file, line))
	{
		std::wstring wline;
		wline.assign(line.begin(), line.end());
		IMDB_LINE il;
		il.text = wline;
		il.Positive = wline.find(L"pos") != std::wstring::npos;
		imdb_lines.push_back(il);
#ifdef _DEBUG
		if (imdb_lines.size() >= 500)
			break;
#endif;
	}

	// Tokenization
	std::map<std::wstring, int> word_to_index;
	int index = 1; // Start indexing from 1 (0 reserved for padding)
	
//	This** creates a dictionary** mapping each word to an integer.s
	for (const auto& review : imdb_lines) {
		std::wstringstream ss(review.text);
		std::wstring word;
		while (ss >> word) {
			if (word_to_index.find(word) == word_to_index.end()) {
				word_to_index[word] = index++;
			}
		}
	}


	std::map<int, std::wstring> index_to_word;
	for (const auto& pair : word_to_index) {
		index_to_word[pair.second] = pair.first;
	}




	
	// Now, convert each review into a** vector of word indices** :
	std::vector<std::vector<int>> tokenized_reviews;

	for (const auto& review : imdb_lines) {
		std::wstringstream ss(review.text);
		std::wstring word;
		std::vector<int> tokens;

		while (ss >> word) {
			tokens.push_back(word_to_index[word]);
		}
		tokenized_reviews.push_back(tokens);
	}

	/*
	
	### **Step 2: Convert to Word Embeddings**
Instead of using raw indices, we replace them with **word embeddings**:
- Word embeddings are **dense vectors** (e.g., 50D, 100D, 300D).
- Each word gets a **fixed-size numerical representation**.

There are **two ways** to create embeddings:
1. **Pre-trained embeddings** (e.g., GloVe, Word2Vec).
2. **Train your own embeddings** using an **embedding matrix**.

*/


	std::map<std::wstring, std::vector<float>> embeddings; // Word to vector mapping

	if (1)
	{
		std::wifstream file2("f:\\WUITOOLS\\nn\\data\\imdb\\glove.6B.100d.txt"); // Pre-trained embeddings file
		std::wstring word;
		float value;
		while (file2 >> word) {
			std::vector<float> vec;
			for (int i = 0; i < 100; i++) { // Assuming 100D embeddings
				file >> value;
				vec.push_back(value);
			}
			embeddings[word] = vec;
		}


		std::vector<std::vector<float>> embedded_reviews;

		for (const auto& tokens : tokenized_reviews) {
			std::vector<float> review_embedding;
			for (int token : tokens) {
				std::wstring word2 = index_to_word[token]; // Reverse lookup
				if (embeddings.find(word2) != embeddings.end()) {
					review_embedding.insert(review_embedding.end(), embeddings[word2].begin(), embeddings[word2].end());
				}
				else {
					std::vector<matrix_t> mm(100);
					review_embedding.insert(review_embedding.end(),mm.begin(),mm.end()); // Default embedding
				}
			}
			embedded_reviews.push_back(review_embedding);
		}
	

		int max_time_steps = find_max_time_steps(tokenized_reviews);
		int embedding_dim = 100;
		for (size_t y = 0; y < embedded_reviews.size(); y++) {
			// Convert current embedded review to matrix

			std::vector<std::vector<float>> review_embeddings;

			// Reshape flat vector into (time_steps, embedding_dim)
			int num_words =(int)(embedded_reviews[y].size() / embedding_dim);
			for (int t = 0; t < num_words; t++) {
				std::vector<float> word_embedding(
					embedded_reviews[y].begin() + t * embedding_dim,
					embedded_reviews[y].begin() + (t + 1) * embedding_dim
				);
				review_embeddings.push_back(word_embedding);
			}

			Matrix input_matrix = convert_to_matrix(review_embeddings, max_time_steps, embedding_dim);

			// Forward Propagation
//			ForwardPropagationRNN(input_matrix);

			// Compute Loss (optional, if needed per sample)
	//		if (y > 0)
		//		totalLoss += error(Layers.back().output, labels[y]);

		//	// Backpropagation Through Time (BPTT)
	//		BackPropagationRNN(labels[y]);
		}

	}


}

void ImdbTest()
{
	imdb_processor(L"f:\\WUITOOLS\\nn\\data\\imdb\\IMDB Dataset.csv");
}


