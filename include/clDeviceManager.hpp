#ifndef CLDEVICEMANAGER_HPP
#define CLDEVICEMANAGER_HPP

#include <fstream>
#include <string>

#include <clBLAS.h>

enum PRG{
	CLMAT_EYE = 0,
	CLMAT_ONES,
	CLMAT_ZEROS,
	CLMAT_HADAMARD,
	CLMAT_SUM,
	CLMAT_SUB,
	CLMAT_SUB_IN,
	FUNC_RELU_DIFF, FUNC_RELU,
	FUNC_SIGMOID_DIFF, FUNC_SIGMOID,
	FUNC_TANH_DIFF, FUNC_TANH,
	FUNC_SOFTSIGN_DIFF, FUNC_SOFTSIGN,
	FUNC_SOFTPLUS_DIFF, FUNC_SOFTPLUS,
	FUNC_POLYNOMIAL_DIFF, FUNC_POLYNOMIAL,
	FUNC_TRUNCATEDPOWER_DIFF, FUNC_TRUNCATEDPOWER,
	FUNC_ABS_DIFF, FUNC_ABS,
	FUNC_SOFTMAX_HELPER, FUNC_SOFTMAX,
	FUNC_SQUARE_DIFF, FUNC_SQUARE,
	FUNC_CROSSENTROPY,
	FULL_APPLY_INIT,
	FULL_DELTA_INIT,
	CONV_APPLY_IMG_SET,
	CONV_APPLY_RET_SET,
	CONV_APPLY_ADD_BIAS,
	CONV_DELTA_KERNEL_SET,
	CONV_DELTA_IMG_SET,
	CONV_GRAD_DELTA_SET,
	CONV_GRAD_IMG_SET,
	MAXPOOL_DELTA,
	MAXPOOL_APPLY,
	AVEPOOL_DELTA,
	AVEPOOL_APPLY,
	BN_GRAD,
	BN_GRAD_HELPER,
	BN_DELTA,
	BN_APPLY_MEAN_VAR,
	BN_APPLY,
	ASSIGN_DATA,
	ADD_L2_REG,
	ADAM,
	ADD_VEC_MAT,
	ADD_SCALAR_MAT,
	MULT_VEC_MAT,
	LENG
};

const static std::string PRG_NAME[] = {
	"clMatrix_eye",
	"clMatrix_ones",
	"clMatrix_zeros",
	"clMatrix_hadamard",
	"clMatrix_sum",
	"clMatrix_sub",
	"clMatrix_sub_in", 
	"function_ReLU_diff", "function_ReLU",
	"function_Sigmoid_diff", "function_Sigmoid",
	"function_Tanh_diff", "function_Tanh",
	"function_Softsign_diff", "function_Softsign",
	"function_Softplus_diff", "function_Softplus",
	"function_Polynomial_diff", "function_Polynomial",
	"function_TruncatedPower_diff", "function_TruncatedPower",
	"function_Abs_diff", "function_Abs",
	"function_Softmax_helper", "function_Softmax",
	"function_Square_diff", "function_Square",
	"function_CrossEntropy",
	"full_apply_init",
	"full_delta_init",
	"conv_apply_img_set",
	"conv_apply_ret_set",
	"conv_apply_add_bias",
	"conv_delta_kernel_set",
	"conv_delta_img_set",
	"conv_grad_delta_set",
	"conv_grad_img_set",
	"maxpool_delta",
	"maxpool_apply",
	"averagepool_delta",
	"averagepool_apply",
	"bn_grad",
	"bn_grad_helper",
	"bn_delta",
	"bn_apply_mean_var",
	"bn_apply",
	"assign_data",
	"add_L2_regular",
	"adam",
	"add_vector_matrix",
	"add_scalar_matrix",
	"mult_vector_matrix"
};

class clDeviceManager
{
private:
	cl_platform_id platform = 0;
    cl_device_id device = 0;
    cl_context ctx = 0;
    cl_command_queue queue = 0;

	size_t maximum_work_item[3];
	size_t maximum_work_group;
	
	cl_kernel kernel[PRG::LENG];
	cl_program program[PRG::LENG];

    cl_context_properties props[3] = { CL_CONTEXT_PLATFORM, 0, 0 };

	std::string read_program ( const std::string& filename );
	int build_program ( const int idx );
public:
	clDeviceManager();
	~clDeviceManager();

	cl_context get_context ();	
	cl_command_queue get_queue ();
	size_t get_max_work_item ( int idx );
	size_t get_max_work_group ();

	cl_command_queue* get_queue_ptr ();

	void run_kernel ( int kernel_idx, size_t gl_work_size1, size_t gl_work_size2 = 1, size_t gl_work_size3 = 1 );
	void set_argument ( int kernel_idx, int arg_idx, const void* val );
	void set_argument ( int kernel_idx, int arg_idx, const size_t size );
}cl_device_manager;

int clDeviceManager::build_program ( const int idx )
{
	std::string source = read_program(PRG_NAME[idx]);
	const char* c_source = source.data();
	size_t source_size = source.size();
	cl_int err;
		
	program[idx] = clCreateProgramWithSource(ctx, 1, &c_source, &source_size, &err);
	err = clBuildProgram(program[idx], 1, &device, NULL, NULL, NULL);
	if( err != 0 ){
		printf("Compile error : %s, error code %d\n", PRG_NAME[idx].c_str(), err);
		char buf[32768];
		clGetProgramBuildInfo(program[idx], device, CL_PROGRAM_BUILD_LOG, 32768, buf, NULL);
		printf("  %s\n", buf);
	}
	kernel[idx] = clCreateKernel(program[idx], PRG_NAME[idx].c_str(), &err);
	if( err != 0 ) printf("Failed CreateKernel : %s, error code %d\n", PRG_NAME[idx].c_str(), err);
}

std::string clDeviceManager::read_program ( const std::string& filename )
{
	const std::string FILE_HEADER = "../include/CL/";
	std::string fn = FILE_HEADER + filename + ".cl";
	std::ifstream ifs(fn);
	std::string ret = "";

	while( !ifs.eof() ){
		std::string buf;
		getline(ifs, buf);
		ret += buf;
	}

	return ret;
}

clDeviceManager::clDeviceManager()
{
	cl_int err;

	err = clGetPlatformIDs( 1, &platform, NULL );
	err = clGetDeviceIDs( platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL );

	props[1] = (cl_context_properties)platform;
	ctx = clCreateContext( props, 1, &device, NULL, NULL, &err );
	queue = clCreateCommandQueue( ctx, device, 0, &err );

    err = clblasSetup();

	clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t)*3, maximum_work_item, NULL);
	clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &maximum_work_group, NULL);

	for( int i = 0; i < PRG::LENG; ++i ){
		build_program(i); //program[i] = NULL;
	}
}

clDeviceManager::~clDeviceManager()
{
	for( int i = 0; i < PRG::LENG; ++i ){
		clReleaseProgram(program[i]);
		clReleaseKernel(kernel[i]);
	}
	
    clblasTeardown( );
    clReleaseCommandQueue( queue );
    clReleaseContext( ctx );
}

cl_context clDeviceManager::get_context ()
{
	return ctx;
}

cl_command_queue clDeviceManager::get_queue ()
{
	return queue;
}

size_t clDeviceManager::get_max_work_item ( int idx )
{
	return maximum_work_item[idx];
}

size_t clDeviceManager::get_max_work_group ()
{
	return maximum_work_group;
}


cl_command_queue* clDeviceManager::get_queue_ptr ()
{
	return &queue;
}

void clDeviceManager::run_kernel ( int kernel_idx, size_t gl_work_size1, size_t gl_work_size2, size_t gl_work_size3 )
{
	if( program[kernel_idx] == NULL ){
		build_program( kernel_idx );
	}

	cl_event event;

	size_t global_work_size[3] = { gl_work_size1, gl_work_size2, gl_work_size3 },
		local_work_size[3] = { 1, 1, 0 };
	
	cl_int err = clEnqueueNDRangeKernel(queue, kernel[kernel_idx], 3, NULL, global_work_size, NULL, 0, NULL, &event);
	if( err != 0 ) printf("Kernel runnning failed : %s, error_code = %d\n", PRG_NAME[kernel_idx].c_str(), err);
	clWaitForEvents(1, &event);
	clReleaseEvent(event);
}

void clDeviceManager::set_argument ( int kernel_idx, int arg_idx, const void* val )
{
	cl_int err = clSetKernelArg(kernel[kernel_idx], arg_idx, sizeof(cl_mem), const_cast<void*>(val));
}

void clDeviceManager::set_argument ( int kernel_idx, int arg_idx, const size_t size )
{
	cl_int err = clSetKernelArg(kernel[kernel_idx], arg_idx, size, NULL);
}

#endif