/*
 * Unicode case conversion tables, taken from the Unicode 3 specification, circa 2001.
 * There is no case conversion outside the 0..0xFFFF range, so we don't use UCS4 here.
 */

static struct
{
	unsigned short	firstchar;
	unsigned short	lastchar;
	short		delta;
} UnicodeToUpper[] =
{
      {	0x0061,	0x007A,	-32 },
      {	0x00B5,	0x00B5,	743 },
      {	0x00E0,	0x00F6,	-32 },
      {	0x00F8,	0x00FE,	-32 },
      {	0x00FF,	0x00FF,	121 },
      {	0x0101,	0x0101,	-1 },
      {	0x0103,	0x0103,	-1 },
      {	0x0105,	0x0105,	-1 },
      {	0x0107,	0x0107,	-1 },
      {	0x0109,	0x0109,	-1 },
      {	0x010B,	0x010B,	-1 },
      {	0x010D,	0x010D,	-1 },
      {	0x010F,	0x010F,	-1 },
      {	0x0111,	0x0111,	-1 },
      {	0x0113,	0x0113,	-1 },
      {	0x0115,	0x0115,	-1 },
      {	0x0117,	0x0117,	-1 },
      {	0x0119,	0x0119,	-1 },
      {	0x011B,	0x011B,	-1 },
      {	0x011D,	0x011D,	-1 },
      {	0x011F,	0x011F,	-1 },
      {	0x0121,	0x0121,	-1 },
      {	0x0123,	0x0123,	-1 },
      {	0x0125,	0x0125,	-1 },
      {	0x0127,	0x0127,	-1 },
      {	0x0129,	0x0129,	-1 },
      {	0x012B,	0x012B,	-1 },
      {	0x012D,	0x012D,	-1 },
      {	0x012F,	0x012F,	-1 },
      {	0x0131,	0x0131,	-232 },
      {	0x0133,	0x0133,	-1 },
      {	0x0135,	0x0135,	-1 },
      {	0x0137,	0x0137,	-1 },
      {	0x013A,	0x013A,	-1 },
      {	0x013C,	0x013C,	-1 },
      {	0x013E,	0x013E,	-1 },
      {	0x0140,	0x0140,	-1 },
      {	0x0142,	0x0142,	-1 },
      {	0x0144,	0x0144,	-1 },
      {	0x0146,	0x0146,	-1 },
      {	0x0148,	0x0148,	-1 },
      {	0x014B,	0x014B,	-1 },
      {	0x014D,	0x014D,	-1 },
      {	0x014F,	0x014F,	-1 },
      {	0x0151,	0x0151,	-1 },
      {	0x0153,	0x0153,	-1 },
      {	0x0155,	0x0155,	-1 },
      {	0x0157,	0x0157,	-1 },
      {	0x0159,	0x0159,	-1 },
      {	0x015B,	0x015B,	-1 },
      {	0x015D,	0x015D,	-1 },
      {	0x015F,	0x015F,	-1 },
      {	0x0161,	0x0161,	-1 },
      {	0x0163,	0x0163,	-1 },
      {	0x0165,	0x0165,	-1 },
      {	0x0167,	0x0167,	-1 },
      {	0x0169,	0x0169,	-1 },
      {	0x016B,	0x016B,	-1 },
      {	0x016D,	0x016D,	-1 },
      {	0x016F,	0x016F,	-1 },
      {	0x0171,	0x0171,	-1 },
      {	0x0173,	0x0173,	-1 },
      {	0x0175,	0x0175,	-1 },
      {	0x0177,	0x0177,	-1 },
      {	0x017A,	0x017A,	-1 },
      {	0x017C,	0x017C,	-1 },
      {	0x017E,	0x017E,	-1 },
      {	0x017F,	0x017F,	-300 },
      {	0x0183,	0x0183,	-1 },
      {	0x0185,	0x0185,	-1 },
      {	0x0188,	0x0188,	-1 },
      {	0x018C,	0x018C,	-1 },
      {	0x0192,	0x0192,	-1 },
      {	0x0195,	0x0195,	97 },
      {	0x0199,	0x0199,	-1 },
      {	0x01A1,	0x01A1,	-1 },
      {	0x01A3,	0x01A3,	-1 },
      {	0x01A5,	0x01A5,	-1 },
      {	0x01A8,	0x01A8,	-1 },
      {	0x01AD,	0x01AD,	-1 },
      {	0x01B0,	0x01B0,	-1 },
      {	0x01B4,	0x01B4,	-1 },
      {	0x01B6,	0x01B6,	-1 },
      {	0x01B9,	0x01B9,	-1 },
      {	0x01BD,	0x01BD,	-1 },
      {	0x01BF,	0x01BF,	56 },
      {	0x01C5,	0x01C5,	-1 },
      {	0x01C6,	0x01C6,	-2 },
      {	0x01C8,	0x01C8,	-1 },
      {	0x01C9,	0x01C9,	-2 },
      {	0x01CB,	0x01CB,	-1 },
      {	0x01CC,	0x01CC,	-2 },
      {	0x01CE,	0x01CE,	-1 },
      {	0x01D0,	0x01D0,	-1 },
      {	0x01D2,	0x01D2,	-1 },
      {	0x01D4,	0x01D4,	-1 },
      {	0x01D6,	0x01D6,	-1 },
      {	0x01D8,	0x01D8,	-1 },
      {	0x01DA,	0x01DA,	-1 },
      {	0x01DC,	0x01DC,	-1 },
      {	0x01DD,	0x01DD,	-79 },
      {	0x01DF,	0x01DF,	-1 },
      {	0x01E1,	0x01E1,	-1 },
      {	0x01E3,	0x01E3,	-1 },
      {	0x01E5,	0x01E5,	-1 },
      {	0x01E7,	0x01E7,	-1 },
      {	0x01E9,	0x01E9,	-1 },
      {	0x01EB,	0x01EB,	-1 },
      {	0x01ED,	0x01ED,	-1 },
      {	0x01EF,	0x01EF,	-1 },
      {	0x01F2,	0x01F2,	-1 },
      {	0x01F3,	0x01F3,	-2 },
      {	0x01F5,	0x01F5,	-1 },
      {	0x01F9,	0x01F9,	-1 },
      {	0x01FB,	0x01FB,	-1 },
      {	0x01FD,	0x01FD,	-1 },
      {	0x01FF,	0x01FF,	-1 },
      {	0x0201,	0x0201,	-1 },
      {	0x0203,	0x0203,	-1 },
      {	0x0205,	0x0205,	-1 },
      {	0x0207,	0x0207,	-1 },
      {	0x0209,	0x0209,	-1 },
      {	0x020B,	0x020B,	-1 },
      {	0x020D,	0x020D,	-1 },
      {	0x020F,	0x020F,	-1 },
      {	0x0211,	0x0211,	-1 },
      {	0x0213,	0x0213,	-1 },
      {	0x0215,	0x0215,	-1 },
      {	0x0217,	0x0217,	-1 },
      {	0x0219,	0x0219,	-1 },
      {	0x021B,	0x021B,	-1 },
      {	0x021D,	0x021D,	-1 },
      {	0x021F,	0x021F,	-1 },
      {	0x0223,	0x0223,	-1 },
      {	0x0225,	0x0225,	-1 },
      {	0x0227,	0x0227,	-1 },
      {	0x0229,	0x0229,	-1 },
      {	0x022B,	0x022B,	-1 },
      {	0x022D,	0x022D,	-1 },
      {	0x022F,	0x022F,	-1 },
      {	0x0231,	0x0231,	-1 },
      {	0x0233,	0x0233,	-1 },
      {	0x0253,	0x0253,	-210 },
      {	0x0254,	0x0254,	-206 },
      {	0x0256,	0x0257,	-205 },
      {	0x0259,	0x0259,	-202 },
      {	0x025B,	0x025B,	-203 },
      {	0x0260,	0x0260,	-205 },
      {	0x0263,	0x0263,	-207 },
      {	0x0268,	0x0268,	-209 },
      {	0x0269,	0x0269,	-211 },
      {	0x026F,	0x026F,	-211 },
      {	0x0272,	0x0272,	-213 },
      {	0x0275,	0x0275,	-214 },
      {	0x0280,	0x0280,	-218 },
      {	0x0283,	0x0283,	-218 },
      {	0x0288,	0x0288,	-218 },
      {	0x028A,	0x028B,	-217 },
      {	0x0292,	0x0292,	-219 },
      {	0x0345,	0x0345,	84 },
      {	0x03AC,	0x03AC,	-38 },
      {	0x03AD,	0x03AF,	-37 },
      {	0x03B1,	0x03C1,	-32 },
      {	0x03C2,	0x03C2,	-31 },
      {	0x03C3,	0x03CB,	-32 },
      {	0x03CC,	0x03CC,	-64 },
      {	0x03CD,	0x03CE,	-63 },
      {	0x03D0,	0x03D0,	-62 },
      {	0x03D1,	0x03D1,	-57 },
      {	0x03D5,	0x03D5,	-47 },
      {	0x03D6,	0x03D6,	-54 },
      {	0x03DB,	0x03DB,	-1 },
      {	0x03DD,	0x03DD,	-1 },
      {	0x03DF,	0x03DF,	-1 },
      {	0x03E1,	0x03E1,	-1 },
      {	0x03E3,	0x03E3,	-1 },
      {	0x03E5,	0x03E5,	-1 },
      {	0x03E7,	0x03E7,	-1 },
      {	0x03E9,	0x03E9,	-1 },
      {	0x03EB,	0x03EB,	-1 },
      {	0x03ED,	0x03ED,	-1 },
      {	0x03EF,	0x03EF,	-1 },
      {	0x03F0,	0x03F0,	-86 },
      {	0x03F1,	0x03F1,	-80 },
      {	0x03F2,	0x03F2,	-79 },
      {	0x0430,	0x044F,	-32 },
      {	0x0450,	0x045F,	-80 },
      {	0x0461,	0x0461,	-1 },
      {	0x0463,	0x0463,	-1 },
      {	0x0465,	0x0465,	-1 },
      {	0x0467,	0x0467,	-1 },
      {	0x0469,	0x0469,	-1 },
      {	0x046B,	0x046B,	-1 },
      {	0x046D,	0x046D,	-1 },
      {	0x046F,	0x046F,	-1 },
      {	0x0471,	0x0471,	-1 },
      {	0x0473,	0x0473,	-1 },
      {	0x0475,	0x0475,	-1 },
      {	0x0477,	0x0477,	-1 },
      {	0x0479,	0x0479,	-1 },
      {	0x047B,	0x047B,	-1 },
      {	0x047D,	0x047D,	-1 },
      {	0x047F,	0x047F,	-1 },
      {	0x0481,	0x0481,	-1 },
      {	0x048D,	0x048D,	-1 },
      {	0x048F,	0x048F,	-1 },
      {	0x0491,	0x0491,	-1 },
      {	0x0493,	0x0493,	-1 },
      {	0x0495,	0x0495,	-1 },
      {	0x0497,	0x0497,	-1 },
      {	0x0499,	0x0499,	-1 },
      {	0x049B,	0x049B,	-1 },
      {	0x049D,	0x049D,	-1 },
      {	0x049F,	0x049F,	-1 },
      {	0x04A1,	0x04A1,	-1 },
      {	0x04A3,	0x04A3,	-1 },
      {	0x04A5,	0x04A5,	-1 },
      {	0x04A7,	0x04A7,	-1 },
      {	0x04A9,	0x04A9,	-1 },
      {	0x04AB,	0x04AB,	-1 },
      {	0x04AD,	0x04AD,	-1 },
      {	0x04AF,	0x04AF,	-1 },
      {	0x04B1,	0x04B1,	-1 },
      {	0x04B3,	0x04B3,	-1 },
      {	0x04B5,	0x04B5,	-1 },
      {	0x04B7,	0x04B7,	-1 },
      {	0x04B9,	0x04B9,	-1 },
      {	0x04BB,	0x04BB,	-1 },
      {	0x04BD,	0x04BD,	-1 },
      {	0x04BF,	0x04BF,	-1 },
      {	0x04C2,	0x04C2,	-1 },
      {	0x04C4,	0x04C4,	-1 },
      {	0x04C8,	0x04C8,	-1 },
      {	0x04CC,	0x04CC,	-1 },
      {	0x04D1,	0x04D1,	-1 },
      {	0x04D3,	0x04D3,	-1 },
      {	0x04D5,	0x04D5,	-1 },
      {	0x04D7,	0x04D7,	-1 },
      {	0x04D9,	0x04D9,	-1 },
      {	0x04DB,	0x04DB,	-1 },
      {	0x04DD,	0x04DD,	-1 },
      {	0x04DF,	0x04DF,	-1 },
      {	0x04E1,	0x04E1,	-1 },
      {	0x04E3,	0x04E3,	-1 },
      {	0x04E5,	0x04E5,	-1 },
      {	0x04E7,	0x04E7,	-1 },
      {	0x04E9,	0x04E9,	-1 },
      {	0x04EB,	0x04EB,	-1 },
      {	0x04ED,	0x04ED,	-1 },
      {	0x04EF,	0x04EF,	-1 },
      {	0x04F1,	0x04F1,	-1 },
      {	0x04F3,	0x04F3,	-1 },
      {	0x04F5,	0x04F5,	-1 },
      {	0x04F9,	0x04F9,	-1 },
      {	0x0561,	0x0586,	-48 },
      {	0x1E01,	0x1E01,	-1 },
      {	0x1E03,	0x1E03,	-1 },
      {	0x1E05,	0x1E05,	-1 },
      {	0x1E07,	0x1E07,	-1 },
      {	0x1E09,	0x1E09,	-1 },
      {	0x1E0B,	0x1E0B,	-1 },
      {	0x1E0D,	0x1E0D,	-1 },
      {	0x1E0F,	0x1E0F,	-1 },
      {	0x1E11,	0x1E11,	-1 },
      {	0x1E13,	0x1E13,	-1 },
      {	0x1E15,	0x1E15,	-1 },
      {	0x1E17,	0x1E17,	-1 },
      {	0x1E19,	0x1E19,	-1 },
      {	0x1E1B,	0x1E1B,	-1 },
      {	0x1E1D,	0x1E1D,	-1 },
      {	0x1E1F,	0x1E1F,	-1 },
      {	0x1E21,	0x1E21,	-1 },
      {	0x1E23,	0x1E23,	-1 },
      {	0x1E25,	0x1E25,	-1 },
      {	0x1E27,	0x1E27,	-1 },
      {	0x1E29,	0x1E29,	-1 },
      {	0x1E2B,	0x1E2B,	-1 },
      {	0x1E2D,	0x1E2D,	-1 },
      {	0x1E2F,	0x1E2F,	-1 },
      {	0x1E31,	0x1E31,	-1 },
      {	0x1E33,	0x1E33,	-1 },
      {	0x1E35,	0x1E35,	-1 },
      {	0x1E37,	0x1E37,	-1 },
      {	0x1E39,	0x1E39,	-1 },
      {	0x1E3B,	0x1E3B,	-1 },
      {	0x1E3D,	0x1E3D,	-1 },
      {	0x1E3F,	0x1E3F,	-1 },
      {	0x1E41,	0x1E41,	-1 },
      {	0x1E43,	0x1E43,	-1 },
      {	0x1E45,	0x1E45,	-1 },
      {	0x1E47,	0x1E47,	-1 },
      {	0x1E49,	0x1E49,	-1 },
      {	0x1E4B,	0x1E4B,	-1 },
      {	0x1E4D,	0x1E4D,	-1 },
      {	0x1E4F,	0x1E4F,	-1 },
      {	0x1E51,	0x1E51,	-1 },
      {	0x1E53,	0x1E53,	-1 },
      {	0x1E55,	0x1E55,	-1 },
      {	0x1E57,	0x1E57,	-1 },
      {	0x1E59,	0x1E59,	-1 },
      {	0x1E5B,	0x1E5B,	-1 },
      {	0x1E5D,	0x1E5D,	-1 },
      {	0x1E5F,	0x1E5F,	-1 },
      {	0x1E61,	0x1E61,	-1 },
      {	0x1E63,	0x1E63,	-1 },
      {	0x1E65,	0x1E65,	-1 },
      {	0x1E67,	0x1E67,	-1 },
      {	0x1E69,	0x1E69,	-1 },
      {	0x1E6B,	0x1E6B,	-1 },
      {	0x1E6D,	0x1E6D,	-1 },
      {	0x1E6F,	0x1E6F,	-1 },
      {	0x1E71,	0x1E71,	-1 },
      {	0x1E73,	0x1E73,	-1 },
      {	0x1E75,	0x1E75,	-1 },
      {	0x1E77,	0x1E77,	-1 },
      {	0x1E79,	0x1E79,	-1 },
      {	0x1E7B,	0x1E7B,	-1 },
      {	0x1E7D,	0x1E7D,	-1 },
      {	0x1E7F,	0x1E7F,	-1 },
      {	0x1E81,	0x1E81,	-1 },
      {	0x1E83,	0x1E83,	-1 },
      {	0x1E85,	0x1E85,	-1 },
      {	0x1E87,	0x1E87,	-1 },
      {	0x1E89,	0x1E89,	-1 },
      {	0x1E8B,	0x1E8B,	-1 },
      {	0x1E8D,	0x1E8D,	-1 },
      {	0x1E8F,	0x1E8F,	-1 },
      {	0x1E91,	0x1E91,	-1 },
      {	0x1E93,	0x1E93,	-1 },
      {	0x1E95,	0x1E95,	-1 },
      {	0x1E9B,	0x1E9B,	-59 },
      {	0x1EA1,	0x1EA1,	-1 },
      {	0x1EA3,	0x1EA3,	-1 },
      {	0x1EA5,	0x1EA5,	-1 },
      {	0x1EA7,	0x1EA7,	-1 },
      {	0x1EA9,	0x1EA9,	-1 },
      {	0x1EAB,	0x1EAB,	-1 },
      {	0x1EAD,	0x1EAD,	-1 },
      {	0x1EAF,	0x1EAF,	-1 },
      {	0x1EB1,	0x1EB1,	-1 },
      {	0x1EB3,	0x1EB3,	-1 },
      {	0x1EB5,	0x1EB5,	-1 },
      {	0x1EB7,	0x1EB7,	-1 },
      {	0x1EB9,	0x1EB9,	-1 },
      {	0x1EBB,	0x1EBB,	-1 },
      {	0x1EBD,	0x1EBD,	-1 },
      {	0x1EBF,	0x1EBF,	-1 },
      {	0x1EC1,	0x1EC1,	-1 },
      {	0x1EC3,	0x1EC3,	-1 },
      {	0x1EC5,	0x1EC5,	-1 },
      {	0x1EC7,	0x1EC7,	-1 },
      {	0x1EC9,	0x1EC9,	-1 },
      {	0x1ECB,	0x1ECB,	-1 },
      {	0x1ECD,	0x1ECD,	-1 },
      {	0x1ECF,	0x1ECF,	-1 },
      {	0x1ED1,	0x1ED1,	-1 },
      {	0x1ED3,	0x1ED3,	-1 },
      {	0x1ED5,	0x1ED5,	-1 },
      {	0x1ED7,	0x1ED7,	-1 },
      {	0x1ED9,	0x1ED9,	-1 },
      {	0x1EDB,	0x1EDB,	-1 },
      {	0x1EDD,	0x1EDD,	-1 },
      {	0x1EDF,	0x1EDF,	-1 },
      {	0x1EE1,	0x1EE1,	-1 },
      {	0x1EE3,	0x1EE3,	-1 },
      {	0x1EE5,	0x1EE5,	-1 },
      {	0x1EE7,	0x1EE7,	-1 },
      {	0x1EE9,	0x1EE9,	-1 },
      {	0x1EEB,	0x1EEB,	-1 },
      {	0x1EED,	0x1EED,	-1 },
      {	0x1EEF,	0x1EEF,	-1 },
      {	0x1EF1,	0x1EF1,	-1 },
      {	0x1EF3,	0x1EF3,	-1 },
      {	0x1EF5,	0x1EF5,	-1 },
      {	0x1EF7,	0x1EF7,	-1 },
      {	0x1EF9,	0x1EF9,	-1 },
      {	0x1F00,	0x1F07,	8 },
      {	0x1F10,	0x1F15,	8 },
      {	0x1F20,	0x1F27,	8 },
      {	0x1F30,	0x1F37,	8 },
      {	0x1F40,	0x1F45,	8 },
      {	0x1F51,	0x1F51,	8 },
      {	0x1F53,	0x1F53,	8 },
      {	0x1F55,	0x1F55,	8 },
      {	0x1F57,	0x1F57,	8 },
      {	0x1F60,	0x1F67,	8 },
      {	0x1F70,	0x1F71,	74 },
      {	0x1F72,	0x1F75,	86 },
      {	0x1F76,	0x1F77,	100 },
      {	0x1F78,	0x1F79,	128 },
      {	0x1F7A,	0x1F7B,	112 },
      {	0x1F7C,	0x1F7D,	126 },
      {	0x1F80,	0x1F87,	8 },
      {	0x1F90,	0x1F97,	8 },
      {	0x1FA0,	0x1FA7,	8 },
      {	0x1FB0,	0x1FB1,	8 },
      {	0x1FB3,	0x1FB3,	9 },
      {	0x1FBE,	0x1FBE,	-7205 },
      {	0x1FC3,	0x1FC3,	9 },
      {	0x1FD0,	0x1FD1,	8 },
      {	0x1FE0,	0x1FE1,	8 },
      {	0x1FE5,	0x1FE5,	7 },
      {	0x1FF3,	0x1FF3,	9 },
      {	0x2170,	0x217F,	-16 },
      {	0x24D0,	0x24E9,	-26 },
      {	0xFF41,	0xFF5A,	-32 }
};
#define	UCS4NumToUpperChars (sizeof(UnicodeToUpper)/sizeof(UnicodeToUpper[0]))

static struct
{
	unsigned short	firstchar;
	unsigned short	lastchar;
	short		delta;
} UnicodeToLower[] =
{
      {	0x0041,	0x005A,	32 },
      {	0x00C0,	0x00D6,	32 },
      {	0x00D8,	0x00DE,	32 },
      {	0x0100,	0x0100,	1 },
      {	0x0102,	0x0102,	1 },
      {	0x0104,	0x0104,	1 },
      {	0x0106,	0x0106,	1 },
      {	0x0108,	0x0108,	1 },
      {	0x010A,	0x010A,	1 },
      {	0x010C,	0x010C,	1 },
      {	0x010E,	0x010E,	1 },
      {	0x0110,	0x0110,	1 },
      {	0x0112,	0x0112,	1 },
      {	0x0114,	0x0114,	1 },
      {	0x0116,	0x0116,	1 },
      {	0x0118,	0x0118,	1 },
      {	0x011A,	0x011A,	1 },
      {	0x011C,	0x011C,	1 },
      {	0x011E,	0x011E,	1 },
      {	0x0120,	0x0120,	1 },
      {	0x0122,	0x0122,	1 },
      {	0x0124,	0x0124,	1 },
      {	0x0126,	0x0126,	1 },
      {	0x0128,	0x0128,	1 },
      {	0x012A,	0x012A,	1 },
      {	0x012C,	0x012C,	1 },
      {	0x012E,	0x012E,	1 },
      {	0x0130,	0x0130,	-199 },
      {	0x0132,	0x0132,	1 },
      {	0x0134,	0x0134,	1 },
      {	0x0136,	0x0136,	1 },
      {	0x0139,	0x0139,	1 },
      {	0x013B,	0x013B,	1 },
      {	0x013D,	0x013D,	1 },
      {	0x013F,	0x013F,	1 },
      {	0x0141,	0x0141,	1 },
      {	0x0143,	0x0143,	1 },
      {	0x0145,	0x0145,	1 },
      {	0x0147,	0x0147,	1 },
      {	0x014A,	0x014A,	1 },
      {	0x014C,	0x014C,	1 },
      {	0x014E,	0x014E,	1 },
      {	0x0150,	0x0150,	1 },
      {	0x0152,	0x0152,	1 },
      {	0x0154,	0x0154,	1 },
      {	0x0156,	0x0156,	1 },
      {	0x0158,	0x0158,	1 },
      {	0x015A,	0x015A,	1 },
      {	0x015C,	0x015C,	1 },
      {	0x015E,	0x015E,	1 },
      {	0x0160,	0x0160,	1 },
      {	0x0162,	0x0162,	1 },
      {	0x0164,	0x0164,	1 },
      {	0x0166,	0x0166,	1 },
      {	0x0168,	0x0168,	1 },
      {	0x016A,	0x016A,	1 },
      {	0x016C,	0x016C,	1 },
      {	0x016E,	0x016E,	1 },
      {	0x0170,	0x0170,	1 },
      {	0x0172,	0x0172,	1 },
      {	0x0174,	0x0174,	1 },
      {	0x0176,	0x0176,	1 },
      {	0x0178,	0x0178,	-121 },
      {	0x0179,	0x0179,	1 },
      {	0x017B,	0x017B,	1 },
      {	0x017D,	0x017D,	1 },
      {	0x0181,	0x0181,	210 },
      {	0x0182,	0x0182,	1 },
      {	0x0184,	0x0184,	1 },
      {	0x0186,	0x0186,	206 },
      {	0x0187,	0x0187,	1 },
      {	0x0189,	0x018A,	205 },
      {	0x018B,	0x018B,	1 },
      {	0x018E,	0x018E,	79 },
      {	0x018F,	0x018F,	202 },
      {	0x0190,	0x0190,	203 },
      {	0x0191,	0x0191,	1 },
      {	0x0193,	0x0193,	205 },
      {	0x0194,	0x0194,	207 },
      {	0x0196,	0x0196,	211 },
      {	0x0197,	0x0197,	209 },
      {	0x0198,	0x0198,	1 },
      {	0x019C,	0x019C,	211 },
      {	0x019D,	0x019D,	213 },
      {	0x019F,	0x019F,	214 },
      {	0x01A0,	0x01A0,	1 },
      {	0x01A2,	0x01A2,	1 },
      {	0x01A4,	0x01A4,	1 },
      {	0x01A6,	0x01A6,	218 },
      {	0x01A7,	0x01A7,	1 },
      {	0x01A9,	0x01A9,	218 },
      {	0x01AC,	0x01AC,	1 },
      {	0x01AE,	0x01AE,	218 },
      {	0x01AF,	0x01AF,	1 },
      {	0x01B1,	0x01B2,	217 },
      {	0x01B3,	0x01B3,	1 },
      {	0x01B5,	0x01B5,	1 },
      {	0x01B7,	0x01B7,	219 },
      {	0x01B8,	0x01B8,	1 },
      {	0x01BC,	0x01BC,	1 },
      {	0x01C4,	0x01C4,	2 },
      {	0x01C5,	0x01C5,	1 },
      {	0x01C7,	0x01C7,	2 },
      {	0x01C8,	0x01C8,	1 },
      {	0x01CA,	0x01CA,	2 },
      {	0x01CB,	0x01CB,	1 },
      {	0x01CD,	0x01CD,	1 },
      {	0x01CF,	0x01CF,	1 },
      {	0x01D1,	0x01D1,	1 },
      {	0x01D3,	0x01D3,	1 },
      {	0x01D5,	0x01D5,	1 },
      {	0x01D7,	0x01D7,	1 },
      {	0x01D9,	0x01D9,	1 },
      {	0x01DB,	0x01DB,	1 },
      {	0x01DE,	0x01DE,	1 },
      {	0x01E0,	0x01E0,	1 },
      {	0x01E2,	0x01E2,	1 },
      {	0x01E4,	0x01E4,	1 },
      {	0x01E6,	0x01E6,	1 },
      {	0x01E8,	0x01E8,	1 },
      {	0x01EA,	0x01EA,	1 },
      {	0x01EC,	0x01EC,	1 },
      {	0x01EE,	0x01EE,	1 },
      {	0x01F1,	0x01F1,	2 },
      {	0x01F2,	0x01F2,	1 },
      {	0x01F4,	0x01F4,	1 },
      {	0x01F6,	0x01F6,	-97 },
      {	0x01F7,	0x01F7,	-56 },
      {	0x01F8,	0x01F8,	1 },
      {	0x01FA,	0x01FA,	1 },
      {	0x01FC,	0x01FC,	1 },
      {	0x01FE,	0x01FE,	1 },
      {	0x0200,	0x0200,	1 },
      {	0x0202,	0x0202,	1 },
      {	0x0204,	0x0204,	1 },
      {	0x0206,	0x0206,	1 },
      {	0x0208,	0x0208,	1 },
      {	0x020A,	0x020A,	1 },
      {	0x020C,	0x020C,	1 },
      {	0x020E,	0x020E,	1 },
      {	0x0210,	0x0210,	1 },
      {	0x0212,	0x0212,	1 },
      {	0x0214,	0x0214,	1 },
      {	0x0216,	0x0216,	1 },
      {	0x0218,	0x0218,	1 },
      {	0x021A,	0x021A,	1 },
      {	0x021C,	0x021C,	1 },
      {	0x021E,	0x021E,	1 },
      {	0x0222,	0x0222,	1 },
      {	0x0224,	0x0224,	1 },
      {	0x0226,	0x0226,	1 },
      {	0x0228,	0x0228,	1 },
      {	0x022A,	0x022A,	1 },
      {	0x022C,	0x022C,	1 },
      {	0x022E,	0x022E,	1 },
      {	0x0230,	0x0230,	1 },
      {	0x0232,	0x0232,	1 },
      {	0x0386,	0x0386,	38 },
      {	0x0388,	0x038A,	37 },
      {	0x038C,	0x038C,	64 },
      {	0x038E,	0x038F,	63 },
      {	0x0391,	0x03A1,	32 },
      {	0x03A3,	0x03AB,	32 },
      {	0x03DA,	0x03DA,	1 },
      {	0x03DC,	0x03DC,	1 },
      {	0x03DE,	0x03DE,	1 },
      {	0x03E0,	0x03E0,	1 },
      {	0x03E2,	0x03E2,	1 },
      {	0x03E4,	0x03E4,	1 },
      {	0x03E6,	0x03E6,	1 },
      {	0x03E8,	0x03E8,	1 },
      {	0x03EA,	0x03EA,	1 },
      {	0x03EC,	0x03EC,	1 },
      {	0x03EE,	0x03EE,	1 },
      {	0x0400,	0x040F,	80 },
      {	0x0410,	0x042F,	32 },
      {	0x0460,	0x0460,	1 },
      {	0x0462,	0x0462,	1 },
      {	0x0464,	0x0464,	1 },
      {	0x0466,	0x0466,	1 },
      {	0x0468,	0x0468,	1 },
      {	0x046A,	0x046A,	1 },
      {	0x046C,	0x046C,	1 },
      {	0x046E,	0x046E,	1 },
      {	0x0470,	0x0470,	1 },
      {	0x0472,	0x0472,	1 },
      {	0x0474,	0x0474,	1 },
      {	0x0476,	0x0476,	1 },
      {	0x0478,	0x0478,	1 },
      {	0x047A,	0x047A,	1 },
      {	0x047C,	0x047C,	1 },
      {	0x047E,	0x047E,	1 },
      {	0x0480,	0x0480,	1 },
      {	0x048C,	0x048C,	1 },
      {	0x048E,	0x048E,	1 },
      {	0x0490,	0x0490,	1 },
      {	0x0492,	0x0492,	1 },
      {	0x0494,	0x0494,	1 },
      {	0x0496,	0x0496,	1 },
      {	0x0498,	0x0498,	1 },
      {	0x049A,	0x049A,	1 },
      {	0x049C,	0x049C,	1 },
      {	0x049E,	0x049E,	1 },
      {	0x04A0,	0x04A0,	1 },
      {	0x04A2,	0x04A2,	1 },
      {	0x04A4,	0x04A4,	1 },
      {	0x04A6,	0x04A6,	1 },
      {	0x04A8,	0x04A8,	1 },
      {	0x04AA,	0x04AA,	1 },
      {	0x04AC,	0x04AC,	1 },
      {	0x04AE,	0x04AE,	1 },
      {	0x04B0,	0x04B0,	1 },
      {	0x04B2,	0x04B2,	1 },
      {	0x04B4,	0x04B4,	1 },
      {	0x04B6,	0x04B6,	1 },
      {	0x04B8,	0x04B8,	1 },
      {	0x04BA,	0x04BA,	1 },
      {	0x04BC,	0x04BC,	1 },
      {	0x04BE,	0x04BE,	1 },
      {	0x04C1,	0x04C1,	1 },
      {	0x04C3,	0x04C3,	1 },
      {	0x04C7,	0x04C7,	1 },
      {	0x04CB,	0x04CB,	1 },
      {	0x04D0,	0x04D0,	1 },
      {	0x04D2,	0x04D2,	1 },
      {	0x04D4,	0x04D4,	1 },
      {	0x04D6,	0x04D6,	1 },
      {	0x04D8,	0x04D8,	1 },
      {	0x04DA,	0x04DA,	1 },
      {	0x04DC,	0x04DC,	1 },
      {	0x04DE,	0x04DE,	1 },
      {	0x04E0,	0x04E0,	1 },
      {	0x04E2,	0x04E2,	1 },
      {	0x04E4,	0x04E4,	1 },
      {	0x04E6,	0x04E6,	1 },
      {	0x04E8,	0x04E8,	1 },
      {	0x04EA,	0x04EA,	1 },
      {	0x04EC,	0x04EC,	1 },
      {	0x04EE,	0x04EE,	1 },
      {	0x04F0,	0x04F0,	1 },
      {	0x04F2,	0x04F2,	1 },
      {	0x04F4,	0x04F4,	1 },
      {	0x04F8,	0x04F8,	1 },
      {	0x0531,	0x0556,	48 },
      {	0x1E00,	0x1E00,	1 },
      {	0x1E02,	0x1E02,	1 },
      {	0x1E04,	0x1E04,	1 },
      {	0x1E06,	0x1E06,	1 },
      {	0x1E08,	0x1E08,	1 },
      {	0x1E0A,	0x1E0A,	1 },
      {	0x1E0C,	0x1E0C,	1 },
      {	0x1E0E,	0x1E0E,	1 },
      {	0x1E10,	0x1E10,	1 },
      {	0x1E12,	0x1E12,	1 },
      {	0x1E14,	0x1E14,	1 },
      {	0x1E16,	0x1E16,	1 },
      {	0x1E18,	0x1E18,	1 },
      {	0x1E1A,	0x1E1A,	1 },
      {	0x1E1C,	0x1E1C,	1 },
      {	0x1E1E,	0x1E1E,	1 },
      {	0x1E20,	0x1E20,	1 },
      {	0x1E22,	0x1E22,	1 },
      {	0x1E24,	0x1E24,	1 },
      {	0x1E26,	0x1E26,	1 },
      {	0x1E28,	0x1E28,	1 },
      {	0x1E2A,	0x1E2A,	1 },
      {	0x1E2C,	0x1E2C,	1 },
      {	0x1E2E,	0x1E2E,	1 },
      {	0x1E30,	0x1E30,	1 },
      {	0x1E32,	0x1E32,	1 },
      {	0x1E34,	0x1E34,	1 },
      {	0x1E36,	0x1E36,	1 },
      {	0x1E38,	0x1E38,	1 },
      {	0x1E3A,	0x1E3A,	1 },
      {	0x1E3C,	0x1E3C,	1 },
      {	0x1E3E,	0x1E3E,	1 },
      {	0x1E40,	0x1E40,	1 },
      {	0x1E42,	0x1E42,	1 },
      {	0x1E44,	0x1E44,	1 },
      {	0x1E46,	0x1E46,	1 },
      {	0x1E48,	0x1E48,	1 },
      {	0x1E4A,	0x1E4A,	1 },
      {	0x1E4C,	0x1E4C,	1 },
      {	0x1E4E,	0x1E4E,	1 },
      {	0x1E50,	0x1E50,	1 },
      {	0x1E52,	0x1E52,	1 },
      {	0x1E54,	0x1E54,	1 },
      {	0x1E56,	0x1E56,	1 },
      {	0x1E58,	0x1E58,	1 },
      {	0x1E5A,	0x1E5A,	1 },
      {	0x1E5C,	0x1E5C,	1 },
      {	0x1E5E,	0x1E5E,	1 },
      {	0x1E60,	0x1E60,	1 },
      {	0x1E62,	0x1E62,	1 },
      {	0x1E64,	0x1E64,	1 },
      {	0x1E66,	0x1E66,	1 },
      {	0x1E68,	0x1E68,	1 },
      {	0x1E6A,	0x1E6A,	1 },
      {	0x1E6C,	0x1E6C,	1 },
      {	0x1E6E,	0x1E6E,	1 },
      {	0x1E70,	0x1E70,	1 },
      {	0x1E72,	0x1E72,	1 },
      {	0x1E74,	0x1E74,	1 },
      {	0x1E76,	0x1E76,	1 },
      {	0x1E78,	0x1E78,	1 },
      {	0x1E7A,	0x1E7A,	1 },
      {	0x1E7C,	0x1E7C,	1 },
      {	0x1E7E,	0x1E7E,	1 },
      {	0x1E80,	0x1E80,	1 },
      {	0x1E82,	0x1E82,	1 },
      {	0x1E84,	0x1E84,	1 },
      {	0x1E86,	0x1E86,	1 },
      {	0x1E88,	0x1E88,	1 },
      {	0x1E8A,	0x1E8A,	1 },
      {	0x1E8C,	0x1E8C,	1 },
      {	0x1E8E,	0x1E8E,	1 },
      {	0x1E90,	0x1E90,	1 },
      {	0x1E92,	0x1E92,	1 },
      {	0x1E94,	0x1E94,	1 },
      {	0x1EA0,	0x1EA0,	1 },
      {	0x1EA2,	0x1EA2,	1 },
      {	0x1EA4,	0x1EA4,	1 },
      {	0x1EA6,	0x1EA6,	1 },
      {	0x1EA8,	0x1EA8,	1 },
      {	0x1EAA,	0x1EAA,	1 },
      {	0x1EAC,	0x1EAC,	1 },
      {	0x1EAE,	0x1EAE,	1 },
      {	0x1EB0,	0x1EB0,	1 },
      {	0x1EB2,	0x1EB2,	1 },
      {	0x1EB4,	0x1EB4,	1 },
      {	0x1EB6,	0x1EB6,	1 },
      {	0x1EB8,	0x1EB8,	1 },
      {	0x1EBA,	0x1EBA,	1 },
      {	0x1EBC,	0x1EBC,	1 },
      {	0x1EBE,	0x1EBE,	1 },
      {	0x1EC0,	0x1EC0,	1 },
      {	0x1EC2,	0x1EC2,	1 },
      {	0x1EC4,	0x1EC4,	1 },
      {	0x1EC6,	0x1EC6,	1 },
      {	0x1EC8,	0x1EC8,	1 },
      {	0x1ECA,	0x1ECA,	1 },
      {	0x1ECC,	0x1ECC,	1 },
      {	0x1ECE,	0x1ECE,	1 },
      {	0x1ED0,	0x1ED0,	1 },
      {	0x1ED2,	0x1ED2,	1 },
      {	0x1ED4,	0x1ED4,	1 },
      {	0x1ED6,	0x1ED6,	1 },
      {	0x1ED8,	0x1ED8,	1 },
      {	0x1EDA,	0x1EDA,	1 },
      {	0x1EDC,	0x1EDC,	1 },
      {	0x1EDE,	0x1EDE,	1 },
      {	0x1EE0,	0x1EE0,	1 },
      {	0x1EE2,	0x1EE2,	1 },
      {	0x1EE4,	0x1EE4,	1 },
      {	0x1EE6,	0x1EE6,	1 },
      {	0x1EE8,	0x1EE8,	1 },
      {	0x1EEA,	0x1EEA,	1 },
      {	0x1EEC,	0x1EEC,	1 },
      {	0x1EEE,	0x1EEE,	1 },
      {	0x1EF0,	0x1EF0,	1 },
      {	0x1EF2,	0x1EF2,	1 },
      {	0x1EF4,	0x1EF4,	1 },
      {	0x1EF6,	0x1EF6,	1 },
      {	0x1EF8,	0x1EF8,	1 },
      {	0x1F08,	0x1F0F,	-8 },
      {	0x1F18,	0x1F1D,	-8 },
      {	0x1F28,	0x1F2F,	-8 },
      {	0x1F38,	0x1F3F,	-8 },
      {	0x1F48,	0x1F4D,	-8 },
      {	0x1F59,	0x1F59,	-8 },
      {	0x1F5B,	0x1F5B,	-8 },
      {	0x1F5D,	0x1F5D,	-8 },
      {	0x1F5F,	0x1F5F,	-8 },
      {	0x1F68,	0x1F6F,	-8 },
      {	0x1F88,	0x1F8F,	-8 },
      {	0x1F98,	0x1F9F,	-8 },
      {	0x1FA8,	0x1FAF,	-8 },
      {	0x1FB8,	0x1FB9,	-8 },
      {	0x1FBA,	0x1FBB,	-74 },
      {	0x1FBC,	0x1FBC,	-9 },
      {	0x1FC8,	0x1FCB,	-86 },
      {	0x1FCC,	0x1FCC,	-9 },
      {	0x1FD8,	0x1FD9,	-8 },
      {	0x1FDA,	0x1FDB,	-100 },
      {	0x1FE8,	0x1FE9,	-8 },
      {	0x1FEA,	0x1FEB,	-112 },
      {	0x1FEC,	0x1FEC,	-7 },
      {	0x1FF8,	0x1FF9,	-128 },
      {	0x1FFA,	0x1FFB,	-126 },
      {	0x1FFC,	0x1FFC,	-9 },
      {	0x2126,	0x2126,	-7517 },
      {	0x212A,	0x212A,	-8383 },
      {	0x212B,	0x212B,	-8262 },
      {	0x2160,	0x216F,	16 },
      {	0x24B6,	0x24CF,	26 },
      {	0xFF21,	0xFF3A,	32 }
};
#define	UCS4NumToLowerChars (sizeof(UnicodeToLower)/sizeof(UnicodeToLower[0]))

static struct
{
	unsigned short	ch;
	short		delta;
} UnicodeToTitle[] =
{
      {	0x01C4,	1 },
      {	0x01C6,	-1 },
      {	0x01C7,	1 },
      {	0x01C9,	-1 },
      {	0x01CA,	1 },
      {	0x01CC,	-1 },
      {	0x01F1,	1 },
      {	0x01F3,	-1 }
};
#define	UCS4NumTitleChars (sizeof(UnicodeToTitle)/sizeof(UnicodeToTitle[0]))

static unsigned short	UnicodeNoCaseConvLetters[] =
{
	0x00AA, 0x00BA, 0x00DF, 0x0138, 0x0149, 0x0180, 0x018D, 0x019A,
	0x019B, 0x019E, 0x01AA, 0x01AB, 0x01BA, 0x01BE, 0x01F0, 0x0250,
	0x0251, 0x0252, 0x0255, 0x0258, 0x025A, 0x025C, 0x025D, 0x025E,
	0x025F, 0x0261, 0x0262, 0x0264, 0x0265, 0x0266, 0x0267, 0x026A,
	0x026B, 0x026C, 0x026D, 0x026E, 0x0270, 0x0271, 0x0273, 0x0274,
	0x0276, 0x0277, 0x0278, 0x0279, 0x027A, 0x027B, 0x027C, 0x027D,
	0x027E, 0x027F, 0x0281, 0x0282, 0x0284, 0x0285, 0x0286, 0x0287,
	0x0289, 0x028C, 0x028D, 0x028E, 0x028F, 0x0290, 0x0291, 0x0293,
	0x0294, 0x0295, 0x0296, 0x0297, 0x0298, 0x0299, 0x029A, 0x029B,
	0x029C, 0x029D, 0x029E, 0x029F, 0x02A0, 0x02A1, 0x02A2, 0x02A3,
	0x02A4, 0x02A5, 0x02A6, 0x02A7, 0x02A8, 0x02A9, 0x02AA, 0x02AB,
	0x02AC, 0x02AD, 0x0390, 0x03B0, 0x03D2, 0x03D3, 0x03D4, 0x03D7,
	0x03F3, 0x04C0, 0x0587, 0x10A0, 0x10A1, 0x10A2, 0x10A3, 0x10A4,
	0x10A5, 0x10A6, 0x10A7, 0x10A8, 0x10A9, 0x10AA, 0x10AB, 0x10AC,
	0x10AD, 0x10AE, 0x10AF, 0x10B0, 0x10B1, 0x10B2, 0x10B3, 0x10B4,
	0x10B5, 0x10B6, 0x10B7, 0x10B8, 0x10B9, 0x10BA, 0x10BB, 0x10BC,
	0x10BD, 0x10BE, 0x10BF, 0x10C0, 0x10C1, 0x10C2, 0x10C3, 0x10C4,
	0x10C5, 0x1E96, 0x1E97, 0x1E98, 0x1E99, 0x1E9A, 0x1F50, 0x1F52,
	0x1F54, 0x1F56, 0x1FB2, 0x1FB4, 0x1FB6, 0x1FB7, 0x1FC2, 0x1FC4,
	0x1FC6, 0x1FC7, 0x1FD2, 0x1FD3, 0x1FD6, 0x1FD7, 0x1FE2, 0x1FE3,
	0x1FE4, 0x1FE6, 0x1FE7, 0x1FF2, 0x1FF4, 0x1FF6, 0x1FF7, 0x207F,
	0x2102, 0x2107, 0x210A, 0x210B, 0x210C, 0x210D, 0x210E, 0x210F,
	0x2110, 0x2111, 0x2112, 0x2113, 0x2115, 0x2119, 0x211A, 0x211B,
	0x211C, 0x211D, 0x2124, 0x2128, 0x212C, 0x212D, 0x212F, 0x2130,
	0x2131, 0x2133, 0x2134, 0x2139, 0xFB00, 0xFB01, 0xFB02, 0xFB03,
	0xFB04, 0xFB05, 0xFB06, 0xFB13, 0xFB14, 0xFB15, 0xFB16, 0xFB17
};

#define	UCS4NumNoCase	(sizeof(UnicodeNoCaseConvLetters)/		\
			 sizeof(UnicodeNoCaseConvLetters[0]))
