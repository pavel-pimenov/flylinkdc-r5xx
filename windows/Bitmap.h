#pragma once

class CBmp
{
	public:
		CBmp(HBITMAP hBMP = NULL);
		~CBmp();
		
		HBITMAP CreateCompatibleBitmap(HDC hDC, int w, int h);
		HBITMAP LoadBitmap(LPCWSTR lpszName);
		void Unload();
		void FreeBMPfromDC();
		
		DWORD GetDimension(unsigned char *iBits = 0);
		
		operator HDC();
		
		operator HBITMAP()
		{
			return m_hBMP;
		}
		
		void Attach(HBITMAP hBMP);
		void Detach();
		
	private:
		HDC m_hDC;
		HBITMAP m_hBMP, m_hBMPorg;
};