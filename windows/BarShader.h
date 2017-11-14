#pragma once

#include "atlcoll.h"

class CBarShader
{
	public:
		CBarShader(uint32_t dwHeight, uint32_t dwWidth, COLORREF crColor = 0, uint64_t dwFileSize = 1ui64);
		~CBarShader(void);
		
		//set the width of the bar
		void SetWidth(uint32_t width);
		
		//set the height of the bar
		void SetHeight(uint32_t height);
		
		//returns the width of the bar
		int GetWidth() const
		{
			return m_iWidth;
		}
		
		//returns the height of the bar
		int GetHeight() const
		{
			return m_iHeight;
		}
		
		//sets new file size and resets the shader
		void SetFileSize(uint64_t qwFileSize);
		
		//fills in a range with a certain color, new ranges overwrite old
		void FillRange(uint64_t qwStart, uint64_t qwEnd, COLORREF crColor);
		
		//fills in entire range with a certain color
		void Fill(COLORREF crColor);
		
		//draws the bar
		void Draw(CDC& dc, int iLeft, int iTop, int);
		
	protected:
		void BuildModifiers();
		void FillRect(CDC& dc, LPCRECT rectSpan, COLORREF crColor);
		
		uint64_t    m_qwFileSize;
		uint32_t    m_iWidth;
		uint32_t    m_iHeight;
		double      m_dblPixelsPerByte;
		double      m_dblBytesPerPixel;
		
	private:
		CRBMap<uint64_t, COLORREF> m_Spans;
		vector<double> m_pdblModifiers;
		byte    m_used3dlevel;
		bool    m_bIsPreview;
		uint32_t CalcHalfHeight(uint32_t p_x) const
		{
			return (p_x + 1) / 2;
		}
		void CalcPerPixelandPerByte(); //[+] PPA
};

typedef struct tagHLSTRIPLE
{
	DOUBLE hlstHue;
	DOUBLE hlstLightness;
	DOUBLE hlstSaturation;
} HLSTRIPLE;

class OperaColors
{
	public:
		static inline BYTE getRValue(const COLORREF& cr)
		{
			return (BYTE)(cr & 0xFF);
		}
		static inline BYTE getGValue(const COLORREF& cr)
		{
			return (BYTE)(((cr & 0xFF00) >> 8) & 0xFF);
		}
		static inline BYTE getBValue(const COLORREF& cr)
		{
			return (BYTE)(((cr & 0xFF0000) >> 16) & 0xFF);
		}
		static double RGB2HUE(double red, double green, double blue);
		static RGBTRIPLE HUE2RGB(double m0, double m2, double h);
		static HLSTRIPLE RGB2HLS(BYTE red, BYTE green, BYTE blue);
		static inline HLSTRIPLE RGB2HLS(COLORREF c)
		{
			return RGB2HLS(getRValue(c), getGValue(c), getBValue(c));
		}
		static RGBTRIPLE HLS2RGB(double hue, double lightness, double saturation);
		static inline RGBTRIPLE HLS2RGB(const HLSTRIPLE& hls)
		{
			return HLS2RGB(hls.hlstHue, hls.hlstLightness, hls.hlstSaturation);
		}
		static inline COLORREF RGB2REF(const RGBTRIPLE& c)
		{
			return RGB(c.rgbtRed, c.rgbtGreen, c.rgbtBlue);
		}
		static inline COLORREF blendColors(const COLORREF& cr1, const COLORREF& cr2, double balance = 0.5)
		{
			BYTE r1 = getRValue(cr1);
			BYTE g1 = getGValue(cr1);
			BYTE b1 = getBValue(cr1);
			BYTE r2 = getRValue(cr2);
			BYTE g2 = getGValue(cr2);
			BYTE b2 = getBValue(cr2);
			return RGB(
			           (r1 * (balance * 2) + (r2 * ((1 - balance) * 2))) / 2,
			           (g1 * (balance * 2) + (g2 * ((1 - balance) * 2))) / 2,
			           (b1 * (balance * 2) + (b2 * ((1 - balance) * 2))) / 2
			       );
		}
		static inline COLORREF brightenColor(const COLORREF& c, double brightness = 0)
		{
			if (EqualD(brightness, 0))
			{
				return c;
			}
			else if (brightness > 0)
			{
				BYTE r = getRValue(c);
				BYTE g = getGValue(c);
				BYTE b = getBValue(c);
				return RGB(
				           (r + ((255 - r) * brightness)),
				           (g + ((255 - g) * brightness)),
				           (b + ((255 - b) * brightness))
				       );
			}
			else
			{
				return RGB(
				           (getRValue(c) * (1 + brightness)),
				           (getGValue(c) * (1 + brightness)),
				           (getBValue(c) * (1 + brightness))
				       );
			}
		}
		static void FloodFill(CDC& hDC, int x1, int y1, int x2, int y2, const COLORREF c1, const COLORREF c2, bool p_light = true);
		static void EnlightenFlood(const COLORREF& clr, COLORREF& a, COLORREF& b);
		static COLORREF TextFromBackground(COLORREF bg);
		
		static void ClearCache(); // A _lot_ easier than to clear certain cache items
		
	private:
		struct FloodCacheItem
#ifdef _DEBUG
				: boost::noncopyable
#endif
		{
			FloodCacheItem() : w(0), h(0), hDC(nullptr), m_bitmap(nullptr)
			{
			}
			void cleanup()
			{
				if (hDC)
				{
					if (!::DeleteObject(m_bitmap))
					{
						const auto l_error_code = GetLastError();
						if (l_error_code)
						{
							dcassert(l_error_code == 0);
							dcdebug("DeleteObject(hBr) = error_code = %d", l_error_code);
						}
					}
					m_bitmap = nullptr;
					
					if (!::DeleteDC(hDC))
					{
						const auto l_error_code = GetLastError();
						if (l_error_code)
						{
							dcassert(l_error_code == 0);
							dcdebug("!DeleteDC(hDC) = error_code = %d", l_error_code);
						}
					}
					hDC = nullptr;
				}
			}
			~FloodCacheItem()
			{
				cleanup();
			}
			struct FCIMapper
			{
				COLORREF c1;
				COLORREF c2;
				bool bLight;
//				FCIMapper(): c1(0),c2(0),bLight(false)
//				{
//				}
			} m_mapper;
			
			int w;
			int h;
			HDC hDC;
			HBITMAP m_bitmap;
		};
		
		struct fci_hash
		{
			size_t operator()(const FloodCacheItem::FCIMapper& __x) const
			{
				return ((__x.c1 ^ __x.c2) + (__x.bLight << 24)); //-V109
			}
			//bool operator()(const FloodCacheItem::FCIMapper& a, const FloodCacheItem::FCIMapper& b) {
			//  return a.c1 < b.c1 && a.c2 < b.c2;
			//};
		};
		
		struct fci_equal_to : public std::binary_function<FloodCacheItem::FCIMapper, FloodCacheItem::FCIMapper, bool>
		{
			bool operator()(const FloodCacheItem::FCIMapper& __x, const FloodCacheItem::FCIMapper& __y) const
			{
				return (__x.c1 == __y.c1) && (__x.c2 == __y.c2) && (__x.bLight == __y.bLight);
			}
		};
		
		typedef std::unordered_map<FloodCacheItem::FCIMapper, FloodCacheItem*, fci_hash, fci_equal_to> FCIMap;
		
		static FCIMap g_flood_cache;
};
