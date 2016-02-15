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
#include <eigen>
#endif

template<class T>
struct Matrix
{
	int m, n;
#ifdef USE_EIGEN
	Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> v;
#else
	std::vector<T> v;
#endif
	
	Matrix(){}
	Matrix( const int& m, const int& n ) :m(m), n(n)
	{
#ifdef USE_EIGEN
		v = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>(m, n);
		for( int i = 0; i < m; ++i ) for( int j = 0; j < n; ++j ) v(i,j) = T();
#else
		v = std::vector<T>(m*n, T());
#endif
	}

	Matrix( const std::vector<double>& v ):m(v.size()), n(1)
	{
#ifdef USE_EIGEN
		this->v = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>(m, n);
		for( int i = 0; i < m; ++i ) this->v(i, 0) = v[i];
#else
		this->v = std::vector<T>(v.size(), T());
		for( int i = 0; i < m; ++i ) this->v[i] = v[i];
#endif
	}

#ifdef USE_EIGEN
	Matrix( const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>& A ) :v(A), m(A.rows()), n(A.cols()) {}
#endif
	
	static Matrix<T> eye ( const int& m, const int& n )
	{
		Matrix<T> ret(m, n);
		for( int i = 0; i < std::min(m,n); ++i ) ret(i,i) = 1.0;
		return ret;
	}

	static Matrix<T> ones ( const int& m, const int& n )
	{
		Matrix<T> ret(m, n);
		for( int i = 0; i < m; ++i ) for( int j = 0; j < n; ++j ) ret(i,j) = 1.0;
		return ret;
	}

	static Matrix<T> zeros ( const int& m, const int& n )
	{
		return Matrix<T>(m, n);
	}

	static Matrix<T> transpose( const Matrix<T>& mat )
	{
		int i, j;
		int m = mat.m, n = mat.n;
		Matrix<T> ret(n, m);

#pragma omp parallel for default(none) \
	private(i,j) shared(m,n,mat,ret)
		for( i = 0; i < n; ++i ){
			for( j = 0; j < m; ++j ){
				ret(i,j) = mat(j,i);
			}
		}

		return ret;
	}

	static double norm_fro ( const Matrix<T>& mat )
	{
		int m = mat.m, n = mat.n;
		double ret = 0.0;

		for( int i = 0; i < m; ++i )
			for( int j = 0; j < n; ++j )
				ret += mat(i,j)*mat(i,j);

		return sqrt(ret);
	}

	const double& operator () ( int i, int j ) const
	{
#ifdef USE_EIGEN
		return v(i, j);
#else		
		return v[i*n + j];
#endif
	}

	double& operator () ( int i, int j )
	{
#ifdef USE_EIGEN
		return v(i, j);
#else		
		return v[i*n + j];
#endif
	}

	friend Matrix<T> operator + ( const Matrix<T>& m1, const Matrix<T>& m2 )
	{
		int m = m1.m, n = m1.n;
		Matrix<T> ret(m, n);

#ifdef USE_EIGEN
		ret.v = m1.v + m2.v;
#else
		int i, j;

#pragma omp parallel for default(none) \
	private(i,j) shared(m,n,m1,m2,ret)
		for( i = 0; i < m; ++i )
			for( j = 0; j < n; ++j )
				ret(i,j) = m1(i,j) + m2(i,j);
		
#endif
		return ret;
	}
	
	friend Matrix<T> operator - ( const Matrix<T>& m1, const Matrix<T>& m2 )
	{
		int m = m1.m, n = m1.n;
		Matrix<T> ret(m, n);
#ifdef USE_EIGEN
		ret.v = m1.v - m2.v;
#else
		int i, j;
		
#pragma omp parallel for default(none)			\
	private(i,j) shared(m,n,m1,m2,ret)
		for( i = 0; i < m; ++i )
			for( j = 0; j < n; ++j )
				ret(i,j) = m1(i,j) - m2(i,j);
		
#endif
		return ret;
	}

	friend Matrix<T> operator * ( const Matrix<T>& m1, const Matrix<T>& m2 )
	{
		int m = m1.m, n = m2.n, l = m1.n;
		Matrix<T> ret(m, n);

#ifdef USE_EIGEN
		ret.v = m1.v*m2.v;
#else
		int i, j, k;
		double sum;

#pragma omp parallel for default(none) \
	private(i,j,k,sum) shared(m,n,l,m1,m2,ret)
		for( i = 0; i < m; ++i )
			for( j = 0; j < n; ++j ){
				sum = 0.0;
				for( k = 0; k < l; ++k )
					sum += m1(i,k)*m2(k,j);
				ret(i,j) = sum;
			}
		
#endif
		return ret;
	}

	friend Matrix<T> operator * ( const double& c, const Matrix<T>& m1 )
	{
		int m = m1.m, n = m1.n;
		Matrix<T> ret(m, n);
#ifdef USE_EIGEN
		ret.v = c*m1.v;
#else
		int i, j;

#pragma omp parallel for default(none) \
	private(i,j) shared(m,n,c,m1,ret)
		for( i = 0; i < m; ++i )
			for( j = 0; j < n; ++j ){
				ret(i,j) = c*m1(i,j);
			}
		
#endif
		return ret;
	}

	friend std::ostream& operator << ( std::ostream& os, const Matrix<T>& A )
	{
		for( int i = 0; i < A.m; ++i ){
			for( int j = 0; j < A.n; ++j ){
				if( j != 0 ) os << " ";
				os << std::setprecision(3) << std::setw(7) << A(i,j);
			}
			std::cout << std::endl;
		}
		return os;
	}
};

template<class T>
int pivoting ( const Matrix<T>& A, const Matrix<T>& L, const Matrix<T>& U, const int& j )
{
	int m = A.m, n = A.n;
	double max_pivot = -1.0E10;
	int idx = j;

	for( int i = j; i < std::min(m, n); ++i ){
		double sum = 0.0;
		for( int k = 0; k < n; ++k ) max_pivot = max(max_pivot, L(i,k));

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
			for( int j = 0; j < std::min(m,n); ++j ) swap(A(i,j), A(idx,j));
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