#include "HybType.h"

HybType HybType::thing = HybType("hybpython::thing", -1);
HybType HybType::logical = HybType("bool", 0);
HybType HybType::numeric_long = HybType("hybpython::pylong", 1);
HybType HybType::numeric_ulong = HybType("hybpython::pyulong", 1);
HybType HybType::numeric_int = HybType("int", 1);
HybType HybType::numeric_float = HybType("double", 2);
HybType HybType::numeric_complex = HybType("hybpython::complex", 3);
HybType HybType::str = HybType("hybpython::string", -1);

std::map<std::string, HybType*> HybType::sm_generated_builtins;
std::map<std::string, HybType*> HybType::sm_forged;

HybType* HybType::generatedbuiltin(std::string name, int scalarindex)
{
	auto found = sm_generated_builtins.find(name);
	if (found == sm_generated_builtins.end())
	{
		HybType* res = new HybType(name, scalarindex);
		sm_generated_builtins[name] = res;
		return res;
	}
	else
		return (*found).second;
}

HybType* HybType::builtinfunction()
{
	return forge("builtinfunction");
}

HybType* HybType::forge(std::string name)
{
	// builtin types
	if (name == numeric_int._name) return &numeric_int;
	if (name == numeric_float._name) return &numeric_float;
	if (name == numeric_complex._name) return &numeric_complex;
	if (name == thing._name) return &thing;
	if (name == logical._name) return &logical;
	if (name == str._name) return &str;

	// builtin type aliases
	if (name == "int") return &numeric_int;
	if (name == "float") return &numeric_float;
	if (name == "complex") return &numeric_complex;
	if (name == "str") return &str;

	// TODO : more
	if (sm_generated_builtins.find(name) != sm_generated_builtins.end())
		return sm_generated_builtins[name];
		
	auto found = sm_forged.find(name);
	if (found == sm_forged.end())
	{
		HybType* res = new HybType(name, -1);
		sm_forged[name] = res;
		return res;
	}
	else
		return (*found).second;
}

// builtin-mapped - https://docs.python.org/3/library/stdtypes.html
template<>  HybType* HybType::builtin<bool>() { return &HybType::logical; }
template<>  HybType* HybType::builtin<int64_t>() { return &HybType::numeric_int; }
template<>  HybType* HybType::builtin<double>() { return &HybType::numeric_float; }
template<>  HybType* HybType::builtin<complex>() { return &HybType::numeric_complex; }

// others
template<>  HybType* HybType::builtin<void>() { return generatedbuiltin("void", -1); }
template<>  HybType* HybType::builtin<void*>() { return generatedbuiltin("void*", -1); }
template<>  HybType* HybType::builtin<const char*>() { return generatedbuiltin("const char*", -1); }

// other scalars
template<>  HybType* HybType::builtin<int>() { return generatedbuiltin("int", 1); }
template<>  HybType* HybType::builtin<float>() { return generatedbuiltin("float", 2); }
