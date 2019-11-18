// MD5Calc.h: interface for the MD5Calc class.

#pragma once


#if !defined(DCPLUSPLUS_DCPP_MD5CALC_H_)
#define DCPLUSPLUS_DCPP_MD5CALC_H_

#include "Util.h"

class MD5Calc
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		char* CalcMD5FromString(const char *s8_Input);
		char* CalcMD5FromFile(const wchar_t *s8_Path);
		MD5Calc();
		
		void MD5Init();
		void MD5Update(unsigned char *buf, unsigned len);
		char* MD5FinalToString();
		
		~MD5Calc();
		
	private:
		struct MD5Context
		{
			unsigned long buf[4];
			unsigned long bits[2];
			unsigned char in[64];
		};
		
		void MD5Final(unsigned char digest[16]);
		
		void MD5Transform(unsigned long buf[4], unsigned long in[16]);
		
		void byteReverse(unsigned char *buf, unsigned longs);
		
		MD5Context m_ctx;
		char ms8_MD5[40]; // Output buffer
};

#endif // !defined(DCPLUSPLUS_DCPP_MD5CALC_H_)

