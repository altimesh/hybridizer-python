// (c) ALTIMESH 2019 -- all rights reserved

#include "nvrtc.h" // runtime compilation of C++
#include <cuda.h> // cuda driver 

#include "CUDAJIT.h"

std::pair<std::string, std::string> GeneratePTX(std::string cuda, std::map<std::string, std::string> headers)
{
	nvrtcResult nvres;

	const char** contents = new const char*[headers.size()];
	const char** names = new const char*[headers.size()];

	int k = 0;
	for (auto iter = headers.begin(); iter != headers.end(); ++iter, ++k)
	{
		names[k] = (*iter).first.c_str();
		contents[k] = (*iter).second.c_str();
	}

	nvrtcProgram prog;
	nvres = ::nvrtcCreateProgram(&prog, cuda.c_str(), nullptr, (int)headers.size(), contents, names);

	const char* opts[] = { "--std=c++11" };
	nvres = ::nvrtcCompileProgram(prog, 1, opts);

	size_t ptxSize;
	char *ptx;
	char *log;
	{
		size_t logSize;
		nvres = ::nvrtcGetProgramLogSize(prog, &logSize);
		log = new char[logSize];
		nvres = ::nvrtcGetProgramLog(prog, log);
		// Obtain PTX from the program.

		nvres = ::nvrtcGetPTXSize(prog, &ptxSize);
		ptx = new char[ptxSize];
		nvres = ::nvrtcGetPTX(prog, ptx);
		nvres = ::nvrtcDestroyProgram(&prog);
	}

	std::string resPtx = ptx;
	std::string resLog = log;

	delete[] ptx;
	delete[] log;

	delete[] contents;
	delete[] names;

	return std::pair<std::string, std::string>(resPtx, resLog);
}

#include <vector>

std::pair<std::string, std::string> GenerateModule(std::string ptx, void** res, int* ressize)
{
	CUresult cures;
	CUlinkState lState;
	void *cuOut;
	size_t outSize;

	{
		// from ptxjit
		CUjit_option options[7];
		void *optionVals[7];
		float walltime;
		char error_log[8192], info_log[8192];
		unsigned int logSize = 8192;
		int myErr = 0;

		// Setup linker options
		// Return walltime from JIT compilation
		options[0] = CU_JIT_WALL_TIME;
		optionVals[0] = (void *)&walltime;
		// Pass a buffer for info messages
		options[1] = CU_JIT_INFO_LOG_BUFFER;
		optionVals[1] = (void *)info_log;
		// Pass the size of the info buffer
		options[2] = CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES;
		optionVals[2] = (void*)(size_t)logSize;
		// Pass a buffer for error message
		options[3] = CU_JIT_ERROR_LOG_BUFFER;
		optionVals[3] = (void *)error_log;
		// Pass the size of the error buffer
		options[4] = CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES;
		optionVals[4] = (void *)(size_t)logSize;
		// Make the linker verbose
		options[5] = CU_JIT_LOG_VERBOSE;
		optionVals[5] = (void *)1;

		options[6] = CU_JIT_TARGET;
		optionVals[6] = (void*)CU_TARGET_COMPUTE_50; // TODO: verify this !!


		// Create a pending linker invocation
		cures = cuLinkCreate(6, options, optionVals, &lState);
		if (cures == CUDA_SUCCESS)
			cures = cuLinkAddData(lState, CU_JIT_INPUT_PTX, (void*)(ptx.c_str()), ptx.size(), "jitted.cu", 0, 0, 0);
		if (cures == CUDA_SUCCESS)
			cures = cuLinkComplete(lState, &cuOut, &outSize);

		if (cures != CUDA_SUCCESS)
		{
			*res = nullptr;
			*ressize = 0;
			
		}
		else {
			*res = cuOut;
			*ressize = (int)outSize;
		}
		return std::pair<std::string, std::string>(info_log, error_log);
	}
}

void* MapData(void* source, size_t nbytes)
{
	cuMemHostRegister(source, nbytes, CU_MEMHOSTREGISTER_DEVICEMAP);
	CUdeviceptr ptr;
	cuMemHostGetDevicePointer(&ptr, source, 0);
	return (void*)ptr;
}

void UnMapData(void* source)
{
	cuMemHostUnregister(source);
}