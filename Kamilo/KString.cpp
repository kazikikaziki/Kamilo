#include "KString.h"

#include <locale.h>
#include <windows.h> // FILETIME
#include <Shlwapi.h> // PathIsDirectoryW, PathFileExistsW
#include "KInternal.h"

#define K__PATH_SLASH  '/'

namespace Kamilo {


static const char *EMPTY_STR = ""; // mStr を nullptr にしないために使う

#pragma region KStringView
bool KStringView::isPathDelim(char c) {
	return c=='/' || c=='\\';
}
KStringView::KStringView() {
	set_view(nullptr, 0);
}
KStringView::KStringView(const char *s) {
	set_view(s, s ? strlen(s) : 0);
}
KStringView::KStringView(const char *s, int len) {
	set_view(s, len);
}
KStringView::KStringView(const char *s, const char *e) {
	set_view(s, e);
}
KStringView::KStringView(const std::string &s) {
	set_view(s.c_str(), s.length());
}
KStringView::KStringView(const KStringView &s) {
	set_view(s.mStr, s.mLen);
}
KStringView::~KStringView() {
}
char KStringView::operator[](int index) const {
	return mStr[index];
}
bool KStringView::operator==(const KStringView &s) const {
	return compare(s) == 0;
}
bool KStringView::operator!=(const KStringView &s) const {
	return compare(s) != 0;
}
const char * KStringView::data() const {
	return mStr;
}
const char * KStringView::begin() const {
	return mStr;
}
const char * KStringView::end() const {
	return mStr + mLen;
}
void KStringView::set_view(const char *s, int len) { // len = string length (excluding null terminator)
	if (s) {
		mStr = s;
		mLen = len;
	} else {
		mStr = EMPTY_STR;
		mLen = 0;
	}
}
void KStringView::set_view(const char *s, const char *e) {
	if (s && e && s <= e) {
		mStr = s;
		mLen = e - s;
	} else {
		mStr = EMPTY_STR;
		mLen = 0;
	}
}
bool KStringView::empty() const {
	return mLen == 0;
}
int KStringView::size() const {
	return mLen;
}
uint32_t KStringView::hash() const {
	return KStringUtils::gethash(mStr, mLen);
}
void KStringView::getString(char *out, int maxsize) const {
	int len = maxsize-1;
	if (mLen < len) len = mLen;
	strncpy(out, mStr, len);
	out[len] = '\0';
}
std::string KStringView::toStdString() const {
	return std::string(mStr, mLen);
}
std::wstring KStringView::toWide() const {
#if 1
	int len = K__Utf8ToWide(nullptr, 0, mStr, mLen);
	std::wstring ws(len + 32, 0);
	K__Utf8ToWide(&ws[0], ws.size(), mStr, mLen);
	ws.resize(wcslen(ws.c_str())); // ws.size が正しい文字列長さを返すように調整する
	return ws;
#else
	std::string u8 = toStdString();
	return K__Utf8ToWideStd(u8);
#endif
}
std::string KStringView::toAnsi(const char *_locale) const {
#if 1
	std::wstring ws = toWide();
	int len = K__WideToAnsi(nullptr, 0, ws.c_str(), _locale);
	std::string mb(len + 32, 0);
	K__WideToAnsi(&mb[0], mb.size(), ws.c_str(), _locale);
	mb.resize(strlen(mb.c_str())); // mb.size が正しい文字列長さを返すように調整する
	return mb;
#else
	std::string u8 = toStdString();
	std::wstring ws = K__Utf8ToWideStd(u8);
	return K__WideToAnsiStd(ws, _locale);
#endif
}
uint32_t KStringView::hexToUint() const {
	uint32_t u = 0;
	hexToUintTry(&u);
	return u;
}
bool KStringView::hexToUintTry(uint32_t *result) const {
	char s[NUMSTRLEN] = {0};
	getString(s, NUMSTRLEN);
	return KStringUtils::hexToUintTry(s, result);
}
int KStringView::toInt() const {
	int i = 0;
	toIntTry(&i);
	return i;
}
bool KStringView::toIntTry(int *result) const {
	char s[NUMSTRLEN] = {0};
	getString(s, NUMSTRLEN);
	return KStringUtils::toIntTry(s, result);
}
uint32_t KStringView::toUint() const {
	uint32_t u = 0;
	toUintTry(&u);
	return u;
}
bool KStringView::toUintTry(uint32_t *result) const {
	char s[NUMSTRLEN] = {0};
	getString(s, NUMSTRLEN);
	return KStringUtils::toUintTry(s, result);
}
uint64_t KStringView::toUint64() const {
	uint64_t u = 0;
	toUint64Try(&u);
	return u;
}
bool KStringView::toUint64Try(uint64_t *result) const {
	char s[NUMSTRLEN] = {0};
	getString(s, NUMSTRLEN);
	return KStringUtils::toUint64Try(s, result);
}
float KStringView::toFloat() const {
	float f = 0;
	toFloatTry(&f);
	return f;
}
bool KStringView::toFloatTry(float *result) const {
	char s[NUMSTRLEN] = {0};
	getString(s, NUMSTRLEN);
	return KStringUtils::toFloatTry(s, result);
}
bool KStringView::startsWith(char ch) const {
	return mLen > 0 && mStr[0] == ch;
}
bool KStringView::startsWith(const char *sub) const {
	int sublen = strlen(sub);
	if (sublen <= mLen) {
		if (strncmp(mStr, sub, sublen) == 0) {
			return true;
		}
	}
	return false;
}
bool KStringView::endsWith(char ch) const {
	return mLen > 0 && mStr[mLen-1] == ch;
}
bool KStringView::endsWith(const char *sub) const {
	int sublen = strlen(sub);
	if (sublen <= mLen) {
		if (strncmp(mStr+mLen-sublen, sub, sublen) == 0) {
			return true;
		}
	}
	return false;
}
KStringView KStringView::trimUtf8Bom() const {
	const char *s = KStringUtils::skipUtf8Bom(mStr);
	const char *e = end();
	return KStringView(s, e-s);
}
KStringView KStringView::trim() const {
	const char *s = begin();
	const char *e = end();
	while (isascii(*s) && isblank(*s)) {
		s++;
	}
	while (s < e && isascii(e[-1]) && isblank(e[-1])) {
		e--;
	}
	return KStringView(s, e-s);
}
KStringView KStringView::substr(int start, int count) const {
	if (mLen  < start      ) start = mLen;
	if (start < 0          ) start = 0;
	if (count < 0          ) count = mLen-start; // count が負の値だったら末尾まですべて得る
	if (mLen  < start+count) count = mLen-start; // count が長すぎたら調整
	return KStringView(mStr+start, count);
}
int KStringView::findChar(char c, int start) const {
	for (int i=start; i<mLen; i++) {
		if (mStr[i] == c) return i;
	}
	return -1;
}

// 文字列 substr を探し、見つかった範囲を返す
int KStringView::find(const char *substr, int start, KStringView *result_view) const {
	size_t sublen = strlen(substr);
	const char *t = strstr(mStr, substr);
	if (t && t+sublen < mStr+mLen) {
		if (result_view) *result_view = KStringView(t, sublen);
		return t - mStr;
	}
	return -1;
}

// 文字列 start_tok と end_tok を探す。
// start_tok と end_tok を含まない範囲（内側の範囲）を inner_range にセットし、
// start_tok と end_tok を含む範囲（外側の範囲）を outer_range にセットする。
// 例えば文字列 "AAA <BBB> CCC" から "<" と ">" を探す場合、
// 内側の範囲は "BBB" であり外側の範囲は "<BBB>" になる
bool KStringView::findRange(const char *start_tok, const char *end_tok, KStringView *inner_range, KStringView *outer_range) const {
	K__Assert(start_tok);
	K__Assert(end_tok);
	KStringView startrange;
	if (find(start_tok, 0, &startrange) < 0) {
		return false;
	}
	KStringView endrange;
	if (find(end_tok, 0, &endrange) < 0) {
		return false;
	}
	// tok2 のほうが tok1 より前に見つかった場合は失敗
	if (endrange.data() < startrange.data()) {
		return false;
	}
	// 開始トークン末尾から終了トークン先頭まで
	if (inner_range) {
		inner_range->set_view(startrange.end(), endrange.begin());
	}
	// 開始トークン先頭から終了トークン末尾まで
	if (outer_range) {
		outer_range->set_view(startrange.begin(), endrange.end());
	}
	return true;
}
KStringView KStringView::pathLast() const {
	int i = findLastDelim();
	if (i >= 0) {
		const char *s = mStr + i + 1; // デリミタの一つ右を指す
		return KStringView(s, mLen-(i+1));
	}
	return KStringView();
}
int KStringView::findLastChar(char c) const {
	for (int i=mLen-1; i>=0; i--) {
		if (mStr[i] == c) {
			return i;
		}
	}
	return -1;
}
int KStringView::findLastDelim() const {
	for (int i=mLen-1; i>=0; i--) {
		if (isPathDelim(mStr[i])) {
			return i;
		}
	}
	return -1;
}
KStringView KStringView::pathPopLast() const {
	int i = findLastDelim();
	if (i >= 0) {
		return KStringView(mStr, i-1); // デリミタを含めない
	}
	return KStringView();
}
KStringView KStringView::pathExt() const {
	int i = findLastChar('.');
	if (i >= 0) {
		return KStringView(mStr+i, mLen-i);
	}
	return KStringView();
}
KStringView KStringView::pathPopExt() const {
	int i = findLastChar('.');
	if (i >= 0) {
		return KStringView(mStr, i);
	}
	return KStringView();
}
KStringView KStringView::pathPopLastDelim() const {
	if (isPathDelim(mStr[mLen-1])) {
		return KStringView(mStr, mLen-1);
	} else {
		return KStringView(mStr, mLen);
	}
}
bool KStringView::eq(const KStringView &other) const {
	return compare(other, false) == 0;
}
int KStringView::compare(const KStringView &other, bool ignore_case) const {
	if (mLen == other.mLen) {
		if (ignore_case) {
			return _strnicmp(mStr, other.mStr, mLen);
		} else {
			return strncmp(mStr, other.mStr, mLen);
		}
	} else {
		return mLen - other.mLen;
	}
}
int KStringView::pathCommonSize(const KStringView &other) const {
	int len = 0;
	for (int i=0; ; i++) {
		if (mStr[i] == other.mStr[i]) {
			if (isPathDelim(mStr[i])) {
				len = i+1;
			}
			if (i == mLen) {
				len = i;
				break;
			}
		} else {
			break;
		}
	}
	return len;
}
int KStringView::pathCompare(const KStringView &other, bool ignore_case, bool ignore_path) const {
	if (ignore_path) {
		KStringView path1 = pathPopLast();
		KStringView path2 = other.pathPopLast();
		return path1.compare(path2, ignore_case);
	} else {
		return compare(other, ignore_case);
	}
}
int KStringView::pathCompareLast(const KStringView &last) const {
	return pathLast().compare(last, false);
}
int KStringView::pathCompareExt(const KStringView &ext) const {
	return pathExt().compare(ext, false);
}
bool KStringView::pathGlob(const KStringView &pattern) const {
	const int MAXSIZE = 1024;
	char tmp[MAXSIZE] = {0};
	strncpy(tmp, mStr, MAXSIZE-1);
	tmp[MAXSIZE-1] = '\0';
	return KPathUtils::K_PathGlob(tmp, pattern.mStr);
}
std::string KStringView::pathNormalized() const {
	char tmp[KPathUtils::MAX_SIZE] = {0};
	KPathUtils::K_PathNormalize(tmp, sizeof(tmp), mStr);
	return tmp;
}
std::string KStringView::pathPushLast(const KStringView &last) const {
	char tmp[KPathUtils::MAX_SIZE] = {0};
	KPathUtils::K_PathPushLast(tmp, sizeof(tmp), mStr, last.mStr);
	return tmp;
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
std::vector<KStringView> KStringView::split(const char *delims, bool condense_delims, bool _trim, int maxcount, KStringView *out_rest) const {
	K__Assert(delims);
	std::vector<KStringView> result;
	int s = 0; // 開始インデックス
	int i = 0;
	while (i < mLen) {
		if (strchr(delims, mStr[i])) {
			// 分割文字が見つかった or 末尾に達した
			// [トークンを登録する]
			// デリミタ圧縮が有効 (condense_delims) ならばトークン長さをチェック (s < p) し、
			// 長さが 0 ならデリミタが連続しているのでトークン追加しない。
			// デリミタ圧縮無効ならば長さチェックしない（長さが 0 であっても有効なトークンとして追加する）
			if (!condense_delims || s<i) {
				KStringView sv = KStringView(mStr+s, i-s);
				if (_trim) {
					result.push_back(sv.trim());
				} else {
					result.push_back(sv);
				}
			} 
			// デリミタをスキップして次トークンの先頭に移動する
			// condense_delims がセットされているならば連続するデリミタを1つの塊として扱う
			size_t span = 1;
			if (condense_delims) {
				span = strspn(mStr+i, delims);
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
	if (s < mLen) {
		// s が終端文字に達していない。
		// 入力文字列がまだ残っている
		KStringView rest(mStr + s, mLen - s);
		if (maxcount <= 0 || (int)result.size() < maxcount) { // 最後のトークンを追加
			if (_trim) {
				result.push_back(rest.trim());
			} else {
				result.push_back(rest);
			}
		} else if (out_rest) {
			*out_rest = rest; // 最後の文字列を余り文字列としてセット
		}
	}
	return result;
}
std::vector<KStringView> KStringView::splitComma(int maxcount, KStringView *rest) const {
	return split(",", true, true, maxcount, rest);
}
std::vector<KStringView> KStringView::splitPaths(int maxcount, KStringView *rest) const {
	return split("\\/", true, true, maxcount, rest);
}
std::vector<KStringView> KStringView::splitLines(bool skip_empty_lines, bool _trim) const {
	// split("\r\n") としてしまうと空行が検出できなくなる（\r\n\r\n をまとめて一つの改行文字として処理してしまう）
	// かといって condense_delims を true にすると \r \n \r \n と分割して３つの空行があると判定されてしまうため
	// どちらにしても split 関数ではうまく処理できない。行分割専用のコードを使う
	std::vector<KStringView> result;
	const char *s = nullptr;
	int len = 0;
	for (int i=0; i<mLen; i++) {
		if (mStr[i]=='\r' && mStr[i+1]=='\n') { // 改行コードが2文字
			KStringView line = KStringView(s, len);
			if (_trim) line = line.trim();
			if (!skip_empty_lines || !line.empty()) {
				result.push_back(line);
			}
			i++; // ※1文字余計に進める
			s = nullptr;
			len = 0;

		} else if (mStr[i]=='\r' || mStr[i]=='\n') { // 改行コードが1文字
			KStringView line = KStringView(s, len);
			if (_trim) line = line.trim();
			if (!skip_empty_lines || !line.empty()) {
				result.push_back(line);
			}
			s = nullptr;
			len = 0;
			
		} else if (len == 0) {
			s = mStr + i;
			len = 1;

		} else {
			len++;
		}
	}
	if (len > 0) {
		result.push_back(KStringView(s, len));
	}
	return result;
}
#pragma endregion // KStringView





#pragma region KString
static const char *g_EmptyStr = "";

KString KString::format(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	std::string result = KStringUtils::K_vsprintf(fmt, args);
	va_end(args);
	return result;
}
KString KString::join(const KStringView &s1, const KStringView &s2, const char *sep) {
	if (s1.empty()) {
		return s2.toStdString();
	}
	if (s2.empty()) {
		return s1.toStdString();
	}
	return s1.toStdString() + sep + s2.toStdString();
}
KString KString::join(const KStringView *list, int size, const char *sep) {
	if (size <= 0 || list==nullptr) return std::string();

	// 最初の文字列
	std::string s = list[0].toStdString();

	// セパレータを挟んで後続文字列を追加していく
	for (int i=1; i<size; i++) {
		s += sep;
		s += list[i].toStdString();
	}
	return s;
}
KString KString::join(const KString &s1, const KString &s2, const char *sep) {
	if (s1.empty()) {
		return s2.toStdString();
	}
	if (s2.empty()) {
		return s1.toStdString();
	}
	return s1.toStdString() + sep + s2.toStdString();
}
KString KString::join(const KString *list, int size, const char *sep) {
	if (size <= 0 || list==nullptr) return std::string();

	// 最初の文字列
	std::string s = list[0].toStdString();

	// セパレータを挟んで後続文字列を追加していく
	for (int i=1; i<size; i++) {
		s += sep;
		s += list[i].toStdString();
	}
	return s;
}
KString KString::fromWide(const std::wstring &ws) {
	std::string u8 = KStringUtils::wideToUtf8(ws);
	return KString(u8);
}
KString KString::fromAnsi(const std::string &mb, const char *_locale) {
	std::wstring ws = KStringUtils::ansiToWide(mb, _locale);
	return KString::fromWide(ws);
}
KString KString::fromBin(const void *data, size_t size) {
	std::wstring ws = KStringUtils::binToWide(data, size);
	return KString::fromWide(ws);
}
KString KString::fromBin(const std::string &bin) {
	std::wstring ws = KStringUtils::binToWide(bin.data(), bin.size());
	return KString::fromWide(ws);
}
KString::KString() {
	mStr = nullptr;
	mLen = 0;
}
KString::KString(const char *s) {
	mStr = nullptr;
	_assign(s, -1);
}
KString::KString(const std::string &s) {
	mStr = nullptr;
	_assign(s.c_str(), s.size());
}
KString::KString(const KStringView &s) {
	mStr = nullptr;
	_assign(s.data(), s.size());
}
KString::KString(const KString &s) {
	mStr = nullptr;
	_assign(s.c_str(), s.size());
}
KString::~KString() {
	if (mStr) free(mStr);
}
void KString::_assign(const char *s, int n) {
	if (s) {
		int len = n >= 0 ? n : strlen(s);
		char *ptr = (char *)realloc(mStr, len + 1); // +1 for null terminator
		if (ptr) {
			mLen = len;
			mStr = ptr;
			memcpy(mStr, s, len);
			mStr[len] = '\0';
			return;
		}
	}
	// s==nullptr の場合または realloc が nullptr を返した場合はバッファを開放する
	free(mStr);
	mStr = nullptr;
	mLen = 0;
}
const char * KString::c_str() const {
	return mStr ? mStr : g_EmptyStr; // nullを返さないように
}
bool KString::empty() const {
	return mLen == 0;
}
bool KString::notEmpty() const {
	return mLen > 0;
}
int KString::size() const {
	return mLen;
}
std::string KString::toStdString() const {
	return std::string(c_str());
}
std::string KString::toAnsi(const char *_locale) const {
	return KStringUtils::utf8ToAnsi(c_str(), _locale);
}
std::wstring KString::toWide() const {
	return KStringUtils::utf8ToWide(c_str());
}
bool KString::equals(const char *s) const {
	return strcmp(c_str(), s) == 0;
}
bool KString::equals(const KString &s) const {
	return strcmp(c_str(), s.c_str()) == 0;
}
int KString::find(const char *substr, int start) const {
	return KStringUtils::find(c_str(), substr, start);
}
int KString::find(const KString &substr, int start) const {
	return find(substr.c_str(), start);
}
int KString::findChar(char chr, int start) const {
	return KStringUtils::findChar(c_str(), chr, start);
}
bool KString::startsWith(const char *substr) const {
	return KStringUtils::startsWith(c_str(), substr);
}
bool KString::startsWith(const KString &substr) const {
	return startsWith(substr.c_str());
}
bool KString::endsWith(const char *substr) const {
	return KStringUtils::endsWith(c_str(), substr);
}
bool KString::endsWith(const KString &substr) const {
	return endsWith(substr.c_str());
}
int KString::toInt(int def) const {
	return KStringUtils::toInt(c_str(), def);
}
bool KString::toIntTry(int *val) const {
	return KStringUtils::toIntTry(c_str(), val);
}
float KString::toFloat(float def) const {
	return KStringUtils::toFloat(c_str(), def);
}
bool KString::toFloatTry(float *val) const {
	return KStringUtils::toFloatTry(c_str(), val);
}
KString KString::replace(int start, int count, const char *str) const {
	std::string s = toStdString();
	KStringUtils::replace(s, start, count, str);
	return s;
}
KString KString::replace(int start, int count, const KString &str) const {
	return replace(start, count, str.c_str());
}

KString KString::replace(const char *before, const char *after) const {
	std::string s = toStdString();
	KStringUtils::replace(s, before, after);
	return s;
}
KString KString::replace(const KString &before, const KString &after) const {
	return replace(before.c_str(), after.c_str());
}

KString KString::replaceChar(char before, char after) const {
	std::string s = toStdString();
	KStringUtils::replaceChar(const_cast<char*>(s.data()), before, after);
	return s;
}
KString KString::remove(int start, int count) const {
	if (start < 0 || mLen <= start) {
		return KString(); // fail
	}
	if (count > 0) {
		// start から count 文字を削除した残りを得る
		std::string s = toStdString();
		s.erase(start, count);
		return s;
	
	} else if (count < 0) {
		// start 以降を削除する = 最初の start 文字を得る
		std::string s = toStdString();
		s = s.substr(0, start);
		return s.substr(0, start);
	
	} else { // count == 0
		//削除なし。同一文字列を返す
		return *this;
	}
}
KString KString::subString(int start, int count) const {
	if (start < 0 || mLen <= start) {
		return KString(); // fail
	}
	if (count > 0) {
		// start から count 文字を返す
		std::string s = toStdString();
		s = s.substr(start, count);
		return s;
	
	} else if (count < 0) {
		// start 以降を返す
		std::string s = toStdString();
		s = s.substr(start);
		return s;

	} else { // count == 0
		// 空文字列を返す
		return KString();
	}
}
KString KString::trimUtf8Bom() const {
	const char *s = KStringUtils::skipUtf8Bom(c_str());
	return s;
}
KString KString::trim() const {
	std::string s = toStdString();
	KStringUtils::trim(s);
	return s;
}
KStringView KString::view() const {
	return KStringView(mStr, mLen);
}

static std::vector<KString> StringViewList_to_StringList(const KStringViewList &vlist) {
	std::vector<KString> slist;
	for (auto it=vlist.begin(); it!=vlist.end(); ++it) {
		slist.push_back(it->toStdString());
	}
	return slist;
}

std::vector<KString> KString::split(const char *delims, bool condense_delims, bool _trim, int maxcount, KString *rest) const {
	KStringView vrest;
	KStringViewList vlist = view().split(delims, condense_delims, _trim, maxcount, &vrest);
	if (rest) *rest = vrest.toStdString();
	return StringViewList_to_StringList(vlist);
}
std::vector<KString> KString::splitComma(int maxcount, KString *rest) const {
	KStringView vrest;
	KStringViewList vlist = view().splitComma(maxcount, &vrest);
	if (rest) *rest = vrest.toStdString();
	return StringViewList_to_StringList(vlist);
}
std::vector<KString> KString::splitPaths(int maxcount, KString *rest) const {
	KStringView vrest;
	KStringViewList vlist = view().splitPaths(maxcount, &vrest);
	if (rest) *rest = vrest.toStdString();
	return StringViewList_to_StringList(vlist);
}
std::vector<KString> KString::splitLines(bool skip_empty_lines, bool _trim) const {
	KStringViewList vlist = view().splitLines(skip_empty_lines, _trim);
	return StringViewList_to_StringList(vlist);
}
KString KString::pathExt() const {
	return view().pathExt();
}
KString KString::pathLast() const {
	return view().pathLast();
}
KString KString::pathPopExt() const {
	return view().pathPopExt();
}
KString KString::pathPopLast() const {
	return view().pathPopLast();
}
KString KString::pathPopLastDelim() const {
	return view().pathPopLastDelim();
}
KString KString::pathJoin(const KString &last) const {
	char tmp[KPathUtils::MAX_SIZE] = {0};
	KPathUtils::K_PathPushLast(tmp, sizeof(tmp), mStr, last.mStr);
	return tmp;
}
KString KString::pathNormalized() const {
	char tmp[KPathUtils::MAX_SIZE] = {0};
	KPathUtils::K_PathNormalize(tmp, sizeof(tmp), mStr);
	return tmp;
}
int KString::pathCommonSize(const KString &other) const {
	return view().pathCommonSize(other.view());
}
int KString::pathCompare(const KString &other, bool ignore_case, bool ignore_path) const {
	return view().pathCompare(other.view(), ignore_case, ignore_path);
}
int KString::pathCompareLast(const KString &last) const {
	return view().pathCompareLast(last.view());
}
int KString::pathCompareExt(const KString &ext) const {
	return view().pathCompareExt(ext.view());
}
bool KString::pathGlob(const KString &pattern) const {
	return view().pathGlob(pattern.view());
}

KString & KString::operator = (const KString &s) {
	if (mStr) free(mStr);
	mStr = nullptr;
	mLen = 0;
	_assign(s.c_str(), s.size());
	return *this;
}
char KString::operator[](int index) const {
	return c_str()[index];
}
bool KString::operator == (const KString &s) const {
	return strcmp(c_str(), s.c_str()) == 0;
}
bool KString::operator != (const KString &s) const {
	return strcmp(c_str(), s.c_str()) != 0;
}
#pragma endregion // KString





#pragma region KStringUtils

int KStringUtils::wideToAnsi(char *out_ansi, int max_out_bytes, const wchar_t *ws, const char *_locale) {
	return K__WideToAnsi(out_ansi, max_out_bytes, ws, _locale);
}
std::string KStringUtils::wideToAnsi(const std::wstring &ws, const char *_locale) {
	return K__WideToAnsiStd(ws, _locale);
}

int KStringUtils::ansiToWide(wchar_t *out_wide, int max_out_wchars, const char *ansi, const char *_locale) {
	return K__AnsiToWide(out_wide, max_out_wchars, ansi, _locale);
}
std::wstring KStringUtils::ansiToWide(const std::string &ansi, const char *_locale) {
	return K__AnsiToWideStd(ansi, _locale);
}

int KStringUtils::wideToUtf8(char *out_u8, int max_out_bytes, const wchar_t *ws) {
	return K__WideToUtf8(out_u8, max_out_bytes, ws);
}
std::string KStringUtils::wideToUtf8(const std::wstring &ws) {
	return K__WideToUtf8Std(ws);
}

int KStringUtils::utf8ToWide(wchar_t *out_ws, int max_out_widechars, const char *u8) {
	return K__Utf8ToWide(out_ws, max_out_widechars, u8, 0);
}
std::wstring KStringUtils::utf8ToWide(const std::string &u8) {
	return K__Utf8ToWideStd(u8);
}

void KStringUtils::ansiToUtf8(char *u8, int size, const char *ansi, const char *_locale) {
	std::wstring ws = K__AnsiToWideStd(ansi, _locale);
	K__WideToUtf8(u8, size, ws.c_str());
}
std::string KStringUtils::ansiToUtf8(const std::string &ansi, const char *_locale) {
	std::wstring ws = K__AnsiToWideStd(ansi, _locale);
	return K__WideToUtf8Std(ws);
}

void KStringUtils::utf8ToAnsi(char *ansi, int size, const char *u8, const char *_locale) {
	std::wstring ws = K__Utf8ToWideStd(u8);
	K__WideToAnsi(ansi, size, ws.c_str(), _locale);
}
std::string KStringUtils::utf8ToAnsi(const std::string &u8, const char *_locale) {
	std::wstring ws = K__Utf8ToWideStd(u8);
	return K__WideToAnsiStd(ws, _locale);
}

std::wstring KStringUtils::binToWide(const void *data, int size) {
	// エンコード不明の文字列からワイド文字列を得る
	// あくまでも日本語前提なので、ここでは SJIS または UTF8 が使われていると仮定する
	if (data == nullptr || size == 0) {
		return std::wstring();
	}

	// BOM で始まるデータなら UTF8 で確定させる
	if (K__StartsWithUtf8Bom(data, size)) {
		return K__Utf8ToWideStd((const char *)data);
	}

	std::wstring ws;

	// 現在のロケールにおけるマルチバイト文字列として変換
	ws = K__AnsiToWideStd((const char *)data, "");
	if (ws.size() > 0) {
		return ws;
	}

	// SJIS であると仮定して変換
	// （非日本語環境でゲームを実行しているとき、SJIS保存されたファイルをロードしようとしている場合など）
	// SJISをUTF8として解釈できる事が多いが、UTF8をSJISとして解釈できることは少ないため、
	// 先にSJISへの変換を試みる。入力がUTF8の場合は大体失敗してくれるはず。
	ws = K__AnsiToWideStd((const char *)data, "JPN");
	if (ws.size() > 0) {
		return ws;
	}

	// BOM なしの UTF8 であると仮定して変換
	ws = K__Utf8ToWideStd((const char *)data);
	if (ws.size() > 0) {
		return ws;
	}

	// どの方法でも変換できなかった
	return std::wstring();
}

/// u8の先頭が utf8 bom で始まっているなら、その次の文字アドレスを返す。
/// utf8 bom で始まっていない場合はそのまま u8 を返す
const char * KStringUtils::skipUtf8Bom(const char *s) {
	return K__SkipUtf8Bom(s);
}

/// strtol を簡略化したもの。
///
/// 整数への変換が成功すれば val に値をセットして true を返す。
/// 失敗した場合は何もせずに false を返す
bool KStringUtils::toIntTry(const char *s, int *val) {
	return K__strtoi(s, val);
}
bool KStringUtils::toIntTry(const std::string &s, int *val) {
	return K__strtoi(s.c_str(), val);
}
int KStringUtils::toInt(const char *s, int def) {
	int retval = def;
	K__strtoi(s, &retval);
	return retval;
}
int KStringUtils::toInt(const std::string &s, int def) {
	return toInt(s.c_str(), def);
}
bool KStringUtils::toFloatTry(const char *s, float *val) {
	return K__strtof(s, val);
}
bool KStringUtils::toFloatTry(const std::string &s, float *val) {
	return K__strtof(s.c_str(), val);
}

/// strtof を簡略化したもの
float KStringUtils::toFloat(const char *s, float def) {
	float retval = def;
	K__strtof(s, &retval);
	return retval;
}
float KStringUtils::toFloat(const std::string &s, float def) {
	return toFloat(s.c_str(), def);
}
bool KStringUtils::toUintTry(const char *s, uint32_t *val) {
	return K__strtoui32(s, val);
}
bool KStringUtils::toUintTry(const std::string &s, uint32_t *val) {
	return K__strtoui32(s.c_str(), val);
}
uint32_t KStringUtils::toUint(const char *s) {
	uint32_t retval = 0;
	K__strtoui32(s, &retval);
	return retval;
}
uint32_t KStringUtils::toUint(const std::string &s) {
	return toUint(s.c_str());
}

bool KStringUtils::toUint64Try(const char *s, uint64_t *val) {
	return K__strtoui64(s, val);
}
bool KStringUtils::toUint64Try(const std::string &s, uint64_t *val) {
	return K__strtoui64(s.c_str(), val);
}

bool KStringUtils::hexToUintTry(const char *s, uint32_t *val) {
	// strtoul に空文字を渡すと正常終了との区別がつかない
	// (err インジケーターが終端文字を指してしまい、エラー扱いにならない）ので、
	// あらかじめ除外しておく
	if (s && s[0]) {
		char *err = nullptr;
		uint32_t n = (uint32_t)strtoul(s, &err, 16);
		if (err[0] == '\0') {
			if (val) *val = n;
			return true;
		}
	}
	return false;
}

bool KStringUtils::startsWith(const char *s, const char *sub) {
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
bool KStringUtils::startsWith(const std::string &s, const std::string &sub) {
	return startsWith(s.c_str(), sub.c_str());
}
bool KStringUtils::endsWith(const char *s, const char *sub) {
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
bool KStringUtils::endsWith(const std::string &s, const std::string &sub) {
	return endsWith(s.c_str(), sub.c_str());
}
bool KStringUtils::equals(const char *s1, const char *s2) {
	return s1 && s2 && strcmp(s1, s2)==0;
}
bool KStringUtils::isEmpty(const char *s) {
	return s==nullptr || s[0]==0;
}
bool KStringUtils::trim(char *s) {
	KStringView sv0(s);
	KStringView sv1 = sv0.trim();
	if (sv1 != sv0) {
		int lpos = sv1.begin() - s;
		int rpos = sv1.end() - s;
		s[rpos] = '\0';
		if (lpos > 0) {
			memmove(s, s+lpos, rpos - lpos + 1); // 終端文字も含めて移動する
		}
		return true;
	}
	return false;
}

bool KStringUtils::trim(std::string &s) {
	bool ret = false;
	if (s.size() > 0) {
		ret = trim(&s[0]);
		s.resize(strlen(s.c_str()));
	}
	return ret;
}
int KStringUtils::find(const char *s, const char *sub, int start) {
	return K__StrFind(s, sub, start);
}
int KStringUtils::find(const std::string &s, const std::string &sub, int start) {
	return K__StrFind(s.c_str(), sub.c_str(), start);
}
int KStringUtils::findChar(const char *s, char chr, int start) {
	K__Assert(s);
	if (start < 0) start = 0;
	if ((int)strlen(s) <= start) return -1;
	const char *p = strchr(s + start, chr);
	return p ? (p - s) : -1;
}
int KStringUtils::findChar(const std::string &s, char chr, int start) {
	return findChar(s.c_str(), chr, start);
}
void KStringUtils::replace(std::string &s, int start, int count, const char *str) {
	K__Assert(str);
	s.erase(start, count);
	s.insert(start, str);
}
void KStringUtils::replace(std::string &s, const char *before, const char *after) {
	K__Assert(before);
	K__Assert(after);
	if (isEmpty(before)) return;
	int pos = find(s.c_str(), before);
	while (pos >= 0) {
		replace(s, pos, strlen(before), after);
		pos += strlen(after);
		pos = find(s.c_str(), before, pos);
	}
}
void KStringUtils::replaceChar(char *s, char before, char after) {
	K__Assert(s);
	for (size_t i=0; s[i]; i++) {
		if (s[i] == before) {
			s[i] = after;
		}
	}
}
uint32_t KStringUtils::gethash(const char *s, int size) {
	// CRC32 の計算を行う
	// 厳密には crc32 ではなく crc32b であることに注意. PHP の crc32 と同じで, ZIP のチェックサムとして使える
	// https://stackoverflow.com/questions/15861058/what-is-the-difference-between-crc32-and-crc32b
	//
	// CRC32の逆演算Pythonコード
	// https://qiita.com/taiyaki8926/items/94b8f12973d477749d10
	//
	// Cyclic Redundancy Check(CRC)を理解する
	// https://qiita.com/tobira-code/items/dbcffc41f54201130b6c
	//
	// PNGで使うCRC32を計算する
	// https://qiita.com/mikecat_mixc/items/e5d236e3a3803ef7d3c5
	//
	static const uint32_t table[256] = {
		0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
		0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
		0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
		0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
		0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
		0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
		0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
		0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
		0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
		0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
		0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
		0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
		0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
		0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
		0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
		0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
	};
	if (size == 0) size = strlen(s);
	uint32_t crc = ~0;
	for (int i=0; i<size; i++){
		crc = (crc >> 8) ^ table[(crc ^ ((const uint8_t*)s)[i]) & 0xFF];
	}
	return ~crc;
}
std::string KStringUtils::K_vsprintf(const char *fmt, va_list args) {
	return K__vsprintf_std(fmt, args);
}
std::string KStringUtils::K_sprintf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	std::string result = K__vsprintf_std(fmt, args);
	va_end(args);
	return result;
}

namespace Test {
void Test_str() {
	{
	#if 1
		const wchar_t test_ws[] = L"\u3042\u3044\u3046\u3048\u304a"; // "あいうえお" in UTF16
		const char test_mb[] = "\x82\xa0\x82\xa2\x82\xa4\x82\xa6\x82\xa8"; // "あいうえお" in SJIS
		const char test_u8[] = "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a"; // "あいうえお" in UTF8
	#else
		const wchar_t test_ws[] = L"あいうえお"; // u3042 u3044 u3046 u3048 u304A
		const char test_mb[] = "\x82\xa0\x82\xa2\x82\xa4\x82\xa6\x82\xa8"; // SJIS (あ = 0x82a0)
		const char test_u8[] = u8"あいうえお"; // E38182 E38184 E38186 E38188 E3818A
	#endif
		wchar_t ws[16] = {0};
		char u8[16] = {0};
		char mb[16] = {0};
		int len = 0;

		// Wide --> Utf8
		KStringUtils::wideToUtf8(u8, sizeof(u8), test_ws); // 変換する
		K__Verify(strcmp(test_u8, u8) == 0);

		// Utf8 --> Wide
		KStringUtils::utf8ToWide(ws, sizeof(ws)/sizeof(wchar_t), test_u8); // 変換する
		K__Verify(wcscmp(test_ws, ws) == 0);

		// Wide --> Ansi (SJIS)
		KStringUtils::wideToAnsi(mb, sizeof(mb), test_ws, "jpn"); // 変換する
		K__Verify(strcmp(test_mb, mb) == 0);

		// Wide --> Ansi (SJIS)
		KStringUtils::wideToAnsi(mb, sizeof(mb), test_ws, "japanese"); // 変換する
		K__Verify(strcmp(test_mb, mb) == 0);

		// Wide --> Ansi (SJIS)
		KStringUtils::wideToAnsi(mb, sizeof(mb), test_ws, "japanese_japan.932"); // 変換する
		K__Verify(strcmp(test_mb, mb) == 0);

		// バッファ長さを超えた場合でも必ず終端文字が入る
		K__Verify(KStringUtils::wideToUtf8(nullptr, 0, L"ABCD_") >= 5+1); // 終端文字を含めたサイズ以上の値を返す
		K__Verify(KStringUtils::wideToUtf8(nullptr, 0, L"") >= 0+1); // 終端文字を含めたサイズ以上の値を返す
		{
			char s[4] = {'\xff', '\xff', '\xff', '\xff'};
			K__Verify(KStringUtils::wideToUtf8(s, 4, L"ABCD") == 0); // バッファが足りないのでエラーになる
		}

		char lc[256];
		strcpy_s(lc, sizeof(lc), setlocale(LC_CTYPE, nullptr));
		{
			setlocale(LC_CTYPE, "JPN");

			// Wide --> Ansi (Current Locale)
			KStringUtils::ansiToWide(ws, sizeof(ws)/sizeof(wchar_t), test_mb, "");
			K__Verify(wcscmp(test_ws, ws) == 0);
		}
		setlocale(LC_CTYPE, lc);
	}
	{
		std::string s;
		s="abc";    KStringUtils::replace(s, "a", "");   K__Verify(s=="bc");
		s="abcabc"; KStringUtils::replace(s, "a", "");   K__Verify(s=="bcbc");
		s="abcabc"; KStringUtils::replace(s, "a", "ax"); K__Verify(s=="axbcaxbc");
		s="abc";    KStringUtils::replace(s, "", "x");   K__Verify(s=="abc");
		s="abc";    KStringUtils::replace(s, "", "");    K__Verify(s=="abc");

		s="   aaabbb "; KStringUtils::trim(s); K__Verify(s=="aaabbb");
		s="aaabbb ";    KStringUtils::trim(s); K__Verify(s=="aaabbb");
		s="   aaabbb";  KStringUtils::trim(s); K__Verify(s=="aaabbb");
		s="aaabbb";     KStringUtils::trim(s); K__Verify(s=="aaabbb");
		s="";           KStringUtils::trim(s); K__Verify(s=="");

		K__Verify(KStringUtils::startsWith("abc", "ab") == true);
		K__Verify(KStringUtils::startsWith("", "abc") == false);
		K__Verify(KStringUtils::startsWith("abc", "") == true);
		K__Verify(KStringUtils::startsWith("", "") == true);
		K__Verify(KStringUtils::startsWith(" abc", "ab") == false);
		K__Verify(KStringUtils::startsWith("abc", "abcd") == false);

		K__Verify(KStringUtils::endsWith("abc", "bc") == true);
		K__Verify(KStringUtils::endsWith("", "abc") == false);
		K__Verify(KStringUtils::endsWith("abc", "") == true);
		K__Verify(KStringUtils::endsWith("", "") == true);
		K__Verify(KStringUtils::endsWith("abc ", "bc") == false);
		K__Verify(KStringUtils::endsWith("abc", "xabc") == false);

		K__Verify(KStringUtils::find("aaa x", "x") == 4);
		K__Verify(KStringUtils::find("aaa x", "a") == 0);
		K__Verify(KStringUtils::find("aaa x", "aaa") == 0);
		K__Verify(KStringUtils::find("aaa x", " ") == 3);
		K__Verify(KStringUtils::find("aa", "aaaa") == -1);

		K__Verify(KStringUtils::toInt("12") == 12);
		K__Verify(KStringUtils::toInt("+12") == 12);
		K__Verify(KStringUtils::toInt("-12") == -12);
		K__Verify(KStringUtils::toInt("012") == 10); // 先頭の0は8進数
		K__Verify(KStringUtils::toInt("0x12") == 18); // hex ok
		K__Verify(KStringUtils::toInt("0x012") == 18);
		K__Verify(KStringUtils::toInt("12.0") == 0); // FAIL
		K__Verify(KStringUtils::toInt("12.0f") == 0); // FAIL
		K__Verify(KStringUtils::toInt("0.12e2") == 0); // FAIL
		K__Verify(KStringUtils::toInt("0.12e-2") == 0); // FAIL
		K__Verify(KStringUtils::toInt("0x0FFFFFFF") == 0x0FFFFFFF);
		K__Verify(KStringUtils::toInt("0xFFFFFFFF") == 0x7FFFFFFF); // int32 max
		K__Verify(KStringUtils::toInt("") == 0); // FAIL
		K__Verify(KStringUtils::toInt("  12 ") == 0); // FAIL
		K__Verify(KStringUtils::toInt(" +12 ") == 0); // FAIL
		K__Verify(KStringUtils::toInt(" -12 ") == 0); // FAIL
		K__Verify(KStringUtils::toInt("- 12 ") == 0); // FAIL

		K__Verify(KStringUtils::toFloat("12") == 12);
		K__Verify(KStringUtils::toFloat("+12") == 12);
		K__Verify(KStringUtils::toFloat("-12") == -12);
		K__Verify(KStringUtils::toFloat("012") == 12); // strtod は先頭の 0 を無視する。8進数とは見なさない
		K__Verify(KStringUtils::toFloat("0x12") == 18); // hex ok
		K__Verify(KStringUtils::toFloat("0x012") == 18);
		K__Verify(KStringUtils::toFloat("12.5") == 12.5f);
		K__Verify(KStringUtils::toFloat("12.5f") == 0); // FAIL: No C-style suffix
		K__Verify(KStringUtils::toFloat("0.12e2") == 12);
		K__Verify(KStringUtils::toFloat("0.12e-2") == 0.0012f);
		K__Verify(KStringUtils::toFloat("0x0FFFFFFF") == 0x0FFFFFFF);
		K__Verify(KStringUtils::toFloat("0xFFFFFFFF") == 4.29496730e+09f); // 有効桁で切り捨てされる (正しい値は 4294967295)
		K__Verify(KStringUtils::toFloat("") == 0); // FAIL
		K__Verify(KStringUtils::toFloat("  12 ") == 0); // FAIL
		K__Verify(KStringUtils::toFloat(" +12 ") == 0); // FAIL
		K__Verify(KStringUtils::toFloat(" -12 ") == 0); // FAIL
		K__Verify(KStringUtils::toFloat("- 12 ") == 0); // FAIL

		K__Verify(KStringUtils::toUint("-12") == 0xFFFFFFF4); // 桁あふれ発生
		K__Verify(KStringUtils::toUint("12") == 12);
		K__Verify(KStringUtils::toUint("0x10") == 16);
		K__Verify(KStringUtils::toUint("") == 0); // FAIL
		K__Verify(KStringUtils::toUint("12.3") == 0); // FAIL
		K__Verify(KStringUtils::toUint("0xFFFFFFFF") == 0xFFFFFFFF);
	}
	{
		KStringView rest;
		auto tok = KStringView("aa,bb,cc,dd").split(",", true, true, 2, &rest);
		K__Verify(tok.size() == 2);
		K__Verify(tok[0].compare("aa") == 0);
		K__Verify(tok[1].compare("bb") == 0);
		K__Verify(rest.compare("cc,dd") == 0);
	}
	{
		KStringView rest;
		auto tok = KStringView("aa,,bb,,cc,,dd").split(",", true, true, 2, &rest);
		K__Verify(tok.size() == 2);
		K__Verify(tok[0].compare("aa") == 0);
		K__Verify(tok[1].compare("bb") == 0);
		K__Verify(rest.compare("cc,,dd") == 0);
	}
	{
		KStringView rest;
		auto tok = KStringView("aa,,bb,,cc,,dd").split(",", false, true, 4, &rest);
		K__Verify(tok.size() == 4);
		K__Verify(tok[0].compare("aa") == 0);
		K__Verify(tok[1].compare(""  ) == 0); // ".." の部分は . と . の間に空文字列が挟まっているとみなす
		K__Verify(tok[2].compare("bb") == 0);
		K__Verify(tok[3].compare(""  ) == 0);
		K__Verify(rest.compare("cc,,dd") == 0);
	}

	{
		KString a;
		K__Verify(a == "");
		K__Verify(a.empty());
		K__Verify(a.c_str());
		K__Verify(strlen(a.c_str()) == 0);

		KString b = "abc";
		K__Verify(b == "abc");
		K__Verify(b.empty() == false);
		K__Verify(b.subString(0, 0) == "");
		K__Verify(b.subString(1, 2) == "bc");
		K__Verify(b.subString(1, -1) == "bc");
		K__Verify(b.subString(0, -1) == "abc");
		K__Verify(b.subString(10, -1) == "");
		K__Verify(b.subString(-9, -1) == "");

		K__Verify(b.remove(0, 0) == b);
		K__Verify(b.remove(1, 2) == "a");
		K__Verify(b.remove(1, -1) == "a");
		K__Verify(b.remove(0, -1) == "");
		K__Verify(b.remove(10, -1) == "");
		K__Verify(b.remove(-9, -1) == "");

		K__Verify(b.toWide() == L"abc");
		K__Verify(b.replace(1, 0, "xx") == "axxbc");
		K__Verify(b.replace(1, 2, "xx") == "axx");
		K__Verify(b.replace(1, 2, "xxyy") == "axxyy");
		K__Verify(b.replace(1, 2, "") == "a");
		K__Verify(b.startsWith(""));
		K__Verify(b.startsWith("a"));
		K__Verify(b.startsWith("ab"));
		K__Verify(b.startsWith("abc"));
		K__Verify(b.endsWith("abc"));
		K__Verify(b.endsWith("bc"));
		K__Verify(b.endsWith("c"));
		K__Verify(b.endsWith(""));

		KString c = " xyz ";
		K__Verify(c.trim() == "xyz");
		K__Verify(c.findChar(' ') == 0);
		K__Verify(c.findChar('a') == -1);
		K__Verify(c.findChar(' ', 1) == 4);
		K__Verify(c.findChar(' ', 5) == -1);
		K__Verify(c.remove(0, 1) == "xyz ");
	}


	{
		KToken tok("aa,bb,cc,dd", "/", true, 3);
		K__Verify(tok.size() == 1);
		K__Verify(tok.compare(0, "aa,bb,cc,dd") == 0);
	}
	{
		KToken tok("aa,bb,cc,dd", ",", true, 3);
		K__Verify(tok.size() == 3);
		K__Verify(tok.compare(0, "aa") == 0);
		K__Verify(tok.compare(1, "bb") == 0);
		K__Verify(tok.compare(2, "cc,dd") == 0);
	}
	{
		KToken tok("aa,,bb,,cc,,dd", ",", true, 3);
		K__Verify(tok.size() == 3);
		K__Verify(tok.compare(0, "aa") == 0);
		K__Verify(tok.compare(1, "bb") == 0);
		K__Verify(tok.compare(2, "cc,,dd") == 0);
	}
	{
		KToken tok("aa,,bb,,cc,,dd", ",", false, 5);
		K__Verify(tok.size() == 5);
		K__Verify(tok.compare(0, "aa") == 0);
		K__Verify(tok.compare(1, ""  ) == 0); // ".." の部分は . と . の間に空文字列が挟まっているとみなす
		K__Verify(tok.compare(2, "bb") == 0);
		K__Verify(tok.compare(3, ""  ) == 0);
		K__Verify(tok.compare(4, "cc,,dd") == 0);
	}
}
} // Test
#pragma endregion // KStringUtils





#pragma region KPathUtils
// 末尾の区切り文字を取り除き、指定した区切り文字に変換した文字列を得る
void KPathUtils::K_PathNormalizeEx(char *path_u8, char old_delim, char new_delim) {
	K__Assert(path_u8);
	K__Assert(isprint(old_delim));
	K__Assert(isprint(new_delim));
	KStringUtils::trim(path_u8); // 前後の空白をスキップ
	KStringUtils::replaceChar(path_u8, old_delim, new_delim); // 区切り文字を置換
	K_PathRemoveLastDelim(path_u8); // 最後の区切り文字は削除する
}
void KPathUtils::K_PathNormalizeEx(char *out_path, int out_size, const char *path_u8, char old_delim, char new_delim) {
	strcpy_s(out_path, out_size, path_u8);
	K_PathNormalizeEx(out_path, old_delim, new_delim);
}

// 末尾の区切り文字を取り除き、適切な区切り文字に変換した文字列を得る
void KPathUtils::K_PathNormalize(char *path_u8) {
	K_PathNormalizeEx(path_u8, K__PATH_BACKSLASH, K__PATH_SLASH);
}
void KPathUtils::K_PathNormalize(char *out_path, int out_size, const char *path_u8) {
	K_PathNormalizeEx(out_path, out_size, path_u8, K__PATH_BACKSLASH, K__PATH_SLASH);
}

/// 2つのパスの先頭部分に含まれる共通のサブパスの文字列長さを得る（区切り文字を含む）
int KPathUtils::K_PathGetCommonSize(const char *pathA, const char *pathB) {
	K__Assert(pathA);
	K__Assert(pathB);
	int len = 0;
	for (int i=0; ; i++) {
		if (pathA[i] == pathB[i]) {
			if (pathA[i] == K__PATH_SLASH || pathA[i] == K__PATH_BACKSLASH) {
				len = i+1;
			}
			if (pathA[i] == '\0') {
				len = i;
				break;
			}
		} else {
			break;
		}
	}
	return len;
}

/// 完全なパスを得る
bool KPathUtils::K_PathGetFullPath(char *out_path, int out_size, const char *path_u8) {
	K__Assert(path_u8);
	wchar_t wpath[KPathUtils::MAX_SIZE] = {0};
	wchar_t wfull[KPathUtils::MAX_SIZE] = {0};
	K__Utf8ToWidePath(wpath, KPathUtils::MAX_SIZE, path_u8);
	if (_wfullpath(wfull, wpath, KPathUtils::MAX_SIZE)) {
		K__ReplaceW(wfull, K__PATH_BACKSLASH, K__PATH_SLASH);
		KStringUtils::wideToUtf8(out_path, out_size, wfull);
		return true;
	}
	return false;
}

/// base から path への相対パスを得る
void KPathUtils::K_PathGetRelative(char *out_path, int out_size, const char *path_u8, const char *base_u8) {
	K__Assert(out_path);
	K__Assert(path_u8);
	K__Assert(base_u8);
	// パス区切り文字で区切る
	auto tok_path = KStringView(path_u8).split("/\\", true, true, 0, nullptr);
	auto tok_base = KStringView(base_u8).split("/\\", true, true, 0, nullptr);
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
	out_path[0] = '\0';
	for (int i=c; i<numtok_base; i++) {
		KPathUtils::K_PathPushLast(out_path, out_size, "..");
	}

	// 登り切ったので、今度は目的地までパスを下げていく
	for (int i=c; i<numtok_path; i++) {
		int len = tok_path[i].size();
		char tmp[KPathUtils::MAX_SIZE] = {0};
		tok_path[i].getString(tmp, sizeof(tmp));
		KPathUtils::K_PathPushLast(out_path, out_size, tmp);
	}
}

/// パス末尾にあるサブパスの先頭部分を返す
/// パスが一つの要素からのみなる場合は path_u8 そのものを返す
/// それ以外の場合は末尾の '\0' へのポインタを返す
/// @code
/// "aaa/bbb" == >"bbb"
/// "aaa/bbb/" ==> ""
/// @endcode
const char * KPathUtils::K_PathGetLast(const char *path_u8) {
	return K_PathGetLast((char *)path_u8);
}
char * KPathUtils::K_PathGetLast(char *path_u8) {
	K__Assert(path_u8);
	char *sl = strrchr(path_u8, K__PATH_SLASH);
	char *bs = strrchr(path_u8, K__PATH_BACKSLASH);
	char *delim = (sl > bs) ? sl : bs; // max(sl, bs)
	return delim ? delim+1 : path_u8; // デリミタの次を指すように
}

/// パスの末尾が / で終わるようにする
void KPathUtils::K_PathAppendLastDelim(char *path_u8, int size) {
	K__Assert(path_u8);
	int len = strlen(path_u8);
	char *c = &path_u8[len-1];
	if (*c == K__PATH_BACKSLASH) {
		*c = K__PATH_SLASH;
	}
	if (*c != K__PATH_SLASH) {
		strcat_s(path_u8, size, "/");
	}
}
void KPathUtils::K_PathAppendLastDelim(char *out_path, int out_size, char *path) {
	strcpy_s(out_path, out_size, path);
	K_PathAppendLastDelim(out_path, out_size);
}

/// パスの末尾が / で終わっている場合、それを取り除く
void KPathUtils::K_PathRemoveLastDelim(char *path_u8) {
	K__Assert(path_u8);
	int len = strlen(path_u8);
	char *c = &path_u8[len-1];
	if (*c == K__PATH_BACKSLASH) {
		*c = '\0';
	}
	if (*c == K__PATH_SLASH) {
		*c = '\0';
	}
}
void KPathUtils::K_PathRemoveLastDelim(char *out_path, int out_size, const char *path) {
	strcpy_s(out_path, out_size, path);
	K_PathRemoveLastDelim(out_path);
}

/// パス末尾の拡張子部分を返す (ドットを含む)
/// 拡張子がない場合は末尾文字部分を返す
const char * KPathUtils::K_PathGetExt(const char *path_u8) {
	K__Assert(path_u8);
	return K_PathGetExt((char *)path_u8);
}

char * KPathUtils::K_PathGetExt(char *path_u8) {
	
	K__Assert(path_u8);
	// 末尾の "." を探す
	// 見つからなかったら末尾文字のポインタを返す
	char *p = strrchr(path_u8, '.');
	if (p == nullptr) {
		return strrchr(path_u8, '\0');
	}

	// "." よりも後にパス区切り文字 "/" があったらダメ (例: "aaa.zip/ccc")
	// それは拡張子とはみなさない
	if (strchr(p, K__PATH_SLASH)) {
		return strrchr(path_u8, '\0');
	}

	// "." よりも後にパス区切り文字 "\\" があったらダメ (例: "aaa.zip/ccc")
	// それは拡張子とはみなさない
	if (strchr(p, K__PATH_BACKSLASH)) {
		return strrchr(path_u8, '\0');
	}
	return p;
}

/// パスの末尾にサブパスを追加する
void KPathUtils::K_PathPushLast(char *path_u8, int size, const char *more_u8) {
	K__Assert(path_u8);
	K__Assert(more_u8);
	if (KStringUtils::isEmpty(path_u8)) {
		// "" + "morepath" ==> "morepath"
		strcpy_s(path_u8, size, more_u8);
		K_PathRemoveLastDelim(path_u8);
		return;
	}
	if (KStringUtils::isEmpty(more_u8)) {
		// "pathstring" + "" ==> "pathstring"
		return;
	}
	K_PathAppendLastDelim(path_u8, size); // "path" ==> "path/"
	strcat_s(path_u8, size, more_u8);  // "path/" + "more" ==> "path/more"
	K_PathRemoveLastDelim(path_u8); // "path/more/" ==> "path/more"
}
void KPathUtils::K_PathPushLast(char *out_path, int out_size, const char *path, const char *more) {
	strcat_s(out_path, out_size, path);
	K_PathPushLast(out_path, out_size, more);
}

/// パスの末尾に拡張子を追加する
void KPathUtils::K_PathPushExt(char *path_u8, int size, const char *ext_u8) {
	K__Assert(path_u8);
	K__Assert(ext_u8);
	if (ext_u8[0] != '.') {
		strcat_s(path_u8, size, ".");
	}
	strcat_s(path_u8, size, ext_u8);
}
void KPathUtils::K_PathPushExt(char *out_path, int out_size, const char *path, const char *ext) {
	strcpy_s(out_path, out_size, path);
	K_PathPushExt(out_path, out_size, ext);
}

/// パス末尾にあるサブパスを取り除く
/// path_u8 の末尾にあるデリミタをヌル文字に書き換える。
/// 取り除いた末尾部分の先頭を返す
void KPathUtils::K_PathPopLast(char *path_u8) {
	K__Assert(path_u8);
	// K_PathGetLast はデリミタまたは末尾文字のアドレスを返すので、
	// それをそのまま '\0' に書き換えるだけでよい
	char *s = K_PathGetLast(path_u8);
	K__Assert(s);
	s[0] = '\0';
	K_PathRemoveLastDelim(path_u8);
}
void KPathUtils::K_PathPopLast(char *out_path, int out_size, const char *path) {
	strcpy_s(out_path, out_size, path);
	K_PathPopLast(out_path);
}

/// パス末尾にある拡張子を取り除く
void KPathUtils::K_PathPopExt(char *path_u8) {
	K__Assert(path_u8);
	// K_PathGetExt は ドットまたは末尾文字のアドレスを返すので、
	// それをそのまま '\0' に書き換えるだけでよい
	char *s = K_PathGetExt(path_u8);
	K__Assert(s);
	s[0] = '\0';
}
void KPathUtils::K_PathPopExt(char *out_path, int out_size, const char *path) {
	strcpy_s(out_path, out_size, path);
	K_PathPopExt(out_path);
}

/// パス区切り文字を含んでいる場合は true
bool KPathUtils::K_PathHasDelim(const char *path_u8) {
	return strchr(path_u8, '/') != nullptr;
}

/// パスが実在し、かつディレクトリかどうか調べる
bool KPathUtils::K_PathIsDir(const char *path_u8) {
	K__Assert(path_u8);
	wchar_t wpath[MAX_SIZE] = {0};
	K__Utf8ToWidePath(wpath, MAX_SIZE, path_u8);
	return PathIsDirectoryW(wpath);
}

/// パスが実在し、かつ非ディレクトリかどうか調べる
bool KPathUtils::K_PathIsFile(const char *path_u8) {
	K__Assert(path_u8);
	wchar_t wpath[MAX_SIZE] = {0};
	K__Utf8ToWidePath(wpath, MAX_SIZE, path_u8);
	return PathFileExistsW(wpath) && !PathIsDirectoryW(wpath); // パスが存在し、かつディレクトリでないならファイルであるとする
}

/// パスが実在するか調べる
bool KPathUtils::K_PathExists(const char *path_u8) {
	K__Assert(path_u8);
	wchar_t wpath[MAX_SIZE] = {0};
	K__Utf8ToWidePath(wpath, MAX_SIZE, path_u8);
	return PathFileExistsW(wpath);
}

/// パスで指定されたファイルのバイト数を得る
/// サイズを取得できない場合は false を返す
bool KPathUtils::K_PathGetFileSize(const char *path_u8, int *out_size) {
	K__Assert(path_u8);
	wchar_t wpath[MAX_SIZE] = {0};
	K__Utf8ToWidePath(wpath, MAX_SIZE, path_u8);
	//
	HANDLE hFile = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
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
static time_t K__FileTimeToTimeT(const FILETIME *ft) {
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
bool KPathUtils::K_PathGetTimeStamp(const char *path_u8, time_t *out_time_cma) {
	K__Assert(path_u8);
	wchar_t wpath[MAX_SIZE] = {0};
	K__Utf8ToWidePath(wpath, MAX_SIZE, path_u8);
	//
	HANDLE hFile = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile != INVALID_HANDLE_VALUE) {
		FILETIME ctm, mtm, atm;
		GetFileTime(hFile, &ctm, &atm, &mtm);
		CloseHandle(hFile);
		if (out_time_cma) {
			out_time_cma[0] = K__FileTimeToTimeT(&ctm); // creation
			out_time_cma[1] = K__FileTimeToTimeT(&mtm); // modify
			out_time_cma[2] = K__FileTimeToTimeT(&atm); // access
		}
		return true;
	}
	return false;
}

/// パス同士を比較する
int KPathUtils::K_PathCompare(const char *path1, const char *path2, bool ignore_case, bool ignore_path) {
	return K::pathCompare(path1, path2, ignore_case, ignore_path);
}

/// パスの末尾部分を last と比較する
int KPathUtils::K_PathCompareLast(const char *path, const char *last) {
	return strcmp(K_PathGetLast(path), last);
}

/// パスの拡張子を ext と比較する
int KPathUtils::K_PathCompareExt(const char *path, const char *ext) {
	return strcmp(K_PathGetExt(path), ext);
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
bool KPathUtils::K_PathGlob(const char *path, const char *glob) {
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
				if (K_PathGlob(subpath, subglob)) {
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

namespace Test {
void Test_path() {
	{
		KName e;
		KName a("aaa");
		KName A("aaa");
		KName b("bbb");
		KName c("ccc");
		K__Verify(e.empty());
		K__Verify(strcmp(a.c_str(), "aaa") == 0);
		K__Verify(strcmp(b.c_str(), "bbb") == 0);
		K__Verify(strcmp(c.c_str(), "ccc") == 0);
		K__Verify(e != a);
		K__Verify(a == a);
		K__Verify(a == A);
		K__Verify(b != c);
	}
	{
		char s[256] = {0};
		KPathUtils::K_PathPushLast(s, sizeof(s), "aaa"); K__Verify(strcmp(s, "aaa") == 0);
		KPathUtils::K_PathPushLast(s, sizeof(s), "bbb"); K__Verify(strcmp(s, "aaa/bbb") == 0);
		KPathUtils::K_PathPushLast(s, sizeof(s), "ccc"); K__Verify(strcmp(s, "aaa/bbb/ccc") == 0);
		KPathUtils::K_PathPopLast(s); K__Verify(strcmp(s, "aaa/bbb") == 0);
		KPathUtils::K_PathPushExt(s, sizeof(s), ".exe"); K__Verify(strcmp(s, "aaa/bbb.exe")==0);
		KPathUtils::K_PathPopExt(s); K__Verify(strcmp(s, "aaa/bbb")==0);
		KPathUtils::K_PathPopExt(s); K__Verify(strcmp(s, "aaa/bbb")==0); // 変化なし

		strcpy_s(s, sizeof(s), "bbb/aaa.exe");
		K__Verify(strcmp(KPathUtils::K_PathGetExt(s), ".exe") == 0);

		strcpy_s(s, sizeof(s), "bbb/aaa.exe.zip");
		K__Verify(strcmp(KPathUtils::K_PathGetExt(s), ".zip") == 0);

		strcpy_s(s, sizeof(s), "bbb/aaa.zip/ccc.bmp");
		K__Verify(strcmp(KPathUtils::K_PathGetExt(s), ".bmp") == 0);

		strcpy_s(s, sizeof(s), "bbb/aaa.zip/ccc");
		K__Verify(KStringUtils::isEmpty(KPathUtils::K_PathGetExt(s)));

		strcpy_s(s, sizeof(s), "bbb/aaa");
		K__Verify(KStringUtils::isEmpty(KPathUtils::K_PathGetExt(s)));

		strcpy_s(s, sizeof(s), "bbb/aaa.exe");
		K__Verify(strcmp(KPathUtils::K_PathGetLast(s), "aaa.exe") == 0);

		strcpy_s(s, sizeof(s), "aaa.exe");
		K__Verify(strcmp(KPathUtils::K_PathGetLast(s), "aaa.exe") == 0);

		strcpy_s(s, sizeof(s), "");
		KPathUtils::K_PathPushLast(s, sizeof(s), "aaa/"); K__Verify(strcmp(s, "aaa") == 0); // 末尾の区切りは消える
		
		strcpy_s(s, sizeof(s), "aaa/");
		KPathUtils::K_PathPushLast(s, sizeof(s), "bbb"); K__Verify(strcmp(s, "aaa/bbb") == 0); // aaa//bbb にはならない
	}
	{
		K__Verify(KPathUtils::K_PathGetCommonSize("aaa/bbb/ccc", "aaa/bbb/ccc") == 11);
		K__Verify(KPathUtils::K_PathGetCommonSize("aaa/bbb/ccc", "aaa/bbb/ddd") == 8);
		K__Verify(KPathUtils::K_PathGetCommonSize("aaa/bbb/ccc", "aaa/bbb_ccc") == 4);
		K__Verify(KPathUtils::K_PathGetCommonSize("aaa", "") == 0);
		K__Verify(KPathUtils::K_PathGetCommonSize("", "") == 0);
	}
	{
		char s[256];
		KPathUtils::K_PathGetRelative(s, sizeof(s), "aa/bb/cc", "aa");        K__Verify(strcmp(s, "bb/cc") == 0);
		KPathUtils::K_PathGetRelative(s, sizeof(s), "aa/bb/cc", "aa/bb");     K__Verify(strcmp(s, "cc") == 0);
		KPathUtils::K_PathGetRelative(s, sizeof(s), "aa/bb/cc", "aa/bb/ee");  K__Verify(strcmp(s, "../cc") == 0);
		KPathUtils::K_PathGetRelative(s, sizeof(s), "aa/bb/cc", "ee/ff");     K__Verify(strcmp(s, "../../aa/bb/cc") == 0);
		KPathUtils::K_PathGetRelative(s, sizeof(s), "", "aa");  K__Verify(strcmp(s, "..") == 0);
		KPathUtils::K_PathGetRelative(s, sizeof(s), "aa", "");  K__Verify(strcmp(s, "aa") == 0);
		KPathUtils::K_PathGetRelative(s, sizeof(s), "", "");    K__Verify(strcmp(s, "") == 0);
	}
	{
		K__Verify(KPathUtils::K_PathGlob("abc", "*") == true);
		K__Verify(KPathUtils::K_PathGlob("abc", "a*") == true);
		K__Verify(KPathUtils::K_PathGlob("abc", "ab*") == true);
		K__Verify(KPathUtils::K_PathGlob("abc", "abc*") == true);
		K__Verify(KPathUtils::K_PathGlob("abc", "a*c") == true);
		K__Verify(KPathUtils::K_PathGlob("abc", "*abc") == true);
		K__Verify(KPathUtils::K_PathGlob("abc", "*bc") == true);
		K__Verify(KPathUtils::K_PathGlob("abc", "*c") == true);
		K__Verify(KPathUtils::K_PathGlob("abc", "*a*b*c*") == true);
		K__Verify(KPathUtils::K_PathGlob("abc", "*bc*") == true);
		K__Verify(KPathUtils::K_PathGlob("abc", "*c*") == true);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb.ext", "a*.ext") == false); // ワイルドカード * はパス区切り文字を含まない
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb.ext", "a*/*.ext") == true);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb.ext", "a*/*.*t") == true);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb.ext", "aaa/*.ext") == true);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb.ext", "aaa/bbb*ext") == true);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb.ext", "aaa*bbb*ext") == false);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb/ccc.ext", "aaa/*/ccc.ext") == true);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb/ccc.ext", "aaa/*.ext") == false);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb.ext", "*.aaa") == false);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb.ext", "aaa*bbb") == false);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb.ext", "*/*.ext") == true);
		K__Verify(KPathUtils::K_PathGlob("aaa/bbb.ext", "*/*.*") == true);
	}
}
} // Test
#pragma endregion // KPathUtils





#pragma region CNameStringTable
#if 1
class CNameStringTable {
	static constexpr uint32_t TABLESIZE = 1 << 10;
	static constexpr uint32_t TABLEMASK = TABLESIZE - 1;// TABLESIZE must be 2^n
	struct DATA {
		std::string str;
		uint32_t hash;
		DATA *next;
		DATA *prev;
		
		DATA() {
			hash = 0;
			next = nullptr;
			prev = nullptr;
		}
	};
	DATA *mTable[TABLESIZE];
public:
	CNameStringTable() {
		memset(mTable, 0, sizeof(mTable));
	}
	~CNameStringTable() {
		clear();
	}
	void clear() {
		for (int i=0; i<TABLESIZE; i++) {
			if (mTable[i]) {
				DATA *data = mTable[i];
				while (data) {
					DATA *tmp = data->next;
					delete data;
					data = tmp;
				}
				mTable[i] = nullptr;
			}
		}
	}
	const char * contains(const char *s) const {
		uint32_t hash = KStringUtils::gethash(s);
		uint32_t index = hash & TABLEMASK;
		DATA *data = mTable[index];
		while (data && data->hash != hash) {
			data = data->next;
		}
		return data ? data->str.c_str() : nullptr;
	}
	const char * insert(const char *s) {
		return find_or_add(s)->str.c_str();
	}

private:
	DATA * find_or_add(const char *s) {
		uint32_t hash = KStringUtils::gethash(s);
		uint32_t index = hash & TABLEMASK;

		DATA *data = mTable[index];
		DATA *last = data;
		while (data && data->hash != hash) {
			last = data;
			data = data->next;
		}
		if (data) {
			return data; // found!
		} else {
			DATA *newdata = new DATA();
			newdata->hash = hash;
			newdata->str = s;
			newdata->next = nullptr;
			if (last) {
				last->next = newdata;
				newdata->prev = last;
			} else {
				mTable[index] = newdata;
			}
			return newdata;
		}
	}
};
#else
class CNameStringTable {
	std::unordered_set<std::string> mTable;
public:
	void clear() {
		mTable.clear();
	}
	const char * contains(const char *s) const {
		K_assert(s);
		auto it = mTable.find(s);
		return (it != mTable.end()) ? it->c_str() : nullptr;
	}
	const char * insert(const char *s) {
		K_assert(s);
		auto it = mTable.insert(s);
		return it.first->c_str();
	}
};
#endif
#pragma endregion // CNameStringTable









#pragma region KName
static CNameStringTable & KName_GetTable() {
	static CNameStringTable sTable;
	return sTable;
}

#if NAME_PTR
KName::KName() {
	m_ptr = nullptr;
}
KName::KName(const char *name) {
	if (name && name[0]) {
		CNameStringTable &table = KName_GetTable();
		m_ptr = table.insert(name ? name : "");
	} else {
		m_ptr = nullptr; // for empty string
	}
}
KName::KName(const std::string &name) {
	if (!name.empty()) {
		CNameStringTable &table = KName_GetTable();
		m_ptr = table.insert(name.c_str());
	} else {
		m_ptr = nullptr; // for empty string
	}
}
KName::KName(const KName &name) {
	m_ptr = name.m_ptr;
}
KName::~KName() {
}
const char * KName::c_str() const {
	return m_ptr ? m_ptr : "";
}
bool KName::empty() const {
	return m_ptr==nullptr || m_ptr[0]=='\0';
}
size_t KName::hash() const {
	return (size_t)m_ptr;
}
bool KName::operator == (const KName &name) const {
	return m_ptr == name.m_ptr;
}
bool KName::operator < (const KName &name) const {
	return m_ptr < name.m_ptr; // アルファベット順ではなく、アドレスの大小関係であることに注意せよ
}
#else
static const char *s_empty_cstring = "";

KName::KName() {
	m_str = nullptr;
	m_hash = -1;
}
KName::KName(const char *name) {
	if (name && name[0]) {
		m_str = (char *)malloc(strlen(name) + 1);
		strcpy(m_str, name);
	} else {
		m_str = (char*)s_empty_cstring;
	}
	m_hash = -1;
}
KName::KName(const std::string &name) {
	if (!name.empty()) {
		m_str = (char *)malloc(name.size() + 1);
		strcpy(m_str, name.c_str());
	} else {
		m_str = (char*)s_empty_cstring;
	}
	m_hash = -1;
}
KName::KName(const KName &name) {
	if (!name.empty()) {
		m_str = (char *)malloc(strlen(name.c_str()) + 1);
		strcpy(m_str, name.c_str());
	} else {
		m_str = (char*)s_empty_cstring;
	}
	m_hash = name.m_hash;
}
KName::~KName() {
	if (m_str != s_empty_cstring) {
		free(m_str);
	}
}
const char * KName::c_str() const {
	return m_str;
}
bool KName::empty() const {
	return m_str != s_empty_cstring;
}
size_t KName::hash() const {
	if (m_hash == -1 && m_str && m_str[0]) {
		m_hash = K__ComputeCrc32String(m_str);
	}
	return m_hash;
}
bool KName::operator == (const KName &name) const {
	return hash() == name.hash() && strcmp(m_str, name.m_str);
}
bool KName::operator < (const KName &name) const {
	return hash() < name.hash(); // アルファベット順ではなく、アドレスの大小関係であることに注意せよ
}
#endif

bool KName::operator == (const char *name) const {
	return *this == KName(name);
}
bool KName::operator == (const std::string &name) const {
	return *this == KName(name);
}
bool KName::operator != (const char *name) const {
	return !(*this==name);
}
bool KName::operator != (const std::string &name) const {
	return !(*this==name);
}
bool KName::operator != (const KName &name) const {
	return !(*this==name);
}
bool KName::operator < (const char *name) const {
	return *this < KName(name);
}
bool KName::operator < (const std::string &name) const {
	return *this < KName(name);
}
#pragma endregion // KName





#pragma region KPath func
static void kkpath_strcpy(char *dst, size_t dstlen, const char *src) {
	strcpy_s(dst, dstlen, src);
}
static void kkpath_strcat(char *dst, size_t dstlen, const char *src) {
	strcat_s(dst, dstlen, src);
}
static void kkpath_replace_char(char *s, char before, char after) {
	KStringUtils::replaceChar(s, before, after);
}

// 末尾の区切り文字を取り除き、適切な区切り文字に変換した文字列を得る
static void kkpath_normalize(char *dest, const char *path, int maxlen, char olddelim, char newdelim) {
	KPathUtils::K_PathNormalizeEx(dest, maxlen, path, olddelim, newdelim);
}

#pragma endregion // KPath func


#pragma region KPath
const KPath KPath::Empty = KPath();


KPath KPath::fromAnsi(const char *mb, const char *locale) {
	K__Assert(mb);
	K__Assert(locale);
	return KPath(KStringUtils::ansiToUtf8(mb, locale).c_str());
}
KPath KPath::fromUtf8(const char *u8) {
	return KPath(u8 ? u8 : "");
}
KPath KPath::fromFormat(const char *fmt, ...) {
	char tmp[SIZE];
	va_list args;
	va_start(args, fmt);
	vsprintf_s(tmp, SIZE, fmt, args);
	va_end(args);
	return KPath(tmp);
}
KPath::KPath() {
	clear();
}
KPath::KPath(const char *u8) {
	if (u8 && u8[0]) {
		u8 = KStringUtils::skipUtf8Bom(u8); // BOMをスキップ
		kkpath_normalize(m_path, u8, KPath::SIZE, NATIVE_DELIM, K__PATH_SLASH);
	} else {
		m_path[0] = 0;
	}
	m_hash = (uint32_t)(-1);
}
KPath::KPath(const std::string &u8) {
	if (!u8.empty()) {
		kkpath_normalize(m_path, u8.c_str(), KPath::SIZE, NATIVE_DELIM, K__PATH_SLASH);
	} else {
		m_path[0] = 0;
	}
	m_hash = (uint32_t)(-1);
}
KPath::KPath(const wchar_t *ws) {
	if (ws && ws[0]) {
		std::string mb = KStringUtils::wideToUtf8(ws);
		kkpath_normalize(m_path, mb.c_str(), KPath::SIZE, NATIVE_DELIM, K__PATH_SLASH);
	} else {
		m_path[0] = 0;
	}
	m_hash = (uint32_t)(-1);
}
KPath::KPath(const std::wstring &ws) {
	if (!ws.empty()) {
		std::string mb = KStringUtils::wideToUtf8(ws);
		kkpath_normalize(m_path, mb.c_str(), KPath::SIZE, NATIVE_DELIM, K__PATH_SLASH);
	} else {
		m_path[0] = 0;
	}
	m_hash = (uint32_t)(-1);
}
KPath::KPath(const KPath &other) {
	memcpy(m_path, other.m_path, sizeof(m_path));
	m_hash = (uint32_t)(-1);
}
void KPath::clear() {
	m_path[0] = 0;
	m_hash = (uint32_t)(-1);
}
bool KPath::empty() const {
	return m_path[0] == 0;
}
size_t KPath::size() const {
	return strlen(m_path);
}
size_t KPath::hash() const {
	if (m_hash == (uint32_t)(-1)) {
		m_hash = std::hash<std::string>()(m_path);
	}
	return m_hash;
}
void KPath::getNativeWideString(wchar_t *out, size_t max_wchars) const {
	std::wstring ws = toWideString(NATIVE_DELIM_W);
	wcscpy_s(out, max_wchars, ws.c_str());
}
void KPath::getNativeAnsiString(char *out, size_t maxsize) const {
	std::string s = toAnsiString(NATIVE_DELIM, "");
	strcpy_s(out, maxsize, s.c_str());
}
KPath KPath::directory() const {
#if 1
	char tmp[SIZE] = {0};
	KPathUtils::K_PathPopLast(tmp, SIZE, m_path);
	return KPath(tmp);
#else
	char tmp[SIZE] = {0};
	kkpath_strcpy(tmp, sizeof(tmp), m_path);
	char *delim = strrchr(tmp, K__PATH_SLASH);
	if (delim) {
		// パス区切りが見つかった。それよりも前の部分を返す
		*delim = '\0';
		return KPath(tmp);
	} else {
		// パス区切り無し。ディレクトリ部は存在しないので空のパスを返す
		return KPath::Empty;
	}
#endif
}
bool KPath::hasDirectory() const {
	// パス区切りがあればディレクトリ名を含んでいるものとする
	return strrchr(m_path, K__PATH_SLASH) != nullptr;
}
KPath KPath::filename() const {
	const char *s = strrchr(m_path, K__PATH_SLASH);
	if (s) {
		// パス区切りあり。それよりも後ろを返す
		return KPath(s+1);
	} else {
		// パス区切り無し。そのまま返す
		return *this;
	}
}
KPath KPath::extension(bool strip_period) const {
	const char *s = strrchr(m_path, '.');
	if (s) {
		if (strip_period && s[0] == '.') {
			return KPath(s + 1);
		} else {
			return KPath(s);
		}
	} 
	return KPath::Empty;
}
KPath KPath::joinString(const KPath &s) const {
	return joinString(s.m_path);
}
KPath KPath::joinString(const char *s) const {
#if 1
	char tmp[SIZE] = {0};
	strcpy_s(tmp, SIZE, m_path);
	strcat_s(tmp, SIZE, s);
	return KPath(tmp);
#else
	KPath ret;
	size_t a = strlen(m_path);
	size_t b = strlen(s);
	if (a + b + 1 >= SIZE) {
		TOO_LONG_PATH_ERROR();
		return KPath::Empty;
	}
	kkpath_strcpy(ret.m_path, sizeof(ret.m_path), m_path);
	kkpath_strcat(ret.m_path, sizeof(ret.m_path), s);
	return ret;
#endif
}
size_t KPath::split(KPathList *list) const {
	if (list) list->clear();
	size_t num = 0;
	std::string tmp;
	for (size_t i=0; i<strlen(m_path); i++) {
		if (m_path[i] == K__PATH_SLASH) {
			if (list) list->push_back(tmp);
			num++;
			tmp.clear();
		} else {
			tmp.push_back(m_path[i]);
		}
	}
	if (!tmp.empty()) {
		if (list) list->push_back(tmp);
		num++;
	}
	return num;
}
KPath KPath::getCommonPath(const KPath &other) const {
	KPath ret;
	KPathList list;
	if (getCommonPath(other, &list)) {
		ret.join(list);
	}
	return ret;
}
int KPath::getCommonPath(const KPath &other, KPathList *list) const {
	KPathList common;

	// パス区切り文字で区切る
	KPathList tok_this;
	KPathList tok_other;
	this->split(&tok_this);
	other.split(&tok_other);

	// 先頭の共通パスを得る
	size_t i = 0;
	while (1) {
		if (i >= tok_this.size()) break;
		if (i >= tok_other.size()) break;
		if (tok_this[i] != tok_other[i]) break;
		common.push_back(tok_this[i]);
		i++;
	}

	if (list) *list = common;
	return common.size();
}

KPath KPath::getRelativePathFrom(const KPath &other) const {
	// パス区切り文字で区切る
	KPathList tok_this;
	KPathList tok_other;
	this->split(&tok_this);
	other.split(&tok_other);

	// 先頭の共通パス部分を削除する
	while (1) {
		if (tok_this.empty()) break;
		if (tok_other.empty()) break;
		if (tok_this[0] != tok_other[0]) break;
		tok_this.erase(tok_this.begin());
		tok_other.erase(tok_other.begin());
	}

	// 必要な数だけ ".." で上に行く
	KPath rel;
	for (size_t i=0; i<tok_other.size(); i++) {
		rel = rel.join("..");
	}

	// 登り切ったので、今度は目的地までパスを下げていく
	for (size_t i=0; i<tok_this.size(); i++) {
		rel = rel.join(tok_this[i]);
	}
	return rel;
}
KPath KPath::join(const KPath &more) const {
	if (m_path[0] == '\0') {
		return more;
	}
	if (more.m_path[0] == '\0') {
		return *this;
	}
	char tmp[SIZE] = {'\0'};
	kkpath_strcpy(tmp, sizeof(tmp), m_path);

	size_t n = strlen(m_path);
	strncpy_s(tmp, SIZE, m_path, n);
	tmp[n] = K__PATH_SLASH;
	int offset = n + 1;
	kkpath_strcpy(tmp + offset, sizeof(tmp) - offset, more.m_path);
	return KPath(tmp);
}
KPath KPath::join(const KPathList &more) const {
	KPath ret;
	for (auto it=more.begin(); it!=more.end(); ++it) {
		ret = ret.join(*it);
	}
	return ret;
}


KPath KPath::changeExtension(const KPath &ext) const {
	return changeExtension(ext.m_path);
}
KPath KPath::changeExtension(const char *ext) const {
	K__Assert(ext);
	char tmp[SIZE];
	kkpath_strcpy(tmp, sizeof(tmp), m_path);
	if (ext[0] == '\0') {
		// ext が空文字列の場合は、単に拡張子を削除する
		char *s = strrchr(tmp, '.');
		if (s) *s = '\0';

	} else if (ext[0] == '.') {
		// ext がドットで始まっている場合
		char *s = strrchr(tmp, '.');
		if (s) {
			int offset = (int)(s - tmp);
			kkpath_strcpy(s, sizeof(tmp) - offset, ext);
		} else {
			kkpath_strcat(tmp, sizeof(tmp), ext);
		}

	} else {
		// ext がドットで始まっていない場合
		char *s = strrchr(tmp, '.');
		if (s) *s = '\0';
		kkpath_strcat(tmp, sizeof(tmp), ".");
		kkpath_strcat(tmp, sizeof(tmp), ext);
	}
	return KPath(tmp);
}
bool KPath::hasExtension(const KPath &ext) const {
	return extension().compare(ext) == 0;
}
bool KPath::hasExtension(const char *ext) const {
	return extension().compare_str(ext) == 0;
}
bool KPath::endsWithPath(const KPath &sub) const {
	if (sub.m_path[0] == '\0') return true; // String::endsWith と同じ挙動。空文字列εは全ての文字列末尾にマッチする
	size_t sublen = strlen(sub.m_path);
	size_t thislen = strlen(m_path);
	if (thislen < sublen) return false;
	int idx = (int)(thislen - sublen);
	K__Assert(0 <= idx && idx < SIZE);
	if (idx > 0 && m_path[idx-1] != K__PATH_SLASH) return false; // パス区切り単位でないとダメ
	if (strcmp(m_path + idx, sub.m_path) != 0) return false;
	return true;
}
bool KPath::startsWithPath(const KPath &sub) const {
	if (sub.m_path[0] == '\0') return true; // String::startsWith と同じ挙動。空文字列εは全ての文字列先頭にマッチする
	size_t sublen = strlen(sub.m_path);
	size_t thislen = strlen(m_path);
	if (thislen < sublen) return false;
	char c = m_path[sublen];
	if (c != '\0' && c != K__PATH_SLASH) return false; // パス区切り単位でないとダメ
	if (strncmp(m_path, sub.m_path, sublen) != 0) return false;
	return true;
}
bool KPath::startsWithString(const char *sub) const {
	if (sub == nullptr) return true; // nullptr の場合は空文字列とみなす。空文字列は常に文字列先頭にマッチする
	size_t sublen = strlen(sub);
	size_t thislen = strlen(m_path);
	if (thislen < sublen) return false; // sub文字列の方が長い場合は決してマッチしない
	return strncmp(m_path, sub, sublen) == 0;
}
bool KPath::endsWithString(const char *sub) const {
	if (sub == nullptr) return true; // nullptr の場合は空文字列とみなす。空文字列は常に文字列末尾にマッチする
	size_t sublen = strlen(sub);
	size_t thislen = strlen(m_path);
	if (thislen < sublen) return false; // sub文字列の方が長い場合は決してマッチしない
	return strncmp(m_path, sub, sublen) == 0;
}
bool KPath::operator ==(const KPath &p) const {
	return compare(p) == 0;
}
bool KPath::operator !=(const KPath &p) const {
	return compare(p) != 0;
}
bool KPath::operator < (const KPath &p) const {
	return compare(p) < 0;
}
int KPath::compare_str(const char *p) const {
	K__Assert(p);
	return strcmp(m_path, p);
}
int KPath::compare_str(const char *p, bool ignore_case, bool ignore_path) const {
	K__Assert(p);
	if (ignore_path) {
		const char *s1 = strrchr(m_path, K__PATH_SLASH);
		const char *s2 = strrchr(p,     K__PATH_SLASH);
		if (s1) { s1++; /* K__PATH_SLASHの次の文字へ */ } else { s1 = m_path; }
		if (s2) { s2++; /* K__PATH_SLASHの次の文字へ */ } else { s2 = p; }
		return ignore_case ? K__stricmp(s1, s2) : strcmp(s1, s2);
	
	} else {
		const char *s1 = m_path;
		const char *s2 = p;
		return ignore_case ? K__stricmp(s1, s2) : strcmp(s1, s2);
	}
}
int KPath::compare(const KPath &p) const {
	return compare_str(p.m_path);
}
int KPath::compare(const KPath &p, bool ignore_case, bool ignore_path) const {
	return compare_str(p.m_path, ignore_case, ignore_path);
}
bool KPath::glob(const KPath &pattern) const {
	return KPathUtils::K_PathGlob(m_path, pattern.m_path);
}
const char * KPath::c_str() const {
	return m_path;
}
const char * KPath::u8() const {
	return m_path;
}
char * KPath::ptr() {
	return m_path;
}
KPath KPath::getFullPath() const {
	std::wstring ws = toWideString(NATIVE_DELIM);
	wchar_t wout[SIZE] = {0};
	GetFullPathNameW(ws.c_str(), SIZE, wout, nullptr);
	return KPath(wout);
}
bool KPath::isRelative() const {
	std::wstring ws = toWideString(NATIVE_DELIM);
	return PathIsRelativeW(ws.c_str());
}
std::wstring KPath::toWideString(char sep) const {
	char tmp[SIZE];
	kkpath_strcpy(tmp, sizeof(tmp), m_path);
	kkpath_replace_char(tmp, K__PATH_SLASH, sep);
	return KStringUtils::utf8ToWide(tmp);
}
std::string KPath::toAnsiString(char sep, const char *_locale) const {
	K__Assert(_locale);
	char tmp[SIZE];
	kkpath_strcpy(tmp, sizeof(tmp), m_path);
	kkpath_replace_char(tmp, K__PATH_SLASH, sep);
	return KStringUtils::utf8ToAnsi(tmp, _locale);
}
std::string KPath::toUtf8(char sep) const {
	std::string u8 = m_path;

	// 区切り文字を置換
	for (size_t i=0; u8[i]; i++) {
		if (u8[i] == K__PATH_SLASH) {
			u8[i] = sep;
		}
	}
	return u8;
}
#pragma endregion // KPath


namespace Test {
void Test_pathstring() {
	// なんとcrc32が同値になる (crc32b の場合）
	//K__Assert(KPath("girl/186/05(cn).sprite").hash() == KPath("effect/fire_wide_05.mesh").hash());
	{
		K__Assert(KPath(".").compare(".") == 0); // 特殊な文字が省略されない
		K__Assert(KPath("..").compare("..") == 0); // 特殊な文字が省略されない
		K__Assert(KPath("../a").compare("../a") == 0); // 特殊な文字が省略されない
		K__Assert(KPath("aaa/..").join("bbb").compare("aaa/../bbb") == 0); // 相対パス指定でも素直にパス結合できる
		K__Assert(KPath("..").join("aaa").compare("../aaa") == 0); // 相対パス指定でも素直にパス結合できる
		K__Assert(KPath("..").join("..").compare("../..") == 0); // 相対パス指定でも素直にパス結合できる
		K__Assert(KPath("AAA").compare_str("AAA") == 0);
		K__Assert(KPath("AAA\\bbb").compare_str("AAA/bbb") == 0); // compare_str は内部の文字列と直接比較する。内部ではデリミタが変換されているので注意
		K__Assert(KPath("aaa/bbb").compare_str("bbb", false, true) == 0); // パスを無視して比較
		K__Assert(KPath("aaa/bbb").compare_str("BBB", true, true) == 0); // 大小文字とパスを無視して比較
		K__Assert(KPath("").directory() == "");
		K__Assert(KPath("").filename() == "");
		K__Assert(KPath("").extension() == "");
		K__Assert(KPath("").isRelative());
		K__Assert(KPath("aaa").isRelative());
		K__Assert(KPath("../aaa").isRelative());
		K__Assert(!KPath("c:").isRelative());
		K__Assert(!KPath("c:\\aaa").isRelative());

		K__Assert(KPath("").join("") == "");
		K__Assert(KPath("").join("aaa") == "aaa");
		K__Assert(KPath("aaa").join("bbb") == "aaa/bbb");
		K__Assert(KPath("aaa").join("") == "aaa");

		K__Assert(KPath("").directory() == "");
		K__Assert(KPath("aaa.txt").directory() == "");
		K__Assert(KPath("aaa\\bbb.txt").directory() == "aaa");
		K__Assert(KPath("aaa\\bbb\\ccc.txt").directory() == "aaa/bbb");

		K__Assert(KPath("aaa.txt").filename() == "aaa.txt");
		K__Assert(KPath("aaa\\bbb.txt").filename() == "bbb.txt");
		K__Assert(KPath("aaa\\bbb\\ccc.txt").filename() == "ccc.txt");
		K__Assert(KPath("aaa\\bbb.zzz\\ccc.txt").filename() == "ccc.txt");

		K__Assert(KPath("q/aaa/bbb.c").getRelativePathFrom("q/aaa"    ).compare("bbb.c") == 0);
		K__Assert(KPath("q/aaa/bbb.c").getRelativePathFrom("q/aaa/ccc").compare("../bbb.c") == 0);
		K__Assert(KPath("q/aaa/bbb.c").getRelativePathFrom("q/bbb"    ).compare("../aaa/bbb.c") == 0);
		K__Assert(KPath("q/aaa/bbb.c").getRelativePathFrom("q/bbb/ccc").compare("../../aaa/bbb.c") == 0);

		K__Assert(KPath("q/aaa/bbb.c").changeExtension(".txt").compare("q/aaa/bbb.txt") == 0);
		K__Assert(KPath("q/aaa/bbb.c").changeExtension( "txt").compare("q/aaa/bbb.txt") == 0);
		K__Assert(KPath("q/aaa/bbb"  ).changeExtension(".txt").compare("q/aaa/bbb.txt") == 0);
		K__Assert(KPath("q/aaa/bbb"  ).changeExtension( "txt").compare("q/aaa/bbb.txt") == 0);
		K__Assert(KPath("q/aaa/bbb.c").changeExtension("").compare("q/aaa/bbb") == 0);
		K__Assert(KPath("q/aaa/bbb"  ).changeExtension("").compare("q/aaa/bbb") == 0);

		KPathList list;
		KPath("aaa/bbb/ccc").split(&list);
		K__Assert(list.size() == 3);
		K__Assert(list[0] == "aaa");
		K__Assert(list[1] == "bbb");
		K__Assert(list[2] == "ccc");

		KPath("/").split(&list);
		K__Assert(list.empty());

		KPath("c:/aaa/bbb/").split(&list);
		K__Assert(list.size() == 3);
		K__Assert(list[0] == "c:");
		K__Assert(list[1] == "aaa");
		K__Assert(list[2] == "bbb");
	}
	{
		K__Assert(KPath("abc").glob("*") == true);
		K__Assert(KPath("abc").glob("a*") == true);
		K__Assert(KPath("abc").glob("ab*") == true);
		K__Assert(KPath("abc").glob("abc*") == true);
		K__Assert(KPath("abc").glob("a*c") == true);
		K__Assert(KPath("abc").glob("*abc") == true);
		K__Assert(KPath("abc").glob("*bc") == true);
		K__Assert(KPath("abc").glob("*c") == true);
		K__Assert(KPath("abc").glob("*a*b*c*") == true);
		K__Assert(KPath("abc").glob("*bc*") == true);
		K__Assert(KPath("abc").glob("*c*") == true);
		K__Assert(KPath("aaa/bbb.ext").glob("a*.ext") == false);
		K__Assert(KPath("aaa/bbb.ext").glob("a*/*.ext") == true);
		K__Assert(KPath("aaa/bbb.ext").glob("a*/*.*t") == true);
		K__Assert(KPath("aaa/bbb.ext").glob("aaa/*.ext") == true);
		K__Assert(KPath("aaa/bbb.ext").glob("aaa/bbb*ext") == true);
		K__Assert(KPath("aaa/bbb.ext").glob("aaa*bbb*ext") == false);
		K__Assert(KPath("aaa/bbb/ccc.ext").glob("aaa/*/ccc.ext") == true);
		K__Assert(KPath("aaa/bbb/ccc.ext").glob("aaa/*.ext") == false);
		K__Assert(KPath("aaa/bbb.ext").glob("*.aaa") == false);
		K__Assert(KPath("aaa/bbb.ext").glob("aaa*bbb") == false);
	}
	{
		char loc[32];
		strcpy_s(loc, sizeof(loc), setlocale(LC_CTYPE, nullptr));
		K__Assert(KPath(u8"表計算\\ソフト\\能力.txt").directory() == KPath(u8"表計算/ソフト"));
		K__Assert(KPath(u8"表計算\\ソフト\\能力.txt").filename()  == KPath(u8"能力.txt"));
		K__Assert(KPath(u8"表計算\\ソフト\\能力.txt").extension().compare(".txt") == 0);
		K__Assert(KPath(u8"表計算\\ソフト\\能力.txt").changeExtension(".bmp") == KPath(u8"表計算\\ソフト\\能力.bmp"));
		K__Assert(KPath(u8"表計算\\ソフト\\能力.txt").changeExtension("") == KPath(u8"表計算\\ソフト\\能力"));
		setlocale(LC_CTYPE, loc);
	}
}
} // Test




#pragma region KToken funcs
#define BLANK_CHARS "\n\r\t "
static int KTok_isblank(int c) {
	return isascii(c) && isblank(c);
}

// s で最初に現れる非空白文字に移動する
static char * KTok_skip_blank(char *s) {
	return s + strspn(s, BLANK_CHARS);
}
static const char * KTok_skip_blank(const char *s) {
	return s + strspn(s, BLANK_CHARS);
}

// 範囲 [s..e] の文字列の末尾にある空白文字を '\0' で上書きする。
// 新しい末尾位置（ヌル文字）ポインタを返す
static char * KTok_trim_rihgt_blanks(char *s, char *e) {
	while (s < e && KTok_isblank(e[-1])) {
		e[-1] = '\0';
		e--;
	}
	return e;
}

// "\r\n" ===> "\n"
static void KTok_convert_newlines(std::string &out, const char *s) {
	out.reserve(strlen(s)); // 少なくとも元の文字列よりかは短くなるので strlen(s) だけ確保しておけばよい
	for (const char *c=s; *c; /*none*/) {
		if (c[0] == '\r' && c[1] == '\n') {
			out.push_back('\n');
			c += 2;
		} else if (c[0] == '\n') {
			out.push_back('\n');
			c++;
		} else {
			out.push_back(c[0]);
			c++;
		}
	}
}
#pragma endregion // KToken funcs


#pragma region KToken
KToken::KToken() {
}
KToken::KToken(const char *text, const char *delims, bool condense_delims, size_t maxdiv) {
	tokenize(text, delims, condense_delims, maxdiv);
}
// static func
void KToken::tokenize_raw(const char *text, const char *delims, bool condense_delims, int maxdiv, std::vector<KStringView> &ranges) {
	KStringView sv(text);
	if (maxdiv >= 2) {
		KStringView rest;
		ranges = sv.split(delims, condense_delims, true, maxdiv-1, &rest);
		if (!rest.empty()) {
			ranges.push_back(rest);
		}

	} else if (maxdiv == 1) {
		ranges.push_back(text);

	} else {
		ranges = sv.split(delims, condense_delims, true, 0);
	}
}
size_t KToken::tokenize(const char *text, const char *delims, bool condense_delims, size_t maxdiv) {
	m_str.clear();
	m_tok.clear();
	if (text == nullptr || text[0] == '\0') return 0;
	if (delims == nullptr || delims[0] == '\0') return 0;

	// strtok による文字列破壊に備え、入力文字列をコピーする
	m_str.assign(text);

	// デリミタに応じて素直に分割する
	tokenize_raw(m_str.c_str(), delims, condense_delims, maxdiv, m_tok);

	// 各トークンの前後の空白を取り除く（トークン範囲を変更するだけ）
	for (auto it=m_tok.begin(); it!=m_tok.end(); it++) {
		*it = it->trim();
	}
	
	// トークンの終端にヌル文字を置く
	// m_tok には this->m_str のアドレスが入っているため、
	// ここでは this->m_str の内容が書き換わることになる
	for (auto it=m_tok.begin(); it!=m_tok.end(); it++) {
		const char *endptr = it->end();
		*(char*)endptr = '\0';
	}
	return m_tok.size();
}
size_t KToken::splitLines(const char *text, bool with_empty_line) {
	m_str.clear();
	m_tok.clear();
	if (text == nullptr || text[0] == '\0') return 0;

	// 処理を簡単かつ確実にするため、１文字で改行を表すようにしておく
	std::string cpy;
	KTok_convert_newlines(cpy, text);

	// 空白行を認識させるなら、連続する改行文字を結合してはいけない
	bool condense_delims = !with_empty_line;

	// 改行文字を区切りにして分割する
	return tokenize(cpy.c_str(), "\n", condense_delims);
}
size_t KToken::split(const char *text, const char *delims) {
	return tokenize(text, delims, true, 2);
}
const char * KToken::operator[](size_t index) const { 
	return m_tok[index].begin();
}
const char * KToken::get(size_t index, const char *def) const {
	if (index < m_tok.size()) {
		auto tok = m_tok[index];
		K__Assert(*tok.end() == '\0');
		return tok.begin();
	} else {
		return def;
	}
}
int KToken::toInt(size_t index, int def) const {
	int val = def;
	if (index < m_tok.size()) {
		m_tok[index].toIntTry(&val);
	}
	return val;
}
bool KToken::isInt(size_t index) const {
	if (index < m_tok.size() && m_tok[index].toIntTry(nullptr)) {
		return true;
	}
	return false;
}
uint32_t KToken::toUint32(size_t index, uint32_t def) const {
	uint32_t val = def;
	if (index < m_tok.size()) {
		m_tok[index].toUintTry(&val);
	}
	return val;
}
float KToken::toFloat(size_t index, float def) const {
	float val = def;
	if (index < m_tok.size()) {
		m_tok[index].toFloatTry(&val);
	}
	return val;
}
int KToken::compare(size_t index, const char *str) const {
	K__Assert(str);
	if (index < m_tok.size()) {
		return m_tok[index].compare(str);
	} else {
		return strcmp("", str);
	}
}
size_t KToken::size() const { 
	return m_tok.size();
}
#pragma endregion // KToken



#pragma region KNumval
KNumval::KNumval() {
	clear();
}
KNumval::KNumval(const char *expr) {
	clear();
	parse(expr);
}
void KNumval::clear() {
	num[0] = '\0';
	suf[0] = '\0';
	numf = 0.0f;
}
void KNumval::assign(const KNumval &source) {
	*this = source;
}
bool KNumval::parse(const char *expr) {
	if (expr == nullptr || expr[0] == 0) return false;

	// 数字文字列と単位文字列に分割
	int pos = (int)strlen(expr);
	for (int i=(int)strlen(expr)-1; i>=0; i--) {
		K__Assert(!isblank(expr[i]));
		if (isdigit(expr[i])) {
			break;
		}
		pos = i;
	}
	strncpy_s(this->num, sizeof(this->num), expr, pos); // [pos] よりも前を取得
	this->num[pos] = 0;

	strcpy_s(this->suf, sizeof(this->suf), expr+pos); // [pos] よりも後を取得

	// 数字文字列を解析
	this->numf = KStringUtils::toFloat(this->num);
	return true;
}
bool KNumval::has_suffix(const char *_suf) const {
	return strcmp(this->suf, _suf) == 0;
}
int KNumval::valuei(int percent_base) const {
	if (has_suffix("%")) {
		return (int)(this->numf * percent_base / 100);
	} else {
		return (int)(this->numf);
	}
}
float KNumval::valuef(float percent_base) const {
	if (has_suffix("%")) {
		return this->numf * percent_base / 100;
	} else {
		return this->numf;
	}
}
#pragma endregion // KNumval



namespace Test {
void Test_numval() {
	{
		KNumval n("20");
		K__Assert(strcmp(n.num, "20") == 0);
		K__Assert(strcmp(n.suf, "") == 0);
		K__Assert(n.numf == 20);
		K__Assert(n.has_suffix("") == true);
		K__Assert(n.valuei(0) == 20); // % が付いていないので引数は無視される
		K__Assert(n.valuei(100) == 20); // % が付いていないので引数は無視される
	}

	{
		KNumval n("70%");
		K__Assert(strcmp(n.num, "70") == 0);
		K__Assert(strcmp(n.suf, "%") == 0);
		K__Assert(n.numf == 70);
		K__Assert(n.has_suffix("") == false);
		K__Assert(n.has_suffix("%") == true);
		K__Assert(n.valuei(0) == 0); // % が付いている。この場合は 0 の 70% の値を返す
		K__Assert(n.valuei(200) == 140); // % が付いている。この場合は 200 の 70% の値を返す
	}

	{
		KNumval n("70pt");
		K__Assert(strcmp(n.num, "70") == 0);
		K__Assert(strcmp(n.suf, "pt") == 0);
		K__Assert(n.numf == 70);
		K__Assert(n.has_suffix("") == false);
		K__Assert(n.has_suffix("pt") == true);
		K__Assert(n.valuei(0) == 70); // % が付いていないので引数は無視される
		K__Assert(n.valuei(200) == 70); // % が付いていないので引数は無視される
	}
}
} // Test


#pragma region KCommandLineArgs
KCommandLineArgs::KCommandLineArgs(const char *cmdline) {
	char u8[1024] = {0};
	KStringUtils::ansiToUtf8(u8, sizeof(u8), cmdline);
	m_tok.tokenize(u8, "/, ");
}
int KCommandLineArgs::size() const {
	return m_tok.size();
}
const char * KCommandLineArgs::operator[](int index) const {
	return m_tok[index];
}
bool KCommandLineArgs::contains(const char *s) const {
	for (int i=0; i<size(); i++) {
		if (strcmp(m_tok[i], s) == 0) {
			return true;
		}
	}
	return false;
}
#pragma endregion // KCommandLineArgs


} // namespace
