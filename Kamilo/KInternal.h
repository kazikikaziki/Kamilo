﻿/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once

#include <inttypes.h> // uint32_t, uint64_t
#include <stdarg.h> // va_list
#include <string>
#include <vector>

#define K__Assert(expr)  (!(expr) ? (K__Error("[ASSERTION_FAILURE]: %s(%d): %s", __FILE__, __LINE__, #expr), K__Break()) : (void)0)
#define K__Verify(expr)  (!(expr) ? (K__Error("[ASSERTION_FAILURE]: %s(%d): %s", __FILE__, __LINE__, #expr), K__Break()) : (void)0)

#define K__ASSERT_RETURN(expr)           if (!(expr)) { K__Assert(expr); return; }
#define K__ASSERT_RETURN_ZERO(expr)      if (!(expr)) { K__Assert(expr); return 0; }
#define K__ASSERT_RETURN_VAL(expr, val)  if (!(expr)) { K__Assert(expr); return val; }

#define K__ERROR(fmt, ...) K__Error("ERROR!! %s(%d): " fmt, __FILE__, __LINE__, ##__VA_ARGS__)


namespace Kamilo {


#pragma region K_LCID
/// LCID
/// "https://docs.microsoft.com/ja-jp/previous-versions/windows/scripting/cc392381(v=msdn.10)"
const int K_LCID_ENGLISH  = 0x0409; // 米国英語
const int K_LCID_JAPANESE = 0x0411; // 日本語
typedef int K_LCID;
#pragma endregion // K_LCID



void K__Break();
void K__Exit();
bool K__IsDebuggerPresent();

void K__Dialog(const char *u8);
void K__Notify(const char *u8);

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


struct _StrW {
	_StrW();
	_StrW(int x);
	_StrW(float x);
	_StrW(const char *u8);
	_StrW(const std::string &u8);
	_StrW(const std::wstring &x);
	std::wstring ws;
};

//----------------------------------------------------
// string
//----------------------------------------------------
class K {
public:
	static std::string win32_GetErrorString(long hr); // HRESULT hr の値からメッセージを得る
	static void win32_MemoryLeakCheck();
	static void win32_ImmDisableIME();

	#pragma region sys
	static uint32_t sysGetCurrentProcessId(); ///< 現在のプロセスIDを得る
	static uint32_t sysGetCurrentThreadId(); ///< 現在のスレッドIDを得る
	static std::string sysGetCurrentDir(); ///< カレントディレクトリを UTF8 で得る
	static bool sysSetCurrentDir(const std::string &dir);
	static std::string sysGetCurrentExecName(); ///< 自分自身（実行ファイル）のファイル名をフルパスで得る
	static std::string sysGetCurrentExecDir(); ///< 自分自身（実行ファイル）の親ディレクトリをフルパスで得る
	#pragma endregion // sys

	#pragma region output debug string
	static void outputDebugStringU(const std::string &u8);
	static void outputDebugStringW(const std::wstring &ws);
	static void outputDebugFmt(const char *fmt, ...);
	template <class... Args> static void outputDebug(Args... args) {
		_StrW arglist[] = {args...};
		int numargs = sizeof...(args);
		std::wstring ws;
		for (int i=0; i<numargs; i++) {
			ws += arglist[i].ws;
		}
		outputDebugStringW(ws);
	}
	#pragma endregion // output debug string

	#pragma region print
	void print(const char *fmt_u8, ...);
	void printW(const wchar_t *wfmt, ...);
	void debug(const char *fmt_u8, ...);
	void debugW(const wchar_t *wfmt, ...);
	void warning(const char *fmt_u8, ...);
	void warningW(const wchar_t *wfmt, ...);
	void error(const char *fmt_u8, ...);
	void errorW(const wchar_t *wfmt, ...);
	void verbose(const char *fmt_u8, ...); // KAMILO_VERBOSE が定義されているときのみ動作する
	#pragma endregion // print

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
	static bool fileGetSize(const std::string &path_u8, int *out_size); ///< ファイルのバイト数を得る
	static bool fileGetTimeStamp(const std::string &path_u8, time_t *out_time_cma); ///< ファイルのタイムスタンプを得る
	static time_t fileGetTimeStamp_Creation(const std::string &path_u8) { time_t cma[3]; fileGetTimeStamp(path_u8, cma); return cma[0]; }
	static time_t fileGetTimeStamp_Modify(const std::string &path_u8)   { time_t cma[3]; fileGetTimeStamp(path_u8, cma); return cma[1]; }
	static time_t fileGetTimeStamp_Access(const std::string &path_u8)   { time_t cma[3]; fileGetTimeStamp(path_u8, cma); return cma[2]; }
	static bool fileCopy(const std::string &src_u8, const std::string &dst_u8, bool overwrite); ///< ファイルをコピーする
	static bool fileMakeDir(const std::string &dir_u8); ///< ディレクトリを作成する
	static bool fileRemove(const std::string &path_u8); ///< ファイルを削除する
	static bool fileRemoveEmptyDir(const std::string &dir_u8); ///< 空のディレクトリを削除する
	static bool fileRemoveEmptyDirTree(const std::string &dir_u8); ///< 空のディレクトリを再帰的に削除する
	static bool fileRemoveFilesInDir(const std::string &dir_u8); ///< dir ディレクトリ内にある全ての非ディレクトリファイルを削除する
	static bool fileRemoveFilesInDirTree(const std::string &dir_u8); ///< dir ディレクトリ内にある全ての非ディレクトリファイルを再帰的に削除する
	static std::vector<std::string> fileGetListInDir(const std::string &dir_u8); ///< dir 直下のファイル名リストを得る
	static std::vector<std::string> fileGetListInDirTree(const std::string &dir_u8); ///< dir 直下のファイル名リストを得る
	static std::string fileLoadString(const std::string &path_u8);
	static void fileSaveString(const std::string &path_u8, const std::string &bin);
	#pragma endregion // file

	#pragma region path
	static std::string pathJoin(const std::string &path1, const std::string &path2);
	static std::string pathRemoveLastDelim(const std::string &path); ///< パスの末尾が / で終わっている場合、それを取り除く
	static std::string pathAppendLastDelim(const std::string &path); ///< パスの末尾が / で終わるようにする
	static std::string pathGetParent(const std::string &path); ///< 末尾のサブパスを取り除く
	static std::string pathGetLast(const std::string &path); ///< 末尾のサブパスを得る
	static bool pathHasExtension(const std::string &path, const std::string &ext);
	static bool pathHasDelim(const std::string &path); ///< パス区切り文字を含んでいる場合は true
	static std::string pathRenameExtension(const std::string &path, const std::string &ext);
	static std::string pathGetExt(const std::string &path); ///< パスの拡張子部分を返す (ドットを含む)
	static int pathCompare(const std::string &path1, const std::string &path2, bool ignore_case=false, bool ignore_path=false); ///< パス同士を比較する
	static int pathCompareExt(const std::string &path1, const std::string &path2, bool ignore_case=false); ///< 拡張子だけ比較する
	static std::string pathNormalize(const std::string &path); ///< 末尾の区切り文字を取り除き、指定した区切り文字に変換した文字列を得る
	static std::string pathNormalize(const std::string &path, char old_delim, char new_delim); ///< 末尾の区切り文字を取り除き、指定した区切り文字に変換した文字列を得る
	static int pathGetCommonSize(const std::string &path1, const std::string &path2); ///< 2つのパスの先頭部分に含まれる共通のサブパスの文字列長さを得る（区切り文字を含む）
	static bool pathIsRelative(const std::string &path);
	static bool pathIsDir(const std::string &path); ///< パスが実在し、かつディレクトリかどうか調べる
	static bool pathIsFile(const std::string &path); ///< パスが実在し、かつ非ディレクトリかどうか調べる
	static bool pathExists(const std::string &path); /// <パスが実在するか調べる
	static std::string pathGetFull(const std::string &path);
	static std::string pathGetRelative(const std::string &path, const std::string &base); ///< base から path への相対パスを得る
	static bool pathGlob(const char *path, const char *glob);
	static bool pathGlob(const std::string &path, const std::string &glob); ///< path が glob パターンと一致しているかどうか調べる
	static std::vector<std::string> pathSplit(const std::string &s);
	static bool pathStartsWith(const std::string &path, const std::string &sub);
	static bool pathEndsWith(const std::string &path, const std::string &sub);
	#pragma endregion // path

	#pragma region string
	static const char * strSkipBom(const char *s);
	static bool strStartsWithBom(const void *data, int size);
	static bool strStartsWithBom(const std::string &s);
	static int strFind(const std::string &s, const std::string &sub, int start=0);
	static int strFindChar(const char *s, char chr, int start=0);
	static int strFindChar(const std::string &s, char chr, int start=0);
	static void strReplace(std::string &s, int start, int count, const std::string &str);
	static void strReplace(std::string &s, const std::string &before, const std::string &after);
	static void strReplaceChar(char *s, char before, char after);
	static void strReplaceChar(wchar_t *s, wchar_t before, wchar_t after);
	static void strReplaceChar(std::string &s, char before, char after);
	static void strReplaceChar(std::wstring &ws, wchar_t before, wchar_t after);
	static bool strStartsWith(const char *s, const char *sub);
	static bool strStartsWith(const std::string &s, const std::string &sub);
	static bool strEndsWith(const char *s, const char *sub);
	static bool strEndsWith(const std::string &s, const std::string &sub);
	static void strTrim(std::string &s);
	static std::vector<std::string> strSplit(const std::string &s, const std::string &delims, int maxcount=0, bool condense_delims=true, bool _trim=true);
	static std::vector<std::string> strSplitLines(const std::string &s, bool skip_empty_lines=true, bool _trim=true);
	static bool strToFloat(const std::string &s, float *p_val);
	static bool strToFloat(const char *s, float *p_val);
	static bool strToInt(const std::string &s, int *p_val);
	static bool strToInt(const char *s, int *p_val);
	static bool strToUInt32(const std::string &s, uint32_t *p_val);
	static bool strToUInt32(const char *s, uint32_t *p_val);
	static bool strToUInt64(const std::string &s, uint64_t *p_val);
	static bool strToUInt64(const char *s, uint64_t *p_val);
	static int str_stricmp(const char *s, const char *t); ///< _stricmp, strcasecmp
	static int str_stricmp(const std::string &s, const std::string &t);
	static char * str_strptime(const char *str, const char *fmt, struct tm *out_tm, const char *_locale); ///< strptime の代替関数
	static std::string str_vsprintf(const char *fmt, va_list args);
	static std::string str_sprintf(const char *fmt, ...);
	static bool str_iswprint(wchar_t wc);
	static bool str_iswgraph(wchar_t wc);
	static bool str_iswblank(wchar_t wc);
	static bool str_iswhalf(wchar_t wc);
	static bool str_iswpathdelim(wchar_t wc);
	static bool str_ispathdelim(char ch);
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
	static std::wstring strBinToWide(const std::string &bin);
	static std::string strBinToUtf8(const std::string &bin);
	#pragma endregion // string
};


namespace Test {
void Test_internal_path();
}

} // namespace
