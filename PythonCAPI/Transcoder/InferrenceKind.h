// (c) ALTIMESH 2019 -- all rights reserved

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