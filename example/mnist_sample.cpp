#include <iostream>
#include <fstream>
#include <functional>
#include <memory>
#include <cmath>

#include "../include/Neuralnet.hpp"
#include "../include/Layer/Layer.hpp"
#include "../include/Layer/FullyConnected.hpp"

using namespace std;

// pixel normalize function
void normalize ( vector<Matrix<double>>& image, vector<vector<double>>& ave )
{
    for( int i = 0; i < image.size(); ++i ){
        ave.emplace_back(image[i].m, 0.0);

        for( int j = 0; j < image[i].m; ++j ){
            for( int k = 0; k < image[i].n; ++k ) ave[i][j] += image[i](j,k);
        }
        for( int j = 0; j < image[i].m; ++j ) ave[i][j] /= image[i].n;

        for( int j = 0; j < image[i].m; ++j )
			for( int k = 0; k < image[i].n; ++k )
				image[i](j,k) -= ave[i][j];
    }
}

int main( int argc, char* argv[] )
{
	// define mini-batch size.
	const int BATCH_SIZE = 50;

	// construct neuralnetwork with CrossEntropy.
    Neuralnet<Matrix, double> net(shared_ptr<LossFunction<double>>(new CrossEntropy<double>));
	vector<shared_ptr<Layer<Matrix, double>>> layers;

	// define layers.
	layers.emplace_back(new FullyConnected<Matrix, double>(1, 28*28, 1, 1000, shared_ptr<Function<double>>(new ReLU<double>)));
	layers.emplace_back(new FullyConnected<Matrix, double>(1, 1000, 1, 500, shared_ptr<Function<double>>(new ReLU<double>)));
	layers.emplace_back(new FullyConnected<Matrix, double>(1, 500, 1, 10, shared_ptr<Function<double>>(new Softmax<double>)));

	// this neuralnet has 4 layers, input, convolutional, pooling and FullyConnected.
	for( int i = 0; i < layers.size(); ++i ){
		net.add_layer(layers[i]);
	}
	
	// read a test data of MNIST(http://yann.lecun.com/exdb/mnist/).
	const int N = 10000;
	ifstream train_image("train-images-idx3-ubyte", ios_base::binary);
	if( !train_image.is_open() ){
		cerr << "\"train-images-idx3-ubyte\" is not found!" << endl;
		return 1;
	}
	ifstream train_label("train-labels-idx1-ubyte", ios_base::binary);
	if( !train_label.is_open() ){
		cerr << "\"train-labels-idx1-ubyte\" is not found!" << endl;
		return 1;
	}

	train_image.seekg(4*4, ios_base::beg);
	train_label.seekg(4*2, ios_base::beg);
	vector<Matrix<double>> train_x(1, Matrix<double>(28*28, N)), train_d(1, Matrix<double>(10, N));
	for( int i = 0; i < N; ++i ){
		unsigned char tmp_lab;
		train_label.read((char*)&tmp_lab, sizeof(unsigned char));
		for( int j = 0; j < 10; ++j ) train_d[0](j, i) = 0.0;
		train_d[0](tmp_lab, i) = 1.0;
		
		for( int j = 0; j < 28*28; ++j ){
			unsigned char c;
			train_image.read((char*)&c, sizeof(unsigned char));
			train_x[0](j, i) = (c/255.0);
		}
	}

	vector<vector<double>> ave;
	// normalize train image.
	normalize(train_x, ave);

	// read a train data of MNIST.
	const int M = 5000;
	ifstream test_image("t10k-images-idx3-ubyte", ios_base::binary);
	if( !test_image.is_open() ){
		cerr << "\"t10k-images-idx3-ubyte\" is not found!" << endl;
		return 1;
	}
	ifstream test_label("t10k-labels-idx1-ubyte", ios_base::binary);
	if( !test_label.is_open() ){
		cerr << "\"t10k-labels-idx1-ubyte\" is not found!" << endl;
		return 1;
	}

	test_image.seekg(4*4, ios_base::beg);
	test_label.seekg(4*2, ios_base::beg);
	vector<Matrix<double>> test_x(1, Matrix<double>(28*28, M)), test_d(1, Matrix<double>(10, M));
	for( int i = 0; i < M; ++i ){
		unsigned char tmp_lab;
		test_label.read((char*)&tmp_lab, sizeof(unsigned char));
		for( int j = 0; j < 10; ++j ) test_d[0](j, i) = 0.0;
		test_d[0](tmp_lab, i) = 1.0;
		
		for( int j = 0; j < 28*28; ++j ){
			unsigned char c;
			test_image.read((char*)&c, sizeof(unsigned char));
			test_x[0](j,i) = (c/255.0 - ave[0][j]);
		}
	}
	
	chrono::time_point<chrono::system_clock> prev_time, total_time;
	// checking error function.
	auto check_error = [&](const Neuralnet<Matrix, double>& nn, const int iter, const std::vector<Matrix<double>>& x, const std::vector<Matrix<double>>& d ) -> void {
		if( iter%(N/BATCH_SIZE) != 0 ) return;

		// extracting number of samples from data(for reduciong memory consumption)
		const int once_num = 1000;

		auto tmp_time = chrono::system_clock::now();
		double flops = (double)cnt_flop / (std::chrono::duration_cast<std::chrono::milliseconds>(tmp_time - prev_time).count()/1e3) / 1e9;
		
		// calculate answer rate of train data
		int train_ans_num = 0;
		for( int i = 0; i < N; i += once_num ){
			int size = min(once_num, N - i);
			vector<Matrix<double>> tmp_x(1);
			for( int j = 0; j < train_x.size(); ++j ) tmp_x[j] = train_x[0].sub(0, i, 28*28, size);
			
			auto tmp_y = nn.apply(tmp_x);
			for( int j = 0; j < tmp_y[0].n; ++j ){
				int idx = 0, lab = -1;
				double max_num = tmp_y[0](0, j);

				for( int k = 0; k < 10; ++k ){
					if( max_num < tmp_y[0](k, j) ){
						max_num = tmp_y[0](k, j);
						idx = k;
					}
					if( train_d[0](k, i+j) == 1.0 ) lab = k;
				}
				if( idx == lab ) ++train_ans_num;
			}
		}

		// calculate answer rate of test data
		int test_ans_num = 0;
		for( int i = 0; i < M; i += once_num ){
			int size = min(once_num, M - i);
			vector<Matrix<double>> tmp_x(1);
			for( int j = 0; j < test_x.size(); ++j ) tmp_x[j] = test_x[0].sub(0, i, 28*28, size);
			
			auto tmp_y = nn.apply(tmp_x);
			for( int j = 0; j < tmp_y[0].n; ++j ){
				int idx = 0, lab = -1;
				double max_num = tmp_y[0](0, j);

				for( int k = 0; k < 10; ++k ){
					if( max_num < tmp_y[0](k, j) ){
						max_num = tmp_y[0](k, j);
						idx = k;
					}
					if( test_d[0](k, i+j) == 1.0 ) lab = k;
				}
				if( idx == lab ) ++test_ans_num;
			}
		}

		printf("Iter %5d, Epoch %3d\n", iter, iter/(N/BATCH_SIZE));
		printf("  Elapsed time : %.3f, Total time : %.3f\n",
			   chrono::duration_cast<chrono::milliseconds>(tmp_time - prev_time).count()/1e3,
			   chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - total_time).count()/1e3);
		printf("  Train answer rate %.2f%%\n", (double)train_ans_num/N*100.0);
		printf("  Test answer rate  %.2f%%\n", (double)test_ans_num/M*100.0);
		printf("  %.3f[GFLOPS]\n\n", flops);
		prev_time = chrono::system_clock::now();
		cnt_flop = 0;
	};

	// set supervised data.

	// set a hyper parameter.
	net.set_EPS(1.0E-3);
	net.set_LAMBDA(0.0);
	net.set_BATCHSIZE(BATCH_SIZE);
	// learning the neuralnet in 10 EPOCH and output error defined above in each epoch.
	prev_time = total_time = chrono::system_clock::now();
	net.learning(train_x, train_d, N/BATCH_SIZE*10, check_error);
}
