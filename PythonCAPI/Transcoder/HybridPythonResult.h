// (c) ALTIMESH 2019 -- all rights reserved

#pragma once
#include <exception>


// Python version : behavior is different between 2.x and 3.x, but there may be more
#ifndef PYTHON_VERSION_MAJOR
#ifdef PYTHON_27
#define PYTHON_VERSION_MAJOR 2
#else
#define PYTHON_VERSION_MAJOR 3
#endif
#endif

#ifndef PYTHON_VERSION_MINOR
#ifdef PYTHON_27
#define PYTHON_VERSION_MINOR 7
#else
#define PYTHON_VERSION_MINOR 6
#endif
#endif

enum HybridPythonResult
{
	SUCCESS = 0,
	NOT_IMPLEMENTED = 0x1,
	UNKNOWN_ATTRIBUTE_TYPE = 0x2,
	UNKNOWN_SYMBOL_TYPE = 0x3,
	GLOBAL_FUNCTION_SHOULD_RETURN_VOID = 0x4,
	UNKNOWN_NAME_CONSTANT = 0x5,
	UNSUPPORTED_COMPARE_OPERATOR = 0x6,
	INVALID_ARGUMENT = 0x7,
	MULTIPLE_RETURN_TYPE = 0x8,
	UNKNOWN_RETURN_TYPE = 0x9,
	MISSING_RETURN_TYPE_ANNOTATION = 0XA,
	NULL_POINTER_EXCEPTION = 0xB,

	MULTIPLE_SYMBOL_TYPES,
	CANNOT_FIND_LAMBDA_POINTER_NAME,
	INTERNAL_ERROR_PROCESSING_LAMBDA,
	CANNOT_GET_SOURCECODE,
	CANNOT_ACCESS_GLOBALS_DICT,
	CANNOT_FIND_TYPE, 
	CANNOT_FIND_TYPE_ANNOTATION,
	UNDEFINED_ATTRIBUTE_TYPE_IN_INTRINSIC,

	// intrinsics
	INTRINSIC_FUNCTION_UNKNOWN_RETURN_TYPE, // return type of intrinsic needs to be specified !

	// disassembler error
	DISASM_VARCOUNT_MISMATCH,
	DISASM_UNCLOSED_BLOCK,
	DISASM_INTERNAL_STACK_ERROR,

	MARSHALLING_ERROR = 0x100000,
};

class PythonException : public std::exception {
private:
	HybridPythonResult _code;
public:
	PythonException(HybridPythonResult result) : _code(result) {}

	const char* what() const throw() 
	{
		switch (_code) {
		case HybridPythonResult::NOT_IMPLEMENTED:
			return "not implemented";
		default: 
			return "unknown error";
		}
	}
};

[[noreturn]] static void ThrowNotImplemented() 
{
	throw new PythonException(HybridPythonResult::NOT_IMPLEMENTED);
}


#pragma region constant strings

#define HYBRID_CUDA_ATTR_NAME "__hybrid_cuda__"
#define HYBRID_CUDA_INTRINSIC_FUNCTION_ATTR_NAME "__hybrid_cuda__intrinsic_function__"
#define HYBRID_CUDA_INTRINSIC_CONSTANT_ATTR_NAME "__hybrid_cuda__intrinsic_constant__"
#define HYBRID_CUDA_INTRINSIC_TYPE_ATTR_NAME "__hybrid_cuda__intrinsic_type__"
#define HYBRID_CUDA_FUNC_ATTR_NAME "__hybrid_cuda__func__"

#pragma endregion