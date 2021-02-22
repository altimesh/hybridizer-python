// LINKING ISSUE !! 
// >>> import hybridcuda
// Fatal Python error : PyThreadState_Get: no current thread

#include "ImportPython.h"
#ifndef PYTHON_27

#include "Transcoder/CUDAKernelWriter.h"

#include "Transcoder/HybridPythonResult.h"

PyObject* HybridCUDAError;

// https://docs.python.org/3.6/c-api/arg.html#c.PyArg_ParseTuple
static PyObject* hybridcuda_version(PyObject * self, PyObject *args)
{
	//const char *command;
	int res = 42;

	//if (!PyArg_ParseTuple(args, "s", &command))
	//	return NULL;

	return PyLong_FromLong(res);
}

/*

import inspect
import hybridcuda
import hello_cudapython
hybridcuda.processfunction(hello_cudapython.kernel)

*/
static PyObject* hybridcuda_processfunction(PyObject* self, PyObject* args)
{
	PyObject* method;
	if (!PyArg_ParseTuple(args, "O", &method))
		return nullptr;

	try
	{
		PyObject* kernel = CUDAKernelWriter::BuildModule(method);

		return kernel;
	}
	catch (HybridPythonResult er)
	{
		char buffer[1024];
		::snprintf(buffer, 1024, "EXCEPTION in PROCESS FUNCTION : (%d)", er);
		PyErr_SetString(PyExc_RuntimeError, buffer);
		return nullptr;
	}
	/*
	if (nullptr == kernel)
		return Py_None;
	else
		return PyLong_FromVoidPtr(kernel);
		*/
}

static PyObject* hybridcuda_processlambda(PyObject* self, PyObject* args)
{
	PyArena* arena = nullptr;
	PyObject* result = nullptr;
	CUDAKernelWriter* kw = nullptr;

	PyObject* method;
	if (!PyArg_ParseTuple(args, "O", &method))
		return nullptr;

	try
	{
		PyObject* result = CUDAKernelWriter::BuildLambda(method);

		return result;
	}
	catch (HybridPythonResult er)
	{
		char buffer[1024];
		::snprintf(buffer, 1024, "EXCEPTION in PROCESS LAMBDA : (%d)", er);
		PyErr_SetString(PyExc_RuntimeError, buffer);
		return nullptr;
	}
}

static PyObject* hybridcuda_disassemble(PyObject* self, PyObject* args)
{
	PyObject* method;
	if (!PyArg_ParseTuple(args, "O", &method))
		return nullptr;

	try
	{
		PyObject* result = CUDAKernelWriter::Disassemble(method);

		return result;
	}
	catch (HybridPythonResult er)
	{
		char buffer[1024];
		::snprintf(buffer, 1024, "EXCEPTION in DISASSEMBLE FUNCTION : (%d)", er);
		PyErr_SetString(PyExc_RuntimeError, buffer);
		return nullptr;
	}
}


/*

DEPRECATED

*/
static PyObject* hybridcuda_processfunctionsource(PyObject* self, PyObject* args)
{
	PyObject* method;
	const char* methodsource;

	if (!PyArg_ParseTuple(args, "Os", &method, &methodsource))
		return nullptr;

	PyObject* kernel = CUDAKernelWriter::BuildModule(method);// , methodsource);
	/*
	PyObject* globals = PyFunction_GetGlobals(method);
	if (PyDict_Check(globals) == 0)
		return NULL;

	PyObject* max_iter = PyDict_GetItemString(globals, "max_iter");
	if (PyLong_Check(max_iter) == 0)
		return NULL;

	int value = PyLong_AsLong(max_iter);
	*/
	PyErr_SetString(PyExc_RuntimeError, "DEPRECATED METHOD");

	return nullptr;
}

#include "CUDAJIT.h"

static PyObject* hybridcuda_registerheader(PyObject* self, PyObject* args)
{
	const char* headername;
	const char* headerpath;

	if (!PyArg_ParseTuple(args, "ss", &headername, &headerpath))
		return nullptr ; // TODO: set error

	PyObject* headers = PyDict_GetItemString(PyModule_GetDict(self), "headers");
	if (headers == nullptr)
	{
		headers = PyDict_New();
		PyDict_SetItemString(PyModule_GetDict(self), "headers", headers);
	}

	FILE* f = ::fopen(headerpath, "rb");
	if (f == nullptr)
	{
		PyErr_SetString(PyExc_RuntimeError, "could not open file");
		return nullptr ;
	}
	::fseek(f, 0, SEEK_END);

	int size = ::ftell(f);
	::fseek(f, 0, SEEK_SET);

	char* data = new char[size + 1];
	::fread(data, 1, size, f);
	data[size] = 0;

	::fclose(f);

	PyDict_SetItemString(headers, headername, PyUnicode_FromString(data));
	delete[] data;

	PyErr_Clear();
	return Py_None;
}

std::map<std::string, std::string> GetHybPythonHeaders(PyObject* self)
{
	std::map<std::string, std::string> headers;

	PyObject* hdrs = PyDict_GetItemString(PyModule_GetDict(self), "headers");
	if (hdrs == nullptr)
		return headers;

	PyObject *key, *value;
	Py_ssize_t pos = 0;

	while (PyDict_Next(hdrs, &pos, &key, &value)) {
		headers[std::string(PyUnicode_AsUTF8(key))] = std::string(PyUnicode_AsUTF8(value)) ;
	}

	return headers;
}

/*

import inspect
import hybridcuda
import hello_cudapython
cures = hybridcuda.initcuda()
hc = hybridcuda.processfunction(hello_cudapython.kernel)
hc = hybridcuda.cudajitcode(hc)

*/
static PyObject* hybridcuda_cudajitcode(PyObject* self, PyObject* args)
{
	PyObject* dict;
	if (!PyArg_ParseTuple(args, "O", &dict))
		return NULL;

	PyObject* kernel = dict;

	// cuda source code is in dict["cuda"]
	PyObject* cudaString = PyDict_GetItemString(kernel, "cuda");
	if (PyUnicode_Check(cudaString) != 0)
	{
		// get the cuda source code
		std::string cudasource = CUDAKernelWriter::TO_STRING(cudaString);

		std::map<std::string, std::string> headers = GetHybPythonHeaders(self);

		// compile it
		std::pair < std::string, std::string> ptxlog = GeneratePTX(cudasource, headers);
		
		PyDict_SetItemString(dict, "ptx", PyUnicode_FromString(ptxlog.first.c_str()));
		PyDict_SetItemString(dict, "nvrtclog", PyUnicode_FromString(ptxlog.second.c_str()));
		
		if (ptxlog.first.size() == 0)
		{
			std::string report("ERROR in cuda JIT :\n");
			report.append(ptxlog.second);

			PyErr_SetString(PyExc_RuntimeError, report.c_str());
			return nullptr;
		}

		// since we reuse kernel, increase ref
		Py_XINCREF(kernel);
		return kernel;
	}
	else
	{
		PyErr_SetString(PyExc_RuntimeError, "could not find cuda source code in result");
		return nullptr;
	}
}

#include "nvrtc.h" // runtime compilation of C++
#include <cuda.h> // cuda driver 

/*

import inspect
import hybridcuda
import hello_cudapython
cures = hybridcuda.initcuda()
hc = hybridcuda.processfunction(hello_cudapython.kernel)
hc = hybridcuda.cudajitcode(hc)
hc = hybridcuda.ptxlinkcode(hc)

*/
static PyObject* hybridcuda_ptxlinkcode(PyObject* self, PyObject* args)
{
	PyObject* dict;
	if (!PyArg_ParseTuple(args, "O", &dict))
		return NULL;

	PyObject* kernel = dict;

	// ptx source code is in dict["ptx"]
	PyObject* ptxString = PyDict_GetItemString(kernel, "ptx");

	if (PyUnicode_Check(ptxString) != 0)
	{
		// get the ptx source code
		std::string ptxsource = CUDAKernelWriter::TO_STRING(ptxString);

		int size = 0;
		void* cubinptr;
		std::pair<std::string,std::string> log = GenerateModule(ptxsource, &cubinptr, &size);

		if (cubinptr == nullptr)
		{
			std::string report("ERROR in module generation :\n");
			report.append(log.second);

			PyErr_SetString(PyExc_RuntimeError, report.c_str());
			return nullptr;
		}

		PyObject* cubin = PyMemoryView_FromMemory((char*)cubinptr, size, PyBUF_READ);

		PyDict_SetItemString(dict, "cubin", cubin);
		PyDict_SetItemString(dict, "linkerinfo", PyUnicode_FromString(log.first.c_str()));

		// since we reuse kernel, increase ref
		Py_XINCREF(kernel);
		return kernel;
	}
	else
	{
		PyErr_SetString(PyExc_RuntimeError, "could not find cuda source code in result");
		return nullptr;
	}
}

static PyObject* hybridcuda_initcuda(PyObject* self, PyObject* args)
{
	CUresult cures = ::cuInit(0);
	CUdevice dev;
	CUcontext ctx;

	// create context
	if (cures == CUDA_SUCCESS)
		// at the moment, only use device zero	
		cures = ::cuDeviceGet(&dev, 0);
	if (cures == CUDA_SUCCESS)
		cures = ::cuCtxCreate(&ctx, CU_CTX_MAP_HOST | CU_CTX_SCHED_BLOCKING_SYNC, dev);
	return PyLong_FromLong(cures);
}

struct Marshaller
{

	// TODO : merge with hybpython.cuh => single source code !!!
	struct npndar1D
	{
		double* data;

		int64_t stride; // in bytes
		int64_t shape;
	};

	struct pylist
	{
		int64_t _size;
		thing* data;
	};

	static_assert(sizeof(pylist) == 16, "INCONSISTENT SIZEOF pylist");

	std::map<PyObject*, CUdeviceptr> ghosts;
	std::map<PyObject*, std::vector <void*>> mappeddata;
	std::map<PyObject*, std::vector<void*>> hostgarbage;
	std::map<PyObject*, std::vector<CUdeviceptr>> devicegarbage;

	// at key address, the pointer to the method named by value should be set.
	// for each method symbol, 
	std::map<std::string, std::vector<void**>> devicelinktable;
	std::map<PyObject*, std::string> functioncudacode;

	void FreeGarbage(PyObject* item)
	{
		for (auto iter = mappeddata[item].begin(); iter != mappeddata[item].end(); ++iter)
		{
			UnMapData(*iter);
		}
		mappeddata.erase(item);
		for (auto iter = hostgarbage[item].begin(); iter != hostgarbage[item].end(); ++iter)
		{
			::free(*iter);
		}
		hostgarbage.erase(item);
		for (auto iter = devicegarbage[item].begin(); iter != devicegarbage[item].end(); ++iter)
		{
			::cuMemFree(*iter);
		}
		devicegarbage.erase(item);
	}

	void* MapNumpyArray(PyObject* item)
	{
		auto found = ghosts.find(item);
		if (found != ghosts.end())
			return (void*)((*found).second);

		PyObject* memview = PyObject_GetAttrString(item, "data");
		Py_buffer* data = PyMemoryView_GET_BUFFER(memview);
		// data->buf
		// TODO: assert things...
		// only 1D array at the moment
		npndar1D ar;
		ar.stride = data->strides[0];
		ar.shape = data->shape[0];
		ar.data = (double*)MapData(data->buf, data->len); // data->len is in bytes
		
		CUdeviceptr d_ar;
		if (CUDA_SUCCESS != ::cuMemAlloc(&d_ar, sizeof(npndar1D)))
			throw 1;
		if (CUDA_SUCCESS != ::cuMemcpyHtoD(d_ar, &ar, sizeof(npndar1D)))
			throw 1;

		// no host garbage as we do ZeroCopy
		devicegarbage[item].push_back(d_ar);
		mappeddata[item].push_back(data->buf);

		ghosts[item] = d_ar;
		return (void*)d_ar;
	}

	void UnMapList(PyObject* item)
	{
		int64_t len = (int64_t)PyList_Size(item);
		thing* data = (thing*)hostgarbage[item][0];
		for (int64_t k = 0; k < len; ++k)
		{
			// read-back data
			switch (data[k]._type)
			{
			case pytype::py_int:
				// TODO: decide 32 or 64 bits ints !!!
				if (PyList_SetItem(item, k, PyLong_FromLong(data[k]._int)) != 0)
					throw HybridPythonResult::MARSHALLING_ERROR;
				break;
			case pytype::py_double:
				if (PyList_SetItem(item, k, PyFloat_FromDouble(data[k]._double)) != 0)
					throw HybridPythonResult::MARSHALLING_ERROR;
				break;
			case pytype::py_complex:
				if (PyList_SetItem(item, k, PyComplex_FromDoubles(data[k]._reim[0], data[k]._reim[1])) != 0)
					throw HybridPythonResult::MARSHALLING_ERROR;
				break;
			case pytype::py_bool:
				if (PyList_SetItem(item, k, PyBool_FromLong(data[k]._bool)) != 0)
					throw HybridPythonResult::MARSHALLING_ERROR;
				break;
			#if 0 // TODO: implement function pointer retrieval ??
			case pytype::py_funcptr:
				// for debugging purposes... TODO : remove ! => don't unmarshal this way !!
				if (PyList_SetItem(item, k, PyLong_FromLong((int64_t)(data[k]._func.ptr))) != 0)
					throw HybridPythonResult::MARSHALLING_ERROR;
				break;
			#endif
			default:
				UnMarshal(PyList_GetItem(item,k));
			}
		}
	}

	void* MapList(PyObject* item) // -- static mapping... => cannot modify list length
	{
		auto found = ghosts.find(item);
		if (found != ghosts.end())
			return (void*)((*found).second);

		if (!PyList_Check(item))
			throw HybridPythonResult::INVALID_ARGUMENT;

		CUdeviceptr d_ar;
		if (CUDA_SUCCESS != ::cuMemAlloc(&d_ar, sizeof(pylist)))
			throw HybridPythonResult::MARSHALLING_ERROR;
		ghosts[item] = d_ar; // a list may hold itself ?

		int64_t len = (int64_t)PyList_Size(item);

		thing* data = new thing[len];
		for (int64_t k = 0; k < len; ++k)
		{
			MarshalForType("hybpython::thing", PyList_GetItem(item,k), (char*)(data + k));
		}

		pylist res;
		// first entry is the size
		res._size = len;
		// second entry is an array of thing
		res.data = (thing*)MapData(data, len * sizeof(thing));

		if (CUDA_SUCCESS != ::cuMemcpyHtoD(d_ar, &res, 2 * sizeof(int64_t)))
			throw HybridPythonResult::MARSHALLING_ERROR;

		hostgarbage[item].push_back(data);
		devicegarbage[item].push_back(d_ar);
		mappeddata[item].push_back(data);
		return (void*)d_ar;
	}

	void UnMapTuple(PyObject* item)
	{
		// well tuples seem immutable...
	}

	void* MapTuple(PyObject* item)
	{
		auto found = ghosts.find(item);
		if (found != ghosts.end())
			return (void*)((*found).second);

		if (!PyTuple_Check(item))
			throw HybridPythonResult::INVALID_ARGUMENT;

		CUdeviceptr d_ar;
		if (CUDA_SUCCESS != ::cuMemAlloc(&d_ar, sizeof(pylist)))
			throw HybridPythonResult::MARSHALLING_ERROR;
		ghosts[item] = d_ar; // a list may hold itself ?

		int64_t len = (int64_t)PyTuple_Size(item);

		thing* data = new thing[len];
		for (int64_t k = 0; k < len; ++k)
		{
			MarshalForType("hybpython::thing", PyTuple_GetItem(item, k), (char*)(data + k));
		}

		pylist res;
		// first entry is the size
		res._size = len;
		// second entry is an array of thing
		res.data = (thing*)MapData(data, len * sizeof(thing));

		if (CUDA_SUCCESS != ::cuMemcpyHtoD(d_ar, &res, 2 * sizeof(int64_t)))
			throw HybridPythonResult::MARSHALLING_ERROR;

		hostgarbage[item].push_back(data);
		devicegarbage[item].push_back(d_ar);
		mappeddata[item].push_back(data);
		return (void*)d_ar;
	}

	std::string GetFunctionOrLambdaDeviceName(PyObject* item)
	{
		if (PyObject_HasAttrString(item, "__hybrid_cuda__") != 1)
		{
			CUDAKernelWriter::BuildLambda(item);
		}
		PyObject* hybridcudadict = PyObject_GetAttrString(item, "__hybrid_cuda__");
		if (hybridcudadict == nullptr)
			throw HybridPythonResult::INTERNAL_ERROR_PROCESSING_LAMBDA;
		// add source code 
		PyObject* cudasourcecodepython = PyDict_GetItemString(hybridcudadict, "cuda");
		std::string cudasourcestring = CUDAKernelWriter::TO_STRING(cudasourcecodepython);
		functioncudacode[item] = cudasourcestring;
		
		PyObject* lambdapointername = PyDict_GetItemString(hybridcudadict, "lambdapointername");
		if (lambdapointername == nullptr)
		{
			throw HybridPythonResult::CANNOT_FIND_LAMBDA_POINTER_NAME;
		}
		return CUDAKernelWriter::TO_STRING(lambdapointername);
	}

	char* MarshalForType(const std::string& expectedType, PyObject* item, char* current)
	{
		if (::strcmp(expectedType.c_str(), "hybpython::pylong") == 0)
		{
			int64_t val;
			if (PyLong_Check(item))
			{
				val = PyLong_AsLongLong(item);
				*((int64_t*)current) = val;
				return current + sizeof(int64_t) ;
			}
			else
				throw HybridPythonResult::NOT_IMPLEMENTED;
		}
		else if (::strcmp(expectedType.c_str(), "hybpython::thing") == 0)
		{
			thing t;
			// get the type of the object
			PyObject* itemtype = PyObject_Type(item);
			if (::strcmp(item->ob_type->tp_name, "int") == 0)
			{
				t._type = pytype::py_int;
				t._longlong = PyLong_AsLongLong(item);
				*((thing*)current) = t;
				return current + sizeof(thing);
			}
			else if (::strcmp(item->ob_type->tp_name, "float") == 0)
			{
				t._type = pytype::py_double;
				t._double = PyFloat_AsDouble(item);
				*((thing*)current) = t;
				return current + sizeof(thing);
			}
			else if (::strcmp(item->ob_type->tp_name, "complex") == 0)
			{
				t._type = pytype::py_complex;
				t._reim[0] = PyComplex_RealAsDouble(item);
				t._reim[1] = PyComplex_ImagAsDouble(item);
				*((thing*)current) = t;
				return current + sizeof(thing);
			}
			else if (::strcmp(item->ob_type->tp_name, "numpy.ndarray") == 0)
			{
				// TODO : verify rank
				t._type = (pytype) (pytype::py_numpyndarray_base | 1); // 1D NUMPY ARRAY
				t._ptr = MapNumpyArray(item);
				*((thing*)current) = t;
				return current + sizeof(thing);
			}
			else if (::strcmp(item->ob_type->tp_name, "list") == 0)
			{
				t._type = pytype::py_list;
				t._ptr = MapList(item);
				*((thing*)current) = t;
				return current + sizeof(thing);
			}
			else if (::strcmp(item->ob_type->tp_name, "tuple") == 0)
			{
				t._type = pytype::py_tuple;
				t._ptr = MapTuple(item);
				*((thing*)current) = t;
				return current + sizeof(thing);
			}
			else if (::strcmp(item->ob_type->tp_name, "bool") == 0)
			{
				t._type = pytype::py_bool;
				t._bool = (item == Py_True) ? 1 : 0; // TODO ? check false ???
				*((thing*)current) = t;
				return current + sizeof(thing);
			}
			else if (::strcmp(item->ob_type->tp_name, "function") == 0)
			{
				
				// check if capture is not null
				PyObject* capture = PyObject_GetAttrString(item, "__closure__");
				if (capture != Py_None)
				{	
					t._type = pytype::py_lambda;
					t._lambda.ptr = 0; // TO BE ASSIGNED LATER.
					// marshal closure -- capture should be a tuple !
					t._lambda.capture = MapTuple(capture);
					thing* ptr = (thing*)current;
					// get function or lambda name 
					devicelinktable[GetFunctionOrLambdaDeviceName(item)].push_back(&(ptr->_func.ptr));
					*ptr = t;
				} else {
					thing* ptr = (thing*)current;
					t._type = pytype::py_funcptr;
					t._func.ptr = 0; // TO BE ASSIGNED LATER.
					// get function or lambda name 
					devicelinktable[GetFunctionOrLambdaDeviceName(item)].push_back(&(ptr->_func.ptr));
					*ptr = t;
				}
				return current + sizeof(thing);
			}
			else if (::strcmp(item->ob_type->tp_name, "cell") == 0)
			{
				// simply return its contents (is this boxing ??)
				return MarshalForType(expectedType, PyObject_GetAttrString(item, "cell_contents"), current);
			}
			else
				throw HybridPythonResult::NOT_IMPLEMENTED;
		}
		else
			throw HybridPythonResult::NOT_IMPLEMENTED;
	}

	void UnMarshal(PyObject* item)
	{
		if (ghosts.find(item) == ghosts.end()) return;
		if (::strcmp(item->ob_type->tp_name, "numpy.ndarray") == 0)
		{
			FreeGarbage(item);
			ghosts.erase(item);
		} 
		else if (::strcmp(item->ob_type->tp_name, "list") == 0)
		{
			ghosts.erase(item); // in case it self-refs
			UnMapList(item);
			FreeGarbage(item);
		}
		else if (::strcmp(item->ob_type->tp_name, "tuple") == 0)
		{
			ghosts.erase(item);
			UnMapTuple(item);
			FreeGarbage(item);
		}
		else
		{
			// GHOST NOT FOUND !!!
			throw HybridPythonResult::MARSHALLING_ERROR;
		}
	}
};
/*

import inspect
import hybridcuda
import hello_cudapython
cures = hybridcuda.initcuda()
hc = hybridcuda.processfunction(hello_cudapython.kernel)
hc = hybridcuda.cudajitcode(hc)
hc = hybridcuda.ptxlinkcode(hc)

import numpy as np
a = np.ones(10)
b = np.ones(10)
c = np.zeros(10)
hc = hybridcuda.launch(hc, 1,1,1, 1,1,1, 0,0,  10, a,b,c)

*/
static PyObject* hybridcuda_launch(PyObject* self, PyObject* args)
{
	char kernelparamsbuffer[2048]; // TODO : check for overflow !!
	void* kpaddr[256];

	int argID = 0;

	PyObject* kernel = PyTuple_GetItem(args, argID++);
	int gridx = PyLong_AsLong(PyTuple_GetItem(args, argID++));
	int gridy = PyLong_AsLong(PyTuple_GetItem(args, argID++));
	int gridz = PyLong_AsLong(PyTuple_GetItem(args, argID++));

	int blockx = PyLong_AsLong(PyTuple_GetItem(args, argID++));
	int blocky = PyLong_AsLong(PyTuple_GetItem(args, argID++));
	int blockz = PyLong_AsLong(PyTuple_GetItem(args, argID++));

	int shared = PyLong_AsLong(PyTuple_GetItem(args, argID++));
	CUstream stream = (CUstream)PyLong_AsLongLong(PyTuple_GetItem(args, argID++));


	CUmodule hModule;
	CUfunction hKernel;
	CUresult cures;
	
	// prepare parameters
	Marshaller marshaller;

	char* current = kernelparamsbuffer;

	int firstParamArgID = argID;

	PyObject* arglist = PyDict_GetItemString(kernel, "argtypes");
	for (int k = 0; k < PyList_Size(arglist); ++k)
	{
		try
		{
			PyObject* arg = PyTuple_GetItem(args, argID++);
			std::string expectedType = CUDAKernelWriter::TO_STRING(PyList_GetItem(arglist, k));
			kpaddr[k] = current;
			current = marshaller.MarshalForType(expectedType, arg, current);
		}
		catch (HybridPythonResult er)
		{
			char buffer[1024];
			::snprintf(buffer, 1024, "Error marshalling parameter : %d", er);
			PyErr_SetString(PyExc_RuntimeError, buffer);
			return nullptr;
		}
	}

	// load the module for the function
	void* modulebuffer = nullptr;

	// check if supplemental code is needed
	if (marshaller.functioncudacode.size() != 0)
	{
		// here, we get the cuda code (may think things otherwise with libraries...
		PyObject* kernelcuda = PyDict_GetItemString(kernel, "cuda");
		std::string kernelsourcestring = CUDAKernelWriter::TO_STRING(kernelcuda);

		std::map<std::string, std::string> headers = GetHybPythonHeaders(self);

		std::string cudasource;

		cudasource.append(kernelsourcestring);
		for (auto iter = marshaller.functioncudacode.begin(); iter != marshaller.functioncudacode.end(); ++iter)
		{
			cudasource.append(iter->second);
		}

		// compile it
		std::pair < std::string, std::string> ptxlog = GeneratePTX(cudasource, headers);
		std::string ptxsource = ptxlog.first;
		if (ptxsource.size() == 0)
		{
			std::string report("ERROR in JITing code with lambdas :\n");
			report.append(ptxlog.second);
			PyErr_SetString(PyExc_RuntimeError, report.c_str());
			return nullptr;
		}

		
		// link it
		int size = 0;
		void* cubinptr;
		std::pair<std::string, std::string> log = GenerateModule(ptxsource, &cubinptr, &size);
		if (cubinptr == nullptr)
		{
			std::string report("ERROR in module generation :\n");
			report.append(log.second);

			PyErr_SetString(PyExc_RuntimeError, report.c_str());
			return nullptr;
		}

		// we have our cubin =>  load it
		modulebuffer = cubinptr;

	} else {
		PyObject* kernelimage = PyDict_GetItemString(kernel, "cubin");
		Py_buffer* image = PyMemoryView_GET_BUFFER(kernelimage);
		modulebuffer = image->buf;
	}

	if (true)
	{
		cures = cuModuleLoadData(&hModule, modulebuffer);
		if (cures != 0) { PyErr_SetString(PyExc_RuntimeError, "Error loading module"); return nullptr; }

		PyObject* kernelnameobject = PyDict_GetItemString(kernel, "kernelname");
		std::string kernelname = CUDAKernelWriter::TO_STRING(kernelnameobject);

		cures = cuModuleGetFunction(&hKernel, hModule, kernelname.c_str());
		if (cures != 0) { PyErr_SetString(PyExc_RuntimeError, "Error finding function in module"); return nullptr; }
	}

	// solve device link table -> a.k.a. .reloc ?
	for (auto iter = marshaller.devicelinktable.begin(); iter != marshaller.devicelinktable.end(); ++iter)
	{
		// get device pointer
		CUdeviceptr funcptrdev;
		size_t ptrsize;
		cures = cuModuleGetGlobal(&funcptrdev, &ptrsize, hModule, iter->first.c_str());
		if (cures != 0) { PyErr_SetString(PyExc_RuntimeError, "Cannot find symbol to function pointer"); return nullptr; }
		if (ptrsize != sizeof(void*))
		{
			PyErr_SetString(PyExc_RuntimeError, "CANNOT RECOVER MARSHALLED FUNCTION POINTER");
			return nullptr;
		}
		// this is the pointer to d_func, we want to read from it
		void* funcptr;
		cures = cuMemcpyDtoH(&funcptr, funcptrdev, ptrsize);
		if (cures != 0) { PyErr_SetString(PyExc_RuntimeError, "Cannot read function pointer"); return nullptr; }

		for (int k = 0; k < iter->second.size(); ++k)
		{
			*(iter->second[k]) = funcptr;
		}
	}

	// call kernel
	cures = cuLaunchKernel(hKernel, gridx, gridy, gridz, blockx, blocky, blockz, shared, stream, kpaddr, nullptr);
	// TODO : decent error report
	if (cures != 0) { PyErr_SetString(PyExc_RuntimeError, "Error launching kernel"); return nullptr; }
	cures = ::cuCtxSynchronize();
	if (cures != CUDA_SUCCESS) 
	{ 
		char message[1024];
		::sprintf(message, "ERROR from cuCtxSynchronize : %d", cures);
		PyErr_SetString(PyExc_RuntimeError, message);
		return nullptr; 
	}

	// unmap !
	for (int k = 0; k < PyList_Size(arglist); ++k)
	{
		try
		{
			PyObject* arg = PyTuple_GetItem(args, firstParamArgID + k);
			marshaller.UnMarshal(arg);
		}
		catch (HybridPythonResult er)
		{
			char buffer[1024];
			::snprintf(buffer, 1024, "Error unmarshalling parameter : %d", er);
			PyErr_SetString(PyExc_RuntimeError, buffer); 
			return nullptr;
		}
	}


	// since we reuse kernel, increase ref
	Py_XINCREF(kernel);
	return kernel;
}

/*

import hybridcuda
import numpy as np
a = np.zeros(5)
hybridcuda.inspect(a)

*/
static PyObject* hybridcuda_inspect(PyObject* self, PyObject* args)
{
	PyObject* param;
	if (!PyArg_ParseTuple(args, "O", &param))
		return NULL;

	return PyLong_FromLong(42);
}

static PyMethodDef HybridCUDAMethods[] = {
	// ...
	{"version",  hybridcuda_version, METH_VARARGS, "Return version."},
	{"processfunction",  hybridcuda_processfunction, METH_VARARGS, "Process a function."},
	{"processlambda",  hybridcuda_processlambda, METH_VARARGS, "Process a lambda."},
	{"processfunctionsource",  hybridcuda_processfunctionsource, METH_VARARGS, "Process a function source code."},
	{"initcuda", hybridcuda_initcuda, METH_VARARGS, "Initialize CUDA context." },
	{"cudajitcode", hybridcuda_cudajitcode, METH_VARARGS, "JIT cuda source code." },
	{"ptxlinkcode", hybridcuda_ptxlinkcode, METH_VARARGS, "Link ptx code to a module code." },
	{"registerheader", hybridcuda_registerheader, METH_VARARGS, "Register header for JIT." },
	{"launch", hybridcuda_launch, METH_VARARGS,
		"Launch kernel. \n"
		"usage: \n"
		"    launch (hc\n"
		"			,gridDimX,gridDimY,gridDimZ\n"
		"			,blockDimX,blockDimY,blockDimZ\n"
		"			,shared,stream,\n"
		"			,*{kernelfunctionarguments})\n"
		"where: \n"
		"	gridDimX,gridDimY,gridDimZ define grid configuration "
		"	blockDimX,blockDimY,blockDimZ define block configuration "
		"	shared,stream define shared memory amount (in bytes) and cuda stream "
		"	*{kernelfunctionarguments} stands for all arguments in function call"
		""},
	{"disassemble", hybridcuda_disassemble, METH_VARARGS, "INTERNAL USE ONLY." },
	{"inspect", hybridcuda_inspect, METH_VARARGS, "INTERNAL USE ONLY." },

	 // ...
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef hybridcudamodule = {
	PyModuleDef_HEAD_INIT,
	"hybridcuda",   /* name of module */
	NULL, // spam_doc, /* module documentation, may be NULL */
	-1,       /* size of per-interpreter state of the module,
				 or -1 if the module keeps state in global variables. */
	HybridCUDAMethods
};

PyMODINIT_FUNC
PyInit_hybridcuda(void)
{
	PyObject *m;

	m = PyModule_Create(&hybridcudamodule);
	if (m == NULL)
		return NULL;

	HybridCUDAError = PyErr_NewException("hybridcuda.error", NULL, NULL);
	Py_INCREF(HybridCUDAError);
	PyModule_AddObject(m, "error", HybridCUDAError);
	return m;
}

#endif