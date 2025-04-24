#ifndef _H_AEFX_CHANNELDEPTHTPL
#define _H_AEFX_CHANNELDEPTHTPL

/** AEFX_ChannelDepthTpl.h

	(c) 2005 Adobe Systems Incorporated

**/

// Basic pixel traits structure. This structure is never used per se, merely overidden -- see below.
template <typename Pixel>
struct PixelTraits {
	typedef int PixType;
	typedef int DataType;
 	static DataType 
    LutFunc(DataType input, const DataType *map);
  	
	enum {max_value = 0 };
};


// 8 bit pixel types, constants, and functions
template <>
struct PixelTraits<PF_Pixel8>{
	typedef PF_Pixel8	PixType;
	typedef u_char		DataType;
	static DataType
	LutFunc(DataType input, const DataType *map){return map[input];}
   	enum {max_value = PF_MAX_CHAN8};
};

// 16 bit pixel types, constants, and functions
template <>
struct PixelTraits<PF_Pixel16>{
	typedef PF_Pixel16 PixType;
	typedef u_short		DataType;
	static u_short
	LutFunc(u_short input, const u_short *map); 
 	enum {max_value = PF_MAX_CHAN16};
};
 

inline u_short 
PixelTraits<PF_Pixel16>::LutFunc(u_short input,
							const u_short *map)
{
 	u_short index  = input >> (15 - PF_TABLE_BITS);
	uint32_t   fract  = input & ((1 << (15 - PF_TABLE_BITS)) - 1);
 	A_long       result = map [index];

	if (fract) {
		result += ((((A_long) map [index + 1] - result) * fract) +
				   (1 << (14 - PF_TABLE_BITS))) >> (15 - PF_TABLE_BITS);
	}
 	return (u_short) result;
	
}

#endif //_H_AEFX_CHANNELDEPTHTPL