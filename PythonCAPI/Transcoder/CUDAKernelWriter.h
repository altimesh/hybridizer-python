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

#include "InferrenceKind.h"
#include "CUDAKernel.h"

#include <string>
#include <map>
#include <stack>
#include <vector>

struct Hybridizer;
struct Disassembler;

class CUDAKernelWriter
{
	Hybridizer* _hybridizer;
	std::vector<std::string> _statements;

	CUDAKernelWriter(std::map<std::string, PyObject*> globals);
	~CUDAKernelWriter();

	std::map<std::string, PyObject*> _globals;
	CUDAGlobalFunction _globalFunction;
	std::map<std::string, CUDADeviceFunction*> _kernelFunctions;

	std::vector<CUDADeviceFunction*> _functionsToProcess;
	CUDAFunction* _currentFunction;

	void EnterFunction(CUDAFunction* func);
	void LeaveFunction();

	std::vector<std::string> ProcessStatements(asdl_seq * stmts);
	std::vector<std::string> ProcessStatement(stmt_ty stmt);
	void ProcessGlobalDefinition(stmt_ty functiondef);

	std::string Express(expr_ty expr);

	CUDAFunctionArgs GetArgs(TypeHurd& hurd, arguments_ty args);

	// module for inspection
	static PyObject* INSPECT_GET_SOURCE ;
	// error stack 
	static std::stack<std::string> ERROR_STACK ;

	std::map<std::string, HybType*> _symbols; // need pointer to return char* on name

	// return a complete cuda module with transcription of an entry point and its dependencies.
	PyObject* GetModule();

	PyObject* GetLambdaCUDA(CUDALambdaFunction* lambda);

public:
	/// utility function - input should be not null and be of Unicode python type
	static std::string TO_STRING(PyObject* unicodetype);

	// return module as a PyDict object
	static PyObject* BuildModule(PyObject* entrypoint);

	// return code snippet for lambda
	static PyObject* BuildLambda(PyObject* lambdaobject);

	// [ONGOING] : disassemble source code
	static PyObject* Disassemble(PyObject* function);

	// return symbol typename
	HybType* GetSymbolType(std::string symbol);

	// HELPER FUNCTION
	static stmt_ty GetFunctionStatementFromSource(PyArena* arena, const std::string& sourcecode);
	// static expr_ty GetLambdaStatementFromSource(PyArena* arena, const std::string& sourcecode);
	static std::string GetSourceCode(PyObject* function);
	static std::map<std::string, PyObject*>  GetGlobals(PyObject* function);

	#pragma region Callbacks from Hybridizer

	// Line printer call-back
	void AppendStatement(const char* line);

	// return symbol mapping
	const char* GetSymbolMap(const char* symbol);

	// gets return-type of symbol from the function name and its parameter list
	const char* GetFunctionReturnType(const char* symbol, int argcount, const char** argtypes);

	// infer type from two input types
	const char* InferType(InferrenceKind infer, const char* left, const char* right);

	// Hybridizer informs that there is a return with the given type
	void DefineFunctionReturn(const char* returntype);

	#pragma endregion
};
