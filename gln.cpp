#include "gln.h"
#include <bzlib.h>
#include <zlib.h>
#define GLAD_GL_IMPLEMENTATION
#include "gl.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
//#include <backtrace.h>
//#include <cxxabi.h> // for demangling C++ symbols

#ifdef __unix__
#include <libgen.h>
#endif

int parm_compression;
int myargc;
char **myargv;

#if 0
static struct backtrace_state *bt_state = NULL;

static void bt_error_callback( void *data, const char *msg, int errnum )
{
    Error("libbacktrace ERROR: %d - %s", errnum, msg);
}

static void bt_syminfo_callback( void *data, uintptr_t pc, const char *symname,
								 uintptr_t symval, uintptr_t symsize )
{
	if (symname != NULL) {
		int status;
		// [glnomad] 10/6/2023: fixed buffer instead of malloc'd buffer, risky however
		char name[2048];
		size_t length = sizeof(name);
		abi::__cxa_demangle(symname, name, &length, &status);
		if (name[0]) {
			symname = name;
		}
		Printf("  %zu %s", pc, symname);
	} else {
        Printf("  %zu (unknown symbol)", pc);
	}
}

static int bt_pcinfo_callback( void *data, uintptr_t pc, const char *filename, int lineno, const char *function )
{
	if (data != NULL) {
		int* hadInfo = (int*)data;
		*hadInfo = (function != NULL);
	}

	if (function != NULL) {
		int status;
		// [glnomad] 10/6/2023: fixed buffer instead of malloc'd buffer, risky however
		char name[2048];
		size_t length = sizeof(name);
		abi::__cxa_demangle(function, name, &length, &status);
		if (name[0]) {
			function = name;
		}

		const char* fileNameSrc = strstr(filename, "/src/");
		if (fileNameSrc != NULL) {
			filename = fileNameSrc+1; // I want "neo/bla/blub.cpp:42"
		}
        Printf("  %zu %s:%d %s", pc, filename, lineno, function);
	}

	return 0;
}

static void bt_error_dummy( void *data, const char *msg, int errnum )
{
	//CrashPrintf("ERROR-DUMMY: %d - %s\n", errnum, msg);
}

static int bt_simple_callback(void *data, uintptr_t pc)
{
	int pcInfoWorked = 0;
	// if this fails, the executable doesn't have debug info, that's ok (=> use bt_error_dummy())
	backtrace_pcinfo(bt_state, pc, bt_pcinfo_callback, bt_error_dummy, &pcInfoWorked);
	if (!pcInfoWorked) { // no debug info? use normal symbols instead
		// yes, it would be easier to call backtrace_syminfo() in bt_pcinfo_callback() if function == NULL,
		// but some libbacktrace versions (e.g. in Ubuntu 18.04's g++-7) don't call bt_pcinfo_callback
		// at all if no debug info was available - which is also the reason backtrace_full() can't be used..
		backtrace_syminfo(bt_state, pc, bt_syminfo_callback, bt_error_callback, NULL);
	}

	return 0;
}


static void do_backtrace(void)
{
    // can't use idStr here and thus can't use Sys_GetPath(PATH_EXE) => added Posix_GetExePath()
	const char* exePath = "mapeditor";
	bt_state = backtrace_create_state(exePath[0] ? exePath : NULL, 0, bt_error_callback, NULL);

    if (bt_state != NULL) {
		int skip = 1; // skip this function in backtrace
		backtrace_simple(bt_state, skip, bt_simple_callback, bt_error_callback, NULL);
	} else {
        Error("(No backtrace because libbacktrace state is NULL)");
	}
}
#endif

void assert_failure(const char *expr, const char *file, const char *func, unsigned line)
{
	Error(
		"ImGui Assertion Failure:\n"
		"\tExpression: %s\n"
		"\tFile: %s\n"
		"\tFunction: %s\n"
		"\tLine: %u"
	, expr, file, func, line);
}

// used by EASTL
// needs to be defined by user
void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
	return GetMemory(size);
}

uint64_t LoadFile(const char *filename, void **buffer)
{
	void *buf;
	uint64_t length;
	FILE *fp;

	fp = SafeOpenRead(filename);
	length = FileLength(fp);
	buf = GetClearedMemory(length);
	SafeRead(buf, length, fp);
	fclose(fp);

	*buffer = buf;

	return length;
}

uint64_t LittleLong(uint64_t l)
{
#ifdef __BIG_ENDIAN__
	byte b1, b2, b3, b4, b5, b6, b7;

	b1 = l & 0xff;
	b2 = (l >> 8) & 0xff;
	b3 = (l >> 16) & 0xff;
	b4 = (l >> 24) & 0xff;
	b5 = (l >> 32) & 0xff;
	b6 = (l >> 40) & 0xff;
	b7 = (l >> 48) & 0xff;

	return ((uint64_t)b1<<48) + ((uint64_t)b2<<40) + ((uint64_t)b3<<32) + ((uint64_t)b4<<24) + ((uint64_t)b5<<16) + ((uint64_t)b6<<8) + b7;
#else
	return l;
#endif
}

uint32_t LittleInt(uint32_t l)
{
#ifdef __BIG_ENDIAN__
	byte b1, b2, b3, b4;

	b1 = l & 0xff;
	b2 = (l >> 8) & 0xff;
	b3 = (l >> 16) & 0xff;
	b4 = (l >> 24) & 0xff;

	return ((uint32_t)b1<<24) + ((uint32_t)b2<<16) + ((uint32_t)b3<<8) + b4;
#else
	return l;
#endif
}

float LittleFloat(float f)
{
	typedef union {
	    float	f;
		uint32_t i;
	} _FloatByteUnion;

	const _FloatByteUnion *in;
	_FloatByteUnion out;

	in = (_FloatByteUnion *)&f;
	out.i = LittleInt(in->i);

	return out.f;
}

#ifndef BMFC
bool LoadJSON(json& data, const std::string& path)
{
	FileStream file;
	if (!FileExists(path.c_str())) {
		Printf("WARNING: failed to load json file '%s', file doesn't exist", path.c_str());
		return false;
	}
	if (!file.Open(path.c_str(), "rb")) {
		Error("[LoadJSON] failed to open json file '%s'", path.c_str());
	}
	try {
		data = json::parse(file.GetStream());
	} catch (const json::exception& e) {
		Printf("ERROR: failed to load json file '%s'. nlohmann::json::exception =>\n\tid: %i\n\tstring: %s", path.c_str(), e.id, e.what());
		file.Close();
		return false;
	}
	file.Close();
	return true;
}
#endif

void Exit(void)
{
    Printf("Exiting app (code : 1)");
    exit(EXIT_SUCCESS);
}

const char *va(const char *fmt, ...)
{
	va_list argptr;
	static char string[2][MAX_VA_BUFFER];
	static int toggle = 0;
	char *buf;

	buf = string[toggle & 1];
	toggle++;

	va_start(argptr, fmt);
	vsnprintf(buf, MAX_VA_BUFFER, fmt, argptr);
	va_end(argptr);

	return buf;
}

static FILE *logfile = NULL;

void Error(const char *fmt, ...)
{
    va_list argptr;
    char buffer[4096];

    va_start(argptr, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, argptr);
    va_end(argptr);

	if (!logfile) {
		logfile = fopen("logfile.log", "w");
		if (!logfile) {
			fprintf(stderr, "Warning: failed to open logfile\n");
		}
	}

#if 0
	do_backtrace();
#endif

#ifndef BMFC
	Window::Print("\n********** ERROR **********");
	Window::Print("%s", buffer);
	Window::Print("Exiting app (code : -1)");
#endif
	fprintf(stderr, "\n********** ERROR **********\n");
	fprintf(stderr, "%s\n", buffer);
	fprintf(stderr, "Exiting app (code : -1)\n");

	if (logfile) {
	#ifdef UNICODE
		fwprintf(logfile, L"\n********** ERROR **********\n");
		fwprintf(logfile, L"%s\n", buffer);
		fwprintf(logfile, L"Exiting app (code : -1)\n");
		fflush(logfile);
	#else
		fprintf(logfile, "\n********** ERROR **********\n");
		fprintf(logfile, "%s\n", buffer);
		fprintf(logfile, "Exiting app (code : -1)\n");
		fflush(logfile);
	#endif
		fclose(logfile);
	}

	fflush(NULL);

    exit(-1);
}

void Printf(const char *fmt, ...)
{
    va_list argptr;
    char buffer[4096];

    va_start(argptr, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, argptr);
    va_end(argptr);

#ifndef BMFC
	Window::Print("%s", buffer);
#endif
	fprintf(stdout, "%s\n", buffer);
	if (logfile) {
	#ifdef UNICODE
		fwprintf(logfile L"%s\n", buffer);
	#else
		fprintf(logfile, "%s\n", buffer);
	#endif
	}
}

static inline const char *bzip2_strerror(int err)
{
	switch (err) {
	case BZ_DATA_ERROR: return "(BZ_DATA_ERROR) buffer provided to bzip2 was corrupted";
	case BZ_MEM_ERROR: return "(BZ_MEM_ERROR) memory allocation request made by bzip2 failed";
	case BZ_DATA_ERROR_MAGIC: return "(BZ_DATA_ERROR_MAGIC) buffer was not compressed with bzip2, it did not contain \"BZA\"";
	case BZ_IO_ERROR: return va("(BZ_IO_ERROR) failure to read or write, file I/O error");
	case BZ_UNEXPECTED_EOF: return "(BZ_UNEXPECTED_EOF) unexpected end of data stream";
	case BZ_OUTBUFF_FULL: return "(BZ_OUTBUFF_FULL) buffer overflow";
	case BZ_SEQUENCE_ERROR: return "(BZ_SEQUENCE_ERROR) bad function call error, please report this bug";
	case BZ_OK:
		break;
	};
	return "No Error... How?";
}

static inline void CheckBZIP2(int errcode, uint64_t buflen, const char *action)
{
	switch (errcode) {
	case BZ_OK:
	case BZ_RUN_OK:
	case BZ_FLUSH_OK:
	case BZ_FINISH_OK:
	case BZ_STREAM_END:
		return; // all good
	case BZ_CONFIG_ERROR:
	case BZ_DATA_ERROR:
	case BZ_DATA_ERROR_MAGIC:
	case BZ_IO_ERROR:
	case BZ_MEM_ERROR:
	case BZ_PARAM_ERROR:
	case BZ_SEQUENCE_ERROR:
	case BZ_OUTBUFF_FULL:
	case BZ_UNEXPECTED_EOF:
		Error("Failure on %s of %lu bytes. BZIP2 error reason:\n\t%s", action, buflen, bzip2_strerror(errcode));
		break;
	};
}

static char *Compress_BZIP2(void *buf, uint64_t buflen, uint64_t *outlen)
{
#if 0
	char *out, *newbuf;
	unsigned int len;
	int ret;

	Printf("Compressing %lu bytes with bzip2...", buflen);

	len = buflen;
	out = (char *)GetMemory(buflen);
	ret = BZ2_bzBuffToBuffCompress(out, &len, (char *)buf, buflen, 9, 4, 50);
	CheckBZIP2(ret, buflen, "compression");

	Printf("Successful compression of %lu to %u bytes with zlib", buflen, len);
	newbuf = (char *)GetMemory(len);
	memcpy(newbuf, out, len);
	FreeMemory(out);
	*outlen = len;

	return newbuf;
#endif
}

static inline const char *zlib_strerror(int err)
{
	switch (err) {
	case Z_DATA_ERROR: return "(Z_DATA_ERROR) buffer provided to zlib was corrupted";
	case Z_BUF_ERROR: return "(Z_BUF_ERROR) buffer overflow";
	case Z_STREAM_ERROR: return "(Z_STREAM_ERROR) bad params passed to zlib, please report this bug";
	case Z_MEM_ERROR: return "(Z_MEM_ERROR) memory allocation request made by zlib failed";
	case Z_OK:
		break;
	};
	return "No Error... How?";
}

static void *zalloc(voidpf opaque, uInt items, uInt size)
{
	(void)opaque;
	(void)items;
	return GetMemory(size);
}

static void zfreeMemory(voidpf opaque, voidpf address)
{
	(void)opaque;
	FreeMemory((void *)address);
}

static char *Compress_ZLIB(void *buf, uint64_t buflen, uint64_t *outlen)
{
	char *out, *newbuf;
	const uint64_t expectedLen = buflen / 2;
	int ret;

	out = (char *)GetMemory(buflen);

#if 0
	stream.zalloc = zalloc;
	stream.zfree = zfree;
	stream.opaque = Z_NULL;
	stream.next_in = (Bytef *)buf;
	stream.avail_in = buflen;
	stream.next_out = (Bytef *)out;
	stream.avail_out = buflen;

	ret = deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 15, 9, );
	if (ret != Z_OK) {
		Error("Failed to compress buffer of %lu bytes (inflateInit2)", buflen);
		return NULL;
	}
	inflate

	do {
		ret = inflate(&stream, Z_SYNC_FLUSH);

		switch (ret) {
		case Z_NEED_DICT:
		case Z_STREAM_ERROR:
			ret = Z_DATA_ERROR;
			break;
		case Z_DATA_ERRO:
		case Z_MEM_ERROR:
			inflateEnd(&stream);
			Error("Failed to compress buffer of %lu bytes (inflate)", buflen);
			return NULL;
		};
		
		if (ret != Z_STREAM_END) {
			newbuf = (char *)Malloc(buflen * 2);
			memcpy(newbuf, out, buflen * 2);
			FreeMemory(out);
			out = newbuf;

			stream.next_out = (Bytef *)(out + buflen);
			stream.avail_out = buflen;
			buflen *= 2;
		}
	} while (ret != Z_STREAM_END);
#endif
	Printf("Compressing %lu bytes with zlib", buflen);

	ret = compress2((Bytef *)out, (uLongf *)outlen, (const Bytef *)buf, buflen, Z_BEST_COMPRESSION);
	if (ret != Z_OK)
		Error("Failure on compression of %lu bytes. ZLIB error reason:\n\t%s", buflen, zError(ret));
	
	Printf("Successful compression of %lu to %lu bytes with zlib", buflen, *outlen);
	newbuf = (char *)GetMemory(*outlen);
	memcpy(newbuf, out, *outlen);
	FreeMemory(out);

	return newbuf;
}

char *Compress(void *buf, uint64_t buflen, uint64_t *outlen, int compression)
{
	switch (compression) {
	case COMPRESS_BZIP2:
		return Compress_BZIP2(buf, buflen, outlen);
	case COMPRESS_ZLIB:
		return Compress_ZLIB(buf, buflen, outlen);
	default:
		break;
	};
	return (char *)buf;
}

static char *Decompress_BZIP2(void *buf, uint64_t buflen, uint64_t *outlen)
{
#if 0
	char *out, *newbuf;
	unsigned int len;
	int ret;

	Printf("Decompressing %lu bytes with bzip2", buflen);
	len = buflen * 2;
	out = (char *)GetMemory(buflen * 2);
	ret = BZ2_bzBuffToBuffDecompress(out, &len, (char *)buf, buflen, 0, 4);
	CheckBZIP2(ret, buflen, "decompression");

	Printf("Successful decompression of %lu to %u bytes with bzip2", buflen, len);
	newbuf = (char *)GetMemory(len);
	memcpy(newbuf, out, len);
	FreeMemory(out);
	*outlen = len;

	return newbuf;
#endif
}

static char *Decompress_ZLIB(void *buf, uint64_t buflen, uint64_t *outlen)
{
	char *out, *newbuf;
	int ret;

	Printf("Decompressing %lu bytes with zlib...", buflen);
	out = (char *)GetMemory(buflen * 2);
	*outlen = buflen * 2;
	ret = uncompress((Bytef *)out, (uLongf *)outlen, (const Bytef *)buf, buflen);
	if (ret != Z_OK)
		Error("Failure on decompression of %lu bytes. ZLIB error reason:\n\t:%s", buflen * 2, zError(ret));
	
	Printf("Successful decompression of %lu bytes to %lu bytes with zlib", buflen, *outlen);
	newbuf = (char *)GetMemory(*outlen);
	memcpy(newbuf, out, *outlen);
	FreeMemory(out);

	return newbuf;
}

char *Decompress(void *buf, uint64_t buflen, uint64_t *outlen, int compression)
{
	switch (compression) {
	case COMPRESS_BZIP2:
		return Decompress_BZIP2(buf, buflen, outlen);
	case COMPRESS_ZLIB:
		return Decompress_ZLIB(buf, buflen, outlen);
	default:
		break;
	};
	return (char *)buf;
}

bool IsAbsolutePath(const char *path)
{
	return strrchr(path, PATH_SEP) == NULL;
}

const char *CurrentDirName(void)
{
	static char pwd[ MAX_OSPATH ];
	if (pwd[0])
		return pwd;

#ifdef _WIN32
	TCHAR	buffer[ MAX_OSPATH ];
	char *s;

	GetModuleFileName( NULL, buffer, arraylen( buffer ) );
	buffer[ arraylen( buffer ) - 1 ] = '\0';

	N_strncpyz( pwd, WtoA( buffer ), sizeof( pwd ) );

	s = strrchr( pwd, PATH_SEP );
	if ( s ) 
		*s = '\0';
	else // bogus case?
	{
		_getcwd( pwd, sizeof( pwd ) - 1 );
		pwd[ sizeof( pwd ) - 1 ] = '\0';
	}

	return pwd;
#else
	// more reliable, linux-specific
	if ( readlink( "/proc/self/exe", pwd, sizeof( pwd ) - 1 ) != -1 )
	{
		pwd[ sizeof( pwd ) - 1 ] = '\0';
		dirname( pwd );
		return pwd;
	}

	if ( !getcwd( pwd, sizeof( pwd ) ) )
	{
		pwd[0] = '\0';
	}

	return pwd;
#endif
}

char *CopyString(const char *s)
{
	if (!s) {
		s = "";
	}
	return strcpy((char *)GetMemory(strlen(s) + 1), s);
}

void *GetMemory(uint64_t size)
{
	void *buf;

	buf = malloc(size);
	if (!buf) {
		Error("GetMemory: out of memory!");
	}

	return buf;
}

void *GetResizedMemory(void *ptr, uint64_t nsize)
{
	void *buf;

	buf = realloc(ptr, nsize);
	if (!buf) {
		Error("GetResizedMemory: out of memory!");
	}

	return buf;
}

void *GetClearedMemory(uint64_t size)
{
	return memset(GetMemory(size), 0, size);
}

void FreeMemory(void *ptr)
{
	free(ptr);
}

int GetParm(const char *parm)
{
    int i;

    for (i = 1; i < myargc - 1; i++) {
        if (!N_stricmp(myargv[i], parm))
            return i;
    }
    return -1;
}

const char *GetFilename(const char *path)
{
	const char *dir;

	dir = strrchr(path, PATH_SEP);
	return dir ? dir + 1 : path;
}

void DefaultExtension( char *path, uint64_t maxSize, const char *extension )
{
	const char *dot = strrchr(path, '.'), *slash;
	if (dot && ((slash = strrchr(path, '/')) == NULL || slash < dot))
		return;
	else
		N_strcat(path, maxSize, extension);
}


void StripExtension(const char *in, char *out, uint64_t destsize)
{
	const char *dot = (char *)strrchr(in, '.'), *slash;

	if (dot && ((slash = (char *)strrchr(in, '/')) == NULL || slash < dot))
		destsize = (destsize < dot-in+1 ? destsize : dot-in+1);

	if ( in == out && destsize > 1 )
		out[destsize-1] = '\0';
	else
		N_strncpy(out, in, destsize);
}

const char *GetExtension( const char *name )
{
	const char *dot = strrchr(name, '.'), *slash;
	if (dot && ((slash = strrchr(name, '/')) == NULL || slash < dot))
		return dot + 1;
	else
		return "";
}
#ifdef _WIN32
/*
=============
N_vsnprintf
 
Special wrapper function for Microsoft's broken _vsnprintf() function. mingw-w64
however, uses Microsoft's broken _vsnprintf() function.
=============
*/
int N_vsnprintf( char *str, size_t size, const char *format, va_list ap )
{
	int retval;
	
	retval = _vsnprintf( str, size, format, ap );

	if ( retval < 0 || (size_t)retval == size ) {
		// Microsoft doesn't adhere to the C99 standard of vsnprintf,
		// which states that the return value must be the number of
		// bytes written if the output string had sufficient length.
		//
		// Obviously we cannot determine that value from Microsoft's
		// implementation, so we have no choice but to return size.
		
		str[size - 1] = '\0';
		return size;
	}
	
	return retval;
}
#endif

int N_isprint( int c )
{
	if ( c >= 0x20 && c <= 0x7E )
		return ( 1 );
	return ( 0 );
}


int N_islower( int c )
{
	if (c >= 'a' && c <= 'z')
		return ( 1 );
	return ( 0 );
}


int N_isupper( int c )
{
	if (c >= 'A' && c <= 'Z')
		return ( 1 );
	return ( 0 );
}


int N_isalpha( int c )
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return ( 1 );
	return ( 0 );
}

bool N_isintegral(float f)
{
	return (int)f == f;
}


bool N_isanumber( const char *s )
{
#ifdef Q3_VM
    //FIXME: implement
    return qfalse;
#else
    char *p;

	if( *s == '\0' )
        return false;

	strtod( s, &p );

    return *p == '\0';
#endif
}

void N_itoa(char *buf, uint64_t bufsize, int i)
{
	snprintf(buf, bufsize, "%i", i);
}

void N_ftoa(char *buf, uint64_t bufsize, float f)
{
	snprintf(buf, bufsize, "%f", f);
}

void N_strcpy (char *dest, const char *src)
{
	char *d = dest;
	const char *s = src;
	while (*s)
		*d++ = *s++;
	
	*d++ = 0;
}

void N_strncpyz (char *dest, const char *src, size_t count)
{
	if (!dest)
		Error( "N_strncpyz: NULL dest");
	if (!src)
		Error( "N_strncpyz: NULL src");
	if (count < 1)
		Error( "N_strncpyz: bad count");
	
#if 1 
	// do not fill whole remaining buffer with zeros
	// this is obvious behavior change but actually it may affect only buggy QVMs
	// which passes overlapping or short buffers to cvar reading routines
	// what is rather good than bad because it will no longer cause overwrites, maybe
	while ( --count > 0 && (*dest++ = *src++) != '\0' );
	*dest = '\0';
#else
	strncpy( dest, src, count-1 );
	dest[ count-1 ] = '\0';
#endif
}

void N_strncpy (char *dest, const char *src, size_t count)
{
	while (*src && count--)
		*dest++ = *src++;

	if (count)
		*dest++ = 0;
}


char *N_strupr(char *s1)
{
	char *s;

	s = s1;
	while (*s) {
		if (*s >= 'a' && *s <= 'z')
			*s = *s - 'a' + 'A';
		s++;
	}
	return s1;
}

// never goes past bounds or leaves without a terminating 0
bool N_strcat(char *dest, size_t size, const char *src)
{
	size_t l1;

	l1 = strlen(dest);
	if (l1 >= size) {
		Printf( "N_strcat: already overflowed" );
		return false;
	}

	N_strncpy( dest + l1, src, size - l1 );
	return true;
}

char *N_stradd(char *dst, const char *src)
{
	char c;
	while ( (c = *src++) != '\0' )
		*dst++ = c;
	*dst = '\0';
	return dst;
}


/*
* Find the first occurrence of find in s.
*/
const char *N_stristr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		if (c >= 'a' && c <= 'z') {
	    	c -= ('a' - 'A');
		}
 	   	len = strlen(find);
    	do {
    		do {
        		if ((sc = *s++) == 0)
          			return NULL;
        		if (sc >= 'a' && sc <= 'z') {
          			sc -= ('a' - 'A');
        		}
      		} while (sc != c);
    	} while (N_stricmpn(s, find, len) != 0);
   		s--;
  	}
  	return s;
}

int N_replace(const char *str1, const char *str2, char *src, size_t max_len)
{
	size_t len1, len2, count;
	ssize_t d;
	const char *s0, *s1, *s2, *max;
	char *match, *dst;

	match = strstr(src, str1);

	if (!match)
		return 0;

	count = 0; // replace count

    len1 = strlen(str1);
    len2 = strlen(str2);
    d = len2 - len1;

    if (d > 0) { // expand and replace mode
        max = src + max_len;
        src += strlen(src);

        do { // expand source string
			s1 = src;
            src += d;
            if (src >= max)
                return count;
            dst = src;
            
            s0 = match + len1;

            while (s1 >= s0)
                *dst-- = *s1--;
			
			// replace match
            s2 = str2;
			while (*s2)
                *match++ = *s2++;
			
            match = strstr(match, str1);

            count++;
		} while (match);

        return count;
    } 
    else if (d < 0) { // shrink and replace mode
        do  { // shrink source string
            s1 = match + len1;
            dst = match + len2;
            while ( (*dst++ = *s1++) != '\0' );
			
			//replace match
            s2 = str2;
			while ( *s2 ) {
				*match++ = *s2++;
			}

            match = strstr( match, str1 );

            count++;
        } 
        while ( match );

        return count;
    }
    else {
	    do { // just replace match
    	    s2 = str2;
			while (*s2)
				*match++ = *s2++;

    	    match = strstr(match, str1);
    	    count++;
		}  while (match);
	}

	return count;
}

size_t N_strlen (const char *str)
{
	size_t count = 0;
    while (str[count]) {
        ++count;
    }
	return count;
}

char *N_strrchr(char *str, char c)
{
    char *s = str;
    size_t len = N_strlen(s);
    s += len;
    while (len--)
    	if (*--s == c) return s;
    return 0;
}

int N_strcmp (const char *str1, const char *str2)
{
    const char *s1 = str1;
    const char *s2 = str2;
	while (1) {
		if (*s1 != *s2)
			return -1;              // strings not equal    
		if (!*s1)
			return 1;               // strings are equal
		s1++;
		s2++;
	}
	
	return 0;
}

bool N_streq(const char *str1, const char *str2)
{
	const char *s1 = str1;
	const char *s2 = str2;
	
	while (*s2 && *s1) {
		if (*s1++ != *s2++)
			return false;
	}
	return true;
}

bool N_strneq(const char *str1, const char *str2, size_t n)
{
	const char *s1 = str1;
	const char *s2 = str2;

	while (*s1 && n) {
		if (*s1++ != *s2++)
			return false;
		n--;
	}
	return true;
}

int N_strncmp( const char *s1, const char *s2, size_t n )
{
	int c1, c2;
	
	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--) {
			return 0;		// strings are equal until end point
		}
		
		if (c1 != c2) {
			return c1 < c2 ? -1 : 1;
		}
	} while (c1);
	
	return 0;		// strings are equal
}

int N_stricmpn (const char *str1, const char *str2, size_t n)
{
	int c1, c2;

	// bk001129 - moved in 1.17 fix not in id codebase
    if (str1 == NULL) {
    	if (str2 == NULL )
            return 0;
        else
            return -1;
    }
    else if (str2 == NULL)
        return 1;


	
	do {
		c1 = *str1++;
		c2 = *str2++;

		if (!n--) {
			return 0;		// strings are equal until end point
		}
		
		if (c1 != c2) {
			if (c1 >= 'a' && c1 <= 'z') {
				c1 -= ('a' - 'A');
			}
			if (c2 >= 'a' && c2 <= 'z') {
				c2 -= ('a' - 'A');
			}
			if (c1 != c2) {
				return c1 < c2 ? -1 : 1;
			}
		}
	} while (c1);
	
	return 0;		// strings are equal
}

int N_stricmp( const char *s1, const char *s2 ) 
{
	unsigned char c1, c2;

	if (s1 == NULL)  {
		if (s2 == NULL)
			return 0;
		else
			return -1;
	}
	else if (s2 == NULL)
		return 1;
	
	do {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 != c2) {
			if ( c1 <= 'Z' && c1 >= 'A' )
				c1 += ('a' - 'A');

			if ( c2 <= 'Z' && c2 >= 'A' )
				c2 += ('a' - 'A');

			if ( c1 != c2 ) 
				return c1 < c2 ? -1 : 1;
		}
	} while ( c1 != '\0' );

	return 0;
}

char* BuildOSPath(const char *curPath, const char *gamepath, const char *npath)
{
	static char ospath[MAX_OSPATH*2+1];
	char temp[MAX_OSPATH];

	if (npath)
		snprintf(temp, sizeof(temp), "%c%s%c%s", PATH_SEP, gamepath, PATH_SEP, npath);
	else
		snprintf(temp, sizeof(temp), "%c%s%c", PATH_SEP, gamepath, PATH_SEP);

	snprintf(ospath, sizeof(ospath), "%s%s", curPath, temp);
	return ospath;
}

void SafeWrite(const void *buffer, size_t size, FILE *fp)
{
    Printf("Writing %lu bytes to file", size);
    if (!fwrite(buffer, size, 1, fp)) {
        Error("Failed to write %lu bytes to file", size);
    }
}

void SafeRead(void *buffer, size_t size, FILE *fp)
{
    Printf("Reading %lu bytes from file", size);
    if (!fread(buffer, size, 1, fp)) {
        Error("Failed to read %lu bytes from file", size);
    }
}

FILE *SafeOpenRead(const char *path)
{
    FILE *fp;
    
    Printf("Opening '%s' in read mode", path);
    fp = fopen(path, "rb");
    if (!fp) {
        Error("Failed to open file %s in read mode", path);
    }
    return fp;
}

FILE *SafeOpenWrite(const char *path)
{
    FILE *fp;
    
	Printf("Opening '%s' in write mode", path);
    fp = fopen(path, "wb");
    if (!fp) {
        Error("Failed to open file %s in write mode", path);
    }
    return fp;
}

bool FileExists(const char *path)
{
    FILE *fp;

    fp = fopen(path, "r");
    if (!fp)
        return false;
    
    fclose(fp);
    return true;
}

uint64_t FileLength(FILE *fp)
{
    uint64_t pos, end;

    pos = ftell(fp);
    fseek(fp, 0L, SEEK_END);
    end = ftell(fp);
    fseek(fp, pos, SEEK_SET);

    return end;
}

#ifndef BMFC
const char *FilterToString(uint32_t filter)
{
    switch (filter) {
    case GL_LINEAR: return "GL_LINEAR";
    case GL_NEAREST: return "GL_NEAREST";
    };

	// None
    return "None";
}

uint32_t StrToFilter(const char *str)
{
    if (!N_stricmp(str, "GL_LINEAR")) return GL_LINEAR;
    else if (!N_stricmp(str, "GL_NEAREST")) return GL_NEAREST;

	// None
    return 0;
}

const char *WrapToString(uint32_t wrap)
{
    switch (wrap) {
    case GL_REPEAT: return "GL_REPEAT";
    case GL_CLAMP_TO_EDGE: return "GL_CLAMP_TO_EDGE";
    case GL_CLAMP_TO_BORDER: return "GL_CLAMP_TO_BORDER";
    case GL_MIRRORED_REPEAT: return "GL_MIRRORED_REPEAT";
    };

	// None
    return "None";
}

uint32_t StrToWrap(const char *str)
{
    if (!N_stricmp(str, "GL_REPEAT")) return GL_REPEAT;
    else if (!N_stricmp(str, "GL_CLAMP_TO_EDGE")) return GL_CLAMP_TO_EDGE;
    else if (!N_stricmp(str, "GL_CLAMP_TO_BORDER")) return GL_CLAMP_TO_BORDER;
    else if (!N_stricmp(str, "GL_MIRRORED_REPEAT")) return GL_MIRRORED_REPEAT;

	// None
	return 0;
}

const char *FormatToString(uint32_t format)
{
    switch (format) {
    case GL_RGBA8: return "GL_RGBA8";
    case GL_RGBA12: return "GL_RGBA12";
    case GL_RGBA16: return "GL_RGBA16";
    };

	// None
	return "None";
}

uint32_t StrToFormat(const char *str)
{
    if (!N_stricmp(str, "GL_RGBA8")) return GL_RGBA8;
    else if (!N_stricmp(str, "GL_RGBA12")) return GL_RGBA12;
    else if (!N_stricmp(str, "GL_RGBA16")) return GL_RGBA16;
    
	// None
    return 0;
}
#endif

