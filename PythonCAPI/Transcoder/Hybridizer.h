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

#include "HybType.h"
#include "InferrenceKind.h"
#include "HybridPythonResult.h"

/*
typedef enum _operator { Add=1, Sub=2, Mult=3, MatMult=4, Div=5, Mod=6, Pow=7,
						 LShift=8, RShift=9, BitOr=10, BitXor=11, BitAnd=12,
						 FloorDiv=13 } operator_ty;
*/

typedef void(_cdecl *LinePrinterCallback)(void* handle, const char* line);
typedef const char*(_cdecl *TypeNameCallback)(void* handle, const char* symbol);
typedef const char*(_cdecl *SymbolMapCallback)(void* handle, const char* symbol);
typedef const char*(_cdecl *FunctionReturnTypeCallback)(void* handle, const char* symbol, int argcount, const char** args);
typedef const char*(_cdecl *AttributeTypeCallback)(void* handle, const char* valuetype, const char* attribute);
typedef const char*(_cdecl *IndexerTypeCallback)(void* handle, const char* parentType, const char* indexerType);
typedef const char*(_cdecl *AttributeSetterTypeCallback)(void* handle, const char* valuetype, const char* attribute, const char* rht_type);
typedef const char*(_cdecl *InferTypeCallback)(void* handle, InferrenceKind infer, const char* left, const char* right);
typedef void(_cdecl *DefineFunctionReturnCallback)(void* handle, const char* symbol, int argcount, const char** args, const char* returntype);
typedef void(_cdecl *ReturnStatementCallback)(void* handle, const char* returntype);

struct TypedExpression
{
	HybType* _type;
	std::string _value;

	static TypedExpression Build(HybType* type, std::string value)
	{
		TypedExpression res;
		res._type = type;
		res._value = value;
		return res;
	}
};

#include <string>
#include <vector>
#include <map>
#include <list>
#include <deque>
#include <set>

std::string GetNameAsString(expr_ty expr);
std::string GetPyObjectAsString(PyObject* o);
void nullPrinterCallback(void* handle, const char* line);

struct Scope
{
	std::map<std::string, HybType*> _symbols;

	void DefineSymbol(std::string symbol, HybType* type)
	{
		if (_symbols.find(symbol) != _symbols.end())
		{
			if (_symbols[symbol]->_name != type->_name)
				// TODO: check if this is an error...
				throw HybridPythonResult::MULTIPLE_SYMBOL_TYPES;
		}
		else
			_symbols[symbol] = type;
	}

	HybType* GetSymbolType(std::string symbol)
	{
		auto found = _symbols.find(symbol);
		if (found != _symbols.end())
			return found->second;
		return nullptr;
	}
};

#ifndef PYTHON_27
struct Disassembler
{
	#pragma region Conditionned stack

	struct Condition
	{
		bool direct; // true if cond is the predicate, false if !cond is.
		expr_ty cond;

		Condition() {}
		Condition(expr_ty expr, bool d = true)
		{
			cond = expr;
			direct = d;
		}

		Condition bar() const
		{
			Condition res;
			res.cond = cond;
			res.direct = !direct;
			return res;
		}

		bool operator<(const Condition& c) const
		{
			if (cond == c.cond)
				return (int)direct < (int)(c.direct);
			return (int64_t)(void*)cond < (int64_t)(void*)(c.cond);
		}

		bool operator>(const Condition& c) const
		{
			if (cond == c.cond)
				return (int)direct > (int)(c.direct);
			return (int64_t)(void*)cond > (int64_t)(void*)(c.cond);
		}

		bool operator==(const Condition& c) const
		{
			if (direct != c.direct) return false;
			if (cond != c.cond) return false;
			return true;
		}

		bool operator!=(const Condition& c) const
		{
			return !(this->operator==(c));
		}
	};

	struct AndCondition
	{
		std::vector<Condition> _conds;

		void erase(const Condition& cond)
		{
			for (auto iter = _conds.begin() ; iter != _conds.end() ; ++iter)
			{
				if ((*iter) == cond)
				{
					_conds.erase(iter);
					return;
				}
			}
			throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;
		}

		// return true if it has all the conditions of cond, and more
		bool issub(const AndCondition& cond)
		{
			for (int k = 0; k < cond._conds.size(); ++k)
			{
				if (!has(cond._conds[k])) return false;
			}
			return true;
		}

		/// returns true if conditions share the same for each except one
		/// res is only relevant if true is returned
		/// res is part of this
		bool complements(const AndCondition& c, Condition* res)
		{
			if (_conds.size() != c._conds.size()) return false;
			int count = 0;
			for (int k = 0; k < _conds.size(); ++k)
			{
				if (c.has(_conds[k])) continue;
				if (c.has(_conds[k].bar()))
				{
					++count;
					if (res != nullptr)
						*res = _conds[k];
				}
				else
					return false;
			}
			return count == 1;
		}

		bool has(const Condition& c) const
		{
			for (int k = 0; k < _conds.size(); ++k)
			{
				if (_conds[k] == c) return true;
			}
			return false;
		}

		AndCondition operator&=(const Condition& c)
		{
			if (has(c))
				throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;
			_conds.push_back(c);
			return *this;
		}

		// when this is a sub of c, return the additional conditions
		AndCondition knowing(const AndCondition& c) const
		{
			AndCondition result;
			int count = 0;
			for (int k = 0; k < _conds.size(); ++k)
			{
				if (c.has(_conds[k])) ++count;
				else result &= _conds[k];
			}
			if (count != c._conds.size())
				throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;
			return result;
		}

		// when this is a sub of c, return the additional conditions
		AndCondition knowing(const Condition& c) const
		{
			AndCondition result;
			int count = 0;
			for (int k = 0; k < _conds.size(); ++k)
			{
				if (c == _conds[k]) ++count;
				else result &= _conds[k];
			}
			if (count != 1)
				throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;
			return result;
		}

		// return the common set of conditions in this and c
		AndCondition commonset(const AndCondition& c)
		{
			AndCondition result;
			for (int k = 0; k < _conds.size(); ++k)
			{
				if (c.has(_conds[k])) result &= _conds[k];
			}
			return result;
		}

		bool operator<(const AndCondition& c) const
		{
			if (_conds.size() < c._conds.size()) return true;
			if (_conds.size() > c._conds.size()) return false;
			for (int k = 0; k < _conds.size(); ++k)
			{
				if (_conds[k] < c._conds[k]) return true;
				if (_conds[k] > c._conds[k]) return false;
			}
			return false;
		}

		bool operator>(const AndCondition& c) const
		{
			if (_conds.size() > c._conds.size()) return true;
			if (_conds.size() < c._conds.size()) return false;
			for (int k = 0; k < _conds.size(); ++k)
			{
				if (_conds[k] > c._conds[k]) return true;
				if (_conds[k] < c._conds[k]) return false;
			}
			return false;
		}

		bool operator!=(const AndCondition& c) const
		{
			return !(this->operator==(c));
		}

		bool operator==(const AndCondition& c) const
		{
			if (_conds.size() != c._conds.size()) return false;
			for (int k = 0; k < _conds.size(); ++k)
			{
				if (!(_conds[k] == c._conds[k])) return false;
			}
			return true;
		}
	};

	struct OrCondition
	{
		std::vector<AndCondition> _conds;

		static OrCondition all()
		{
			// all are possible !
			OrCondition res;
			res._conds.push_back(AndCondition());
			return res;
		}

		OrCondition operator&=(const Condition& c)
		{
			for (int k = 0; k < _conds.size(); ++k)
			{
				_conds[k] &= c;
			}
			return *this;
		}

		OrCondition knowing(const AndCondition& c) const
		{
			OrCondition res;
			for (int k = 0; k < _conds.size(); ++k)
			{
				res._conds.push_back(_conds[k].knowing(c));
			}
			return res;
		}

		void reduce()
		{
			if (_conds.size() == 1) return;
			// reduce a condition => find entries of type cccT,cccF
			int prevsize = -1;
			while (prevsize != _conds.size())
			{
				prevsize = (int)_conds.size();
				// iterate over conditions
				for (int k = 0; k < _conds.size(); ++k)
				{
					// try find some that is its complement (cccTccc, cccFccc)
					int complcount = 0;
					int complindex = -1;
					Condition cc;
					for (int j = 0; j < _conds.size(); ++j)
					{
						if (j == k) continue;
						// There exist a single differentiator
						if (!(_conds[k].complements(_conds[j], &cc)))
							continue;
						complcount++;
						complindex = j;
					}
					if (complcount > 1)
						throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;
					if (complcount == 1)
					{
						_conds[k].erase(cc);
						// remove duplicate entry
						_conds.erase(_conds.begin() + complindex);
						break;
					}
				}
			}
		}

		OrCondition operator |=(const OrCondition& c)
		{
			for (int k = 0; k < c._conds.size(); ++k)
			{
				_conds.push_back(c._conds[k]);
			}
			reduce();
			return *this;
		}

		bool operator<(const OrCondition& c) const
		{
			if (_conds.size() < c._conds.size()) return true;
			if (_conds.size() > c._conds.size()) return false;
			for (int k = 0; k < _conds.size(); ++k)
			{
				if (_conds[k] < c._conds[k]) return true;
				if (_conds[k] > c._conds[k]) return false;
			}
			return false;
		}

		bool operator==(const OrCondition& c) const
		{
			if (_conds.size() != c._conds.size()) return false;
			for (int k = 0; k < _conds.size(); ++k)
			{
				if (!(_conds[k] == c._conds[k])) return false;
			}
			return true;
		}

		bool operator !=(const OrCondition& c) const
		{
			return !(this->operator==(c));
		}
	};

	struct StackEntry
	{
		std::map<OrCondition, expr_ty> _entry;

		static StackEntry Merge(StackEntry a, StackEntry b);
	};

	struct ExprStack
	{
		std::deque<StackEntry> _stack;
	};

	#pragma endregion
	/*
	struct Loop
	{
		int startindex;
		int endindex;
		OrCondition _cond;
		stmt_ty _stmt;
	};*/

	enum BGNode_type
	{
		block,
		loopevent,
		branch,
	};

	enum BGNodeLocation_type
	{
		loc_if,
		loc_else,
		loc_loop
	};

	enum BGNode_loopevent
	{
		loop_start,
		loop_end,
		loop_break,
		loop_getiter,
		loop_foriter,
	};

	struct BGNodeLocation
	{
		BGNodeLocation_type _kind;
		union
		{
			struct 
			{
				unsigned start; // loopstart BGNode
				unsigned bodystart; // first instruction in body
				unsigned end; // end of the loop, not the else.
			} Loop;
			struct
			{
				unsigned branch;
			} If;
			struct
			{
				unsigned branch;
			} Else;
		} v;

		bool operator==(const BGNodeLocation& a) const
		{
			if (_kind != a._kind) return false;
			if (_kind == BGNodeLocation_type::loc_loop)
			{
				if (v.Loop.start != a.v.Loop.start) return false;
				if (v.Loop.end != a.v.Loop.end) return false;
			}
			if (_kind == BGNodeLocation_type::loc_if)
				return v.If.branch == a.v.If.branch;
			if (_kind == BGNodeLocation_type::loc_else)
				return v.If.branch == a.v.If.branch;
			return true;
		}
		
		bool operator!=(const BGNodeLocation& a) const
		{
			return !(*this == a);
		}

		// branch is the offset of the cond jump
		static BGNodeLocation loc_else(unsigned branch)
		{ 
			BGNodeLocation res; 
			res._kind = BGNodeLocation_type::loc_else; 
			res.v.Else.branch = branch;
			return res;
		}
		// branch is the offset of the cond jump
		static BGNodeLocation loc_if(unsigned branch)
		{
			BGNodeLocation res;
			res._kind = BGNodeLocation_type::loc_if;
			res.v.If.branch = branch;
			return res;
		}
		static BGNodeLocation loc_loop(unsigned from, unsigned to, unsigned bodystart)
		{ 
			BGNodeLocation res; res._kind = BGNodeLocation_type::loc_loop; 
			res.v.Loop.start = from;
			res.v.Loop.end = to;
			res.v.Loop.bodystart = bodystart;
			return res; 
		}

		// return true if candidate is reference or a sub of reference
		static bool ispartof(const std::deque<BGNodeLocation>& reference, const std::deque<BGNodeLocation>& candidate)
		{
			if (candidate.size() < reference.size()) return false;
			auto refiter = reference.begin();
			auto caniter = candidate.begin();
			for (int k = (int)reference.size() ; k < candidate.size() ; ++k)
				++caniter;
			while ((refiter != reference.end()) && (caniter != candidate.end()))
			{
				if (!((*refiter) == (*caniter)))
					return false;
				++refiter;
				++caniter;
			}
			if (refiter != reference.end()) return false;
			if (caniter != candidate.end()) return false;
			return true;
		}
	};

	struct BGNodeLocationOption
	{
		std::deque<BGNodeLocation> _location;

		BGNodeLocation front() const { return _location.front(); }
		size_t size() const { return _location.size(); }

		// return true if front can be removed as it differs and all
		// other are identical
		bool complements(const BGNodeLocationOption& opt)
		{
			if (opt._location.size() != _location.size()) return false;
			if (opt._location.front() == _location.front()) return false;

			if (opt._location.front()._kind == BGNodeLocation_type::loc_else)
				if (_location.front()._kind != BGNodeLocation_type::loc_if)
					return false;
			if (opt._location.front()._kind == BGNodeLocation_type::loc_if)
				if (_location.front()._kind != BGNodeLocation_type::loc_else)
					return false;

			auto iter = _location.begin(); ++iter;
			auto jter = opt._location.begin(); ++jter;

			while (iter != _location.end())
			{
				if (*iter != *jter) return false;
				++iter; ++jter;
			}

			return true;
		}

		// return true if this is a sub of opt - return true if equal
		bool issub(const BGNodeLocationOption& opt) const
		{
			if (_location.size() < opt._location.size()) return false;
			auto iter = _location.begin();
			auto jter = opt._location.begin();
			for (int k = (int)opt._location.size(); k < _location.size(); ++k)
				++iter;
			while (iter != _location.end())
			{
				if (*iter != *jter) return false;
				++iter; ++jter;
			}
			return true;
		}

		bool operator==(const BGNodeLocationOption& opt) const
		{
			return (opt.size() == size()) && issub(opt) ;
		}
		bool operator!=(const BGNodeLocationOption& opt) const
		{
			return !(*this == opt);
		}
	};

	struct BGNodeLocationScenario
	{
	private:
		std::vector<BGNodeLocationOption> _options;
	public:
		void addoption(const BGNodeLocationOption& opt)
		{
			for (int k = 0; k < _options.size(); ++k)
			{
				if (opt.issub(_options[k]))
					return;
			}
			_options.push_back(opt);
		}
		BGNodeLocationOption reduce();
	};

	struct BGNode
	{
		unsigned _sourceline;

		unsigned _index;
		OrCondition _cond;
		std::vector<unsigned> _arrivefrom;
		BGNode_type _kind;
		BGNodeLocationOption _location;

		BGNodeLocationScenario _locscenario;

		//union -- seems that using a vector in the union makes it impossible...
		//{
			struct 
			{
				std::vector<stmt_ty> _statements;
			} Block;
			struct
			{
				struct
				{
					stmt_ty _stmt;
				} LoopBreak;
				struct
				{
					unsigned _endloop;
				} LoopStart;
				struct
				{
					expr_ty _range;
				} GetIter;
				struct
				{
					expr_ty _target;
					unsigned _endloop;
				} ForIter;
				BGNode_loopevent _kind;
			} Loopevent;
			struct 
			{
				unsigned _target;
				// which condition for the jump to occur (if cond is nullptr => always)
				// usually, _cond.direct = false
				Condition _cond; // c.expr_ty null for undoncitional
			} Branch;
		//} v;
	};

	// https://docs.python.org/3.6/library/dis.html
	// Python 3.6
	enum OpCodes : unsigned char
	{
		POP_TOP = 1,
		ROT_TWO = 2,
		ROT_THREE = 3,
		DUP_TOP = 4,
		DUP_TOP_TWO = 5,
		NOP = 9,
		UNARY_POSITIVE = 10,
		UNARY_NEGATIVE = 11,
		UNARY_NOT = 12,
		UNARY_INVERT = 15,
		BINARY_MATRIX_MULTIPLY = 16,
		INPLACE_MATRIX_MULTIPLY = 17,
		BINARY_POWER = 19,
		BINARY_MULTIPLY = 20,
		BINARY_MODULO = 22,
		BINARY_ADD = 23,
		BINARY_SUBTRACT = 24,
		BINARY_SUBSCR = 25,
		BINARY_FLOOR_DIVIDE = 26,
		BINARY_TRUE_DIVIDE = 27,
		INPLACE_FLOOR_DIVIDE = 28,
		INPLACE_TRUE_DIVIDE = 29,
		GET_AITER = 50,
		GET_ANEXT = 51,
		BEFORE_ASYNC_WITH = 52,
		INPLACE_ADD = 55,
		INPLACE_SUBTRACT = 56,
		INPLACE_MULTIPLY = 57,
		INPLACE_MODULO = 59,
		STORE_SUBSCR = 60,
		DELETE_SUBSCR = 61,
		BINARY_LSHIFT = 62,
		BINARY_RSHIFT = 63,
		BINARY_AND = 64,
		BINARY_XOR = 65,
		BINARY_OR = 66,
		INPLACE_POWER = 67,
		GET_ITER = 68,
		GET_YIELD_FROM_ITER = 69,
		PRINT_EXPR = 70,
		LOAD_BUILD_CLASS = 71,
		YIELD_FROM = 72,
		GET_AWAITABLE = 73,
		INPLACE_LSHIFT = 75,
		INPLACE_RSHIFT = 76,
		INPLACE_AND = 77,
		INPLACE_XOR = 78,
		INPLACE_OR = 79,
		BREAK_LOOP = 80,
		WITH_CLEANUP_START = 81,
		WITH_CLEANUP_FINISH = 82,
		RETURN_VALUE = 83,
		IMPORT_STAR = 84,
		SETUP_ANNOTATIONS = 85,
		YIELD_VALUE = 86,
		POP_BLOCK = 87,
		END_FINALLY = 88,
		POP_EXCEPT = 89,
		HAVE_ARGUMENT = 90,
		STORE_NAME = 90,
		DELETE_NAME = 91,
		UNPACK_SEQUENCE = 92,
		FOR_ITER = 93,
		UNPACK_EX = 94,
		STORE_ATTR = 95,
		DELETE_ATTR = 96,
		STORE_GLOBAL = 97,
		DELETE_GLOBAL = 98,
		LOAD_CONST = 100,
		LOAD_NAME = 101,
		BUILD_TUPLE = 102,
		BUILD_LIST = 103,
		BUILD_SET = 104,
		BUILD_MAP = 105,
		LOAD_ATTR = 106,
		COMPARE_OP = 107,
		IMPORT_NAME = 108,
		IMPORT_FROM = 109,
		JUMP_FORWARD = 110,
		JUMP_IF_FALSE_OR_POP = 111,
		JUMP_IF_TRUE_OR_POP = 112,
		JUMP_ABSOLUTE = 113,
		POP_JUMP_IF_FALSE = 114,
		POP_JUMP_IF_TRUE = 115,
		LOAD_GLOBAL = 116,
		CONTINUE_LOOP = 119,
		SETUP_LOOP = 120,
		SETUP_EXCEPT = 121,
		SETUP_FINALLY = 122,
		LOAD_FAST = 124,
		STORE_FAST = 125,
		DELETE_FAST = 126,
		STORE_ANNOTATION = 127,
		RAISE_VARARGS = 130,
		CALL_FUNCTION = 131,
		MAKE_FUNCTION = 132,
		BUILD_SLICE = 133,
		LOAD_CLOSURE = 135,
		LOAD_DEREF = 136,
		STORE_DEREF = 137,
		DELETE_DEREF = 138,
		CALL_FUNCTION_KW = 141,
		CALL_FUNCTION_EX = 142,
		SETUP_WITH = 143,
		EXTENDED_ARG = 144,
		LIST_APPEND = 145,
		SET_ADD = 146,
		MAP_ADD = 147,
		LOAD_CLASSDEREF = 148,
		BUILD_LIST_UNPACK = 149,
		BUILD_MAP_UNPACK = 150,
		BUILD_MAP_UNPACK_WITH_CALL = 151,
		BUILD_TUPLE_UNPACK = 152,
		BUILD_SET_UNPACK = 153,
		SETUP_ASYNC_WITH = 154,
		FORMAT_VALUE = 155,
		BUILD_CONST_KEY_MAP = 156,
		BUILD_STRING = 157,
		BUILD_TUPLE_UNPACK_WITH_CALL = 158,
	};

	std::map<int, OrCondition> _conditionsbyoffset; // by offset...
	std::map<int, ExprStack> _stackbyoffset; // by offset...
	ExprStack _stack; // current stack

	std::map<unsigned, BGNode> _branchinggraph;
	//std::deque <Loop> _loops;

	std::string _name;
	std::vector<expr_ty> _constants; // need to be converted from PyObject*
	std::vector<PyObject*> _globals; // need to be converted from PyObject*
	std::vector<PyObject*> _varnames;

	void Process();

	std::vector<expr_ty> _returnvalues;

	stmt_ty GetFunctionDeclStatement();

	~Disassembler()
	{
		for (int k = 0; k < _exprtobecleaned.size(); ++k)
		{
			delete _exprtobecleaned[k];
		}
		_exprtobecleaned.clear();
		for (int k = 0; k < _stmttobecleaned.size(); ++k)
		{
			delete _stmttobecleaned[k];
		}
		_stmttobecleaned.clear();		
		for (int k = 0; k < _slicestobecleaned.size(); ++k)
		{
			delete _slicestobecleaned[k];
		}
		_slicestobecleaned.clear();		
#ifndef PYTHON_27
		for (int k = 0; k < _argstobecleaned.size(); ++k)
		{
			delete _argstobecleaned[k];
		}
		_argstobecleaned.clear();
#endif
		for (int k = 0; k < _asdl_seqtobecleaned.size(); ++k)
		{
			::free(_asdl_seqtobecleaned[k]);
		}
		_asdl_seqtobecleaned.clear();
		for (int k = 0; k < _asdl_int_seqtobecleaned.size(); ++k)
		{
			::free(_asdl_int_seqtobecleaned[k]);
		}
		_asdl_int_seqtobecleaned.clear();
	}

	Disassembler(PyObject* code);

private:
	// keep track of object to be cleaned-up
	std::vector<expr_ty> _exprtobecleaned;
	std::vector<stmt_ty> _stmttobecleaned;
	std::vector<slice_ty> _slicestobecleaned;
	std::vector<asdl_seq*> _asdl_seqtobecleaned;
	std::vector<asdl_int_seq*> _asdl_int_seqtobecleaned;
	std::vector<arguments_ty> _argliststobecleaned;
#ifndef PYTHON_27
	std::vector<arg_ty> _argstobecleaned;
#endif

	asdl_seq* alloc_asdl_seq(int size);
	asdl_int_seq* alloc_asdl_int_seq(int size);
	template<typename T> T alloc();

	// source code of the function
	unsigned char* _code;
	int _codelen;
	int _argcount;
	int _nblocals;

	#pragma region Debugging information
	std::string _filename;
	int _firstlineno;
	unsigned char* _lnotab;

	int _currentline;
	int _prevlnotab;
	unsigned char*  _currentlnotab;
	#pragma endregion

	// https://docs.python.org/3/library/dis.html#python-bytecode-instructions
	void AppendCode(unsigned char* codes, int& index);

	void AddBGNode(const BGNode& node);

	// current conditions
	OrCondition _currentconditions; // most recent is last in vector
	bool _stackforward; // true of stack should be forwarded to next instruction.
	// stack operations
	expr_ty StackPopFront();
	void StackPushFront(expr_ty);
	ExprStack StackMerge(const ExprStack& predicted, const ExprStack& current);

	expr_ty BuildBoolopFromConds(const AndCondition& conds);
	expr_ty BuildIfExprFromStackEntry(const StackEntry& entry, const OrCondition& current);
	asdl_seq* BuildASDLSequence(const std::vector<stmt_ty>& statements);
	asdl_seq* BuildASDLSequence(const std::vector<expr_ty>& statements);

	#pragma region branching graph routines

	void MergeBlockStatements();
	void FeedArriveFrom();
	void RemoveUnreachableCode();
	void AssignLocations();
	// to is inclusive
	std::vector<stmt_ty> ExtractBlock(unsigned from, unsigned to);
	// to is inclusive
	std::pair<unsigned,unsigned> GetLocationRange(const BGNodeLocationOption& loc, unsigned from, unsigned to);

	std::map<expr_ty,int> _expressionsByteCodeLocations;
	int GetBytecodeLocation(expr_ty ex)
	{
		auto found = _expressionsByteCodeLocations.find(ex);
		if (found == _expressionsByteCodeLocations.end()) return -1;
		return found->second;
	}

	std::set<expr_ty> _inplaceexpressions;
	bool IsInplace(expr_ty expr)
	{
		return _inplaceexpressions.find(expr) != _inplaceexpressions.end();
	}
	void RegisterInplace(expr_ty expr)
	{
		_inplaceexpressions.insert(expr);
	}

	#pragma endregion

	#pragma region lambda-related

	expr_ty _closureTuple; // name '__closure__'

	#pragma endregion
};
#endif

struct Hybridizer
{
	// client-side handle
	void* _handle;

	// callbacks
	LinePrinterCallback _funcDeclPrinterCallback;
	LinePrinterCallback _funcBodyPrinterCallback;
	LinePrinterCallback _kernelsPrinterCallback;
	TypeNameCallback _typeNameCallback;
	SymbolMapCallback _symbolMapCallback;
	FunctionReturnTypeCallback _functionReturnTypeCallback;
	IndexerTypeCallback _indexerTypeCallback;
	AttributeTypeCallback _attributeTypeCallback;
	AttributeSetterTypeCallback _attributeSetterTypeCallback;
	InferTypeCallback _inferTypeCallback;
	DefineFunctionReturnCallback _defineFunctionReturnCallback;
	ReturnStatementCallback _returnStatementCallback;

	// express 
	TypedExpression Express(expr_ty expr);
	TypedExpression Express(slice_ty slice);

	// for code organization purposes.
	template <_expr_kind kind> TypedExpression Express(expr_ty expr);
	HybridPythonResult Express(stmt_ty stmt, LinePrinterCallback cp);
	HybridPythonResult ExpressForIterator(expr_ty target, expr_ty iter);

	HybType* InferType(InferrenceKind kind, HybType* left, HybType* right);

	std::list<Scope> _scopes; // first is inner-most

	bool IsBuiltinFunc(std::string symbol);


	// function is intrinsic if has a __hybrid_cuda__intrinsic_function__ or __hybrid_cuda__intrinsic_constant__
	// attribute
	enum IntrinsicKind
	{
		NOT_INTRINSIC = 0,
		INTRINSIC_FUNCTION = 1,
		INTRINSIC_CONSTANT = 2, // skip parameters and parenthesis
		INTRINSIC_TYPE = 3, // don't use get_attr
	};
	IntrinsicKind IsIntrinsic(const std::string& name, std::string* intrinsicname);
	IntrinsicKind IsIntrinsic(expr_ty function, std::string* intrinsicname);
	bool IsDefined(std::string symbol, HybType** found = nullptr)
	{
		// python built-in symbols
		if (IsBuiltinFunc(symbol))
		{
			if (found != nullptr)
				*found = HybType::builtinfunction();
			return true;
		}

		for (auto iter = _scopes.begin(); iter != _scopes.end(); ++iter)
		{
			HybType* type = (*iter).GetSymbolType(symbol);
			if (type != nullptr)
			{
				if (found != nullptr)
					*found = type;
				return true;
			}
		}
		return false;
	}

	// get the type of a symbol
	HybType* GetType(std::string symbol)
	{
		HybType* res;
		if (IsDefined(symbol, &res)) 
			return res;
		const char* tn = _typeNameCallback(_handle, symbol.c_str());
		if (tn == nullptr)
			throw HybridPythonResult::UNKNOWN_SYMBOL_TYPE;
		return HybType::forge(tn); 
	}

	HybType* GetElementType(HybType* t);
	HybType* GetAttributeType(HybType* t, std::string attribute);
	HybType* GetAttributeSetterType(HybType* t, std::string attribute, HybType* rht);

	// return true if symbol is known
	bool SymbolKnown(std::string symbol)
	{
		if (IsDefined(symbol)) return true;
		return _typeNameCallback(_handle, symbol.c_str()) != nullptr;
	}

	void DefineSymbol(std::string symbol, HybType* type)
	{
		// TODO: get more information in location to avoid typing conflict !
		_scopes.front().DefineSymbol(symbol, type);
	}

	void EnterFunctionScope() { _scopes.push_front(Scope()); }
	void LeaveFunctionScope() { _scopes.pop_front(); }

	std::string GetCallSymbolMapping(std::string symbol);
};