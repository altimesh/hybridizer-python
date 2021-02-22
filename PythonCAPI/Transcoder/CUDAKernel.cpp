// (c) Altimesh 2019 -- all rights reserved
#include "CUDAKernel.h"

#include "HybridPythonResult.h"

#pragma region thing implementation

HybType* TypeHurd::GetType(PyObject* po)
{
	return HybType::forge(po->ob_type->tp_name);
}

HybType* TypeHurd::GetType(std::string name)
{
	return HybType::forge(name);
}

#pragma endregion