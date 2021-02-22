#pragma once

// Trick to enable compilation in debug with Python referenced in release
// Enable debug just my module and build within VS...

#pragma warning(push)
#pragma warning(disable:4008)

#ifdef Yield
#undef Yield
#endif

#ifdef _DEBUG
#undef _DEBUG
#include <Python.h>
#include <Python-ast.h>
#define _DEBUG
#else
#include <Python.h>
#include <Python-ast.h>
#endif

#pragma warning(pop)