#ifndef OCL_EXTERNAL_INCLUDE
#define OCL_EXTERNAL_INCLUDE(...) __VA_ARGS__
#endif

OCL_EXTERNAL_INCLUDE(
	__kernel void function_LeakyReLU ( __global float* x, __global float* alpha )
{
	int gid = get_global_id(0);

	x[gid] = (x[gid] <= 0.0 ? *alpha : 1.0f) * x[gid];
}

)
