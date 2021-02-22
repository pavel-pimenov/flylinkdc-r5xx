#ifndef BASE_UTIL_H_
#define BASE_UTIL_H_

#include "typedefs.h"

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#else
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#endif

template<typename T, bool flag> struct ReferenceSelector
{
	typedef T ResultType;
};
template<typename T> struct ReferenceSelector<T, true>
{
	typedef const T& ResultType;
};

template<typename T> class IsOfClassType
{
	public:
		template<typename U> static char check(int U::*);
		template<typename U> static float check(...);
	public:
		enum { Result = sizeof(check<T>(0)) };
};

template<typename T> struct TypeTraits
{
	typedef IsOfClassType<T> ClassType;
	typedef ReferenceSelector < T, ((ClassType::Result == 1) || (sizeof(T) > sizeof(char*))) > Selector;
	typedef typename Selector::ResultType ParameterType;
};

#define GETSET(type, name, name2) \
	private: type name; \
	public: TypeTraits<type>::ParameterType get##name2() const { return name; } \
	void set##name2(TypeTraits<type>::ParameterType a##name2) { name = a##name2; }
#define GETSET_BOOL(type, name, name2) \
	private: type name; \
	public: TypeTraits<type>::ParameterType get##name2() const { return name; } \
	bool set##name2(TypeTraits<type>::ParameterType a##name2) { if(name == a##name2) return false; name = a##name2; return true; }

#define GETM(type, name, name2) \
	private: type name; \
	public: TypeTraits<type>::ParameterType get##name2() const { return name; }

#define GETC(type, name, name2) \
	private: const type name; \
	public: TypeTraits<type>::ParameterType get##name2() const { return name; }

/**
 * Compares two values
 * @return -1 if v1 < v2, 0 if v1 == v2 and 1 if v1 > v2
 */
template<typename T1>
inline int compare(const T1& v1, const T1& v2)
{
	return (v1 < v2) ? -1 : ((v1 == v2) ? 0 : 1);
}

template<typename T>
class AutoArray
{
		typedef T* TPtr;
	public:
#ifdef _DEBUG
		explicit AutoArray(size_t size, char p_fill) : p(new T[size])
		{
			memset(p, p_fill, size);
		}
#endif
		explicit AutoArray(size_t size) : p(new T[size]) { }
		~AutoArray()
		{
			delete[] p;
		}
		operator TPtr()
		{
			return p;
		}
		TPtr data()
		{
			return p;
		}
		
		AutoArray(const AutoArray&) = delete;
		AutoArray& operator= (const AutoArray&) = delete;
		
	private:
		TPtr p;
};

template<class T, size_t N>
class LocalArray
{
	public:
		T m_data[N];
		LocalArray()
		{
			m_data[0]   = 0;
		}
		static size_t size()
		{
			return N;
		}
		T& operator[](size_t p_pos)
		{
			dcassert(p_pos < N);
			return m_data[p_pos];
		}
		const T* data() const
		{
			return m_data;
		}
		T* data()
		{
			return m_data;
		}
		void init()
		{
			memzero(m_data, sizeof(m_data));
		}
		LocalArray(const LocalArray&) = delete;
		LocalArray& operator= (const LocalArray&) = delete;
};

class BaseUtil
{
public:
	static const tstring emptyStringT;
	static const string emptyString;
	static const wstring emptyStringW;
	static const std::vector<uint8_t> emptyByteVector;

	static string translateError(DWORD error);
	static inline string translateError()
	{
		return translateError(GetLastError());
	}
};

#endif // BASE_UTIL_H_
