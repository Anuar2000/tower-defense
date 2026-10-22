// Бір файлдық (single-header) кітапханалардың іске асырылуы тек осы жерде қосылады

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8  // Windows-та кириллица бар жолдарды да ашу үшін
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// miniaudio .ogg файлдарын stb_vorbis арқылы ашады: алдымен тақырыбы, соңында іске асырылуы
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
