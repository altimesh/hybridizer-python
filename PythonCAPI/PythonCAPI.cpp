// PythonCAPI.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "PythonCAPI.h"
#include "opcode.h"
#ifdef PYTHON_27
#include <inttypes.h>
#endif

#pragma region Unicode Objects

// https://docs.python.org/3/c-api/unicode.html#unicode-objects

#ifndef PYTHON_27

// int PyUnicode_Check(PyObject *o)
PYTHONCAPI_API int Hyb_PyUnicode_Check(PyObject *o)
{
	return PyUnicode_Check(o);
}

// Py_ssize_t PyUnicode_GET_LENGTH(PyObject *o)
PYTHONCAPI_API Py_ssize_t Hyb_PyUnicode_GET_LENGTH(PyObject *o)
{
	return PyUnicode_GET_LENGTH(o);
}
// int PyUnicode_KIND(PyObject *o)
PYTHONCAPI_API int Hyb_PyUnicode_KIND(PyObject *o)
{
	return PyUnicode_KIND(o);
}

// int PyUnicode_READY(PyObject *o)
PYTHONCAPI_API int Hyb_PyUnicode_READY(PyObject *o)
{
	return PyUnicode_READY(o);
}

// void* PyUnicode_DATA(PyObject *o)
PYTHONCAPI_API void* Hyb_PyUnicode_DATA(PyObject *o)
{
	return PyUnicode_DATA(o);
}
#endif

#pragma endregion 

#pragma region Long Objects

// https://docs.python.org/3/c-api/long.html

// int PyLong_Check(PyObject *p)
PYTHONCAPI_API int Hyb_PyLong_Check(PyObject *p)
{
	return PyLong_Check(p);
}

// long long PyLong_AsLongLong(PyObject *obj)
PYTHONCAPI_API int64_t Hyb_PyLong_AsLongLong(PyObject *obj)
{
	return PyLong_AsLongLong(obj);
}

// unsigned long long PyLong_AsUnsignedLongLong(PyObject *obj)
PYTHONCAPI_API uint64_t Hyb_PyLong_AsUnsignedLongLong(PyObject *obj)
{
	return PyLong_AsUnsignedLongLong(obj);
}


#pragma endregion 

#pragma region Float Objects

// https://docs.python.org/3/c-api/float.html

// int PyFloat_Check(PyObject *p)
PYTHONCAPI_API int Hyb_PyFloat_Check(PyObject *p)
{
	return PyFloat_Check(p);
}

// double PyFloat_AsDouble(PyObject *pyfloat)
PYTHONCAPI_API double Hyb_PyFloat_AsDouble(PyObject *p)
{
	return PyFloat_AsDouble(p);
}


#pragma endregion 

#pragma region Complex Objects

// https://docs.python.org/3/c-api/float.html

// int PyComplex_Check(PyObject *p)
PYTHONCAPI_API int Hyb_PyComplex_Check(PyObject *p)
{
	return PyComplex_Check(p);
}

// double PyComplex_RealAsDouble(PyObject *pyfloat)
PYTHONCAPI_API double Hyb_PyComplex_RealAsDouble(PyObject *p)
{
	return PyComplex_RealAsDouble(p);
}

// double PyComplex_ImagAsDouble(PyObject *pyfloat)
PYTHONCAPI_API double Hyb_PyComplex_ImagAsDouble(PyObject *p)
{
	return PyComplex_ImagAsDouble(p);
}


#pragma endregion 