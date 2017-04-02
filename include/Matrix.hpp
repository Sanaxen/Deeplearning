#ifndef MATRIX_HPP
#define MATRIX_HPP

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <algorithm>
#include <functional>

#include <assert.h>

#ifdef USE_EIGEN
#include <Eigen>
#elif USE_BLAS
extern "C"{
	void dgemm_(char* transa, char* transb, int* m, int* n, int* k,
				const double* alpha, const double* A, int* lda, const double* B, int* ldb,
				const double* beta, double* C, int* ldc);
	void sgemm_(char* transa, char* transb, int* m, int* n, int* k,
				const float* alpha, const float* A, int* lda, const float* B, int* ldb,
				const float* beta, float* C, int* ldc);
};
#endif

long long cnt_flop = 0;

template<class T>
struct Matrix;

#include "tMatrix.hpp"

#ifdef USE_GPU
template<class T>
struct clMatrix;
#endif

template<class T>
struct Matrix
{
	int m, n;
	long long mem_size;
#ifdef USE_EIGEN
	Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> v;
#else
	// std::vector<T> v;
	T* v;
#endif
	
	Matrix(): m(0), n(0), v(NULL), mem_size(0) { }
	Matrix( const int& m, const int& n ) :m(m), n(n), mem_size((long long)m*n)
	{
#ifdef USE_EIGEN
		v = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>(m, n);
		for( int i = 0; i < m; ++i ) for( int j = 0; j < n; ++j ) v(i,j) = T();
#else
		// v = std::vector<T>(m*n, T());
		v = new T[m*n];
#endif
	}

	Matrix( const std::vector<T>& v ):m(v.size()), n(1), mem_size((long long)m*n)
	{
#ifdef USE_EIGEN
		this->v = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>(m, n);
		for( int i = 0; i < m; ++i ) this->v(i, 0) = v[i];
#else
		// this->v = std::vector<T>(v.size(), T());
		// for( int i = 0; i < m; ++i ) this->v[i] = v[i];
		this->v = new T[m*n];
		for( int i = 0; i < m; ++i ) this->v[i] = v[i];
#endif
	}

#ifdef USE_EIGEN
	Matrix( const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>& A ) :v(A), m(A.rows()), n(A.cols()), mem_size((long long)m*n) {}
#endif

	Matrix( const Matrix<T>& mat )
	{
		m = mat.m; n = mat.n;
		mem_size = (long long)m*n;
		if( m == 0 || n == 0 ){
			v = NULL;
		}
		else{
			v = new T[m*n];
#pragma omp parallel for
			for( int i = 0; i < m*n; ++i )  v[i] = mat.v[i];
		}
	}

	Matrix<T>& operator = ( const Matrix& mat )
	{
		if( v != NULL ) delete [] v;

		m = mat.m; n = mat.n;
		mem_size = (long long)m*n;
		if( m == 0 || n == 0 ){
			v = NULL;
			return *this;
		}
			
		v = new T[m*n];
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i )  v[i] = mat.v[i];

		return *this;
	}

#ifdef USE_GPU
	Matrix( const clMatrix<T>& mat )
	{
		auto tmp = mat.get_matrix();

		this->m = mat.m; this->n = mat.n;
		mem_size = (long long)m*n;
		if( m == 0 || n == 0 ){
			v = NULL;
		}
		else{
			v = new T[m*n];
#pragma omp parallel for
			for( int i = 0; i < m*n; ++i )  v[i] = tmp.v[i];
		}
	}

	Matrix<T>& operator = ( const clMatrix<T>& mat )
	{
		*this = mat.get_matrix();
		
		return *this;
	}
#endif

	~Matrix ()
	{
		if( v != NULL ) delete [] v;
	}
	
	static Matrix<T> eye ( const int& m, const int& n )
	{
		Matrix<T> ret = Matrix<T>::zeros(m, n);
#pragma omp parallel for
		for( int i = 0; i < std::min(m,n); ++i ) ret(i,i) = 1.0;
		return ret;
	}

	static Matrix<T> ones ( const int& m, const int& n )
	{
		Matrix<T> ret(m, n);
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) ret.v[i] = 1.0;
		return ret;
	}

	static Matrix<T> zeros ( const int& m, const int& n )
	{
		Matrix<T> ret(m, n);
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) ret.v[i] = 0.0;
		return ret;
	}

	static tMatrix<T> transpose( const Matrix<T>& mat )
	{
// 		int m = mat.m, n = mat.n;
// 		Matrix<T> ret(n, m);

// #pragma omp parallel for
// 		for( int i = 0; i < n*m; ++i ){
// 			int idx1 = i/m, idx2 = i%m;
// 			ret(idx1,idx2) = mat(idx2,idx1);
// 		}

// 		return ret;
		return tMatrix<T>(&mat);
	}

	static Matrix<T> hadamard ( const Matrix<T>& m1, const Matrix<T>& m2 )
	{
		int m = m1.m, n = m1.n;
		Matrix<T> ret(m, n);

#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) ret.v[i] = m1.v[i]*m2.v[i];

		return ret;
	}
	
	static T norm_fro ( const Matrix<T>& mat )
	{
		int m = mat.m, n = mat.n;
		T ret = 0.0;

#pragma omp parallel for reduction(+:ret)
		for( int i = 0; i < m*n; ++i ) ret += mat.v[i]*mat.v[i];

		return sqrt(ret);
	}

	static Matrix<T> to_matrix ( const std::vector<Matrix<T>>& tensor )
	{
		int num_map = tensor.size();
		int leng = tensor[0].m;
		Matrix<T> ret(num_map*leng, tensor[0].n);

#pragma omp parallel for
		for( int i = 0; i < num_map; ++i ){
			for( int j = 0; j < leng; ++j ){
				for( int k = 0; k < tensor[i].n; ++k )
					ret(i*leng + j, k) = tensor[i](j, k);
			}
		}

		return ret;
	}

	static std::vector<Matrix<T>> to_tensor ( const Matrix<T>& mat, int num_map )
	{
		std::vector<Matrix<T>> ret(num_map);

		int leng = mat.m / num_map;
		for( int i = 0; i < num_map; ++i ) ret[i] = mat.sub(i*leng, 0, leng, mat.n);

		return ret;
	}		
	
	void apply ( const std::function<double(const double&)>& func )
	{
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) this->v[i] = func(this->v[i]);
	}
	
	const T& operator () ( int i, int j ) const
	{
#ifdef USE_EIGEN
		return v(i, j);
#else
		assert( 0 <= i && i < m );
		assert( 0 <= j && j < n );

		return v[i*n + j];
#endif
	}

	T& operator () ( int i, int j )
	{
#ifdef USE_EIGEN
		return v(i, j);
#else		
		assert( 0 <= i && i < m );
		assert( 0 <= j && j < n );

		return v[i*n + j];
#endif
	}

	Matrix<T>& operator += ( const Matrix<T>& m1 )
	{
		int m = m1.m, n = m1.n;
#ifdef USE_EIGEN
		this->v += m1.v;
#else
// #pragma omp parallel for
// 		for( int i = 0; i < m; ++i )
// 			for( int j = 0; j < n; ++j )
// 				(*this)(i,j) += m1(i,j);
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) this->v[i] += m1.v[i];
#endif
		cnt_flop += (long long)m*n;

		return *this;
	}

	Matrix<T>& operator -= ( const Matrix<T>& m1 )
	{
		int m = m1.m, n = m1.n;
#ifdef USE_EIGEN
		this->v -= m1.v;
#else
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) this->v[i] -= m1.v[i];
#endif
		cnt_flop += (long long)m*n;

		return *this;
	}

	Matrix<T>& operator *= ( const Matrix<T>& m1 )
	{
#ifdef USE_EIGEN
		this->v *= m1.v;
#else
		*this = *this*m1;
#endif
		return *this;
	}

	Matrix<T>& operator *= ( const tMatrix<T>& m1 )
	{
#ifdef USE_EIGEN
		this->v *= m1.v;
#else
		*this = *this*m1;
#endif
		return *this;
	}

	Matrix<T>& operator *= ( const T& c )
	{
#ifdef USE_EIGEN
		this->v *= c;
#else
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) this->v[i] *= c;
#endif
		cnt_flop += (long long)this->m*this->n;

		return *this;
	}
	
	Matrix<T>& operator /= ( const T& c )
	{
#ifdef USE_EIGEN
		this->v /= c;
#else
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) this->v[i] /= c;
#endif
		cnt_flop += (long long)this->m*this->n;

		return *this;
	}

	friend Matrix<T> operator + ( const Matrix<T>& m1, const Matrix<T>& m2 )
	{
		int m = m1.m, n = m1.n;
		Matrix<T> ret(m, n);

#ifdef USE_EIGEN
		ret.v = m1.v + m2.v;
#else
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) ret.v[i] = m1.v[i] + m2.v[i];
#endif
		cnt_flop += (long long)m*n;

		return ret;
	}
	
	friend Matrix<T> operator - ( const Matrix<T>& m1, const Matrix<T>& m2 )
	{
		int m = m1.m, n = m1.n;
		Matrix<T> ret(m, n);
#ifdef USE_EIGEN
		ret.v = m1.v - m2.v;
#else
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) ret.v[i] = m1.v[i] - m2.v[i];
#endif
		cnt_flop += (long long)m*n;

		return ret;
	}

	friend Matrix<T> operator - ( const Matrix<T>& m1 )
	{
		int m = m1.m, n = m1.n;
		Matrix<T> ret(m, n);
#ifdef USE_EIGEN
		ret.v = -m1.v;
#else
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) ret.v[i] = -m1.v[i];
#endif
		cnt_flop += (long long)m*n;

		return ret;
	}

	friend Matrix<T> operator * ( const T& c, const Matrix<T>& m1 )
	{
		int m = m1.m, n = m1.n;
		Matrix<T> ret(m, n);
#ifdef USE_EIGEN
		ret.v = c*m1.v;
#else
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) ret.v[i] = c*m1.v[i];
#endif
		cnt_flop += (long long)m*n;

		return ret;
	}

	friend Matrix<T> operator * ( const Matrix<T>& m1, const T& c )
	{
		return c*m1;
	}
	 
	friend Matrix<T> operator / ( const Matrix<T>& m1, const T& c )
	{
		return (1.0/c)*m1;
	}

	friend std::ostream& operator << ( std::ostream& os, const Matrix<T>& A )
	{
		for( int i = 0; i < A.m; ++i ){
			for( int j = 0; j < A.n; ++j ){
				if( j != 0 ) os << " ";
				os << std::scientific << std::setprecision(3) << std::setw(10) << A(i,j);
			}
			std::cout << std::endl;
		}
		return os;
	}

	Matrix<T> sub ( int y, int x, int h, int w ) const
	{
		Matrix<T> ret(h, w);
#pragma omp parallel for
		for( int i = 0; i < w; ++i )
			for( int j = 0; j < h; ++j )
				ret(j, i) = (*this)(y + j, x + i);

		return ret;
	}

	void sub ( int y, int x, int h, int w, const Matrix<T>& mat )
	{
		Matrix<T> ret(h, w);
#pragma omp parallel for
		for( int i = 0; i < w; ++i )
			for( int j = 0; j < h; ++j )
				(*this)(y+j, x+i) = mat(j, i);
	}

	void clip ( const T& val )
	{
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ){
			if( v[i] > val ) v[i] = val;
			else if( v[i] < -val ) v[i] = -val;
		}
	}

	void mult ( const T& alpha, const Matrix<T>& B, const T& beta, Matrix<T>& C ) const;
	void mult ( const T& alpha, const tMatrix<T>& B, const T& beta, Matrix<T>& C ) const;

	void hadamard ( const Matrix<T>& A )
	{
		int m = this->m, n = this->n;

		assert( this->m == A.m && this->n == A.n );

#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) this->v[i] *= A.v[i];
	}

	void reshape ( int m, int n )
	{
		assert( (long long)m*n <= mem_size );

		this->m = m; this->n = n;
	}

	void copy ( const Matrix<T>& A )
	{
#pragma omp parallel for
		for( int i = 0; i < m*n; ++i ) this->v[i] = A.v[i];
	}
};

template<>
void Matrix<float>::mult ( const float& alpha, const Matrix<float>& B, const float& beta, Matrix<float>& C ) const
{
	int m = this->m, n = B.n, l = this->n;

	assert( this->n == B.m );
	assert( m == C.m );
	assert( n == C.n );

#ifdef USE_BLAS
	char transA = 'N', transB = 'N';
	
	if( m != 0 && n != 0 && l != 0 ){
		sgemm_( &transA, &transB, &n, &m, &l, &alpha,
				&B(0,0), &n, &(*this)(0,0), &l,
				&beta, &C(0,0), &n );
	}
#else
#pragma omp parallel for
	for( int i = 0; i < m; ++i )
		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < l; ++k ) sum += (*this)(i,k)*B(k,j);
			C(i,j) = sum;
		}
#endif
	
	cnt_flop += (long long)m*n*(2*l-1);
}

template<>
void Matrix<double>::mult ( const double& alpha, const Matrix<double>& B, const double& beta, Matrix<double>& C ) const
{
	int m = this->m, n = B.n, l = this->n;

	assert( this->n == B.m );
	assert( m == C.m );
	assert( n == C.n );

#ifdef USE_BLAS
	char transA = 'N', transB = 'N';
	
	if( m != 0 && n != 0 && l != 0 ){
		dgemm_( &transA, &transB, &n, &m, &l, &alpha,
				&B(0,0), &n, &(*this)(0,0), &l,
				&beta, &C(0,0), &n );
	}
#else
#pragma omp parallel for
	for( int i = 0; i < m; ++i )
		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < l; ++k ) sum += (*this)(0i,k)*B(k,j);
			C(i,j) = sum;
		}
#endif
	
	cnt_flop += (long long)m*n*(2*l-1);
}

template<>
void Matrix<float>::mult ( const float& alpha, const tMatrix<float>& B, const float& beta, Matrix<float>& C ) const
{
	int m = this->m, n = B.n, l = this->n;

	assert( this->n == B.m );
	assert( m == C.m );
	assert( n == C.n );

#ifdef USE_BLAS
	int k = B.m;
	char transA = 'T', transB = 'N';
	
	if( m != 0 && n != 0 && l != 0 ){
		sgemm_( &transA, &transB, &n, &m, &l, &alpha,
				&B(0,0), &k, &(*this)(0,0), &l,
				&beta, &C(0,0), &n );
	}
#else
#pragma omp parallel for
	for( int i = 0; i < m; ++i )
		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < l; ++k ) sum += (*this)(i,k)*B(k,j);
			C(i,j) = sum;
		}
#endif
	
	cnt_flop += (long long)m*n*(2*l-1);
}

template<>
void Matrix<double>::mult ( const double& alpha, const tMatrix<double>& B, const double& beta, Matrix<double>& C ) const
{
	int m = this->m, n = B.n, l = this->n;

	assert( this->n == B.m );
	assert( m == C.m );
	assert( n == C.n );

#ifdef USE_BLAS
	int k = B.m;
	char transA = 'T', transB = 'N';
	
	if( m != 0 && n != 0 && l != 0 ){
		dgemm_( &transA, &transB, &n, &m, &l, &alpha,
				&B(0,0), &k, &(*this)(0,0), &l,
				&beta, &C(0,0), &n );
	}
#else
#pragma omp parallel for
	for( int i = 0; i < m; ++i )
		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < l; ++k ) sum += (*this)(i,k)*B(k,j);
			C(i,j) = sum;
		}
#endif
	
	cnt_flop += (long long)m*n*(2*l-1);
}

template<>
void tMatrix<float>::mult ( const float& alpha, const Matrix<float>& B, const float& beta, Matrix<float>& C ) const
{
	int m = this->m, n = B.n, l = this->n;

	assert( this->n == B.m );
	assert( m == C.m );
	assert( n == C.n );

#ifdef USE_BLAS
	char transA = 'N', transB = 'T';
	
	if( m != 0 && n != 0 && l != 0 ){
		sgemm_( &transA, &transB, &n, &m, &l, &alpha,
				&B(0,0), &n, &(*this)(0,0), &m,
				&beta, &C(0,0), &n );
	}
#else
#pragma omp parallel for
	for( int i = 0; i < m; ++i )
		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < l; ++k ) sum += (*this)(i,k)*B(k,j);
			C(i,j) = sum;
		}
#endif
	
	cnt_flop += (long long)m*n*(2*l-1);
}

template<>
void tMatrix<double>::mult ( const double& alpha, const Matrix<double>& B, const double& beta, Matrix<double>& C ) const
{
	int m = this->m, n = B.n, l = this->n;

	assert( this->n == B.m );
	assert( m == C.m );
	assert( n == C.n );

#ifdef USE_BLAS
	char transA = 'N', transB = 'T';
	
	if( m != 0 && n != 0 && l != 0 ){
		dgemm_( &transA, &transB, &n, &m, &l, &alpha,
				&B(0,0), &n, &(*this)(0,0), &m,
				&beta, &C(0,0), &n );
	}
#else
#pragma omp parallel for
	for( int i = 0; i < m; ++i )
		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < l; ++k ) sum += (*this)(i,k)*B(k,j);
			C(i,j) = sum;
		}
#endif
	
	cnt_flop += (long long)m*n*(2*l-1);
}

template<>
void tMatrix<float>::mult ( const float& alpha, const tMatrix<float>& B, const float& beta, Matrix<float>& C ) const
{
	int m = this->m, n = B.n, l = this->n;

	assert( this->n == B.m );
	assert( m == C.m );
	assert( n == C.n );

#ifdef USE_BLAS
	int k = B.m;
	char transA = 'T', transB = 'T';
	
	if( m != 0 && n != 0 && l != 0 ){
		sgemm_( &transA, &transB, &n, &m, &l, &alpha,
				&B(0,0), &k, &(*this)(0,0), &m,
				&beta, &C(0,0), &n );
	}
#else
#pragma omp parallel for
	for( int i = 0; i < m; ++i )
		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < l; ++k ) sum += (*this)(i,k)*B(k,j);
			C(i,j) = sum;
		}
#endif
	
	cnt_flop += (long long)m*n*(2*l-1);
}

template<>
void tMatrix<double>::mult ( const double& alpha, const tMatrix<double>& B, const double& beta, Matrix<double>& C ) const
{
	int m = this->m, n = B.n, l = this->n;

	assert( this->n == B.m );
	assert( m == C.m );
	assert( n == C.n );

#ifdef USE_BLAS
	int k = B.m;
	char transA = 'T', transB = 'T';
	
	if( m != 0 && n != 0 && l != 0 ){
		dgemm_( &transA, &transB, &n, &m, &l, &alpha,
				&B(0,0), &k, &(*this)(0,0), &m,
				&beta, &C(0,0), &n );
	}
#else
#pragma omp parallel for
	for( int i = 0; i < m; ++i )
		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < l; ++k ) sum += (*this)(i,k)*B(k,j);
			C(i,j) = sum;
		}
#endif
	
	cnt_flop += (long long)m*n*(2*l-1);
}

Matrix<double> operator * ( const Matrix<double>& m1, const Matrix<double>& m2 )
{
	int m = m1.m, n = m2.n, l = m1.n;
	Matrix<double> ret(m, n);
#ifdef USE_EIGEN
	ret.v = m1.v*m2.v;
#elif USE_BLAS
	double ONE = 1.0, ZERO = 0.0;
	char transA = 'N', transB = 'N';
	
	if( m != 0 && n != 0 && l != 0 ){
		dgemm_(&transA, &transB, &n, &m, &l, &ONE,
			   &m2(0,0), &n, &m1(0,0), &l,
			   &ZERO, &ret(0,0), &n);
	}
#else
#pragma omp parallel for
	for( int i = 0; i < m; ++i )
		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < l; ++k )
				sum += m1(i,k)*m2(k,j);
			ret(i,j) = sum;
		}
		
#endif
	cnt_flop += (long long)m*n*(2*l-1);

	return ret;
}

Matrix<float> operator * ( const Matrix<float>& m1, const Matrix<float>& m2 )
{
	int m = m1.m, n = m2.n, l = m1.n;
	Matrix<float> ret(m, n);
#ifdef USE_EIGEN
	ret.v = m1.v*m2.v;
#elif USE_BLAS
	float ONE = 1.0, ZERO = 0.0;
	char transA = 'N', transB = 'N';

	if( m != 0 && n != 0 && l != 0 ){
		sgemm_(&transA, &transB, &n, &m, &l, &ONE,
			   &m2(0,0), &n, &m1(0,0), &l,
			   &ZERO, &ret(0,0), &n);
	}
#else
#pragma omp parallel for
	for( int i = 0; i < m; ++i )
		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < l; ++k )
				sum += m1(i,k)*m2(k,j);
			ret(i,j) = sum;
		}
		
#endif
	cnt_flop += (long long)m*n*(2*l-1);

	return ret;
}

Matrix<double> operator * ( const Matrix<double>& m1, const tMatrix<double>& m2 )
{
	int m = m1.m, n = m2.n, l = m1.n;
	Matrix<double> ret(m, n);
#ifdef USE_EIGEN
	ret.v = m1.v*m2.v;
#elif USE_BLAS
	int k = m2.m;
	double ONE = 1.0, ZERO = 0.0;
	char transA = 'T', transB = 'N';
		
	if( m != 0 && n != 0 && l != 0 ){
		dgemm_(&transA, &transB, &n, &m, &l, &ONE,
			   &m2(0,0), &k, &m1(0,0), &l,
			   &ZERO, &ret(0,0), &n);
	}
#else
	int i, j, k;
#pragma omp parallel for default(none)			\
	private(i,j,k) shared(m,n,l,m1,m2,ret)
	for( i = 0; i < m; ++i )
		for( j = 0; j < n; ++j ){
			double sum = 0.0;
			for( k = 0; k < l; ++k )
				sum += m1(i,k)*m2(k,j);
			ret(i,j) = sum;
		}
		
#endif
	cnt_flop += (long long)m*n*(2*l-1);

	return ret;
}
Matrix<float> operator * ( const Matrix<float>& m1, const tMatrix<float>& m2 )
{
	int m = m1.m, n = m2.n, l = m1.n;
	Matrix<float> ret(m, n);
#ifdef USE_EIGEN
	ret.v = m1.v*m2.v;
#elif USE_BLAS
	int k = m2.m;
	float ONE = 1.0, ZERO = 0.0;
	char transA = 'T', transB = 'N';
		
	if( m != 0 && n != 0 && l != 0 ){
		sgemm_(&transA, &transB, &n, &m, &l, &ONE,
			   &m2(0,0), &k, &m1(0,0), &l,
			   &ZERO, &ret(0,0), &n);
	}
#else
	int i, j, k;
#pragma omp parallel for default(none)			\
	private(i,j,k) shared(m,n,l,m1,m2,ret)
	for( i = 0; i < m; ++i )
		for( j = 0; j < n; ++j ){
			double sum = 0.0;
			for( k = 0; k < l; ++k )
				sum += m1(i,k)*m2(k,j);
			ret(i,j) = sum;
		}
		
#endif
	cnt_flop += (long long)m*n*(2*l-1);

	return ret;
}

Matrix<double> operator * ( const tMatrix<double>& m1, const Matrix<double>& m2 )
{
	int m = m1.m, n = m2.n, l = m1.n;
	Matrix<double> ret(m, n);
#ifdef USE_EIGEN
	ret.v = m1.v*m2.v;
#elif USE_BLAS
	int k = m2.m;
	double ONE = 1.0, ZERO = 0.0;
	char transA = 'N', transB = 'T';

	if( m != 0 && n != 0 && l != 0 ){
		dgemm_(&transA, &transB, &n, &m, &l, &ONE,
			   &m2(0,0), &n, &m1(0,0), &m,
			   &ZERO, &ret(0,0), &n);
	}
#else
	int i, j, k;
#pragma omp parallel for default(none)			\
	private(i,j,k) shared(m,n,l,m1,m2,ret)
	for( i = 0; i < m; ++i )
		for( j = 0; j < n; ++j ){
			double sum = 0.0;
			for( k = 0; k < l; ++k )
				sum += m1(i,k)*m2(k,j);
			ret(i,j) = sum;
		}
		
#endif
	cnt_flop += (long long)m*n*(2*l-1);

	return ret;
}
Matrix<float> operator * ( const tMatrix<float>& m1, const Matrix<float>& m2 )
{
	int m = m1.m, n = m2.n, l = m1.n;
	Matrix<float> ret(m, n);
#ifdef USE_EIGEN
	ret.v = m1.v*m2.v;
#elif USE_BLAS
	int k = m2.m;
	float ONE = 1.0, ZERO = 0.0;
	char transA = 'N', transB = 'T';

	if( m != 0 && n != 0 && l != 0 ){
		sgemm_(&transA, &transB, &n, &m, &l, &ONE,
			   &m2(0,0), &n, &m1(0,0), &m,
			   &ZERO, &ret(0,0), &n);
	}
#else
	int i, j, k;
#pragma omp parallel for default(none)			\
	private(i,j,k) shared(m,n,l,m1,m2,ret)
	for( i = 0; i < m; ++i )
		for( j = 0; j < n; ++j ){
			double sum = 0.0;
			for( k = 0; k < l; ++k )
				sum += m1(i,k)*m2(k,j);
			ret(i,j) = sum;
		}
		
#endif
	cnt_flop += (long long)m*n*(2*l-1);

	return ret;
}
	
Matrix<double> operator * ( const tMatrix<double>& m1, const tMatrix<double>& m2 )
{
	int m = m1.m, n = m2.n, l = m1.n;
	Matrix<double> ret(m, n);
#ifdef USE_EIGEN
	ret.v = m1.v*m2.v;
#elif USE_BLAS
	double ONE = 1.0, ZERO = 0.0;
	char transA = 'T', transB = 'T';
		
	if( m != 0 && n != 0 && l != 0 ){
		dgemm_(&transA, &transB, &m, &n, &l, &ONE,
			   &m1(0,0), &l, &m2(0,0), &n,
			   &ZERO, &ret(0,0), &m);
	}
#else
	int i, j, k;
#pragma omp parallel for default(none)			\
	private(i,j,k) shared(m,n,l,m1,m2,ret)
	for( i = 0; i < m; ++i )
		for( j = 0; j < n; ++j ){
			double sum = 0.0;
			for( k = 0; k < l; ++k )
				sum += m1(i,k)*m2(k,j);
			ret(i,j) = sum;
		}
		
#endif
	cnt_flop += (long long)m*n*(2*l-1);

	return ret;
}
Matrix<float> operator * ( const tMatrix<float>& m1, const tMatrix<float>& m2 )
{
	int m = m1.m, n = m2.n, l = m1.n;
	Matrix<float> ret(m, n);
#ifdef USE_EIGEN
	ret.v = m1.v*m2.v;
#elif USE_BLAS
	float ONE = 1.0, ZERO = 0.0;
	char transA = 'T', transB = 'T';
		
	if( m != 0 && n != 0 && l != 0 ){
		sgemm_(&transA, &transB, &m, &n, &l, &ONE,
			   &m1(0,0), &l, &m2(0,0), &n,
			   &ZERO, &ret(0,0), &m);
	}
#else
	int i, j, k;
#pragma omp parallel for default(none)			\
	private(i,j,k) shared(m,n,l,m1,m2,ret)
	for( i = 0; i < m; ++i )
		for( j = 0; j < n; ++j ){
			double sum = 0.0;
			for( k = 0; k < l; ++k )
				sum += m1(i,k)*m2(k,j);
			ret(i,j) = sum;
		}
		
#endif
	cnt_flop += (long long)m*n*(2*l-1);

	return ret;
}

template<class T>
int pivoting ( const Matrix<T>& A, const Matrix<T>& L, const Matrix<T>& U, const int& j )
{
	int m = A.m, n = A.n;
	double max_pivot = L(j,0);
	int idx = j;

	for( int i = j; i < std::min(m, n); ++i ){
		double sum = 0.0;
		for( int k = 0; k < n; ++k ) max_pivot = std::max(max_pivot, L(i,k));

		for( int k = 0; k < i; ++k ) sum += L(i,k)*U(k,j)/max_pivot;

		double x = A(i,j)/max_pivot + sum;
		if( max_pivot < x ){
			max_pivot = x;
			idx = i;
		}
	}

	return idx;
}

template<class T>
void LU_decomp ( Matrix<T> A, Matrix<T>& L, Matrix<T>& U, Matrix<T>& P )
{
	int m = A.m, n = A.n;
	
	if( m > n ){
		L = Matrix<T>::eye(m, n);
		U = Matrix<T>::zeros(n, n);
	}
	else{
		L = Matrix<T>::eye(m, m);
		U = Matrix<T>::eye(m, n);
	}
	P = Matrix<T>::eye(m, m);

	for( int i = 0; i < m; ++i ){
		int idx = pivoting(A, L, U, i);

		if( idx != i ){
			for( int j = 0; j < std::min(m,n); ++j ) std::swap(A(i,j), A(idx,j));
			P(i,i) = P(idx,idx) = 0.0;
			P(i,idx) = P(idx,i) = 1.0;
		}

		for( int j = 0; j < n; ++j ){
			double sum = 0.0;
			for( int k = 0; k < std::min(i,j); ++k ) sum += L(i,k)*U(k,j);

			if( i > j ) L(i,j) = (A(i,j) - sum)/U(j,j);
			else U(i,j) = A(i,j) - sum;
		}
	}
}

template<class T>
Matrix<T> FBS ( const Matrix<T>& L, const Matrix<T>& U, const Matrix<T>& P, Matrix<T> B )
{
	int m = L.m, n = L.n;
	Matrix<T> Y = Matrix<T>::zeros(B.m, B.n), X = Matrix<T>::zeros(B.m, B.n);

	B = Matrix<T>::transpose(P)*B;
	
	for( int i = 0; i < m; ++i ){
		std::vector<T> sum(B.n, T());
		for( int j = 0; j < B.n; ++j ){
			for( int k = 0; k < i; ++k ){
				sum[j] += L(i,k)*Y(k,j);
			}
		}
		
		for( int j = 0; j < B.n; ++j ){
			Y(i,j) = (B(i,j) - sum[j]) / L(i,i);
		}
	}
	
	for( int i = m-1; i >= 0; --i ){
		std::vector<T> sum(B.n, T());
		for( int j = 0; j < B.n; ++j ){
			for( int k = i; k < n; ++k ){
				sum[j] += U(i,k)*X(k,j);
			}
		}
		
		for( int j = 0; j < B.n; ++j ){
			X(i,j) = (Y(i,j) - sum[j]) / U(i,i);
		}
	}
	
	return X;
}

#endif
