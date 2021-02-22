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

#include "CUDAKernelWriter.h"

#include <pyarena.h>

#include "HybridPythonResult.h"

// for some reason organization of headers is a mess.... Use opaque function decls
extern "C" __declspec(dllexport) Hybridizer* _cdecl HybridPython_CreateHybridizer(void* handle, void* fdeclcb, void* fbodycb, void* kernelcb, void* tn, void* sm, void* frt, void* at, void* it, void* ast, void* df, void* rcb);
extern "C" __declspec(dllexport) void _cdecl HybridPython_DestroyHybridizer(Hybridizer* hyb);
extern "C" __declspec(dllexport) void _cdecl HybridPython_EnterFunctionScope(Hybridizer* hyb);
extern "C" __declspec(dllexport) void _cdecl HybridPython_LeaveFunctionScope(Hybridizer* hyb);
extern "C" __declspec(dllexport) void _cdecl HybridPython_DefineSymbol(Hybridizer* hyb, std::string symbol, HybType* t);

extern "C" __declspec(dllexport) Disassembler* _cdecl HybridPython_CreateDisassembler(PyObject* o);
extern "C" __declspec(dllexport) stmt_ty _cdecl HybridPython_DisassemblerProcess(Disassembler* disasm);
extern "C" __declspec(dllexport) void _cdecl HybridPython_DestroyDisassembler(Disassembler* disasm);
extern "C" __declspec(dllexport) HybType* _cdecl HybridPython_DisassemblerGetReturnType(Disassembler* disasm);

PyObject* CUDAKernelWriter::INSPECT_GET_SOURCE = nullptr;
std::stack<std::string> CUDAKernelWriter::ERROR_STACK ;

std::string CUDAKernelWriter::TO_STRING(PyObject* str)
{
	if (nullptr == str)
	{
		ERROR_STACK.push("Null in " __FUNCTION__);
		throw HybridPythonResult::NULL_POINTER_EXCEPTION;
	}
	if (PyUnicode_Check(str) == 0)
	{
		ERROR_STACK.push("Not a string type in " __FUNCTION__);
		throw 1;
	}
	std::string sourcestr = "";
#ifdef PYTHON_27
	memcpy((void*) sourcestr.c_str(), (const char*)PyUnicode_AsUnicode(str), PyUnicode_GetSize(str));
#else
	switch (PyUnicode_KIND(str))
	{
		case PyUnicode_Kind::PyUnicode_1BYTE_KIND:
			sourcestr.append((const char*)PyUnicode_1BYTE_DATA(str), (size_t)PyUnicode_GET_LENGTH(str));
			break;
		default:
			ERROR_STACK.push("Invalid string type in " __FUNCTION__);
			throw 1;
	}
#endif
	return sourcestr;
}

std::string CUDAKernelWriter::GetSourceCode(PyObject* function)
{
	if (nullptr == function)
	{
		ERROR_STACK.push("nullpointer in " __FUNCTION__);
		throw 1;
	}
	if (0 == PyFunction_Check(function))
	{
		ERROR_STACK.push("Expected a function object in " __FUNCTION__);
		throw 1;
	}

	if (nullptr == INSPECT_GET_SOURCE)
	{
		PyObject* inspect = PyImport_AddModule("inspect");
		PyObject* inspect_dict = PyModule_GetDict(inspect);
		PyObject* getsource = PyDict_GetItemString(inspect_dict, "getsource");
		INSPECT_GET_SOURCE = getsource ;
	}

	PyObject* arglist = Py_BuildValue("(O)", function);

	PyObject* source = PyEval_CallObject(INSPECT_GET_SOURCE, arglist);
	
	if (source == nullptr)
	{
		ERROR_STACK.push("inspect.getsource returned null !");
		throw HybridPythonResult::CANNOT_GET_SOURCECODE;
	}

	return TO_STRING(source);
}

// get statements from source code
stmt_ty CUDAKernelWriter::GetFunctionStatementFromSource(PyArena* arena, const std::string& sourcecode)
{
	mod_ty m = PyParser_ASTFromString(sourcecode.c_str(), "@source@.py", Py_file_input, nullptr, arena);
	if (m == nullptr)
		ThrowNotImplemented();
	asdl_seq * statements = nullptr;

	switch (m->kind)
	{
	case _mod_kind::Module_kind:
		statements = m->v.Module.body; break;
	case _mod_kind::Interactive_kind:
		statements = m->v.Interactive.body; break;
	case _mod_kind::Suite_kind:
		statements = m->v.Suite.body; break;
	default:
		ERROR_STACK.push("Invalid code snippet kind in " __FUNCTION__);
		throw 1;
	}

	// verify that it is only one statement which is a function def
	if (statements->size != 1)
	{
		ERROR_STACK.push("Build kernel called with source code which is not a single function def in " __FUNCTION__);
		throw 1;
	}
	stmt_ty stmt = (stmt_ty)statements->elements[0];
	if (stmt->kind != _stmt_kind::FunctionDef_kind)
	{
		ERROR_STACK.push("Build kernel called with source code which is not a function definition in " __FUNCTION__);
		throw 1;
	}

	return stmt;
}

#include "HybridPythonResult.h"

#if 0
// get statements from source code
expr_ty CUDAKernelWriter::GetLambdaStatementFromSource(PyArena* arena, const std::string& sourcecode)
{
	mod_ty m = PyParser_ASTFromString(sourcecode.c_str(), "@source@.py", Py_file_input, nullptr, arena);
	if (m == nullptr)
	{
		ERROR_STACK.push("Could not parse code snippet kind in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}

	asdl_seq * statements = nullptr;

	switch (m->kind)
	{
	case _mod_kind::Module_kind:
		statements = m->v.Module.body; break;
	case _mod_kind::Interactive_kind:
		statements = m->v.Interactive.body; break;
	case _mod_kind::Suite_kind:
		statements = m->v.Suite.body; break;
	default:
		ERROR_STACK.push("Invalid code snippet kind in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}

	// verify that it is only one statement which is a function def
	if (statements->size != 1)
	{
		ERROR_STACK.push("Build kernel called with source code which is not a single function def in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	stmt_ty stmt = (stmt_ty)statements->elements[0];
	if (stmt->kind == _stmt_kind::Assign_kind)
	{
		if (stmt->v.Assign.targets->size != 1)
		{
			ERROR_STACK.push("Several targets on lambda construction not supported in " __FUNCTION__);
			throw HybridPythonResult::NOT_IMPLEMENTED;
		}
		return (expr_ty)(stmt->v.Assign.value);
	}
	else
	{
		ERROR_STACK.push("lambda declaration use-case not supported in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}

	return nullptr;
}
#endif

// TODO : HANDLE borrowed reference and new reference properly !
#include "HybridPythonResult.h"

void CUDAKernelWriter::EnterFunction(CUDAFunction* func)
{
	_currentFunction = func;
	HybridPython_EnterFunctionScope(_hybridizer);
}

void CUDAKernelWriter::LeaveFunction()
{
	_currentFunction = nullptr;
	HybridPython_LeaveFunctionScope(_hybridizer);
}

std::string JSONSerialize(stmt_ty statement);

PyObject* CUDAKernelWriter::Disassemble(PyObject* function)
{
	PyObject* result = PyDict_New();

	{
		try
		{
			PyObject* code = PyObject_GetAttrString(function, "__code__");
			Disassembler* disasm = HybridPython_CreateDisassembler(code);
			stmt_ty func = HybridPython_DisassemblerProcess(disasm);
			std::string res = JSONSerialize(func);

			HybridPython_DestroyDisassembler(disasm);
#ifdef PYTHON_27
			PyDict_SetItemString(result, "hybrid", PyUnicode_FromString(res.c_str()));
#else
			PyDict_SetItemString(result, "hybrid", _PyUnicode_FromASCII(res.c_str(), res.length()));
#endif
		}
		catch (HybridPythonResult)
		{ 
			PyErr_Clear();
#ifdef PYTHON_27
			PyDict_SetItemString(result, "hybrid", PyUnicode_FromString("<EXCEPTION>"));
#else
			PyDict_SetItemString(result, "hybrid", _PyUnicode_FromASCII("<EXCEPTION>", ::strlen("<EXCEPTION>")));
#endif
		}
	}

	{
		try
		{
			// get source code
			std::string sourcecode = GetSourceCode(function);

			// create py arena and process source code
			PyArena* arena = PyArena_New();

			stmt_ty func = GetFunctionStatementFromSource(arena, sourcecode);
			std::string res = JSONSerialize(func);

			PyArena_Free(arena);
#ifdef PYTHON_27
			PyDict_SetItemString(result, "inspect", PyUnicode_FromString(res.c_str()));
#else
			PyDict_SetItemString(result, "inspect", _PyUnicode_FromASCII(res.c_str(), res.length()));
#endif
		}
		catch (HybridPythonResult)
		{
			PyErr_Clear();
#ifdef PYTHON_27
			PyDict_SetItemString(result, "inspect", PyUnicode_FromString("<EXCEPTION>"));
#else
			PyDict_SetItemString(result, "inspect", _PyUnicode_FromASCII("<EXCEPTION>", ::strlen("<EXCEPTION>")));
#endif
		}
	}

	return result;
}

std::map<std::string, PyObject*>  CUDAKernelWriter::GetGlobals(PyObject* function)
{
	// get globals visible from function
	PyObject* globals = PyFunction_GetGlobals(function);
	if (PyDict_Check(globals) == 0)
		throw HybridPythonResult::INVALID_ARGUMENT;

	// EXPLORE globals -- we have functions !
	std::map<std::string, PyObject*> m_globals;
	// https://docs.python.org/3.8/c-api/dict.html
	PyObject* keys = PyDict_Keys(globals); // TODO: check what to do if keys is borrowed or not...
	{
		for (Py_ssize_t i = 0; i < PyList_Size(keys); ++i)
		{
			std::string name = TO_STRING(PyList_GetItem(keys, i));
			m_globals[name] = PyDict_GetItemString(globals, name.c_str());
		}
	}
	return m_globals;
}

PyObject* CUDAKernelWriter::BuildLambda(PyObject* lambda)
{
	PyObject* result = nullptr;
	CUDAKernelWriter* kw = nullptr;
	Disassembler* disasm = nullptr;

	try
	{
		std::map<std::string, PyObject*> m_globals = GetGlobals(lambda);

		PyObject* code = PyObject_GetAttrString(lambda, "__code__");
		disasm = HybridPython_CreateDisassembler(code);
		// lambda is actually a function, with a return statement
		stmt_ty stmt = HybridPython_DisassemblerProcess(disasm);
		/*
		// get source code
		std::string sourcecode = GetSourceCode(lambda);

		// create py arena and process source code
		arena = PyArena_New();
		
		expr_ty expr = GetLambdaStatementFromSource(arena, sourcecode);
		if (expr->kind != _expr_kind::Lambda_kind)
		{
			throw HybridPythonResult::NOT_IMPLEMENTED;
		}
		*/

		char name[64];
		::snprintf(name, 64, "lambda_%p", lambda);

		CUDALambdaFunction* func = new CUDALambdaFunction();
		func->setname(name);
		func->lambda = lambda; // this is not a function, it is a lambda...
		func->returntype = HybType::getthing();

		kw = new CUDAKernelWriter(m_globals);
		kw->EnterFunction(func);

		func->args = kw->GetArgs(func->hurd, stmt->v.FunctionDef.args);

		// build a fake return statement
		// https://stackoverflow.com/questions/862412/is-it-possible-to-have-multiple-statements-in-a-python-lambda-expression
		if (stmt->v.FunctionDef.body->size != 1)
			throw HybridPythonResult::NOT_IMPLEMENTED;
		func->statements = kw->ProcessStatement((stmt_ty)(stmt->v.FunctionDef.body->elements[0]));

		kw->LeaveFunction();

		// process all the device functions -- NOTE: size of _functionsToProcess may change within the loop
		for (int k = 0; k < kw->_functionsToProcess.size(); ++k)
		{
			CUDADeviceFunction* func = kw->_functionsToProcess[k];
			kw->EnterFunction(func);
			
			PyObject* code = PyObject_GetAttrString(func->function, "__code__");
			disasm = HybridPython_CreateDisassembler(code);
			stmt = HybridPython_DisassemblerProcess(disasm);
			
			if (stmt->kind != _stmt_kind::FunctionDef_kind)
			{
				ERROR_STACK.push("Invalid argument : not a function definition in " __FUNCTION__);
				throw HybridPythonResult::INVALID_ARGUMENT;
			}
			func->args = kw->GetArgs(func->hurd, stmt->v.FunctionDef.args);
			func->statements = kw->ProcessStatements(stmt->v.FunctionDef.body);
			kw->LeaveFunction();
		}

		result = kw->GetLambdaCUDA(func);

		delete kw; kw = nullptr;
		HybridPython_DestroyDisassembler(disasm);
	}
	catch (HybridPythonResult er)
	{
		if (kw != nullptr) delete kw;
		if (disasm != nullptr) HybridPython_DestroyDisassembler(disasm);

		throw er;
	}

	return result;
}

#define USE_INSPECT  false

PyObject* CUDAKernelWriter::BuildModule(PyObject* function)
{
	PyArena* arena = nullptr ;
	PyObject* result = nullptr;
	CUDAKernelWriter* kw = nullptr;
	stmt_ty stmt = nullptr;
	Disassembler* disasm = nullptr;

	try
	{
		std::map<std::string, PyObject*> m_globals = GetGlobals(function);

		if (!USE_INSPECT)
		{
			PyObject* code = PyObject_GetAttrString(function, "__code__");
			disasm = HybridPython_CreateDisassembler(code);
			stmt = HybridPython_DisassemblerProcess(disasm);
		}
		else 
		{
			// get source code
			std::string sourcecode = GetSourceCode(function);

			// create py arena and process source code
			arena = PyArena_New();

			stmt = GetFunctionStatementFromSource(arena, sourcecode);
		}
		kw = new CUDAKernelWriter(m_globals);

		kw->EnterFunction(&(kw->_globalFunction));
		kw->ProcessGlobalDefinition(stmt);
		kw->LeaveFunction();

		// process all the device functions -- NOTE: size of _functionsToProcess may change within the loop
		for (int k = 0; k < kw->_functionsToProcess.size(); ++k)
		{
			CUDADeviceFunction* func = kw->_functionsToProcess[k];
			kw->EnterFunction(func);
			if (!USE_INSPECT)
			{
				PyObject* code = PyObject_GetAttrString(func->function, "__code__");
				disasm = HybridPython_CreateDisassembler(code);
				stmt = HybridPython_DisassemblerProcess(disasm);
				// if no RETURN_VALUE opcode, return void, otherwise, return thing
				func->returntype = HybridPython_DisassemblerGetReturnType(disasm);
			}
			else 
			{
				// get source code
				std::string sourcecode = GetSourceCode(func->function);
				// get statement
				stmt = GetFunctionStatementFromSource(arena, sourcecode);
			}
			if (stmt->kind != _stmt_kind::FunctionDef_kind)
			{
				ERROR_STACK.push("Invalid argument : not a function definition in " __FUNCTION__);
				throw HybridPythonResult::INVALID_ARGUMENT;
			}
			func->args = kw->GetArgs(func->hurd, stmt->v.FunctionDef.args);
			func->statements = kw->ProcessStatements(stmt->v.FunctionDef.body);
			kw->LeaveFunction();
		}

		result = kw->GetModule();

		delete kw; kw = nullptr;
		
		if (arena != nullptr)
			PyArena_Free(arena); 
		arena = nullptr;

		if (disasm != nullptr)
			HybridPython_DestroyDisassembler(disasm);
		disasm = nullptr;
	}
	catch (HybridPythonResult er)
	{
		if (kw != nullptr) delete kw;
		if (arena != nullptr) PyArena_Free(arena);
		if (disasm != nullptr) HybridPython_DestroyDisassembler(disasm);

		throw er;
	}

	return result;
}

struct stringbuffer
{
	size_t offset;
	size_t capacity;
	char* data;

	stringbuffer(size_t capa = 2048)
	{
		data = new char[capa];
		offset = 0;
		capacity = capa;
		data[0] = 0; // for empty buffers.
	}

	~stringbuffer()
	{
		delete[] data;
	}

	template <typename... args>
	void printf(const char* format, args... parameters)
	{
		offset += ::snprintf(data + offset, capacity - offset, format, parameters...);
	}
};

/*

struct __device_f
{
	typedef float return_t;

	static float call();
};

float __device_f::call()
{
	return 42.0;
}

// Type your code here, or load an example.
__device_f::return_t square(int num) {
	return __device_f::call();
}


	*/

static void DeclareFunctionPrototype(CUDAFunctorDef* func, stringbuffer& buffer)
{
	if (func->returntype == nullptr)
		throw HybridPythonResult::UNKNOWN_RETURN_TYPE;
	buffer.printf("struct %s \n{\n\tusing return_t = %s ; \n", func->holdertypename.c_str(), func->returntype->_name.c_str());
	PyObject* capture = func->getcapture();
	buffer.printf("\tstatic __device__  %s  call (", func->returntype->_name.c_str());
	if (capture != nullptr)
		buffer.printf("const hybpython::thing& __closure__ ");
	for (int k = 0; k < func->args.size(); ++k)
	{
		if ((capture != nullptr) || (k != 0)) buffer.printf(" , ");
		buffer.printf("%s %s", func->args.args[k].second->_name.c_str(), func->args.args[k].first.c_str());
	}
	buffer.printf(") ;\n");

	buffer.printf("} ;\n");
}

PyObject* CUDAKernelWriter::GetLambdaCUDA(CUDALambdaFunction* lambda)
{
	PyObject* result = PyDict_New();
	PyDict_SetItemString(result, "version", PyLong_FromLong(42));

	stringbuffer buffer(1024 * 64); // 64 kB for lambda...

	// get all other functions used by lambda
	buffer.printf("// declare all functions -- prototypes \n");

	for (auto iter = _kernelFunctions.begin(); iter != _kernelFunctions.end(); ++iter)
	{
		DeclareFunctionPrototype(iter->second, buffer);
	}
	buffer.printf("\n");

	buffer.printf("// declare all functions -- implementations \n");
	for (auto iter = _kernelFunctions.begin(); iter != _kernelFunctions.end(); ++iter)
	{
		CUDADeviceFunction* func = (*iter).second;
		buffer.printf("__device__  %s  %s (", func->returntype->_name.c_str(), func->getname().c_str());
		for (int k = 0; k < func->args.size(); ++k)
		{
			if (k != 0) buffer.printf(" , ");
			buffer.printf("%s %s", func->args.args[k].second->_name.c_str(), func->args.args[k].first.c_str());
		}
		buffer.printf(") \n");
		buffer.printf("{\n");
		for (int k = 0; k < func->statements.size(); ++k)
		{
			buffer.printf("\t%s\n", func->statements[k].c_str());
		}
		buffer.printf("}\n");
		buffer.printf("\n");
	}
	buffer.printf("\n");

	// a guard, just in case...
	char guard[256];
	::snprintf(guard, 256, "__HYBPYTHON_LAMBDA_GUARD_%p__", lambda);

	buffer.printf("#ifndef %s\n", guard);
	buffer.printf("#define %s\n", guard);

	// declare lambda prototype -- TODO add capture as well (if applicable)...
	DeclareFunctionPrototype(lambda, buffer);

	// declare lambda body
	buffer.printf("__device__  %s  %s (", lambda->returntype->_name.c_str(), lambda->getname().c_str());

	PyObject* capture = lambda->getcapture();
	if (capture != nullptr)
		buffer.printf("const hybpython::thing& __closure__ , ");

	for (int k = 0; k < lambda->args.size(); ++k)
	{
		if (k != 0) buffer.printf(" , ");
		buffer.printf("%s %s", lambda->args.args[k].second->_name.c_str(), lambda->args.args[k].first.c_str());
	}
	buffer.printf(") \n");
	buffer.printf("{\n");
	for (int k = 0; k < lambda->statements.size(); ++k)
	{
		buffer.printf("\t%s\n", lambda->statements[k].c_str());
	}
	buffer.printf("}\n");
	buffer.printf("\n");

	stringbuffer funcinputtypes(1024 * 4); // , is prepend
	stringbuffer funcinputthings(1024 * 4); // , is prepend

	if (capture != nullptr)
	{
		funcinputtypes.printf(" , hybpython::thing const &");
		funcinputthings.printf(" , hybpython::thing const &");
	}
	for (int k = 0; k < lambda->args.size(); ++k)
	{
		funcinputtypes.printf(" , "); funcinputthings.printf(" , ");
		funcinputtypes.printf("%s", lambda->args.args[k].second->_name.c_str());
		funcinputthings.printf("hybpython::thing");
	}

	char pointername[64];
	::snprintf(pointername, 64, "g_lambdaptr__%s", lambda->holdertypename.c_str());

	buffer.printf("__device__ void* %s = (void*) hybpython::thingified_func<%s,%s%s>::%s<%s%s> ;\n",
		pointername,
		lambda->returntype == HybType::builtin<void>() ? "false" : "true",
		lambda->returntype->_name.c_str(),
		funcinputtypes.data,
		// which function
		capture == nullptr ? "call" : "capturecall",
		lambda->getname().c_str(),
		funcinputthings.data);
	buffer.printf("\n");

	buffer.printf("#endif // %s\n", guard);

	PyDict_SetItemString(result, "cuda", PyUnicode_FromString(buffer.data));

	PyDict_SetItemString(result, "lambdapointername", PyUnicode_FromString(pointername));

	// add global function argument types
	PyObject* argtypelist = PyList_New(lambda->args.size());
	for (int k = 0; k < lambda->args.size(); ++k)
	{
		PyList_SetItem(argtypelist, k,
			PyUnicode_FromString(lambda->args.args[k].second->_name.c_str()));
	}

	PyDict_SetItemString(result, "argtypes", argtypelist);

	// TODO: capture here !
	
	// attach dict to object
	PyObject_SetAttrString(lambda->lambda, HYBRID_CUDA_ATTR_NAME, result);

	return result;
}

PyObject* CUDAKernelWriter::GetModule()
{
	PyObject* result = PyDict_New();
	PyDict_SetItemString(result, "version", PyLong_FromLong(42));
	// Generate CUDA -- 64 MB should suffice for code...
	stringbuffer buffer (1024 * 1024 * 64);

	buffer.printf("// GENERATED BY HYBRIDIZER/PYTHON \n");
	buffer.printf("\n");
	buffer.printf("#include <hybpython.cuh>\n");
	buffer.printf("\n");

	buffer.printf("// declare all functions -- prototypes \n");

	for (auto iter = _kernelFunctions.begin() ; iter != _kernelFunctions.end(); ++iter)
	{
		DeclareFunctionPrototype(iter->second, buffer);
	}
	buffer.printf("\n");

	buffer.printf("// declare all functions -- implementations \n");
	for (auto iter = _kernelFunctions.begin(); iter != _kernelFunctions.end(); ++iter)
	{
		CUDADeviceFunction* func = (*iter).second;
		buffer.printf("__device__  %s  %s (", func->returntype->_name.c_str(), func->getname().c_str());
		for (int k = 0; k < func->args.size(); ++k)
		{
			if (k != 0) buffer.printf(" , ");
			buffer.printf("%s %s", func->args.args[k].second->_name.c_str(), func->args.args[k].first.c_str());
		}
		buffer.printf(") \n");
		buffer.printf("{\n");
		for (int k = 0; k < func->statements.size(); ++k)
		{
			buffer.printf("\t%s\n", func->statements[k].c_str());
		}
		buffer.printf("}\n");
		buffer.printf("\n");
	}
	buffer.printf("\n");

	buffer.printf("// declare global function \n");
	buffer.printf("extern \"C\" \n");
	buffer.printf("__global__ void %s (", _globalFunction.getname().c_str());
	for (int k = 0; k < _globalFunction.args.size(); ++k)
	{
		if (k != 0) buffer.printf(" , ");
		buffer.printf("%s %s", _globalFunction.args.args[k].second->_name.c_str(), _globalFunction.args.args[k].first.c_str());
	}
	buffer.printf(") \n");
	buffer.printf("{\n");
	for (int k = 0; k < _globalFunction.statements.size(); ++k)
	{
		buffer.printf("\t%s\n", _globalFunction.statements[k].c_str());
	}
	buffer.printf("}\n");
	buffer.printf("\n");

	PyDict_SetItemString(result, "cuda", PyUnicode_FromString(buffer.data));

	PyDict_SetItemString(result, "kernelname", PyUnicode_FromString(_globalFunction.getname().c_str()));

	// add global function argument types
	PyObject* argtypelist = PyList_New(_globalFunction.args.size());
	for (int k = 0; k < _globalFunction.args.size(); ++k)
	{
		PyList_SetItem(argtypelist, k, 
			PyUnicode_FromString(_globalFunction.args.args[k].second->_name.c_str()));
	}

	PyDict_SetItemString(result, "argtypes", argtypelist);

	return result;
}

#pragma region Hybridizer Callbacks

#include "HybridPythonResult.h"

static void sg_NestedFunctionDeclPrinterCallback(void* handle, const char* line)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	ThrowNotImplemented();
}

static void sg_NestedFunctionBodyPrinterCallback(void* handle, const char* line)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	ThrowNotImplemented();
}

static void sg_KernelPrinterCallback(void* handle, const char* line)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	ckw->AppendStatement(line);
}

static const char* sg_TypeNameCallback(void* handle, const char* symbol)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	HybType* symbolType = ckw->GetSymbolType(symbol);
	if (symbolType == nullptr)
		return nullptr;
	return symbolType->_name.c_str();
}

static const char* sg_SymbolMapCallback(void* handle, const char* symbol)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	return ckw->GetSymbolMap(symbol);
}

static const char* sg_FunctionReturnTypeCallback(void* handle, const char* symbol, int argcount, const char** argtypes)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	return ckw->GetFunctionReturnType(symbol, argcount, argtypes);
}

static const char* sg_AttributeTypeCallback(void* handle, const char* valuetype, const char* attribute)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	return HybType::getthing()->_name.c_str();
	//ThrowNotImplemented();
}

static const char* sg_AttributeSetterTypeCallback(void* handle, const char* valuetype, const char* attribute, const char* rht_type)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	ThrowNotImplemented();
}

static const char* sg_InferTypeCallback(void* handle, InferrenceKind infer, const char* left, const char* right)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	return ckw->InferType(infer, left, right);
}

static void sg_DefineFunctionReturnCallback(void* handle, const char* symbol, int argcount, const char** argtypes, const char* returntype)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	ThrowNotImplemented();
}

static void sg_ReturnStatementCallback(void* handle, const char* returntype)
{
	CUDAKernelWriter* ckw = (CUDAKernelWriter*)handle;
	ckw->DefineFunctionReturn(returntype);
}

#pragma endregion

#pragma region Constructor and Destructor

// extern "C" __declspec(dllexport) Hybridizer* _cdecl HybridPython_CreateHybridizer(
// 	void* handle, LinePrinterCallback cb, TypeNameCallback tn, SymbolMapCallback sm, FunctionReturnTypeCallback frt, AttributeTypeCallback at,
// 	InferTypeCallback it, AttributeSetterTypeCallback ast);

CUDAKernelWriter::CUDAKernelWriter(std::map<std::string, PyObject*> globals)
{
	_globals = globals;
	_hybridizer = HybridPython_CreateHybridizer(this, 
		sg_NestedFunctionDeclPrinterCallback,
		sg_NestedFunctionBodyPrinterCallback,
		sg_KernelPrinterCallback,
		sg_TypeNameCallback,
		sg_SymbolMapCallback,
		sg_FunctionReturnTypeCallback,
		sg_AttributeTypeCallback,
		sg_InferTypeCallback,
		sg_AttributeSetterTypeCallback,
		sg_DefineFunctionReturnCallback,
		sg_ReturnStatementCallback);
}

CUDAKernelWriter::~CUDAKernelWriter()
{
	HybridPython_DestroyHybridizer(_hybridizer);
	_hybridizer = nullptr;
	for (auto iter = _symbols.begin(); iter != _symbols.end(); ++iter)
	{
		delete (*iter).second;
	}
}

#pragma endregion

CUDAFunctionArgs CUDAKernelWriter::GetArgs(TypeHurd& hurd, arguments_ty args)
{
	if (args->vararg != nullptr)
	{
		ERROR_STACK.push("Not Implemented Exception : variable arguments not supported for device-code in " __FUNCTION__);
		throw 1;
	}
	if (args->kwarg != nullptr)
	{
		ERROR_STACK.push("Not Implemented Exception : KW arguments not supported for device-code in " __FUNCTION__);
		throw 1;
	}

	if (args->defaults != nullptr)
	{
		ERROR_STACK.push("Not Implemented Exception : default arguments not supported for device-code in " __FUNCTION__);
		throw 1;
	}

	CUDAFunctionArgs res;
	if (args->args != nullptr)
	{
		for (int k = 0; k < args->args->size; ++k)
		{
			arg_ty arg = (arg_ty)(args->args->elements[k]);

#ifdef PYTHON_27
			std::string name = TO_STRING(arg->Name);
#else
			std::string name = TO_STRING(arg->arg);
#endif

			HybType* argtype = nullptr;
#ifndef PYTHON_27
			if (arg->annotation != nullptr)
			{
				if (arg->annotation->kind != _expr_kind::Name_kind)
				{
					ERROR_STACK.push("Application Exception : argument annotation is not a name in " __FUNCTION__);
					throw 1;
				}
				std::string argtypename = TO_STRING(arg->annotation->v.Name.id);
				argtype = hurd.GetType(argtypename);
				if (argtype == nullptr)
				{
					ERROR_STACK.push("Application Exception : argument annotation is not a known builtin in " __FUNCTION__);
					throw 1;
				}
			}
			else {
				//argtype = hurd.BuildTypeName("__arg_" + name + "_t");
				argtype = HybType::getthing();
			}
#else
			argtype = HybType::getthing();
#endif

			res.args.push_back(std::pair<std::string, HybType*>(name, argtype));
		}
	}

	return res;
}

void CUDAKernelWriter::ProcessGlobalDefinition(stmt_ty functiondef)
{
	if (functiondef->kind != _stmt_kind::FunctionDef_kind)
	{
		ERROR_STACK.push("Invalid argument : not a function definition in " __FUNCTION__);
		throw 1;
	}

	if (0 != _globalFunction.args.size())
	{
		ERROR_STACK.push("Invalid State : _globalFunction not new in " __FUNCTION__);
		throw 1;
	}
	if (0 != _globalFunction.statements.size())
	{
		ERROR_STACK.push("Invalid State : _globalFunction not new in " __FUNCTION__);
		throw 1;
	}

	_globalFunction.setname(TO_STRING(functiondef->v.FunctionDef.name));
	_globalFunction.args = GetArgs(_globalFunction.hurd, functiondef->v.FunctionDef.args);

	// IF KNOWN SHOULD BE VOID !! ... functiondef->v.FunctionDef.returns;
	HybridPython_EnterFunctionScope(_hybridizer);
	
	// define args
	for (int k = 0; k < _globalFunction.args.size(); ++k)
	{
		auto arg = _globalFunction.args.args[k];
		HybridPython_DefineSymbol(_hybridizer, arg.first, arg.second);
	}

	_globalFunction.statements = ProcessStatements(functiondef->v.FunctionDef.body);

	HybridPython_LeaveFunctionScope(_hybridizer);
}

// TODO : avoid multiple copies of strings with std::move
std::vector<std::string> CUDAKernelWriter::ProcessStatements(asdl_seq * stmts)
{
	std::vector<std::string> result; // may be empty !!
	if (stmts != nullptr)
	{
		for (int k = 0; k < stmts->size; ++k)
		{
			std::vector<std::string> statement = ProcessStatement((stmt_ty)(stmts->elements[k]));
			result.reserve(result.size() + statement.size());
			for (int j = 0; j < statement.size(); ++j)
			{
				result.push_back(statement[j]);
			}
		}
	}
	return result;
}

extern "C" __declspec(dllexport) HybridPythonResult _cdecl HybridPython_ExpressStatement(
	Hybridizer* hyb, stmt_ty statement);

std::vector<std::string> CUDAKernelWriter::ProcessStatement(stmt_ty stmt)
{
	_statements.clear();
	HybridPythonResult hpr = HybridPython_ExpressStatement(_hybridizer, stmt);
	if (hpr != HybridPythonResult::SUCCESS)
		throw hpr;

	return _statements;
}

#pragma region Symbol management

// https://docs.python.org/3.6/library/functions.html
HybType* CUDAKernelWriter::GetSymbolType(std::string symbol)
{
	auto found = _symbols.find(symbol);
	if (found != _symbols.end()) return (*found).second;

	// test symbols from arguments of function
	for (int k = 0; k < _currentFunction->args.size(); ++k)
	{
		if (symbol == _currentFunction->args.args[k].first)
			return _currentFunction->args.args[k].second;
	}

	// test from globals
	for (auto iter = _globals.begin(); iter != _globals.end(); ++iter)
	{
		if (iter->first == symbol)
		{
			return _currentFunction->hurd.GetType(iter->second);
		}
	}

	if (symbol == "range") return _currentFunction->hurd.GetType("void");
	
	return nullptr;
}

#pragma endregion

void CUDAKernelWriter::AppendStatement(const char* line)
{
	// TODO: fancy indenting ??
	_statements.push_back(line);
}

const char* CUDAKernelWriter::GetSymbolMap(const char* symbol)
{
	// is it a function ? -- defined in globals...
	for (auto iter = _globals.begin(); iter != _globals.end(); ++iter)
	{
		if (iter->first == symbol)
		{
			if (PyFunction_Check(iter->second))
			{
				auto found = _kernelFunctions.find(symbol);
				if (found == _kernelFunctions.end())
				{
					CUDADeviceFunction* cdf = new CUDADeviceFunction();
					cdf->function = iter->second;
					cdf->setname("__devicefunction_" + std::string(symbol));
					_kernelFunctions[symbol] = cdf;

					_functionsToProcess.push_back(cdf);

					return cdf->getname().c_str();
				}
				else
					return found->second->getname().c_str();
			}
			
			// TODO : global functions
		}
	}

	// don't map
	return symbol;
}

static std::string UNICODE_TO_STRING(PyObject* o)
{
	char buffer[1024];
	switch (PyUnicode_KIND(o))
	{
	case PyUnicode_Kind::PyUnicode_1BYTE_KIND:
	{
		::memcpy(buffer, (const char*)PyUnicode_1BYTE_DATA(o), (size_t)PyUnicode_GET_LENGTH(o));
		buffer[(size_t)PyUnicode_GET_LENGTH(o)] = 0;
		std::string symbol(buffer);
		return symbol;
	}
	default:
		ThrowNotImplemented();
	}
}

const char* CUDAKernelWriter::GetFunctionReturnType(const char* symbol, int argcount, const char** argtypes)
{
	#pragma region Python language builtins
	// https://docs.python.org/3.6/library/functions.html
	// TODO: accelerate !
	if (::strcmp(symbol, "abs") == 0)
	{
		if (argcount != 1)
			throw HybridPythonResult::INVALID_ARGUMENT;
		// set expected argument types
		return argtypes[0];
	}
	else if (::strcmp(symbol, "ascii") == 0)
	{
		// TODO: implement strings first
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "bin") == 0)
	{
		// TODO: implement strings first
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "bool") == 0)
	{
		return HybType::builtin<bool>()->_name.c_str();
	}
	else if (::strcmp(symbol, "bytearray") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "bytes") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "callable") == 0)
	{
		// TODO: implement strings first
		return HybType::builtin<bool>()->_name.c_str();
	}
	else if (::strcmp(symbol, "compile") == 0)
	{
		// wel.... not sure support is on tracks for this...
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "complex") == 0)
	{
		if (argcount != 2)
			throw HybridPythonResult::INVALID_ARGUMENT;
		argtypes[0] = HybType::builtin<double>()->_name.c_str();
		argtypes[1] = HybType::builtin<double>()->_name.c_str();
		return HybType::builtin<complex>()->_name.c_str();
	}
	else if (::strcmp(symbol, "delattr") == 0)
	{
		return HybType::builtin<void>()->_name.c_str();
	}
	else if (::strcmp(symbol, "dict") == 0)
	{
		// TODO: support dictionary
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "dir") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "divmod") == 0)
	{
		// TODO: return pairs
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "enumerate") == 0)
	{
		// return enumerables
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "eval") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "exec") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "filter") == 0)
	{
		// need iterator support
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "float") == 0)
	{
		if (argcount != 1)
			throw HybridPythonResult::INVALID_ARGUMENT;
		// set expected argument types
		return HybType::builtin<double>()->_name.c_str();
	}
	else if (::strcmp(symbol, "format") == 0)
	{
		// TODO : string support
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "frozenset") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "getattr") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "globals") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "hasattr") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "hash") == 0)
	{
		return HybType::builtin<int64_t>()->_name.c_str();
	}
	else if (::strcmp(symbol, "help") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "hex") == 0)
	{
		// TODO : string support
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "id") == 0)
	{
		return HybType::builtin<int64_t>()->_name.c_str();
	}
	else if (::strcmp(symbol, "input") == 0)
	{
		// will ever ?
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "int") == 0)
	{
		return HybType::builtin<int64_t>()->_name.c_str();
	}
	else if (::strcmp(symbol, "isinstance") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "issubclass") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "iter") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "len") == 0)
	{
		return HybType::builtin<int64_t>()->_name.c_str();
	}
	else if (::strcmp(symbol, "list") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "locals") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "map") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "max") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "memoryview") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "min") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "next") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "object") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "oct") == 0)
	{
		// TODO : string support
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "open") == 0)
	{
		// will ever ?
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "ord") == 0)
	{
		// TODO : string support
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "pow") == 0)
	{
		// well, 10**2 is int and 10**-1 is float => seriously !!!
		if (argcount == 3)
			return HybType::builtin<int64_t>()->_name.c_str();
		if (argcount == 2)
		{
			if ((::strcmp (argtypes[0], HybType::builtin<complex>()->_name.c_str()) == 0) ||
				(::strcmp (argtypes[1], HybType::builtin<complex>()->_name.c_str()) == 0))
				return HybType::builtin<complex>()->_name.c_str();
			else
				return HybType::builtin<double>()->_name.c_str();
		}
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "print") == 0)
	{
		// TODO : string support
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "open") == 0)
	{
		// ??
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "property") == 0)
	{
		// ??
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "range") == 0)
	{
		// ??
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "repr") == 0)
	{
		// ??
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "reversed") == 0)
	{
		// ??
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "round") == 0)
	{
		if (argcount == 1)
			return HybType::builtin<int64_t>()->_name.c_str();
		return argtypes[0];
	}
	else if (::strcmp(symbol, "set") == 0)
	{
		// ??
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "setattr") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "slice") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "sorted") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "str") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "sum") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "super") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "tuple") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "type") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "vars") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	else if (::strcmp(symbol, "zip") == 0)
	{
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	#pragma endregion


	std::string functionname;
	std::string functionholdername;

	auto found = _kernelFunctions.find(symbol);
	if (found != _kernelFunctions.end())
	{
		CUDADeviceFunction* foundfunc = found->second;
		for (int k = 0; k < argcount; ++k)
		{
			argtypes[k] = foundfunc->args.args[k].second->_name.c_str();
		}
		functionname = foundfunc->getname();
		functionholdername = foundfunc->holdertypename;
	}
	else
	{
		if (_globals.find(symbol) == _globals.end())
			throw HybridPythonResult::UNKNOWN_SYMBOL_TYPE;

		PyObject* func = _globals[symbol];
		if (PyObject_HasAttrString(func, HYBRID_CUDA_INTRINSIC_FUNCTION_ATTR_NAME) ||
			PyObject_HasAttrString(func, HYBRID_CUDA_INTRINSIC_CONSTANT_ATTR_NAME))
		{
			// intrinsic functions need a return type
			PyObject* funcObject = PyObject_GetAttrString(func, HYBRID_CUDA_FUNC_ATTR_NAME);
			PyObject* funcAnnotations = PyObject_GetAttrString(funcObject, "__annotations__");
			PyObject* returnType = PyDict_GetItemString(funcAnnotations, "return");
			if (returnType == Py_None)
				return "void";
			if (returnType == nullptr)
				throw HybridPythonResult::INTRINSIC_FUNCTION_UNKNOWN_RETURN_TYPE;
			PyObject* typequalname = PyObject_GetAttrString(returnType, "__qualname__");
			std::string strname = UNICODE_TO_STRING(typequalname).c_str();
			return HybType::forge(strname)->_name.c_str(); // forge the type to store its name in a string...
		}

		// if not found, insert it !
		CUDADeviceFunction* cdf = new CUDADeviceFunction();
		cdf->function = _globals[symbol],
		cdf->setname(functionname = "__devicefunction_" + std::string(symbol));
		functionholdername = cdf->holdertypename;
		_kernelFunctions[symbol] = cdf;

		_functionsToProcess.push_back(cdf);

		// all expected args are thing, for now... ; though should have a map from symbol...
		for (int k = 0; k < argcount; ++k)
		{
			argtypes[k] = HybType::getthing()->_name.c_str();
		}
	}
	// in this implementation, all functions return python <thing>.
	// TODO: unless specified otherwise in annotation => or builtin functions
	// return type is defined at function prototype declaration

	std::string forgedreturntype = functionholdername + std::string("::return_t");
	return HybType::forge(forgedreturntype)->_name.c_str(); // forge the type to store its name in a string...
}

const char* CUDAKernelWriter::InferType(InferrenceKind infer, const char* left, const char* right)
{
	// TODO: unless specified otherwise in annotation
	return HybType::getthing()->_name.c_str();
}

void CUDAKernelWriter::DefineFunctionReturn(const char* returntype)
{
	if (_currentFunction == &_globalFunction)
		throw HybridPythonResult::GLOBAL_FUNCTION_SHOULD_RETURN_VOID;

	((CUDADeviceFunction*)_currentFunction)->returntype = HybType::forge(returntype);
}

#pragma region JSON serialization

#include "../json.hpp"

nlohmann::json TO_JSON(stmt_ty statement);
nlohmann::json TO_JSON(expr_ty expression);
nlohmann::json TO_JSON(slice_ty slice);
nlohmann::json TO_JSON(cmpop_ty op);
nlohmann::json TO_JSON(operator_ty op);
nlohmann::json TO_JSON(boolop_ty op);
nlohmann::json TO_JSON(unaryop_ty op);
nlohmann::json TO_JSON(expr_context_ty op);
nlohmann::json TO_JSON(arguments_ty op);
#ifndef PYTHON_27
nlohmann::json TO_JSON(arg_ty op);
#endif

nlohmann::json TO_JSON(PyObject* obj);

template<typename T>
nlohmann::json TO_JSON(asdl_seq* seq)
{
	if (seq == nullptr) return "<nullptr>";

	nlohmann::json j;
	for (int k = 0; k < seq->size; ++k)
	{
		j.push_back(TO_JSON((T)(seq->elements[k])));
	}
	return j;
}

template<typename T>
nlohmann::json TO_JSON(asdl_int_seq* seq)
{
	if (seq == nullptr) return "<nullptr>";

	nlohmann::json j;
	for (int k = 0; k < seq->size; ++k)
	{
		j.push_back(TO_JSON((T)(seq->elements[k])));
	}
	return j;
}

nlohmann::json TO_JSON(PyObject* obj)
{
	if (obj == nullptr) return "<nullptr>";

	nlohmann::json j;
	if (PyLong_Check(obj))
		j["long"] = PyLong_AsLongLong(obj);
	else if (PyFloat_Check(obj))
		j["float"] = PyLong_AsLongLong(obj);
	else if (PyUnicode_Check(obj))
		j["str"] = CUDAKernelWriter::TO_STRING(obj);
	else if (obj == Py_None)
		j["none"] = "<none>";
	else
		ThrowNotImplemented();
	return j;
}
#ifndef PYTHON_27
nlohmann::json TO_JSON(arg_ty op)
{
	if (op == nullptr) return "<nullptr>";

	nlohmann::json j;

	j["annotation"] = TO_JSON(op->annotation);
	j["arg"] = CUDAKernelWriter::TO_STRING(op->arg);
	j["lineno"] = op->lineno;
	j["col_offset"] = op->col_offset;

	return j;
}
#endif

nlohmann::json TO_JSON(expr_context_ty ctx)
{
	switch (ctx)
	{
	case expr_context_ty::Load: return "Load";
	case expr_context_ty::Store: return "Store";
	case expr_context_ty::Del: return "Del";
	case expr_context_ty::AugLoad: return "AugLoad";
	case expr_context_ty::AugStore: return "AugStore";
	case expr_context_ty::Param: return "Param";
	default:
		ThrowNotImplemented();
	}
}

nlohmann::json TO_JSON(arguments_ty op)
{
	if (op == nullptr) return "<nullptr>";

	nlohmann::json j;

#ifndef PYTHON_27
	j["args"] = TO_JSON<arg_ty>(op->args);
#endif
	j["defaults"] = TO_JSON<expr_ty>(op->defaults);
	j["kwargs"] = TO_JSON(op->kwarg);
	j["vararg"] = TO_JSON(op->vararg);

	return j;
}

nlohmann::json TO_JSON(cmpop_ty op)
{
	switch (op)
	{
	case cmpop_ty::Eq: return "==";
	case cmpop_ty::Gt: return ">";
	case cmpop_ty::GtE: return ">=";
	case cmpop_ty::In: return "in";
	case cmpop_ty::Is: return "is";
	case cmpop_ty::IsNot: return "is not";
	case cmpop_ty::Lt: return "<";
	case cmpop_ty::LtE: return "<=";
	case cmpop_ty::NotEq: return "!=";
	case cmpop_ty::NotIn: return "not in";
	default: ThrowNotImplemented();
	}
}

nlohmann::json TO_JSON(boolop_ty op)
{
	switch (op)
	{
	case boolop_ty::And: return "&&";
	case boolop_ty::Or: return "||";
	default: ThrowNotImplemented();
	}
}

nlohmann::json TO_JSON(unaryop_ty op)
{
	switch (op)
	{
	case unaryop_ty::Not: return "!";
	case unaryop_ty::Invert: return "~"; // ??
	case unaryop_ty::UAdd: return "+";
	case unaryop_ty::USub: return "-";
	default: ThrowNotImplemented();
	}
}

nlohmann::json TO_JSON(operator_ty op)
{
	switch (op)
	{
		case operator_ty::Add: return "+"; 
		case operator_ty::Sub: return "-";
		case operator_ty::Mult: return "*";
#ifndef PYTHON_27
		case operator_ty::MatMult: return "@";
#endif
		case operator_ty::Div: return "/";
		case operator_ty::Mod: return "%";
		case operator_ty::Pow: return "**";
		case operator_ty::LShift: return "<<";
		case operator_ty::RShift: return ">>";
		case operator_ty::BitOr: return "|";
		case operator_ty::BitXor: return "^";
		case operator_ty::BitAnd: return "&";
		case operator_ty::FloorDiv: return "//";
		default:
			ThrowNotImplemented();
	}
}

nlohmann::json TO_JSON(stmt_ty statement)
{
	if (statement == nullptr) return "<nullptr>";

	nlohmann::json j;
	j["kind"] = statement->kind;
	j["lineno"] = statement->lineno;
	switch (statement->kind)
	{
#ifndef PYTHON_27
	case _stmt_kind::AsyncFor_kind:
		ThrowNotImplemented();
	case _stmt_kind::AsyncFunctionDef_kind:
		ThrowNotImplemented();
	case _stmt_kind::AsyncWith_kind:
		ThrowNotImplemented();
	case _stmt_kind::AnnAssign_kind:
		j["AnnAssign"]["annotation"] = TO_JSON(statement->v.AnnAssign.annotation);
		j["AnnAssign"]["simple"] = statement->v.AnnAssign.simple;
		j["AnnAssign"]["target"] = TO_JSON(statement->v.AnnAssign.target);
		j["AnnAssign"]["value"] = TO_JSON(statement->v.AnnAssign.value);
		break;
	case _stmt_kind::Nonlocal_kind:
		ThrowNotImplemented();
	case _stmt_kind::Try_kind:
		ThrowNotImplemented();
#endif
	case _stmt_kind::Assert_kind:
		j["Assert"]["msg"] = TO_JSON(statement->v.Assert.msg);
		j["Assert"]["test"] = TO_JSON(statement->v.Assert.test);
		break;
	case _stmt_kind::Assign_kind:
		j["Assign"]["targets"] = TO_JSON<expr_ty>(statement->v.Assign.targets);
		j["Assign"]["value"] = TO_JSON(statement->v.Assign.value);
		break;
	case _stmt_kind::AugAssign_kind:
		j["AugAssign"]["targets"] = TO_JSON(statement->v.AugAssign.target);
		j["AugAssign"]["value"] = TO_JSON(statement->v.AugAssign.value);
		j["AugAssign"]["op"] = TO_JSON(statement->v.AugAssign.op);
		break;
	case _stmt_kind::Delete_kind:
		j["Delete"]["targets"] = TO_JSON<expr_ty>(statement->v.Delete.targets);
		break;
	case _stmt_kind::Expr_kind:
		j["Expr"]["value"] = TO_JSON(statement->v.Expr.value);
		break;
	case _stmt_kind::For_kind:
		j["For"]["body"] = TO_JSON<stmt_ty>(statement->v.For.body);
		j["For"]["iter"] = TO_JSON(statement->v.For.iter);
		j["For"]["orelse"] = TO_JSON<stmt_ty>(statement->v.For.orelse);
		j["For"]["target"] = TO_JSON(statement->v.For.target);
		break;
	case _stmt_kind::FunctionDef_kind:
		j["FunctionDef"]["args"] = TO_JSON(statement->v.FunctionDef.args);
		j["FunctionDef"]["body"] = TO_JSON<stmt_ty>(statement->v.FunctionDef.body);
		// TODO : statement->v.FunctionDef.decorator_list
		j["FunctionDef"]["name"] = CUDAKernelWriter::TO_STRING(statement->v.FunctionDef.name);
#ifndef PYTHON_27
		j["FunctionDef"]["returns"] = TO_JSON(statement->v.FunctionDef.returns);
#endif
		break;
	case _stmt_kind::If_kind:
		j["If"]["body"] = TO_JSON<stmt_ty>(statement->v.If.body);
		j["If"]["orelse"] = TO_JSON<stmt_ty>(statement->v.If.orelse);
		j["If"]["test"] = TO_JSON(statement->v.If.test);
		break;
	case _stmt_kind::Return_kind:
		j["Return"]["value"] = TO_JSON(statement->v.Return.value);
		break;
	case _stmt_kind::While_kind:
		j["While"]["body"] = TO_JSON<stmt_ty>(statement->v.While.body);
		j["While"]["orelse"] = TO_JSON<stmt_ty>(statement->v.While.orelse);
		j["While"]["test"] = TO_JSON(statement->v.While.test);
		break;
	case _stmt_kind::Continue_kind:
	case _stmt_kind::Pass_kind:
	case _stmt_kind::Break_kind:
		break;
	case _stmt_kind::Global_kind:
	case _stmt_kind::ImportFrom_kind:
	case _stmt_kind::Import_kind:
	case _stmt_kind::Raise_kind:
	case _stmt_kind::ClassDef_kind:
	case _stmt_kind::With_kind:
	default:
		ThrowNotImplemented();
	}
	return j;
}

nlohmann::json TO_JSON(slice_ty slice)
{
	if (slice == nullptr) return "<nullptr>";

	nlohmann::json j;
	j["kind"] = slice->kind;
	switch (slice->kind)
	{
	case _slice_kind::ExtSlice_kind:
		ThrowNotImplemented();
	case _slice_kind::Index_kind:
		j["Index"]["value"] = TO_JSON(slice->v.Index.value);
		break;
	case _slice_kind::Slice_kind:
		j["Slice"]["lower"] = TO_JSON(slice->v.Slice.lower);
		j["Slice"]["upper"] = TO_JSON(slice->v.Slice.upper);
		j["Slice"]["step"] = TO_JSON(slice->v.Slice.step);
		break;
	}
	return j;
}

nlohmann::json TO_JSON(expr_ty expression)
{
	if (expression == nullptr) return "<nullptr>";

	nlohmann::json j;
	j["kind"] = expression->kind;
	j["lineno"] = expression->lineno;
	j["col_offset"] = expression->col_offset;

	switch (expression->kind)
	{
		case _expr_kind::BoolOp_kind:
			j["BoolOp"]["op"] = TO_JSON(expression->v.BoolOp.op);
			j["BoolOp"]["values"] = TO_JSON<expr_ty>(expression->v.BoolOp.values);
			break;
		case _expr_kind::BinOp_kind:
			j["BinOp"]["op"] = TO_JSON(expression->v.BinOp.op);
			j["BinOp"]["left"] = TO_JSON(expression->v.BinOp.left);
			j["BinOp"]["right"] = TO_JSON(expression->v.BinOp.right);
			break;
		case _expr_kind::UnaryOp_kind:
			j["UnaryOp"]["op"] = TO_JSON(expression->v.UnaryOp.op);
			j["UnaryOp"]["operand"] = TO_JSON(expression->v.UnaryOp.operand);
			break;
			case _expr_kind::Lambda_kind:
				ThrowNotImplemented();
		case _expr_kind::IfExp_kind:
			j["IfExpr"]["body"] = TO_JSON(expression->v.IfExp.body);
			j["IfExpr"]["orelse"] = TO_JSON(expression->v.IfExp.orelse);
			j["IfExpr"]["test"] = TO_JSON(expression->v.IfExp.test);
			break;
		case _expr_kind::Compare_kind:
			j["Compare"]["left"] = TO_JSON(expression->v.Compare.left);
			j["Compare"]["comparators"] = TO_JSON<expr_ty>(expression->v.Compare.comparators);
			j["Compare"]["ops"] = TO_JSON<cmpop_ty>(expression->v.Compare.ops);
			break;
		case _expr_kind::Call_kind:
			j["Call"]["args"] = TO_JSON<expr_ty>(expression->v.Call.args);
			j["Call"]["func"] = TO_JSON(expression->v.Call.func);
			// TODO expression->v.Call.keywords);
			break;
		case _expr_kind::Num_kind:
			j["Num"]["n"] = TO_JSON(expression->v.Num.n);
			break;
		case _expr_kind::Str_kind:
			j["Str"]["s"] = TO_JSON(expression->v.Str.s);
			break;
		case _expr_kind::Attribute_kind:
			j["Attribute"]["attr"] = TO_JSON(expression->v.Attribute.attr);
			j["Attribute"]["ctx"] = TO_JSON(expression->v.Attribute.ctx);
			j["Attribute"]["value"] = TO_JSON(expression->v.Attribute.value);
			break;
		case _expr_kind::Subscript_kind:
			j["Subscript"]["ctx"] = TO_JSON(expression->v.Subscript.ctx);
			j["Subscript"]["slice"] = TO_JSON(expression->v.Subscript.slice);
			j["Subscript"]["value"] = TO_JSON(expression->v.Subscript.value);
			break;
		case _expr_kind::Name_kind:
			j["Name"]["ctx"] = TO_JSON(expression->v.Name.ctx);
			j["Name"]["id"] = TO_JSON(expression->v.Name.id);
			break;
#ifndef PYTHON_27
		case _expr_kind::Constant_kind:
			j["Constant"]["value"] = TO_JSON(expression->v.Constant.value);
			break;
#endif
		case _expr_kind::Dict_kind:
		case _expr_kind::Set_kind:
		case _expr_kind::ListComp_kind:
		case _expr_kind::SetComp_kind:
		case _expr_kind::DictComp_kind:
		case _expr_kind::GeneratorExp_kind:
		case _expr_kind::Yield_kind:
		case _expr_kind::List_kind:
		case _expr_kind::Tuple_kind:
#ifndef PYTHON_27
		case _expr_kind::Starred_kind:
		case _expr_kind::Await_kind:
		case _expr_kind::YieldFrom_kind:
		case _expr_kind::FormattedValue_kind:
		case _expr_kind::JoinedStr_kind:
		case _expr_kind::Bytes_kind:
		case _expr_kind::NameConstant_kind:
		case _expr_kind::Ellipsis_kind:
#endif
				ThrowNotImplemented();
	}
	return j;
}

std::string JSONSerialize(stmt_ty statement)
{
	nlohmann::json j = TO_JSON(statement);

	return j.dump();
}

#pragma endregion
