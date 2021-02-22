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

enum InferrenceKind
{
	ENCLOSING = 0,

	OPERATOR_TY_OFFSET = 100,

#ifdef PYTHON_27
	OPERATOR_TY_Add = 101, OPERATOR_TY_Sub = 102, OPERATOR_TY_Mult = 103, OPERATOR_TY_Div = 104, OPERATOR_TY_Mod = 105,
	OPERATOR_TY_Pow = 106, OPERATOR_TY_LShift = 107, OPERATOR_TY_RShift = 108, OPERATOR_TY_BitOr = 109,
	OPERATOR_TY_BitXor = 110, OPERATOR_TY_BitAnd = 111, OPERATOR_TY_FloorDiv = 112,
	OPERATOR_TY_MAX = 113,

#else
	OPERATOR_TY_Add = 101, OPERATOR_TY_Sub = 102, OPERATOR_TY_Mult = 103, OPERATOR_TY_MatMult = 104,
	OPERATOR_TY_Div = 105, OPERATOR_TY_Mod = 106, OPERATOR_TY_Pow = 107,
	OPERATOR_TY_LShift = 108, OPERATOR_TY_RShift = 109, OPERATOR_TY_BitOr = 110, OPERATOR_TY_BitXor = 111,
	OPERATOR_TY_BitAnd = 112, OPERATOR_TY_FloorDiv = 113,

	OPERATOR_TY_MAX = 114,
#endif

	// when accessing the contents of a type, from "array" type and the "indexer" type (can be slice)
	INDEXER = 120,
};