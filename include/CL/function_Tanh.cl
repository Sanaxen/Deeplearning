#ifndef OCL_EXTERNAL_INCLUDE
#define OCL_EXTERNAL_INCLUDE(...) __VA_ARGS__
#endif

OCL_EXTERNAL_INCLUDE(
__kernel void function_Tanh ( __global float* y, __global float* x )
{
	int gid = get_global_id(0);

	y[gid] = tanh(x[gid]);
}

)
