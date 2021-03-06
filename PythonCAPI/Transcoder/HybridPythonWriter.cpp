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

#include "HybridPythonWriter.h"

extern "C" __declspec(dllexport) Hybridizer* _cdecl HybridPython_CreateHybridizer(
	void* handle, 
	LinePrinterCallback fdeclcb, // for nested function
	LinePrinterCallback fbodycb, // for nested function
	LinePrinterCallback kernelcb,  // for entry point
	TypeNameCallback tn, 
	SymbolMapCallback sm, 
	FunctionReturnTypeCallback frt,
	IndexerTypeCallback idt,
	AttributeTypeCallback at,
	InferTypeCallback it, 
	AttributeSetterTypeCallback ast, 
	DefineFunctionReturnCallback dfr,
	ReturnStatementCallback rcb)
{
	Hybridizer* res = new Hybridizer();
	res->_handle = handle;
	res->_funcDeclPrinterCallback = fdeclcb;
	res->_funcBodyPrinterCallback = fbodycb;
	res->_kernelsPrinterCallback = kernelcb;
	res->_typeNameCallback = tn;
	res->_indexerTypeCallback = idt;
	res->_symbolMapCallback = sm;
	res->_functionReturnTypeCallback = frt;
	res->_attributeTypeCallback = at;
	res->_inferTypeCallback = it;
	res->_attributeSetterTypeCallback = ast;
	res->_defineFunctionReturnCallback = dfr;
	res->_returnStatementCallback = rcb;
	return res;
}

extern "C" __declspec(dllexport) void _cdecl HybridPython_DestroyHybridizer(
	Hybridizer* hyb)
{
	delete hyb;
}

extern "C" __declspec(dllexport) HybridPythonResult _cdecl HybridPython_ExpressStatement(
	Hybridizer* hyb, stmt_ty statement)
{
	return hyb->Express(statement, hyb->_kernelsPrinterCallback);
}

extern "C" __declspec(dllexport) stmt_ty _cdecl HybridPython_GetStatement(mod_ty mod, int k) {
	return (stmt_ty) mod->v.Module.body->elements[k];
}

extern "C" __declspec(dllexport) void _cdecl HybridPython_EnterFunctionScope(Hybridizer* hyb)
{
	hyb->EnterFunctionScope();
}

extern "C" __declspec(dllexport) void _cdecl HybridPython_LeaveFunctionScope(Hybridizer* hyb)
{
	hyb->LeaveFunctionScope();
}

extern "C" __declspec(dllexport) void _cdecl HybridPython_DefineSymbol(Hybridizer* hyb, std::string symbol, HybType* t)
{
	hyb->DefineSymbol(symbol, t);
}

#ifndef PYTHON_27
extern "C" __declspec(dllexport) Disassembler* _cdecl HybridPython_CreateDisassembler(PyObject* o)
{
	Disassembler* disasm = new Disassembler(o);
	return disasm;
}

extern "C" __declspec(dllexport) stmt_ty _cdecl HybridPython_DisassemblerProcess(Disassembler* disasm)
{
	disasm->Process();
	return disasm->GetFunctionDeclStatement();
}

extern "C" __declspec(dllexport) void _cdecl HybridPython_DestroyDisassembler(Disassembler* disasm)
{
	delete disasm;
}

extern "C" __declspec(dllexport) HybType* _cdecl HybridPython_DisassemblerGetReturnType(Disassembler* disasm)
{
	return disasm->_returnvalues.size() == 0 ? HybType::builtin<void>() : HybType::getthing() ;
}

#endif