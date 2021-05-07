/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once

#include <inttypes.h> // uint32_t, uint64_t
#include <stdarg.h> // va_list
#include <string>

#define K__UTF8BOM_STR    "\xEF\xBB\xBF" 
#define K__UTF8BOM_LEN    3 

#define K__PATH_SLASH          '/'
#define K__PATH_SLASHW        L'/'
#define K__PATH_BACKSLASH      '\\'
#define K__PATH_BACKSLASHW    L'\\'

#define K__Assert(expr)  (!(expr) ? (K__Error("[ASSERTION_FAILURE]: %s(%d): %s", __FILE__, __LINE__, #expr), K__Break()) : (void)0)
#define K__Verify(expr)  (!(expr) ? (K__Error("[ASSERTION_FAILURE]: %s(%d): %s", __FILE__, __LINE__, #expr), K__Break()) : (void)0)

#define K__ASSERT_RETURN(expr)           if (!(expr)) { K__Assert(expr); return; }
#define K__ASSERT_RETURN_ZERO(expr)      if (!(expr)) { K__Assert(expr); return 0; }
#define K__ASSERT_RETURN_VAL(expr, val)  if (!(expr)) { K__Assert(expr); return val; }

#define K__ERROR()        K__Error("An error occurred: %s(%d)", __FILE__, __LINE__)
#define K__ERROR_MSG(msg) K__Error("An error occurred: %s(%d): %s", __FILE__, __LINE__, msg)


namespace Kamilo {


void K__Break();
void K__Exit();
bool K__IsDebuggerPresent();

void K__Dialog(const char *u8);
void K__Notify(const char *u8);

void K__RawPrintf(const char *fmt_u8, ...);
void K__DebugPrint(const char *fmt_u8, ...);
void K__Print(const char *fmt_u8, ...);
void K__PrintW(const wchar_t *wfmt, ...);
void K__Error(const char *fmt_u8, ...);
void K__ErrorW(const wchar_t *wfmt, ...);
void K__Warning(const char *fmt_u8, ...);
void K__WarningW(const wchar_t *wfmt, ...);

// KAMILO_VERBOSE が定義されているときのみ動作する
void K__Verbose(const char *fmt_u8, ...);

void K__SetDebugPrintHook(void (*hook)(const char *u8));
void K__SetPrintHook(void (*hook)(const char *u8));
void K__SetWarningHook(void (*hook)(const char *u8));
void K__SetErrorHook(void (*hook)(const char *u8));

// HRESULT hr の値からメッセージを得る
bool K__Win32GetErrorString(long hr, char *buf, int size);
std::string K__Win32GetErrorStringStd(long hr);



//----------------------------------------------------
// path
//----------------------------------------------------
std::string K__PathJoin(const std::string &s1, const std::string &s2);
std::string K__PathRenameExtension(const std::string &path, const std::string &ext);
int K__PathCompare(const char *path1, const char *path2, bool ignore_case, bool ignore_path);
void K__fullpath_u8(char *out_u8, int maxsize, const char *path_u8);



//----------------------------------------------------
// string
//----------------------------------------------------
const char * K__SkipUtf8Bom(const char *s);
bool K__StartsWithUtf8Bom(const void *data, int size);
int K__StrFind(const char *s, const char *sub, int start);
void K__Replace(std::string &s, int start, int count, const char *str);
void K__Replace(std::string &s, const char *before, const char *after);
void K__ReplaceW(wchar_t *s, wchar_t before, wchar_t after);
void K__ReplaceA(char *s, char before, char after);
bool K__strtof(const std::string &s, float *val);
bool K__strtoi(const std::string &s, int *val);
bool K__strtof(const char *s, float *val);
bool K__strtoi(const char *s, int *val);
bool K__strtoui32(const char *s, uint32_t *val);
bool K__strtoui64(const char *s, uint64_t *val);
int K__stricmp(const char *s, const char *t); ///< _stricmp または strcasecmp のエイリアス
char * K__strptime_l(const char *str, const char *fmt, struct tm *out_tm, const char *_locale);
std::string K__vsprintf_std(const char *fmt, va_list args);
std::string K__sprintf_std(const char *fmt, ...);

bool K__iswprint(wchar_t wc); // 印字文字（空白を含む）
bool K__iswgraph(wchar_t wc); // 印字文字（空白を含まない）
bool K__iswblank(wchar_t wc); // 空白文字
bool K__iswhalf(wchar_t wc); // 半角英数


// utf8 ==> wide
int K__Utf8ToWide(wchar_t *out_ws, int max_out_widechars, const char *u8, int u8bytes);
std::wstring K__Utf8ToWideStd(const std::string &u8);

// wide ==> utf8
int K__WideToUtf8(char *out_u8, int max_out_bytes, const wchar_t *ws);
std::string K__WideToUtf8Std(const std::wstring &ws);

// wide ==> ansi
int K__WideToAnsi(char *out_ansi, int max_out_bytes, const wchar_t *ws, const char *_locale);
int K__WideToAnsiL(char *out_ansi, int max_out_bytes, const wchar_t *ws, _locale_t loc);
std::string K__WideToAnsiStd(const std::wstring &ws, const char *_locale);

// ansi ==> wide
int K__AnsiToWide(wchar_t *out_wide, int max_out_wchars, const char *ansi, const char *_locale);
int K__AnsiToWideL(wchar_t *out_wide, int max_out_wchars, const char *ansi, _locale_t loc);
std::wstring K__AnsiToWideStd(const std::string &ansi, const char *_locale);

void K__Utf8ToWidePath(wchar_t *out_wpath, int num_wchars, const char *path_u8);
void K__WideToUtf8Path(char *out_path_u8, int num_bytes, const wchar_t *wpath);


struct _StrW {
	_StrW() {
	}
	_StrW(int x) {
		ws = std::to_wstring(x);
	}
	_StrW(float x) {
		ws = std::to_wstring(x);
	}
	_StrW(const std::string &u8) {
		ws = K__Utf8ToWideStd(u8);
	}
	_StrW(const std::wstring &x) {
		ws = x;
	}
	std::wstring ws;
};

void _OutputW(const std::wstring &ws);

// Win32 の OutputDebugString に可変引数を渡せるようにしたもの。
// デバッガーの「出力ウィンドウ」に対してだけ文字列を出力する
template <class... Args> void K__OutputDebugString(Args... args) {
	_StrW arglist[] = {args...};
	int numargs = sizeof...(args);
	std::wstring ws;
	for (int i=0; i<numargs; i++) {
		ws += arglist[i].ws;
	}
	_OutputW(ws);
	_OutputW(L"\n");
}

class K {
public:
	#pragma region clock
	static uint64_t clockNano64(); ///< システム時刻をナノ秒単位で得る
	static uint64_t clockMsec64(); ///< システム時刻をミリ秒単位で取得する
	static uint32_t clockMsec32(); ///< システム時刻をミリ秒単位で取得する
	#pragma endregion

	#pragma region sleep
	static void sleepPeriodBegin(); ///< タイマーの精度を変更する
	static void sleepPeriodEnd();
	static void sleep(int msec);
	#pragma endregion

	#pragma region numeric
	static float degToRad(float deg);
	static float radToDeg(float rad);
	static float lerp(float a, float b, float t);
	static float min(float a, float b);
	static float max(float a, float b);
	static int min(int a, int b);
	static int max(int a, int b);
	#pragma endregion

	#pragma region file
	static bool fileShellOpen(const std::string &path_u8);
	static FILE * fileOpen(const std::string &path_u8, const std::string &mode_u8);
	static std::string fileLoadString(const std::string &filename);
	static void fileSaveString(const std::string &filename, const std::string &bin);
	#pragma endregion // file

	#pragma region sys
	static uint32_t sysGetCurrentProcessId(); ///< 現在のプロセスIDを得る
	static uint32_t sysGetCurrentThreadId(); ///< 現在のスレッドIDを得る
	static std::string sysGetCurrentDir(); ///< カレントディレクトリを UTF8 で得る
	static bool sysSetCurrentDir(const std::string &dir);
	static std::string sysGetCurrentExecName(); ///< 自分自身（実行ファイル）のファイル名を得る
	static std::string sysGetCurrentExecDir(); ///< 自分自身（実行ファイル）の親ディレクトリを得る
	#pragma endregion // sys

	#pragma region path
	static std::string pathJoin(const std::string &s1, const std::string &s2) { return K__PathJoin(s1, s2); }
	static std::string pathRenameExtension(const std::string &path, const std::string &ext) { return K__PathRenameExtension(path, ext); }
	static int pathCompare(const char *path1, const char *path2, bool ignore_case, bool ignore_path) { K__PathCompare(path1, path2, ignore_case, ignore_path); }
	static std::string pathGetFull(const std::string &s) {
		char t[256] = {0};
		K__fullpath_u8(t, sizeof(t), s.c_str());
		return t;
	}
	#pragma endregion // path

	#pragma region string
	static const char * strSkipBom(const char *s) { return K__SkipUtf8Bom(s); }
	static bool strStartsWithBom(const void *data, int size) { return K__StartsWithUtf8Bom(data, size); }
	static int strFind(const char *s, const char *sub, int start) { return K__StrFind(s, sub, start); }
	static void strReplace(std::string &s, int start, int count, const char *str) { K__Replace(s, start, count, str); }
	static void strReplace(std::string &s, const char *before, const char *after) { K__Replace(s, before, after); }
	static void strReplaceW(wchar_t *s, wchar_t before, wchar_t after) { K__ReplaceW(s, before, after); }
	static void strReplaceA(char *s, char before, char after) { K__ReplaceA(s, before, after); }
	static bool strToFloat(const std::string &s, float *val) { return K__strtof(s, val); }
	static bool strToFloat(const char *s, float *val) { return K__strtof(s, val); }
	static bool strToInt(const std::string &s, int *val) { return K__strtoi(s, val); }
	static bool strToInt(const char *s, int *val) { return K__strtoi(s, val); }
	static bool strToUInt32(const char *s, uint32_t *val) { return K__strtoui32(s, val); }
	static bool strToUInt64(const char *s, uint64_t *val) { return K__strtoui64(s, val); }
	static int strCaseCmp(const char *s, const char *t) { return K__stricmp(s, t); }
	static char * strParseTime(const char *str, const char *fmt, struct tm *out_tm, const char *_locale) { return K__strptime_l(str, fmt, out_tm, _locale); }
	static std::string vsprintf_std(const char *fmt, va_list args) { return K__vsprintf_std(fmt, args); }
	static std::string sprintf_std(const char *fmt, ...) {
		va_list args;
		va_start(args, fmt);
		std::string result = K__vsprintf_std(fmt, args);
		va_end(args);
		return result;
	}

	static bool isPrintW(wchar_t wc) { return K__iswprint(wc); }
	static bool isGraphW(wchar_t wc) { return K__iswgraph(wc); }
	static bool isBlankW(wchar_t wc) { return K__iswblank(wc); }
	static bool isHalfW(wchar_t wc) { return K__iswhalf(wc); }

	// utf8 ==> wide
	static int strUtf8ToWide(wchar_t *out_ws, int max_out_widechars, const char *u8, int u8bytes) { return K__Utf8ToWide(out_ws, max_out_widechars, u8, u8bytes); }
	static std::wstring strUtf8ToWideStd(const std::string &u8) { return K__Utf8ToWideStd(u8); }

	// wide ==> utf8
	static int strWideToUtf8(char *out_u8, int max_out_bytes, const wchar_t *ws) { return K__WideToUtf8(out_u8, max_out_bytes, ws); }
	static std::string strWideToUtf8Std(const std::wstring &ws) { return K__WideToUtf8Std(ws); }

	// wide ==> ansi
	static int strWideToAnsi(char *out_ansi, int max_out_bytes, const wchar_t *ws, const char *_locale) { return K__WideToAnsi(out_ansi, max_out_bytes, ws, _locale); }
	static int strWideToAnsiL(char *out_ansi, int max_out_bytes, const wchar_t *ws, _locale_t loc) { return K__WideToAnsiL(out_ansi, max_out_bytes, ws, loc); }
	static std::string strWideToAnsiStd(const std::wstring &ws, const char *_locale) { return K__WideToAnsiStd(ws, _locale); }

	// ansi ==> wide
	static int strAnsiToWide(wchar_t *out_wide, int max_out_wchars, const char *ansi, const char *_locale) { return K__AnsiToWide(out_wide, max_out_wchars, ansi, _locale); }
	static int strAnsiToWideL(wchar_t *out_wide, int max_out_wchars, const char *ansi, _locale_t loc) { return K__AnsiToWideL(out_wide, max_out_wchars, ansi, loc); }
	static std::wstring strAnsiToWideStd(const std::string &ansi, const char *_locale) { return K__AnsiToWideStd(ansi, _locale); }

	static void strUtf8ToWidePath(wchar_t *out_wpath, int num_wchars, const char *path_u8) { K__Utf8ToWidePath(out_wpath, num_wchars, path_u8); }
	static void strWideToUtf8Path(char *out_path_u8, int num_bytes, const wchar_t *wpath) { K__WideToUtf8Path(out_path_u8, num_bytes, wpath); }
	#pragma endregion // string
};




} // namespace
