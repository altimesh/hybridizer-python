#pragma once

[[noreturn]] extern void ThrowNotImplemented();

#include <string>
#include <map>

struct HybType
{
	std::string _name;
	int _scalarIndex;

	inline HybType() {}
	inline HybType(std::string name, int scalarindex = -1) 
	{
		_name = name;
		_scalarIndex = scalarindex;
	}

	template <typename T>
	static HybType* builtin();
	static HybType* builtinfunction();
	static HybType* forge(std::string name);
	static inline HybType* getthing() { return &thing; }

	#pragma region python builtin types

private:
	static HybType thing;
	static HybType logical;
	static HybType numeric_int;
	static HybType numeric_long;
	static HybType numeric_ulong;
	static HybType numeric_float;
	static HybType numeric_complex;
	static HybType str;

	static std::map<std::string, HybType*> sm_generated_builtins;
	static std::map<std::string, HybType*> sm_forged;

public:

	#pragma endregion

	static inline HybType* scalar(int i) {
		HybType res;
		res._scalarIndex = i;
		switch (i)
		{
		case 1:
			return &numeric_int;
		case 2:
			return &numeric_float;
		case 3:
			return &numeric_complex;
		default:
			ThrowNotImplemented();
		}
	}

	inline bool IsScalar() const
	{
		return _scalarIndex > 0;
	}

	inline bool IsString() const 
	{
		if (_name == "hybpython::string") return true;
		if (_name == "const char*") return true;
		if (_scalarIndex != -1) return false;
		ThrowNotImplemented();
		return false;
	}

	bool operator==(const HybType& a) const { return _name == a._name; }
	bool operator!=(const HybType& a) const { return _name != a._name; }

	inline int ScalarIndex() const
	{
		return _scalarIndex;
	}

private:
	static HybType* generatedbuiltin(std::string name, int scalarindex);
};

struct complex
{
	double real;
	double imag;
};


