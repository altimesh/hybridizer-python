// (c) ALTIMESH 2019 -- all rights reserved


#include "../ImportPython.h"
#include "Hybridizer.h"

void nullPrinterCallback(void* handle, const char* line) {}

#pragma region Hybridizer

std::string Hybridizer::GetCallSymbolMapping(std::string symbol)
{
	if (symbol == "float") return "hybpython::__builtin_float";
	if (symbol == "print") return "hybpython::__builtin_print"; 
	if (symbol == "complex") return "hybpython::__builtin_complex";
	return _symbolMapCallback(_handle, symbol.c_str());
}

bool Hybridizer::IsBuiltinFunc(std::string symbol)
{
	if (symbol == "float") return true;
	if (symbol == "print") return true;
	if (symbol == "complex") return true;
	// TODO : complete according to https://docs.python.org/3/library/functions.html
	return false;
}

static std::string UNICODE_TO_STRING(PyObject* o)
{
	char buffer[1024];
#ifdef PYTHON_27
	char* ptr = PyString_AsString(PyObject_Str(o));
	memcpy(buffer, ptr, strlen(ptr));
	return std::string(ptr);
#else
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
#endif
}

Hybridizer::IntrinsicKind Hybridizer::IsIntrinsic(expr_ty function, std::string* intrinsicname)
{
	if (function->kind != _expr_kind::Name_kind) return Hybridizer::IntrinsicKind::NOT_INTRINSIC;
	std::string name = UNICODE_TO_STRING(function->v.Name.id);

	return IsIntrinsic(name, intrinsicname);
}

Hybridizer::IntrinsicKind Hybridizer::IsIntrinsic(const std::string& name, std::string* intrinsicname)
{
	// find corresponding PyObject
	PyObject * dict = PyEval_GetGlobals();
	if(nullptr == dict)
		return Hybridizer::IntrinsicKind::NOT_INTRINSIC;
	PyObject* func = PyDict_GetItemString(dict, name.c_str());
	if (func == nullptr) return Hybridizer::IntrinsicKind::NOT_INTRINSIC;

	if (PyObject_HasAttrString(func, HYBRID_CUDA_INTRINSIC_FUNCTION_ATTR_NAME))
	{
		PyObject* intrinsicfunc = PyObject_GetAttrString(func, HYBRID_CUDA_INTRINSIC_FUNCTION_ATTR_NAME);
		*intrinsicname = UNICODE_TO_STRING(intrinsicfunc);
		return Hybridizer::IntrinsicKind::INTRINSIC_FUNCTION;
	}
	else if (PyObject_HasAttrString(func, HYBRID_CUDA_INTRINSIC_CONSTANT_ATTR_NAME))
	{
		PyObject* intrinsicfunc = PyObject_GetAttrString(func, HYBRID_CUDA_INTRINSIC_CONSTANT_ATTR_NAME);
		*intrinsicname = UNICODE_TO_STRING(intrinsicfunc);
		return Hybridizer::IntrinsicKind::INTRINSIC_CONSTANT;
	}
	else if (PyObject_HasAttrString(func, HYBRID_CUDA_INTRINSIC_TYPE_ATTR_NAME))
	{
		PyObject* intrinsicfunc = PyObject_GetAttrString(func, HYBRID_CUDA_INTRINSIC_TYPE_ATTR_NAME);
		*intrinsicname = UNICODE_TO_STRING(intrinsicfunc);
		return Hybridizer::IntrinsicKind::INTRINSIC_TYPE;
	}
	return Hybridizer::IntrinsicKind::NOT_INTRINSIC;
}

HybType* Hybridizer::InferType(InferrenceKind kind, HybType* left, HybType* right)
{
	if (kind == InferrenceKind::ENCLOSING)
	{
		if (left == right) {
			return left ;
		}
		else if (left->IsScalar() && right->IsScalar())
		{
			int leftScalarIndex = left->ScalarIndex();
			int rightScalarIndex = right->ScalarIndex();

			int common = leftScalarIndex > rightScalarIndex ? leftScalarIndex : rightScalarIndex;

			return HybType::scalar(common);
		}
		else if (left->IsScalar() && right == HybType::getthing()) {
			return HybType::getthing() ;
		} 
		else if (right->IsScalar() && left == HybType::getthing()) {
			return HybType::getthing() ;
		}
	}
	else if ((kind > InferrenceKind::OPERATOR_TY_OFFSET) && (kind < InferrenceKind::OPERATOR_TY_MAX))
	{
		switch (kind)
		{
		case InferrenceKind::OPERATOR_TY_Div:
			#if PYTHON_VERSION_MAJOR==3
			{
				/*
					>>> type(5/2)
					<class 'float'>
					>>> type(6/2)
					<class 'float'>
				*/
				HybType* res = InferType(InferrenceKind::ENCLOSING, left, right);
				if (res->ScalarIndex() == 1) // for integer types, return float, otherwise, stay identical
					return HybType::scalar(2);
				return res;
			}
			#elif PYTHON_VERSION_MAJOR==2
			return InferType(InferrenceKind::ENCLOSING, left, right);
			#else
			#error unsupported PYTHON_VERSION_MAJOR
			#endif
		case InferrenceKind::OPERATOR_TY_Add:
		case InferrenceKind::OPERATOR_TY_Mult:
		case InferrenceKind::OPERATOR_TY_Sub:
		case InferrenceKind::OPERATOR_TY_Pow:
		case InferrenceKind::OPERATOR_TY_Mod:
		case InferrenceKind::OPERATOR_TY_FloorDiv:

		case InferrenceKind::OPERATOR_TY_BitAnd:
		case InferrenceKind::OPERATOR_TY_BitOr:
		case InferrenceKind::OPERATOR_TY_BitXor:

		case InferrenceKind::OPERATOR_TY_LShift:
		case InferrenceKind::OPERATOR_TY_RShift:
			return InferType(InferrenceKind::ENCLOSING, left, right);
		default:
			ThrowNotImplemented();
		}
	}
	return HybType::forge(_inferTypeCallback(_handle, kind, left->_name.c_str(), right->_name.c_str()));
}

std::string ExpressCompareOpFunctor(cmpop_ty compare)
{
	switch (compare)
	{
	case cmpop_ty::Gt:
		return "hybpython::compareops::gt";
	case cmpop_ty::Lt:
		return "hybpython::compareops::lt";
	case cmpop_ty::Eq:
		return "hybpython::compareops::eq";
	case cmpop_ty::GtE:
		return "hybpython::compareops::gte";
	case cmpop_ty::LtE:
		return "hybpython::compareops::lte";
	case cmpop_ty::NotEq:
		return "hybpython::compareops::noteq";
	}

	throw HybridPythonResult::UNSUPPORTED_COMPARE_OPERATOR;
	return "";
}

template <> TypedExpression Hybridizer::Express<_expr_kind::Num_kind>(expr_ty expr)
{
	char buffer[1024];

	PyObject* o = expr->v.Num.n;
#ifdef PYTHON_27
	if (PyInt_Check(o)) {
		TypedExpression res;
		res._type = HybType::builtin<int64_t>();
		int value = PyInt_AS_LONG(o);
		// TODO : check overflow !!
		::snprintf(buffer, 1024, "hybpython::pylong (%d)", value);
		res._value = std::string(buffer);
		return res;
	}
	else if (PyLong_Check(o))
#else
	if (PyLong_Check(o))
#endif
	{
		int overflow;
		TypedExpression res;
		res._type = HybType::builtin<int64_t>();
		int value = PyLong_AsLongAndOverflow(o, &overflow);
		// TODO : check overflow !!
		::snprintf(buffer, 1024, "hybpython::pylong (%d)", value);
		res._value = std::string(buffer) ;
		return res;
		/*
			PyLongObject

			PyLong pl = PyLong.TryCast(expr.Num.n);
		if (pl != null) return new TypedExpression("hybpython::pylong (" + pl.Value.ToString() + ")", _expressor.MapPythonType(PythonTypes.py_int));
		PyFloat pf = PyFloat.TryCast(expr.Num.n);
		if (pf != null) return new TypedExpression("hybpython::pyfloat (" + pf.Value.ToString("G") + ")", _expressor.MapPythonType(PythonTypes.py_double));
		PyComplex pc = PyComplex.TryCast(expr.Num.n);
		if (pc != null) return new TypedExpression(String.Format(
			"hybpython::complex({0},{1})",
			pc.Value.re.ToString("G"), pc.Value.im.ToString("G")), _expressor.MapPythonType(PythonTypes.py_complex));
		*/
	}
	else if (PyFloat_Check(o))
	{
		TypedExpression res;
		res._type = HybType::builtin<double>();
		double value = PyFloat_AsDouble(o);
		::snprintf(buffer, 1024, "%lf", value);
		res._value = std::string(buffer);
		return res;
	}
	else
		ThrowNotImplemented();
}

std::string GetNameAsString(expr_ty expr)
{
	return GetPyObjectAsString(expr->v.Name.id);
}

std::string GetPyObjectAsString(PyObject* o) 
{
#ifdef PYTHON_27
	return UNICODE_TO_STRING(o);
#else 
	if (PyUnicode_CheckExact(o))
	{
		switch (PyUnicode_KIND(o))
		{
		case PyUnicode_Kind::PyUnicode_1BYTE_KIND:
		{
			std::string symbol = UNICODE_TO_STRING(o);
			return symbol;
		}
		default:
			ThrowNotImplemented();
		}
	}
	else
		ThrowNotImplemented();
#endif
}

template <> TypedExpression Hybridizer::Express<_expr_kind::Name_kind>(expr_ty expr)
{
	PyObject* o = expr->v.Name.id;

	try
	{
		std::string symbol = GetNameAsString(expr);
		// SMALL "hack/hardcoded constant" for lambda captures
		if (symbol == "__closure__")
			return TypedExpression::Build(HybType::getthing(), symbol);
#ifdef PYTHON_27
		if(symbol == "True")
			return TypedExpression::Build(HybType::forge("bool"), "true");
		if (symbol == "False")
			return TypedExpression::Build(HybType::forge("bool"), "false");
#endif
		return TypedExpression::Build(GetType(symbol), symbol);
	}
	catch (HybridPythonResult hpr)
	{
		throw hpr;
	}
}

#ifndef PYTHON_27
template <> TypedExpression Hybridizer::Express<_expr_kind::NameConstant_kind>(expr_ty expr)
{
	singleton o = expr->v.NameConstant.value;
	
	try
	{
		if (PyBool_Check(o)) {
			if (PyObject_IsTrue(o))
				return TypedExpression::Build(HybType::builtin<bool>(), "true");
			else
				return TypedExpression::Build(HybType::builtin<bool>(), "false");
		}
		else {
			throw HybridPythonResult::UNKNOWN_NAME_CONSTANT;
		}
	}
	catch (HybridPythonResult hpr)
	{
		throw hpr;
	}
}
#endif

template <> TypedExpression Hybridizer::Express<_expr_kind::Str_kind>(expr_ty expr)
{
	//char buffer[1024];
	PyObject* o = expr->v.Str.s;

	
#ifdef PYTHON_27
	std::string symbol = UNICODE_TO_STRING(o);


	std::string str = "\"";
	for (int k = 0; k < symbol.length(); ++k)
	{
		// TODO : check specials !
		if (symbol[k] == '\t') str += "\\t";
		else if (symbol[k] == '\n') str += "\\n";
		else str += symbol[k];
	}
	str += "\"";

	return TypedExpression::Build(HybType::builtin<const char*>(), str);

#else
	if (PyUnicode_CheckExact(o))
	{
		switch (PyUnicode_KIND(o))
		{
		case PyUnicode_Kind::PyUnicode_1BYTE_KIND:
		{
			// convert to expressible string (replace special chars by chars
			size_t length = (size_t)PyUnicode_GET_LENGTH(o);

			const char* input = (const char*)PyUnicode_1BYTE_DATA(o);

			std::string str = "\"";
			for (int k = 0; k < length; ++k)
			{
				// TODO : check specials !
				if (input[k] == '\t') str += "\\t";
				else if (input[k] == '\n') str += "\\n";
				else str += input[k];
			}
			str += "\"";

			return TypedExpression::Build(HybType::builtin<const char*>(), str);
		}
		default:
			ThrowNotImplemented();
		}
	}
	else
		ThrowNotImplemented();
#endif
}

template <> TypedExpression Hybridizer::Express < _expr_kind::Call_kind >(expr_ty expr)
{
	std::string returnType;

	// TODO : support keywords ??

	asdl_seq* args = expr->v.Call.args;
	int argcount = args == nullptr ? 0 : (int)args->size;
	std::vector<TypedExpression> params;
	std::vector<const char*> paramTypes;

	for (int k = 0; k < argcount; ++k)
	{
		TypedExpression te = Express((expr_ty)args->elements[k]);
		params.push_back(te);
		paramTypes.push_back(params[k]._type->_name.c_str()); // te will be destroyed by the time we iterate, hence _name.c_str()
	}

	std::vector<const char*> expectedArgTypes = paramTypes ;

	TypedExpression func;
	{
		char buffer[1024];
		auto tmp = expr->v.Call.func;
		if (tmp->kind == _expr_kind::Name_kind) {
			std::string funcName = GetNameAsString(tmp);

			const char** pt0 = expectedArgTypes.size() > 0 ? &(expectedArgTypes[0]) : nullptr;
			int nbargs = args == nullptr ? 0 : (int)(args->size);

			HybType* funcType;
			if (IsDefined(funcName, &funcType))
			{
				if (funcType == HybType::builtinfunction())
					returnType = _functionReturnTypeCallback(_handle, funcName.c_str(), nbargs, pt0);
				else if (funcType == HybType::getthing())
					returnType = HybType::getthing()->_name.c_str();
				else
					returnType = _functionReturnTypeCallback(_handle, funcName.c_str(), nbargs, pt0);
			}
			else
				returnType = _functionReturnTypeCallback(_handle, funcName.c_str(), nbargs, pt0);

			func = TypedExpression::Build(HybType::forge(returnType), funcName);
		}
		else {
			TypedExpression value = Express(tmp->v.Attribute.value);
			std::string attribute = UNICODE_TO_STRING(tmp->v.Attribute.attr);

			std::string attributeType = GetAttributeType(value._type, attribute)->_name;

			::snprintf(buffer, 1024, "%s.%s", value._value.c_str(), attribute.c_str());

			func = TypedExpression::Build(HybType::forge(attributeType), buffer);
			returnType = func._type->_name;
			expectedArgTypes = paramTypes;
		}
	}

	std::string callLine;
	// is it an intrinsic ?
	std::string intrinsic;
	Hybridizer::IntrinsicKind kind = IsIntrinsic(expr->v.Call.func, &intrinsic);
	if (kind == Hybridizer::IntrinsicKind::INTRINSIC_CONSTANT)
	{
		return TypedExpression::Build(HybType::forge(returnType), intrinsic);
	}
	else if (kind == Hybridizer::IntrinsicKind::INTRINSIC_FUNCTION)
	{
		callLine = intrinsic;
	} 
	else 
	{
#ifndef PYTHON_27
		HybType* sType = this->GetType(func._value);
		// is func a local symbol ? => Don't use symbol mapping
		if ((sType == HybType::forge("function")) || 
			(sType == HybType::forge("builtinfunction")))
			callLine = GetCallSymbolMapping(func._value); 
		else
			callLine = func._value;
#else
		callLine = GetCallSymbolMapping(func._value);
#endif
	}

	callLine += " ( ";
	for (int k = 0; k < argcount; ++k)
	{
		if (k != 0) callLine += " , ";
		if (::strcmp(expectedArgTypes[k], paramTypes[k]) != 0)
			callLine += "hybpython::cast<" + std::string(expectedArgTypes[k]) + "," + std::string(paramTypes[k]) + ">::from (" + params[k]._value + ")";
		else
			callLine += params[k]._value;
	}
	callLine += " ) ";

	/*
	const char** pt0 = paramTypes.size() > 0 ? &(paramTypes[0]) : nullptr;
	std::string returntype = func._type->_name;
	std::string funcname = func._value;
	if (funcname == "print")
		returntype = "void";
	else if (funcname == "float")
		returntype = "double";
		*/

	return TypedExpression::Build(HybType::forge(returnType), callLine);
}

HybType* Hybridizer::GetAttributeType(HybType* type, std::string attribute)
{
// check if type is intrinsic type
	std::string intrinName;
	// if type => probably an intrinsic
	IntrinsicKind isintrin = IsIntrinsic(type->_name, &intrinName);
	if (isintrin == IntrinsicKind::INTRINSIC_TYPE)
	{
		PyObject* globals = PyEval_GetGlobals();
		if (globals == nullptr)
			throw HybridPythonResult::CANNOT_ACCESS_GLOBALS_DICT;
		PyObject* typeobj = PyDict_GetItemString(globals, type->_name.c_str());
		if (typeobj == nullptr)
			throw HybridPythonResult::CANNOT_FIND_TYPE;
		// look for annotations -- access type
		PyObject* annotations = PyObject_GetAttrString(typeobj, "__annotations__");
		PyObject* attrtype = PyDict_GetItemString(annotations, attribute.c_str());
		if (attrtype != nullptr)
		{
			// attrtype is a type =>
			if (!PyType_Check(attrtype))
				throw HybridPythonResult::CANNOT_FIND_TYPE_ANNOTATION;
			PyObject* name = PyObject_GetAttrString(attrtype, "__qualname__");
			if( !PyUnicode_Check(name))
				throw HybridPythonResult::CANNOT_FIND_TYPE_ANNOTATION;
			std::string attrtypename = UNICODE_TO_STRING(name);
			return HybType::forge(attrtypename);
		}
		else
			throw HybridPythonResult::UNDEFINED_ATTRIBUTE_TYPE_IN_INTRINSIC;
	}
	const char* at = _attributeTypeCallback(_handle, type->_name.c_str(), attribute.c_str());
	if (at == nullptr)
		throw HybridPythonResult::UNKNOWN_ATTRIBUTE_TYPE;

	return HybType::forge(at);
}

HybType* Hybridizer::GetAttributeSetterType(HybType* type, std::string attribute, HybType* rht)
{
	const char* ast = _attributeSetterTypeCallback(_handle, type->_name.c_str(), attribute.c_str(), rht->_name.c_str());
	if (ast == nullptr)
		throw HybridPythonResult::UNKNOWN_ATTRIBUTE_TYPE;

	return HybType::forge(ast);
}

template <> TypedExpression Hybridizer::Express < _expr_kind::Attribute_kind >(expr_ty expr)
{
	char buffer[1024];

	TypedExpression value = Express(expr->v.Attribute.value);
	std::string attribute = UNICODE_TO_STRING(expr->v.Attribute.attr);

		std::string attributeType;

	// accessing static data => value._type == "type"
	if (value._type == HybType::forge("type"))
	{
		// type name is the value
		attributeType = GetAttributeType(HybType::forge(value._value), attribute)->_name;
	} else {
		attributeType = GetAttributeType(value._type, attribute)->_name;
	}

	std::string intrinsicName;
	Hybridizer::IntrinsicKind intrin = IsIntrinsic(value._value, &intrinsicName);

	if (intrin == Hybridizer::IntrinsicKind::INTRINSIC_TYPE)
		::snprintf(buffer, 1024, "%s.%s", value._value.c_str(), attribute.c_str());
	else
		::snprintf(buffer, 1024, "%s.get_attribute<%s>(\"%s\")", value._value.c_str(), attributeType.c_str(), attribute.c_str());


	return TypedExpression::Build(HybType::forge(attributeType), buffer);
}

template <> TypedExpression Hybridizer::Express < _expr_kind::BoolOp_kind >(expr_ty expr)
{
	char buffer[1024];
	auto op = expr->v.BoolOp.op;
	assert(expr->v.BoolOp.values->size == 2);
	switch (op) 
	{
		case boolop_ty::And:
			::sprintf(buffer, "(%s) && (%s)", Express((expr_ty) (expr->v.BoolOp.values->elements[0]))._value.c_str(), Express((expr_ty)(expr->v.BoolOp.values->elements[1]))._value.c_str());
			break;
		case boolop_ty::Or:
			::sprintf(buffer, "(%s) || (%s)", Express((expr_ty)(expr->v.BoolOp.values->elements[0]))._value.c_str(), Express((expr_ty)(expr->v.BoolOp.values->elements[1]))._value.c_str());
			break;
	}
	return TypedExpression::Build(HybType::builtin<bool>(), buffer);
}

template <> TypedExpression Hybridizer::Express < _expr_kind::IfExp_kind >(expr_ty expr)
{
	char buffer[1024];

	TypedExpression body = Express(expr->v.IfExp.body);
	expr_ty func = expr->v.IfExp.test;
	if (expr->v.IfExp.orelse == nullptr)
	{
		::snprintf(buffer, 1024, "(%s) ? (%s) : hybridpython::buildnone()", Express(func)._value.c_str(), body._value.c_str());
		return TypedExpression::Build(HybType::getthing(), buffer);
	}
	else {
		TypedExpression orelse = Express(expr->v.IfExp.orelse);
		HybType* inferred = InferType(InferrenceKind::ENCLOSING, body._type, orelse._type);
		// get cast
		int offset = ::snprintf(buffer, 1024, "hybpython::ternary <%s> (%s , ", inferred->_name.c_str(), Express(func)._value.c_str());
		if (body._type != inferred)
			offset += ::snprintf(buffer + offset, 1024 - offset, "hybpython::cast <%s,%s>::from (%s)", inferred->_name.c_str(), body._type->_name.c_str(), body._value.c_str());
		else
			offset += ::snprintf(buffer + offset, 1024 - offset, "%s", body._value.c_str());
		offset += ::snprintf(buffer + offset, 1024 - offset, " , ");
		if (orelse._type != inferred)
			offset += ::snprintf(buffer + offset, 1024 - offset, "hybpython::cast <%s,%s>::from (%s)", inferred->_name.c_str(), orelse._type->_name.c_str(), orelse._value.c_str());
		else
			offset += ::snprintf(buffer + offset, 1024 - offset, "%s", orelse._value.c_str());
		offset += ::snprintf(buffer + offset, 1024 - offset, ")");

		return TypedExpression::Build(inferred, buffer);
	}
}

template <> TypedExpression Hybridizer::Express < _expr_kind::Compare_kind >(expr_ty expr)
{
	char buffer[1024], buffer2[256];

	expr_ty left = expr->v.Compare.left;
	asdl_int_seq* ops = expr->v.Compare.ops;
	asdl_seq* comparators = expr->v.Compare.comparators;

	if (ops->size != comparators->size)
		ThrowNotImplemented();

	char* startB = buffer;
	char* endB = buffer + 1024;

	startB += ::snprintf(startB, endB - startB, "hybpython::compare_op<");

	for (int k = 0; k < ops->size; ++k)
	{
		if (k != 0)
			startB += ::snprintf(startB, endB - startB, " , ");
		startB += ::snprintf(startB, endB - startB, "%s", ExpressCompareOpFunctor((cmpop_ty)((*ops).elements[k])).c_str());
	}

	char* startB2 = buffer2;
	char* endB2 = buffer2 + 256;
	startB2 += ::snprintf(startB2, endB2 - startB2, "<%s", Express(left)._type->_name.c_str());
	for (int k = 0; k < ops->size; ++k)
	{
		startB2 += ::snprintf(startB2, endB2 - startB2, " , %s", Express((expr_ty)(comparators->elements[k]))._type->_name.c_str());
	}
	startB2 += ::snprintf(startB2, endB2 - startB2, ">");

	startB += ::snprintf(startB, endB - startB, "%s", buffer2);

	startB += ::snprintf(startB, endB - startB, ">::compare %s ( %s", buffer2, Express(left)._value.c_str());
	for (int k = 0; k < ops->size; ++k)
	{
		startB += ::snprintf(startB, endB - startB, " , %s", Express((expr_ty)(comparators->elements[k]))._value.c_str());
	}
	startB += ::snprintf(startB, endB - startB, " )");

	return TypedExpression::Build(HybType::builtin<bool>(), buffer);
}

HybType* Hybridizer::GetElementType(HybType* type)
{
	if (type == HybType::getthing()) return HybType::getthing();
	if (type == HybType::forge("list")) return HybType::getthing();
	if (type == HybType::forge("tuple")) return HybType::getthing();
	ThrowNotImplemented();
}

template <> TypedExpression Hybridizer::Express < _expr_kind::Subscript_kind >(expr_ty expr)
{
	char buffer[1024];

	slice_ty slice = expr->v.Subscript.slice;

	switch (slice->kind)
	{
	case _slice_kind::Index_kind:
	{
		TypedExpression index = Express(slice->v.Index.value);
		if (index._type->IsScalar())
		{
			TypedExpression value = Express(expr->v.Subscript.value);
			const char* indexType = _indexerTypeCallback(_handle, value._type->_name.c_str(), index._type->_name.c_str());
			::snprintf(buffer, 1024, "hybpython::index_helper<%s, %s, %s>::get(%s, %s)",
				value._type->_name.c_str(), index._type->_name.c_str(), indexType, 
				value._value.c_str(), index._value.c_str());
			return TypedExpression::Build(HybType::forge(indexType), buffer);
		}
		else if (index._type->IsString())
		{
			TypedExpression value = Express(expr->v.Subscript.value);
			std::string attrname = index._value;
			attrname = attrname.substr(1, attrname.length() - 2);
			HybType* attrtype = GetAttributeType(value._type, attrname);
			::snprintf(buffer, 1024, "%s . get_attribute < %s > ( \"%s\" )", value._value.c_str(), attrtype->_name.c_str(), attrname.c_str());
			return TypedExpression::Build(attrtype, buffer);
		}
		else
			ThrowNotImplemented();
	}
	default:
		ThrowNotImplemented();
	}
}


/*
typedef enum _operator { Add=1, Sub=2, Mult=3, MatMult=4, Div=5, Mod=6, Pow=7,
						 LShift=8, RShift=9, BitOr=10, BitXor=11, BitAnd=12,
						 FloorDiv=13 } operator_ty;
*/
/*
 Add = 1,
		Sub = 2,
		Mult = 3,
		Div = 4,
		Mod = 5,
		Pow = 6,
		LShift = 7,
		RShift = 8,
		BitOr = 9,
		BitXor = 10,
		BitAnd = 11,
		FloorDiv = 12
*/

#ifdef PYTHON_27
static const char* S_BINARY_OPERATOR_FUNCTORS[] = {
		0,
		"hybpython::binaryops::add",
		"hybpython::binaryops::sub",
		"hybpython::binaryops::mul",
		"hybpython::binaryops::div",
		"hybpython::binaryops::mod",
		"hybpython::binaryops::pow",
		"hybpython::binaryops::lshift",
		"hybpython::binaryops::rshift",
		"hybpython::binaryops::bitOr",
		"hybpython::binaryops::bitXor",
		"hybpython::binaryops::bitAnd",
		"hybpython::binaryops::floordiv",
};


// TODO : verify this !!!
static const char* S_ASSIGN_OPERATOR_FUNCTORS[] = {
		0,
		"hybpython::assignops::add",
		"hybpython::assignops::sub",
		"hybpython::assignops::mul",
		"hybpython::assignops::div",
		"hybpython::assignops::mod",
		"hybpython::assignops::pow",
		"hybpython::assignops::lshift",
		"hybpython::assignops::rshift",
		"hybpython::assignops::bitOr",
		"hybpython::assignops::bitXor",
		"hybpython::assignops::bitAnd",
		"hybpython::assignops::floordiv",
};
#else

static const char* S_BINARY_OPERATOR_FUNCTORS[] = {
		0,
		"hybpython::binaryops::add",
		"hybpython::binaryops::sub",
		"hybpython::binaryops::mul",
		0,
		"hybpython::binaryops::div",
		"hybpython::binaryops::mod",
		"hybpython::binaryops::pow",
		"hybpython::binaryops::lshift",
		"hybpython::binaryops::rshift",
		"hybpython::binaryops::bitOr",
		"hybpython::binaryops::bitXor",
		"hybpython::binaryops::bitAnd",
		"hybpython::binaryops::floordiv",
};


// TODO : verify this !!!
static const char* S_ASSIGN_OPERATOR_FUNCTORS[] = {
		0,
		"hybpython::assignops::add",
		"hybpython::assignops::sub",
		"hybpython::assignops::mul",
		0,
		"hybpython::assignops::div",
		"hybpython::assignops::mod",
		"hybpython::assignops::pow",
		"hybpython::assignops::lshift",
		"hybpython::assignops::rshift",
		"hybpython::assignops::bitOr",
		"hybpython::assignops::bitXor",
		"hybpython::assignops::bitAnd",
		"hybpython::assignops::floordiv",
};
#endif

template <> TypedExpression Hybridizer::Express < _expr_kind::BinOp_kind >(expr_ty expr)
{
	char buffer[1024];

	TypedExpression left = Express(expr->v.BinOp.left);
	TypedExpression right = Express(expr->v.BinOp.right);

	HybType* inferred = InferType((InferrenceKind)((int)(InferrenceKind::OPERATOR_TY_OFFSET) + expr->v.BinOp.op), left._type, right._type);

	if (S_BINARY_OPERATOR_FUNCTORS[expr->v.BinOp.op] == 0)
		ThrowNotImplemented();

	std::string st = "<" + left._type->_name + "," + right._type->_name + "," + inferred->_name + ">" ;

	::snprintf(buffer, 1024, "hybpython::binary_op <%s %s>::eval <> ( %s , %s )", S_BINARY_OPERATOR_FUNCTORS[expr->v.BinOp.op], st.c_str(), left._value.c_str(), right._value.c_str());
	return TypedExpression::Build(inferred, buffer);
}


template <> TypedExpression Hybridizer::Express < _expr_kind::UnaryOp_kind >(expr_ty expr)
{
	expr_ty operand = expr->v.UnaryOp.operand;
	unaryop_ty op = expr->v.UnaryOp.op;
	switch (op) {
		case unaryop_ty::UAdd:
			return Express(operand);
		case unaryop_ty::USub:
			{
				TypedExpression arg = Express(operand);
				char buffer[1024];
				::snprintf(buffer, 1024, "hybpython::unary_op < hybpython::unaryops::minus < %s > >::eval <> ( %s )", arg._type->_name.c_str(), arg._value.c_str());
				return TypedExpression::Build(arg._type, buffer);
			}
		case unaryop_ty::Invert:
			{
				TypedExpression arg = Express(operand);
				char buffer[1024];
				::snprintf(buffer, 1024, "hybpython::unary_op < hybpython::unaryops::invert < %s > >::eval <> ( %s )", arg._type->_name.c_str(), arg._value.c_str());
				return TypedExpression::Build(arg._type, buffer);
			}
		case unaryop_ty::Not:
			{
				TypedExpression arg = Express(operand);
				char buffer[1024];
				::snprintf(buffer, 1024, "hybpython::unary_op < hybpython::unaryops::not_op < %s > >::eval <> ( %s )", arg._type->_name.c_str(), arg._value.c_str());
				return TypedExpression::Build(HybType::builtin<bool>(), buffer);
			}
		default:
			ThrowNotImplemented();
	}
}

TypedExpression Hybridizer::Express(expr_ty expr)
{
	switch (expr->kind)
	{
	case _expr_kind::Num_kind:			return Express< _expr_kind::Num_kind       >(expr);
	case _expr_kind::Name_kind:			return Express< _expr_kind::Name_kind      >(expr);
	case _expr_kind::BoolOp_kind:		return Express< _expr_kind::BoolOp_kind    >(expr);
	case _expr_kind::Str_kind:			return Express< _expr_kind::Str_kind       >(expr);
	case _expr_kind::Call_kind:			return Express< _expr_kind::Call_kind      >(expr);
	case _expr_kind::Attribute_kind:	return Express< _expr_kind::Attribute_kind >(expr);
	case _expr_kind::IfExp_kind:		return Express< _expr_kind::IfExp_kind     >(expr);
	case _expr_kind::Compare_kind:		return Express< _expr_kind::Compare_kind   >(expr);
	case _expr_kind::Subscript_kind:	return Express< _expr_kind::Subscript_kind >(expr);
	case _expr_kind::BinOp_kind:		return Express< _expr_kind::BinOp_kind     >(expr);
	case _expr_kind::UnaryOp_kind:		return Express< _expr_kind::UnaryOp_kind   >(expr);
#ifndef PYTHON_27
	case _expr_kind::NameConstant_kind:	return Express< _expr_kind::NameConstant_kind     >(expr);
#endif
	default:
		// TODO : throw !!
		ThrowNotImplemented();
	}
}

TypedExpression Hybridizer::Express(slice_ty slice)
{
	switch (slice->kind)
	{
		case _slice_kind::Index_kind:
			return Express(slice->v.Index.value);
		default:
			ThrowNotImplemented();
	}
}

HybridPythonResult Hybridizer::ExpressForIterator(expr_ty target, expr_ty iter)
{
	static char buffer[1024];

	if (target->kind != _expr_kind::Name_kind)
	{
		// ERROR_STACK.push("Not Implemented : expecting a name kind in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	std::string itername = GetNameAsString(target) ;

	if (iter->kind != _expr_kind::Call_kind)
	{
		// ERROR_STACK.push("Not Implemented : expecting a call kind in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	if ((iter->v.Call.keywords != nullptr) && (iter->v.Call.keywords->size != 0))
	{
		// ERROR_STACK.push("Not Implemented : expecting no keyword in iter call in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	if (iter->v.Call.func->kind != _expr_kind::Name_kind)
	{
		// ERROR_STACK.push("Not Implemented : expecting a named function in iter call in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}

	asdl_seq * args = iter->v.Call.args;
	if (args == nullptr)
	{
		// ERROR_STACK.push("Not Implemented : expecting non null arg in iter call in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}

	// iterator type is pylong unless already defined...
	HybType* iterType = nullptr ;

	if (GetNameAsString(iter->v.Call.func) == "range")
	{
		iterType = HybType::builtin<int64_t>(); // as for now...
	}
	else
	{
		// ERROR_STACK.push("Not Implemented : expecting <range> function in iter call in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}

	if (args == nullptr)
	{
		// ERROR_STACK.push("Not Implemented : expecting non null arg in iter call in " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
	
	const char* itname = itername.c_str();
		
	if (args->size == 1)
	{
		TypedExpression size = Express((expr_ty)(iter->v.Call.args->elements[0]));
		::snprintf(buffer, 1024, "for ( %s  %s = hybpython::cast <%s,hybpython::pylong>::from(0) ; %s < hybpython::cast <%s,%s>::from ( %s ) ; ++ %s )", iterType->_name.c_str(), itname, iterType->_name.c_str(), itname, iterType->_name.c_str(), size._type->_name.c_str(), size._value.c_str(), itname);

		DefineSymbol(itname, iterType);
		
		_kernelsPrinterCallback(_handle, buffer);

		return HybridPythonResult::SUCCESS;
	}
	else if (args->size == 2) {
		TypedExpression start = Express((expr_ty)(iter->v.Call.args->elements[0]));
		TypedExpression upper = Express((expr_ty)(iter->v.Call.args->elements[1]));

		//HybType* 
			//iterType = InferType(InferrenceKind::ENCLOSING, start._type, upper._type);

		DefineSymbol(itname, iterType);

		int offset = ::snprintf(buffer, 1024, "for ( %s %s = ", iterType->_name.c_str(), itname);
		if (start._type != iterType)
			offset += ::snprintf(buffer + offset, 1024 - offset, "hybpython::cast <%s,%s>::from (%s)", iterType->_name.c_str(), start._type->_name.c_str(), start._value.c_str());
		else
			offset += ::snprintf(buffer + offset, 1024 - offset, "%s", start._value.c_str());
		offset += ::snprintf(buffer + offset, 1024 - offset, " ; %s < ", itname);
		if (upper._type != iterType)
			offset += ::snprintf(buffer + offset, 1024 - offset, "hybpython::cast <%s,%s>::from (%s)", iterType->_name.c_str(), upper._type->_name.c_str(), upper._value.c_str());
		else
			offset += ::snprintf(buffer + offset, 1024 - offset, "(%s)", upper._value.c_str());
		offset += ::snprintf(buffer + offset, 1024 - offset, " ; ++ %s ", itname);
		offset += ::snprintf(buffer + offset, 1024 - offset, " ) ");

		_kernelsPrinterCallback(_handle, buffer);

		return HybridPythonResult::SUCCESS;
	}
	else if (args->size == 3) {

		const char* itname = itername.c_str();
		TypedExpression start = Express((expr_ty)(iter->v.Call.args->elements[0]));
		TypedExpression upper = Express((expr_ty)(iter->v.Call.args->elements[1]));
		TypedExpression step = Express((expr_ty)(iter->v.Call.args->elements[2]));

		HybType* iterType = InferType(InferrenceKind::ENCLOSING, start._type, upper._type);
		iterType = InferType(InferrenceKind::ENCLOSING, iterType, step._type);

		DefineSymbol(itname, iterType);

		int offset = ::snprintf(buffer, 1024, "for ( %s %s = ", iterType->_name.c_str(), itname);
		if (start._type != iterType)
			offset += ::snprintf(buffer + offset, 1024 - offset, "hybpython::cast <%s,%s>::from (%s)", iterType->_name.c_str(), start._type->_name.c_str(), start._value.c_str());
		else
			offset += ::snprintf(buffer + offset, 1024 - offset, "%s", start._value.c_str());
		offset += ::snprintf(buffer + offset, 1024 - offset, " ; %s < ", itname);
		if (upper._type != iterType)
			offset += ::snprintf(buffer + offset, 1024 - offset, "hybpython::cast <%s,%s>::from (%s)", iterType->_name.c_str(), upper._type->_name.c_str(), upper._value.c_str());
		else
			offset += ::snprintf(buffer + offset, 1024 - offset, "(%s)", upper._value.c_str());
		offset += ::snprintf(buffer + offset, 1024 - offset, " ; %s += ", itname);
		if (step._type != iterType)
			offset += ::snprintf(buffer + offset, 1024 - offset, "hybpython::cast <%s,%s>::from (%s)", iterType->_name.c_str(), step._type->_name.c_str(), step._value.c_str());
		else
			offset += ::snprintf(buffer + offset, 1024 - offset, "(%s)", step._value.c_str());
		offset += ::snprintf(buffer + offset, 1024 - offset, " ) ");

		_kernelsPrinterCallback(_handle, buffer);

		return HybridPythonResult::SUCCESS;
	}
	else {
		// ERROR_STACK.push("Not Implemented : more that 3 parameters in range " __FUNCTION__);
		throw HybridPythonResult::NOT_IMPLEMENTED;
	}
}

HybridPythonResult Hybridizer::Express(stmt_ty statement, LinePrinterCallback cb)
{
	char buffer[1024];
	try
	{
		switch (statement->kind)
		{
		case _stmt_kind::Return_kind:
			{
				auto value = statement->v.Return.value;
				if (value != nullptr) {
					TypedExpression te = Express(statement->v.Return.value);
#ifndef PYTHON_27
					::snprintf(buffer, 1024, "return hybpython::cast <return_t,%s>::from (%s) ;", te._type->_name.c_str(), te._value.c_str());
#else
					::snprintf(buffer, 1024, "return %s;", te._value.c_str());
#endif
					if (_returnStatementCallback != nullptr)
						_returnStatementCallback(_handle, te._type->_name.c_str());
				}
				else {
					::snprintf(buffer, 1024, "return;");
				}

				cb(_handle, buffer);
				
				// NEED TO STORE THE RETURN TYPE !
				// auto fdef = statement->v.FunctionDef;
				// _defineFunctionReturnCallback(_handle, te._type->_name.c_str(), te._type);
			}
			return HybridPythonResult::SUCCESS;
		case _stmt_kind::Expr_kind:
			{
				TypedExpression te = Express(statement->v.Expr.value);

				::snprintf(buffer, 1024, "%s ;", te._value.c_str());

				cb(_handle, buffer);
			}
			return HybridPythonResult::SUCCESS;
		case _stmt_kind::Assign_kind:
			{
				asdl_seq* targets = statement->v.Assign.targets;
				if (targets->size != 1)
					// multiple assigns not supported yet !!
					ThrowNotImplemented();

				expr_ty target0 = (expr_ty)(targets->elements[0]);

				switch (target0->kind)
				{
				case _expr_kind::Name_kind:
				{
					// don't use Express(target0) because it would search for symbol type => may fail if variable declaration
					std::string assignTarget;

					if (target0->kind == _expr_kind::Name_kind)
						assignTarget = UNICODE_TO_STRING(target0->v.Name.id);
					else
						assignTarget = Express(target0)._value;

					TypedExpression rht = Express(statement->v.Assign.value);
					bool found = IsDefined(assignTarget);
					// get type of symbol
					if (!SymbolKnown(assignTarget))
					{
						// define symbol
						DefineSymbol(assignTarget, rht._type);
						if (!found)
							::snprintf(buffer, 1024, "%s %s = %s;", rht._type->_name.c_str(), assignTarget.c_str(), rht._value.c_str());
						else
							::snprintf(buffer, 1024, "%s = %s;", assignTarget.c_str(), rht._value.c_str());
						cb(_handle, buffer);
					}
					else {
						::snprintf(buffer, 1024, "%s = %s;", assignTarget.c_str(), rht._value.c_str());
						cb(_handle, buffer);
					}

					return HybridPythonResult::SUCCESS;
				}
				case _expr_kind::Attribute_kind:
				{
					TypedExpression value = Express(target0->v.Attribute.value);
					std::string attribute = UNICODE_TO_STRING(target0->v.Attribute.attr);
					TypedExpression rht = Express(statement->v.Assign.value);

					HybType* attributeSetterType = GetAttributeSetterType(value._type, attribute, rht._type);

					::snprintf(buffer, 1024, "%s.set_attribute<%s>(\"%s\", %s);", value._value.c_str(), attributeSetterType->_name.c_str(), attribute.c_str(), rht._value.c_str());
					cb(_handle, buffer);

					return HybridPythonResult::SUCCESS;
				}
				case _expr_kind::Subscript_kind:
				{
					TypedExpression ar = Express(target0->v.Subscript.value);
					TypedExpression slice = Express(target0->v.Subscript.slice);
					TypedExpression rht = Express(statement->v.Assign.value);

					::snprintf(buffer, 1024, "hybpython::index_helper<%s, %s, %s>::set(%s, %s, %s);",
						ar._type->_name.c_str(), slice._type->_name.c_str(), rht._type->_name.c_str(),
						ar._value.c_str(), slice._value.c_str(), rht._value.c_str());
					cb(_handle, buffer);

					return HybridPythonResult::SUCCESS;
				}
				default:
					ThrowNotImplemented();
				}
			}
		case _stmt_kind::AugAssign_kind:
			{
				expr_ty target = statement->v.AugAssign.target;
				operator_ty op = statement->v.AugAssign.op;
				expr_ty value = statement->v.AugAssign.value;

				switch (target->kind)
				{
					case _expr_kind::Attribute_kind:
					{
						TypedExpression rht = Express(value);
						TypedExpression lht = Express(target);

						std::string st_op = S_BINARY_OPERATOR_FUNCTORS[op];
						
						TypedExpression value = Express(target->v.Attribute.value);
						std::string attribute = UNICODE_TO_STRING(target->v.Attribute.attr);

						HybType* attributeSetterType = GetAttributeSetterType(value._type, attribute, rht._type);

						HybType* addType = InferType((InferrenceKind)((int)(InferrenceKind::OPERATOR_TY_OFFSET) + op), lht._type, rht._type);
						std::string op_template = "<" + lht._type->_name + ", " + rht._type->_name + ", " + addType->_name + ">";
						::snprintf(buffer, 1024, "%s.set_attribute<%s>(\"%s\", hybpython::binary_op <%s%s>::eval <> (%s , %s));", value._value.c_str(), attributeSetterType->_name.c_str(), attribute.c_str(), st_op.c_str(), op_template.c_str(), lht._value.c_str(), rht._value.c_str());
						cb(_handle, buffer);
						return HybridPythonResult::SUCCESS;
					}
					case _expr_kind::Name_kind:
					{
						TypedExpression rht = Express(value);
						TypedExpression lht = Express(target);
						std::string st_op = S_ASSIGN_OPERATOR_FUNCTORS[op];
						st_op += "<" + lht._type->_name + ", " + rht._type->_name + ">";
						::snprintf(buffer, 1024, "hybpython::assign_op<%s>::eval<%s, %s>(%s, %s);", st_op.c_str(), lht._type->_name.c_str(), rht._type->_name.c_str(), lht._value.c_str(), rht._value.c_str());
						cb(_handle, buffer);
						return HybridPythonResult::SUCCESS;
					}
					case _expr_kind::Subscript_kind:
					{
						TypedExpression ar = Express(target->v.Subscript.value);
						TypedExpression slice = Express(target->v.Subscript.slice);
						TypedExpression rht = Express(statement->v.AugAssign.value);

						::snprintf(buffer, 1024, "hybpython::index_helper<%s, %s, %s>::get( %s , %s )",
							ar._type->_name.c_str(), slice._type->_name.c_str(), rht._type->_name.c_str(),
							ar._value.c_str(), slice._value.c_str());

						TypedExpression getindex = TypedExpression::Build(GetElementType(ar._type), buffer);

						HybType* inferred = InferType((InferrenceKind)((int)(InferrenceKind::OPERATOR_TY_OFFSET) + op), getindex._type, rht._type);

						if (S_BINARY_OPERATOR_FUNCTORS[op] == 0)
							ThrowNotImplemented();

						std::string st = "<" + getindex._type->_name + "," + rht._type->_name + "," + inferred->_name + ">";

						::snprintf(buffer, 1024, "hybpython::index_helper < %s , %s , %s >::set ( %s , %s , hybpython::binary_op <%s %s>::eval <> (%s , %s) ) ;",
							ar._type->_name.c_str(), slice._type->_name.c_str(), rht._type->_name.c_str(),
							ar._value.c_str(), slice._value.c_str(),

							S_BINARY_OPERATOR_FUNCTORS[op], st.c_str(),
							getindex._value.c_str(), rht._value.c_str());
						cb(_handle, buffer);

						return HybridPythonResult::SUCCESS;
					}
					default:
						ThrowNotImplemented();
				}
			}
		case _stmt_kind::If_kind:
			{
				expr_ty test = statement->v.If.test;
				asdl_seq* body = statement->v.If.body;
                asdl_seq* orelse = statement->v.If.orelse;
				//asdl_seq* orelse = statement->v.If.orelse; // TODO: else if
				::snprintf(buffer, 1024, "if (%s) {", Express(test)._value.c_str());
				cb(_handle, buffer);
				for (int k = 0; k < body->size; ++k) {
					Express((stmt_ty)body->elements[k], cb);
				}
				cb(_handle, "}");
				if (orelse != nullptr) {
					cb(_handle, "else {");
					for (int k = 0; k < orelse->size; ++k) {
						Express((stmt_ty)orelse->elements[k], cb);
					}
					cb(_handle, "}");
				}
				return HybridPythonResult::SUCCESS;
			}
		case _stmt_kind::For_kind:
			{
				expr_ty target = statement->v.For.target;
				expr_ty iter = statement->v.For.iter;

				std::string itername = GetNameAsString(target);
				ExpressForIterator(target, iter);
				cb(_handle, "{");

				for (int k = 0; k < statement->v.For.body->size; ++k)
				{
					Express((stmt_ty)(statement->v.For.body->elements[k]), cb);
				}
				_kernelsPrinterCallback(_handle, "}");
			}
			return HybridPythonResult::SUCCESS;

		case _stmt_kind::While_kind:
			{
				expr_ty test = statement->v.While.test;
				asdl_seq* body = statement->v.While.body;
				asdl_seq* orelse = statement->v.While.orelse;
				if (orelse != nullptr)
					return HybridPythonResult::NOT_IMPLEMENTED;

				TypedExpression ttest = Express(test);
				::snprintf(buffer, 1024, "while (%s) {", ttest._value.c_str());
				cb(_handle, buffer);

				for (int k = 0; k < body->size; ++k)
				{
					Express((stmt_ty)(body->elements[k]), cb);
				}
				cb(_handle, "}");
			}
			return HybridPythonResult::SUCCESS;
		case _stmt_kind::FunctionDef_kind:
			{
				auto functionDef = statement->v.FunctionDef;
				std::string name = GetPyObjectAsString(functionDef.name);

				asdl_seq* args = functionDef.args->args;
				asdl_seq* body = functionDef.body;
#ifndef PYTHON_27
				expr_ty returns = functionDef.returns;
				if (returns == nullptr) {
					throw HybridPythonResult::MISSING_RETURN_TYPE_ANNOTATION;
				}
				std::string returnTypeName = GetNameAsString(returns);
#else
				std::string returnTypeName = HybType::getthing()->_name;
#endif

				EnterFunctionScope();

				int argcount = args == nullptr ? 0 : (int)args->size;
				std::vector<const char*> argTypes;
				std::vector<std::string> argNames;
				for (int k = 0; k < argcount; ++k)
				{
#ifndef PYTHON_27
					arg_ty arg = (arg_ty)(args->elements[k]);
					if (arg->annotation != nullptr) {
						if (arg->annotation->kind != _expr_kind::Name_kind)
							throw HybridPythonResult::NOT_IMPLEMENTED;
					}
					std::string annotation = UNICODE_TO_STRING(arg->annotation->v.Name.id);
					HybType* argType = HybType::forge(annotation);
					argTypes.push_back(argType->_name.c_str());
					std::string argName = UNICODE_TO_STRING(arg->arg);
#else
					HybType* argType = HybType::getthing();
					std::string argName = UNICODE_TO_STRING(((expr_ty)args->elements[k])->v.Name.id);
#endif
					argNames.push_back(argName);
					DefineSymbol(argName.c_str(), argType);
				}

				int offset = ::snprintf(buffer, 1024, "hyb_device %s %s (", returnTypeName.c_str(), name.c_str());

				for (int k = 0; k < argcount; ++k)
				{
					offset += ::snprintf(buffer + offset, 1024 - offset, "%s %s", argTypes[k], argNames[k].c_str());
					if (k < argcount - 1)
						offset += ::snprintf(buffer + offset, 1024 - offset, ", ");
				}
				offset += ::snprintf(buffer + offset, 1024 - offset, ")");

				::snprintf(buffer + offset, 1024 - offset, ";");
				_funcDeclPrinterCallback(_handle, buffer);
				::snprintf(buffer + offset, 1024 - offset, " { ");
				_funcBodyPrinterCallback(_handle, buffer);

				_defineFunctionReturnCallback(_handle, name.c_str(), argcount, argcount > 0 ? &(argTypes[0]) : nullptr, returnTypeName.c_str());

				for (int k = 0; k < body->size; ++k)
				{
					Express((stmt_ty)body->elements[k], _funcBodyPrinterCallback);
				}

				_funcBodyPrinterCallback(_handle, "}");

				LeaveFunctionScope();
			}
			return HybridPythonResult::SUCCESS;
		case _stmt_kind::Assert_kind:
			{
				expr_ty test = statement->v.Assert.test;
				TypedExpression te = Express(statement->v.Assert.test);
				::snprintf(buffer, 1024, "if (! ( %s ) ) hybpython::TRAP();", te._value.c_str());
				cb(_handle, buffer);
				return HybridPythonResult::SUCCESS;
			}
		case _stmt_kind::Pass_kind:
			// pass is no op... 
			return HybridPythonResult::SUCCESS;
		default:
			ThrowNotImplemented();
		}
	}
	catch (HybridPythonResult hr)
	{
		throw hr;
	}
}

#pragma endregion

#ifndef PYTHON_27
#pragma region Disassembler

Disassembler::Disassembler(PyObject* code)
{
	PyObject* co_code = PyObject_GetAttrString(code, "co_code"); // bytes
	PyObject* co_consts = PyObject_GetAttrString(code, "co_consts"); // tuple
	PyObject* co_names = PyObject_GetAttrString(code, "co_names"); // tuple
	PyObject* co_argcount = PyObject_GetAttrString(code, "co_argcount"); // int
	PyObject* co_varnames = PyObject_GetAttrString(code, "co_varnames"); // tuple
	PyObject* co_nblocals = PyObject_GetAttrString(code, "co_nlocals"); // int
	PyObject* co_lnotab = PyObject_GetAttrString(code, "co_lnotab"); // bytes
	PyObject* co_stacksize = PyObject_GetAttrString(code, "co_stacksize"); // int
	PyObject* co_firstlineno = PyObject_GetAttrString(code, "co_firstlineno"); // int
	PyObject* co_filename = PyObject_GetAttrString(code, "co_filename"); // str
	PyObject* co_name = PyObject_GetAttrString(code, "co_name"); // str

	_name = UNICODE_TO_STRING(co_name);

	// get the code
	_code = (unsigned char*) PyBytes_AsString(co_code);
	_codelen = (int)PyBytes_Size(co_code);

	// get constants
	for (Py_ssize_t k = 0; k < PyTuple_Size(co_consts); ++k)
	{
		expr_ty res = new _expr(); _exprtobecleaned.push_back(res);
		res->lineno = -1;
		PyObject* cst = PyTuple_GetItem(co_consts, k);
		if (PyLong_Check(cst))
		{
			res->kind = _expr_kind::Num_kind;
			res->v.Num.n = cst;
		}
		else if (PyFloat_Check(cst))
		{
			res->kind = _expr_kind::Num_kind;
			res->v.Num.n = cst;
		}
		else if (PyUnicode_Check(cst))
		{
			res->kind = _expr_kind::Str_kind;
			res->v.Str.s = cst;
		}
		else if (cst == Py_None)
		{
			res->kind = _expr_kind::Constant_kind;
			res->v.Constant.value = cst;
		}
		else
			ThrowNotImplemented();
		_constants.push_back(res);
	}

	// get globals
	for (Py_ssize_t k = 0; k < PyTuple_Size(co_names); ++k)
	{
		_globals.push_back(PyTuple_GetItem(co_names, k));
	}

	// parameter count
	_argcount = _PyLong_AsInt(co_argcount);

	// variable names
	for (Py_ssize_t k = 0; k < PyTuple_Size(co_varnames); ++k)
	{
		_varnames.push_back(PyTuple_GetItem(co_varnames, k));
	}

	// build the closure tuple -- don't use values directly !!
	_closureTuple = new _expr();
	_exprtobecleaned.push_back(_closureTuple);
	_closureTuple->kind = _expr_kind::Name_kind;
	_closureTuple->v.Name.ctx = _expr_context::Param;
	_closureTuple->v.Name.id = PyUnicode_FromStringAndSize("__closure__", ::strlen("__closure__"));

	// nb locals
	_nblocals = _PyLong_AsInt(co_nblocals);

	// SANITY CHECK
	if (_nblocals != _varnames.size())
		throw HybridPythonResult::DISASM_VARCOUNT_MISMATCH;

	// debugging stuff
	_firstlineno = _PyLong_AsInt(co_firstlineno);
	_lnotab = (unsigned char*)PyBytes_AsString(co_lnotab);
	_filename = UNICODE_TO_STRING(co_filename);
}

unaryop_ty GetUnaryOperation(Disassembler::OpCodes code)
{
	switch (code)
	{
		case Disassembler::OpCodes::UNARY_INVERT: return unaryop_ty::Invert;
		case Disassembler::OpCodes::UNARY_NEGATIVE: return unaryop_ty::USub;
		case Disassembler::OpCodes::UNARY_POSITIVE: return unaryop_ty::UAdd;
		case Disassembler::OpCodes::UNARY_NOT: return unaryop_ty::Not;
		default:
			ThrowNotImplemented();
	}
}

operator_ty GetBinaryOperation(Disassembler::OpCodes code)
{
	switch (code)
	{
		case Disassembler::OpCodes::BINARY_POWER:		return operator_ty::Pow;
		case Disassembler::OpCodes::BINARY_MULTIPLY:	return operator_ty::Mult;
		case Disassembler::OpCodes::BINARY_MODULO:		return operator_ty::Mod;
		case Disassembler::OpCodes::BINARY_ADD:			return operator_ty::Add;
		case Disassembler::OpCodes::BINARY_SUBTRACT:	return operator_ty::Sub;
		case Disassembler::OpCodes::BINARY_FLOOR_DIVIDE:return operator_ty::FloorDiv;
		case Disassembler::OpCodes::BINARY_TRUE_DIVIDE:	return operator_ty::Div;
		case Disassembler::OpCodes::BINARY_LSHIFT:		return operator_ty::LShift;
		case Disassembler::OpCodes::BINARY_RSHIFT:		return operator_ty::RShift;
		case Disassembler::OpCodes::BINARY_AND:			return operator_ty::BitAnd;
		case Disassembler::OpCodes::BINARY_XOR:			return operator_ty::BitXor;
		case Disassembler::OpCodes::BINARY_OR:			return operator_ty::BitOr;
		case Disassembler::OpCodes::INPLACE_POWER:		return operator_ty::Pow;
		case Disassembler::OpCodes::INPLACE_MULTIPLY:	return operator_ty::Mult;
		case Disassembler::OpCodes::INPLACE_MODULO:		return operator_ty::Mod;
		case Disassembler::OpCodes::INPLACE_ADD:		return operator_ty::Add;
		case Disassembler::OpCodes::INPLACE_SUBTRACT:	return operator_ty::Sub;
		case Disassembler::OpCodes::INPLACE_FLOOR_DIVIDE:return operator_ty::FloorDiv;
		case Disassembler::OpCodes::INPLACE_TRUE_DIVIDE:return operator_ty::Div;
		case Disassembler::OpCodes::INPLACE_LSHIFT:		return operator_ty::LShift;
		case Disassembler::OpCodes::INPLACE_RSHIFT:		return operator_ty::RShift;
		case Disassembler::OpCodes::INPLACE_AND:		return operator_ty::BitAnd;
		case Disassembler::OpCodes::INPLACE_XOR:		return operator_ty::BitXor;
		case Disassembler::OpCodes::INPLACE_OR:			return operator_ty::BitOr;
		default:
			ThrowNotImplemented();
	}
}

asdl_seq* Disassembler::alloc_asdl_seq(int size)
{
	asdl_seq* res = (asdl_seq*)::malloc(sizeof(asdl_seq) + size * sizeof(void*));
	_asdl_seqtobecleaned.push_back(res);
	res->size = size;
	return res;
}

asdl_int_seq* Disassembler::alloc_asdl_int_seq(int size)
{
	asdl_int_seq* res = (asdl_int_seq*)::malloc(sizeof(asdl_int_seq) + size * sizeof(int));
	_asdl_int_seqtobecleaned.push_back(res);
	res->size = size;
	return res;
}

template<>
stmt_ty Disassembler::alloc<stmt_ty>()
{
	stmt_ty res = new _stmt();
	_stmttobecleaned.push_back(res);
	return res;
}

template<>
expr_ty Disassembler::alloc<expr_ty>()
{
	expr_ty res = new _expr();
	_exprtobecleaned.push_back(res);
	return res;
}

template<>
slice_ty Disassembler::alloc<slice_ty>()
{
	slice_ty res = new _slice();
	_slicestobecleaned.push_back(res);
	return res;
}

template<>
arguments_ty Disassembler::alloc<arguments_ty>()
{
	arguments_ty res = new _arguments();
	_argliststobecleaned.push_back(res);
	return res;
}

template<>
arg_ty Disassembler::alloc<arg_ty>()
{
	arg_ty res = new _arg();
	_argstobecleaned.push_back(res);
	return res;
}

/// returns nullptr if statements is empty vector
asdl_seq* Disassembler::BuildASDLSequence(const std::vector<stmt_ty>& statements)
{
	if (statements.size() == 0) return nullptr;
	asdl_seq* res = alloc_asdl_seq((int)statements.size());
	for (int k = 0; k < statements.size(); ++k)
	{
		res->elements[k] = (void*)(statements[k]);
	}
	return res;
}

// empty returns an empty sequence
asdl_seq* Disassembler::BuildASDLSequence(const std::vector<expr_ty>& expressions)
{
	asdl_seq* res = alloc_asdl_seq((int)expressions.size());
	for (int k = 0; k < expressions.size(); ++k)
	{
		res->elements[k] = (void*)(expressions[k]);
	}
	return res;
}


Disassembler::AndCondition GetCommonConditions(const Disassembler::OrCondition& c)
{
	Disassembler::AndCondition res;
	if (c._conds.size() == 0) return res;
	for (int k = 0; k < c._conds[0]._conds.size(); ++k)
	{
		Disassembler::Condition candidate = c._conds[0]._conds[k];
		bool all = true;
		for (int j = 1; j < c._conds.size(); ++j)
		{
			if (!(c._conds[j].has(candidate))) all = false;
		}
		if (all)
			res &= candidate;
	}
	return res;
}

#if 0  // deprecated
enum branchkind
{
	branch_if,		// entering if
	branch_else,	// entering else
	branch_endif,	// leaving if
	branch_endelse,	// leaving else
	branch_multi,	// multiple branches...
};

// group statements so that front block is current condition
void Disassembler::GroupStatements()
{
	if (_blocks.front()._cond == _currentconditions)
		return;

	// identify branch type
	AndCondition commoncurrent = GetCommonConditions(_currentconditions);
	AndCondition commonfront = GetCommonConditions(_blocks.front()._cond);

	if (commoncurrent.issub(commonfront))
	{
		// entering if => add block
		_blocks.push_front(StatementBlock(_currentconditions));
		return;
	} 
	Condition ifcond;
	if (commoncurrent.complements(commonfront, &ifcond))
	{
		// entering else => add block
		_blocks.push_front(StatementBlock(_currentconditions));
		return;
	}

	// try merge two parts of stack
	if (_blocks.size() <= 1)
		throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;

	AndCondition commonnext = GetCommonConditions(_blocks[1]._cond);
	if (commonnext.complements(commonfront, &ifcond))
	{
		// both entries are an ifelse
		if (!(ifcond.direct)) 
			throw HybridPythonResult::NOT_IMPLEMENTED; // TODO
		
		OrCondition ifbranchcond = _blocks[1]._cond;
		OrCondition elsebranchcond = _blocks[0]._cond;
		
		AndCondition branchingcause; branchingcause &= ifcond;
		AndCondition branchingcausebar; branchingcausebar &= ifcond.bar();

		OrCondition basecond = ifbranchcond.knowing(branchingcause);

		// verify real branching..
		if (basecond != elsebranchcond.knowing(branchingcausebar))
			throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;

		// create the ifelse
		stmt_ty ifstmt = alloc<stmt_ty>();
		ifstmt->lineno = -1;
		ifstmt->kind = _stmt_kind::If_kind;
		ifstmt->v.If.test = ifcond.cond;
		ifstmt->v.If.orelse = BuildASDLSequence(_blocks[0]._statements);
		ifstmt->v.If.body = BuildASDLSequence(_blocks[1]._statements);


		// pop two blocks
		_blocks.pop_front();
		_blocks.pop_front();

		// empty ?
		if (ifstmt->v.If.body == nullptr)
		{
			if (ifstmt->v.If.orelse != nullptr)
				throw HybridPythonResult::NOT_IMPLEMENTED; // TODO : revert condition
			else
			{
				// simply poped empty blocks
				GroupStatements();
				return;
			}
		}
		// if front block has proper condition => insert statement
		if (_blocks.front()._cond == basecond)
		{
			_blocks.front()._statements.push_back(ifstmt);
		}
		else {
			// create a block
			StatementBlock sb = StatementBlock(basecond);
			sb._statements.push_back(ifstmt);
			_blocks.push_front(sb);
		}

		// continue grouping
		GroupStatements();
		return;
	}
	if (commonfront.issub(commonnext))
	{
		// simple if, find branching cause
		if (_blocks[0]._statements.size() == 0)
		{
			_blocks.pop_front();
			GroupStatements();
			return;
		}

		AndCondition branchingcause = commonfront.knowing(commonnext);
		if (branchingcause._conds.size() != 1)
			throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;

		OrCondition ifbranchcond = _blocks[0]._cond;
		OrCondition basecond = ifbranchcond.knowing(branchingcause);

		if (_blocks[1]._cond != basecond)
			throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR; // TODO ?

		ifcond = branchingcause._conds[0];
		if (!(ifcond.direct))
			throw HybridPythonResult::NOT_IMPLEMENTED; // TODO

		// create the if
		stmt_ty ifstmt = alloc<stmt_ty>();
		ifstmt->lineno = -1;
		ifstmt->kind = _stmt_kind::If_kind;
		ifstmt->v.If.test = ifcond.cond;
		ifstmt->v.If.body = BuildASDLSequence(_blocks[0]._statements);
		ifstmt->v.If.orelse = nullptr;

		_blocks.pop_front();

		_blocks.front()._statements.push_back(ifstmt);

		// continue grouping
		GroupStatements();
		return;
	}
	
	// current is less restrictive
	throw HybridPythonResult::NOT_IMPLEMENTED;
}

void Disassembler::AddStatement(stmt_ty stmt)
{
	GroupStatements();

	// are we in same block ?
	if (_blocks.front()._cond == _currentconditions)
	{
		_blocks.front()._statements.push_back(stmt);
		return;
	}
}

#endif

void Disassembler::AddBGNode(const BGNode& node)
{
	_branchinggraph[node._index] = node;
}

#pragma region Stack Operations

[[noreturn]] static void ThrowInternalStackError()
{
	throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;
}

Disassembler::StackEntry Disassembler::StackEntry::Merge(Disassembler::StackEntry a, Disassembler::StackEntry b)
{
	Disassembler::StackEntry result = a;
	for (auto kv = b._entry.begin(); kv != b._entry.end(); ++kv)
	{
		auto found = result._entry.find(kv->first);
		if (found == result._entry.end())
			result._entry[kv->first] = kv->second;
		else
			// verify same : same condition, two different stack values ???
			if (found->second != kv->second)
				ThrowInternalStackError();
	}
	return result;
}

Disassembler::ExprStack Disassembler::StackMerge(const Disassembler::ExprStack& predicted, const Disassembler::ExprStack& current)
{
	Disassembler::ExprStack result;
	// both should be same size
	if (current._stack.size() != predicted._stack.size())
		ThrowInternalStackError();
	// process each entry
	auto pter = predicted._stack.begin();
	auto cter = current._stack.begin();
	while (pter != predicted._stack.end())
	{
		result._stack.push_back(Disassembler::StackEntry::Merge(*pter, *cter));
		++pter; ++cter;
	}
	return result;
}

void Disassembler::StackPushFront(expr_ty expr)
{
	StackEntry se;
	se._entry[_currentconditions] = expr;
	_stack._stack.push_front(se);
}


template<typename T>
std::pair<Disassembler::Condition, std::pair<T, T>> GetDifferentiator(
	const std::pair<Disassembler::OrCondition, T>& left,
	const std::pair<Disassembler::OrCondition, T>& right)
{
	// get common conditions
	Disassembler::AndCondition commonleft = GetCommonConditions(left.first);
	Disassembler::AndCondition commonright = GetCommonConditions(right.first);
	
	std::vector<Disassembler::Condition> differentiators;
	// find the one that is opposite, and return it
	for (int k = 0; k < commonleft._conds.size(); ++k)
	{
		Disassembler::Condition c = commonleft._conds[k].bar();
		if (commonright.has(c))
			differentiators.push_back(c);
	}
	if (differentiators.size() != 1)
		ThrowInternalStackError();

	return std::pair<Disassembler::Condition, std::pair<T, T>>(differentiators[0],
		std::pair<T, T>(right.second, left.second));
}

#include <set>

expr_ty Disassembler::BuildBoolopFromConds(const AndCondition& conds)
{
	if (conds._conds.size() == 0)
		ThrowInternalStackError();
	if (conds._conds.size() == 1)
		return conds._conds[0].cond;

	int i = 1; 
	bool direct = conds._conds[0].direct;
	for (; i < conds._conds.size() - 1; ++i)
	{
		if (conds._conds[i].direct != direct) break;
	}
	// we have the size of the expression
	expr_ty boolop = alloc<expr_ty>();
	boolop->lineno = _currentline;
	boolop->kind = _expr_kind::BoolOp_kind;
	boolop->v.BoolOp.op = direct ? _boolop::And : _boolop::Or;

	AndCondition remain;
	std::vector<expr_ty> vals ;
	for (int k = 0 ; k < conds._conds.size(); ++k)
	{
		if (k < i)
			vals.push_back(conds._conds[k].cond);
		else
			remain &= conds._conds[k];
	}
	// append last
	vals.push_back(BuildBoolopFromConds(remain));
	boolop->v.BoolOp.values = BuildASDLSequence(vals);

	return boolop;
}

expr_ty Disassembler::BuildIfExprFromStackEntry(const Disassembler::StackEntry& entry, const OrCondition& current)
{
	if (entry._entry.size() == 0)
		ThrowInternalStackError();
	if (entry._entry.size() == 1)
	{
		// TODO: check "_current"
		std::pair<OrCondition, expr_ty> top = *(entry._entry.begin());
		return top.second;
	}
	if (entry._entry.size() == 2)
	{
		// trivial split
		// get the differentiating expression
		std::pair<OrCondition, expr_ty> left = *(entry._entry.begin());
		std::pair<OrCondition, expr_ty> right = *(++(entry._entry.begin()));

		// get differentiator -- first, is true on condition, condition is direct
		// => first has c and second has c_bar in all entries
		std::pair<Condition, std::pair<expr_ty, expr_ty>> differentiator = GetDifferentiator(left, right);
		// TODO: verify remaining conds are _currentcondition ?

		// is condition one of the expressions ?
		if ((differentiator.first.cond == differentiator.second.first) ||
			(differentiator.first.cond == differentiator.second.second))
		{
			// it is a boolean operation -> build a condition list
			// get common condition for multiple entry and/or
			AndCondition commonleft = GetCommonConditions(left.first);
			AndCondition commonright = GetCommonConditions(right.first);
			// remove current
			AndCondition commoncurrent = GetCommonConditions(current);
			commonleft = commonleft.knowing(commoncurrent);
			commonright = commonright.knowing(commoncurrent);

			Condition delta;
			if (!(commonleft.complements(commonright, &delta)))
				ThrowInternalStackError();

			AndCondition condlist = commonleft;

			// append to condlist last condition
			if (commonleft.has(Condition(left.second, true)) || commonleft.has(Condition(left.second, false)))
				{ condlist = commonright; condlist &= Condition(right.second); }
			else if (condlist.has(Condition(right.second, true)) || condlist.has(Condition(right.second, false)))
				{ condlist = commonleft; condlist &= Condition(left.second); }
			else
				ThrowInternalStackError();

			// Generate expression from condlist
			expr_ty boolop = BuildBoolopFromConds(condlist);
			return boolop;
			
			/*
			// => and
			if ((differentiator.first.direct) && (differentiator.first.cond == differentiator.second.second))
			{
				std::vector<expr_ty> values ; 
				values.push_back(differentiator.second.second);
				values.push_back(differentiator.second.first);
				// we have second .and. first
				expr_ty andexpr = alloc<expr_ty>();
				andexpr->lineno = _currentline;
				andexpr->kind = _expr_kind::BoolOp_kind;
				andexpr->v.BoolOp.op = boolop_ty::And;
				andexpr->v.BoolOp.values = BuildASDLSequence(values);

				return andexpr;
			}
			// or
			else if ((differentiator.first.direct) && (differentiator.first.cond == differentiator.second.first))
			{
				std::vector<expr_ty> values;
				values.push_back(differentiator.second.first);
				values.push_back(differentiator.second.second);
				// we have second .and. first
				expr_ty orexpr = alloc<expr_ty>();
				orexpr->lineno = _currentline;
				orexpr->kind = _expr_kind::BoolOp_kind;
				orexpr->v.BoolOp.op = boolop_ty::Or;
				orexpr->v.BoolOp.values = BuildASDLSequence(values);

				return orexpr;
			}
			else
				ThrowNotImplemented();*/
		} else {
			expr_ty ifexpr = alloc<expr_ty>();
			ifexpr->lineno = _currentline;
			ifexpr->kind = _expr_kind::IfExp_kind;
			ifexpr->v.IfExp.test = differentiator.first.cond;
			ifexpr->v.IfExp.body = differentiator.first.direct ? differentiator.second.first : differentiator.second.second;
			ifexpr->v.IfExp.orelse = differentiator.first.direct ? differentiator.second.second : differentiator.second.first;

			return ifexpr;
		}
	}
	int size = (int) entry._entry.size();
	// process in vectors for perf reasons
	std::vector<OrCondition> keys;
	std::vector<expr_ty> values;
	for (auto iter = entry._entry.begin(); iter != entry._entry.end(); ++iter)
	{
		keys.push_back(iter->first);
		values.push_back(iter->second);
	}
	// foreach entry of stack, get common condition
	std::vector<AndCondition> commons;
	for (int k = 0; k < size; ++k)
	{
		commons.push_back(GetCommonConditions(keys[k]));
	}
	// get all the conditions in one set
	std::set<Condition> allconds;
	for (int k = 0; k < size; ++k)
	{
		for (int j = 0; j < commons[k]._conds.size(); ++j)
		{
			allconds.insert(commons[k]._conds[j]);
		}
	}
	// finally, for each cond, count C and Cbar
	// if sum is size => split expression
	for (auto iter = allconds.begin(); iter != allconds.end(); ++iter)
	{
		int c_count = 0;
		int cbar_count = 0;
		for (int k = 0; k < size; ++k)
		{
			if (commons[k].has(*iter)) ++c_count;
			if (commons[k].has(iter->bar())) ++cbar_count;
		}
		if ((c_count + cbar_count) == size)
		{
			Condition differentiator = *iter;
			// use the direct one
			differentiator.direct = true;

			// build two stack entries
			Disassembler::StackEntry c_entry;
			Disassembler::StackEntry cbar_entry;
			for (int k = 0; k < size; ++k)
			{
				if (commons[k].has(differentiator))
					c_entry._entry[keys[k]] = values[k];
				else if (commons[k].has(differentiator.bar()))
					cbar_entry._entry[keys[k]] = values[k];
				else
					ThrowInternalStackError();
			}
			OrCondition c_current = current, cbar_current = current;
			c_current &= differentiator;
			cbar_current &= differentiator.bar();

			// express both sides
			expr_ty c_expr = BuildIfExprFromStackEntry(c_entry, c_current);
			expr_ty cbar_expr = BuildIfExprFromStackEntry(cbar_entry, cbar_current);

			// build the if expression
			expr_ty ifexpr = alloc<expr_ty>();
			ifexpr->lineno = _currentline;
			ifexpr->kind = _expr_kind::IfExp_kind;
			ifexpr->v.IfExp.test = differentiator.cond;
			ifexpr->v.IfExp.body = c_expr ;
			ifexpr->v.IfExp.orelse = cbar_expr ;

			return ifexpr;
		}
	}
	// FAILURE IF HERE => could not find any differentiator
	ThrowInternalStackError();
}

expr_ty Disassembler::StackPopFront()
{
	Disassembler::StackEntry entry = _stack._stack.front();
	_stack._stack.pop_front();

	expr_ty res = BuildIfExprFromStackEntry(entry, _currentconditions);

	if (res->kind == _expr_kind::IfExp_kind)
	{
		// unwind BG up to test index
		int testbytecodeloc = GetBytecodeLocation(res->v.IfExp.test);
		if (testbytecodeloc < 0)
			ThrowInternalStackError();
		int todelete = (int)_branchinggraph.rbegin()->first ; 
		while (todelete >= testbytecodeloc)
		{
			unsigned prev = (--(_branchinggraph.find(todelete)))->first;
			_branchinggraph.erase(todelete);
			todelete = prev;
		}
	}
	else if (res->kind == _expr_kind::BoolOp_kind)
	{
		// unwind BG up to cond index
		int testbytecodeloc = _currentline;
		for (int k = 0; k < res->v.BoolOp.values->size; ++k)
		{
			int loc = GetBytecodeLocation((expr_ty)(res->v.BoolOp.values->elements[k]));
			if (loc < testbytecodeloc) testbytecodeloc = loc;
		}
		int todelete = (int)_branchinggraph.rbegin()->first;
		while (todelete >= testbytecodeloc)
		{
			unsigned prev = (--(_branchinggraph.find(todelete)))->first;
			_branchinggraph.erase(todelete);
			todelete = prev;
		}
	}

	return res;
}

#pragma endregion

bool IsLoopStatement(_stmt_kind v)
{
	if (v == _stmt_kind::For_kind) return true;
	if (v == _stmt_kind::While_kind) return true;
	return false;
}

void Disassembler::AppendCode(unsigned char* codes, int& index)
{
	BGNode node;
	node._index = index;
	node._cond = _currentconditions;
	node._sourceline = _currentline;

	_stackforward = true; // default behavior, unless we have unconditional jump...
	int currentindex = index;
	// https://docs.python.org/3.6/library/dis.html
	Disassembler::OpCodes op = (Disassembler::OpCodes)(codes[index++]);

	unsigned int oparg = codes[index++];

	switch (op)
	{

		#pragma region LOAD/STORE

		case Disassembler::OpCodes::LOAD_DEREF:
			{
				unsigned offset = oparg;
				// we load subscript of the '__closure__' tuple
				expr_ty expr = alloc<expr_ty>();
				expr->lineno = _currentline;
				expr->kind = _expr_kind::Subscript_kind;
				expr->v.Subscript.ctx = expr_context_ty::Load;
				expr->v.Subscript.slice = alloc<slice_ty>();
				expr->v.Subscript.slice->kind = _slice_kind::Index_kind;
				expr_ty slice = alloc<expr_ty>();
				slice->lineno = _currentline;
				slice->kind = _expr_kind::Num_kind;
				slice->v.Num.n = PyLong_FromLong(offset);
				expr->v.Subscript.slice->v.Index.value = slice;
				expr->v.Subscript.value = _closureTuple;
				StackPushFront(expr) ;
				return;
			}

		case Disassembler::OpCodes::LOAD_FAST:
			{
				unsigned offset = oparg;
				expr_ty expr = alloc<expr_ty>();
				expr->lineno = _currentline;
				expr->kind = _expr_kind::Name_kind;
				expr->v.Name.ctx = expr_context_ty::Load;
				expr->v.Name.id = _varnames[offset];
				_expressionsByteCodeLocations[expr] = node._index;
				StackPushFront(expr);
				return;
			}
		case Disassembler::OpCodes::STORE_FAST:
			{
				unsigned offset = oparg;

				expr_ty target = alloc<expr_ty>();
				target->lineno = _currentline;
				target->kind = _expr_kind::Name_kind;
				target->v.Name.ctx = expr_context_ty::Store;
				target->v.Name.id = _varnames[offset];

				stmt_ty store = nullptr;
				// also handle inplace
				expr_ty value = StackPopFront();
				if (IsInplace(value))
				{
					if (value->kind != _expr_kind::BinOp_kind)
						ThrowNotImplemented();
					store = alloc<stmt_ty>();
					store->lineno = _currentline;
					store->kind = _stmt_kind::AugAssign_kind;
					store->v.AugAssign.op = value->v.BinOp.op;
					store->v.AugAssign.target = target;
					// seems that inspect does not set augstore... target->v.Name.ctx = expr_context_ty::AugStore;
					store->v.AugAssign.value = value->v.BinOp.right;
				} 
				else 
				{

					store = alloc<stmt_ty>();
					store->lineno = _currentline;
					store->kind = _stmt_kind::Assign_kind;
					store->v.Assign.targets = alloc_asdl_seq(1);
					store->v.Assign.targets->elements[0] = target;
					store->v.Assign.value = value;
				}
				node._kind = BGNode_type::block;
				node.Block._statements.push_back(store);

				AddBGNode(node);

				return;
			}
		case Disassembler::OpCodes::LOAD_CONST:
			{
				unsigned offset = oparg;
				StackPushFront(_constants[offset]);
				return;
			}

		case Disassembler::OpCodes::LOAD_GLOBAL:
			{
				unsigned offset = oparg;
				expr_ty expr = alloc<expr_ty>();
				expr->lineno = _currentline;
				expr->kind = _expr_kind::Name_kind;
				expr->v.Name.ctx = expr_context_ty::Load;
				expr->v.Name.id = _globals[offset];
				_expressionsByteCodeLocations[expr] = node._index;
				StackPushFront(expr);
				return;
			}

		case Disassembler::OpCodes::LOAD_ATTR:
			{
				unsigned offset = oparg;
				expr_ty expr = alloc<expr_ty>();
				expr->lineno = _currentline;
				expr->kind = _expr_kind::Attribute_kind;
				expr->v.Attribute.value = StackPopFront();
				expr->v.Attribute.ctx = expr_context_ty::Load;
				expr->v.Attribute.attr = _globals[offset];
				_expressionsByteCodeLocations[expr] = node._index;
				StackPushFront(expr);
				return;
			}

		case Disassembler::OpCodes::STORE_SUBSCR:
			{
				stmt_ty store = alloc<stmt_ty>();
	
				expr_ty slice = StackPopFront();
				expr_ty ar = StackPopFront();
				expr_ty value = StackPopFront();

				expr_ty subscr = alloc<expr_ty>();
				subscr->lineno = _currentline;
				subscr->kind = _expr_kind::Subscript_kind;
				subscr->v.Subscript.ctx = expr_context_ty::Store;
				subscr->v.Subscript.slice = alloc<slice_ty>();
				subscr->v.Subscript.slice->kind = _slice_kind::Index_kind;
				subscr->v.Subscript.slice->v.Index.value = slice;
				subscr->v.Subscript.value = ar;

				if (IsInplace(value))
				{
					if (value->kind != _expr_kind::BinOp_kind)
						ThrowNotImplemented();
					store = alloc<stmt_ty>();
					store->lineno = _currentline;
					store->kind = _stmt_kind::AugAssign_kind;
					store->v.AugAssign.op = value->v.BinOp.op;
					store->v.AugAssign.target = subscr;
					store->v.AugAssign.value = value->v.BinOp.right;
				}
				else
				{
					store->lineno = _currentline;
					store->kind = _stmt_kind::Assign_kind;
					store->v.Assign.value = value;
					store->v.Assign.targets = alloc_asdl_seq(1);
					store->v.Assign.targets->elements[0] = subscr;
				}

				node._kind = BGNode_type::block;
				node.Block._statements.push_back(store);

				AddBGNode(node);
	
				return;
			}

		#pragma endregion

		#pragma region Binary

		case Disassembler::OpCodes::COMPARE_OP:
			{
				cmpop_ty cmpop;
				switch (oparg)
				{
				case 0: cmpop = cmpop_ty::Lt; break;
				case 4: cmpop = cmpop_ty::Gt; break;
				case 3: cmpop = cmpop_ty::NotEq; break;
				case 2: cmpop = cmpop_ty::Eq; break;
				case 1: cmpop = cmpop_ty::LtE; break;
				case 5: cmpop = cmpop_ty::GtE; break;
				default: 
					throw HybridPythonResult::NOT_IMPLEMENTED;
				}

				expr_ty cmp = alloc<expr_ty>();
				cmp->lineno = _currentline;
				cmp->kind = _expr_kind::Compare_kind;
				cmp->v.Compare.ops = alloc_asdl_int_seq(1);
				cmp->v.Compare.comparators = alloc_asdl_seq(1);

				cmp->v.Compare.ops->elements[0] = cmpop;
				cmp->v.Compare.comparators->elements[0] = StackPopFront();
				cmp->v.Compare.left = StackPopFront();

				_expressionsByteCodeLocations[cmp] = node._index;

				StackPushFront(cmp);

				return;
			}


		case Disassembler::OpCodes::BINARY_POWER:
		case Disassembler::OpCodes::BINARY_MULTIPLY:
		case Disassembler::OpCodes::BINARY_MODULO:
		case Disassembler::OpCodes::BINARY_ADD:
		case Disassembler::OpCodes::BINARY_SUBTRACT:
		case Disassembler::OpCodes::BINARY_FLOOR_DIVIDE:
		case Disassembler::OpCodes::BINARY_TRUE_DIVIDE:
		case Disassembler::OpCodes::BINARY_LSHIFT:
		case Disassembler::OpCodes::BINARY_RSHIFT:
		case Disassembler::OpCodes::BINARY_AND:
		case Disassembler::OpCodes::BINARY_XOR:
		case Disassembler::OpCodes::BINARY_OR:
			{
				expr_ty expr = alloc<expr_ty>();
				expr->lineno = _currentline;
				expr->kind = _expr_kind::BinOp_kind;
				expr->v.BinOp.op = GetBinaryOperation(op);
				expr->v.BinOp.right = StackPopFront();
				expr->v.BinOp.left = StackPopFront();

				_expressionsByteCodeLocations[expr] = node._index;
				
				StackPushFront(expr);

				return;
			}

		case Disassembler::OpCodes::BINARY_SUBSCR:
			{
				expr_ty slice = StackPopFront();
				expr_ty ar = StackPopFront();

				expr_ty subscr = alloc<expr_ty>();
				subscr->lineno = _currentline;
				subscr->kind = _expr_kind::Subscript_kind;
				subscr->v.Subscript.ctx = expr_context_ty::Load;
				subscr->v.Subscript.slice = alloc<slice_ty>();
				subscr->v.Subscript.slice->kind = _slice_kind::Index_kind;
				subscr->v.Subscript.slice->v.Index.value = slice; // TODO: check slice type ???
				subscr->v.Subscript.value = ar;

				_expressionsByteCodeLocations[subscr] = node._index;

				StackPushFront(subscr);

				return;
			}

		#pragma endregion

		#pragma region Unary

		case Disassembler::OpCodes::UNARY_POSITIVE:
		case Disassembler::OpCodes::UNARY_NEGATIVE:
		case Disassembler::OpCodes::UNARY_NOT:
		case Disassembler::OpCodes::UNARY_INVERT:
			{
				expr_ty expr = alloc<expr_ty>();
				expr->lineno = _currentline;
				expr->kind = _expr_kind::UnaryOp_kind;
				expr->v.UnaryOp.op = GetUnaryOperation(op);
				expr->v.UnaryOp.operand = StackPopFront();

				_expressionsByteCodeLocations[expr] = node._index;

				StackPushFront(expr);

				return;
			}

		#pragma endregion

		case Disassembler::OpCodes::RETURN_VALUE:
			{
				expr_ty stackfront = StackPopFront();
				_returnvalues.push_back(stackfront);

				stmt_ty stmt = alloc<stmt_ty>();
				
				stmt->lineno = _currentline;
				stmt->kind = _stmt_kind::Return_kind;
				stmt->v.Return.value = stackfront;

				node._kind = BGNode_type::block;
				node.Block._statements.push_back(stmt);

				AddBGNode(node);

				_stackforward = false;
				
				return;
			}

		case Disassembler::OpCodes::CALL_FUNCTION:
			{
				// https://docs.python.org/3.6/library/dis.html#opcode-CALL_FUNCTION
				expr_ty call = alloc<expr_ty>();
				call->lineno = _currentline;
				call->kind = _expr_kind::Call_kind;
				std::vector<expr_ty> args;
				for (int k = 0; k < (int)oparg; ++k)
				{
					// last argument at top of stack
					args.insert(args.begin(), StackPopFront());
				}
				call->v.Call.func = StackPopFront();
				call->v.Call.keywords = alloc_asdl_seq(0); // empty sequence expected, not nullptr
				call->v.Call.args = BuildASDLSequence(args);

				StackPushFront(call);

				return;
			}

		case Disassembler::OpCodes::NOP:
			{
				return;
			}

		#pragma region inplace

		case Disassembler::OpCodes::INPLACE_FLOOR_DIVIDE:
		case Disassembler::OpCodes::INPLACE_TRUE_DIVIDE: 
		case Disassembler::OpCodes::INPLACE_SUBTRACT:
		case Disassembler::OpCodes::INPLACE_MULTIPLY:
		case Disassembler::OpCodes::INPLACE_MODULO:
		case Disassembler::OpCodes::INPLACE_POWER:
		case Disassembler::OpCodes::INPLACE_LSHIFT:
		case Disassembler::OpCodes::INPLACE_RSHIFT:
		case Disassembler::OpCodes::INPLACE_AND:
		case Disassembler::OpCodes::INPLACE_XOR:
		case Disassembler::OpCodes::INPLACE_OR:
		case Disassembler::OpCodes::INPLACE_ADD:
			{
				expr_ty expr = alloc<expr_ty>();
				expr->lineno = _currentline;
				expr->kind = _expr_kind::BinOp_kind;
				expr->v.BinOp.op = GetBinaryOperation(op);
				expr->v.BinOp.right = StackPopFront();
				expr->v.BinOp.left = StackPopFront();

				_expressionsByteCodeLocations[expr] = node._index;
				RegisterInplace(expr);

				StackPushFront(expr);

				return;
			}
		
		case Disassembler::OpCodes::INPLACE_MATRIX_MULTIPLY:
			ThrowNotImplemented();

		#pragma endregion

		#pragma region jump operations
		case Disassembler::OpCodes::JUMP_IF_FALSE_OR_POP:
			{
				unsigned offset = oparg;

				// extract the condition => construct the if expr if necessary
				expr_ty condition = StackPopFront();
				
				// create the condition object
				Condition truecond, falsecond;
				truecond.direct = true;
				truecond.cond = condition;
				falsecond.direct = false;
				falsecond.cond = condition;
				OrCondition falseconditionset = _currentconditions;
				falseconditionset &= falsecond;
				_currentconditions &= truecond;

				// add the condition with false cond at target
				ExprStack targetstack = _stack;
				StackEntry se ; se._entry[falseconditionset] = condition;
				targetstack._stack.push_front(se);
				// at target, set the stack
				_stackbyoffset[offset] = targetstack;

				_conditionsbyoffset[offset] |= falseconditionset;

				node._kind = BGNode_type::branch;
				node.Branch._cond = falsecond;
				node.Branch._target = offset;

				AddBGNode(node);

				return;
			}
		case Disassembler::OpCodes::JUMP_IF_TRUE_OR_POP:
			{
				unsigned offset = oparg;

				// extract the condition => construct the if expr if necessary
				expr_ty condition = StackPopFront();

				// create the condition object
				Condition truecond, falsecond;
				truecond.direct = true;
				truecond.cond = condition;
				falsecond.direct = false;
				falsecond.cond = condition;
				OrCondition trueconditionset = _currentconditions;
				trueconditionset &= truecond;
				_currentconditions &= falsecond;

				// add the condition with false cond at target
				ExprStack targetstack = _stack;
				StackEntry se; se._entry[trueconditionset] = condition;
				targetstack._stack.push_front(se);
				// at target, set the stack
				_stackbyoffset[offset] = targetstack;

				_conditionsbyoffset[offset] |= trueconditionset;

				node._kind = BGNode_type::branch;
				node.Branch._cond = truecond;
				node.Branch._target = offset;

				AddBGNode(node);

				return;
			}
		case Disassembler::OpCodes::POP_JUMP_IF_FALSE:
			{
				unsigned offset = oparg;
				expr_ty condition = StackPopFront();
								
				// get jump target : offset -> save stack status...
				_stackbyoffset[offset] = _stack;
				
				// add a condition
				Condition truecond, falsecond;
				truecond.direct = true;
				truecond.cond = condition;
				falsecond.direct = false;
				falsecond.cond = condition;

				OrCondition falseconditionset = _currentconditions;
				falseconditionset &= falsecond;
				_currentconditions &= truecond;

				_conditionsbyoffset[offset] |= falseconditionset;

				node._kind = BGNode_type::branch;
				node.Branch._cond = falsecond;
				node.Branch._target = offset;

				AddBGNode(node);

				return;
			}
		case Disassembler::OpCodes::POP_JUMP_IF_TRUE:
			{
				unsigned offset = oparg;
				expr_ty condition = StackPopFront();

				// get jump target : offset -> save stack status...
				_stackbyoffset[offset] = _stack;

				// add a condition
				Condition truecond, falsecond;
				truecond.direct = true;
				truecond.cond = condition;
				falsecond.direct = false;
				falsecond.cond = condition;

				OrCondition trueconditionset = _currentconditions;
				trueconditionset &= truecond;
				_currentconditions &= falsecond;

				_conditionsbyoffset[offset] |= trueconditionset;

				node._kind = BGNode_type::branch;
				node.Branch._cond = truecond;
				node.Branch._target = offset;

				AddBGNode(node);

				return;
			}
		case Disassembler::OpCodes::JUMP_FORWARD:
			{
				// 6 JUMP_FORWARD             2 (to 10)
				unsigned target = oparg + index;

				// target conditions and stacks need to be aligned !
				if (_conditionsbyoffset.find(target) != _conditionsbyoffset.end())
					_conditionsbyoffset[target] |= _currentconditions;
				else
					_conditionsbyoffset[target] = _currentconditions;
				if (_stackbyoffset.find(target) != _stackbyoffset.end())
					_stackbyoffset[target] = StackMerge(_stackbyoffset[target], _stack);
				else
					_stackbyoffset[target] = _stack;

				node._kind = BGNode_type::branch;
				node.Branch._cond = Condition(nullptr);
				node.Branch._target = target;

				AddBGNode(node);

				// no stack forwarding after this instruction !
				_stackforward = false;

				return;
			}

		case Disassembler::OpCodes::JUMP_ABSOLUTE:
			{	
				unsigned target = oparg;

				node._kind = BGNode_type::branch;
				node.Branch._cond = Condition(nullptr);
				node.Branch._target = target;

				AddBGNode(node);

				// if jump downwards, this is an if (end of if block)
				if ((int)target > index)
				{

					// target conditions and stacks need to be aligned !
					if (_conditionsbyoffset.find(target) != _conditionsbyoffset.end())
						_conditionsbyoffset[target] |= _currentconditions;
					else
						_conditionsbyoffset[target] = _currentconditions;
					if (_stackbyoffset.find(target) != _stackbyoffset.end())
						_stackbyoffset[target] = StackMerge(_stackbyoffset[target], _stack);
					else
						_stackbyoffset[target] = _stack;

					// no stack forwarding after this instruction !
					_stackforward = false;
				}
				else {
					// this is a loop end => 
					_stackforward = false;
				}
				return;
			}

		#pragma endregion

		#pragma region Loops

		case Disassembler::OpCodes::BREAK_LOOP:
			{
				stmt_ty brk = alloc<stmt_ty>();
				brk->lineno = _currentline;
				brk->kind = _stmt_kind::Break_kind;

				node._kind = BGNode_type::loopevent;
				node.Loopevent._kind = BGNode_loopevent::loop_break;
				node.Loopevent.LoopBreak._stmt = brk;

				AddBGNode(node);

				return;
			}

		case Disassembler::OpCodes::SETUP_LOOP:
			{
				unsigned endloop = index + oparg;

				node._kind = BGNode_type::loopevent;
				node.Loopevent._kind = BGNode_loopevent::loop_start;
				node.Loopevent.LoopStart._endloop = endloop;

				AddBGNode(node);

				#if 0
				Loop l; 
				l.startindex = index;
				l.endindex = endloop;
				l._cond = _currentconditions;
				
				// prepare a statement - begin with a while
				stmt_ty loop = alloc<stmt_ty>();
				loop->lineno = _currentline;
				// set to while by default => FOR_ITER op code will change type.
				loop->kind = _stmt_kind::While_kind;

				l._stmt = loop;

				_loops.push_front(l); // link between current block stack entry and loops

				AddStatement(loop);
				#endif
				return;
			}

		case Disassembler::OpCodes::GET_ITER:
			{
				node._kind = BGNode_type::loopevent;
				node.Loopevent._kind = BGNode_loopevent::loop_getiter;
				node.Loopevent.GetIter._range = StackPopFront();

				AddBGNode(node);

				// done
				return ;
			}

		case Disassembler::OpCodes::FOR_ITER:
			{
				// TODO: here current block should be empty... (besides for statement)

				// push stack to target -- its actually the FOR_ITER that does the jump.
				unsigned target = oparg + index;
				// target conditions and stacks need to be aligned !
				if (_conditionsbyoffset.find(target) != _conditionsbyoffset.end())
					_conditionsbyoffset[target] |= _currentconditions;
				else
					_conditionsbyoffset[target] = _currentconditions;
				if (_stackbyoffset.find(target) != _stackbyoffset.end())
					_stackbyoffset[target] = StackMerge(_stackbyoffset[target], _stack);
				else
					_stackbyoffset[target] = _stack;

				node._kind = BGNode_type::loopevent;
				node.Loopevent._kind = BGNode_loopevent::loop_foriter;
				node.Loopevent.ForIter._endloop = target;

				// TODO : handle large argument (more than 256 args+locals)
				// next instruction should be a STORE => get it !
				switch (codes[index])
				{
					case Disassembler::OpCodes::STORE_FAST:
						{
							expr_ty target = alloc<expr_ty>();
							target->lineno = _currentline;
							target->kind = _expr_kind::Name_kind;
							target->v.Name.ctx = expr_context_ty::Store;
							target->v.Name.id = _varnames[codes[index + 1]];

							node.Loopevent.ForIter._target = target;

							break;
						}
					default: 
						throw HybridPythonResult::NOT_IMPLEMENTED;
				}
				index += 2; // INSTRUCTION IS CONSUMED HERE => VERIFY IT IS OKAY OTHER PLACES...
				
				// verify loop end ? => there shall be a POP_BLOCK there anyway...

				AddBGNode(node);

				return;
			}

		case Disassembler::OpCodes::POP_BLOCK:
			{
				node._kind = BGNode_type::loopevent;
				node.Loopevent._kind = BGNode_loopevent::loop_end;
				
				AddBGNode(node);

				return;

				/*
				// get the loop
				Loop l = _loops.front();
				std::vector<stmt_ty> body;

				switch (l._stmt->kind)
				{
					case _stmt_kind::While_kind:
						{
							// unwind blocks to while : should be an empty block, and a block with the condition
							StatementBlock sb = _blocks.front(); _blocks.pop_front() ;
							if (sb._statements.size() != 0)
								throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;
							if (_blocks.front()._statements.size() == 0)
								throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;
							StatementBlock body = _blocks.front(); _blocks.pop_front();

							stmt_ty loopstmt = _blocks.front()._statements.back();
							if (loopstmt != l._stmt)
								throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;

							// get the condition
							AndCondition loopcond = GetCommonConditions(body._cond);
							AndCondition basecond = GetCommonConditions(_blocks.front()._cond);

							if (!loopcond.issub(basecond))
								throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;

							AndCondition discr = loopcond.knowing(basecond);
							if (discr._conds.size() != 1)
								throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;

							if (!(discr._conds[0].direct))
								throw HybridPythonResult::NOT_IMPLEMENTED;

							OrCondition check = _blocks.front()._cond;
							check &= discr._conds[0];
							if (body._cond != check)
								throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;

							loopstmt->v.While.test = discr._conds[0].cond;
							loopstmt->v.While.body = BuildASDLSequence(body._statements);
							loopstmt->v.While.orelse = nullptr; // may be filled-in later on.

							// repush the else block 
							_blocks.push_front(sb);
							return;
						}
					case _stmt_kind::For_kind:
						{
							StatementBlock body = _blocks.front(); _blocks.pop_front();

							stmt_ty loopstmt = _blocks.front()._statements.back();
							if (loopstmt != l._stmt)
								throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;
							
							loopstmt->v.For.body = BuildASDLSequence(body._statements);
							loopstmt->v.For.orelse = nullptr; // may be filled-in later on.

							// block for else 
							_blocks.push_front(StatementBlock(_currentconditions));
							return;
						}
					default:
						throw HybridPythonResult::NOT_IMPLEMENTED;
				}
				*/
			}
		#pragma endregion

		#pragma region Stack operations

		case Disassembler::OpCodes::ROT_THREE:
			{
				// TODO: make sure conditions are the same
				StackEntry st0 = _stack._stack.front(); _stack._stack.pop_front();
				StackEntry st1 = _stack._stack.front(); _stack._stack.pop_front();
				StackEntry st2 = _stack._stack.front(); _stack._stack.pop_front();

				_stack._stack.push_front(st0);
				_stack._stack.push_front(st2);
				_stack._stack.push_front(st1);

				return;
			}

		case Disassembler::OpCodes::ROT_TWO:
		{
			// TODO: make sure conditions are the same
			StackEntry st0 = _stack._stack.front(); _stack._stack.pop_front();
			StackEntry st1 = _stack._stack.front(); _stack._stack.pop_front();

			_stack._stack.push_front(st0);
			_stack._stack.push_front(st1);

			return;
		}

		case Disassembler::OpCodes::DUP_TOP_TWO:
			{
				// TODO: make sure conditions are the same
				StackEntry st0 = _stack._stack.front(); _stack._stack.pop_front();
				StackEntry st1 = _stack._stack.front(); _stack._stack.pop_front();

				_stack._stack.push_front(st1);
				_stack._stack.push_front(st0);
				_stack._stack.push_front(st1);
				_stack._stack.push_front(st0);

				return;
			}

		case Disassembler::OpCodes::DUP_TOP:
			{
				StackEntry st0 = _stack._stack.front();
				
				_stack._stack.push_front(st0);

				return;
			}

		case Disassembler::OpCodes::POP_TOP:
			{
				// expression statement
				expr_ty front = StackPopFront();

				stmt_ty stmt = alloc<stmt_ty>();
				stmt->lineno = _currentline;
				stmt->kind = _stmt_kind::Expr_kind;
				stmt->v.Expr.value = front;

				node._kind = BGNode_type::block;
				node.Block._statements.push_back(stmt);

				AddBGNode(node);

				return;
			}

		#pragma endregion

		case Disassembler::OpCodes::RAISE_VARARGS:
			{
				unsigned count = oparg;
				
				stmt_ty stmt = alloc<stmt_ty>();
				stmt->lineno = _currentline;
				stmt->kind = _stmt_kind::Raise_kind;
				stmt->v.Raise.cause = nullptr;
				stmt->v.Raise.exc = StackPopFront();

				node._kind = BGNode_type::block;
				node.Block._statements.push_back(stmt);

				AddBGNode(node);

				return;
			}

		case Disassembler::OpCodes::BINARY_MATRIX_MULTIPLY:
		case Disassembler::OpCodes::GET_AITER:
		case Disassembler::OpCodes::GET_ANEXT:
		case Disassembler::OpCodes::BEFORE_ASYNC_WITH:
		case Disassembler::OpCodes::DELETE_SUBSCR:
		case Disassembler::OpCodes::GET_YIELD_FROM_ITER:
		case Disassembler::OpCodes::PRINT_EXPR:
		case Disassembler::OpCodes::LOAD_BUILD_CLASS:
		case Disassembler::OpCodes::YIELD_FROM:
		case Disassembler::OpCodes::GET_AWAITABLE:
		case Disassembler::OpCodes::WITH_CLEANUP_START:
		case Disassembler::OpCodes::WITH_CLEANUP_FINISH:
		case Disassembler::OpCodes::IMPORT_STAR:
		case Disassembler::OpCodes::SETUP_ANNOTATIONS:
		case Disassembler::OpCodes::YIELD_VALUE:
		case Disassembler::OpCodes::END_FINALLY:
		case Disassembler::OpCodes::POP_EXCEPT:
		case Disassembler::OpCodes::HAVE_ARGUMENT:
		//case Disassembler::OpCodes::STORE_NAME:
		case Disassembler::OpCodes::DELETE_NAME:
		case Disassembler::OpCodes::UNPACK_SEQUENCE:
		case Disassembler::OpCodes::UNPACK_EX:
		case Disassembler::OpCodes::STORE_ATTR:
		case Disassembler::OpCodes::DELETE_ATTR:
		case Disassembler::OpCodes::STORE_GLOBAL:
		case Disassembler::OpCodes::DELETE_GLOBAL:
		case Disassembler::OpCodes::LOAD_NAME:
		case Disassembler::OpCodes::BUILD_TUPLE:
		case Disassembler::OpCodes::BUILD_LIST:
		case Disassembler::OpCodes::BUILD_SET:
		case Disassembler::OpCodes::BUILD_MAP:
		case Disassembler::OpCodes::IMPORT_NAME:
		case Disassembler::OpCodes::IMPORT_FROM:
		case Disassembler::OpCodes::CONTINUE_LOOP:
		case Disassembler::OpCodes::SETUP_EXCEPT:
		case Disassembler::OpCodes::SETUP_FINALLY:
		case Disassembler::OpCodes::DELETE_FAST:
		case Disassembler::OpCodes::STORE_ANNOTATION:
		case Disassembler::OpCodes::MAKE_FUNCTION:
		case Disassembler::OpCodes::BUILD_SLICE:
		case Disassembler::OpCodes::LOAD_CLOSURE:
		case Disassembler::OpCodes::STORE_DEREF:
		case Disassembler::OpCodes::DELETE_DEREF:
		case Disassembler::OpCodes::CALL_FUNCTION_KW:
		case Disassembler::OpCodes::CALL_FUNCTION_EX:
		case Disassembler::OpCodes::SETUP_WITH:
		case Disassembler::OpCodes::EXTENDED_ARG:
		case Disassembler::OpCodes::LIST_APPEND:
		case Disassembler::OpCodes::SET_ADD:
		case Disassembler::OpCodes::MAP_ADD:
		case Disassembler::OpCodes::LOAD_CLASSDEREF:
		case Disassembler::OpCodes::BUILD_LIST_UNPACK:
		case Disassembler::OpCodes::BUILD_MAP_UNPACK:
		case Disassembler::OpCodes::BUILD_MAP_UNPACK_WITH_CALL:
		case Disassembler::OpCodes::BUILD_TUPLE_UNPACK:
		case Disassembler::OpCodes::BUILD_SET_UNPACK:
		case Disassembler::OpCodes::SETUP_ASYNC_WITH:
		case Disassembler::OpCodes::FORMAT_VALUE:
		case Disassembler::OpCodes::BUILD_CONST_KEY_MAP:
		case Disassembler::OpCodes::BUILD_STRING:
		case Disassembler::OpCodes::BUILD_TUPLE_UNPACK_WITH_CALL:
			ThrowNotImplemented();
		default:
			ThrowNotImplemented();
	};
}

void Disassembler::MergeBlockStatements()
{
	// merge block statements of same condition
	for (auto iter = _branchinggraph.begin(); iter != _branchinggraph.end(); ++iter)
	{
		if (iter->second._kind == BGNode_type::block)
		{
			// push to next if exists
			auto next = iter; ++next;
			if ((next->second._kind == BGNode_type::block) &&
				(next->second._cond == iter->second._cond))
			{
				for (int k = 0; k < next->second.Block._statements.size(); ++k)
				{
					iter->second.Block._statements.push_back(next->second.Block._statements[k]);
				}
				next->second.Block._statements = iter->second.Block._statements;
				iter->second.Block._statements.clear();
			}
		}
	}
}

void Disassembler::FeedArriveFrom()
{
	// feed arrive from
	for (auto iter = _branchinggraph.begin(); iter != _branchinggraph.end(); ++iter)
	{
		switch (iter->second._kind)
		{
		case BGNode_type::block:
		{
			// next is fed with arrive from
			auto next = iter;
			++next;
			if (next != _branchinggraph.end())
				next->second._arrivefrom.push_back(iter->first);
		}
		break;
		case BGNode_type::branch:
		{
			unsigned target = iter->second.Branch._target;
			auto entry = _branchinggraph.lower_bound(target);
			/* TODO: necessary ??? / check arrive from
			if (entry->first != target)
			{
				// check arrive in first statement
				if (entry->second._kind != BGNode_type::block)
					throw HybridPythonResult::DISASM_INTERNAL_STACK_ERROR;
			}*/
			entry->second._arrivefrom.push_back(iter->first);
			// if conditional jump, 
			if (iter->second.Branch._cond.cond != nullptr)
			{
				// next is fed with arrive from
				auto next = iter;
				++next;
				if (next != _branchinggraph.end())
					next->second._arrivefrom.push_back(iter->first);
			}
		}
		break;
		case BGNode_type::loopevent:
		{
			switch (iter->second.Loopevent._kind)
			{
			case BGNode_loopevent::loop_foriter:
				// may go to end of loop
			{
				unsigned target = iter->second.Loopevent.ForIter._endloop;
				auto entry = _branchinggraph.lower_bound(target);
				entry->second._arrivefrom.push_back(iter->first);
			}
			// jumpover to next case : also go to next instruction -- FOR_ITER is like a conditional jump.
			case BGNode_loopevent::loop_getiter:
			case BGNode_loopevent::loop_end: // when foriter is done, it jumps to loop_end, as while does
			case BGNode_loopevent::loop_start:
			{
				// next is fed with arrive from
				auto next = iter;
				++next;
				if (next != _branchinggraph.end())
					next->second._arrivefrom.push_back(iter->first);
			}
			break;
			case BGNode_loopevent::loop_break:
				// cannot do anything at this stage, need location stack
				break;
			}
		}
		break;
		default:
			ThrowInternalStackError();
		}
		if (iter->second._kind != BGNode_type::branch) continue;
	}
}

void Disassembler::RemoveUnreachableCode()
{
	// remove code that arrives from nowhere (can occur when jump targets are optimized
	std::vector<unsigned> removekeys;
	auto iter = _branchinggraph.begin(); 
	// skip root entry that arrives from nowhere by construction
	++iter;
	for (; iter != _branchinggraph.end(); ++iter)
	{
		if (iter->second._arrivefrom.size() == 0) removekeys.push_back(iter->first);
	}
	for (int k = 0; k < removekeys.size(); _branchinggraph.erase(removekeys[k++])) {}
}

Disassembler::BGNodeLocationOption Disassembler::BGNodeLocationScenario::reduce()
{
	if (_options.size() < 1)
		ThrowInternalStackError();
	// merge entries
	while (_options.size() != 1)
	{
		bool found = false;
		for (int k = 0 ; k < _options.size() ; ++k)
		{
			for (int j = k+1 ; j < _options.size() ; ++j)
			{
				if (_options[k].complements(_options[j]))
				{
					_options[k]._location.pop_front();
					_options.erase(_options.begin() + j);
					found = true;
					break;
				}
			}
		}
		if (!found) // cannot be reduced.
			ThrowInternalStackError();
	}
	return _options[0];
}

void Disassembler::AssignLocations()
{
	// first entry gets "allpath" scenario
	_branchinggraph.begin()->second._locscenario.addoption(BGNodeLocationOption());

	// push scenarii and process arrive from
	unsigned previndex = 0;
	std::deque<unsigned> loopends; // stack of loop ends.
	for (auto iter = _branchinggraph.begin(); iter != _branchinggraph.end(); previndex = iter->first, ++iter)
	{
		// merge scenario options
		iter->second._location = iter->second._locscenario.reduce();

		// push location to targets
		switch (iter->second._kind)
		{
			case BGNode_type::block:
				{
					// assign to next
					auto next = iter; ++next;
					if (next != _branchinggraph.end())
						next->second._locscenario.addoption(iter->second._location);
				}
				break;
			case BGNode_type::branch:
				{
					// get target
					unsigned target = iter->second.Branch._target;
					if (target > iter->first)
					{
						auto found = _branchinggraph.lower_bound(target);
						BGNodeLocationOption jumpopt = iter->second._location;
						BGNodeLocationOption contopt = iter->second._location;
						if (iter->second.Branch._cond.cond != nullptr)
						{
							// if (cond.direct == false => if, otherwise notimpl
							if (iter->second.Branch._cond.direct)
							{
								jumpopt._location.push_front(BGNodeLocation::loc_if(iter->first));
								contopt._location.push_front(BGNodeLocation::loc_else(iter->first));
							} else {
								jumpopt._location.push_front(BGNodeLocation::loc_else(iter->first));
								contopt._location.push_front(BGNodeLocation::loc_if(iter->first));
							}
							auto next = iter; ++next;
							if (next != _branchinggraph.end())
								next->second._locscenario.addoption(contopt);
						}
						if (found != _branchinggraph.end())
							found->second._locscenario.addoption(jumpopt);
					}
					else {
						// if non conditional, continue statement => no-op, otherwise, next
						if (iter->second.Branch._cond.cond != nullptr)
						{
							// if (cond.direct == false => if, otherwise notimpl
							if (iter->second.Branch._cond.direct)
								ThrowNotImplemented();
							BGNodeLocationOption contopt = iter->second._location;
							contopt._location.push_front(BGNodeLocation::loc_if(iter->first));
							auto next = iter; ++next;
							if (next != _branchinggraph.end())
								next->second._locscenario.addoption(contopt);
						}
					}
				}
				break;
			case BGNode_type::loopevent:
				{
					switch (iter->second.Loopevent._kind)
					{
						case BGNode_loopevent::loop_start:
							{
								BGNodeLocationOption enter = iter->second._location;
								unsigned bodystart;
								// if followed by foriter => for
								auto next = iter; ++next;
								if (next->second._kind == BGNode_type::loopevent)
								{
									// FOR LOOP - start,getiter,foriter
									if (next->second.Loopevent._kind != BGNode_loopevent::loop_getiter)
										ThrowInternalStackError();
									auto foriter = next; ++foriter;
									bodystart = foriter->first;
								} else {
									// WHILE LOOP
									bodystart = next->first;
								}
								unsigned endloop = iter->second.Loopevent.LoopStart._endloop;
								loopends.push_back(endloop);
								enter._location.push_front(BGNodeLocation::loc_loop(iter->first, endloop, bodystart));
								if (next != _branchinggraph.end())
									next->second._locscenario.addoption(enter);
								auto found = _branchinggraph.lower_bound(endloop);
								if (found != _branchinggraph.end())
									found->second._locscenario.addoption(iter->second._location);

								break;
							}
						case BGNode_loopevent::loop_foriter:
							{
								unsigned endbody = iter->second.Loopevent.ForIter._endloop;
								BGNodeLocationOption leave = iter->second._location;
								if (leave.front()._kind != BGNodeLocation_type::loc_loop)
									ThrowInternalStackError();
								leave._location.pop_front();
								auto found = _branchinggraph.lower_bound(endbody);
								if (found != _branchinggraph.end())
									found->second._locscenario.addoption(leave);
								// also assign to next. => no break
							}
						case BGNode_loopevent::loop_getiter:
						case BGNode_loopevent::loop_break:
							{
								// assign to next
								auto next = iter; ++next;
								if (next != _branchinggraph.end())
									next->second._locscenario.addoption(iter->second._location);
							}
							// nothing here
							break;
						case BGNode_loopevent::loop_end:
							{
								// nothing to do here !
							}
							break;
						default:
							ThrowInternalStackError();
					}
				}
				break;
			default:
				ThrowInternalStackError();
		}
	}

	// assign locations 
}

void Disassembler::Process()
{
	_stackforward = false;
	_prevlnotab = 0;
	_currentline = _firstlineno;
	_currentlnotab = _lnotab;

	_currentconditions = OrCondition::all();
	_conditionsbyoffset[0] = _currentconditions;

	for (int index = 0; index < _codelen; )
	{
		// maintain line number
		if (((int)(*_currentlnotab) + _prevlnotab) == index)
		{
			_prevlnotab = index;
			_currentline += _currentlnotab[1];
			_currentlnotab += 2;
		}
		// stack forwarding operations
		if (_stackforward)
		{
			if (_stackbyoffset.find(index) != _stackbyoffset.end())
				_stackbyoffset[index] = StackMerge(_stackbyoffset[index], _stack);
			else
				_stackbyoffset[index] = _stack;

			// condition forwarding ...
			if (_conditionsbyoffset.find(index) != _conditionsbyoffset.end())
				_conditionsbyoffset[index] |= _currentconditions;
			else
				_conditionsbyoffset[index] = _currentconditions;
		}
		_stack = _stackbyoffset[index]; // should be empty at first iteration
		_currentconditions = _conditionsbyoffset[index]; // should be empty at first iteration

		// close loops 

		AppendCode(_code, index);
	}

	MergeBlockStatements();
	FeedArriveFrom();
	RemoveUnreachableCode();
	AssignLocations();

}

std::pair<unsigned,unsigned> Disassembler::GetLocationRange(const BGNodeLocationOption& loc, unsigned from, unsigned to)
{
	if (loc.size() == 0) return std::pair<unsigned, unsigned>(from, to);
	// from should be valid and have proper location
	auto iter = _branchinggraph.find(from);
	if (iter == _branchinggraph.end())
		ThrowInternalStackError();

	while (iter->second._location.issub(loc))
	{
		++iter ;
		if (iter->first > to)
			return std::pair<unsigned,unsigned>(from,to);
	}
	// rewind once => inclusive
	--iter;
	return std::pair<unsigned, unsigned>(from,iter->first);
}

std::vector<stmt_ty> Disassembler::ExtractBlock(unsigned from, unsigned to)
{
	if (to < from) return std::vector<stmt_ty>();
	if (_branchinggraph.find(from) == _branchinggraph.end())
		ThrowInternalStackError();

	std::vector<stmt_ty> result ;
	
	// base location of the whole block.
	BGNodeLocationOption loc = _branchinggraph[from]._location;
	unsigned index = from;

	while (index <= to)
	{
		auto current = _branchinggraph.find(index);

		// get the location
		auto location = current->second._location;

		// different location ?
		if (location != loc)
		{
			ThrowInternalStackError();
		}
		// while location is the same, accumulate
		else //  (location == loc)
		{
			switch (current->second._kind)
			{
				case BGNode_type::block:
					{
						for (int k = 0; k < current->second.Block._statements.size(); ++k)
						{
							// ==== STMT ====
							result.push_back(current->second.Block._statements[k]);
							// ==== STMT ====
						}
						// go to the next index
						index = (++current)->first;
						continue;
					}
				case BGNode_type::branch:
					{
						// can be a continue statement
						if (current->second.Branch._target < index)
						{
							// location needs to be the same
							if (location != loc)
								ThrowInternalStackError();

							bool found = false;
							// look for loop in stack
							for (auto iter = location._location.begin(); iter != location._location.end(); ++iter)
							{
								if (iter->_kind == BGNodeLocation_type::loc_loop)
								{
									if (_branchinggraph.lower_bound(current->second.Branch._target)->first == iter->v.Loop.bodystart)
									{
										found = true;

										break;
									}
								}
							}
							if (!found)
								ThrowInternalStackError();

							// do we have a condition ?
							if (current->second.Branch._cond.cond != nullptr)
							{
								// conditionnally jump to upper, means the condition is down-till end of loop body
								auto ifbodynode = current; ++ifbodynode;
								std::vector<stmt_ty> ifblock = ExtractBlock(ifbodynode->first, to);

								// build the if statement 
								stmt_ty ifstmt = alloc<stmt_ty>();
								ifstmt->lineno = current->second._sourceline;
								ifstmt->kind = _stmt_kind::If_kind;
								if (current->second.Branch._cond.direct) // cond is the cond of the jump => reversed !
									ThrowInternalStackError();
								ifstmt->v.If.test = current->second.Branch._cond.cond;
								ifstmt->v.If.body = BuildASDLSequence(ifblock);
								ifstmt->v.If.orelse = nullptr;

								// ==== STMT ====
								result.push_back(ifstmt);
								// ==== STMT ====

								auto next = _branchinggraph.lower_bound(to);
								if (next->first <= to) ++next;
								index = next->first;
								continue;

							} else {
								stmt_ty cont = alloc<stmt_ty>();
								cont->lineno = current->second._sourceline;
								cont->kind = _stmt_kind::Continue_kind;

								// ==== STMT ====
								result.push_back(cont);
								// ==== STMT ====
							}
							// go to the next index
							index = (++current)->first;
							continue;
						}
						else if (current->second.Branch._cond.cond == nullptr)
						{
							// no if statement => verify last statement
							if (index == to)
							{
								// break the loop
								index = to+1;
								continue;
							} else 
								ThrowInternalStackError();
						}
						// if statement
						else {
							auto ifloc = location;
							auto elseloc = location;
							ifloc._location.push_front(BGNodeLocation::loc_if(current->first));
							elseloc._location.push_front(BGNodeLocation::loc_else(current->first));
							
							// get its range
							// go to next instruction
							unsigned ifstart = (++(_branchinggraph.lower_bound(from)))->first;
							std::pair<unsigned,unsigned> ifrange = GetLocationRange(ifloc, ifstart, to);
							unsigned elsestart = (++(_branchinggraph.lower_bound(ifrange.second)))->first;
							std::pair<unsigned,unsigned> elserange = GetLocationRange(elseloc, elsestart, to);

							std::vector<stmt_ty> ifblock = ExtractBlock(ifrange.first, ifrange.second);
							std::vector<stmt_ty> elseblock = ExtractBlock(elserange.first, elserange.second);

							stmt_ty stmt = nullptr;

							// if else block is Raise => may be an assert
							if ((elseblock.size() == 1) && (elseblock[0]->kind == _stmt_kind::Raise_kind))
							{
								expr_ty exc = elseblock[0]->v.Raise.exc;
								if ((exc->kind != _expr_kind::Name_kind) &&
									(exc->kind != _expr_kind::Call_kind)) ThrowNotImplemented();

								std::string excname;
								if (exc->kind == _expr_kind::Name_kind) excname = UNICODE_TO_STRING(exc->v.Name.id);
								else if (exc->kind == _expr_kind::Call_kind) excname = UNICODE_TO_STRING(exc->v.Call.func->v.Name.id);
								// TODO : more assertions for function call...
								if (excname != std::string("AssertionError")) ThrowNotImplemented();
								if (ifblock.size() != 0) ThrowNotImplemented();
								// Build the assert statement
								stmt_ty asrt = alloc<stmt_ty>();
								asrt->lineno = current->second._sourceline;
								asrt->kind = _stmt_kind::Assert_kind;
								asrt->v.Assert.msg = exc->kind == _expr_kind::Call_kind ? (expr_ty)(exc->v.Call.args->elements[0]) : nullptr;
								asrt->v.Assert.test = current->second.Branch._cond.cond;

								stmt = asrt;
							}
							else
							{
								// build the if statement 
								stmt_ty ifstmt = alloc<stmt_ty>();
								ifstmt->lineno = current->second._sourceline;
								ifstmt->kind = _stmt_kind::If_kind;
								if (current->second.Branch._cond.direct) // cond is the cond of the jump => reversed !
									ThrowInternalStackError();
								ifstmt->v.If.test = current->second.Branch._cond.cond;
								ifstmt->v.If.body = BuildASDLSequence(ifblock);
								ifstmt->v.If.orelse = BuildASDLSequence(elseblock);

								stmt = ifstmt;
							}

							// ==== STMT ====
							result.push_back(stmt);
							// ==== STMT ====

							// go to next index
							index = (++(_branchinggraph.lower_bound(elserange.second)))->first;
							continue;
						}
					}
				case BGNode_type::loopevent:
				{
					// which loop event
					switch (current->second.Loopevent._kind)
					{
					case BGNode_loopevent::loop_end:
					{
						// this may happen at the end of while loops => empty statement
						index = (++current)->first;
						continue;
					}
					case BGNode_loopevent::loop_break:
					{
						// ==== STMT ====
						result.push_back(current->second.Loopevent.LoopBreak._stmt);
						// ==== STMT ====

						index = (++current)->first;
						continue;
					}
					case BGNode_loopevent::loop_start:
					{
						// next instruction has the loop range
						auto loopfirst = current; ++loopfirst;
						std::pair<unsigned, unsigned> looprange = GetLocationRange(loopfirst->second._location, loopfirst->first, to);
						if (loopfirst->second._kind == BGNode_type::loopevent)
						{
							if (loopfirst->second.Loopevent._kind != BGNode_loopevent::loop_getiter)
								ThrowInternalStackError();
							// FOR LOOP
							auto getiternode = loopfirst++;
							auto foriternode = loopfirst++;
							auto bodyfirst = loopfirst;

							std::pair<unsigned, unsigned> bodyrange = GetLocationRange(bodyfirst->second._location, 
										bodyfirst->first, 
										foriternode->second.Loopevent.ForIter._endloop);
							std::vector<stmt_ty> body = ExtractBlock(bodyrange.first, bodyrange.second);

							// remove last continue statement -- if not removed by unreachable code statement
							if (body.back()->kind == _stmt_kind::Continue_kind)
								body.pop_back();

							// ORELSE IS TODO 

							stmt_ty forstmt = alloc<stmt_ty>();
							forstmt->lineno = current->second._sourceline;
							forstmt->kind = _stmt_kind::For_kind;
							forstmt->v.For.target = foriternode->second.Loopevent.ForIter._target;
							forstmt->v.For.iter = getiternode->second.Loopevent.GetIter._range;
							forstmt->v.For.body = BuildASDLSequence(body);
							forstmt->v.For.orelse = nullptr;
							
							// ==== STMT ====
							result.push_back(forstmt);
							// ==== STMT ====

							auto found = _branchinggraph.lower_bound(looprange.second);
							++found;
							// TODO: verify loop is popped !
							index = found->first;
							continue;
						}
						else
						{
							expr_ty cond = nullptr;
							// condition ?
							if (loopfirst->second._kind == BGNode_type::branch)
							{
								if (loopfirst->second.Branch._cond.direct)
									ThrowNotImplemented();
								cond = loopfirst->second.Branch._cond.cond;
								++loopfirst;
							}

							std::pair<unsigned, unsigned> bodyrange = GetLocationRange(loopfirst->second._location, loopfirst->first, looprange.second);
							std::vector<stmt_ty> body = ExtractBlock(bodyrange.first, bodyrange.second);
							// remove last continue statement -- if not removed by unreachable code statement
							if (body.back()->kind == _stmt_kind::Continue_kind)
								body.pop_back();

							// ORELSE IS TODO: std::pair<unsigned, unsigned> elserange = GetLocationRange(

							stmt_ty whilestmt = alloc<stmt_ty>();
							whilestmt->lineno = current->second._sourceline;
							whilestmt->kind = _stmt_kind::While_kind;
							whilestmt->v.While.test = cond;
							whilestmt->v.While.body = BuildASDLSequence(body);
							whilestmt->v.While.orelse = nullptr;

							// ==== STMT ====
							result.push_back(whilestmt) ;
							// ==== STMT ====

							auto found = _branchinggraph.lower_bound(looprange.second);
							++found;
							// TODO: verify loop is popped !
							index = found->first;
							continue;
						}
					}
					default:
						ThrowInternalStackError();
				}
			}
			}
		}
	}

	#if 0
	std::deque<BGNodeLocation> loc = _branchinggraph[from]._location._location;

	unsigned index = from;

	while (index <= to)
	{
		auto current = _branchinggraph.find(index);
		switch (current->second._kind)
		{
			case BGNode_type::block:
				{
					for (int k = 0; k < current->second.Block._statements.size(); ++k)
					{
						// ==== STMT ====
						result.push_back(current->second.Block._statements[k]);
						// ==== STMT ====
					}
					// go to the next index
					index = (++current)->first;
					continue;
				}
			case BGNode_type::branch:
				{
					// no condition, up jump => its a continue...
					if (current->second.Branch._cond.cond == nullptr)
					{
						if (current->second.Branch._target > index)
						{
							// its a down jump => last instruction of if block => skip and return !
							if (index != to)
								ThrowInternalStackError();

							index = (++current)->first;
							break;

						} else {
							// rewind and look for loop
							int loopstart = -1;
							for (auto iter = loc.begin(); iter != loc.end() ; ++iter)
							{
								if (iter->_kind == BGNodeLocation_type::loc_loop)
									loopstart = iter->v.Loop.start;
							}
							if (loopstart == -1)
								ThrowInternalStackError();
							if ((int)(current->second.Branch._target) == loopstart)
							{
								// continue statement
								stmt_ty cont = alloc<stmt_ty>();
								cont->lineno = current->second._sourceline;
								cont->kind = _stmt_kind::Continue_kind;

								// ==== STMT ====
								result.push_back(cont);
								// ==== STMT ====

								// go to the next index
								index = (++current)->first;
								continue;
							} else
								ThrowInternalStackError();
						}
					} 
					// condition here => entering an if
					else 
					{
						// there is condition => entering an if, next should be an if
						auto next = current; ++next;
						if (next->second._location.front()._kind != BGNodeLocation_type::loc_if)
							ThrowInternalStackError();
						if (!(BGNodeLocation::ispartof(loc, next->second._location._location)))
							ThrowInternalStackError();

						// get the span of the if
						auto ifloc = next->second._location._location;
						auto subseq = next; ++subseq;
						while (BGNodeLocation::ispartof(ifloc, subseq->second._location._location)) ++subseq;
						// get the previous index
						--subseq;
						// extract the if block
						std::vector<stmt_ty> ifblock = ExtractBlock(next->first, subseq->first);
						// if ifblock is empty => (ternary operator) => return empty
						if (ifblock.size() == 0)
						{
							// go to the next index
							index = (++subseq)->first;
							continue;
						}
						
						// check if an elseblock exists
						std::vector<stmt_ty> elseblock;
						next = ++subseq;
						if ((next->second._location.size() >= 1) &&
							(next->second._location.front()._kind == BGNodeLocation_type::loc_else))
						{
							if (!(BGNodeLocation::ispartof(loc, next->second._location._location)))
								ThrowInternalStackError();
							auto elseloc = next->second._location._location;
							while (BGNodeLocation::ispartof(elseloc, subseq->second._location._location)) ++subseq;
							--subseq;

							// extract the else block
							elseblock = ExtractBlock(next->first, subseq->first);
						}

						// build the if block
						stmt_ty ifstmt = alloc<stmt_ty>();
						ifstmt->lineno = current->second._sourceline;
						ifstmt->kind = _stmt_kind::If_kind;
						if (current->second.Branch._cond.direct) // cond is the cond of the jump => reversed !
							ThrowInternalStackError();
						ifstmt->v.If.test = current->second.Branch._cond.cond;
						ifstmt->v.If.body = BuildASDLSequence(ifblock);
						ifstmt->v.If.orelse = BuildASDLSequence(elseblock);

						// ==== STMT ====
						result.push_back(ifstmt);
						// ==== STMT ====

						// go to the next index
						index = (++subseq)->first;
						continue;
					}
				}
			case BGNode_type::loopevent:
				{
					// which loop event
					switch (current->second.Loopevent._kind)
					{
						case BGNode_loopevent::loop_end:
							{
								// this may happen at the end of while loops => empty statement
								index = (++current)->first;
								continue;
							}
						case BGNode_loopevent::loop_break:
							{
								// ==== STMT ====
								result.push_back(current->second.Loopevent.LoopBreak._stmt);
								// ==== STMT ====

								index = (++current)->first;
								continue;
							}
						case BGNode_loopevent::loop_start:
							{
								// entering loop prelude -> we know the end... (after the orelse)
								unsigned loopfullend = current->second.Loopevent.LoopStart._endloop;
								
								auto next = current ; ++next;
								
								auto looploc = next->second._location;

								// branch => While
								if (next->second._kind == BGNode_type::branch)
								{
									if (next->second.Branch._cond.cond == nullptr)
										ThrowInternalStackError();

									// body has the same loc as the next entry
									auto bodystart = next; ++bodystart;
									auto bodyloc = bodystart->second._location._location;
									auto subseq = bodystart; ++subseq;
									while (BGNodeLocation::ispartof(bodyloc, subseq->second._location._location)) ++subseq;
									--subseq;

									std::vector<stmt_ty> body = ExtractBlock(bodystart->first, subseq->first);

									++subseq ;
									// here, should be loopevent::end
									if (subseq->second._kind != BGNode_type::loopevent)
										ThrowInternalStackError();
									if (subseq->second.Loopevent._kind != BGNode_loopevent::loop_end)
										ThrowInternalStackError();

									// go to next iteration == consume loopend
									++subseq;
									// extract orelse (needs testing)
									auto orelsestart = subseq;
									auto orelseloc = orelsestart->second._location;

									subseq = _branchinggraph.lower_bound(loopfullend);
									--subseq;

									std::vector<stmt_ty> orelse = ExtractBlock(orelsestart->first, subseq->first);

									// build the statement
									stmt_ty whilestmt = alloc<stmt_ty>();
									whilestmt->lineno = current->second._sourceline;
									whilestmt->kind = _stmt_kind::While_kind;
									if (!(next->second.Branch._cond.direct))
										throw HybridPythonResult::NOT_IMPLEMENTED;
									whilestmt->v.While.test = next->second.Branch._cond.cond;
									whilestmt->v.While.body = BuildASDLSequence(body);
									whilestmt->v.While.orelse = BuildASDLSequence(orelse);

									// ==== STMT ====
									result.push_back(whilestmt);
									// ==== STMT ====

									index = (++subseq)->first;
									continue;
								}
								// loopevent => For
								else if (next->second._kind == BGNode_type::loopevent)
								{
									// FOR, but expecting get_iter followed by for_iter
									if (next->second.Loopevent._kind != BGNode_loopevent::loop_getiter)
										ThrowInternalStackError();
									auto getiter = next;

									// consume getiter
									++next;
									if (next->second._kind != BGNode_type::loopevent)
										ThrowInternalStackError();
									if (next->second.Loopevent._kind != BGNode_loopevent::loop_foriter)
										ThrowInternalStackError();
									auto foriter = next;

									auto looploc = next->second._location._location;
									auto bodystart = next; ++bodystart;
									auto subseq = bodystart;
									while (BGNodeLocation::ispartof(looploc, subseq->second._location._location)) ++subseq;
									--subseq ;

									// here, should be loopevent::end
									if (subseq->second._kind != BGNode_type::loopevent)
										ThrowInternalStackError();
									if (subseq->second.Loopevent._kind != BGNode_loopevent::loop_end)
										ThrowInternalStackError();

									// subseq shall be loopend => don't take it
									--subseq;

									std::vector<stmt_ty> body = ExtractBlock(bodystart->first, subseq->first);

									// go back to loopend
									++subseq;
									// go to next iteration == consume loopend
									++subseq; 
									// extract orelse (needs testing)
									auto orelsestart = subseq;
									auto orelseloc = orelsestart->second._location;

									subseq = _branchinggraph.lower_bound(loopfullend);
									--subseq;

									std::vector<stmt_ty> orelse = ExtractBlock(orelsestart->first, subseq->first);

									// build the statement
									stmt_ty forstmt = alloc<stmt_ty>();
									forstmt->lineno = current->second._sourceline;
									forstmt->kind = _stmt_kind::For_kind;
									forstmt->v.For.target = foriter->second.Loopevent.ForIter._target;
									forstmt->v.For.iter = getiter->second.Loopevent.GetIter._range;
									forstmt->v.For.body = BuildASDLSequence(body);
									forstmt->v.For.orelse = BuildASDLSequence(orelse);

									// ==== STMT ====
									result.push_back(forstmt);
									// ==== STMT ====

									index = (++subseq)->first;
									continue ;
								}
								else
									ThrowNotImplemented();
							}
						default:
							ThrowInternalStackError();
					}
				}
			default:
				ThrowInternalStackError();
		}
	}
	#endif

	return result;
}

stmt_ty Disassembler::GetFunctionDeclStatement()
{
	stmt_ty func = alloc<stmt_ty>();
	func->lineno = _firstlineno;
	func->kind = _stmt_kind::FunctionDef_kind;

	arguments_ty args = alloc<arguments_ty>();
	func->v.FunctionDef.args = args;
	if (_argcount != 0)
	{
		args->args = alloc_asdl_seq(_argcount);
		for (Py_ssize_t k = 0; k < args->args->size; ++k)
		{
			arg_ty arg = alloc<arg_ty>();
			arg->annotation = nullptr; // CAVEAT : cannot recover annotations from compiled code !
			arg->lineno = _firstlineno;
			arg->arg = _varnames[k];
			args->args->elements[k] = arg;
		}
	}
	else
		args->args = nullptr;
	args->defaults = nullptr; // cannot have defaults here
	args->kwarg = nullptr;
	args->kwonlyargs = nullptr;
	args->kw_defaults = nullptr;
	args->vararg = nullptr;

	std::vector<stmt_ty> block = ExtractBlock(_branchinggraph.begin()->first, _branchinggraph.rbegin()->first);

	// check if last statement is return none, in which case, remove statement (to fit with inspect for comparisons...)
	if ((block.back()->kind == _stmt_kind::Return_kind) &&
		(block.back()->v.Return.value->kind == _expr_kind::Constant_kind) &&
		(block.back()->v.Return.value->v.Constant.value == Py_None))
	{
		block.pop_back();
	}

	func->v.FunctionDef.body = BuildASDLSequence(block);

	func->v.FunctionDef.name = _PyUnicode_FromASCII(_name.c_str(), _name.size());
	func->v.FunctionDef.returns = nullptr;

	return func;
}


#pragma endregion
#endif