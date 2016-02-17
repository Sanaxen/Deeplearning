#ifndef POOLING_HPP
#define POOLING_HPP

#include "Layer.hpp"

class Pooling : public Layer
{
private:
	int prev_ldu, ldu;
	int m, n, stlide;
	std::vector<Mat> S;
public:
	Pooling( int prev_num_map, int prev_num_unit, int prev_ldu,
			 int num_map, int num_unit, int ldu,
			 int m, int n, int stlide, 
			 const std::function<double(double)>& activate_func, 
			 const std::function<double(double)>& activate_diff_func );

	void init ( std::mt19937& m );
	void finalize();

	std::vector<std::vector<Mat>> calc_gradient ( const std::vector<Mat>& U, const std::vector<Mat>& delta );
	std::vector<Mat> calc_delta ( const std::vector<Mat>& U, const std::vector<Mat>& delta );
	void update_W ( const std::vector<std::vector<Mat>>& dW );

	std::vector<Mat> apply ( const std::vector<Mat>& U, bool use_func = true );
	std::vector<std::vector<Vec>> apply ( const std::vector<std::vector<Vec>>& u, bool use_func = true );
	std::vector<Mat> unpooling ( const std::vector<Mat>& U );
	std::vector<std::vector<Vec>> unpooling ( const std::vector<std::vector<Vec>>& u );

	void set_W ( const std::string& filename );
	void output_W ( const std::string& filename );
};

Pooling::Pooling( int prev_num_map, int prev_num_unit, int prev_ldu,
				  int num_map, int num_unit, int ldu,
				  int m, int n, int stlide, 
				  const std::function<double(double)>& activate_func, 
				  const std::function<double(double)>& activate_diff_func )
{
	this->prev_num_map = prev_num_map;
	this->prev_num_unit = prev_num_unit;
	this->prev_ldu = prev_ldu;

	this->num_map = num_map;
	this->num_unit = num_unit;
	this->ldu = ldu;

	this->m = m; this->n = n; this->stlide = stlide;

	this->activate_func = activate_func;
	this->activate_diff_func = activate_diff_func;
}

void Pooling::init ( std::mt19937& m )
{
	W = std::vector<std::vector<Mat>>();
}

void Pooling::finalize ()
{
	
}

std::vector<std::vector<Pooling::Mat>> Pooling::calc_gradient ( const std::vector<Mat>& U, const std::vector<Mat>& delta )
{
	return std::vector<std::vector<Mat>>();
}

std::vector<Pooling::Mat> Pooling::calc_delta ( const std::vector<Mat>& U, const std::vector<Mat>& delta )
{
	const int Y = prev_num_unit/prev_ldu, X = prev_ldu;
	std::vector<Mat> nx_delta(prev_num_map);

	int i, j, x, y, s, t;
#pragma omp parallel for default(none) \
	private(i,j,s,t,y,x) shared(Y, X, nx_delta, delta, U)
	for( i = 0; i < prev_num_map; ++i ){
		nx_delta[i] = Mat(U[i].m, U[i].n);
		for( j = 0; j < U[i].n; ++j )
			for( x = 0; x < X; x += stlide )
				for( y = 0; y < Y; y += stlide ){
					int idx1 = x/stlide + y/stlide*ldu, idx2 = x+y*prev_ldu;
					double val = U[i](x+y*prev_ldu,j);
					
					for( s = 0; s < m; ++s )
						for( t = 0; t < n; ++t ){
							int nx = x + s, ny = y + t;
							if( nx < 0 || nx >= X|| ny < 0 || ny >= Y ) continue;
							nx = (x + s)/stlide; ny = (y + t)/stlide;

							if( val < U[i]((x+s)+(y+t)*prev_ldu,j) ){
								idx1 = nx+ny*ldu;
								idx2 = (x+s)+(y+t)*prev_ldu;
								val = U[i](idx2,j);
							}
						}
					nx_delta[i](idx2,j) = delta[i](idx1,j) *
						prev_activate_diff_func(U[i](idx2,j));
				}
	}
	
	return nx_delta;
}

void Pooling::update_W ( const std::vector<std::vector<Mat>>& dW )
{
	
}

std::vector<Pooling::Mat> Pooling::apply ( const std::vector<Mat>& U, bool use_func )
{
	const int Y = prev_num_unit/prev_ldu, X = prev_ldu;
	std::vector<Mat> new_S(num_map, Mat(num_unit, U[0].n));
	std::vector<Mat> ret(num_map);

	int i,j,k,y,x,s,t;
#pragma omp parallel for default(none) \
	private(i,j,y,x,s,t) shared(new_S, Y, X, ret, U)
	for( i = 0; i < num_map; ++i ){
		ret[i] = Mat(num_unit, U[0].n);
		for( j = 0; j < U[0].n; ++j ){
			for( y = 0; y < Y; y += stlide )
				for( x = 0; x < X; x += stlide ){
					int idx = 0;
					double val = U[i](x+prev_ldu*y,j);

					for( s = 0; s < m; ++s )
						for( t = 0; t < n; ++t ){
							int nx = x+s, ny = y+t;
							if( nx < 0 || nx >= X || ny < 0 || ny >= Y ) continue;
							if( val < U[i](nx + ny*prev_ldu,j) ){
								val = U[i](nx + ny*prev_ldu,j);
								idx = nx + ny*prev_ldu;
							}
						}
					ret[i](x/stlide + (y/stlide)*ldu,j) = val;
					new_S[i](x/stlide + (y/stlide)*ldu,j) = idx;
				}
		}
	}

	S = new_S;
	
#pragma omp parallel for default(none) \
	private(i,j,k) shared(ret, use_func)
	for( int i = 0; i < num_map; ++i )
		for( int j = 0; j < ret[i].m; ++j )
			for( int k = 0; k < ret[i].n; ++k )
				ret[i](j,k) = (use_func ? activate_func(ret[i](j,k)) : ret[i](j,k));

	return ret;
}

std::vector<std::vector<Pooling::Vec>> Pooling::apply ( const std::vector<std::vector<Vec>>& u, bool use_func )
{
	std::vector<Mat> tmp(prev_num_map);
	for( int i = 0; i < prev_num_map; ++i )
		tmp[i] = Mat(u[0][0].size(), u.size());

	for( int i = 0; i < prev_num_map; ++i )
		for( int j = 0; j < u[0][0].size(); ++j )
			for( int k = 0; k < u.size(); ++k )
				tmp[i](j,k) = u[k][i][j];
	
	auto U = apply(tmp, use_func);
	std::vector<std::vector<Vec>> ret(U[0].n);
	for( int i = 0; i < U[0].n; ++i ){
		ret[i] = std::vector<Vec>(U.size(), Vec(U[0].m));
		for( int j = 0; j < U.size(); ++j )
			for( int k = 0; k < U[0].m; ++k )
				ret[i][j][k] = U[j](k,i);
	}

	return ret;
}

std::vector<Pooling::Mat> Pooling::unpooling ( const std::vector<Mat>& U )
{
	const int Y = prev_num_unit/prev_ldu, X = prev_ldu;
	std::vector<Mat> ret(num_map);

	int i,j,k,y,x;
#pragma omp parallel for default(none) \
	private(i,j,y,x) shared(Y, X, ret, U)
	for( i = 0; i < num_map; ++i ){
		ret[i] = Mat(prev_num_unit, U[0].n);
		for( j = 0; j < U[0].n; ++j ){
			for( y = 0; y < Y; y += stlide )
				for( x = 0; x < X; x += stlide ){
					int idx = S[i](x/stlide + (y/stlide)*ldu, j);
					double val = U[i](x+prev_ldu*y,j);

					ret[i](idx,j) = U[i](x/stlide + (y/stlide)*ldu,j);
				}
		}
	}

	return ret;
}

std::vector<std::vector<Pooling::Vec>> Pooling::unpooling ( const std::vector<std::vector<Vec>>& u )
{
	std::vector<Mat> tmp(num_map);
	for( int i = 0; i < num_map; ++i )
		tmp[i] = Mat(u[0][0].size(), u.size());

	for( int i = 0; i < num_map; ++i )
		for( int j = 0; j < u[0][0].size(); ++j )
			for( int k = 0; k < u.size(); ++k )
				tmp[i](j,k) = u[k][i][j];
	
	auto U = unpooling(tmp);
	std::vector<std::vector<Vec>> ret(U[0].n);
	for( int i = 0; i < U[0].n; ++i ){
		ret[i] = std::vector<Vec>(U.size(), Vec(U[0].m));
		for( int j = 0; j < U.size(); ++j )
			for( int k = 0; k < U[0].m; ++k )
				ret[i][j][k] = U[j](k,i);
	}

	return ret;
}

void Pooling::set_W ( const std::string& filename )
{
	std::ifstream ifs(filename, std::ios::binary);
}

void Pooling::output_W ( const std::string& filename )
{
	std::ofstream ofs(filename, std::ios::binary);
}

#endif
