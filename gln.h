#ifndef _COMMON_HPP_
#define _COMMON_HPP_

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include "gl.h"
#include <memory>
#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <glm/glm.hpp>
#include <math.h>

#include <boost/thread.hpp>

void Mem_Init(void);
void Mem_Shutdown(void);
void *Mem_Alloc(uint32_t size);
void *Mem_Realloc(void *ptr, uint64_t nsize);

void *GetResizedMemory(void *ptr, uint64_t nsize);
void *GetClearedMemory(uint64_t size);
void *GetMemory(uint64_t size);
void FreeMemory(void *ptr);
char *CopyString(const char *s);

#ifndef BMFC
struct heap_allocator
{
	constexpr heap_allocator(void) noexcept { }
	constexpr heap_allocator(const char* _name = "allocator") noexcept { }
	constexpr heap_allocator(const heap_allocator &) noexcept { }

	inline bool operator!=(const eastl::allocator) { return true; }
	inline bool operator!=(const heap_allocator) { return false; }
	inline bool operator==(const eastl::allocator) { return false; }
	inline bool operator==(const heap_allocator) { return true; }

    inline void* allocate(size_t n) const
    { return GetMemory(n); }
	inline void* allocate(size_t& n, size_t& alignment, size_t& offset) const
    { return GetMemory(n); }
	inline void* allocate(size_t n, size_t alignment, size_t alignmentOffset, int flags) const
    { return GetMemory(n); }
	inline void deallocate(void *p, size_t) const noexcept
    { FreeMemory(p); }
};
#endif

template<typename T>
class heap_allocator_template
{
public:
    heap_allocator_template(const char* _name = "allocator") noexcept { }
    template<typename U>
	heap_allocator_template(const heap_allocator_template<U> &) noexcept { }
    ~heap_allocator_template() { }

    typedef T value_type;

    inline T* allocate(size_t n) const
    { return static_cast<T *>(GetMemory(n)); }
	inline T* allocate(size_t& n, size_t& alignment, size_t& offset) const
    { return static_cast<T *>(GetMemory(n)); }
    inline T* allocate(size_t n, size_t alignment, size_t alignmentOffset, int flags) const
    { return static_cast<T *>(GetMemory(n)); }
	inline void deallocate(void *p, size_t) const noexcept
    { FreeMemory(p); }
};

#ifndef BMFC
#include <nlohmann/json.hpp>
#endif
#include "defs.h"

#define PAD(base, alignment) (((base)+(alignment)-1) & ~((alignment)-1))
template<typename type, typename alignment>
INLINE type *PADP(type *base, alignment align)
{ return (type *)((void *)PAD((intptr_t)base, align)); }

void Error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
const char *CurrentDirName(void);
const char *va(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
uint64_t LoadFile(const char *filename, void **buffer);
#ifndef BMFC
bool LoadJSON(json& data, const std::string& path);
#endif
void Exit(void);
int GetParm(const char *parm);
bool N_strcat(char *dest, size_t size, const char *src);
int N_isprint(int c);
int N_isalpha(int c);
int N_isupper(int c);
int N_islower(int c);
bool N_isintegral(float f);
void N_strncpy (char *dest, const char *src, size_t count);
void N_strncpyz (char *dest, const char *src, size_t count);
bool N_isanumber(const char *s);
int N_stricmp( const char *s1, const char *s2 );
uint64_t LittleLong(uint64_t l);
uint32_t LittleInt(uint32_t l);
float LittleFloat(float f);
int N_stricmpn (const char *str1, const char *str2, size_t n);
const char *N_stristr(const char *s, const char *find);
#ifdef _WIN32
int N_vsnprintf( char *str, size_t size, const char *format, va_list ap );
#else
#define N_vsnprintf vsnprintf
#endif
uint32_t StrToFilter(const char *str);
uint32_t StrToWrap(const char *str);
uint32_t StrToFormat(const char *str);
const char *FilterToString(uint32_t filter);
const char *FormatToString(uint32_t filter);
const char *WrapToString(uint32_t filter);
const char *GetExtension( const char *name );
void StripExtension(const char *in, char *out, uint64_t destsize);
void DefaultExtension( char *path, uint64_t maxSize, const char *extension );
void SafeWrite(const void *buffer, size_t size, FILE *fp);
void SafeRead(void *buffer, size_t size, FILE *fp);
FILE *SafeOpenRead(const char *path);
FILE *SafeOpenWrite(const char *path);
bool FileExists(const char *path);
uint64_t FileLength(FILE *fp);
const char* GetFilename(const char *path);
bool IsAbsolutePath(const char *path);
INLINE const char *GetAbsolutePath(const char *path)
{ return IsAbsolutePath(path) ? path : GetFilename(path); }
char* BuildOSPath(const char *curPath, const char *gamepath, const char *npath);

template<typename A, typename B, typename C>
INLINE A clamp(const A& value, const B& min, const C& max)
{
    if (value > max) {
        return max;
    } else if (value < min) {
        return min;
    }
    return value;
}

typedef struct {
    int i;
    const char *s;
} StringToInt_t;

INLINE int StringToInt(const std::string& from, const StringToInt_t *to, int nElems)
{
    for (int i = 0; i < nElems; i++) {
        if (!N_stricmp(to[i].s, from.c_str())) {
            return to[i].i;
        }
    }
    return -1;
}

inline constexpr const StringToInt_t texture_details[] = {
    {0, "GPU vs God"},
    {1, "Expensive Shit We've Got Here"},
    {2, "Normie"},
    {3, "Integrated GPU"}
};

inline constexpr const StringToInt_t texture_filters[] = {
    {0, "Nearest"},
    {1, "Linear"},
    {2, "Bilinear"},
    {3, "Trilinear"}
};

inline constexpr const StringToInt_t texture_filters_alt[] = {
    {0, "GL_NEAREST | GL_NEAREST | (Nearest)"},
    {1, "GL_LINEAR  | GL_LINEAR  | (Linear)"},
    {2, "GL_NEAREST | GL_LINEAR  | (Bilinear)"},
    {3, "GL_LINEAR  | GL_NEAREST | (Trilinear)"}
};

typedef enum : uint32_t {
    DIR_NORTH = 0,
    DIR_NORTH_EAST,
    DIR_EAST,
    DIR_SOUTH_EAST,
    DIR_SOUTH,
    DIR_SOUTH_WEST,
    DIR_WEST,
    DIR_NORTH_WEST,
    
    NUMDIRS
} dirtype_t;


/*
All the random shit I pulled from GtkRadiant into this project
*/
//#include "idatastream.h"
//#include "stream.h"

/*
Editor-specific stuff
*/
//#include "gln_files.h"
//#ifndef BMFC
//#include "list.h"
//#include "idll.h"
//#include "command.h"
#include "events.h"
//#include "gui.h"
//#include "preferences.h"
//#include "editor.h"
#include "ImGuiFileDialog.h"
//#include "Texture.h"
//#include "tileset.h"
//#include "project.h"
//#endif
//#include "entity.h"
//#include "map.h"
//#include "parse.h"

#if 0
#undef new
#undef delete

INLINE void *operator new(size_t size) noexcept
{ return GetClearedMemory(size); }
INLINE void *operator new(size_t size, const std::nothrow_t&) noexcept
{ return ::operator new(size); }
INLINE void *operator new[](size_t size) noexcept
{ return ::operator new(size); }
INLINE void *operator new[](size_t size, const std::nothrow_t&) noexcept
{ return ::operator new[](size); }
INLINE void *operator new(size_t size, std::align_val_t alignment) noexcept
{ (void)alignment; return ::operator new(size); }
INLINE void *operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{ return ::operator new(size, alignment); }
INLINE void *operator new[](size_t size, std::align_val_t alignment) noexcept
{ return ::operator new(size, alignment); }
INLINE void *operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{ return ::operator new[](size, alignment); }

INLINE void operator delete(void* ptr) noexcept
{
    if (ptr)
        FreeMemory(ptr);
}
INLINE void operator delete(void* ptr, const std::nothrow_t&) noexcept
{ ::operator delete(ptr); }
INLINE void operator delete(void* ptr, size_t) noexcept
{ ::operator delete(ptr); }
INLINE void operator delete[] (void* ptr) noexcept
{ ::operator delete(ptr); }
INLINE void operator delete[] (void* ptr, const std::nothrow_t&) noexcept
{ ::operator delete[](ptr); }
INLINE void operator delete[] (void* ptr, size_t) noexcept
{ ::operator delete[](ptr); }
INLINE void operator delete(void* ptr, std::align_val_t) noexcept
{ ::operator delete(ptr); }
INLINE void operator delete(void* ptr, std::align_val_t alignment, const std::nothrow_t&) noexcept
{ ::operator delete(ptr, alignment); }
INLINE void operator delete(void* ptr, size_t, std::align_val_t alignment) noexcept
{ ::operator delete(ptr, alignment); }
INLINE void operator delete[] (void* ptr, std::align_val_t alignment) noexcept
{ ::operator delete(ptr, alignment); }
INLINE void operator delete[] (void* ptr, std::align_val_t alignment, const std::nothrow_t&) noexcept
{ ::operator delete[](ptr, alignment); }
INLINE void operator delete[] (void* ptr, size_t, std::align_val_t alignment) noexcept
{ ::operator delete[](ptr, alignment); }
#endif

#define DotProduct(x,y) (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,dst) {dst[0]=a[0]-b[0];dst[1]=a[1]-b[1];dst[2]=a[2]-b[2];}
#define VectorAdd(a,b,c) {c[0]=a[0]+b[0];c[1]=a[1]+b[1];c[2]=a[2]+b[2];}
#define VectorCopy(dst,src) {dst[0]=src[0];dst[1]=src[1];dst[2]=src[2];}
#define VectorScale(a,scale,dst) {dst[0]=scale*a[0];dst[1]=scale*a[1];dst[2]=scale*a[2];}
#define VectorClear(x) {x[0] = x[1] = x[2] = 0;}
#define	VectorNegate(x) {x[0]=-x[0];x[1]=-x[1];x[2]=-x[2];}

INLINE vec_t VectorNormalize( const vec3_t in, vec3_t out ) {
	vec_t	length, ilength;

	length = sqrt (in[0]*in[0] + in[1]*in[1] + in[2]*in[2]);
	if (length == 0)
	{
		VectorClear (out);
		return 0;
	}

	ilength = 1.0/length;
	out[0] = in[0]*ilength;
	out[1] = in[1]*ilength;
	out[2] = in[2]*ilength;

	return length;
}

INLINE vec_t ColorNormalize( const vec3_t in, vec3_t out ) {
	float	max, scale;

	max = in[0];
	if (in[1] > max)
		max = in[1];
	if (in[2] > max)
		max = in[2];

	if (max == 0) {
		out[0] = out[1] = out[2] = 1.0;
		return 0;
	}

	scale = 1.0 / max;

	VectorScale (in, scale, out);

	return max;
}

INLINE void VectorInverse (vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

INLINE void VectorMA (vec3_t va, float scale, vec3_t vb, vec3_t vc)
{
	vc[0] = va[0] + scale*vb[0];
	vc[1] = va[1] + scale*vb[1];
	vc[2] = va[2] + scale*vb[2];
}

INLINE void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

#define COMPRESS_BZIP2 0
#define COMPRESS_ZLIB 1

extern int parm_compression;
extern int myargc;
extern char **myargv;

typedef enum {
    // image data
    BUFFER_JPEG = 0,
    BUFFER_PNG,
    BUFFER_BMP,
    BUFFER_TGA,

    // audio data
    BUFFER_OGG,
    BUFFER_WAV,
    
    // simple data
    BUFFER_DATA,
    
    // text
    BUFFER_TEXT,
} bufferType_t;

char *Compress(void *buf, uint64_t buflen, uint64_t *outlen, int compression = parm_compression);
char *Decompress(void *buf, uint64_t buflen, uint64_t *outlen, int compression = parm_compression);


static void LOG_PRINTF(const char *func, const char *fmt, ...)
{
    va_list argptr;
    char msg[8192];

    va_start(argptr, fmt);
    vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    Printf("%s: %s", func, msg);
}
#define Log_Printf(...) LOG_PRINTF(__PRETTY_FUNCTION__, __VA_ARGS__)

#endif