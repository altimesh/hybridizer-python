/*

Copyright (c) 2017 - 2021 -- ALTIMESH -- all rights reserved

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

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