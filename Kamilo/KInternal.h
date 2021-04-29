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

void K__SetDebugPrintHook(void (*hook)(const char *u8));
void K__SetPrintHook(void (*hook)(const char *u8));
void K__SetWarningHook(void (*hook)(const char *u8));
void K__SetErrorHook(void (*hook)(const char *u8));

uint64_t K__ClockNano64(); ///< システム時刻をナノ秒単位で得る
inline uint64_t K__ClockMsec64() { return K__ClockNano64() / 1000000; } ///< システム時刻をミリ秒単位で取得する
inline uint32_t K__ClockMsec32() { return (uint32_t)K__ClockMsec64(); } ///< システム時刻をミリ秒単位で取得する


void K__SleepPeriodBegin(); ///< タイマーの精度を変更する
void K__SleepPeriodEnd();
void K__Sleep(int msec);

const char * K__SkipUtf8Bom(const char *s);
bool K__StartsWithUtf8Bom(const void *data, int size);

int K__StrFind(const char *s, const char *sub, int start);
void K__Replace(std::string &s, int start, int count, const char *str);
void K__Replace(std::string &s, const char *before, const char *after);
void K__ReplaceW(wchar_t *s, wchar_t before, wchar_t after);
void K__ReplaceA(char *s, char before, char after);
bool K__strtof(const char *s, float *val);
bool K__strtoi(const char *s, int *val);
bool K__strtoui32(const char *s, uint32_t *val);
bool K__strtoui64(const char *s, uint64_t *val);
int K__stricmp(const char *s, const char *t); ///< _stricmp または strcasecmp のエイリアス
uint32_t K__pid(); ///< プロセスIDを得る
void K__getcwd_u8(char *out_u8, int maxsize); ///< カレントディレクトリを UTF8 で得る
bool K__setcwd_u8(const char *path_u8);
FILE * K__fopen_u8(const char *path_u8, const char *mode_u8);
void K__selfpath_u8(char *out_u8, int maxsize); ///< 自分の名前を得る（実行ファイル名）
void K__fullpath_u8(char *out_u8, int maxsize, const char *path_u8);
std::string K__GetExecDirU8();
std::string K__GetCurrentDirU8();
void K__SetCurrentDirU8(const std::string &dir);
bool K__ShellOpenU8(const char *path_u8); ///< 指定されたファイルまたはフォルダをシステム標準の方法で開く
char * K__strptime_l(const char *str, const char *fmt, struct tm *out_tm, const char *_locale);
std::string K__vsprintf_std(const char *fmt, va_list args);
std::string K__sprintf_std(const char *fmt, ...);

std::string K__PathJoin(const char *p1, const char *p2);
std::string K__PathRenameExtension(const char *path, const char *ext);
int K__PathCompare(const char *path1, const char *path2, bool ignore_case, bool ignore_path);

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


// HRESULT hr の値からメッセージを得る
bool K__Win32GetErrorString(long hr, char *buf, int size);
std::string K__Win32GetErrorStringStd(long hr);


std::string K__LoadStringFromFile(const char *filename_u8);
void K__SaveStringToFile(const char *filename_u8, const std::string &bin);

uint32_t K__GetCurrentThreadId();

float K__DegToRad(float deg);
float K__RadToDeg(float rad);
float K__Min(float a, float b);
float K__Max(float a, float b);
float K__Lerp(float a, float b, float t);
int K__Min(int a, int b);
int K__Max(int a, int b);

} // namespace
