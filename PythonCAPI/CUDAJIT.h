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

#include <string>
#include <map>
#include <vector>

std::pair<std::string, std::string> GeneratePTX(std::string cuda, std::map<std::string,std::string> headers);
// return info_log/error_log pair
std::pair<std::string, std::string> GenerateModule(std::string ptx, void** res, int* size);

void* MapData(void* source, size_t nbytes);
void UnMapData(void* source);

#pragma region Marshalling helpers

enum pytype
{
	py_none = 0x0,

	py_int = 0x1,
	py_double = 0x2,
	py_complex = 0x3,

	py_numericmask = 0xF,

	py_iterator = 0x10,

	py_list = 0x100,
	py_tuple = 0x200,
	py_range = 0x300,
	py_str = 0x400,
	py_bytes = 0x500,
	py_bytearray = 0x600,
	py_memoryview = 0x700,

	py_sequencemask = 0xF00,

	py_set = 0x1000,
	py_frozenset = 0x2000,

	py_setmask = 0xF000,

	py_dict = 0x10000,
	py_other = 0x100000,

	// hybpython extension
	py_numpyndarray_base = 0x1000000, // add dimension at the end up to 255.
	py_numpyndarray_dimmask = 0xFF,

	// lets treat boolean as an int...
	py_bool = 0x200000,

	// function pointers and lambdas
	py_funcptr = 0x400000,
	py_lambda = 0x800000, // lambda may have capture
};

struct pyfuncptr
{
	void* ptr;
};

struct pylambda
{
	void* ptr;
	void* capture;
};

struct thing
{
	union
	{
		pytype _type;
		int64_t _type__padding;
	};

	union
	{
		int _int;
		long long _longlong;
		double _double;
		double _reim[2];
		void* _ptr;
		int _bool;
		// ...
		pyfuncptr _func;
		pylambda _lambda;
	};
};

static_assert(sizeof(thing) == 24, "INCONSISTENT THING SIZE !!!");

#pragma endregion