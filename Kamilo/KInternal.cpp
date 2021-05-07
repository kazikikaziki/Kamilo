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
	std::string mb = K__WideToAnsiStd(ws, "");
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
		std::wstring ws = K__Utf8ToWideStd(u8);
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
		std::wstring ws = K__Utf8ToWideStd(u8);
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
		std::string u8 = K__WideToUtf8Std(ws);
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
		std::wstring ws = K__Utf8ToWideStd(u8);
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
		std::string u8 = K__WideToUtf8Std(ws);
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
		std::wstring ws = K__Utf8ToWideStd(u8);
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
		std::string u8 = K__WideToUtf8Std(ws);
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
		std::wstring ws = K__Utf8ToWideStd(u8);
		K__WPrintLn(ws.c_str());
	}
#endif
}



bool K__strtof(const std::string &s, float *val) {
	return K__strtof(s.c_str(), val);
}
bool K__strtoi(const std::string &s, int *val) {
	return K__strtoi(s.c_str(), val);
}

bool K__strtof(const char *s, float *val) {
	if (s == nullptr) return false;
	char *err = 0;
	float result = strtof(s, &err);
	if (err == s || *err) return false;
	if (val) *val = result;
	return true;
}
bool K__strtoi(const char *s, int *val) {
	if (s == nullptr) return false;
	char *err = 0;
	int result = strtol(s, &err, 0);
	if (err == s || *err) return false;
	if (val) *val = result;
	return true;
}
bool K__strtoui32(const char *s, uint32_t *val) {
	if (s == nullptr) return false;
	char *err = 0;
	uint32_t result = strtoul(s, &err, 0);
	if (err == s || *err) return false;
	if (val) *val = result;
	return true;
}
bool K__strtoui64(const char *s, uint64_t *val) {
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
char * K__strptime_l(const char *str, const char *fmt, struct tm *out_tm, const char *_locale) {
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

int K__stricmp(const char *s, const char *t) {
#ifdef _WIN32
	return _stricmp(s, t);
#else
	return strcasecmp(s, t);
#endif
}

/// 半角英数かどうか
bool K__iswhalf(wchar_t wc) {
	WORD t = 0;
	GetStringTypeW(CT_CTYPE3, &wc, 1, &t);
	return (t & C3_HALFWIDTH) != 0;
}

/// iswprint の代替関数
/// 印字可能文字（空白やタブ等も含む）を判別する
/// 標準関数の iswprint と同じだが、ロケールを設定していなくてもよい
///  (iswprint はロケールを設定しないと正しく動作しない場合がある）
bool K__iswprint(wchar_t wc) { // 印字文字（空白を含む）
	// 文字 YES
	// 空白 YES
	// タブ YES
	// 改行 NO
	WORD t = 0;
	GetStringTypeW(CT_CTYPE1, &wc, 1, &t);
	return (t & C1_DEFINED) && !(t & C1_CNTRL);
}

/// iswgraph の代替関数
/// 印字可能文字（空白やタブ等を含まない）を判別する
/// 標準関数の iswgraph と同じだが、ロケールを設定していなくてもよい
///  (iswgraph はロケールを設定しないと正しく動作しない場合がある）
bool K__iswgraph(wchar_t wc) {
	// 文字 YES
	// 空白 NO
	// タブ NO
	// 改行 NO
	WORD t = 0;
	GetStringTypeW(CT_CTYPE1, &wc, 1, &t);
	return (t & C1_DEFINED) && !(t & C1_CNTRL) && !(t & C1_SPACE);
}

/// iswblank の代替関数
/// ブランク文字（空白やタブ等。改行文字などは含まない）を判別する
/// 標準関数の iswgraph と同じだが、ロケールを設定していなくてもよい
///  (iswgraph はロケールを設定しないと正しく動作しない場合がある）
bool K__iswblank(wchar_t wc) {
	// 文字 NO
	// 空白 YES
	// タブ YES
	// 改行 NO
	WORD t = 0;
	GetStringTypeW(CT_CTYPE1, &wc, 1, &t);
	return (t & C1_DEFINED) && (t & C1_BLANK);
}

std::string K__vsprintf_std(const char *fmt, va_list args) {
	// 長さ取得
	int len = 0;
	{
		// vsnprintfを args に対して複数回呼び出すときには注意が必要。
		// args はストリームのように動作するため、args の位置をリセットしなければならない
		// args に対しては va_start することができないので、毎回コピーを取り、それを使うようにする
		va_list ap;
		va_copy(ap, args);
		len = vsnprintf(nullptr, 0, fmt, ap);
		va_end(ap);
	}
	if (len > 0) {
		std::string s(len+1, 0); // vsprintf はヌル文字も書き込むため、len+1 確保しないといけない
		int n = vsnprintf(&s[0], s.size(), fmt, args);
		s.resize(n);
		return s;
	} else {
		return "";
	}
}
std::string K__sprintf_std(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	std::string result = K__vsprintf_std(fmt, args);
	va_end(args);
	return result;
}



void K__ReplaceA(char *s, char before, char after) {
	K__Assert(s);
	for (size_t i=0; s[i]; i++) {
		if (s[i] == before) {
			s[i] = after;
		}
	}
}
void K__ReplaceW(wchar_t *s, wchar_t before, wchar_t after) {
	K__Assert(s);
	for (size_t i=0; s[i]; i++) {
		if (s[i] == before) {
			s[i] = after;
		}
	}
}
int K__StrFind(const char *s, const char *sub, int start) {
	K__Assert(s);
	K__Assert(sub);
	if (start < 0) start = 0;
	if ((int)strlen(s) < start) return -1; // 範囲外
	if ((int)strlen(s) == start) return strlen(sub)==0 ? start : -1; // 検索開始位置が文字列末尾の場合、空文字列だけがマッチする
	if (strlen(sub)==0) return start; // 空文字列はどの場所であっても必ずマッチする。常に検索開始位置にマッチする
	const char *p = strstr(s + start, sub);
	return p ? (p - s) : -1;
}
void K__Replace(std::string &s, int start, int count, const char *str) {
	K__Assert(str);
	s.erase(start, count);
	s.insert(start, str);
}
void K__Replace(std::string &s, const char *before, const char *after) {
	K__Assert(before);
	K__Assert(after);
	if (strlen(before) == 0) return;
	int pos = K__StrFind(s.c_str(), before, 0);
	while (pos >= 0) {
		K__Replace(s, pos, strlen(before), after);
		pos += strlen(after);
		pos = K__StrFind(s.c_str(), before, pos);
	}
}




/// u8の先頭が utf8 bom で始まっているなら、その次の文字アドレスを返す。
/// utf8 bom で始まっていない場合はそのまま u8 を返す
const char * K__SkipUtf8Bom(const char *s) {
	K__Assert(s);
	if (strncmp(s, K__UTF8BOM_STR, K__UTF8BOM_LEN) == 0) {
		return s + K__UTF8BOM_LEN;
	} else {
		return s;
	}
}

bool K__StartsWithUtf8Bom(const void *data, int size) {
	return (size >= K__UTF8BOM_LEN) && (memcmp(data, K__UTF8BOM_STR, K__UTF8BOM_LEN) == 0);
}


// utf8 から wide に変換する
// out_ws に書き込んだバイト数を返す（終端文字を含むので必ず1以上の値になる）。エラーが発生した場合は 0 を返す
// out_ws または max_out_widechars が 0 の場合は変換後の文字数（終端文字を含む）を返す
// ※BOMは取り除かれる
int K__Utf8ToWide(wchar_t *out_ws, int max_out_widechars, const char *u8, int u8bytes) {
	if (u8bytes <= 0) u8bytes = strlen(u8);
	const char *u8str = u8;
	if (K__StartsWithUtf8Bom(u8, u8bytes)) { // BOMをスキップする
		u8str = K__SkipUtf8Bom(u8);
		u8bytes -= K__UTF8BOM_LEN;
	}
	return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8str, u8bytes, out_ws, max_out_widechars);
}

// utf8 から wide に変換する
// ※BOMは取り除かれる
std::wstring K__Utf8ToWideStd(const std::string &u8) {
	const char *s = K__SkipUtf8Bom(u8.c_str()); // BOMをスキップする
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
int K__WideToUtf8(char *out_u8, int max_out_bytes, const wchar_t *ws) {
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
std::string K__WideToUtf8Std(const std::wstring &ws) {
	std::string u8;
	int len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
	u8.resize(len + 1 + 32); // WideCharToMultiByte は末尾のヌル文字も書き込むので、その領域も確保することに注意（念のため少し多めに確保する）
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws.c_str(), -1, &u8[0], len, nullptr, nullptr);
	u8.resize(strlen(u8.c_str())); // u8.size が正しい文字列長さを返すように調整する
	return u8;
}


int K__WideToAnsiL(char *out_ansi, int max_out_bytes, const wchar_t *ws, _locale_t loc) {
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
// 終端文字を含むバイト数を返す（つまり必ず1以上の値になる）。エラーが発生した場合は 0
int K__WideToAnsi(char *out_ansi, int max_out_bytes, const wchar_t *ws, const char *_locale) {
	K__Assert(ws);
	K__Assert(_locale);
	int num_bytes = 0;
	_locale_t loc = _create_locale(LC_CTYPE, _locale);
	if (loc) {
		num_bytes = K__WideToAnsiL(out_ansi, max_out_bytes, ws, loc);
		_free_locale(loc);
	} else {
		K__Error("INVALID_LOCALE '%s' at K__WideToAnsi", _locale);
	}
	return num_bytes; // 0=ERROR
}
std::string K__WideToAnsiStd(const std::wstring &ws, const char *_locale) {
	std::string mb;
	int req_bytes = K__WideToAnsi(nullptr, 0, ws.c_str(), _locale);
	// 変換後のバイト数（終端文字含む）
	// req_bytes = 0: エラー
	// req_bytes = 1: 空文字列なので終端文字のみ
	// req_bytes > 1: 変換済み
	if (req_bytes > 0) {
		mb.resize(req_bytes); // K__WideToAnsi は終端文字も書き込むことに注意。req_bytesは終端文字を含んだサイズである
		K__WideToAnsi(&mb[0], mb.size(), ws.c_str(), _locale);
		mb.resize(strlen(mb.c_str())); // 終端文字を含まないサイズにする
	}
	return mb;
}



int K__AnsiToWideL(wchar_t *out_wide, int max_out_wchars, const char *ansi, _locale_t loc) {
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

// 終端文字を含むバイト数を返す（つまり必ず1以上の値になる）。エラーが発生した場合は 0
int K__AnsiToWide(wchar_t *out_wide, int max_out_wchars, const char *ansi, const char *_locale) {
	K__Assert(ansi);
	K__Assert(_locale);
	int num_wchars = 0;
	_locale_t loc = _create_locale(LC_CTYPE, _locale);
	if (loc) {
		num_wchars = K__AnsiToWideL(out_wide, max_out_wchars, ansi, loc);
		_free_locale(loc);
	} else {
		K__Error("INVALID_LOCALE '%s' at K__AnsiToWide", _locale);
	}
	return num_wchars; // 0=ERROR
}

std::wstring K__AnsiToWideStd(const std::string &ansi, const char *_locale) {
	std::wstring ws;
	int req_wchars = K__AnsiToWide(nullptr, 0, ansi.c_str(), _locale);
	// 変換後のバイト数（終端文字含む）
	// req_wchars = 0: エラー
	// req_wchars = 1: 空文字列なので終端文字のみ
	// req_wchars > 1: 変換済み
	if (req_wchars > 0) {
		ws.resize(req_wchars); // K__Utf8ToWide は終端文字も書き込む。req_wchars は終端文字を含んだ文字数であることに注意
		K__AnsiToWide(&ws[0], ws.size(), ansi.c_str(), _locale);
		ws.resize(wcslen(ws.c_str())); // 終端文字を含まないサイズにする
	}
	return ws;
}





void K__Utf8ToWidePath(wchar_t *out_wpath, int num_wchars, const char *path_u8) {
	// windows形式のパスに変換する
	K__Utf8ToWide(out_wpath, num_wchars, path_u8, 0);
	K__ReplaceW(out_wpath, K__PATH_SLASHW, K__PATH_BACKSLASHW);
}
void K__WideToUtf8Path(char *out_path_u8, int num_bytes, const wchar_t *wpath) {
	K__WideToUtf8(out_path_u8, num_bytes, wpath);
	K__ReplaceA(out_path_u8, K__PATH_BACKSLASH, K__PATH_SLASH);
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
	std::wstring wpath = K__Utf8ToWideStd(path_u8);
	int h = (int)::ShellExecuteW(nullptr, L"OPEN", wpath.c_str(), L"", L"", SW_SHOW);
	return h > 32; // ShellExecute は成功すると 32 より大きい値を返す
}
FILE * K::fileOpen(const std::string &path_u8, const std::string &mode_u8) {
	// fopen の UTF8 版
	std::wstring wpath = K__Utf8ToWideStd(path_u8);
	std::wstring wmode = K__Utf8ToWideStd(mode_u8);
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
std::string K::fileLoadString(const std::string &filename_u8) {
	std::string bin;
	FILE *file = fileOpen(filename_u8, "r");
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
void K::fileSaveString(const std::string &filename_u8, const std::string &bin) {
	FILE *file = fileOpen(filename_u8, "w");
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
	K__ReplaceW(wpath, K__PATH_BACKSLASHW, K__PATH_SLASHW);
	return K__WideToUtf8Std(wpath);
}

/// chdir, SetCurrentDirectory の UTF8 版
///
/// カレントディレクトリ名を utf8 で得る
/// @see https://linuxjm.osdn.jp/html/LDP_man-pages/man2/chdir.2.html
bool K::sysSetCurrentDir(const std::string &dir) {
	wchar_t wpath[MAX_PATH] = {0};
	K__Utf8ToWidePath(wpath, MAX_PATH, dir.c_str());
	if (::SetCurrentDirectoryW(wpath)) {
		return true; // OK
	} else {
		K__ERROR_MSG(dir.c_str());
		return false; // FAIL
	}
}

std::string K::sysGetCurrentExecName() {
	wchar_t wpath[MAX_PATH] = {0};
	::GetModuleFileNameW(nullptr, wpath, MAX_PATH);
	K__ReplaceW(wpath, L'\\', L'/');
	return K__WideToUtf8Std(wpath);
}
std::string K::sysGetCurrentExecDir() {
	wchar_t wpath[MAX_PATH] = {0};
	::GetModuleFileNameW(nullptr, wpath, MAX_PATH);
	::PathRemoveFileSpecW(wpath);
	K__ReplaceW(wpath, L'\\', L'/');
	return K__WideToUtf8Std(wpath);
}
#pragma endregion // sys




#pragma region path
std::string K::pathJoin(const std::string &s1, const std::string &s2) {
	if (s1.empty()) return s2;
	if (s2.empty()) return s1;
	return s1 + "/" + s2;
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
		return ignore_case ? K__stricmp(s1, s2) : strcmp(s1, s2);
	
	} else {
		const char *s1 = path1.c_str();
		const char *s2 = path2.c_str();
		return ignore_case ? K__stricmp(s1, s2) : strcmp(s1, s2);
	}
}
std::string K::pathGetFull(const std::string &s) {
	wchar_t wpath[MAX_PATH] = {0};
	wchar_t wfull[MAX_PATH] = {0};
	K__Utf8ToWidePath(wpath, MAX_PATH, s.c_str());
	if (_wfullpath(wfull, wpath, MAX_PATH)) {
		K__ReplaceW(wfull, K__PATH_BACKSLASH, K__PATH_SLASH);
		return K__WideToUtf8Std(wfull);
	} else {
		return s;
	}
}
#pragma endregion // path






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

	const int MAX_DIALOG_STR = 1024 * 4;

	wchar_t ws[MAX_DIALOG_STR] = {0};
	K__Utf8ToWide(ws, MAX_DIALOG_STR, u8, 0);

	int btn = MessageBoxW(nullptr, ws, PathFindFileNameW(wpath), MB_ICONSTOP|MB_ABORTRETRYIGNORE);
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

	const int MAX_DIALOG_STR = 1024 * 4;

	wchar_t ws[MAX_DIALOG_STR] = {0};
	K__Utf8ToWide(ws, MAX_DIALOG_STR, u8, 0);

	int btn = MessageBoxW(nullptr, ws, PathFindFileNameW(wpath), MB_ICONINFORMATION|MB_OK);
	if (btn == IDABORT) {
		K__Exit();
	}
	if (btn == IDRETRY) {
		K__Break();
	}
}









} // namespace
