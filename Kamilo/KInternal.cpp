#include "KInternal.h"

#include <iomanip> // std::get_time
#include <sstream> // std::istringstream
#define _USE_MATH_DEFINES // M_PI
#include <math.h>
#include <locale.h> // _create_locale, _free_locale
#include <Windows.h> // MultiByteToWideChar, WideCharToMultiByte
#include <Shlwapi.h> // PathFindFileNameW


#define K_SPRINTF_BUFSIZE    (1024 * 4)
#define K_USE_MBSTOWCS_SAFE  1
#define K_USE_WCSTOMBS_SAFE  1

namespace Kamilo {

static void (*g_DebugPrintHook)(const char *u8) = nullptr;
static void (*g_PrintHook)(const char *u8) = nullptr;
static void (*g_WarningHook)(const char *u8) = nullptr;
static void (*g_ErrorHook)(const char *u8) = nullptr;

void K__SetDebugPrintHook(void (*hook)(const char *u8)) {
	g_DebugPrintHook = hook;
}
void K__SetPrintHook(void (*hook)(const char *u8)) {
	g_PrintHook = hook;
}
void K__SetWarningHook(void (*hook)(const char *u8)) {
	g_WarningHook = hook;
}
void K__SetErrorHook(void (*hook)(const char *u8)) {
	g_ErrorHook = hook;
}
void _OutputW(const std::wstring &ws) {
	OutputDebugStringW(ws.c_str());
}

static void K__WPrintLn(const wchar_t *ws) {
#if 1
	// windows では wprintf があまり信用できないので普通の printf を使う
	std::string mb = K::strWideToAnsi(ws, "");
	printf("%s\n", mb.c_str());
#else
	wprintf(L"%s\n", ws);
#endif

	OutputDebugStringW(ws);
	OutputDebugStringW(L"\n");
}

void K__RawPrintf(const char *fmt_u8, ...) {
	// 無限再帰防止のため、いかなるライブラリ関数も呼んではいけない (K_assert 含む)
	char s[1024 * 4];
	va_list args;
	va_start(args, fmt_u8);
	vsnprintf(s, sizeof(s), fmt_u8, args);
	va_end(args);

	printf("%s\n", s);
	OutputDebugStringA(s);
	OutputDebugStringA("\n");
}
void K__DebugPrint(const char *fmt_u8, ...) {
	char u8[K_SPRINTF_BUFSIZE] = {0};
	va_list args;
	va_start(args, fmt_u8);
	vsnprintf(u8, sizeof(u8), fmt_u8, args);
	va_end(args);
	if (g_DebugPrintHook) {
		g_DebugPrintHook(u8);
	} else {
		std::wstring ws = K::strUtf8ToWide(u8);
		K__WPrintLn(ws.c_str());
	}
}
void K__Print(const char *fmt_u8, ...) {
	char u8[K_SPRINTF_BUFSIZE] = {0};
	va_list args;
	va_start(args, fmt_u8);
	vsnprintf(u8, sizeof(u8), fmt_u8, args);
	va_end(args);
	if (g_PrintHook) {
		g_PrintHook(u8);
	} else {
		std::wstring ws = K::strUtf8ToWide(u8);
		K__WPrintLn(ws.c_str());
	}
}
void K__PrintW(const wchar_t *wfmt, ...) {
	wchar_t ws[K_SPRINTF_BUFSIZE] = {0};
	va_list args;
	va_start(args, wfmt);
	vswprintf(ws, sizeof(ws)/sizeof(wchar_t), wfmt, args);
	va_end(args);
	if (g_PrintHook) {
		std::string u8 = K::strWideToUtf8(ws);
		g_PrintHook(u8.c_str());
	} else {
		K__WPrintLn(ws);
	}
}
void K__Warning(const char *fmt_u8, ...) {
	char u8[K_SPRINTF_BUFSIZE] = {0};
	va_list args;
	va_start(args, fmt_u8);
	vsnprintf(u8, sizeof(u8), fmt_u8, args);
	va_end(args);
	if (g_WarningHook) {
		g_WarningHook(u8);
	} else {
		std::wstring ws = K::strUtf8ToWide(u8);
		K__WPrintLn(ws.c_str());
	}
}
void K__WarningW(const wchar_t *wfmt, ...) {
	wchar_t ws[K_SPRINTF_BUFSIZE] = {0};
	va_list args;
	va_start(args, wfmt);
	vswprintf(ws, sizeof(ws)/sizeof(wchar_t), wfmt, args);
	va_end(args);
	if (g_WarningHook) {
		std::string u8 = K::strWideToUtf8(ws);
		g_WarningHook(u8.c_str());
	} else {
		K__WPrintLn(ws);
	}
}
void K__Error(const char *fmt_u8, ...) {
	char u8[K_SPRINTF_BUFSIZE] = {0};
	va_list args;
	va_start(args, fmt_u8);
	vsnprintf(u8, sizeof(u8), fmt_u8, args);
	va_end(args);
	if (g_ErrorHook) {
		g_ErrorHook(u8);
	} else {
		std::wstring ws = K::strUtf8ToWide(u8);
		K__WPrintLn(ws.c_str());
	}
}
void K__ErrorW(const wchar_t *wfmt, ...) {
	wchar_t ws[K_SPRINTF_BUFSIZE] = {0};
	va_list args;
	va_start(args, wfmt);
	vswprintf(ws, sizeof(ws)/sizeof(wchar_t), wfmt, args);
	va_end(args);
	if (g_ErrorHook) {
		std::string u8 = K::strWideToUtf8(ws);
		g_ErrorHook(u8.c_str());
	} else {
		K__WPrintLn(ws);
	}
}

void K__Verbose(const char *fmt_u8, ...) {
#ifdef KAMILO_VERBOSE
	char u8[K_SPRINTF_BUFSIZE] = {0};
	va_list args;
	va_start(args, fmt_u8);
	vsnprintf(u8, sizeof(u8), fmt_u8, args);
	va_end(args);
	if (g_WarningHook) {
		g_WarningHook(u8);
	} else {
		std::wstring ws = K::strUtf8ToWide(u8);
		K__WPrintLn(ws.c_str());
	}
#endif
}


































void K__Utf8ToWidePath(wchar_t *out_wpath, int num_wchars, const char *path_u8) {
	// windows形式のパスに変換する
	K::strUtf8ToWide(out_wpath, num_wchars, path_u8, 0);
	K::strReplaceChar(out_wpath, K__PATH_SLASHW, K__PATH_BACKSLASHW);
}
void K__WideToUtf8Path(char *out_path_u8, int num_bytes, const wchar_t *wpath) {
	K::strWideToUtf8(out_path_u8, num_bytes, wpath);
	K::strReplaceChar(out_path_u8, K__PATH_BACKSLASH, K__PATH_SLASH);
}

std::wstring K__Utf8ToWidePath(const std::string &path_u8) {
	// windows形式のパスに変換する
	std::wstring ws = K::strUtf8ToWide(path_u8);
	K::strReplaceChar(ws, K__PATH_SLASHW, K__PATH_BACKSLASHW);
	return ws;
}
std::string K__WideToUtf8Path(const std::wstring &wpath) {
	// windows形式のパスに変換する
	std::string s = K::strWideToUtf8(wpath);
	K::strReplaceChar(s, K__PATH_BACKSLASH, K__PATH_SLASH);
	return s;
}


#pragma region clock
uint64_t K::clockNano64() {
	#ifdef _WIN32
	{
		static LARGE_INTEGER s_freq = {0, 0};
		if (s_freq.QuadPart == 0) {
			::QueryPerformanceFrequency(&s_freq);
		}
		// https://msdn.microsoft.com/ja-jp/library/cc410966.aspx
		// マルチプロセッサ環境の場合、QueryPerformanceCounter が必ず同じプロセッサで実行されるように固定する必要がある
		// SetThreadAffinityMask を使い、指定したスレッドを実行可能なCPUを限定する
		LARGE_INTEGER time;
		HANDLE hThread = ::GetCurrentThread();
		DWORD_PTR oldmask = ::SetThreadAffinityMask(hThread, 1);
		::QueryPerformanceCounter(&time);
		::SetThreadAffinityMask(hThread, oldmask);
		return (uint64_t)((double)time.QuadPart / s_freq.QuadPart * 1000000000.0);
	}
	#else
	{
		struct timespec time;
		::clock_gettime(CLOCK_MONOTONIC, &time);
		return (uint64_t)time.tv_sec * 1000000000 + time.tv_nsec;
	}
	#endif
}
uint64_t K::clockMsec64() {
	return clockNano64() / 1000000;
}
uint32_t K::clockMsec32() {
	return (uint32_t)clockMsec64();
}
#pragma endregion // clock





#pragma region sleep
static TIMECAPS g_TimeCaps;

void K::sleepPeriodBegin() {
	::timeGetDevCaps(&g_TimeCaps, sizeof(TIMECAPS));
	::timeBeginPeriod(g_TimeCaps.wPeriodMin);
}
void K::sleepPeriodEnd() {
	::timeEndPeriod(g_TimeCaps.wPeriodMin);
}
void K::sleep(int msec) {
	// だいたいmsecミリ秒待機する。
	// 精度は問題にせず、適度な時間待機できればそれで良い。
	// エラーによるスリープ中断 (nanosleepの戻り値チェック) も考慮しない
	::Sleep(msec);
}
#pragma endregion // sleep




#pragma region numeric
float K::degToRad(float deg) {
	return (float)M_PI * deg / 180.0f;
}
float K::radToDeg(float rad) {
	return 180.0f * rad / (float)M_PI;
}
float K::min(float a, float b) { // NOMINMAX が未定義だと重複エラーの可能性あり
	return a<b ? a : b;
}
float K::max(float a, float b) { // NOMINMAX が未定義だと重複エラーの可能性あり
	return a>b ? a : b;
}
int K::min(int a, int b) {
	return a<b ? a : b;
}
int K::max(int a, int b) {
	return a>b ? a : b;
}
float K::lerp(float a, float b, float t) {
	t = (t < 0.0f) ? 0.0f : (t < 1.0f) ? t : 1.0f;
	return a + (b - a) * t;
}
#pragma endregion // numeric




#pragma region file
bool K::fileShellOpen(const std::string &path_u8) {
	std::wstring wpath = strUtf8ToWide(path_u8);
	int h = (int)::ShellExecuteW(nullptr, L"OPEN", wpath.c_str(), L"", L"", SW_SHOW);
	return h > 32; // ShellExecute は成功すると 32 より大きい値を返す
}
FILE * K::fileOpen(const std::string &path_u8, const std::string &mode_u8) {
	// fopen の UTF8 版
	std::wstring wpath = strUtf8ToWide(path_u8);
	std::wstring wmode = strUtf8ToWide(mode_u8);
	FILE *file = nullptr;
	if (1) {
		// _wfopen で開く
		// 読み取りモードで開く場合は、他のアプリが対象ファイルを開いていても成功する
		file = ::_wfopen(wpath.c_str(), wmode.c_str());
		if (file == nullptr) {
			K__OutputDebugString("*** Failed to open file \"", path_u8, "\"");
		}
	} else {
		errno_t err = ::_wfopen_s(&file, wpath.c_str(), wmode.c_str());
		// errno constants
		// https://docs.microsoft.com/ja-jp/cpp/c-runtime-library/errno-constants?view=msvc-160
		if (err == ENOENT) {
			// 指定された名前のファイルが存在しない
			K__OutputDebugString("*** No file named \"", path_u8, "\"");
		}
		if (err == EACCES) {
			// 指定されたモードでアクセスできない (_wfopen だと成功するかも）
			K__OutputDebugString("*** Permission denied \"", path_u8, "\"");
		}
	}
	return file;
}

/// 指定されたファイルのバイト数を得る
/// サイズを取得できない場合は false を返す
bool K::fileGetSize(const std::string &path_u8, int *out_size) {
	std::wstring wpath = K__Utf8ToWidePath(path_u8);
	//
	HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD dwSize = GetFileSize(hFile, nullptr);
		CloseHandle(hFile);
		if (dwSize != (DWORD)(-1)) {
			if (out_size) *out_size = (int)dwSize;
			return true;
		}
	}
	return false;
}

/// FILETIME から time_t へ変換する
static time_t _FILETIME_to_timet(const FILETIME *ft) {
	// FILETIME ==> time_t
	K__Assert(ft);
	K__Assert(sizeof(time_t) == 8); // 64bit
	// FILETIMEを等価な64ビットのファイル時刻形式（1601/1/1 0:00から100ナノ秒刻み）に変換
	uint64_t ntfs64 = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
	// NTFSで使われている64ビットのファイル時刻形式から
	// time_t 形式（1970/1/1 0:00から1秒刻み）を得る
	uint64_t basetime = 116444736000000000; // FILETIME at 1970/1/1 0:00
	uint64_t nsec  = 10000000; // 100nsec --> sec (10 * 1000 * 1000)
	return (time_t)((ntfs64 - basetime) / nsec);
}


/// パスで指定されたファイルのタイムスタンプを得る
/// time_cma time_t の配列 Create, Modify, Access の順で値が入る
/// 時刻を取得できない場合は false を返す
/// @code
///		time_t cma[] = {0, 0, 0};
///		K_PathGetTimeStamp("myfile.txt", cma);
///		// cma[0] <-- creation time
///		// cma[1] <-- modify time
///		// cma[2] <-- access time
/// @endcode
bool K::fileGetTimeStamp(const std::string &path_u8, time_t *out_time_cma) {
	std::wstring wpath = K__Utf8ToWidePath(path_u8);
	//
	HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile != INVALID_HANDLE_VALUE) {
		FILETIME ctm, mtm, atm;
		GetFileTime(hFile, &ctm, &atm, &mtm);
		CloseHandle(hFile);
		if (out_time_cma) {
			out_time_cma[0] = _FILETIME_to_timet(&ctm); // creation
			out_time_cma[1] = _FILETIME_to_timet(&mtm); // modify
			out_time_cma[2] = _FILETIME_to_timet(&atm); // access
		}
		return true;
	}
	return false;
}

std::string K::fileLoadString(const std::string &path_u8) {
	std::string bin;
	FILE *file = fileOpen(path_u8, "r");
	if (file) {
		char buf[1024];
		while (1) {
			int sz = ::fread(buf, 1, sizeof(buf), file);
			if (sz == 0) break;
			bin.append(buf, sz);
		}
		::fclose(file);
	}
	return bin;
}
void K::fileSaveString(const std::string &path_u8, const std::string &bin) {
	FILE *file = fileOpen(path_u8, "w");
	if (file) {
		::fwrite(bin.data(), bin.size(), 1, file);
		::fclose(file);
	}
}
#pragma endregion // file




#pragma region sys
/// getpid, GetCurrentProcessId の代替関数
///
/// プロセスIDを得る
/// @see https://linuxjm.osdn.jp/html/LDP_man-pages/man2/getpid.2.html
uint32_t K::sysGetCurrentProcessId() {
	return ::GetCurrentProcessId();
}

uint32_t K::sysGetCurrentThreadId() {
	return ::GetCurrentThreadId();
}

/// getcwd, GetCurrentDirectory の UTF8 版
/// @see https://linuxjm.osdn.jp/html/LDP_man-pages/man2/getcwd.2.html
std::string K::sysGetCurrentDir() {
	wchar_t wpath[MAX_PATH] = {0};
	::GetCurrentDirectoryW(MAX_PATH, wpath);
	strReplaceChar(wpath, K__PATH_BACKSLASHW, K__PATH_SLASHW);
	return strWideToUtf8(wpath);
}

/// chdir, SetCurrentDirectory の UTF8 版
///
/// カレントディレクトリ名を utf8 で得る
/// @see https://linuxjm.osdn.jp/html/LDP_man-pages/man2/chdir.2.html
bool K::sysSetCurrentDir(const std::string &dir) {
	std::wstring wdir = K__Utf8ToWidePath(dir);
	if (::SetCurrentDirectoryW(wdir.c_str())) {
		return true; // OK
	} else {
		K__ERROR_MSG(dir.c_str());
		return false; // FAIL
	}
}

std::string K::sysGetCurrentExecName() {
	wchar_t wpath[MAX_PATH] = {0};
	::GetModuleFileNameW(nullptr, wpath, MAX_PATH);
	strReplaceChar(wpath, L'\\', L'/');
	return strWideToUtf8(wpath);
}
std::string K::sysGetCurrentExecDir() {
	wchar_t wpath[MAX_PATH] = {0};
	::GetModuleFileNameW(nullptr, wpath, MAX_PATH);
	::PathRemoveFileSpecW(wpath);
	strReplaceChar(wpath, L'\\', L'/');
	return strWideToUtf8(wpath);
}
#pragma endregion // sys




#pragma region path
std::string K::pathJoin(const std::string &s1, const std::string &s2) {
	if (s1.empty()) return s2;
	if (s2.empty()) return s1;
	if (pathIsRelative(s2)) {
		std::string ss1 = pathRemoveLastDelim(s1);
		return ss1 + "/" + s2;
	} else {
		return s2; // 後続が絶対パスの場合は連結しない
	}
}

std::string K::pathRemoveLastDelim(const std::string &path) {
	std::string s = path;
	if (s.back() == K__PATH_BACKSLASH) {
		s.pop_back();
	}
	if (s.back() == K__PATH_SLASH) {
		s.pop_back();
	}
	return s;
}
std::string K::pathAppendLastDelim(const std::string &path) {
	std::string s = path;
	if (s.back() == K__PATH_BACKSLASH) {
		s.pop_back();
	}
	if (s.back() != K__PATH_SLASH) {
		s.push_back(K__PATH_SLASH);
	}
	return s;
}

static int _FindLastDelim(const std::string &path) {
	int sl = path.find_last_of(K__PATH_SLASH);
	int bs = path.find_last_of(K__PATH_BACKSLASH);
	return (sl > bs) ? sl : bs; // max(sl, bs)
}

std::string K::pathGetParent(const std::string &path) {
	int pos = _FindLastDelim(path);
	if (pos >= 0) {
		return path.substr(0, pos);
	} else {
		return "";
	}
}
std::string K::pathGetLast(const std::string &path) {
	int pos = _FindLastDelim(path);
	if (pos >= 0) {
		return path.substr(pos+1);
	} else {
		return path;
	}
}

bool K::pathHasDelim(const std::string &path) {
	if ((int)path.find(K__PATH_SLASH) >= 0) {
		return true;
	}
	if ((int)path.find(K__PATH_BACKSLASH) >= 0) {
		return true;
	}
	return false;
}

/// パス末尾の拡張子部分を返す (ドットを含む)
std::string K::pathGetExt(const std::string &path) {
	// 末尾の "." を探す
	int pos = (int)path.find_last_of('.');
	if (pos < 0) {
		return "";
	}

	// "." よりも後にパス区切り文字 "/" があったらダメ (例: "aaa.zip/ccc")
	// それは拡張子とはみなさない
	if ((int)path.find_last_of(K__PATH_SLASH) > pos) {
		return "";
	}

	// "." よりも後にパス区切り文字 "\\" があったらダメ (例: "aaa.zip/ccc")
	// それは拡張子とはみなさない
	if ((int)path.find_last_of(K__PATH_BACKSLASH) > pos) {
		return "";
	}
	return path.substr(pos);
}

std::string K::pathRenameExtension(const std::string &path, const std::string &ext) {
	size_t pos = path.rfind('.');
	if (pos != std::string::npos) {
		return path.substr(0, pos) + ext;
	} else {
		return path + ext;
	}
}
int K::pathCompare(const std::string &path1, const std::string &path2, bool ignore_case, bool ignore_path) {
	if (ignore_path) {
		const char *s1 = strrchr(path1.c_str(), K__PATH_SLASH);
		const char *s2 = strrchr(path2.c_str(), K__PATH_SLASH);
		if (s1) { s1++; /* K__PATH_SLASH の次の文字へ */ } else { s1 = path1.c_str(); }
		if (s2) { s2++; /* K__PATH_SLASH の次の文字へ */ } else { s2 = path2.c_str(); }
		return ignore_case ? str_stricmp(s1, s2) : strcmp(s1, s2);
	
	} else {
		const char *s1 = path1.c_str();
		const char *s2 = path2.c_str();
		return ignore_case ? str_stricmp(s1, s2) : strcmp(s1, s2);
	}
}
// 末尾の区切り文字を取り除き、指定した区切り文字に変換した文字列を得る
std::string K::pathNormalize(const std::string &path, char old_delim, char new_delim) {
	K__Assert(isprint(old_delim));
	K__Assert(isprint(new_delim));
	std::string s = path;
	strTrim(s); // 前後の空白を削除
	strReplaceChar(s, old_delim, new_delim); // 区切り文字を置換
	return pathRemoveLastDelim(s); // 最後の区切り文字は削除する
}
std::string K::pathNormalize(const std::string &path) {
	return pathNormalize(path, K__PATH_BACKSLASH, K__PATH_SLASH);
}

/// 2つのパスの先頭部分に含まれる共通のサブパスの文字列長さを得る（区切り文字を含む）
int K::pathGetCommonSize(const std::string &path1, const std::string &path2) {
	int len = 0;
	for (int i=0; ; i++) {
		if (path1[i] == path2[i]) {
			if (path1[i] == K__PATH_SLASH || path1[i] == K__PATH_BACKSLASH) {
				len = i+1;
			}
			if (path1[i] == '\0') {
				len = i;
				break;
			}
		} else {
			break;
		}
	}
	return len;
}

std::string K::pathGetFull(const std::string &s) {
	std::wstring wpath = K__Utf8ToWidePath(s);
	wchar_t wfull[MAX_PATH] = {0};
	if (_wfullpath(wfull, wpath.c_str(), MAX_PATH)) {
		strReplaceChar(wfull, K__PATH_BACKSLASH, K__PATH_SLASH);
		return strWideToUtf8(wfull);
	} else {
		return s;
	}
}

/// base から path への相対パスを得る
std::string K::pathGetRelative(const std::string &path, const std::string &base) {
	// パス区切り文字で区切る
	auto tok_path = strSplit(path, "/\\", true, true, 0, nullptr);
	auto tok_base = strSplit(base, "/\\", true, true, 0, nullptr);
	int numtok_path = tok_path.size();
	int numtok_base = tok_base.size();

	// 先頭の共通パス部分をスキップ
	int c = 0;
	while (c<numtok_path && c<numtok_base) {
		if (tok_path[c].compare(tok_base[c]) != 0) {
			break;
		}
		c++;
	}

	// 必要な数だけ ".." で上に行く
	std::string relpath;
	for (int i=c; i<numtok_base; i++) {
		relpath = pathJoin(relpath, "..");
	}

	// 登り切ったので、今度は目的地までパスを下げていく
	for (int i=c; i<numtok_path; i++) {
		int len = tok_path[i].size();
		relpath = pathJoin(relpath, tok_path[i]);
	}

	return relpath;
}

/// ファイル名 glob パターンと一致しているかどうかを調べる
///
/// ワイルドカードは * のみ利用可能。
/// ワイルドカード * はパス区切り記号 / とは一致しない。
/// @code
/// K_PathGlob("abc", "*") ===> true
/// K_PathGlob("abc", "a*") ===> true
/// K_PathGlob("abc", "ab*") ===> true
/// K_PathGlob("abc", "abc*") ===> true
/// K_PathGlob("abc", "a*c") ===> true
/// K_PathGlob("abc", "*abc") ===> true
/// K_PathGlob("abc", "*bc") ===> true
/// K_PathGlob("abc", "*c") ===> true
/// K_PathGlob("abc", "*a*b*c*") ===> true
/// K_PathGlob("abc", "*bc*") ===> true
/// K_PathGlob("abc", "*c*") ===> true
/// K_PathGlob("aaa/bbb.ext", "a*.ext") ===> false // ワイルドカード * はパス区切り文字を含まない
/// K_PathGlob("aaa/bbb.ext", "a*/*.ext") ===> true
/// K_PathGlob("aaa/bbb.ext", "a*/*.*t") ===> true
/// K_PathGlob("aaa/bbb.ext", "aaa/*.ext") ===> true
/// K_PathGlob("aaa/bbb.ext", "aaa/bbb*ext") ===> true
/// K_PathGlob("aaa/bbb.ext", "aaa*bbb*ext") ===> false
/// K_PathGlob("aaa/bbb/ccc.ext", "aaa/*/ccc.ext") ===> true
/// K_PathGlob("aaa/bbb/ccc.ext", "aaa/*.ext") ===> false
/// K_PathGlob("aaa/bbb.ext", "*.aaa") ===> false
/// K_PathGlob("aaa/bbb.ext", "aaa*bbb") ===> false
/// K_PathGlob("aaa/bbb.ext", "*/*.ext") ===> true
/// K_PathGlob("aaa/bbb.ext", "*/*.*") ===> true
/// @endcode
bool K::pathGlob(const char *path, const char *glob) {
	if (path == nullptr || path[0] == '\0') {
		return false;
	}
	if (glob == nullptr || glob[0] == '\0') {
		return false;
	}
	if (glob[0]=='*' && glob[1]=='\0') {
		return true;
	}
	int p = 0;
	int g = 0;
	while (1) {
		const char *sp = path + p;
		const char *sg = glob + g;
		if (*sg == '*') {
			if (/*strcmp(sg, "*") == 0*/sg[0]=='*' && sg[1]=='\0') {
				return true; // 末尾が * だった場合は全てにマッチする
			}
			const char *subglob = sg + 1; // '*' の次の文字列
			for (const char *subpath=sp; *subpath; subpath++) {
				if (subpath[0] == K__PATH_SLASH && subglob[0] != K__PATH_SLASH) {
					return false; // 区切り文字を跨いで判定しない
				}
				if (pathGlob(subpath, subglob)) {
					return true;
				}
			}
			return false;
		}
		if (*sp == K__PATH_SLASH && *sg == '\0') {
			return true; // パス単位で一致しているならOK
		}
		if (*sp == L'\0' && *sg == '\0') {
			return true;
		}
		if (*sp != *sg) {
			return false;
		}
		p++;
		g++;
	}
	return true;
}
bool K::pathGlob(const std::string &path, const std::string &glob) {
	return pathGlob(path.c_str(), glob.c_str());
}

bool K::pathIsRelative(const std::string &path) {
	std::wstring wpath = K__Utf8ToWidePath(path);
	return PathIsRelativeW(wpath.c_str());
}
bool K::pathIsDir(const std::string &path) {
	// パスが実在し、かつディレクトリである
	std::wstring wpath = K__Utf8ToWidePath(path);
	return PathIsDirectoryW(wpath.c_str());
}
bool K::pathIsFile(const std::string &path) {
	// パスが実在し、かつ非ディレクトリである
	std::wstring wpath = K__Utf8ToWidePath(path);
	return PathFileExistsW(wpath.c_str()) && !PathIsDirectoryW(wpath.c_str()); // パスが存在し、かつディレクトリでないならファイルであるとする
}
bool K::pathExists(const std::string &path) {
	std::wstring wpath = K__Utf8ToWidePath(path);
	return PathFileExistsW(wpath.c_str());
}
#pragma endregion // path




#pragma region string
/// u8の先頭が utf8 bom で始まっているなら、その次の文字アドレスを返す。
/// utf8 bom で始まっていない場合はそのまま u8 を返す
const char * K::strSkipBom(const char *s) {
	K__Assert(s);
	if (strncmp(s, K__UTF8BOM_STR, K__UTF8BOM_LEN) == 0) {
		return s + K__UTF8BOM_LEN;
	} else {
		return s;
	}
}

bool K::strStartsWithBom(const void *data, int size) {
	return (size >= K__UTF8BOM_LEN) && (memcmp(data, K__UTF8BOM_STR, K__UTF8BOM_LEN) == 0);
}
bool K::strStartsWithBom(const std::string &s) {
	return strStartsWithBom(s.data(), s.size());
}

/// インデックス start 以降の部分から、文字列 sub に一致する部分を探す。
/// みつかれば、その先頭インデックスを返す。そうでなければ -1 を返す。
/// sub に空文字列を指定した場合は常に検索開始位置 (start) を返す
int K::strFind(const std::string &s, const std::string &sub, int start) {
	if (start < 0) start = 0;
	if ((int)s.size() < start) return -1; // 範囲外
	if ((int)s.size() == start) return sub.empty() ? start : -1; // 検索開始位置が文字列末尾の場合、空文字列だけがマッチする
	if (sub.empty()) return start; // 空文字列はどの場所であっても必ずマッチする。常に検索開始位置にマッチする
	const char *p = strstr(s.c_str() + start, sub.c_str());
	return p ? (p - s.c_str()) : -1;
}

/// インデックス start 以降の部分から文字 chr に一致する部分を探す
/// みつかれば、そのインデックスを返す。そうでなければ -1 を返す。
int K::strFindChar(const char *s, char chr, int start) {
	K__Assert(s);
	if (start < 0) start = 0;
	if ((int)strlen(s) <= start) return -1;
	const char *p = strchr(s + start, chr);
	return p ? (p - s) : -1;
}
int K::strFindChar(const std::string &s, char chr, int start) {
	return strFindChar(s.c_str(), chr, start);
}

/// start, count で指定された範囲の部分文字列を str で置換する。
/// 置換前と置換後の文字数が変化してもよい。
/// start には 0 以上 size() 未満の開始インデックスを指定する。
/// count には文字数を指定する。-1 を指定した場合、残り全ての文字を対象にする。
/// 不正な引数を指定した場合、関数は失敗し、文字列は変化しない。
void K::strReplace(std::string &s, int start, int count, const std::string &str) {
#if 1
	s.replace(start, count, str);
#else
	s.erase(start, count);
	s.insert(start, str);
#endif
}

void K::strReplace(std::string &s, const std::string &before, const std::string &after) {
	if (before.empty()) return;
	int pos = strFind(s.c_str(), before.c_str(), 0);
	while (pos >= 0) {
		strReplace(s, pos, before.size(), after);
		pos += after.size();
		pos = strFind(s.c_str(), before.c_str(), pos);
	}
}
void K::strReplaceChar(char *s, char before, char after) {
	K__Assert(s);
	for (size_t i=0; s[i]; i++) {
		if (s[i] == before) {
			s[i] = after;
		}
	}
}
void K::strReplaceChar(wchar_t *s, wchar_t before, wchar_t after) {
	K__Assert(s);
	for (size_t i=0; s[i]; i++) {
		if (s[i] == before) {
			s[i] = after;
		}
	}
}
void K::strReplaceChar(std::string &s, char before, char after) {
	for (auto it=s.begin(); it!=s.end(); ++it) {
		if (*it == before) {
			*it = after;
		}
	}
}
void K::strReplaceChar(std::wstring &ws, wchar_t before, wchar_t after) {
	for (auto it=ws.begin(); it!=ws.end(); ++it) {
		if (*it == before) {
			*it = after;
		}
	}
}
/// 文字列 s が sub で始まっているかどうか。
///
/// s と sub のどちらかまたは両方が nullptr か空文字列だった場合は false を返す
bool K::strStartsWith(const char *s, const char *sub) {
	K__Assert(s);
	K__Assert(sub);
	if (sub==nullptr || sub[0]=='\0') { // 空文字列はどんな文字列の先頭とも一致する
		return true;
	}
	size_t slen = strlen(s);
	size_t sublen = strlen(sub);
	if (sublen <= slen) {
		if (strncmp(s, sub, strlen(sub)) == 0) {
			return true;
		}
	}
	return false;
}
bool K::strStartsWith(const std::string &s, const std::string &sub) {
	return strStartsWith(s.c_str(), sub.c_str());
}

/// 文字列 s が sub で終わっているかどうか。
///
/// s と sub のどちらかまたは両方が nullptr か空文字列だった場合は false を返す
bool K::strEndsWith(const char *s, const char *sub) {
	K__Assert(s);
	K__Assert(sub);
	if (sub==nullptr || sub[0]=='\0') { // 空文字列はどんな文字列の先頭とも一致する
		return true;
	}
	size_t slen = strlen(s);
	size_t sublen = strlen(sub);
	if (sublen <= slen) {
		if (strncmp(s + slen - sublen, sub, sublen) == 0) {
			return true;
		}
	}
	return false;
}
bool K::strEndsWith(const std::string &s, const std::string &sub) {
	return strEndsWith(s.c_str(), sub.c_str());
}
void K::strTrim(std::string &s) {
	// trim left
	while (!s.empty() && isascii(s.front()) && isblank(s.front())) {
		s.erase(s.begin());
	}

	// trim right
	while (!s.empty() && isascii(s.back()) && isblank(s.back())) {
		s.pop_back();
	}
}


// delim 分割のために使う文字。複数指定できる。
//		カンマで区切る場合 ","
//		カンマと改行で区切る場合 ",\r\n"
//		空白で区切る場合 " "
//		空白とタブで区切る場合 " \t"
// condense_delims 分割文字が連続している場合、それをまとめて一つの分割文字とするか
//		例えばカンマ区切り文字列の場合 "aaa,,bbb" を "aaa" "" "bbb" の３要素に分割したいなら condense_delims を false にする。
//		改行区切り文字列で "aaa\n\nbbbb" を "aaa" "bbb" の２要素に分割したいなら連続する \n をまとめて扱いたいので condense_delims を true にする
// tirm 分割したときに前後の空白文字を取り除くか
//		"aaa   , bbb" を分割したとき "aaa   " と " bbb" のようにしたいなら _trim=false にする。"aaa" と "bbb" にしたいなら _trim=true にする
// maxcount 分割後の最大要素数。0だと上限なし。1だと分割しない＝元の文字列そのまま。2以上を指定した場合は、先頭から順番に分割していく。残った文字列は rest に入る
//      "aaa=bbb=ccc" を '=' で分割すると以下のようになる
//      maxcount=0 ==> returns: {"aaa", "bbb", "ccc"} rest: ""
//      maxcount=1 ==> returns: {"aaa"}               rest: "bbb=ccc"
//      maxcount=2 ==> returns: {"aaa", "bbb"}        rest: "ccc"
//      maxcount=3 ==> returns: {"aaa", "bbb", "ccc"} rest: ""
std::vector<std::string> K::strSplit(const std::string &str, const std::string &delims, bool condense_delims, bool _trim, int maxcount, std::string *p_rest) {
	std::vector<std::string> result;
	int s = 0; // 開始インデックス
	int i = 0;
	int len = str.size();
	while (i < len) {
		if (delims.find(str[i]) != std::string::npos) {
			// 分割文字が見つかった or 末尾に達した
			// [トークンを登録する]
			// デリミタ圧縮が有効 (condense_delims) ならばトークン長さをチェック (s < p) し、
			// 長さが 0 ならデリミタが連続しているのでトークン追加しない。
			// デリミタ圧縮無効ならば長さチェックしない（長さが 0 であっても有効なトークンとして追加する）
			if (!condense_delims || s<i) {
				std::string sv = str.substr(s, i-s);
				if (_trim) {
					strTrim(sv);
				}
				result.push_back(sv);
			} 
			// デリミタをスキップして次トークンの先頭に移動する
			// condense_delims がセットされているならば連続するデリミタを1つの塊として扱う
			size_t span = 1;
			if (condense_delims) {
				span = strspn(str.c_str()+i, delims.c_str());
			}
			i += span;
			s = i;
			// 最大トークン数に達したら終了する
			if (maxcount > 0 && result.size() == maxcount) {
				break;
			}
		} else {
			i++;
		}
	}

	// 末尾トークンがまだ残っているなら、それを登録
	if (s < len) {
		// s が終端文字に達していない。
		// 入力文字列がまだ残っている
		std::string rest = str.substr(s, len - s);
		if (maxcount <= 0 || (int)result.size() < maxcount) { // 最後のトークンを追加
			if (_trim) {
				strTrim(rest);
			}
			if (p_rest) *p_rest = ""; // 余りなし
			result.push_back(rest);
		
		} else {
			if (p_rest) *p_rest = rest; // 最後の文字列を余り文字列としてセット
		}
	}
	return result;
}

bool K::strToFloat(const std::string &s, float *val) {
	return strToFloat(s.c_str(), val);
}
bool K::strToFloat(const char *s, float *val) {
	if (s == nullptr) return false;
	char *err = 0;
	float result = strtof(s, &err);
	if (err == s || *err) return false;
	if (val) *val = result;
	return true;
}
bool K::strToInt(const std::string &s, int *val) {
	return strToInt(s.c_str(), val);
}
bool K::strToInt(const char *s, int *val) {
	if (s == nullptr) return false;
	char *err = 0;
	int result = strtol(s, &err, 0);
	if (err == s || *err) return false;
	if (val) *val = result;
	return true;
}
bool K::strToUInt32(const char *s, uint32_t *val) {
	if (s == nullptr) return false;
	char *err = 0;
	uint32_t result = strtoul(s, &err, 0);
	if (err == s || *err) return false;
	if (val) *val = result;
	return true;
}
bool K::strToUInt64(const char *s, uint64_t *val) {
	if (s == nullptr) return false;
	char *err = 0;
	uint64_t result = _strtoui64(s, &err, 0);
	if (err == s || *err) return false;
	if (val) *val = result;
	return true;
}

/// strptime の代替関数
/// @see https://linuxjm.osdn.jp/html/LDP_man-pages/man3/strptime.3.html
/// Visual Studio では strptime が定義されていないので、代わりにこれを使う。
/// @code
///		K_strptime_l("2020-12-17 10:12:01", "%Y-%m-%d %H:%M:%S", &tm, "");
/// @endcode
char * K::str_strptime(const char *str, const char *fmt, struct tm *out_tm, const char *_locale) {
	K__Assert(str);
	K__Assert(fmt);
	K__Assert(out_tm);
	K__Assert(_locale);
	#ifdef _WIN32
	{
		// strptime は Visual Studio では使えないので代替関数を用意する
		// https://code.i-harness.com/ja/q/4e939
		std::istringstream input(str);
		input.imbue(std::locale(_locale));
		input >> std::get_time(out_tm, fmt);
		if (input.fail()) return nullptr;
		return (char*)(str + (int)input.tellg()); // 戻り値は解析済み文字列の次の文字
	}
	#else
	{
		return strptime(str, fmt, out_tm);
	}
	#endif
}
int K::str_stricmp(const char *s, const char *t) {
	#ifdef _WIN32
	return _stricmp(s, t);
	#else
	return strcasecmp(s, t);
	#endif
}
std::string K::str_vsprintf(const char *fmt, va_list args) {
	// 長さ取得
	int len = 0;
	{
		// vsnprintfを args に対して複数回呼び出すときには注意が必要。
		// args はストリームのように動作するため、args の位置をリセットしなければならない
		// args に対しては va_start することができないので、毎回コピーを取り、それを使うようにする
		va_list ap;
		va_copy(ap, args);
		len = ::vsnprintf(nullptr, 0, fmt, ap);
		va_end(ap);
	}
	if (len > 0) {
		std::string s(len+1, 0); // vsprintf はヌル文字も書き込むため、len+1 確保しないといけない
		int n = ::vsnprintf(&s[0], s.size(), fmt, args);
		s.resize(n);
		return s;
	} else {
		return "";
	}
}
std::string K::str_sprintf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	std::string result = str_vsprintf(fmt, args);
	va_end(args);
	return result;
}

/// iswprint の代替関数
/// 印字可能文字（空白やタブ等も含む）を判別する
/// 標準関数の iswprint と同じだが、ロケールを設定していなくてもよい
///  (iswprint はロケールを設定しないと正しく動作しない場合がある）
bool K::str_iswprint(wchar_t wc) { // 印字文字（空白を含む）
	// 文字 YES
	// 空白 YES
	// タブ YES
	// 改行 NO
	WORD t = 0;
	::GetStringTypeW(CT_CTYPE1, &wc, 1, &t);
	return (t & C1_DEFINED) && !(t & C1_CNTRL);
}

/// iswgraph の代替関数
/// 印字可能文字（空白やタブ等を含まない）を判別する
/// 標準関数の iswgraph と同じだが、ロケールを設定していなくてもよい
///  (iswgraph はロケールを設定しないと正しく動作しない場合がある）
bool K::str_iswgraph(wchar_t wc) {
	// 文字 YES
	// 空白 NO
	// タブ NO
	// 改行 NO
	WORD t = 0;
	::GetStringTypeW(CT_CTYPE1, &wc, 1, &t);
	return (t & C1_DEFINED) && !(t & C1_CNTRL) && !(t & C1_SPACE);
}

/// iswblank の代替関数
/// ブランク文字（空白やタブ等。改行文字などは含まない）を判別する
/// 標準関数の iswgraph と同じだが、ロケールを設定していなくてもよい
///  (iswgraph はロケールを設定しないと正しく動作しない場合がある）
bool K::str_iswblank(wchar_t wc) {
	// 文字 NO
	// 空白 YES
	// タブ YES
	// 改行 NO
	WORD t = 0;
	::GetStringTypeW(CT_CTYPE1, &wc, 1, &t);
	return (t & C1_DEFINED) && (t & C1_BLANK);
}

/// 半角英数かどうか
bool K::str_iswhalf(wchar_t wc) {
	WORD t = 0;
	::GetStringTypeW(CT_CTYPE3, &wc, 1, &t);
	return (t & C3_HALFWIDTH) != 0;
}

// utf8 から wide に変換する
// out_ws に書き込んだバイト数を返す（終端文字を含むので必ず1以上の値になる）。エラーが発生した場合は 0 を返す
// out_ws または max_out_widechars が 0 の場合は変換後の文字数（終端文字を含む）を返す
// ※BOMは取り除かれる
int K::strUtf8ToWide(wchar_t *out_ws, int max_out_widechars, const char *u8, int u8bytes) {
	if (u8bytes <= 0) u8bytes = strlen(u8);
	const char *u8str = u8;
	if (strStartsWithBom(u8, u8bytes)) { // BOMをスキップする
		u8str = strSkipBom(u8);
		u8bytes -= K__UTF8BOM_LEN;
	}
	return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8str, u8bytes, out_ws, max_out_widechars);
}

// utf8 から wide に変換する
// ※BOMは取り除かれる
std::wstring K::strUtf8ToWide(const std::string &u8) {
	const char *s = K::strSkipBom(u8.c_str()); // BOMをスキップする
	if (s[0] == '\0') {
		return L"";
	}
	std::wstring ws;
	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
	if (len == 0) {
		// 副作用防止のため K__Error などではなく K__RawPrintf を使う
		K__RawPrintf("Failed to convert UTF8 string into WideChar (%d bytes string)", u8.size());
		return L"";
	}
	ws.resize(len + 1 + 32); // MultiByteToWideChar は末尾のヌル文字も書き込むので、その領域も確保することに注意（念のため少し多めに確保する）
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, &ws[0], len);
	ws.resize(wcslen(ws.c_str())); // ws.size が正しい文字列長さを返すように調整する
	return ws;
}

// wide から utf8 に変換する
// out_u8 に書き込んだバイト数を返す（終端文字を含むので必ず1以上の値になる）。エラーが発生した場合は 0 を返す
// out_u8 または max_out_bytes を 0 にした場合は変換後の文字列を格納するために必要なバイト数を返す（末尾文字を含む）
int K::strWideToUtf8(char *out_u8, int max_out_bytes, const wchar_t *ws) {
	// WideCharToMultiByte の戻り値を信用しない
	// http://blog.livedoor.jp/blackwingcat/archives/976097.html
	// WideCharToMultiByte は終端文字を「含む」全体のバイト数を返す
	// エラーの場合は 0 になる
	int len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, -1, out_u8, max_out_bytes, nullptr, nullptr);
	if (len == 0) return 0; // FAIL
	if (out_u8 && max_out_bytes > 0) return strlen(out_u8) + 1; // 終端文字を含むバイト数
	return len + 32; // 指示されたサイズよりも少し大きめの値を返すようにする
}

// wide から utf8 に変換する
std::string K::strWideToUtf8(const std::wstring &ws) {
	std::string u8;
	int len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
	u8.resize(len + 1 + 32); // WideCharToMultiByte は末尾のヌル文字も書き込むので、その領域も確保することに注意（念のため少し多めに確保する）
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws.c_str(), -1, &u8[0], len, nullptr, nullptr);
	u8.resize(strlen(u8.c_str())); // u8.size が正しい文字列長さを返すように調整する
	return u8;
}

/// ワイド文字列をANSI文字列に変換する
/// @return 変換後のワイド文字数（終端文字を含む）
///
/// ※ANSI文字列とは、現在のロケールに基づくマルチバイト文字列を表す。日本語環境なら SJIS) 
///
/// @param ws 入力ワイド文字列
/// @param out_ansi  得られたANSI文字列
/// @param max_out_bytes  取得する文字列の最大バイト数（終端文字含む）
/// @param _locale   ロケール識別子。空文字列 "" を指定した場合は現在のシステムロケールを使う
/// @return 終端文字を含むバイト数を返す（つまり必ず1以上の値になる）。エラーが発生した場合は 0
///
/// ロケール識別子の正式な形は "language[_region[.codepage]]" となっていて、
/// 日本語向けなら "jpn" や "japanese", "japanese_japan.932" などと指定する（大小文字は区別しない）。
/// ロケール識別子については標準関数の setlocale の解説を参照すること
/// @see https://docs.microsoft.com/ja-jp/cpp/c-runtime-library/locale-names-languages-and-country-region-strings
///
int K::strWideToAnsi(char *out_ansi, int max_out_bytes, const wchar_t *ws, const char *_locale) {
	K__Assert(ws);
	K__Assert(_locale);
	int num_bytes = 0;
	_locale_t loc = _create_locale(LC_CTYPE, _locale);
	if (loc) {
		num_bytes = strWideToAnsiL(out_ansi, max_out_bytes, ws, loc);
		_free_locale(loc);
	} else {
		K__Error("INVALID_LOCALE '%s' at K__WideToAnsi", _locale);
	}
	return num_bytes; // 0=ERROR
}
int K::strWideToAnsiL(char *out_ansi, int max_out_bytes, const wchar_t *ws, _locale_t loc) {
	// ワイド文字列からマルチバイト文字列へ変換する。
	// 変換後の文字列を格納するために必要なバイト数（終端文字を含む）を返す
	// この関数は末尾のヌル文字も出力してそれを含めた長さを返すため、文字列長さ＋１のバッファを確保する必要がある
	// エラーなどで何も出力できない場合は 0 を返す
	// 空文字列を生成した場合は 1 を返す
	K__Assert(ws);
	K__Assert(loc);
	if (K_USE_WCSTOMBS_SAFE) {
		// ヌル文字含む長さ
		// _wcstombs_s_l は必ず末尾にヌル文字をおき、それを含めたサイズを返す
		size_t len_bytes = 0; // ヌル文字含む長さ
		_wcstombs_s_l(&len_bytes, out_ansi, max_out_bytes, ws, _TRUNCATE, loc); // 必要なサイズを問い合わせるなら out_ansi = nullptr にする
		return (int)len_bytes; // [0=ERR] [1=EMPTY STRING] [>=2 OK]

	} else {
		if (out_ansi && max_out_bytes>0) {
			// 安全版の動作に合わせて、末尾には必ずヌル文字列を置く
			int new_strlen = (int)_wcstombs_l(out_ansi, ws, max_out_bytes, loc);
			K__Assert(new_strlen <= max_out_bytes);
			if (new_strlen == max_out_bytes) {
				out_ansi[new_strlen-1] = '\0';
				return max_out_bytes;
			} else {
				return new_strlen + 1; // with null terminator
			}

		} else {
			int req_strlen = (int)_wcstombs_l(nullptr, ws, 0, loc);
			return req_strlen + 1; // with null terminator
		}
	}
}

std::string K::strWideToAnsi(const std::wstring &ws, const char *_locale) {
	std::string mb;
	int req_bytes = strWideToAnsi(nullptr, 0, ws.c_str(), _locale);
	// 変換後のバイト数（終端文字含む）
	// req_bytes = 0: エラー
	// req_bytes = 1: 空文字列なので終端文字のみ
	// req_bytes > 1: 変換済み
	if (req_bytes > 0) {
		mb.resize(req_bytes); // strWideToAnsi は終端文字も書き込むことに注意。req_bytesは終端文字を含んだサイズである
		strWideToAnsi(&mb[0], mb.size(), ws.c_str(), _locale);
		mb.resize(strlen(mb.c_str())); // 終端文字を含まないサイズにする
	}
	return mb;
}

std::wstring K::strAnsiToWide(const std::string &ansi, const char *_locale) {
	std::wstring ws;
	int req_wchars = strAnsiToWide(nullptr, 0, ansi.c_str(), _locale);
	// 変換後のバイト数（終端文字含む）
	// req_wchars = 0: エラー
	// req_wchars = 1: 空文字列なので終端文字のみ
	// req_wchars > 1: 変換済み
	if (req_wchars > 0) {
		ws.resize(req_wchars); // strAnsiToWide は終端文字も書き込む。req_wchars は終端文字を含んだ文字数であることに注意
		strAnsiToWide(&ws[0], ws.size(), ansi.c_str(), _locale);
		ws.resize(wcslen(ws.c_str())); // 終端文字を含まないサイズにする
	}
	return ws;
}
std::string K::strAnsiToUtf8(const std::string &ansi, const char *_locale) {
	std::wstring ws = strAnsiToWide(ansi, _locale);
	return strWideToUtf8(ws);
}
std::string K::strUtf8ToAnsi(const std::string &u8, const char *_locale) {
	std::wstring ws = strUtf8ToWide(u8);
	return strWideToAnsi(ws, _locale);
}

int K::strAnsiToWideL(wchar_t *out_wide, int max_out_wchars, const char *ansi, _locale_t loc) {
	// マルチバイト文字列からワイド文字列へ変換する。変換後の文字数（終端文字を含まない）を返す
	// ※変換できない場合でもエラーメッセージやログを出さない。
	// 　文字を変換するためではなく、「変換できるかどうかの確認」のために呼ばれる場合があるため。
	K__Assert(ansi);
	K__Assert(loc);
	if (K_USE_MBSTOWCS_SAFE) {
		// ヌル文字含む長さ
		// _mbstowcs_s_l は必ず末尾にヌル文字をおき、それを含めたサイズを返す
		size_t len_wchars = 0;
		_mbstowcs_s_l(&len_wchars, out_wide, max_out_wchars, ansi, _TRUNCATE, loc); // 必要なサイズを問い合わせるなら out_wide = nullptr にする
		return (int)len_wchars; // [0=ERR] [1=EMPTY STRING] [>=2 OK]
	
	} else {
		if (out_wide && max_out_wchars>0) {
			// 安全版の動作に合わせて、末尾には必ずヌル文字列を置く
			int new_wcslen = (int)_mbstowcs_l(out_wide, ansi, max_out_wchars, loc);
			K__Assert(new_wcslen <= max_out_wchars);
			if (new_wcslen == max_out_wchars) {
				out_wide[new_wcslen-1] = '\0';
				return max_out_wchars;
			} else {
				return new_wcslen + 1; // with null terminator
			}
		} else {
			int req_wcslen = (int)_mbstowcs_l(nullptr, ansi, 0, loc);
			return req_wcslen + 1; // with null terminator
		}
	}
}


/// ANSI文字列からワイド文字への変換
/// ※ANSI文字列とは、現在のロケールに基づくマルチバイト文字列を表す。日本語環境なら SJIS) 
/// ロケール引数については K_StrWideToAnsi を参照
/// 終端文字を含むバイト数を返す（つまり必ず1以上の値になる）。エラーが発生した場合は 0
int K::strAnsiToWide(wchar_t *out_wide, int max_out_wchars, const char *ansi, const char *_locale) {
	K__Assert(ansi);
	K__Assert(_locale);
	int num_wchars = 0;
	_locale_t loc = _create_locale(LC_CTYPE, _locale);
	if (loc) {
		num_wchars = strAnsiToWideL(out_wide, max_out_wchars, ansi, loc);
		_free_locale(loc);
	} else {
		K__Error("INVALID_LOCALE '%s' at K__AnsiToWide", _locale);
	}
	return num_wchars; // 0=ERROR
}

std::wstring K::strBinToWide(const void *data, int size) {
	// エンコード不明の文字列からワイド文字列を得る
	// あくまでも日本語前提なので、ここでは SJIS または UTF8 が使われていると仮定する
	if (data == nullptr || size == 0) {
		return std::wstring();
	}

	// BOM で始まるデータなら UTF8 で確定させる
	if (K::strStartsWithBom(data, size)) {
		return K::strUtf8ToWide((const char *)data);
	}

	std::wstring ws;

	// 現在のロケールにおけるマルチバイト文字列として変換
	ws = K::strAnsiToWide((const char *)data, "");
	if (ws.size() > 0) {
		return ws;
	}

	// SJIS であると仮定して変換
	// （非日本語環境でゲームを実行しているとき、SJIS保存されたファイルをロードしようとしている場合など）
	// SJISをUTF8として解釈できる事が多いが、UTF8をSJISとして解釈できることは少ないため、
	// 先にSJISへの変換を試みる。入力がUTF8の場合は大体失敗してくれるはず。
	ws = K::strAnsiToWide((const char *)data, "JPN");
	if (ws.size() > 0) {
		return ws;
	}

	// BOM なしの UTF8 であると仮定して変換
	ws = K::strUtf8ToWide((const char *)data);
	if (ws.size() > 0) {
		return ws;
	}

	// どの方法でも変換できなかった
	return std::wstring();
}
#pragma endregion // string








#define K__LCID_ENGLISH 0x0409 // 米国英語

bool K__Win32GetErrorString(long hr, char *buf, int size) {
	return FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, K__LCID_ENGLISH, buf, size, nullptr) > 0;
}
std::string K__Win32GetErrorStringStd(long hr) {
	char buf[1024] = {0};
	K__Win32GetErrorString(hr, buf, sizeof(buf));
	return buf;
}



void K__Break() {
	if (IsDebuggerPresent()) {
		__debugbreak();
	}
}

void K__Exit() {
	if (1) {
		// 黙って強制終了する。
		// exit() とは違い、この方法で終了すると例外発生ウィンドウが出ない
		TerminateProcess(OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId()), 0);
	}
	exit(-1);
}

bool K__IsDebuggerPresent() {
	return IsDebuggerPresent();
}

void K__Dialog(const char *u8) {
	wchar_t wpath[MAX_PATH] = {0};
	GetModuleFileNameW(nullptr, wpath, MAX_PATH);

	std::wstring ws = K::strUtf8ToWide(u8);

	int btn = MessageBoxW(nullptr, ws.c_str(), PathFindFileNameW(wpath), MB_ICONSTOP|MB_ABORTRETRYIGNORE);
	if (btn == IDABORT) {
		K__Exit();
	}
	if (btn == IDRETRY) {
		K__Break();
	}
}
void K__Notify(const char *u8) {
	wchar_t wpath[MAX_PATH] = {0};
	GetModuleFileNameW(nullptr, wpath, MAX_PATH);

	std::wstring ws= K::strUtf8ToWide(u8);

	int btn = MessageBoxW(nullptr, ws.c_str(), PathFindFileNameW(wpath), MB_ICONINFORMATION|MB_OK);
	if (btn == IDABORT) {
		K__Exit();
	}
	if (btn == IDRETRY) {
		K__Break();
	}
}





namespace Test {
void Test_internal_path() {
	
	char p[256] = {"bbb"};
	PathAppendA(p, "c:/aaa");

	strcpy(p, "filename.exe");
	PathRemoveFileSpecA(p);

	strcpy(p, "filename.exe");
	const char *ss = PathFindFileName(p);

	{
		char s[256] = {0};
		K__Verify(K::pathJoin("", "aaa")        == "aaa");
		K__Verify(K::pathJoin("aaa", "")        == "aaa");
		K__Verify(K::pathJoin("", "aaa/")       == "aaa/");
		K__Verify(K::pathJoin("aaa", "")        == "aaa");
		K__Verify(K::pathJoin("aaa", "bbb")     == "aaa/bbb");
		K__Verify(K::pathJoin("aaa/bbb", "ccc") == "aaa/bbb/ccc");
		K__Verify(K::pathJoin("aaa/", "bbb")   == "aaa/bbb"); // aaa//bbb にはならない
		K__Verify(K::pathJoin("aaa/", "/bbb")   == "/bbb"); // 後続が絶対パスだった場合は、そのパスで置き換える
		K__Verify(K::pathGetParent("aaa.exe")     == "");
		K__Verify(K::pathGetParent("aaa/bbb.exe") == "aaa");
		K__Verify(K::pathGetParent("aaa/bbb/ccc") == "aaa/bbb");
		K__Verify(K::pathRenameExtension("aaa/bbb", ".exe") == "aaa/bbb.exe");
		K__Verify(K::pathRenameExtension("aaa/bbb.exe", "") == "aaa/bbb");
		K__Verify(K::pathRenameExtension("aaa/bbb", "")     == "aaa/bbb");
		K__Verify(K::pathGetExt("bbb/aaa.exe")         == ".exe");
		K__Verify(K::pathGetExt("bbb/aaa.exe.zip")     == ".zip");
		K__Verify(K::pathGetExt("bbb/aaa.zip/ccc.bmp") == ".bmp");
		K__Verify(K::pathGetExt("bbb/aaa.zip/ccc")     == "");
		K__Verify(K::pathGetExt("bbb/aaa")             == "");
		K__Verify(K::pathGetLast("bbb/aaa.exe")        == "aaa.exe");
		K__Verify(K::pathGetLast("aaa.exe")            == "aaa.exe");
	}
	{
		K__Verify(K::pathGetCommonSize("aaa/bbb/ccc", "aaa/bbb/ccc") == 11);
		K__Verify(K::pathGetCommonSize("aaa/bbb/ccc", "aaa/bbb/ddd") == 8);
		K__Verify(K::pathGetCommonSize("aaa/bbb/ccc", "aaa/bbb_ccc") == 4);
		K__Verify(K::pathGetCommonSize("aaa", "") == 0);
		K__Verify(K::pathGetCommonSize("", "") == 0);
	}
	{
		K__Verify(K::pathGetRelative("aa/bb/cc", "aa")       == "bb/cc");
		K__Verify(K::pathGetRelative("aa/bb/cc", "aa/bb")    == "cc");
		K__Verify(K::pathGetRelative("aa/bb/cc", "aa/bb/ee") == "../cc");
		K__Verify(K::pathGetRelative("aa/bb/cc", "ee/ff")    == "../../aa/bb/cc");
		K__Verify(K::pathGetRelative("", "aa")               == "..");
		K__Verify(K::pathGetRelative("aa", "")               == "aa");
		K__Verify(K::pathGetRelative("", "")                 == "");
	}
	{
		K__Verify(K::pathGlob("abc", "*") == true);
		K__Verify(K::pathGlob("abc", "a*") == true);
		K__Verify(K::pathGlob("abc", "ab*") == true);
		K__Verify(K::pathGlob("abc", "abc*") == true);
		K__Verify(K::pathGlob("abc", "a*c") == true);
		K__Verify(K::pathGlob("abc", "*abc") == true);
		K__Verify(K::pathGlob("abc", "*bc") == true);
		K__Verify(K::pathGlob("abc", "*c") == true);
		K__Verify(K::pathGlob("abc", "*a*b*c*") == true);
		K__Verify(K::pathGlob("abc", "*bc*") == true);
		K__Verify(K::pathGlob("abc", "*c*") == true);
		K__Verify(K::pathGlob("aaa/bbb.ext", "a*.ext") == false); // ワイルドカード * はパス区切り文字を含まない
		K__Verify(K::pathGlob("aaa/bbb.ext", "a*/*.ext") == true);
		K__Verify(K::pathGlob("aaa/bbb.ext", "a*/*.*t") == true);
		K__Verify(K::pathGlob("aaa/bbb.ext", "aaa/*.ext") == true);
		K__Verify(K::pathGlob("aaa/bbb.ext", "aaa/bbb*ext") == true);
		K__Verify(K::pathGlob("aaa/bbb.ext", "aaa*bbb*ext") == false);
		K__Verify(K::pathGlob("aaa/bbb/ccc.ext", "aaa/*/ccc.ext") == true);
		K__Verify(K::pathGlob("aaa/bbb/ccc.ext", "aaa/*.ext") == false);
		K__Verify(K::pathGlob("aaa/bbb.ext", "*.aaa") == false);
		K__Verify(K::pathGlob("aaa/bbb.ext", "aaa*bbb") == false);
		K__Verify(K::pathGlob("aaa/bbb.ext", "*/*.ext") == true);
		K__Verify(K::pathGlob("aaa/bbb.ext", "*/*.*") == true);
	}
}
} // Test




} // namespace
