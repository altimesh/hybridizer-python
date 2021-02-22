#pragma once

#include "../ImportPython.h"
#include <string>
#include <vector>
#include <map>

#include "HybType.h"

/// repository for types used in the function -- including runtime type defs.
struct TypeHurd
{
	HybType* GetType(PyObject* typeobj);
	HybType* GetType(std::string name);
};

/// arguments to a function
struct CUDAFunctionArgs
{
	std::vector<std::pair<std::string, HybType*>> args;

	int size() const { return (int)(args.size()); }
};

struct CUDAFunction
{
protected:
	std::string name;

public:
	virtual void setname(std::string inputname)
	{
		name = inputname;
	}

	const std::string& getname() const { return name; }

	TypeHurd hurd;
	CUDAFunctionArgs args;

	std::vector<std::string> statements;
};

/// __global__ device-side function
struct CUDAGlobalFunction : public CUDAFunction
{
};

struct CUDAFunctorDef : public CUDAFunction
{
	std::string holdertypename;

	virtual void setname(std::string inputname)
	{
		holdertypename = inputname;
		name = inputname + "::call";
	}
	HybType* returntype;

	virtual PyObject* getcapture() { return nullptr; }
};

/// __device__ device-side function
struct CUDADeviceFunction : public CUDAFunctorDef
{
	PyObject* function;

	virtual PyObject* getcapture() { return nullptr; }
};

/// function generated from a lambda
struct CUDALambdaFunction : public CUDAFunctorDef
{
	PyObject* lambda;

	virtual PyObject* getcapture() 
	{
		PyObject* res = PyObject_GetAttrString(lambda, "__closure__");
		if (res == Py_None)
			return nullptr;
		return res;
	}
};