// (c) ALTIMESH 2018 -- all rights reserved
// (c) ALTIMESH 2018 -- all rights reserved
#pragma once

// Python version : behavior is different between 2.x and 3.x, but there may be more
#ifndef PYTHON_VERSION_MAJOR
#define PYTHON_VERSION_MAJOR 3
#endif

#ifndef PYTHON_VERSION_MINOR
#define PYTHON_VERSION_MINOR 6
#endif

#ifndef hyb_device // hybridizer.cuda.cuh may already be included !

#if defined(__CUDACC__)
#ifndef hyb_device
#define hyb_inline __forceinline__ 

#define hyb_constant __constant__
#if defined(HYBRIDIZER_NO_HOST)
#define hyb_host
#define	hyb_device  __device__  
#else
#define hyb_host __host__
#define	hyb_device  __device__   
#endif
#endif
#else
#ifndef hyb_device
#define hyb_inline inline 
#define hyb_device
#define hyb_constant 
#endif
#endif

#if defined(__CUDACC_RTC__)
#define HYBRIDIZER_NO_HOST
#endif

#endif


#if defined(HYBRIDIZER_NO_HOST)

#define hyb_hd __device__
typedef long long int int64_t;
typedef long long unsigned uint64_t;

#else

#ifndef hyb_hd
#define hyb_hd __host__ __device__ 
#endif

#include <cstdint>
#include <cstdio>
#include <cmath>

#endif

/// use those instead of std::traits, because std:: is not supported by nvrtc
#pragma region traits
namespace hybridizer
{
#pragma region integral_constant
	template<class T, T v>
	struct integral_constant {
		static constexpr T value = v;
		typedef T value_type;
		typedef integral_constant type;
	};

	struct false_type : integral_constant<bool, false> {};
	struct true_type : integral_constant<bool, false> {};
#pragma endregion
#pragma region arithmetic
	template<typename T> struct is_arithmetic { const static bool value = false; };
	template<> struct is_arithmetic<float> { const static bool value = true; };
	template<> struct is_arithmetic<double> { const static bool value = true; };
	template<> struct is_arithmetic<char> { const static bool value = true; };
	template<> struct is_arithmetic<unsigned char> { const static bool value = true; };
	template<> struct is_arithmetic<short> { const static bool value = true; };
	template<> struct is_arithmetic<unsigned short> { const static bool value = true; };
	template<> struct is_arithmetic<int> { const static bool value = true; };
	template<> struct is_arithmetic<unsigned int> { const static bool value = true; };
	template<> struct is_arithmetic<long> { const static bool value = true; };
	template<> struct is_arithmetic<unsigned long> { const static bool value = true; };
	template<> struct is_arithmetic<long long> { const static bool value = true; };
	template<> struct is_arithmetic<unsigned long long> { const static bool value = true; };
#pragma endregion
#pragma region integral
	template<typename T> struct is_integral { const static bool value = false; };
	template<> struct is_integral<char> { const static bool value = true; };
	template<> struct is_integral<unsigned char> { const static bool value = true; };
	template<> struct is_integral<short> { const static bool value = true; };
	template<> struct is_integral<unsigned short> { const static bool value = true; };
	template<> struct is_integral<int> { const static bool value = true; };
	template<> struct is_integral<unsigned int> { const static bool value = true; };
	template<> struct is_integral<long> { const static bool value = true; };
	template<> struct is_integral<unsigned long> { const static bool value = true; };
	template<> struct is_integral<long long> { const static bool value = true; };
	template<> struct is_integral<unsigned long long> { const static bool value = true; };
#pragma endregion
#pragma region enable_if
	template<bool val, typename T = void> struct enable_if {};
	template<typename T> struct enable_if<true, T> { typedef T type; };
#pragma endregion
#pragma region remove_cv
	template<typename T> struct remove_const { typedef T type; };
	template<typename T> struct remove_const<const T> { typedef T type; };
	template<typename T> struct remove_volatile { typedef T type; };
	template<typename T> struct remove_volatile<volatile T> { typedef T type; };
	template<typename T>
	struct remove_cv { typedef typename remove_volatile<typename remove_const<T>::type>::type type; };
#pragma endregion
#pragma region is_pointer
	template<typename>
	struct is_pointer { static const bool value = false; };
	template<typename T>
	struct is_pointer<T*> { static const bool value = true; };
	template<typename T>
	struct is_pointer<T* const> { static const bool value = true; };
	template<typename T>
	struct is_pointer<T* volatile> { static const bool value = true; };
	template<typename T>
	struct is_pointer<T* const volatile> { static const bool value = true; };
#pragma endregion

#pragma region remove_pointer
	template<typename T> struct remove_pointer { typedef T type; };
	template<typename T> struct remove_pointer<T*> { typedef T type; };
	template<typename T> struct remove_pointer<T* const> { typedef T type; };
	template<typename T> struct remove_pointer<T* volatile> { typedef T type; };
	template<typename T> struct remove_pointer<T* const volatile> { typedef T type; };
#pragma endregion

#pragma region dereference
	template<typename T>
	struct dereference_helper { static hyb_inline hyb_hd T take(T val) { return val; } };
	template<typename T>
	struct dereference_helper<T*> { static hyb_inline hyb_hd T take(T* val) { return *val; } };
	template<typename T>
	struct dereference_helper<T* const> { static hyb_inline hyb_hd T take(T* val) { return *val; } };
	template<typename T>
	struct dereference_helper<T* volatile> { static hyb_inline hyb_hd T take(T* val) { return *val; } };
	template<typename T>
	struct dereference_helper<T* const volatile> { static hyb_inline hyb_hd T take(T* val) { return *val; } };

	template <typename T>
	static hyb_hd hyb_inline typename remove_pointer<T>::type dereference(T val) { return dereference_helper<T>::take(val); }
#pragma endregion
}
#pragma endregion

class conststr {
	const char* p;
	size_t sz;
public:
	template<size_t N>
	hyb_hd hyb_inline conststr(const char(&a)[N]) : p(a), sz(N - 1) {}

	// constexpr functions signal errors by throwing exceptions
	// in C++11, they must do so from the conditional operator ?:
	hyb_hd hyb_inline char operator[](size_t n) const
	{
		return n < sz ? p[n] : '\0';
	}
	hyb_hd hyb_inline size_t size() const { return sz; }

	hyb_hd hyb_inline bool equals(conststr a) const
	{
		if (sz != a.size())
			return false;
		if (sz == 0)
			return true;
		for (size_t i = 0; i < sz; ++i)
			if (p[i] != a[i])
				return false;
		return true;
	}
};


template<typename T, typename E = void>
struct pythonview {};


template <typename T>
struct pythonview<T, typename hybridizer::enable_if<!hybridizer::is_arithmetic<T>::value && !hybridizer::is_pointer<T>::value>::type>
{
	T* inner;
	template <typename U> hyb_hd pythonview<U> get_attribute(conststr atr);
	template <typename U> hyb_hd void set_attribute(conststr atr, pythonview<U> value);
	hyb_hd pythonview(T* d) { inner = d; }
	hyb_hd operator T*() { return inner; }
	hyb_hd T* operator()() { return inner; }
};

template <typename T>
struct pythonview<T, typename hybridizer::enable_if<hybridizer::is_arithmetic<T>::value>::type>
{
	T inner;
	hyb_hd pythonview(T d) { inner = d; }
	hyb_hd operator T() { return inner; }
	hyb_hd T operator()() { return inner; }
};

template <typename T>
struct pythonview<T, typename hybridizer::enable_if<hybridizer::is_pointer<T>::value>::type>
{
	T inner;
	template <typename U> hyb_hd pythonview<U> get_attribute(conststr atr);
	template <typename U> hyb_hd void set_attribute(conststr atr, pythonview<U> value);
	hyb_hd pythonview(T d) { inner = d; }
	hyb_hd operator T() { return inner; }
	hyb_hd T operator()() { return inner; }
};

template <typename T> hyb_inline hyb_device pythonview<T> hybtrap();

#pragma endregion


namespace hybridizer 
{
	namespace python
	{
		#pragma region integral
		template<typename T> struct is_integral { const static bool value = false; };
		template<> struct is_integral<char> { const static bool value = true; };
		template<> struct is_integral<unsigned char> { const static bool value = true; };
		template<> struct is_integral<short> { const static bool value = true; };
		template<> struct is_integral<unsigned short> { const static bool value = true; };
		template<> struct is_integral<int> { const static bool value = true; };
		template<> struct is_integral<unsigned int> { const static bool value = true; };
		template<> struct is_integral<long> { const static bool value = true; };
		template<> struct is_integral<unsigned long> { const static bool value = true; };
		template<> struct is_integral<long long> { const static bool value = true; };
		template<> struct is_integral<unsigned long long> { const static bool value = true; };
		#pragma endregion
		#pragma region is_orderable
		template<typename T> struct is_orderable { const static bool value = false; };
		template<> struct is_orderable<char> { const static bool value = true; };
		template<> struct is_orderable<unsigned char> { const static bool value = true; };
		template<> struct is_orderable<short> { const static bool value = true; };
		template<> struct is_orderable<unsigned short> { const static bool value = true; };
		template<> struct is_orderable<int> { const static bool value = true; };
		template<> struct is_orderable<unsigned int> { const static bool value = true; };
		template<> struct is_orderable<long> { const static bool value = true; };
		template<> struct is_orderable<unsigned long> { const static bool value = true; };
		template<> struct is_orderable<long long> { const static bool value = true; };
		template<> struct is_orderable<unsigned long long> { const static bool value = true; };
		template<> struct is_orderable<float> { const static bool value = true; };
		template<> struct is_orderable<double> { const static bool value = true; };
		#pragma endregion
		#pragma region enable_if
		template<bool val, typename T = void> struct enable_if {};
		template<typename T> struct enable_if<true, T> { typedef T type; };
		#pragma endregion

		[[noreturn]] hyb_hd void TRAP()
		{
			#ifdef __CUDA_ARCH__
			asm("trap;");
			#else
			throw 1;
			#endif
		}

		#pragma region RANGES

		/* USE THIS to avoid __hybrid_range ? => CANNOT because of "break" and "continue" clauses
		template < typename lambda_t, typename range_t, typename iter_t = typename range_t::iter_t >
		hyb_inline hyb_device void for_iterator(range_t range, lambda_t lambda)
		{
			for (iter_t i = range.begin(); range.cont(i); i = range.next(i))
			{
				lambda(i);
			}
		}
		*/

		struct cudagridrange1D
		{
			typedef int iter_t;

			int _start;
			int _N;

			hyb_inline hyb_hd cudagridrange1D(int N)
			{
				_start = 0;
				_N = N;
			}

			hyb_inline hyb_hd cudagridrange1D(int start, int N)
			{
				_start = start;
				_N = N;
			}

			hyb_inline hyb_hd int begin() { return _start + threadIdx.x + blockIdx.x * blockDim.x; }
			hyb_inline hyb_hd int end() { return _N; }
			hyb_inline hyb_hd bool cont(int i) { return (i < _N); }
			hyb_inline hyb_hd int next(int i) { return i + blockDim.x * gridDim.x; }
		};


		struct range
		{
			typedef int iter_t;

			int _start;
			int _N;

			hyb_inline hyb_hd range(int N)
			{
				_start = 0;
				_N = N;
			}

			hyb_inline hyb_hd range(int start, int N)
			{
				_start = start;
				_N = N;
			}

			hyb_inline hyb_hd int begin() { return _start; }
			hyb_inline hyb_hd int end() { return _N; }
			hyb_inline hyb_hd bool cont(int i) { return (i < _N); }
			hyb_inline hyb_hd int next(int i) { return ++i; }
		};

		/* SAMPLE USAGE
		__global__ void addKernel(int N, int *c, const int *a, const int *b)
		{
			// LAMBDA WITH CAPTURE :
			//for_iterator<>(defaultgridrange(N), [=](auto i) { c[i] = a[i] + b[i]; });
			// "OLD" SCHOOL
			auto __hybrid_range = defaultgridrange(N);
			for (auto i = __hybrid_range.begin(); __hybrid_range.cont(i); i = __hybrid_range.next(i))
			{
				c[i] = a[i] + b[i];
			}
		}
		*/

		#pragma endregion

		struct thing;

		#pragma region complex

		struct complex
		{
			double re, im;

			hyb_hd complex() {}

			hyb_hd complex(int a) { re = (double)a; im = 0.0; }
			hyb_hd complex(int64_t a) { re = (double)a; im = 0.0; }
			hyb_hd complex(float a) { re = (double)a; im = 0.0; }
			hyb_hd complex(double a) { re = a; im = 0.0; }

			hyb_hd complex(double r, double i) { re = r; im = i; }

			hyb_hd complex(const thing& r, const thing& i);

			hyb_hd complex conj() const { return complex(re, -im); }
			hyb_hd double real() const { return re; }
			hyb_hd double imag() const { return im; }

			hyb_hd complex operator-() const { return complex(-re, -im); }

			// for .real and .imag => read-only attributes...
			/*
				>>> a = complex(1,2)
				>>> a.real = 12
				Traceback (most recent call last):
				  File "<stdin>", line 1, in <module>
				AttributeError: readonly attribute
				>>> 
			*/
			template<typename T>
			hyb_hd T get_attribute(const conststr& attr) const
			{
				if (attr.equals("real")) return T(re);
				else if (attr.equals("imag")) return T(im);
				else
					TRAP();
			}
		};

		hyb_hd complex operator+(const complex& x, const complex& y) { return complex(x.re + y.re, x.im + y.im); }
		hyb_hd complex operator-(const complex& x, const complex& y) { return complex(x.re - y.re, x.im - y.im); }
		hyb_hd complex operator*(const double& a, const complex& z) { return complex(a * z.re, a * z.im); }
		hyb_hd complex operator*(const complex& x, const complex& y) { return complex(x.re * y.re - x.im * y.im, x.im * y.re + x.re * y.im); }
		hyb_hd complex operator/(const complex& x, const complex& y)
		{
			double factor = 1.0 / (y.re*y.re + y.im*y.im);
			return factor * x * y.conj();
		}
		hyb_hd complex operator/(const complex& x, const double& y)
		{
			double factor = 1.0 / y;
			return factor * x;
		}
		hyb_hd bool operator!=(const complex& a, const complex& b)
		{
			if ((a.re == b.re) && (a.im == b.im)) return false;
			return true;
		}
		hyb_hd bool operator==(const complex& a, const complex& b)
		{
			if ((a.re == b.re) && (a.im == b.im)) return true;
			return false;
		}
		// https://en.wikipedia.org/wiki/Complex_logarithm
		hyb_hd complex log(const complex& a)
		{
			double r = ::sqrt(a.re * a.re + a.im * a.im);
			complex res;
			res.re = ::log(r);
			res.im = ::atan2(a.im, a.re);
			return res;
		}
		// https://en.wikipedia.org/wiki/Exponential_function#Complex_plane
		hyb_hd complex exp(const complex& a)
		{
			complex res;
			double r = ::exp(a.re);
			res.re = r * ::cos(a.im);
			res.im = r * ::sin(a.im);
			return res;
		}

		#pragma endregion

		#pragma region  Math trivia

		template <typename T> hyb_inline hyb_hd T floor(const T& a);

		template <> hyb_inline hyb_hd int floor<int>(const int& a) { return a; }
		template <> hyb_inline hyb_hd int64_t floor<int64_t>(const int64_t& a) { return a; }
		template <> hyb_inline hyb_hd double floor<double>(const double& a) { return ::floor(a); }
		template <> hyb_inline hyb_hd complex floor<complex>(const complex& a) { TRAP(); }

		template <typename T> hyb_inline hyb_hd T fmod(const T& a, const T& b);

		template <> hyb_inline hyb_hd int fmod<int>(const int& a, const int& b) { return a % b; }
		template <> hyb_inline hyb_hd int64_t fmod<int64_t>(const int64_t& a, const int64_t& b) { return a % b; }
		template <> hyb_inline hyb_hd double fmod<double>(const double& a, const double& b) { return ::fmod(a,b); }
		template <> hyb_inline hyb_hd complex fmod<complex>(const complex& a, const complex& b) { TRAP(); }

		#pragma endregion

		#pragma region cast

		typedef int64_t pylong;
		typedef uint64_t pyulong;

		template <typename U, typename V>
		struct cast
		{
			static hyb_hd U from(V v);
		};

		template <typename T>
		struct cast<T, T>
		{
			static hyb_hd T from(T t) { return t; }
		};

		template<> hyb_hd double cast<double, int>::from(int a) { return (double)a; }
		template<> hyb_hd double cast<double, pylong>::from(pylong a) { return (double)a; }

		template<> hyb_hd pylong cast<pylong, int>::from(int a) { return a; }
		template<> hyb_hd int cast<int, pylong>::from(pylong a) { return (int)a; }

		template<> hyb_hd pyulong cast<pyulong, int>::from(int a) { return (pyulong) a; }
		template<> hyb_hd int cast<int, pyulong>::from(pyulong a) { return (int)a; }

		template<> hyb_hd pyulong cast<pyulong, unsigned int>::from(unsigned int a) { return (pyulong)a; }
		template<> hyb_hd unsigned int cast<unsigned int, pyulong>::from(pyulong a) { return (unsigned int)a; }
		#pragma endregion

		#pragma region thing

		struct pyaccessor;


		struct pydict
		{

		};

		// https://docs.python.org/3/library/stdtypes.html
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
			py_lambda  = 0x800000, // lambda may have capture
		};

		struct pylist;

		struct pyfuncptr
		{
			void* ptr;
		};

		struct pylambda
		{
			void* ptr;
			pylist* capture;
		};

		template <typename index_t, typename value_t>
		struct pylist_accessor
		{
			static hyb_hd void set(const pylist& l, index_t i, const value_t& v);
			static hyb_hd value_t get(const pylist& l, index_t i);
		};

		// pylist is a list of thing
		struct pylist
		{
			int64_t _size;
			thing* data;

			template <typename index_t, typename value_t>
			hyb_hd void set(index_t i, value_t v)
			{
				pylist_accessor<index_t, value_t>::set(*this, i, v);
			}

			template <typename index_t, typename value_t>
			hyb_hd value_t get(index_t i)
			{
				return pylist_accessor<index_t, value_t>::get(*this, i);
			}
		};

		static_assert(sizeof(pylist) == 16, "INCONSISTENT SIZEOF pylist");

		struct numpyndarraybase
		{
			double* data;
		};

		struct npshape
		{
			int64_t stride; // in bytes
			int64_t shape;
		};

		template<int dim>
		struct numpyndarray : numpyndarraybase
		{
			npshape shape[dim];

			template <typename index_t, typename value_t>
			hyb_hd void set(index_t i, value_t v);

			template <typename index_t, typename value_t>
			hyb_hd value_t get(index_t i);

			// 1D offset - TODO : ellipsis for higher dim ?
			template <typename index_t>
			hyb_hd index_t offset(index_t i)
			{
				return (index_t)((shape[0].stride * (index_t)i) / (index_t)8);
			}
		};

		struct thing
		{
			union // __thing_type // for debugging display purposes, we name unions
			{
				pytype _type;
				int64_t _type__padding;
			} ; 

			union // __thing_value // for debugging display purposes, we name unions
			{
				int64_t _int;
				uint64_t _uint;
				double _double;
				complex _complex;
				pydict* _dict;
				pylist* _list;
				pylist* _tuple;
				numpyndarraybase* _npbase;

				int _bool;

				pyfuncptr _func;
				pylambda _lambda;
			} ;

			hyb_hd thing()
			{
				_type = py_none;
			}

			#pragma region Constructors

			#define hyb_explicit explicit

			hyb_hd hyb_explicit thing(bool const& i)
			{
				_type = pytype::py_bool;
				_bool = i ? 1 : 0;
			}

			hyb_hd hyb_explicit thing(int const& i)
			{
				_type = pytype::py_int;
				_int = i;
			}

			hyb_hd hyb_explicit thing(unsigned int const & i)
			{
				_type = pytype::py_int;
				_int = i;
			}

			hyb_hd hyb_explicit thing(pylong const& i)
			{
				_type = pytype::py_int;
				_int = i;
			}

			hyb_hd hyb_explicit thing(pyulong const& i)
			{
				_type = pytype::py_int;
				_uint = i;
			}

			hyb_hd hyb_explicit thing(double const& z)
			{
				_type = pytype::py_double;
				_double = z;
			}

			hyb_hd hyb_explicit thing(complex const& z)
			{
				_type = pytype::py_complex;
				_complex = z;
			}

			hyb_hd hyb_explicit thing(pylist* const& l)
			{
				_type = pytype::py_list;
				_list = l;
			}

			// copy constructor
			hyb_hd thing(thing const& t)
			{
				_type = t._type;
				_complex = t._complex;
			}

			template<int dim>
			hyb_hd hyb_explicit thing(const numpyndarray<dim>*& np)
			{
				_type = (pytype) (pytype::py_numpyndarray_base | (dim & py_numpyndarray_dimmask));
				_npbase = np;
			}

			#undef hyb_explicit

			#pragma endregion

			#pragma region Operator=

			hyb_hd thing& operator=(bool const & i)
			{
				_type = pytype::py_bool;
				_bool = i ? 1 : 0;
				return *this;
			}

			hyb_hd thing& operator=(int const & i)
			{
				_type = pytype::py_int;
				_int = i;
				return *this;
			}

			hyb_hd thing& operator=(unsigned int const & i)
			{
				_type = pytype::py_int;
				_int = i;
				return *this;
			}

			hyb_hd thing& operator=(double const & z)
			{
				_type = pytype::py_double;
				_double = z;
				return *this;
			}

			hyb_hd thing& operator=(complex const & z)
			{
				_type = pytype::py_complex;
				_complex = z;
				return *this;
			}

			hyb_hd thing& operator=(pylist* const & l)
			{
				_type = pytype::py_list;
				_list = l;
				return *this;
			}

			hyb_hd thing& operator=(thing const & t)
			{
				_type = t._type;
				_complex = t._complex;
				return *this;
			}

			template<int dim>
			hyb_hd thing& operator=(numpyndarray<dim>* const & np)
			{
				_type = (pytype)(pytype::py_numpyndarray_base | (dim & py_numpyndarray_dimmask));
				_npbase = np;
				return *this;
			}

			#pragma endregion

			template <typename T>
			__host__ __device__ T as() const;

			hyb_hd thing astype(pytype t) const;

			enum op_t
			{
				add, sub, mul, div, floordiv, mod,
				// compare
				comp_lt, comp_gt, comp_eq, comp_ge, comp_le, comp_neq,
				// integer only
				lshift, rshift, bitAnd, bitOr, bitXor
			};

			template<op_t op, typename T, typename E = void>
			struct operateint;

			template<op_t op, typename T>
			struct operateint <op, T, typename enable_if < is_integral<T>::value >::type >
			{
				static hyb_hd T eval(const T& left, const T& right)
				{
					switch (op)
					{
					case op_t::lshift: return left << right; 
					case op_t::rshift: return left >> right; 
					case op_t::bitXor: return left ^ right;
					case op_t::bitOr: return left | right;
					case op_t::bitAnd: return left & right;

					default:
						TRAP();
					}
				}
			};

			template<op_t op, typename T>
			struct operateint <op, T, typename enable_if < !is_integral<T>::value >::type >
			{
				static hyb_hd T eval(const T& left, const T& right)
				{
					TRAP();
				}
			};

			template<op_t op, typename T>
			hyb_hd static inline T operate(const thing& a, const thing& b, pytype type)
			{
				T left = a.as<T>();
				T right = b.as<T>();

				T val;

				switch (op)
				{
				case op_t::bitXor:
				case op_t::bitOr: 
				case op_t::bitAnd:
				case op_t::lshift:
				case op_t::rshift: 
					val = operateint<op, T>::eval(left, right); 
					break;
				case op_t::add: val = left + right; break;
				case op_t::sub: val = left - right; break;
				case op_t::mul: val = left * right; break;
				case op_t::div: val = left / right; break;
				case op_t::mod: val = fmod<T>(left, right); break;
				case op_t::floordiv: 
				{
					val = floor<T>(left / right);
					break;
				}
				default:
					// TODO : error ?
					break;
				}

				return val;
			}

			template<op_t op>
			hyb_hd static inline thing operateany(const thing& a, const thing& b)
			{
				thing res;
				if ((a._type & py_numericmask) == 0) return res;
				if ((b._type & py_numericmask) == 0) return res;

				pytype type = a._type > b._type ? a._type : b._type;
				switch (type)
				{
				case py_int: 
				{
					#if PYTHON_VERSION_MAJOR==3
					if (op == op_t::div)
					{
						res._type = py_double;
						res._double = operate<op, double>(a, b, py_double);
						return res;
					} else
					#endif
					res._int = operate<op, int>(a, b, type); 
					break;
				}
				case py_double: res._double = operate<op, double>(a, b, type); break;
				case py_complex: res._complex = operate<op, complex>(a, b, type); break;
				default:
					// TODO : error ?
					break;
				}

				res._type = type;

				return res;
			}

			template<op_t op, typename T, typename E = void>
			struct docompare;

			template<op_t op, typename T>
			struct docompare <op, T, typename enable_if < is_orderable<T>::value >::type >
			{
				static hyb_hd bool eval(const thing& a, const thing& b)
				{
					T left = a.as<T>();
					T right = b.as<T>();

					switch (op)
					{
					case op_t::comp_lt: return left < right;
					case op_t::comp_gt: return left > right;
					case op_t::comp_ge: return left >= right;
					case op_t::comp_le: return left <= right;

					case op_t::comp_eq: return left == right;
					case op_t::comp_neq: return left != right;
					default:
						TRAP();
					}
				}
			};

			template<op_t op, typename T>
			struct docompare <op, T, typename enable_if < !is_orderable<T>::value >::type >
			{
				static hyb_hd bool eval(const thing& a, const thing& b)
				{
					T left = a.as<T>();
					T right = b.as<T>();

					switch (op)
					{
					case op_t::comp_eq: return left == right;
					case op_t::comp_neq: return left != right;
					default:
						TRAP();
					}

				}
			};

			template<op_t op>
			hyb_hd static inline bool operatecompare(const thing& a, const thing& b)
			{
				// comparison is also supported for non numeric masks => yet not supported for now
				if ((a._type & py_numericmask) == 0) TRAP();
				if ((b._type & py_numericmask) == 0) TRAP();

				pytype type = a._type > b._type ? a._type : b._type;
				switch (type)
				{
				case py_int: return docompare<op, int>::eval(a, b);
				case py_double: return docompare<op, double>::eval(a, b);
				case py_complex: return docompare<op, complex>::eval(a, b);
				default:
					TRAP();
				}
			}

			hyb_hd inline double real()
			{
				if (_type == pytype::py_complex)
					return _complex.re;
				TRAP();
				return 0.0;
			}

			hyb_hd inline double imag()
			{
				if (_type == pytype::py_complex)
					return _complex.im;
				TRAP();
				return 0.0;
			}

			/*
			
			>>> a = 12
			>>> a += 30.0
			>>> a
			42.0

			*/

			template<typename T> 
			hyb_hd thing operator+=(T const& val)
			{
				*this = *this + val;
				return *this;
			}

			template<typename T>
			hyb_hd thing operator-=(T const& val)
			{
				*this = *this - val;
				return *this;
			}

			template<typename T>
			hyb_hd thing operator*=(T const& val)
			{
				*this = *this * val;
				return *this;
			}

			template<typename T>
			hyb_hd thing operator/=(T const& val)
			{
				*this = *this / val;
				return *this;
			}


			hyb_hd thing operator-() const
			{
				thing res;
				res._type = _type;
				switch (_type)
				{
				case pytype::py_int: res._int = -_int; break;
				case pytype::py_double: res._double = -_double; break;
				case pytype::py_complex: res._complex = -_complex; break;
				default: TRAP();
				}
				return res;
			}

			hyb_hd thing operator~() const
			{
				thing res;
				res._type = _type;
				switch (_type)
				{
				case pytype::py_int: res._int = ~_int; break;
				default: TRAP();
				}
				return res;
			}

			hyb_hd bool operator!() const ;

			// TODO
			hyb_hd void set_field(const char* field, const thing& value);

			hyb_hd pyaccessor get_field(const char* field);


			template<typename T>
			hyb_hd T get_attribute(const conststr& attr) const
			{
				if (attr.equals("real"))
				{
					if (_type == pytype::py_complex)
						return T(_complex.real());
					else
						TRAP();
					return T(re);
				}
				else if (attr.equals("imag"))
				{
					if (_type == pytype::py_complex)
						return T(_complex.imag());
					else
						TRAP();
					return T(im);
				}
				else
					TRAP();
			}

			#pragma region callable -- returns a thing !

			template<typename... args_t>
			hyb_hd thing operator()(args_t... args);

			#pragma endregion
		};

		static_assert(sizeof(thing) == 24, "INCONSISTENT THING SIZE !!!");

		template<>
		hyb_hd bool thing::as<bool>() const
		{
			if (_type == py_bool) return _bool != 0;
			if (_type == py_int) return _int != 0;
			if (_type == py_double) return _double != 0.0;
			if (_type == py_complex) return _complex != complex(0.0,0.0);
			TRAP(); // TODO: something else...
		}

		hyb_hd bool thing::operator!() const
		{
			return !as<bool>();
		}

		template<>
		hyb_hd int thing::as<int>() const
		{
			if (_type == py_int) return (int)_int;
			return 0;
		}

		template<>
		hyb_hd pylong thing::as<pylong>() const
		{
			if (_type == py_int) return (pylong)_int;
			return 0;
		}

		template<>
		hyb_hd pyulong thing::as<pyulong>() const
		{
			if (_type == py_int) return (pyulong)_int;
			return 0;
		}

		template<>
		hyb_hd double thing::as<double>() const
		{
			if (_type == py_int) return (double)_int;
			if (_type == py_double) return _double;
			return 0.0;
		}

		template<>
		hyb_hd complex thing::as<complex>() const
		{
			if (_type == py_int) return complex(_int);
			if (_type == py_double) return complex(_double);
			if (_type == py_complex) return _complex;
			return 0.0;
		}

		// used for capture parameter forwarding...
		template<>
		hyb_hd hybridizer::python::thing const& thing::as< hybridizer::python::thing const&>() const
		{
			return *this;
		}

		template<>
		hyb_hd thing thing::as<thing>() const
		{
			return *this;
		}

		template<> hyb_hd pylong cast<pylong, thing>::from(thing a) { return a.as<pylong>(); }
		template<> hyb_hd int cast<int, thing>::from(thing a) { return a.as<int>(); }
		template<> hyb_hd double cast<double, thing>::from(thing a) { return a.as<double>(); }

		template<> hyb_hd thing cast<thing, pylong >::from(pylong a) { return thing(a); }
		template<> hyb_hd thing cast<thing, int    >::from(int    a) { return thing(a); }
		template<> hyb_hd thing cast<thing, double >::from(double a) { return thing(a); }


		// TODO
		hyb_hd int hybstrcmp(const char* a, const char* b) { return 0; }


		hyb_hd void thing::set_field(const char* field, const thing& value)
		{
			if (hybstrcmp(field, "real") == 0)
			{
				_complex.re = value.as<double>();
				return;
			}
			TRAP();
		}

		hyb_hd thing thing::astype(pytype t) const
		{
			thing res;
			res._type = t;
			switch (t)
			{
			case py_int: res._int = as<int>(); break;
			case py_double: res._double = as<double>(); break;
			case py_complex: res._complex = as<complex>(); break;
			default: // TODO ??
				break;
			}
			return res;
		}

		#pragma region pylist set/get

		template <>
		struct pylist_accessor<thing, thing>
		{
			static hyb_hd void set(const pylist& l, thing i, const thing& v) { l.data[i.as<int64_t>()] = v; }
			static hyb_hd thing get(const pylist& l, thing i) { return l.data[i.as<int64_t>()]; }
		};

		template <typename value_t>
		struct pylist_accessor<thing, value_t>
		{
			static hyb_hd void set(const pylist& l, thing i, const value_t& v) 
			{ 
				l.data[i.as<int64_t>()] = thing(v); 
			}
			static hyb_hd thing get(const pylist& l, thing i) 
			{ 
				thing res = l.data[i.as<int64_t>()];
				return res.as<value_t>(); 
			}
		};

		template <typename index_t>
		struct pylist_accessor<index_t, thing>
		{
			static hyb_hd void set(const pylist& l, index_t i, const thing& v) { l.data[i] = v; }
			static hyb_hd thing get(const pylist& l, index_t i) { return l.data[i]; }
		};

		template <typename index_t, typename value_t>
		hyb_hd void pylist_accessor<index_t, value_t>::set(const pylist& l, index_t i, const value_t& v)
		{
			l.data[i] = thing(v);
		}

		template <typename index_t, typename value_t>
		hyb_hd value_t pylist_accessor<index_t, value_t>::get(const pylist& l, index_t i)
		{
			thing res = l.data[i];
			return res.as<value_t>();
		}

		#pragma endregion

		#pragma region npndarray set/get

		template<> template<> hyb_hd void numpyndarray<1>::set<pylong, thing>(pylong i, thing v)
		{
			data[offset(i)] = v._double;
		}

		template<> template<> hyb_hd thing numpyndarray<1>::get<pylong, thing>(pylong i)
		{
			return thing(data[offset(i)]);
		}

		template<> template<> hyb_hd void numpyndarray<1>::set<int, thing>(int i, thing v)
		{
			data[offset(i)] = v._double;
		}

		template<> template<> hyb_hd void numpyndarray<1>::set<int, int>(int i, int v)
		{
			data[offset(i)] = v ;
		}

		template<> template<> hyb_hd thing numpyndarray<1>::get<int, thing>(int i)
		{
			return thing(data[offset(i)]);
		}

		template<> template<> hyb_hd void numpyndarray<1>::set<thing, thing>(thing i, thing v)
		{
			data[offset(i.as<int64_t>())] = v._double;
		}

		template<> template<> hyb_hd thing numpyndarray<1>::get<thing, thing>(thing i)
		{
			return thing(data[offset(i.as<int64_t>())]);
		}

		template<> template<> hyb_hd void numpyndarray<1>::set<int64_t, int64_t>(int64_t i, int64_t v)
		{
			data[offset(i)] = (double)v;
		}

		template<> template<> hyb_hd int64_t numpyndarray<1>::get<int64_t, int64_t>(int64_t i)
		{
			return (int64_t)(data[offset(i)]);
		}

		#pragma region storing bool
		/*
			>>> import numpy;
			>>> a = numpy.ones(1);
			>>> a[0]
			1.0
			>>> a[0] = True
			>>> a[0]
			1.0
			>>> a[0] = False
			>>> a[0]
			0.0
		*/
		template<> template<> hyb_hd void numpyndarray<1>::set<pylong, bool>(pylong i, bool v)
		{
			data[offset(i)] = v ? 1.0 : 0.0;
		}

		template<> template<> hyb_hd void numpyndarray<1>::set<int, bool>(int i, bool v)
		{
			data[offset(i)] = v ? 1.0 : 0.0;
		}
		#pragma endregion


		#pragma endregion

		// CANNOT BE PASSED AS A PARAMETER : 
		struct pyaccessor
		{
			thing* obj;
			const char* field;

			hyb_hd void operator=(const thing& value)
			{
				obj->set_field(field, value);
			}
		};

		hyb_hd pyaccessor thing::get_field(const char* field)
		{
			pyaccessor res;
			res.obj = this;
			res.field = field;
			return res;
		}

		// #include <type_traits>

		hyb_hd thing operator+(const thing& a, const thing& b) { return thing::operateany < thing::op_t::add >(a, b); }
		hyb_hd thing operator-(const thing& a, const thing& b) { return thing::operateany < thing::op_t::sub >(a, b); }
		hyb_hd thing operator*(const thing& a, const thing& b) { return thing::operateany < thing::op_t::mul >(a, b); }
		hyb_hd thing operator/(const thing& a, const thing& b) { return thing::operateany < thing::op_t::div >(a, b); }
		hyb_hd thing operator%(const thing& a, const thing& b) { return thing::operateany < thing::op_t::mod >(a, b); }
		hyb_hd thing operator>>(const thing& a, const thing& b) { return thing::operateany < thing::op_t::rshift >(a, b); }
		hyb_hd thing operator<<(const thing& a, const thing& b) { return thing::operateany < thing::op_t::lshift >(a, b); }
		hyb_hd thing operator&(const thing& a, const thing& b) { return thing::operateany < thing::op_t::bitAnd >(a, b); }
		hyb_hd thing operator|(const thing& a, const thing& b) { return thing::operateany < thing::op_t::bitOr >(a, b); }
		hyb_hd thing operator^(const thing& a, const thing& b) { return thing::operateany < thing::op_t::bitXor >(a, b); }
		hyb_hd bool operator<(const thing& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_lt >(a, b); }
		hyb_hd bool operator>(const thing& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_gt >(a, b); }
		hyb_hd bool operator<=(const thing& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_le >(a, b); }
		hyb_hd bool operator>=(const thing& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_ge >(a, b); }
		hyb_hd bool operator==(const thing& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_eq >(a, b); }
		hyb_hd bool operator!=(const thing& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_neq >(a, b); }
		
		hyb_hd bool operator||(const thing& a, const thing& b) { return a.as<bool>() || b.as<bool>(); }
		hyb_hd bool operator&&(const thing& a, const thing& b) { return a.as<bool>() && b.as<bool>(); }

		template <typename T> hyb_hd thing operator+(const thing& a, const T& b) { return thing::operateany < thing::op_t::add >(a, thing(b)); }
		template <typename T> hyb_hd thing operator-(const thing& a, const T& b) { return thing::operateany < thing::op_t::sub >(a, thing(b)); }
		template <typename T> hyb_hd thing operator*(const thing& a, const T& b) { return thing::operateany < thing::op_t::mul >(a, thing(b)); }
		template <typename T> hyb_hd thing operator/(const thing& a, const T& b) { return thing::operateany < thing::op_t::div >(a, thing(b)); }
		template <typename T> hyb_hd thing operator%(const thing& a, const T& b) { return thing::operateany < thing::op_t::mod >(a, thing(b)); }
		template <typename T> hyb_hd thing operator>>(const thing& a, const T& b) { return thing::operateany < thing::op_t::rshift >(a, thing(b)); }
		template <typename T> hyb_hd thing operator<<(const thing& a, const T& b) { return thing::operateany < thing::op_t::lshift >(a, thing(b)); }
		template <typename T> hyb_hd thing operator&(const thing& a, const T& b) { return thing::operateany < thing::op_t::bitAnd >(a, thing(b)); }
		template <typename T> hyb_hd thing operator|(const thing& a, const T& b) { return thing::operateany < thing::op_t::bitOr >(a,  thing(b)); }
		template <typename T> hyb_hd thing operator^(const thing& a, const T& b) { return thing::operateany < thing::op_t::bitXor >(a, thing(b)); }
		template <typename T> hyb_hd bool operator<(const thing& a,  const T& b) { return thing::operatecompare < thing::op_t::comp_lt >(a, thing(b)); }
		template <typename T> hyb_hd bool operator>(const thing& a,  const T& b) { return thing::operatecompare < thing::op_t::comp_gt >(a, thing(b)); }
		template <typename T> hyb_hd bool operator<=(const thing& a, const T& b) { return thing::operatecompare < thing::op_t::comp_le >(a, thing(b)); }
		template <typename T> hyb_hd bool operator>=(const thing& a, const T& b) { return thing::operatecompare < thing::op_t::comp_ge >(a, thing(b)); }
		template <typename T> hyb_hd bool operator==(const thing& a, const T& b) { return thing::operatecompare < thing::op_t::comp_eq >(a, thing(b)); }
		template <typename T> hyb_hd bool operator!=(const thing& a, const T& b) { return thing::operatecompare < thing::op_t::comp_neq >(a, thing(b)); }

		template <typename T> hyb_hd bool operator||(const thing& a, const T& b) { return a.as<bool>() || thing(b).as<bool>(); }
		template <typename T> hyb_hd bool operator&&(const thing& a, const T& b) { return a.as<bool>() && thing(b).as<bool>(); }

		template <typename T> hyb_hd thing operator+(const T& a, const thing& b) { return thing::operateany < thing::op_t::add >(thing(a), b); }
		template <typename T> hyb_hd thing operator-(const T& a, const thing& b) { return thing::operateany < thing::op_t::sub >(thing(a), b); }
		template <typename T> hyb_hd thing operator*(const T& a, const thing& b) { return thing::operateany < thing::op_t::mul >(thing(a), b); }
		template <typename T> hyb_hd thing operator/(const T& a, const thing& b) { return thing::operateany < thing::op_t::div >(thing(a), b); }
		template <typename T> hyb_hd thing operator%(const T& a, const thing& b) { return thing::operateany < thing::op_t::mod >(thing(a), b); }
		template <typename T> hyb_hd thing operator>>(const T& a, const thing& b) { return thing::operateany < thing::op_t::rshift >(thing(a), b); }
		template <typename T> hyb_hd thing operator<<(const T& a, const thing& b) { return thing::operateany < thing::op_t::lshift >(thing(a), b); }
		template <typename T> hyb_hd thing operator&(const T& a, const thing& b) { return thing::operateany < thing::op_t::bitAnd >(thing(a), b); }
		template <typename T> hyb_hd thing operator|(const T& a, const thing& b) { return thing::operateany < thing::op_t::bitOr > (thing(a), b); }
		template <typename T> hyb_hd thing operator^(const T& a, const thing& b) { return thing::operateany < thing::op_t::bitXor >(thing(a), b); }
		template <typename T> hyb_hd bool operator<(const  T& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_lt >(thing(a), b); }
		template <typename T> hyb_hd bool operator>(const  T& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_gt >(thing(a), b); }
		template <typename T> hyb_hd bool operator<=(const T& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_le >(thing(a), b); }
		template <typename T> hyb_hd bool operator>=(const T& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_ge >(thing(a), b); }
		template <typename T> hyb_hd bool operator==(const T& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_eq >(thing(a), b); }
		template <typename T> hyb_hd bool operator!=(const T& a, const thing& b) { return thing::operatecompare < thing::op_t::comp_neq >(thing(a), b); }

		template <typename T> hyb_hd bool operator||(const T& a, const thing& b) { return thing(a).as<bool>() || b.as<bool>(); }
		template <typename T> hyb_hd bool operator&&(const T& a, const thing& b) { return thing(a).as<bool>() && b.as<bool>(); }

		hyb_hd thing operator++(thing& a)
		{
			switch (a._type)
			{
			case pytype::py_int: 
				++a._int; 
				break;
			case pytype::py_double: 
				a._double += 1.0; 
				break;
			case pytype::py_complex: 
				a._complex.re += 1.0; 
				break;
			default: 
				TRAP(); 
				break;
			}
			return a;
		}

		hyb_hd complex::complex(const thing& r, const thing& i)
		{
			re = r.as<double>();
			im = i.as<double>();
		}

		#pragma region indexers

		inline hyb_hd bool isnumpyndarray(pytype t)
		{
			return (t & (~pytype::py_numpyndarray_dimmask)) == pytype::py_numpyndarray_base;
		}

		template <typename array_t, typename index_t, typename value_t>
		struct array_index
		{
			hyb_hd static void set(array_t ar, index_t i, value_t v) 
			{
				ar.set_Item(i, v);
			}

			// TODO: other overloads? 
			hyb_hd static void set(array_t* ar, index_t i, value_t v)
			{
				ar->set_Item(i, v);
			}

			hyb_hd static value_t get(array_t ar, index_t i)
			{
				return ar.get_Item(i);
			}

			hyb_hd static value_t get(array_t* ar, index_t i)
			{
				return ar->get_Item(i);
			}
		};

		template <typename index_t>
		struct array_index<thing, index_t, thing>
		{
			hyb_hd static void set(thing ar, index_t i, thing v)
			{
				if ((ar._type == pytype::py_list) ||
					(ar._type == pytype::py_tuple))
				{
					pylist l = *(ar._list);
					l.set<index_t, thing>(i, v);
				}
				else if (isnumpyndarray(ar._type))
				{
					numpyndarray<1> npar = *((numpyndarray<1>*)(ar._npbase));
					thing converted = (pytype::py_double != v._type) ? v.astype(pytype::py_double) : v;
					npar.set<index_t, thing>(i, converted);
				}
				else
					TRAP();
			}
			hyb_hd static thing get(thing ar, index_t i)
			{
				if ((ar._type == pytype::py_list) ||
					(ar._type == pytype::py_tuple))
				{
					pylist l = *(ar._list);
					return l.get<index_t, thing>(i);
				}
				else if (isnumpyndarray(ar._type))
				{
					numpyndarray<1> npar = *((numpyndarray<1>*)(ar._npbase));
					return npar.get<index_t, thing>(i);
				}
				else
					TRAP();
				return thing(0);
			}
		};

		template <typename index_t, typename value_t>
		struct array_index<thing, index_t, value_t>
		{
			hyb_hd static void set(thing ar, index_t i, value_t v)
			{
				if ((ar._type == pytype::py_list) ||
					(ar._type == pytype::py_tuple))
				{
					pylist l = *(ar._list);
					l.set<index_t, value_t>(i, v);
				}
				else if (isnumpyndarray(ar._type))
				{
					numpyndarray<1> npar = *((numpyndarray<1>*)(ar._npbase));
					npar.set<index_t, value_t>(i, v);
				}
				else
					TRAP();
			}
			hyb_hd static value_t get(thing ar, index_t i)
			{
				if ((ar._type == pytype::py_list) ||
					(ar._type == pytype::py_tuple))
				{
					pylist l = *(ar._list);
					return l.get<index_t, value_t>(i);
				}
				else if (isnumpyndarray(ar._type))
				{
					numpyndarray<1> npar = *((numpyndarray<1>*)(ar._npbase));
					return npar.get<index_t, value_t>(i);
				}
				else
					TRAP();
				return thing(0);
			}
		};

		template <typename array_t, typename index_t, typename value_t>
		struct index_helper 
		{
			static hyb_inline hyb_hd void set(array_t ar, index_t i, value_t v)
			{
				array_index<array_t, index_t, value_t>::set(ar, i, v);
			}

			static hyb_inline hyb_hd pythonview<value_t> get(const array_t ar, index_t i)
			{
				return array_index<array_t, index_t, value_t>::get(ar, i);
			}
		};

		template <typename array_t, typename index_t, typename value_t>
		struct index_helper< pythonview<array_t>, pythonview<index_t>, pythonview<value_t>>
		{
			static hyb_inline hyb_hd void set(pythonview<array_t> ar, pythonview<index_t> i, pythonview<value_t> v)
			{
				array_index<array_t, index_t, value_t>::set(ar.inner, i.inner, v.inner);
			}

			static hyb_inline hyb_hd pythonview<value_t> get(pythonview<array_t> ar, pythonview<index_t> i)
			{
				return array_index<array_t, index_t, value_t>::get(ar.inner, i.inner);
			}
		};

		template <typename array_t, typename index_t, typename value_t>
		struct index_helper < pythonview<array_t>, index_t, pythonview<value_t>>
		{
			static hyb_inline hyb_hd void set(pythonview<array_t> ar, index_t i, pythonview<value_t> v)
			{
				array_index<array_t, index_t, value_t>::set(ar.inner, i, v.inner);
			}

			static hyb_inline hyb_hd pythonview<value_t> get(pythonview<array_t> ar, index_t i)
			{
				return array_index<array_t, index_t, value_t>::get(ar.inner, i);
			}
		};

		template <typename array_t, typename index_t, typename value_t>
		struct index_helper< pythonview<array_t>, pythonview<index_t>, value_t>
		{
			static hyb_inline hyb_hd void set(pythonview<array_t> ar, pythonview<index_t> i, value_t v)
			{
				array_index<array_t, index_t, value_t>::set(ar.inner, i.inner, v);
			}

			static hyb_inline hyb_hd pythonview<value_t> get(pythonview<array_t> ar, pythonview<index_t> i)
			{
				return array_index<array_t, index_t, value_t>::get(ar.inner, i.inner);
			}
		};

		template <typename array_t, typename index_t, typename value_t>
		struct index_helper< pythonview<array_t>, index_t, value_t>
		{
			static hyb_inline hyb_hd void set(pythonview<array_t> ar, index_t i, value_t v)
			{
				array_index<array_t, index_t, value_t>::set(ar.inner, i, v);
			}

			static hyb_inline hyb_hd pythonview<value_t> get(pythonview<array_t> ar, index_t i)
			{
				return array_index<array_t, index_t, value_t>::get(ar.inner, i);
			}
		};


		template<typename arg_t>
		hyb_inline hyb_hd arg_t ternary(thing test, arg_t a, arg_t b)
		{
			return test.as<bool>() ? a : b;
		}

		#pragma endregion

		#pragma endregion

		#pragma region compare

		template <typename op>
		struct compare_op
		{
			template <typename T, typename U>
			static hyb_inline hyb_hd bool compare(T t, U u) { return op::compare(t, u); }
		};

		namespace compareops
		{
			template <typename T, typename U>
			struct gt
			{
				static hyb_inline hyb_hd bool compare(const T& a, const U& b)
				{
					return a > b;
				}
			};

			template <typename T, typename U>
			struct gte
			{
				static hyb_inline hyb_hd bool compare(const T& a, const U& b)
				{
					return a >= b;
				}
			};

			template <typename T, typename U>
			struct lt
			{
				static hyb_inline hyb_hd bool compare(const T& a, const U& b)
				{
					return a < b;
				}
			};

			template <typename T, typename U>
			struct lte
			{
				static hyb_inline hyb_hd bool compare(const T& a, const U& b)
				{
					return a <= b;
				}
			};

			template <typename T, typename U>
			struct eq
			{
				static hyb_inline hyb_hd bool compare(const T& a, const U& b)
				{
					return a == b;
				}
			};

			template <typename T, typename U>
			struct noteq
			{
				static hyb_inline hyb_hd bool compare(const T& a, const U& b)
				{
					return a != b;
				}
			};
		}

		#pragma endregion

		#pragma region unaryops

		template <typename op>
		struct unary_op
		{
			template <typename T>
			static hyb_inline hyb_hd typename op::result_t eval(T t) { return op::eval(t); }
		};

		namespace unaryops
		{
			// plus is no-op... safe for type checking...
			template <typename T>
			struct minus
			{
				typedef T result_t;
				static hyb_inline hyb_hd T eval(const T& a)
				{
					return -a;
				}
			};
			template <typename T>
			struct invert
			{
				typedef T result_t;
				static hyb_inline hyb_hd T eval(const T& a)
				{
					return ~a;
				}
			};

			// not yields True or False, hence a boolean
			// https://docs.python.org/3.6/reference/expressions.html#boolean-operations
			template <typename T>
			struct not_op
			{
				typedef bool result_t;
				static hyb_inline hyb_hd bool eval(const T& a)
				{
					return !a;
				}
			};
		}

		#pragma endregion

		#pragma region binaryops

		template <typename op>
		struct binary_op
		{
			template <typename T, typename U>
			static hyb_inline hyb_hd typename op::result_t eval(T t, U u) { return op::eval(t, u); }
		}; 

		/*typedef enum _operator {
			Add = 1, Sub = 2, Mult = 3, MatMult = 4, Div = 5, Mod = 6, Pow = 7,
			LShift = 8, RShift = 9, BitOr = 10, BitXor = 11, BitAnd = 12,
			FloorDiv = 13
		} operator_ty; */
		namespace binaryops
		{
			template <typename T, typename U, typename V>
			struct add
			{
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return a + b;
				}
			};

			template <typename T, typename U, typename V>
			struct sub
			{
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return a - b;
				}
			};

			template <typename T, typename U, typename V>
			struct mul
			{
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return a * b;
				}
			};

			template <typename T, typename U, typename V>
			struct div
			{
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return a / b;
				}
			};

			// needed for Python 3 behavior
			template <> hyb_inline hyb_hd double div<int, int, double>::eval(const int& a, const int& b)
			{
				return (double)a / (double)b;
			}
			template <> hyb_inline hyb_hd double div<int64_t, int, double>::eval(const int64_t& a, const int& b)
			{
				return (double)a / (double)b;
			}
			template <> hyb_inline hyb_hd double div<int, int64_t, double>::eval(const int& a, const int64_t& b)
			{
				return (double)a / (double)b;
			}

			template <typename T, typename U, typename V>
			struct floordiv
			{
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return floor<V>(a / b);
				}
			};

			template <typename T, typename U, typename V>
			struct mod
			{
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return a % b;
				}
			};

			template <typename T, typename U, typename V>
			struct pow;

			template <>
			struct pow<double, double, double> {
				typedef double result_t;
				static hyb_inline hyb_hd double eval(const double& a, const double& b)
				{
					return ::pow(a, b);
				}
			};

			template <>
			struct pow<float, float, float> {
				typedef float result_t;
				static hyb_inline hyb_hd float eval(const float& a, const float& b)
				{
					return ::powf(a, b);
				}
			};

			template <typename T, typename U, typename V>
			struct lshift
			{
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return a << b;
				}
			};

			template <typename T, typename U, typename V>
			struct rshift
			{
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return a >> b;
				}
			};

			template <typename T, typename U, typename V>
			struct bitOr {
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return a | b;
				}
			};

			template <typename T, typename U, typename V>
			struct bitXor {
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return a ^ b;
				}
			};

			template <typename T, typename U, typename V>
			struct bitAnd
			{
				typedef V result_t;
				static hyb_inline hyb_hd V eval(const T& a, const U& b)
				{
					return a & b;
				}
			};
		}

		#pragma region operators on thing

		namespace binaryops
		{
			/*
			py_none = 0x0,

			py_int = 0x1,
			py_double = 0x2,
			py_complex = 0x3,

			py_numericmask = 0xF,

			*/
			template <>
			struct pow<thing, thing, thing> {
				// https://docs.python.org/3/reference/expressions.html#the-power-operator
				typedef thing result_t;
				static hyb_inline hyb_hd thing eval(const thing& a, const thing& b)
				{
					// depends on type -- both ints
					pytype enclosing = (pytype)(a._type > b._type ? a._type : b._type);
					if (enclosing == pytype::py_int)
					{
						thing res;
						if (b._int < 0)
						{
							res._type = pytype::py_double;
							res._double = ::pow(a.as<double>(), b.as<double>());
							return res;
						}
						else
						{
							res._type = pytype::py_int;
							// TODO : use other implem ?
							res._int = (int64_t)::pow(a.as<double>(), b.as<double>());
							return res;
						}
					}
					else if (enclosing == pytype::py_double)
					{
						thing res;
						res._type = pytype::py_double;
						res._double = ::pow(a.as<double>(), b.as<double>());
						return res;
					}
					else if (enclosing == pytype::py_complex)
					{
						// TODO : special case for int-only ? -- precision issues...
						thing res;
						res._type = pytype::py_complex;
						res._complex = exp(b.as<complex>() * log(a.as<complex>()));
						return res;
					}
					else
						TRAP();
				}
			};
		}

		#pragma endregion

		
		#pragma region Math functions on thing

		template<> hyb_inline hyb_hd thing floor(const thing& a)
		{
			thing res;
			res._type = a._type;
			switch (a._type)
			{
			case py_int: res._int = a._int; break;
			case py_double: res._double = ::floor(a._double); break;
			default:
				TRAP();
			}
			return res;
		}

		template<> hyb_inline hyb_hd thing fmod(const thing& a, const thing& b)
		{
			return thing::operateany<thing::op_t::mod>(a, b);
		}

		namespace binaryops
		{
			// TODO : may need to also write other type combinations when type information is actually inferred
			template <>
			struct floordiv<thing, thing, thing>
			{
				typedef thing result_t;
				static hyb_inline hyb_hd thing eval(const thing& a, const thing& b)
				{
					return thing::operateany<thing::op_t::floordiv>(a, b);
				}
			};
		}

		#pragma endregion

		#pragma endregion

		#pragma region assignops
		namespace assignops {
			template<typename T, typename U>
			struct add {
				static hyb_inline hyb_hd void eval(T& t, const U& u) {
					t += u;
				}
			};
			template<typename T, typename U>
			struct sub {
				static hyb_inline hyb_hd void eval(T& t, const U& u) {
					t -= u;
				}
			};
			template<typename T, typename U>
			struct mul {
				static hyb_inline hyb_hd void eval(T& t, const U& u) {
					t *= u;
				}
			};

			template<typename T, typename U>
			struct div {
				static hyb_inline hyb_hd void eval(T& t, const U& u) {
					t /= u;
				}
			};

			template<typename T, typename U>
			struct floordiv {
				static hyb_inline hyb_hd void eval(T& t, const U& u) {
					t = hybridizer::python::binaryops::floordiv<T, U, T>(t, u);
				}
			};

			template<typename T, typename U>
			struct mod {
				static hyb_inline hyb_hd void eval(T& t, const U& u) {
					t %= u;
				}
			};
		}

		template <typename op>
		struct assign_op
		{
			template <typename T, typename U>
			static hyb_inline hyb_hd void eval(T& t, const U& u) { return op::eval(t, u); }
		};
		#pragma endregion

		#pragma	region ternary

		// needed to prevent type conversion issues
		template<typename arg_t>
		hyb_inline hyb_hd arg_t ternary(bool test, arg_t a, arg_t b)
		{
			return test ? a : b ;
		}

		#pragma endregion

		#pragma region print

		template<typename... args>
		struct print_helper {
			static hyb_inline hyb_hd void print(args... a)
			{
				::printf(a...);
			}
		};


		#pragma endregion

		#pragma region builtin functions


		template<> struct print_helper<double> { static hyb_inline hyb_hd void print(double a) { ::printf("%lf\n", a); } };

		template<typename... args>
		hyb_inline hyb_hd void __builtin_print(args... a)
		{
			print_helper<args...>::print(a...);
		}

		hyb_inline hyb_hd complex __builtin_complex(double re, double im)
		{
			return complex(re,im);
		}

		#pragma endregion

		template<void*>
		struct return_type { };

		#pragma region function pointers and lambdas

		template<int paramcount> struct functortype { };
		template<> struct functortype<0> { typedef thing(func_t)(); };
		template<> struct functortype<1> { typedef thing(func_t)(thing); };
		template<> struct functortype<2> { typedef thing(func_t)(thing, thing); };
		template<> struct functortype<3> { typedef thing(func_t)(thing, thing, thing); };
		template<> struct functortype<4> { typedef thing(func_t)(thing, thing, thing, thing); };
		template<> struct functortype<5> { typedef thing(func_t)(thing, thing, thing, thing, thing); };

		// TODO : think on how to circumvent the redundancy of hasreturn
		template<bool hasreturn, typename return_t, typename... args_t>
		struct thingified_func;

		template<typename... args_t>
		struct thingified_func<false, void, args_t...>
		{
			typedef void(*func_t)(args_t...);

			/// things_t should be thing for each entry
			template<void(*funcptr)(args_t...), typename... things_t>
			static hyb_hd thing call(things_t... things)
			{
				funcptr(things.template as<args_t>()...);
				thing res;
				res._type = pytype::py_none;
				return res;
			}

			template<void(*funcptr)(args_t...), typename... things_t>
			static hyb_hd thing capturecall(const thing& __closure__, things_t... things)
			{
				funcptr(__closure__, things.template as<args_t>()...);
				thing res;
				res._type = pytype::py_none;
				return res;
			}
		};

		template<typename return_t, typename... args_t>
		struct thingified_func<true, return_t, args_t...>
		{
			// static_assert(std::is_same<return_t, void>::value == false, "GENERATOR ERROR : cannot use this template with void return type");
			typedef void(*func_t)(args_t...);

			/// things_t should be thing for each entry
			template<return_t(*funcptr)(args_t...), typename... things_t>
			static hyb_hd thing call(things_t... things)
			{
				return cast<thing, return_t>::from(funcptr(things.template as<args_t>()...));
			}

			/// things_t should be thing for each entry
			template<return_t(*funcptr)(args_t...), typename... things_t>
			static hyb_hd thing capturecall(things_t... things)
			{
				return cast<thing, return_t>::from(funcptr(things.template as<args_t>()...));
			}
		};

		template<typename... args_t>
		hyb_hd thing thing::operator()(args_t... args)
		{
			switch (_type)
			{
			case pytype::py_lambda:
				{
					thing capture;
					capture._type = py_tuple;
					capture._list = _lambda.capture;
					return ((typename functortype<1+sizeof...(args_t)>::func_t*)(_lambda.ptr))(capture, cast<thing, args_t>::from(args)...);
				}
			case pytype::py_funcptr:
				return ((typename functortype<sizeof...(args_t)>::func_t*)(_func.ptr))(cast<thing,args_t>::from(args)...);
			default:
				TRAP();
			}
		}

		#pragma endregion
	} // namespace python
} // namespace hybridizer


// short naming
#define hybpython hybridizer::python

template<>
struct pythonview < hybpython::thing >
{
	hybpython::thing inner ;
	hyb_hd pythonview(hybpython::thing d) { inner = d; }
	hyb_hd operator hybpython::thing() { return inner; }
	hyb_hd hybpython::thing operator()() { return inner; }
};

namespace hybridizer {
	namespace python {
		template <typename T>
		struct cast<T, pythonview<T>>
		{
			static hyb_hd T from(pythonview<T> pv) {
				return pv.operator T();
			}
		};
	}
}

template<> hyb_hd pythonview<int> hybtrap<int>()
{
	#ifdef __CUDA_ARCH__
	asm("trap;");
	return 0;
#else
	throw 1;
	#endif
}

template <> struct pythonview<hybpython::pylong> {
	hybpython::pylong inner;
	hyb_hd pythonview(hybpython::pylong d) { inner = d; }
	hyb_hd operator hybpython::pylong() { return inner; }
};

template<> hyb_hd pythonview<hybpython::pylong> hybtrap<hybpython::pylong>()
{
#ifdef __CUDA_ARCH__
	asm("trap;");
	return 0L;
#else
	throw 1;
#endif
}


template <> struct pythonview<hybpython::pyulong> {
	hybpython::pyulong inner;
	hyb_hd pythonview(hybpython::pyulong d) { inner = d; }
	hyb_hd operator hybpython::pyulong() { return inner; }
};

template<> hyb_hd pythonview<hybpython::pyulong> hybtrap<hybpython::pyulong>()
{
#ifdef __CUDA_ARCH__
	asm("trap;");
	return 0UL;
#else
	throw 1;
#endif
}

template<> hyb_hd pythonview<double> hybtrap<double>()
{
	#ifdef __CUDA_ARCH__
	asm("trap;");
	return 0.0;
	#else
	throw 1;
	#endif
}


template<> struct hybpython::print_helper<pythonview<double>> { static hyb_inline hyb_hd void print(pythonview<double> a) { ::printf("%lf\n", a()); } };