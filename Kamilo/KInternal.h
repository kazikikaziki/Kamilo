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
// string
//----------------------------------------------------


// ansi ==> wide
void K__Utf8ToWidePath(wchar_t *out_wpath, int num_wchars, const char *path_u8);
void K__WideToUtf8Path(char *out_path_u8, int num_bytes, const wchar_t *wpath);



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
	static std::string sysGetCurrentExecName(); ///< 自分自身（実行ファイル）のファイル名をフルパスで得る
	static std::string sysGetCurrentExecDir(); ///< 自分自身（実行ファイル）の親ディレクトリをフルパスで得る
	#pragma endregion // sys

	#pragma region path
	static std::string pathJoin(const std::string &s1, const std::string &s2);
	static std::string pathRenameExtension(const std::string &path, const std::string &ext);
	static int pathCompare(const std::string &path1, const std::string &path2, bool ignore_case, bool ignore_path);
	static std::string pathGetFull(const std::string &s);
	#pragma endregion // path

	#pragma region string
	static const char * strSkipBom(const char *s);
	static bool strStartsWithBom(const void *data, int size);
	static bool strStartsWithBom(const std::string &s);
	static int strFind(const std::string &s, const std::string &sub, int start);
	static void strReplace(std::string &s, int start, int count, const std::string &str);
	static void strReplace(std::string &s, const std::string &before, const std::string &after);
	static void strReplaceChar(char *s, char before, char after);
	static void strReplaceChar(wchar_t *s, wchar_t before, wchar_t after);
	static bool strToFloat(const std::string &s, float *val);
	static bool strToFloat(const char *s, float *val);
	static bool strToInt(const std::string &s, int *val);
	static bool strToInt(const char *s, int *val);
	static bool strToUInt32(const char *s, uint32_t *val);
	static bool strToUInt64(const char *s, uint64_t *val);
	static int str_stricmp(const char *s, const char *t); ///< _stricmp, strcasecmp
	static char * str_strptime(const char *str, const char *fmt, struct tm *out_tm, const char *_locale); ///< strptime の代替関数
	static std::string str_vsprintf(const char *fmt, va_list args);
	static std::string str_sprintf(const char *fmt, ...);
	static bool str_iswprint(wchar_t wc);
	static bool str_iswgraph(wchar_t wc);
	static bool str_iswblank(wchar_t wc);
	static bool str_iswhalf(wchar_t wc);
	static int strUtf8ToWide(wchar_t *out_ws, int max_out_widechars, const char *u8, int u8bytes);
	static int strWideToUtf8(char *out_u8, int max_out_bytes, const wchar_t *ws);
	static int strWideToAnsi(char *out_ansi, int max_out_bytes, const wchar_t *ws, const char *_locale);
	static int strWideToAnsiL(char *out_ansi, int max_out_bytes, const wchar_t *ws, _locale_t loc);
	static std::wstring strUtf8ToWide(const std::string &u8);
	static std::string strWideToUtf8(const std::wstring &ws);
	static std::string strWideToAnsi(const std::wstring &ws, const char *_locale);
	static std::wstring strAnsiToWide(const std::string &ansi, const char *_locale);
	static std::string strAnsiToUtf8(const std::string &ansi, const char *_locale);
	static std::string strUtf8ToAnsi(const std::string &u8, const char *_locale);
	static int strAnsiToWide(wchar_t *out_wide, int max_out_wchars, const char *ansi, const char *_locale);
	static int strAnsiToWideL(wchar_t *out_wide, int max_out_wchars, const char *ansi, _locale_t loc);

	static void strUtf8ToWidePath(wchar_t *out_wpath, int num_wchars, const char *path_u8) { K__Utf8ToWidePath(out_wpath, num_wchars, path_u8); }
	static void strWideToUtf8Path(char *out_path_u8, int num_bytes, const wchar_t *wpath) { K__WideToUtf8Path(out_path_u8, num_bytes, wpath); }
	#pragma endregion // string
};



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
		ws = K::strUtf8ToWide(u8);
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


} // namespace
