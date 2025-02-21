#pragma once

#include <vector>
typedef float matrix_t;

class Matrix {
public:
    Matrix(int rows = 0, int cols = 0, matrix_t val = 0)
    {
        _rows = rows;
        _cols = cols;
        _data.resize(rows * cols, val);
    }

    static Matrix Zeros(int rows, int cols)
    {
        Matrix m;
        m.init(rows, cols);
        return m;
    }

    Matrix& init(int rows, int cols, matrix_t val = 0)
    {
        this->_rows = rows;
        this->_cols = cols;
        this->_data = std::vector<matrix_t>(rows * cols, val);
        return *this;
    }

    Matrix& fill(matrix_t val)
    {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] = val;
        }
        return *this;
    }

    Matrix row(int irow)
    {
        Matrix row_data(1,_cols);
        for (int i = 0; i < _cols; i++) {
            row_data.set(1,i,at(irow, i));
        }
        return row_data;
    }

    void print()
    {
        printf("[\n");
        for (int r = 0; r < _rows; r++) {
            printf("  ");
            for (int c = 0; c < _cols; c++) {
                if (c != 0) printf(", ");
                // negative number has extra '-' character at the start.
                matrix_t val = at(r, c);
                if (val >= 0) printf(" %.6f", val);
                else printf("%.6f", val);
            }
            printf("\n");
        }
        printf("]\n");

    }

    // Inplace operations.
    Matrix& operator+=(const Matrix& other)
    {
        [[maybe_unused]] bool cond = (_rows == other._rows && _cols == other._cols);
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] += other._data[i];
        }
        return *this;

    }

    Matrix& operator*=(matrix_t value)
    {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] *= value;
        }
        return *this;

    }


    Matrix& multiply_inplace(const Matrix& other)
    {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] *= other._data[i];
        }
        return *this;

    }

    // Operators that'll return new matrix.
    Matrix operator-(const Matrix& other)
    {
        Matrix m(this->_rows, this->_cols);
        for (size_t i = 0; i < _data.size(); i++) {
            m._data[i] = this->_data[i] - other._data[i];
        }
        return m;

    }

    Matrix operator*(const Matrix& other) const {

        // (r1 x c1) * (r2 x c2) =>
        //   assert(c1 == r2), result = (r1 x c2)

        Matrix m(this->_rows, other._cols);

        int n = _cols; // Width or a row.
        for (int r = 0; r < m._rows; r++) {
            for (int c = 0; c < m._cols; c++) {

                matrix_t val = 0;
                for (int i = 0; i < n; i++) {
                    val += this->at(r, i) * other.at(i, c);
                }
                m.set(r, c, val);
            }
        }

        return m;
    }

    Matrix operator*(matrix_t value) const {
        Matrix m(_rows, _cols);
        std::vector<matrix_t>& m_data = m.data();
        for (size_t i = 0; i < _data.size(); i++) {
            m_data[i] = _data[i] * value;
        }
        return m;
    }




    matrix_t at(int row, int col) const {
        return _data[row * _cols + col];
    }

    void set(int row, int col, matrix_t value) {
        _data[row * _cols + col] = value;
    }

    matrix_t sum() const {
        matrix_t total = 0;
        for (size_t i = 0; i < _data.size(); i++) {
            total += _data[i];
        }
        return total;
    }


    Matrix& randomize_normal(float mean = 0.0f, float stddev = 1.0f) 
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(mean, stddev);

        for (auto& val : _data) {
            val = dist(gen);
        }
        return *this;
    }

    void  rand_he()
    {
        auto n_in = _data.size();
        float stddev = sqrt(2.0f / n_in);
        randomize_normal(0, stddev);
    }

    void xavier_initialization(int n_in, int n_out) {
        float limit = sqrt(6.0f / (n_in + n_out));
        randomize(-limit, limit);
    }

    Matrix& randomize(matrix_t min = 0, matrix_t max = 1)
    {
        for (size_t i = 0; i < _data.size(); i++) {
            matrix_t val = (matrix_t)((matrix_t)rand() / (matrix_t)RAND_MAX) * (max - min) + min;
            _data[i] = val;
        }
        return *this;
    }

    matrix_t sigmoidv(matrix_t x) {
        return 1.f / (1.f + expf(-x));
    }

    Matrix& sigmoid() {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] = sigmoidv(_data[i]);
        }
        return *this;
    }

    Matrix& sigmoid_derivative()
    {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] = _data[i] * (1 - _data[i]);
        }
        return *this;
    }

    Matrix& relu()
    {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] = std::max(0.0f, _data[i]);
        }
        return *this;
    }

    Matrix& leakyrelu()
    {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] = std::max(0.01f, _data[i]);
        }
        return *this;
    }


    Matrix& relu_derivative()
    {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] = _data[i] > 0 ? (matrix_t)1 : (matrix_t)0;
        }
        return *this;
    }

    Matrix& leakyrelu_derivative()
    {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] = _data[i] > 0 ? (matrix_t)1 : (matrix_t)0.01f;
        }
        return *this;
    }

    Matrix& identity()
    {
        return *this;
    }

    Matrix& identity_derivative()
    {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] = 1.0f;
        }
        return *this;
    }


    std::vector<int> argmax_rows() {
        std::vector<int> indices;

        for (int i = 0; i < rows(); i++) {
            int max_index = 0;
            matrix_t max_value = at(i, 0);

            for (int j = 1; j < cols(); j++) {
                if (at(i, j) > max_value) {
                    max_value = at(i, j);
                    max_index = j;
                }
            }

            indices.push_back(max_index);
        }

        return indices;
    }




    Matrix flatten() const {
        int numRows = this->rows();
        int numCols = this->cols();
        Matrix result(1, numRows * numCols);  // Create a 1x784 matrix

        for (int i = 0; i < numRows; ++i) {
            for (int j = 0; j < numCols; ++j) {
                result.set(0, i * numCols + j, this->at(i, j));
            }
        }
        return result;
    }


    std::vector<matrix_t>& data() {
        return _data;
    }


    const std::vector<matrix_t>& data() const {
        return _data;
    }


    int rows() const {
        return _rows;
    }
    int cols() const {
        return _cols;
    }




    Matrix& square() {
        for (size_t i = 0; i < _data.size(); i++) {
            _data[i] = _data[i] * _data[i];
        }
        return *this;
    }













    Matrix transpose() const {
        Matrix m(_cols, _rows);
        for (int r = 0; r < _rows; r++) {
            for (int c = 0; c < _cols; c++) {
                m.set(c, r, at(r, c));
            }
        }
        return m;
    }


    Matrix multiply(const Matrix& other) const {
        Matrix m(_rows, _cols);
        for (size_t i = 0; i < _data.size(); i++) {
            m._data[i] = _data[i] * other._data[i];
        }
        return m;
    }



private:
    int _rows, _cols;
    std::vector<matrix_t> _data;
};












