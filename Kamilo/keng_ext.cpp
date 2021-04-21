﻿#include "keng_ext.h"

#include <math.h>
#include <algorithm>
#include <unordered_set>
#include <mutex>
#include <stdarg.h>
#include <locale.h> // set_locale for Test

#ifdef _WIN32
#	include <Windows.h>
#	include <DbgHelp.h> // SymInitialize
#	include <tlhelp32.h> // CreateToolhelp32Snapshot
#	include <Shlobj.h> // SHGetFolderPath
#	include <Shlwapi.h>
#	include <d3d9.h>
#	pragma comment(lib, "shlwapi.lib")
#endif

#pragma region IMGUI
// ImGui::GetFont が win32api の GetFont マクロと競合するため、
// Windows.h よりも前に include する
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h" // ImGui::GetCurrentWindow()
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#pragma endregion // IMGUI

#include "KInternal.h"
#include "KStorage.h"
#include "KRes.h"
#include "KDirectoryWalker.h"
#include "KZlib.h"
#include "KZip.h"



#define CORE_SYS 1



namespace Kamilo {


// 格納可能な文字数を超えたら、もう何もできない。
// 下手に長さを切り詰めたまま処理を続行すると、
// 意図しないファイル名やディレクトリになったりして
// 危険すぎるため、ここで強制終了するのが良い
#define TOO_LONG_PATH_ERROR()  KLog::printFatal("Too long path string")



#pragma region KConv
static const size_t UTF8_BOMSIZE = 3;
static const char * UTF8_BOM = "\xEF\xBB\xBF";

static void kkstr_Error(const char *msg, const void *data, int size) {
	KLog::printError("STRING ENCODING FAILED:");
}

std::wstring KConv::toWide(const void *data, int size) {
	// ワイド文字への変換を試みる
	std::wstring out = KStringUtils::binToWide(data, size);
	if (out.size() > 0) {
		return out;
	}
	kkstr_Error("E_CONV: toWide: Failed", data, size);

	// 入力バイナリを wchar_t 配列に拡張したものを返す
	if (data && size > 0) {
		out.resize(size, L'\0');
		for (int i=0; i<size; i++) {
			out[i] = ((const char *)data)[i]; // char --> wchar_t
		}
	}
	return out;
}
std::wstring KConv::toWide(const std::string &data) {
	return toWide(data.data(), data.size());
}
std::string KConv::toUtf8(const void *data, int size) {
	// ワイド文字への変換を試みる
	std::wstring ws = KStringUtils::binToWide(data, size);
	if (ws.size() > 0) {
		return KStringUtils::wideToUtf8(ws);
	}
	// 入力バイナリをそのまま返す
	return std::string((const char*)data, size);
}
std::string KConv::toUtf8(const std::string &data) {
	return toUtf8(data.data(), data.size());
}
#pragma endregion // KConv








#pragma region File.cpp




static time_t kk_get_time_t(const _FILETIME *ft) {
#ifdef _WIN32
	K_assert(sizeof(time_t) == 8); // 64bit
	// FILETIMEを等価な64ビットのファイル時刻形式（1601/1/1 0:00から100ナノ秒刻み）に変換
	uint64_t ntfs64 = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;

	// NTFSで使われている64ビットのファイル時刻形式から
	// time_t 形式（1970/1/1 0:00から1秒刻み）を得る
	uint64_t basetime = 116444736000000000; // FILETIME at 1970/1/1 0:00
	uint64_t nsec  = 10000000; // 100nsec --> sec  10 * 1000 * 1000
	return (time_t)((ntfs64 - basetime) / nsec);
#else
	return 0;
#endif
}
static bool kk_GetTimeStamp(const KPath &filename, time_t *time_cma) {
#ifdef _WIN32
	HANDLE hFile = CreateFileW(filename.toWideString().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) return false;
	FILETIME ctm, mtm, atm;
	GetFileTime(hFile, &ctm, &atm, &mtm);
	CloseHandle(hFile);
	time_cma[0] = kk_get_time_t(&ctm); // creation
	time_cma[1] = kk_get_time_t(&mtm); // modify
	time_cma[2] = kk_get_time_t(&atm); // access
	return true;
#else
	struct stat st;
	memset(&st, 0, sizeof(st));
	stat(filename.u8(), &st);
	return st.st_mtime;
#endif
}


#pragma region KFiles
KPath KFiles::getCurrentDirectory() {
	char s[KPathUtils::MAX_SIZE] = {0};
	K__getcwd_u8(s, sizeof(s));
	return s;
}
bool KFiles::setCurrentDirectory(const KPath &path) {
	return K__setcwd_u8(path.u8());
}
bool KFiles::exists(const KPath &path) {
	return KPathUtils::K_PathExists(path.u8());
}
bool KFiles::isFile(const KPath &path) {
	return KPathUtils::K_PathIsFile(path.u8());
}
bool KFiles::isDirectory(const KPath &path) {
	return KPathUtils::K_PathIsDir(path.u8());
}
bool KFiles::copy(const KPath &src, const KPath &dst, bool overwrite) {
	return K_FileCopy(src.u8(), dst.u8(), overwrite);
}
time_t KFiles::getLastModificationTime(const KPath &filename) {
	time_t time_cma[] = {0, 0, 0};
	KPathUtils::K_PathGetTimeStamp(filename.u8(), time_cma);
	return time_cma[1];
}
size_t KFiles::getFileSize(const KPath &filename) {
	int sz = 0;
	KPathUtils::K_PathGetFileSize(filename.u8(), &sz);
	return (size_t)sz;
}
bool KFiles::makeDirectory(const KPath &dir, int *err) {
	return K_FileMakeDir(dir.u8());
}
bool KFiles::removeFile(const KPath &path) {
	return K_FileRemove(path.u8());
}
bool KFiles::removeDirectory(const KPath &dir) {
	return K_FileRemoveEmptyDir(dir.u8());
}
bool KFiles::removeDirectoryTree(const KPath &dir) {
	return K_FileRemoveEmptyDirTree(dir.u8());
}
bool KFiles::removeFilesInDirectory(const KPath &dir, bool subdir) {
	if (subdir) {
		return K_FileRemoveFilesInDirTree(dir.u8());
	} else {
		return K_FileRemoveFilesInDir(dir.u8());
	}
}
class Scan_cb: public KDirectoryWalker::Callback {
public:
	KPathList m_list;
	bool m_scan_in_subdir;

	Scan_cb() {
		m_scan_in_subdir = false;
	}

	virtual void onDir(const char *name_u8, const char *parent_u8, bool *enter) override {
		m_list.push_back(KPath(parent_u8).join(name_u8));
		*enter = m_scan_in_subdir;
	}
	virtual void onFile(const char *name_u8, const char *parent_u8) override {
		m_list.push_back(KPath(parent_u8).join(name_u8));
	}
};
KPathList KFiles::scanFiles(const KPath &dir) {
	Scan_cb cb;
	cb.m_scan_in_subdir = false;
	KDirectoryWalker::walk(dir.u8(), &cb);
	return cb.m_list;
}
KPathList KFiles::scanFilesInTree(const KPath &dir) {
	Scan_cb cb;
	cb.m_scan_in_subdir = true;
	KDirectoryWalker::walk(dir.u8(), &cb);
	return cb.m_list;
}
#pragma endregion KFiles





const int CHUNK_SIGN_SIZE = sizeof(chunk_id_t);
const int CHUNK_SIZE_SIZE = sizeof(chunk_size_t);

#pragma region KChunkedFileReader
KChunkedFileReader::KChunkedFileReader() {
	m_ptr = nullptr;
	m_end = nullptr;
	m_chunk_data = nullptr;
	m_chunk_id = 0;
	m_chunk_size = 0;
	m_nest = std::stack<const uint8_t *>(); // clear
}
void KChunkedFileReader::init(const void *buf, size_t len) {
	m_ptr = (uint8_t *)buf;
	m_end = (uint8_t *)buf + len;
	m_chunk_id = 0;
	m_chunk_data = nullptr;
	m_chunk_size = 0;
	m_nest = std::stack<const uint8_t *>(); // clear
}
bool KChunkedFileReader::eof() const {
	return m_end <= m_ptr;
}
bool KChunkedFileReader::checkChunkId(chunk_id_t id) const {
	chunk_id_t s;
	if (getChunkHeader(&s, nullptr)) {
		if (s == id) {
			return true;
		}
	}
	return false;
}
void KChunkedFileReader::openChunk(chunk_id_t id) {
	// 子チャンクの先頭に移動する
	chunk_size_t size = 0;
	getChunkHeader(nullptr, &size);
	readChunkHeader(id, 0);

	// この時点で m_ptr は子チャンク先頭を指している。
	// そこからさらに size 進めた部分が子チャンク終端になる
	m_nest.push(m_ptr + size);
}
void KChunkedFileReader::closeChunk() {
	// 入れ子の終端に一致しているか確認する
	K_assert(!m_nest.empty());
	K_assert(m_nest.top() == m_ptr);
	m_nest.pop();
}
bool KChunkedFileReader::getChunkHeader(chunk_id_t *out_id, chunk_size_t *out_size) const {
	if (m_ptr+CHUNK_SIGN_SIZE+CHUNK_SIZE_SIZE < m_end) {
		if (out_id) {
			memcpy(out_id, m_ptr, CHUNK_SIGN_SIZE);
		}
		if (out_size) {
			memcpy(out_size, m_ptr+CHUNK_SIGN_SIZE, CHUNK_SIZE_SIZE);
		}
		return true;
	}
	return false;
}
void KChunkedFileReader::readChunkHeader(chunk_id_t id, chunk_size_t size) {
	K_assert(m_ptr);

	memcpy(&m_chunk_id, m_ptr, CHUNK_SIGN_SIZE);
	m_ptr += CHUNK_SIGN_SIZE;

	memcpy(&m_chunk_size, m_ptr, CHUNK_SIZE_SIZE);
	m_ptr += CHUNK_SIZE_SIZE;

	K_assert(id==0 || id==m_chunk_id);
	K_assert(size==0 || size==m_chunk_size);
	m_chunk_data = m_ptr;
}
void KChunkedFileReader::readChunk(chunk_id_t id, chunk_size_t size, void *data) {
	readChunkHeader(id, size);

	K_assert(m_chunk_data);
	if (m_chunk_size > 0) {
		if (data) memcpy(data, m_chunk_data, m_chunk_size);
	}
	m_ptr = m_chunk_data + m_chunk_size;
}

void KChunkedFileReader::readChunk(chunk_id_t id, chunk_size_t size, std::string *data) {
	readChunkHeader(id, size);

	K_assert(m_chunk_data);
	if (data) {
		data->resize(m_chunk_size);
		if (m_chunk_size > 0) {
			memcpy(&(*data)[0], m_chunk_data, m_chunk_size);
		}
	}
	m_ptr = m_chunk_data + m_chunk_size;
}
std::string KChunkedFileReader::readChunk(chunk_id_t id) {
	std::string val;
	readChunk(id, 0, &val);
	return val;
}
uint8_t KChunkedFileReader::readChunk1(chunk_id_t id) {
	uint8_t val = 0;
	readChunk(id, 1, &val);
	return val;
}
uint16_t KChunkedFileReader::readChunk2(chunk_id_t id) {
	uint16_t val = 0;
	readChunk(id, 2, &val);
	return val;
}
uint32_t KChunkedFileReader::readChunk4(chunk_id_t id) {
	uint32_t val = 0;
	readChunk(id, 4, &val);
	return val;
}
void KChunkedFileReader::readChunk(chunk_id_t id, std::string *data) {
	readChunkHeader(id, 0);

	K_assert(m_chunk_data);
	if (data) {
		data->resize(m_chunk_size);
		if (m_chunk_size > 0) {
			memcpy(&(*data)[0], m_chunk_data, m_chunk_size);
		}
	}
	m_ptr = m_chunk_data + m_chunk_size;
}
#pragma endregion // KChunkedFileReader


#pragma region KChunkedFileWriter
KChunkedFileWriter::KChunkedFileWriter() {
	m_pos = 0;
}
void KChunkedFileWriter::clear() {
	m_pos = 0;
	m_buf.clear();
	while (! m_stack.empty()) m_stack.pop();
}
const void * KChunkedFileWriter::data() const {
	return m_buf.data();
}
size_t KChunkedFileWriter::size() const {
	return m_buf.size();
}
void KChunkedFileWriter::openChunk(chunk_id_t id) {
	// チャンクを開く。チャンクヘッダだけ書き込んでおく
	// ただし、チャンクデータサイズは子チャンクを追加してからでないと確定できないので、
	// いま書き込むのは識別子のみ
	m_buf.resize(m_pos + CHUNK_SIGN_SIZE + CHUNK_SIZE_SIZE);

	memcpy(&m_buf[m_pos], &id, CHUNK_SIGN_SIZE);
	m_pos += CHUNK_SIGN_SIZE;

	m_stack.push(m_pos);
	m_pos += CHUNK_SIZE_SIZE; // データサイズは未確定（チャンクが閉じたときにはじめて確定する）なので、まだ書き込まない
}
void KChunkedFileWriter::closeChunk() {
	// 現在のチャンクのヘッダー位置（サイズ書き込み位置）
	chunk_size_t sizepos = m_stack.top();

	// 現在のチャンクのデータ開始位置
	chunk_size_t datapos = sizepos + CHUNK_SIZE_SIZE;

	// 現在のチャンクのデータサイズ
	chunk_size_t size = m_pos - datapos;

	// 現在のチャンクヘッダ位置まで戻り、確定したチャンクデータサイズを書き込む
	memcpy(&m_buf[sizepos], &size, CHUNK_SIZE_SIZE);
	m_stack.pop();
}
void KChunkedFileWriter::writeChunkN(chunk_id_t id, chunk_size_t size, const void *data) {
	// チャンクを書き込むのに必要なサイズだけ拡張する
	// 新たに必要になるサイズ = チャンクヘッダ＋データ
	m_buf.resize(m_pos + (CHUNK_SIGN_SIZE + CHUNK_SIZE_SIZE) + size);

	memcpy(&m_buf[m_pos], &id, CHUNK_SIGN_SIZE);
	m_pos += CHUNK_SIGN_SIZE;

	memcpy(&m_buf[m_pos], &size, 4);
	m_pos += CHUNK_SIZE_SIZE;

	if (size > 0) {
		if (data) {
			memcpy(&m_buf[m_pos], data, size);
		} else {
			memset(&m_buf[m_pos], 0, size);
		}
		m_pos += size;
	}
}
void KChunkedFileWriter::writeChunkN(chunk_id_t id, const std::string &data) {
	if (data.empty()) {
		// サイズゼロのデータを持つチャンク
		writeChunkN(id, 0, nullptr);
	} else {
		// std::string::size() の戻り値 size_t は 32ビットとは限らないので注意。
		// 例えば MacOS だと size_t は 64bit になる
		writeChunkN(id, (chunk_size_t)data.size(), &data[0]);
	}
}
void KChunkedFileWriter::writeChunkN(chunk_id_t id, const char *data) {
	// strlen の戻り値 size_t は 32ビットとは限らないので注意。
	// 例えば MacOS だと size_t は 64bit になる
	writeChunkN(id, (chunk_size_t)strlen(data), data);
}
void KChunkedFileWriter::writeChunk1(chunk_id_t id, uint8_t data) {
	writeChunkN(id, 1, &data);
}
void KChunkedFileWriter::writeChunk2(chunk_id_t id, uint16_t data) {
	writeChunkN(id, 2, &data);
}
void KChunkedFileWriter::writeChunk4(chunk_id_t id, uint32_t data) {
	writeChunkN(id, 4, &data);
}
#pragma endregion // KChunkedFileWriter


namespace Test {
/// [Test_files_scan] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない
void Test_files_scan() {
	KPath dir = "c:\\windows\\media";
	KPathList files = KFiles::scanFilesInTree(dir);
	for (size_t i=0; i<files.size(); i++) {
		KPath path = dir.join(files[i]);
		if (KFiles::isDirectory(path)) {
			OutputDebugStringW(L"* ");
		} else {
			OutputDebugStringW(L"  ");
		}
		OutputDebugStringW(path.toWideString().c_str());
		OutputDebugStringW(L"\n");
	}
}
/// [Test_files_scan] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない


/// [Test_chunk] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない
void Test_chunk() {
	// データ作成
	KChunkedFileWriter w;
	w.writeChunk2(0x1000, 0x2019);
	w.writeChunk4(0x1001, 0xDeadBeef);
	w.writeChunkN(0x1002, "HELLO WOROLD!");
	w.openChunk(0x1003);
	w.writeChunk1(0x2000, 'a');
	w.writeChunk1(0x2001, 'b');
	w.writeChunk1(0x2002, 'c');
	w.closeChunk();

	// 書き込んだバイナリデータを得る
	std::string bin((const char *)w.data(), w.size());

	// 読み取る
	KChunkedFileReader r;
	r.init(bin.data(), bin.size());
	K_assert(r.readChunk2(0x1000) == 0x2019);
	K_assert(r.readChunk4(0x1001) == 0xDeadBeef);
	K_assert(r.readChunk(0x1002) == "HELLO WOROLD!");

	r.openChunk(0); // 入れ子チャンクに入る(識別子気にしない)
	K_assert(r.readChunk1(0) == 'a');
	K_assert(r.readChunk1(0) == 'b');
	K_assert(r.readChunk1(0) == 'c');
	r.closeChunk();
}
/// [Test_chunk] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない

} // Test








#pragma region KStorage








#pragma endregion // KStorage



#pragma endregion // File.cpp












#pragma region Table.cpp

static void kk__escape_string(std::string &s) {
	KStringUtils::replace(s, "\\", "\\\\");
	KStringUtils::replace(s, "\"", "\\\"");
	KStringUtils::replace(s, "\n", "\\n");
	KStringUtils::replace(s, "\r", "");
	if (s.find(',') != std::string::npos) {
		s.insert(0, "\"");
		s.append("\"");
	}
}
#define HAS_CHAR(str, chr) (strchr(str, chr) != nullptr)
static bool kkxml__stringShouldEscape(const char *s) {
	K_assert(s);
	return HAS_CHAR(s, '<') || HAS_CHAR(s, '>') || HAS_CHAR(s, '"') || HAS_CHAR(s, '\'') || HAS_CHAR(s, '\n');
}

#define EXCEL_ALPHABET_NUM 26
#define EXCEL_COL_LIMIT    (EXCEL_ALPHABET_NUM * EXCEL_ALPHABET_NUM)
#define EXCEL_ROW_LIMIT    1024



#pragma region KExcel
KPath KExcel::encodeCellName(int col, int row) {
	KPath s;
	encodeCellName(col, row, &s);
	return s;
}
bool KExcel::encodeCellName(int col, int row, KPath *name) {
	if (col < 0) return false;
	if (row < 0) return false;
	if (col < EXCEL_ALPHABET_NUM) {
		char c = (char)('A' + col);
		char s[256];
		sprintf_s(s, sizeof(s), "%c%d", c, 1+row);
		if (name) *name = KPath(s);
		return true;
	}
	if (col < EXCEL_ALPHABET_NUM*EXCEL_ALPHABET_NUM) {
		char c1 = (char)('A' + (col / EXCEL_ALPHABET_NUM));
		char c2 = (char)('A' + (col % EXCEL_ALPHABET_NUM));
		char s[256];
		K_assert(isalpha(c1));
		K_assert(isalpha(c2));
		sprintf_s(s, sizeof(s), "%c%c%d", c1, c2, 1+row);
		if (name) *name = KPath(s);
		return true;
	}
	return false;
}
/// "A2" や "AM244" などのセル番号を int の組にデコードする
bool KExcel::decodeCellName(const char *s, int *col, int *row) {
	if (s==nullptr || s[0]=='\0') return false;
	int c = -1;
	int r = -1;

	if (isalpha(s[0]) && isdigit(s[1])) {
		// セル番号が [A-Z][0-9].* にマッチしている。
		// 例えば "A1" や "Z42" など。
		c = toupper(s[0]) - 'A';
		K_assert(0 <= c && c < EXCEL_ALPHABET_NUM);
		r = strtol(s + 1, nullptr, 0);
		r--; // １起算 --> 0起算

	} else if (isalpha(s[0]) && isalpha(s[1]) && isdigit(s[2])) {
		// セル番号が [A-Z][A-Z][0-9].* にマッチしている。
		// 例えば "AA42" や "KZ1217" など
		int idx1 = toupper(s[0]) - 'A';
		int idx2 = toupper(s[1]) - 'A';
		K_assert(0 <= idx1 && idx1 < EXCEL_ALPHABET_NUM);
		K_assert(0 <= idx2 && idx2 < EXCEL_ALPHABET_NUM);
		c = idx1 * EXCEL_ALPHABET_NUM + idx2;
		r = strtol(s + 2, nullptr, 0);
		r--; // １起算 --> 0起算
	}
	if (c >= 0 && r >= 0) {
		// ここで、セル範囲として 0xFFFFF が入る可能性に注意。(LibreOffice Calc で現象を確認）
		// 0xFFFFF 以上の値が入っていたら範囲取得失敗とみなす
		if (r >= 0xFFFFF) {
			return false;
		}
		K_assert(0 <= c && c < EXCEL_COL_LIMIT);
		K_assert(0 <= r && r < EXCEL_ROW_LIMIT);
		if (col) *col = c;
		if (row) *row = r;
		return true;
	}

	return false;
}
bool KExcel::exportXml(const KExcelFile &excel, KOutputStream &output, bool comment) {
	class CB: public KExcelScanCellsCallback {
	public:
		std::string &dest_;
		int last_row_;
		int last_col_;
		
		CB(std::string &s): dest_(s) {
			last_row_ = -1;
			last_col_ = -1;
		}
		virtual void onCell(int col, int row, const char *s) override {
			if (s==nullptr || s[0]=='\0') return;
			K_assert(last_row_ <= row); // 行番号は必ず前回と等しいか、大きくなる
			if (last_row_ != row) {
				if (last_row_ >= 0) { // 行タグを閉じる
					dest_ += "</row>\n";
				}
				if (last_row_ < 0 || last_row_ + 1 < row) {
					// 行番号が飛んでいる場合のみ列番号を付加する
					dest_ += KStringUtils::K_sprintf("\t<row r='%d'>", row);
				} else {
					// インクリメントで済む場合は行番号を省略
					dest_ += "\t<row>";
				}
				last_row_ = row;
				last_col_ = -1;
			}
			if (last_col_ < 0 || last_col_ + 1 < col) {
				// 列番号が飛んでいる場合のみ列番号を付加する
				dest_ += KStringUtils::K_sprintf("<c i='%d'>", col);
			} else {
				// インクリメントで済む場合は列番号を省略
				dest_ += "<c>";
			}
			if (kkxml__stringShouldEscape(s)) { // xml禁止文字が含まれているなら CDATA 使う
				dest_ += KStringUtils::K_sprintf("<![CDATA[%s]]>", s);
			} else {
				dest_ += s;
			}
			dest_ += "</c>";
			last_col_ = col;
		}
	};
	if (excel.empty()) return false;
	if (!output.isOpen()) return false;
	
	std::string s;
	s += "<?xml version='1.0' encoding='utf-8'>\n";
	if (comment) {
		s += u8"<!-- <sheet> タグは「シート」に対応する。 left, top, cols, rows 属性にはそれぞれ、シート内で値が入っているセル範囲の左、上、行数、列数が入る -->\n";
		s += u8"<!-- <row> タグは各シートの「行」に対応する。 <row> の r 属性には 0 起算での行番号が入る。ただし直前の <row> の次の行だった場合 r 属性は省略される -->\n";
		s += u8"<!-- <c> タグは、それぞれの行 <row> 内にある「セル」に対応する。 i 属性には 0 起算での列番号が入る。ただし、直前の <c> の次の列だった場合 i 属性は省略される -->\n";
	}
	s += KStringUtils::K_sprintf("<excel numsheets='%d'>\n", excel.getSheetCount());
	for (int iSheet=0; iSheet<excel.getSheetCount(); iSheet++) {
		int col=0, row=0, nCol=0, nRow=0;
		KPath sheet_name = excel.getSheetName(iSheet);
		excel.getSheetDimension(iSheet, &col, &row, &nCol, &nRow);
		s += KStringUtils::K_sprintf("<sheet name='%s' left='%d' top='%d' cols='%d' rows='%d'>\n", sheet_name.u8(), col, row, nCol, nRow);
		{
			CB cb(s);
			excel.scanCells(iSheet, &cb);
			if (cb.last_row_ >= 0) {
				s += "</row>\n"; // 最終行を閉じる
			} else {
				// セルを一つも出力していないので <row> を閉じる必要もない
			}
		}
		s += "</sheet>";
		if (comment) {
			s += KStringUtils::K_sprintf("<!-- %s -->", sheet_name.u8());
		}
		s += "\n\n";
	}
	s += "</excel>\n";
	output.write(s.data(), s.size()); // UTF8
	return true;
}
void KExcel::exportXml(const char *input_xlsx_filename, const char *output_xml_filename, bool comment) {
	KExcelFile excel;
	excel.loadFromFileName(input_xlsx_filename);
	if (excel.empty()) {
		return;
	}

	KOutputStream outFile = KOutputStream::fromFileName(output_xml_filename);
	if (outFile.isOpen()) {
		exportXml(excel, outFile, comment);
	}
}
std::string KExcel::exportText(const KExcelFile &excel) {
	if (excel.empty()) return "";
	std::string s;
	for (int iSheet=0; iSheet<excel.getSheetCount(); iSheet++) {
		int col=0, row=0, nCol=0, nRow=0;
		KPath sheet_name = excel.getSheetName(iSheet);
		excel.getSheetDimension(iSheet, &col, &row, &nCol, &nRow);
		s += "\n";
		s += "============================================================================\n";
		s += std::string(sheet_name.u8()) + "\n";
		s += "============================================================================\n";
		int blank_lines = 0;
		for (int r=0; r<nRow; r++) {
			bool has_cell = false;
			for (int c=0; c<nCol; c++) {
				const char *str = excel.getDataString(iSheet, c, r);
				if (str && str[0]) {
					if (blank_lines >= 1) {
						s += "\n";
						blank_lines = 0;
					}
					if (has_cell) s += ", ";
					std::string ss = str;
					kk__escape_string(ss);
					s += ss;
					has_cell = true;
				}
			}
			if (has_cell) {
				s += "\n";
			} else {
				blank_lines++;
			}
		}
	}
	return s;
}
std::string KExcel::exportText(const char *xlsx_filenamee) {
	KExcelFile excel;
	if (excel.loadFromFileName(xlsx_filenamee)) {
		return exportText(excel);
	}
	return "";
}
#pragma endregion // KExcel

#pragma region KExcel Utils
static int kk__findindex(KUnzipper &zr, const char *name) {
	int num = zr.getEntryCount();
	for (int i=0; i<num; i++) {
		std::string s;
		zr.getEntryName(i, &s);
		if (s.compare(name) == 0) {
			return i;
		}
	}
	return -1;
}

static KXmlElement * kk__LoadXmlFromZip(KUnzipper &zr, const char *zipname, const char *entry_name) {
	if (!zr.isOpen()) {
		KLog::printError("E_INVALID_ARGUMENT");
		return nullptr;
	}

	// XLSX の XML は常に UTF-8 で書いてある。それを信用する
	int fileid = kk__findindex(zr, entry_name);
	if (fileid < 0) {
		KLog::printError("E_FILE: Failed to open file '%s' from archive '%s'", entry_name, zipname);
		return nullptr;
	}

	std::string xml_u8;
	if (!zr.getEntryData(fileid, "", &xml_u8)) {
		KLog::printError("E_FILE: Failed to open file '%s' from archive '%s'", entry_name, zipname);
		return nullptr;
	}

	KPath doc_name = KPath(zipname).join(entry_name);
	KXmlElement *xdoc = KXmlElement::createFromString(xml_u8.c_str(), doc_name.u8());
	if (xdoc == nullptr) {
		KLog::printError("E_XML: Failed to read xml document: '%s' from archive '%s'", entry_name, zipname);
	}
	return xdoc;
}

class CCoreExcelReader {
	static const int ROW_LIMIT = 10000;

	enum Type {
		TP_NUMBER,
		TP_STRINGID,
		TP_LITERAL,
		TP_OTHER,
	};
	std::string m_filename;
	KXmlElement *m_workbook_doc;
	std::vector<KXmlElement *> m_worksheets;
	std::unordered_map<int, std::string> m_strings;
	typedef std::unordered_map<int, const KXmlElement*> Int_Elms;
	typedef std::unordered_map<const KXmlElement*, Int_Elms> Sheet_RowElms;
	mutable Sheet_RowElms m_row_elements;
public:
	CCoreExcelReader() {
		m_workbook_doc = nullptr;
	}
	virtual ~CCoreExcelReader() {
		clear();
	}
	void clear() {
		for (size_t i=0; i<m_worksheets.size(); i++) {
			K_Drop(m_worksheets[i]);
		}
		m_worksheets.clear();
		m_strings.clear();
		m_row_elements.clear();
		K_Drop(m_workbook_doc);
		m_filename.clear();
	}
	bool empty() const {
		return m_worksheets.empty();
	}
	const char * getFileName() const {
		return m_filename.c_str();
	}
	int getSheetCount() const {
		return (int)m_worksheets.size();
	}
	const char * getSheetName(int sheetId) const {
		const KXmlElement *root_elm = m_workbook_doc->getNode(0);
		K_assert(root_elm);

		const KXmlElement *sheets_xml = root_elm->findNode("sheets");
		K_assert(sheets_xml);

		int idx = 0;
		for (int iSheet=sheets_xml->getNodeIndex("sheet"); iSheet>=0; iSheet=sheets_xml->getNodeIndex("sheet", iSheet+1)) {
			const KXmlElement *xSheet = sheets_xml->getNode(iSheet);
			if (idx == sheetId) {
				const char *s = xSheet->findAttr("name");
				#ifdef _DEBUG
				{
					// <sheet> の sheetId には１起算のシート番号が入っていて、
					// その番号は <sheets> 内での <sheet> の並び順と同じであると仮定している。
					// 一応整合性を確認しておく
					int id = xSheet->findAttrInt("sheetId", -1);
					K_assert(id == 1 + idx);
				}
				#endif
				return s;
			}
			idx++;
		}
		return nullptr;
	}
	int getSheetByName(const char *name) const {
		const KXmlElement *root_elm = m_workbook_doc->getNode(0);
		K_assert(root_elm);

		const KXmlElement *sheets_xml = root_elm->findNode("sheets");
		K_assert(sheets_xml);

		int idx = 0;
		for (int iSheet=sheets_xml->getNodeIndex("sheet"); iSheet>=0; iSheet=sheets_xml->getNodeIndex("sheet", iSheet+1)) {
			const KXmlElement *xSheet = sheets_xml->getNode(iSheet);
			const char *name_u8 = xSheet->findAttr("name");
			if (name_u8 && strcmp(name, name_u8) == 0) {
				#ifdef _DEBUG
				{
					// <sheet> の sheetId には１起算のシート番号が入っていて、
					// その番号は <sheets> 内での <sheet> の並び順と同じであると仮定している。
					// 一応整合性を確認しておく
					int id = xSheet->findAttrInt("sheetId", -1);
					K_assert(id == 1 + idx);
				}
				#endif
				return idx;
			}
			idx++;
		}
		return -1;
	}
	bool getSheetDimension(int sheet, int *col, int *row, int *colcount, int *rowcount) const {
		if (sheet < 0) return false;
		if (sheet >= (int)m_worksheets.size()) return false;

		const KXmlElement *xdoc = m_worksheets[sheet];
		K_assert(xdoc);

		const KXmlElement *xroot = xdoc->getNode(0);
		K_assert(xroot);

		const KXmlElement *xdim = xroot->findNode("dimension");
		K_assert(xdim);

		// セルの定義範囲を表す文字列を取得する
		// この文字列は "A1:E199" のようにコロンで左上端セルと右下端セル番号が書いてある
		const char *attr = xdim->findAttr("ref");

		// コロンを区切りにして分割
		char tmp[256];
		strcpy_s(tmp, sizeof(tmp), attr ? attr : "");
		
		// コロンの位置を見つける。
		char *colon = strchr(tmp, ':');
		if (colon) {
			// コロンの位置で文字列を分割する。コロンをヌル文字に置換する
			*colon = '\0';

			// minpos にはコロン左側の文字列を入れる
			// maxpos にはコロン右側の文字列を入れる
			const char *minpos = tmp;
			const char *maxpos = colon + 1;

			// セル番号から行列インデックスを得る
			int L=0, R=0, T=0, B=0;
			if (parse_cell_position(minpos, &L, &T) && parse_cell_position(maxpos, &R, &B)) {
				if (col) *col = L;
				if (row) *row = T;
				if (colcount) *colcount = R - L + 1;
				if (rowcount) *rowcount = B - T + 1;
				return true;
			}
		}

		// インデックスを得られなかった場合は自力で取得する
		//
		// 条件は分からないが、Libre Office Calc でシートを保存したとき、
		// 実際の範囲は "A1:M60" であるにもかかわらず "A1:AMJ60" といった記述になることがあったため。
		// 念のために自力での取得コードも書いておく。
		const KXmlElement *sheet_xml = xroot->findNode("sheetData");
		if (sheet_xml) {
			CScanDim dim;
			scan_cells(sheet_xml, &dim);
			if (col) *col = dim.col0_;
			if (row) *row = dim.row0_;
			if (colcount) *colcount = dim.col1_ - dim.col0_ + 1;
			if (rowcount) *rowcount = dim.row1_ - dim.row0_ + 1;
			return true;
		}

		return false;
	}
	const char * getDataString(int sheet, int col, int row) const {
		if (sheet < 0 || col < 0 || row < 0) return nullptr;
		const KXmlElement *s = get_sheet_xml(sheet);
		const KXmlElement *r = get_row_xml(s, row);
		const KXmlElement *c = get_cell_xml(r, col);
		return get_cell_text(c);
	}
	bool getCellByText(int sheet, const char *s, int *col, int *row) const {
		if (sheet < 0) return false;
		if (s==nullptr || s[0]=='\0') return false;

		// シートを選択
		const KXmlElement *sheet_xml = get_sheet_xml(sheet);
		if (sheet_xml == nullptr) return false;

		// 同じ文字列を持つセルを探す
		const KXmlElement *c_xml = find_cell(sheet_xml, s);
		if (c_xml == nullptr) return false;

		K_assert(col);
		K_assert(row);

		// 見つかったセルの行列番号を得る
		int icol = -1;
		int irow = -1;
		const char *r = c_xml->findAttr("r");
		parse_cell_position(r, &icol, &irow);

		if (icol >= 0 && irow >= 0) {
			*col = icol;
			*row = irow;
			return true;
		}
		return false;
	}
	void scanCells(int sheet, KExcelScanCellsCallback *cb) const {
		if (sheet < 0) return;
		if (sheet >= (int)m_worksheets.size()) return;

		const KXmlElement *doc = m_worksheets[sheet];
		K_assert(doc);

		const KXmlElement *root_elm = doc->getNode(0);
		K_assert(root_elm);

		const KXmlElement *sheet_xml = root_elm->findNode("sheetData");
		if (sheet_xml) {
			scan_cells(sheet_xml, cb);
		}
	}
	bool loadFromFile(KInputStream &file, const char *xlsx_name) {
		m_row_elements.clear();

		if (!file.isOpen()) {
			KLog::printError("E_INVALID_ARGUMENT");
			return false;
		}

		// XMLS ファイルを ZIP として開く
		bool ok = false;
		{
			KUnzipper zr(file);
			ok = loadFromZipAsXlsx(zr, xlsx_name);
		}
		return ok;
	}

	bool loadFromZipAsXlsx(KUnzipper &zr, const char *xlsx_name) {
		// 文字列テーブルを取得
		const KXmlElement *strings_doc = kk__LoadXmlFromZip(zr, xlsx_name, "xl/sharedStrings.xml");
		if (strings_doc) {
			int stringId = 0;
			const KXmlElement *string_elm = strings_doc->getNode(0);

			for (int si=0; si<string_elm->getNodeCount(); si++) {
				const KXmlElement *si_elm = string_elm->getNode(si);
				if (!si_elm->hasTag("si")) continue;

				// パターンA
				// <si>
				//   <t>テキスト</t>
				// </si>
				const KXmlElement *t_elm = si_elm->findNode("t");
				if (t_elm) {
					const char *s = t_elm->getText("");
					m_strings[stringId] = s;
					stringId++;
					continue;
				}

				// パターンB（テキストの途中でスタイル変更がある場合にこうなる？）
				// <si>
				//   <rPr>スタイル情報いろいろ</rPr>
				//   <r><t>テキスト1</t></r>
				//   <r><t>テキスト2</t></r>
				//   ....
				// </si>
				std::string s;
				for (int r=0; r<si_elm->getNodeCount(); r++) {
					const KXmlElement *r_elm = si_elm->getNode(r);
					if (r_elm->hasTag("r")) {
						const KXmlElement *t_xml = r_elm->findNode("t");
						if (t_xml) {
							s += t_xml->getText("");
						}
					}
				}
				if (!s.empty()) {
					m_strings[stringId] = s;
					stringId++;
					continue;
				}

				// その他のパターン。解析できず。
				// 文字列IDだけを進めておく
				stringId++;
			}
			strings_doc->drop();
		}

		// ワークシート情報を取得
		m_workbook_doc = kk__LoadXmlFromZip(zr, xlsx_name, "xl/workbook.xml");

		// ワークシートを取得
		for (int i=1; ; i++) {
			char s[256];
			sprintf_s(s, sizeof(s), "xl/worksheets/sheet%d.xml", i);
			if (kk__findindex(zr, s) < 0) break;
			KXmlElement *sheet_doc = kk__LoadXmlFromZip(zr, xlsx_name, s);
			if (sheet_doc == nullptr) {
				KLog::printError("E_XLSX: Failed to load document: '%s' in xlsx file.", s);
				break;
			}
			m_worksheets.push_back(sheet_doc);
		}

		// 必要な情報がそろっていることを確認
		if (m_workbook_doc == nullptr || m_worksheets.empty()) {
			KLog::printError("E_FAILED_TO_READ_EXCEL_ARCHIVE: %s", xlsx_name);
			clear();
			return false;
		}

		m_filename = xlsx_name;
		return true;
	}

private:
	const char * get_cell_text(const KXmlElement *cell_xml) const {
		Type tp;
		const char *val = get_cell_raw_data(cell_xml, &tp);
		if (val == nullptr) {
			return "";
		}
		if (tp == TP_STRINGID) {
			// val は文字列ID (整数) を表している。
			// 対応する文字列を文字列テーブルから探す
			int sid = KStringUtils::toInt(val);
			K_assert(sid >= 0);
			auto it = m_strings.find(sid);
			if (it != m_strings.end()) {
				return it->second.c_str();
			}
			return "";
		}
		if (tp == TP_LITERAL) {
			// val は文字列そのものを表している
			return val;
		}
		// それ以外のデータの場合は val の文字列をそのまま返す
		return val;
	}

	const KXmlElement *find_cell(const KXmlElement *sheet_xml, const char *s) const {
		if (sheet_xml == nullptr) return nullptr;
		if (s==nullptr || s[0]=='\0') return nullptr;

		for (int r=0; r<sheet_xml->getNodeCount(); r++) {
			const KXmlElement *xRow = sheet_xml->getNode(r);
			if (!xRow->hasTag("row")) continue;

			for (int c=0; c<xRow->getNodeCount(); c++) {
				const KXmlElement *xCell = xRow->getNode(c);
				if (!xCell->hasTag("c")) continue;

				const char *str = get_cell_text(xCell);
				if (strcmp(str, s) == 0) {
					return xCell;
				}
			}
		}
		return nullptr;
	}

	// "A1" や "AB12" などのセル番号から、行列インデックスを取得する
	bool parse_cell_position(const char *ss, int *col, int *row) const {
		return KExcel::decodeCellName(ss, col, row);
	}
	void scan_cells(const KXmlElement *sheet_xml, KExcelScanCellsCallback *cb) const {
		if (sheet_xml == nullptr) return;

		for (int r=0; r<sheet_xml->getNodeCount(); r++) {
			const KXmlElement *xRow = sheet_xml->getNode(r);
			if (!xRow->hasTag("row")) continue;

			for (int c=0; c<xRow->getNodeCount(); c++) {
				const KXmlElement *xCell = xRow->getNode(c);
				if (!xCell->hasTag("c")) continue;

				const char *pos = xCell->findAttr("r");
				int cidx = -1;
				int ridx = -1;
				if (parse_cell_position(pos, &cidx, &ridx)) {
					K_assert(cidx >= 0 && ridx >= 0);
					const char *val = get_cell_text(xCell);
					cb->onCell(cidx, ridx, val);
				}
			}
		}
	}
	const KXmlElement * get_sheet_xml(int sheet) const {
		if (sheet < 0) return nullptr;
		if (sheet >= (int)m_worksheets.size()) return nullptr;

		const KXmlElement *doc = m_worksheets[sheet];
		K_assert(doc);

		const KXmlElement *root_elm = doc->getNode(0);
		K_assert(root_elm);

		return root_elm->findNode("sheetData");
	}
	const KXmlElement * get_row_xml(const KXmlElement *sheet_xml, int row) const {
		if (sheet_xml == nullptr) return nullptr;
		if (row < 0) return nullptr;
		// キャッシュからから探す
		if (! m_row_elements.empty()) {
			// シートを得る
			Sheet_RowElms::const_iterator sheet_it = m_row_elements.find(sheet_xml);
			if (sheet_it != m_row_elements.end()) {
				// 行を得る
				Int_Elms::const_iterator row_it = sheet_it->second.find(row);
				if (row_it != sheet_it->second.end()) {
					return row_it->second;
				}
				// 指定行が存在しない
				return nullptr;
			}
			// シートがまだキャッシュ化されていない。作成する
		}

		// キャッシュから見つからないなら、キャッシュを作りつつ目的のデータを探す
		const KXmlElement *ret = nullptr;
		for (const KXmlElement *it=sheet_xml->findNode("row"); it!=nullptr; it=sheet_xml->findNode("row", it)) {
			int val = it->findAttrInt("r");
			if (val >= 1) {
				int r = val - 1;
				m_row_elements[sheet_xml][r] = it;
				if (r == row) {
					ret = it;
				}
			}
		}
		return ret;
	}
	const KXmlElement * get_cell_xml(const KXmlElement *row_xml, int col) const {
		const int NUM_ALPHABET = 26; // A-Z
		if (row_xml == nullptr) return nullptr;
		if (col < 0) return nullptr;

		for (int c=0; c<row_xml->getNodeCount(); c++) {
			const KXmlElement *c_elm = row_xml->getNode(c);
			if (!c_elm->hasTag("c")) continue;

			const char *s = c_elm->findAttr("r");
			int col_idx = -1;
			parse_cell_position(s, &col_idx, nullptr);
			if (col_idx == col) {
				return c_elm;
			}
		}
		return nullptr;
	}

	// 無変換のセルデータを得る。得られた文字列が何を表しているかは type によって異なる
	const char * get_cell_raw_data(const KXmlElement *cell_xml, Type *type) const {
		if (cell_xml == nullptr) return nullptr;
		K_assert(type);
		// cell_xml の入力例:
		// <c r="B1" s="0" t="n">
		// 	<v>12</v>
		// </c>

		// データ型
		const char *t = cell_xml->findAttr("t");
		if (t == nullptr) return nullptr;

		if (strcmp(t, "n") == 0) {
			*type = TP_NUMBER; // <v> には数値が指定されている
		} else if (strcmp(t, "s") == 0) {
			*type = TP_STRINGID; // <v> には文字列ＩＤが指定されている
		} else if (strcmp(t, "str") == 0) {
			*type = TP_LITERAL; // <v> には文字列が直接指定されている
		} else { 
			*type = TP_OTHER; // それ以外の値が入っている
		}
		// データ文字列
		const KXmlElement *v_elm = cell_xml->findNode("v");
		if (v_elm == nullptr) return nullptr;
		return v_elm->getText();
	}

	class CScanDim: public KExcelScanCellsCallback {
	public:
		int col0_, col1_, row0_, row1_;

		CScanDim() {
			col0_ = -1;
			col1_ = -1;
			row0_ = -1;
			row1_ = -1;
		}
		virtual void onCell(int col, int row, const char *s) override {
			if (col0_ < 0) {
				col0_ = col;
				col1_ = col;
				row0_ = row;
				row1_ = row;
			} else {
				col0_ = KMath::min(col0_, col);
				col1_ = KMath::max(col1_, col);
				row0_ = KMath::min(row0_, row);
				row1_ = KMath::max(row1_, row);
			}
		}
	};
};
#pragma endregion // KExcel Utils

#pragma region KExcelFile
KExcelFile::KExcelFile() {
	m_impl = std::make_shared<CCoreExcelReader>();
}
bool KExcelFile::empty() const {
	return m_impl->empty();
}
const KPath & KExcelFile::getFileName() const {
	m_name = m_impl->getFileName();
	return m_name;
}
bool KExcelFile::loadFromFile(KInputStream &file, const char *xlsx_name) {
	return m_impl->loadFromFile(file, xlsx_name);
}
bool KExcelFile::loadFromFileName(const char *name) {
	bool ok = false;
	KInputStream file = KInputStream::fromFileName(name);
	if (file.isOpen()) {
		ok = m_impl->loadFromFile(file, name);
	}
	if (!ok) {
		m_impl->clear();
	}
	return ok;
}
bool KExcelFile::loadFromMemory(const void *bin, size_t size, const char *name) {
	bool ok = false;
	KInputStream file = KInputStream::fromMemory(bin, size);
	if (file.isOpen()) {
		ok = m_impl->loadFromFile(file, name);
	}
	if (!ok) {
		m_impl->clear();
	}
	return ok;
}
int KExcelFile::getSheetCount() const {
	return m_impl->getSheetCount();
}
int KExcelFile::getSheetByName(const char *name) const {
	return m_impl->getSheetByName(name);
}
KPath KExcelFile::getSheetName(int sheet) const {
	return m_impl->getSheetName(sheet);
}
bool KExcelFile::getSheetDimension(int sheet, int *col, int *row, int *colcount, int *rowcount) const {
	return m_impl->getSheetDimension(sheet, col, row, colcount, rowcount);
}
const char * KExcelFile::getDataString(int sheet, int col, int row) const {
	return m_impl->getDataString(sheet, col, row);
}
bool KExcelFile::getCellByText(int sheet, const char *s, int *col, int *row) const {
	return m_impl->getCellByText(sheet, s, col, row);
}
void KExcelFile::scanCells(int sheet, KExcelScanCellsCallback *cb) const {
	m_impl->scanCells(sheet, cb);
}
#pragma endregion // KExcelFile








#pragma region KTable
class KTable::Impl {
	KExcelFile m_excel;
	KPathList m_colnames; // 列の名前
	int m_sheet;   // 選択中のテーブルを含んでいるシートのインデックス
	int m_leftcol;   // テーブルの左端の列インデックス
	int m_toprow;    // テーブルの開始マークのある行インデックス
	int m_bottomrow; // テーブルの終端マークのある行インデックス
public:
	Impl() {
		m_sheet = -1;
		m_leftcol = 0;
		m_toprow = 0;
		m_bottomrow = 0;
	}
	~Impl() {
		_unload();
	}
	bool empty() const {
		return m_excel.empty();
	}
	bool _loadFromExcelFile(const KExcelFile &file) {
		m_excel = file;
		return !file.empty();
	}
	void _unload() {
		m_excel = KExcelFile(); // claer
		m_sheet = -1;
		m_leftcol = 0;
		m_toprow = 0;
		m_bottomrow = 0;
	}
	bool _selectTable(const char *sheet_name, const char *top_cell_text, const char *bottom_cell_text) {
		m_sheet = -1;
		m_leftcol = 0;
		m_toprow = 0;
		m_bottomrow = 0;

		if (m_excel.empty()) {
			KLog::printError("E_EXCEL: Null data");
			return false;
		}

		int sheet = 0;
		int col0 = 0;
		int col1 = 0;
		int row0 = 0;
		int row1 = 0;

		// シートを探す
		sheet = m_excel.getSheetByName(sheet_name);
		if (sheet < 0) {
			KLog::printError(u8"E_EXCEL: シート '%s' が見つかりません", sheet_name);
			return false;
		}

		// 開始セルを探す
		if (! m_excel.getCellByText(sheet, top_cell_text, &col0, &row0)) {
			KLog::printError(u8"E_EXCEL_MISSING_TABLE_BEGIN: シート '%s' にはテーブル開始セル '%s' がありません", 
				sheet_name, top_cell_text);
			return false;
		}
	//	KLog::printVerbose("TOP CELL '%s' FOUND AT %s", top_cell_text.u8(), KExcel::encodeCellName(col0, row0).u8());

		// セルの定義範囲
		int dim_row_top = 0;
		int dim_row_cnt = 0;
		if (! m_excel.getSheetDimension(sheet, nullptr, &dim_row_top, nullptr, &dim_row_cnt)) {
			KLog::printError(u8"E_EXCEL_MISSING_SHEET_DIMENSION: シート '%s' のサイズが定義されていません", sheet_name);
			return false;
		}

		// 終了セルを探す
		// 終了セルは開始セルと同じ列で、開始セルよりも下の行にあるはず
		for (int i=row0+1; i<dim_row_top+dim_row_cnt; i++) {
			const char *s = m_excel.getDataString(sheet, col0, i);
			if (strcmp(bottom_cell_text, s) == 0) {
				row1 = i;
				break;
			}
		}
		if (row1 == 0) {
			KPath cell = KExcel::encodeCellName(col0, row0);
			KLog::printError(u8"E_EXCEL_MISSING_TABLE_END: シート '%s' のセル '%s' に対応する終端セル '%s' が見つかりません",
				sheet_name, top_cell_text, bottom_cell_text);
			return false;
		}
		KLog::printVerbose("BOTTOM CELL '%s' FOUND AT %s", bottom_cell_text, KExcel::encodeCellName(col0, row0).u8());

		// 開始セルの右隣からは、カラム名の定義が続く
		KPathList cols;
		{
			int c = col0 + 1;
			while (1) {
				KPath cellstr = m_excel.getDataString(sheet, c, row0);
				if (cellstr.empty()) break;
				cols.push_back(cellstr);
				KLog::printVerbose("ID CELL '%s' FOUND AT %s", cellstr.u8(), KExcel::encodeCellName(col0, row0).u8());
				c++;
			}
			if (cols.empty()) {
				KLog::printError(u8"E_EXCEL_MISSING_COLUMN_HEADER: シート '%s' のテーブル '%s' にはカラムヘッダがありません", 
					sheet_name, top_cell_text);
			}
			col1 = c;
		}

		// テーブル読み取りに成功した
		m_colnames = cols;
		m_sheet  = sheet;
		m_toprow    = row0;
		m_bottomrow = row1;
		m_leftcol   = col0;
		return true;
	}
	KPath getColumnName(int col) const {
		if (m_excel.empty()) {
			KLog::printError("E_EXCEL: No table loaded");
			return KPath::Empty;
		}
		if (m_sheet < 0) {
			KLog::printError("E_EXCEL: No table selected");
			return KPath::Empty;
		}
		if (col < 0 || (int)m_colnames.size() <= col) {
			return KPath::Empty;
		}
		return m_colnames[col];
	}
	int getDataColIndexByName(const KPath &column_name) const {
		if (m_excel.empty()) {
			KLog::printError("E_EXCEL: No table loaded");
			return -1;
		}
		if (m_sheet < 0) {
			KLog::printError("E_EXCEL: No table selected");
			return -1;
		}
		for (size_t i=0; i<m_colnames.size(); i++) {
			if (m_colnames[i].compare(column_name) == 0) {
				return (int)i;
			}
		}
		return -1;
	}
	int getDataColCount() const {
		if (m_excel.empty()) {
			KLog::printError("E_EXCEL: No table loaded");
			return 0;
		}
		if (m_sheet < 0) {
			KLog::printError("E_EXCEL: No table selected");
			return 0;
		}
		return (int)m_colnames.size();
	}
	int getDataRowCount() const {
		if (m_excel.empty()) {
			KLog::printError("E_EXCEL: No table loaded");
			return 0;
		}
		if (m_sheet < 0) {
			KLog::printError("E_EXCEL: No table selected");
			return 0;
		}
		// 開始行と終了行の間にある行数
		int rows = m_bottomrow - m_toprow - 1;
		K_assert(rows > 0);

		return rows;
	}
	const char * getRowMarker(int data_row) const {
		if (m_excel.empty()) {
			KLog::printError("E_EXCEL: No table loaded");
			return nullptr;
		}
		if (m_sheet < 0) {
			KLog::printError("E_EXCEL: No table selected");
			return nullptr;
		}
		if (data_row < 0) return nullptr;

		int col = m_leftcol; // 行マーカーは一番左の列にあるものとする。（一番左のデータ列の、さらにひとつ左側）
		int row = m_toprow + 1 + data_row;
		return m_excel.getDataString(m_sheet, col, row);
	}
	const char * getDataString(int data_col, int data_row) const {
		if (m_excel.empty()) {
			KLog::printError("E_EXCEL: No table loaded");
			return nullptr;
		}
		if (m_sheet < 0) {
			KLog::printError("E_EXCEL: No table selected");
			return nullptr;
		}
		if (data_col < 0) return nullptr;
		if (data_row < 0) return nullptr;

		// 一番左の列 (m_leftcol) は開始・終端キーワードを置くためにある。
		// ほかに文字が入っていてもコメント扱いで無視される。
		// 実際のデータはその右隣の列から開始する
		if (data_col >= m_leftcol + 1 + (int)m_colnames.size()) return nullptr;
		int col = m_leftcol + 1 + data_col;

		// 一番上の行（m_toprow) は開始キーワードとカラム名を書くためにある。
		// 実際のデータ行はそのひとつ下から始まる
		if (data_row >= m_bottomrow) return nullptr;
		int row = m_toprow + 1 + data_row;

		return m_excel.getDataString(m_sheet, col, row);
	}
	int getDataInt(int data_col, int data_row, int def) const {
		const char *s = getDataString(data_col, data_row);
		// 実数形式で記述されている値を整数として取り出す可能性
		float f = 0.0f;
		if (KStringUtils::toFloatTry(s, &f)) {
			return (int)f;
		}
		// 8桁の16進数を取り出す可能性。この場合は符号なしで取り出しておかないといけない
		unsigned int u = 0;
		if (KStringUtils::toUintTry(s, &u)) {
			return (int)u;
		}
		// 普通の整数として取り出す
		int i = 0;
		if (KStringUtils::toIntTry(s, &i)) {
			return i;
		}
		return def;
	}
	float getDataFloat(int data_col, int data_row, float def) const {
		const char *s = getDataString(data_col, data_row);
		return KStringUtils::toFloat(s, def);
	}
	bool getDataSource(int data_col, int data_row, int *col_in_file, int *row_in_file) const {
		if (data_col < 0) return false;
		if (data_row < 0) return false;
		if (col_in_file) *col_in_file = m_leftcol + 1 + data_col;
		if (row_in_file) *row_in_file = m_toprow  + 1 + data_row;
		return true;
	}
};


KTable::KTable() {
	m_impl = nullptr;
}
bool KTable::empty() const {
	return m_impl->empty();
}
bool KTable::loadFromExcelFile(const KExcelFile &excel, const char *sheetname, const char *top_cell_text, const char *btm_cell_text) {
	auto impl = std::make_shared<Impl>();
	if (impl->_loadFromExcelFile(excel)) {
		const char *top = (top_cell_text && top_cell_text[0]) ? top_cell_text : "@BEGIN";
		const char *btm = (btm_cell_text && btm_cell_text[0]) ? btm_cell_text : "@END";
		if (impl->_selectTable(sheetname, top, btm)) {
			m_impl = impl;
			return true;
		}
	}
	m_impl = nullptr;
	return false;
}
bool KTable::loadFromFile(KInputStream &xmls, const char *filename, const char *sheetname, const char *top_cell_text, const char *btm_cell_text) {
	KExcelFile excel;
	if (excel.loadFromFile(xmls, filename)) {
		if (loadFromExcelFile(excel, sheetname, top_cell_text, btm_cell_text)) {
			return true;
		}
	}
	m_impl = nullptr;
	return false;
}
bool KTable::loadFromExcelMemory(const void *xlsx_bin, size_t xlsx_size, const char *filename, const char *sheetname, const char *top_cell_text, const char *btm_cell_text) {
	if (xlsx_bin && xlsx_size > 0) {
		KExcelFile excel;
		if (excel.loadFromMemory(xlsx_bin, xlsx_size, filename)) {
			if (loadFromExcelFile(excel, sheetname, top_cell_text, btm_cell_text)) {
				return true;
			}
		}
	}
	m_impl = nullptr;
	return false;
}

int KTable::getDataColIndexByName(const char *column_name) const {
	if (m_impl) {
		return m_impl->getDataColIndexByName(column_name);
	}
	return 0;
}
KPath KTable::getColumnName(int col) const {
	if (m_impl) {
		return m_impl->getColumnName(col);
	}
	return KPath::Empty;
}
int KTable::getDataColCount() const {
	if (m_impl) {
		return m_impl->getDataColCount();
	}
	return 0;
}
int KTable::getDataRowCount() const {
	if (m_impl) {
		return m_impl->getDataRowCount();
	}
	return 0;
}
const char * KTable::getRowMarker(int row) const {
	if (m_impl) {
		return m_impl->getRowMarker(row);
	}
	return nullptr;
}
const char * KTable::getDataString(int col, int row) const {
	if (m_impl) {
		return m_impl->getDataString(col, row);
	}
	return nullptr;
}
int KTable::getDataInt(int col, int row, int def) const {
	if (m_impl) {
		return m_impl->getDataInt(col, row, def);
	}
	return def;
}
float KTable::getDataFloat(int col, int row, float def) const {
	if (m_impl) {
		return m_impl->getDataFloat(col, row, def);
	}
	return def;
}
bool KTable::getDataSource(int col, int row, int *col_in_file, int *row_in_file) const {
	if (m_impl && m_impl->getDataSource(col, row, col_in_file, row_in_file)) {
		return true;
	}
	return false;
}
#pragma endregion // KTable



#pragma region KNamedValues
#if NV_TEST
class CNamedValuesImpl: public KNamedValues {
	typedef std::pair<KPath, std::string> PAIR;
	std::vector<PAIR> mItems;
public:
	virtual size_t size() const override {
		return mItems.size();
	}
	virtual void clear() {
		mItems.clear();
	}
	virtual void remove(const char *name) {
		int index = find(name);
		if (index >= 0) {
			mItems.erase(mItems.begin() + index);
		}
	}
	virtual bool loadFromString(const char *xml_u8, const char *filename) override {
		clear();

		bool result = false;
		char errmsg[1024] = {0};
		KXmlReader *xml = K_createXmlReader(xml_u8, filename, errmsg, sizeof(errmsg));
		if (xml) {
			if (xml->enterFirstChild()) {
				result = loadFromXml(xml, false);
				xml->exitChild();
			}
			xml->drop();
		} else {
			KLog::printError("E_XML: %s", errmsg);
			KLog::printError("E_XML: Failed to load: %s", filename);
		}

		return result;
	}
	virtual bool loadFromXml(KXmlReader *xml, bool packed_in_attr=false) override {
		clear();
		if (packed_in_attr) {
			// <XXX AAA="BBB" CCC="DDD" EEE="FFF" ... >
			int numAttrs = xml->getAttrCount();
			for (int i=0; i<numAttrs; i++) {
				const char *key = xml->getAttrName(i);
				const char *val = xml->getAttrValue(i);
				if (key && val) {
					setString(key, val);
				}
			}
		} else {
			// <XXX name="AAA">BBB</XXX>/>
			if (xml->enterFirstChild()) {
				for (; xml->isValid(); xml->nextSibling()) {
					const char *key = xml->getAttrValue("name");
					const char *val = xml->getText();
					if (key && val) {
						setString(key, val);
					}
				}
				xml->exitChild();
			}
		}
		return true;
	}
	virtual std::string saveToString(bool pack_in_attr=false) const override {
		std::string s;
		if (pack_in_attr) {
			s += "<KNamedValues\n";
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				s += K_sprintf("\t%s = '%s'\n", it->first.u8(), it->second.c_str());
			}
			s += "/>\n";
		} else {
			s += "<KNamedValues>\n";
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				s += K_sprintf("  <Pair name='%s'>%s</Pair>\n", it->first.u8(), it->second.c_str());
			}
			s += "</KNamedValues>\n";
		}
		return s;
	}
	virtual const char * getName(int index) const override {
		if (0 <= index && index < (int)mItems.size()) {
			return mItems[index].first.u8();
		} else {
			return nullptr;
		}
	}
	virtual void setString(const char *name, const char *value) override {
		if (name && name[0]) {
			int index = find(name);
			if (index >= 0) {
				mItems[index].second = value ? value : "";
			} else {
				mItems.push_back(PAIR(name, value));
			}
		}
	}
	virtual const char * getString(int index) const override {
		if (0 <= index && index < (int)mItems.size()) {
			return mItems[index].second.c_str();
		} else {
			return nullptr;
		}
	}
};
KNamedValues * KNamedValues::create() {
	return new CNamedValuesImpl();
}
#else
KNamedValues * KNamedValues::create() {
	return new KNamedValues();
}
#endif
#pragma endregion // KNamedValues



#pragma region KOrderedParameters
KOrderedParameters::KOrderedParameters() {
}
const KOrderedParameters::Item * KOrderedParameters::_get(const KPath &key) const {
	auto it = m_key_index_map.find(key);
	if (it != m_key_index_map.end()) {
		int index = it->second;
		return &m_items[index];
	}
	return nullptr;
}
const KOrderedParameters::Item * KOrderedParameters::_at(int index) const {
	K_assert(0 <= index && index < (int)m_items.size());
	return &m_items[index];
}
void KOrderedParameters::clear() {
	m_items.clear();
}
int KOrderedParameters::size() const {
	return (int)m_items.size();
}
void KOrderedParameters::_add(const Item &item) {
	m_key_index_map[item.key] = (int)m_items.size(); // new index
	m_items.push_back(item);
}
bool KOrderedParameters::contains(const KPath &key) const {
	return _get(key) != nullptr;
}
void KOrderedParameters::add(const KPath &key, const KPath &val) {
	if (key.empty()) {
		K_assert(0);//M_error("E_INVALID_KEY: empty");
		return;
	}
	Item item;
	item.key = key;
	item.val = val;
	_add(item);
}
void KOrderedParameters::add(const KPath &key, int val) {
	if (key.empty()) {
		K_assert(0);//M_error("E_INVALID_KEY: empty");
		return;
	}
	Item item;
	item.key = key;
	item.val = KPath::fromFormat("%d", val);
	_add(item);
}
void KOrderedParameters::add(const KPath &key, float val) {
	if (key.empty()) {
		K_assert(0);//M_error("E_INVALID_KEY: empty");
		return;
	}
	Item item;
	item.key = key;
	item.val = KPath::fromFormat("%g", val);
	_add(item);
}
void KOrderedParameters::add(const KOrderedParameters &other) {
	for (int i=0; i<other.size(); i++) {
		KPath key;
		KPath val;
		other.getParameter(i, &key, &val);
		add(key, val);
	}
}
bool KOrderedParameters::queryString(const KPath &key, KPath *val) const {
	const Item *item = _get(key);
	if (item) {
		if (val) *val = item->val;
		return true;
	}
	return false;
}
bool KOrderedParameters::queryInteger(const KPath &key, int *val) const {
	const Item *item = _get(key);
	if (item) {
		if (val) *val = KStringUtils::toInt(item->val.u8());
		return true;
	}
	return false;
}
bool KOrderedParameters::queryFloat(const KPath &key, float *val) const {
	const Item *item = _get(key);
	if (item) {
		if (val) *val = KStringUtils::toFloat(item->val.u8());
		return true;
	}
	return false;
}
bool KOrderedParameters::getParameter(int index, KPath *key, KPath *val) const {
	if (0 <= index && index < size()) {
		const Item *item = _at(index);
		K_assert(item);
		if (key) *key = item->key;
		if (val) *val = item->val;
		return true;
	}
	return false;
}
#pragma endregion // KOrderedParameters



#pragma endregion // Table.cpp














#pragma region KClock

#pragma region KClock static functions
uint64_t KClock::getSystemTimeNano64() {
	return K__ClockNano64();
}
uint64_t KClock::getSystemTimeMsec64() {
	uint64_t nano = getSystemTimeNano64();
	return nano / 1000000;
}
int KClock::getSystemTimeMsec() {
	return (int)getSystemTimeMsec64();
}
float KClock::getSystemTimeMsecF() {
	uint64_t nano = getSystemTimeNano64();
	double msec = nano / 1000000.0;
	return (float)msec;
}
int KClock::getSystemTimeSec() {
	uint64_t nano = getSystemTimeNano64();
	return (int)nano / 1000000000;
}
float KClock::getSystemTimeSecF() {
	return getSystemTimeMsecF() / 1000.0f;
}
#pragma endregion // KClock static functions


#pragma region KClock methods
KClock::KClock() {
	m_start = getSystemTimeNano64();
}
void KClock::reset() {
	m_start = getSystemTimeNano64();
}
uint64_t KClock::getTimeNano64() const {
	const uint64_t t = getSystemTimeNano64();
	return t - m_start;
}
uint64_t KClock::getTimeMsec64() const {
	uint64_t t = getTimeNano64();
	return t / 1000000;
}
int KClock::getTimeNano() const {
	uint64_t t = getTimeNano64();
	return (int)(t & 0x7FFFFFFF);
}
int KClock::getTimeMsec() const {
	uint64_t t = getTimeMsec64();
	return (int)(t & 0x7FFFFFFF);
}
float KClock::getTimeMsecF() const {
	const double MS = 1000.0 * 1000.0;
	uint64_t ns = getTimeNano64();
	return (float)((double)ns / MS);
}
float KClock::getTimeSecondsF() const {
	const double NS = 1000.0 * 1000.0 * 1000.0;
	uint64_t ns = getTimeNano64();
	return (float)((double)ns / NS);
}
#pragma endregion // KClock methods

#pragma endregion // KClock









#pragma region KLuaBank.cpp
#pragma region KLuaBank
KLuaBank::KLuaBank() {
	m_cb = nullptr;
}
KLuaBank::~KLuaBank() {
	clear();
}
void KLuaBank::setCallback(KLuaBankCallback *cb) {
	m_cb = cb;
}
lua_State * KLuaBank::addEmptyScript(const char *name) {
	KLog::printDebug("LUA_BANK: new script '%s'", name);
	lua_State *ls = luaL_newstate();
	luaL_openlibs(ls);
	m_mutex.lock();
	{
		if (m_items.find(name) != m_items.end()) {
			KLog::printWarning("W_SCRIPT_OVERWRITE: Resource named '%s' already exists. The resource data will be overwriten by new one", name);
			this->remove(name);
		}
		K_assert(m_items[name] == nullptr);
		m_items[name] = ls;
	}
	m_mutex.unlock();
	return ls;
}
lua_State * KLuaBank::findScript(const char *name) const {
	lua_State *ret;
	m_mutex.lock();
	{
		auto it = m_items.find(name);
		ret = (it!=m_items.end()) ? it->second : nullptr;
	}
	m_mutex.unlock();
	return ret;
}
bool KLuaBank::addScript(const char *name, const char *code, size_t size) {
	if (KStringUtils::isEmpty(name)) {
		KLog::printError("LUA_BANK: Failed to addScript. Invalid name");
		return false;
	}
	if (code == nullptr || code[0] == '\0' || size == 0) {
		KLog::printError("LUA_BANK: Failed to addScript named '%s': Invaliad code or size", name);
		return false;
	}

	lua_State *ls = addEmptyScript(name);
	K_assert(ls);
	// ロードする
	if (luaL_loadbuffer(ls, code, size, name) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		KLog::printError("LUA_BANK: Failed to addScript named '%s': %s", name, msg);
		this->remove(name);
		return false;
	}

	// チャンク実行よりも前にライブラリをインストールしたい時などに使う
	if (m_cb) m_cb->on_luabank_load(ls, name);

	// 再外周のチャンクを実行し、グローバル変数名や関数名を解決する
	if (lua_pcall(ls, 0, 0, 0) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		KLog::printError("LUA_BANK: Failed to addScript named '%s': %s", name, msg);
		this->remove(name);
		return false;
	}
	return true;
}
lua_State * KLuaBank::queryScript(const char *name, bool reload) {
	// 危険。makeThread ですでに派生スレッドを作っていた場合、
	// ここで元ステートを削除してしまうと生成済みの派生スレッドが無効になってしまう。
	#if 1
	if (reload) {
		this->remove(name);
	}
	#endif

	lua_State *ls = findScript(name);
	if (ls == nullptr) {
		std::string bin = KStorage::loadBinary(name);
		if (addScript(name, bin.data(), bin.size())) {
			ls = findScript(name);
		}
	}
	return ls;
}
lua_State * KLuaBank::makeThread(const char *name) {
	lua_State *ls = queryScript(name);
	K_assert(ls);
	return lua_newthread(ls);
}

bool KLuaBank::contains(const char *name) const {
	bool ret;
	m_mutex.lock();
	{
		ret = m_items.find(name) != m_items.end();
	}
	m_mutex.unlock();
	return ret;
}
void KLuaBank::remove(const char *name) {
	m_mutex.lock();
	{
		auto it = m_items.find(name);
		if (it != m_items.end()) {
			lua_State *ls = it->second;
			lua_close(ls);
			KLog::printDebug("LUA_BANK: remove '%s'", name);
			m_items.erase(it);
		}
	}
	m_mutex.unlock();
}
void KLuaBank::clear() {
	m_mutex.lock();
	{
		for (auto it=m_items.begin(); it!=m_items.end(); ++it) {
			lua_State *ls = it->second;
			lua_close(ls);
		}
		m_items.clear();
	}
	m_mutex.unlock();
}
#pragma endregion // KLuaBank
#pragma endregion // KLuaBank.cpp









#pragma region KMath.cpp





#pragma region KRect
KRect::KRect():
	xmin(0),
	ymin(0),
	xmax(0),
	ymax(0)
{
	sort();
}
KRect::KRect(const KRect &rect):
	xmin(rect.xmin),
	ymin(rect.ymin),
	xmax(rect.xmax),
	ymax(rect.ymax)
{
	sort();
}
KRect::KRect(int _xmin, int _ymin, int _xmax, int _ymax):
	xmin((float)_xmin),
	ymin((float)_ymin),
	xmax((float)_xmax),
	ymax((float)_ymax)
{
	sort();
}
KRect::KRect(float _xmin, float _ymin, float _xmax, float _ymax):
	xmin(_xmin),
	ymin(_ymin),
	xmax(_xmax),
	ymax(_ymax) {
	sort();
}
KRect::KRect(const KVec2 &pmin, const KVec2 &pmax):
	xmin(pmin.x),
	ymin(pmin.y),
	xmax(pmax.x),
	ymax(pmax.y) {
	sort();
}
KRect KRect::getOffsetRect(const KVec2 &delta) const {
	return KRect(
		delta.x + xmin,
		delta.y + ymin,
		delta.x + xmax,
		delta.y + ymax);
}
KRect KRect::getOffsetRect(float dx, float dy) const {
	return KRect(
		dx + xmin,
		dy + ymin,
		dx + xmax,
		dy + ymax);
}
void KRect::offset(float dx, float dy) {
	xmin += dx;
	ymin += dy;
	xmax += dx;
	ymax += dy;
}
void KRect::offset(int dx, int dy) {
	xmin += dx;
	ymin += dy;
	xmax += dx;
	ymax += dy;
}
KRect KRect::getInflatedRect(float dx, float dy) const {
	return KRect(xmin - dx, ymin - dy, xmax + dx, ymax + dy);
}
bool KRect::isIntersectedWith(const KRect &rc) const {
	if (xmin > rc.xmax) return false;
	if (xmax < rc.xmin) return false;
	if (ymin > rc.ymax) return false;
	if (ymax < rc.ymin) return false;
	return true;
}
KRect KRect::getIntersectRect(const KRect &rc) const {
	if (isIntersectedWith(rc)) {
		return KRect(
			KMath::max(xmin, rc.xmin),
			KMath::max(ymin, rc.ymin),
			KMath::min(xmax, rc.xmax),
			KMath::min(ymax, rc.ymax)
		);
	} else {
		return KRect();
	}
}
KRect KRect::getUnionRect(const KVec2 &p) const {
	return KRect(
		KMath::min(xmin, p.x),
		KMath::min(ymin, p.y),
		KMath::max(xmax, p.x),
		KMath::max(ymax, p.y)
	);
}
KRect KRect::getUnionRect(const KRect &rc) const {
	return KRect(
		KMath::min(xmin, rc.xmin),
		KMath::min(ymin, rc.ymin),
		KMath::max(xmax, rc.xmax),
		KMath::max(ymax, rc.ymax)
	);
}
bool KRect::isZeroSized() const {
	return KMath::equals(xmin, xmax) && KMath::equals(ymin, ymax);
}
bool KRect::isEqual(const KRect &rect) const {
	return 
		KMath::equals(xmin, rect.xmin) &&
		KMath::equals(xmax, rect.xmax) &&
		KMath::equals(ymin, rect.ymin) &&
		KMath::equals(ymax, rect.ymax);
}
bool KRect::contains(float x, float y) const {
	return (xmin <= x && x < xmax) &&
		   (ymin <= y && y < ymax);
}
bool KRect::contains(int x, int y) const {
	return (xmin <= (float)x && (float)x < xmax) &&
		   (ymin <= (float)y && (float)y < ymax);
}
bool KRect::contains(const KVec2 &p) const {
	return (xmin <= p.x && p.x < xmax) &&
		   (ymin <= p.y && p.y < ymax);
}
KVec2 KRect::getCenter() const {
	return KVec2(
		(xmin + xmax) / 2.0f,
		(ymin + ymax) / 2.0f
	);
}
KVec2 KRect::getMinPoint() const {
	return KVec2(xmin, ymin);
}
KVec2 KRect::getMaxPoint() const {
	return KVec2(xmax, ymax);
}
float KRect::getSizeX() const {
	return xmax - xmin;
}
float KRect::getSizeY() const {
	return ymax - ymin;
}
void KRect::inflate(float dx, float dy) {
	xmin -= dx;
	xmax += dx;
	ymin -= dy;
	ymax += dy;
}
void KRect::unionWith(const KRect &rect, bool unionX, bool unionY) {
	if (unionX) {
		xmin = KMath::min(xmin, rect.xmin);
		xmax = KMath::max(xmax, rect.xmax);
	}
	if (unionY) {
		ymin = KMath::min(ymin, rect.ymin);
		ymax = KMath::max(ymax, rect.ymax);
	}
}
void KRect::unionWithX(float x) {
	if (x < xmin) xmin = x;
	if (x > xmax) xmax = x;
}
void KRect::unionWithY(float y) {
	if (y < ymin) ymin = y;
	if (y > ymax) ymax = y;
}
void KRect::sort() {
	const auto x0 = KMath::min(xmin, xmax);
	const auto x1 = KMath::max(xmin, xmax);
	const auto y0 = KMath::min(ymin, ymax);
	const auto y1 = KMath::max(ymin, ymax);
	xmin = x0;
	ymin = y0;
	xmax = x1;
	ymax = y1;
}
#pragma endregion // KRect




#pragma region KCubicBezier
#pragma region KCubicBezier functions
static KVec3 kk_BezPos(const KVec3 *p4, float t) {
	// http://geom.web.fc2.com/geometry/bezier/cubic.html
	K_assert(p4);
	K_assert(0 <= t && t <= 1);
	float T = 1.0f - t;
	float x = t*t*t*p4[3].x + 3*t*t*T*p4[2].x + 3*t*T*T*p4[1].x + T*T*T*p4[0].x;
	float y = t*t*t*p4[3].y + 3*t*t*T*p4[2].y + 3*t*T*T*p4[1].y + T*T*T*p4[0].y;
	float z = t*t*t*p4[3].z + 3*t*t*T*p4[2].z + 3*t*T*T*p4[1].z + T*T*T*p4[0].z;
	return KVec3(x, y, z);
}
static KVec3 kk_BezTan(const KVec3 *p4, float t) {
	// http://geom.web.fc2.com/geometry/bezier/cubic.html
	K_assert(p4);
	K_assert(0 <= t && t <= 1);
	float T = 1.0f - t;
	float dx = 3*(t*t*(p4[3].x-p4[2].x)+2*t*T*(p4[2].x-p4[1].x)+T*T*(p4[1].x-p4[0].x));
	float dy = 3*(t*t*(p4[3].y-p4[2].y)+2*t*T*(p4[2].y-p4[1].y)+T*T*(p4[1].y-p4[0].y));
	float dz = 3*(t*t*(p4[3].z-p4[2].z)+2*t*T*(p4[2].z-p4[1].z)+T*T*(p4[1].z-p4[0].z));
	return KVec3(dx, dy, dz);
}
// 3次ベジェ曲線を div 個の直線で分割して、その長さを返す
static float kk_BezLen1(const KVec3 *p4, int div) {
	K_assert(p4);
	K_assert(div >= 2);
	float result = 0;
	KVec3 pa = p4[0];
	for (int i=1; i<div; i++) {
		float t = (float)i / (div - 1);
		KVec3 pb = kk_BezPos(p4, t);
		result += (pb - pa).getLength();
		pa = pb;
	}
	return result;
}

// ベジェ曲線を2分割する
static void kk_BezDiv(const KVec3 *p4, KVec3 *out8) {
	K_assert(p4);
	K_assert(out8);
	// 区間1
	out8[0] = p4[0];
	out8[1] = (p4[0] + p4[1]) / 2;
	out8[2] = (p4[0] + p4[1]*2 + p4[2]) / 4;
	out8[3] = (p4[0] + p4[1]*3 + p4[2]*3 + p4[3]) / 8;
	// 区間2
	out8[4] = (p4[0] + p4[1]*3 + p4[2]*3 + p4[3]) / 8;
	out8[5] = (p4[1] + p4[2]*2 + p4[3]) / 4;
	out8[6] = (p4[2] + p4[3]) / 2;
	out8[7] = p4[3];
}

// 直線 ab で b 方向を見たとき、点(px, py)が左にあるなら正の値を、右にあるなら負の値を返す
// z 値は無視する
static float kk_IsLeft2D(const KVec3 &p, const KVec3 &a, const KVec3 &b) {
	KVec3 ab = b - a;
	KVec3 ap = p - a;
	return ab.x * ap.y - ab.y * ap.x;
}

// 直線 ab と点 p の符号付距離^2。
// 点 a から b を見た時に、点 p が左右どちらにあるかで符号が変わる。正の値なら左側にある
// z 値は無視する
static float kk_DistSq2D(const KVec3 &p, const KVec3 &a, const KVec3 &b) {
	KVec3 ab = b - a;
	KVec3 ap = p - a;
	float cross = ab.x * ap.y - ab.y * ap.x;
	float deltaSq = ab.x * ab.x + ab.y * ab.y;
	return cross * cross / deltaSq;
}

// 3次ベジェ曲線を再帰分割し、その長さを返す
// （未検証）
static float kk_BezLen2(const KVec3 *p4) {
	// 始点と終点
	KVec3 delta = p4[3] - p4[0];
	float delta_len = delta.getLength();

	// 中間点との許容距離を、始点終点の距離の 1/10 に設定する
	float tolerance = delta_len * 0.1f;

	// 曲線中間点 (t = 0.5)
	KVec3 middle = kk_BezPos(p4, 0.5f);

	// 始点＆終点を通る直線と、曲線中間点の距離
	float middle_dist_sq = kk_DistSq2D(middle, p4[0], p4[3]);

	// 始点＆終点を通る直線と、曲線中間点が一定以上離れているなら再分割する
	if (middle_dist_sq >= tolerance) {
		KVec3 pp8[8];
		kk_BezDiv(p4, pp8);
		float len0 = kk_BezLen2(pp8+0); // 前半区間の長さ
		float len1 = kk_BezLen2(pp8+4); // 後半区間の長さ
		return len0 + len1;
	}

	// 始点＆終点を通る直線と、曲線中間点がすごく近い。
	// この場合、曲線がほとんど直線になっているという場合と、
	// 制御点がアルファベットのＺ字型に配置されていて、
	// たまたま曲線中間点が始点＆終点直線に近づいているだけという場合がある
	float left1 = kk_IsLeft2D(p4[1], p4[0], p4[3]); // 始点から終点を見たときの、点1の位置
	float left2 = kk_IsLeft2D(p4[2], p4[0], p4[3]); // 始点から終点を見たときの、点2の位置

	// 直線に対する点1の距離符号と点2の距離符号が異なる場合は
	// 直線を挟んで点1と2が逆方向にある。
	// 制御点がアルファベットのＺ字型に配置されているということなので、再分割する
	if (left1 * left2 < 0) {
		KVec3 pp8[8];
		kk_BezDiv(p4, pp8);
		float len0 = kk_BezLen2(pp8+0); // 前半区間の長さ
		float len1 = kk_BezLen2(pp8+4); // 後半区間の長さ
		return len0 + len1;
	}

	// 曲線がほとんど直線と同じ。
	// 始点と終点の距離をそのまま曲線の距離として返す
	return delta.getLength();
}
#pragma endregion // KCubicBezier functions


#pragma region KCubicBezier methods
KCubicBezier::KCubicBezier() {
	clear();
}
void KCubicBezier::clear() {
	length_ = 0;
	dirty_length_ = true;
	points_.clear();
}
bool KCubicBezier::empty() const {
	return points_.empty();
}
int KCubicBezier::getSegmentCount() const {
	return (int)points_.size() / 4;
}
void KCubicBezier::setSegmentCount(int count) {
	if (count < 0) return;
	points_.resize(count * 4);
	dirty_length_ = true;
}
float KCubicBezier::getWholeLength() const {
	if (dirty_length_) {
		dirty_length_ = false;
		length_ = 0;
		for (int i=0; i<getSegmentCount(); i++) {
			length_ += getLength(i);
		}
	}
	return length_;
}
KVec3 KCubicBezier::getCoordinates(int seg, float t) const {
	KVec3 ret;
	getCoordinates(seg, t, &ret);
	return ret;
}
bool KCubicBezier::getCoordinates(int seg, float t, KVec3 *pos) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (pos) *pos = kk_BezPos(&points_[seg * 4], t);
		return true;
	}
	return false;
}
KVec3 KCubicBezier::getCoordinatesEx(float t) const {
	t = KMath::clamp01(t);
	int numseg = getSegmentCount();
	int seg = (int)(t * numseg);
	float t0 = (float)(seg+0) / numseg;
	float t1 = (float)(seg+1) / numseg;
	float tt = KMath::linearStep(t0, t1, t);
	return getCoordinates(seg, tt);
}
KVec3 KCubicBezier::getTangent(int seg, float t) const {
	KVec3 ret;
	getTangent(seg, t, &ret);
	return ret;
}
bool KCubicBezier::getTangent(int seg, float t, KVec3 *tangent) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (tangent) *tangent = kk_BezTan(&points_[seg * 4], t);
		return true;
	}
	return false;
}
float KCubicBezier::getLength(int seg) const {
	if (0 <= seg && seg < getSegmentCount()) {
		return getLength_Test1(seg, 16);
	}
	return 0.0f;
}
float KCubicBezier::getLength_Test1(int seg, int numdiv) const {
	// 直線分割による近似
	K_assert(0 <= seg && seg < (int)points_.size());
	return kk_BezLen1(&points_[seg * 4], 16);
}
float KCubicBezier::getLength_Test2(int seg) const {
	// ベジェ曲線の再帰分割による近似
	K_assert(0 <= seg && seg < (int)points_.size());
	return kk_BezLen2(&points_[seg * 4]);
}
void KCubicBezier::addSegment(const KVec3 &a, const KVec3 &b, const KVec3 &c, const KVec3 &d) {
	points_.push_back(a);
	points_.push_back(b);
	points_.push_back(c);
	points_.push_back(d);
	dirty_length_ = true;
}
void KCubicBezier::setPoint(int seg, int point, const KVec3 &pos) {
	int i = seg*4 + point;
	if (0 <= i && i < (int)points_.size()) {
		points_[seg*4 + point] = pos;
		dirty_length_ = true;
	}
}
void KCubicBezier::setPoint(int serial_index, const KVec3 &pos) {
	int seg = serial_index / 4;
	int idx = serial_index % 4;
	setPoint(seg, idx, pos);
}
KVec3 KCubicBezier::getPoint(int seg, int point) const {
	int i = seg*4 + point;
	if (0 <= i && i < (int)points_.size()) {
		return points_[seg*4 + point];
	}
	return KVec3();
}
KVec3 KCubicBezier::getPoint(int serial_index) const {
	int seg = serial_index / 4;
	int idx = serial_index % 4;
	return getPoint(seg, idx);
}
int KCubicBezier::getPointCount() const {
	return points_.size();
}
bool KCubicBezier::getFirstAnchor(int seg, KVec3 *anchor, KVec3 *handle) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (anchor) *anchor = getPoint(seg, 0);
		if (handle) *handle = getPoint(seg, 1);
		return true;
	}
	return false;
}
bool KCubicBezier::getSecondAnchor(int seg, KVec3 *handle, KVec3 *anchor) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (handle) *handle = getPoint(seg, 2);
		if (anchor) *anchor = getPoint(seg, 3);
		return true;
	}
	return false;
}
void KCubicBezier::setFirstAnchor(int seg, const KVec3 &anchor, const KVec3 &handle) {
	if (0 <= seg && seg < getSegmentCount()) {
		setPoint(seg, 0, anchor);
		setPoint(seg, 1, handle);
	}
}
void KCubicBezier::setSecondAnchor(int seg, const KVec3 &handle, const KVec3 &anchor) {
	if (0 <= seg && seg < getSegmentCount()) {
		setPoint(seg, 2, handle);
		setPoint(seg, 3, anchor);
	}
}
#pragma endregion // KCubicBezier methods
#pragma endregion // KCubicBezier


#pragma region KCollisionMath
/// 三角形の法線ベクトルを返す
///
/// 三角形法線ベクトル（正規化済み）を得る
/// 時計回りに並んでいるほうを表とする
bool KCollisionMath::getTriangleNormal(KVec3 *result, const KVec3 &o, const KVec3 &a, const KVec3 &b) {
	const KVec3 ao = a - o;
	const KVec3 bo = b - o;
	const KVec3 normal = ao.cross(bo);
	KVec3 nor;
	if (normal.getNormalizedSafe(&nor)) {
		if (result) *result = nor;
		return true;
	}
	// 三角形が線分または点に縮退している。
	// 法線は定義できない
	return false;
}

bool KCollisionMath::isAabbIntersected(const KVec3 &pos1, const KVec3 &halfsize1, const KVec3 &pos2, const KVec3 &halfsize2, KVec3 *intersect_center, KVec3 *intersect_halfsize) {
	const KVec3 span = pos1 - pos2;
	const KVec3 maxspan = halfsize1 + halfsize2;
	if (fabsf(span.x) > maxspan.x) return false;
	if (fabsf(span.y) > maxspan.y) return false;
	if (fabsf(span.z) > maxspan.z) return false;
	const KVec3 min1 = pos1 - halfsize1;
	const KVec3 max1 = pos1 + halfsize1;
	const KVec3 min2 = pos2 - halfsize2;
	const KVec3 max2 = pos2 + halfsize2;
	const KVec3 intersect_min(
		KMath::max(min1.x, min2.x),
		KMath::max(min1.y, min2.y),
		KMath::max(min1.z, min2.z));
	const KVec3 intersect_max(
		KMath::min(max1.x, max2.x),
		KMath::min(max1.y, max2.y),
		KMath::min(max1.z, max2.z));
	if (intersect_center) {
		*intersect_center = (intersect_max + intersect_min) / 2;
	}
	if (intersect_halfsize) {
		*intersect_halfsize = (intersect_max - intersect_min) / 2;
	}
	return true;
}
float KCollisionMath::getPointInLeft2D(float px, float py, float ax, float ay, float bx, float by) {
	float ab_x = bx - ax;
	float ab_y = by - ay;
	float ap_x = px - ax;
	float ap_y = py - ay;
	return ab_x * ap_y - ab_y * ap_x;
}
float KCollisionMath::getSignedDistanceOfLinePoint2D(float px, float py, float ax, float ay, float bx, float by) {
	float ab_x = bx - ax;
	float ab_y = by - ay;
	float ap_x = px - ax;
	float ap_y = py - ay;
	float cross = ab_x * ap_y - ab_y * ap_x;
	float delta = sqrtf(ab_x * ab_x + ab_y * ab_y);
	return cross / delta;
}
float KCollisionMath::getPerpendicularLinePoint(const KVec3 &p, const KVec3 &a, const KVec3 &b) {
	KVec3 ab = b - a;
	KVec3 ap = p - a;
	float k = ab.dot(ap) / ab.getLengthSq();
	return k;
}
float KCollisionMath::getPerpendicularLinePoint2D(float px, float py, float ax, float ay, float bx, float by) {
	KVec3 ab(bx - ax, by - ay, 0.0f);
	KVec3 ap(px - ax, py - ay, 0.0f);
	float k = ab.dot(ap) / ab.getLengthSq();
	return k;
}
bool KCollisionMath::isPointInRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf) {
	if (px < cx - xHalf) return false;
	if (px > cx + xHalf) return false;
	if (py < cy - yHalf) return false;
	if (py > cy + yHalf) return false;
	return true;
}
bool KCollisionMath::isPointInXShearedRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf, float xShear) {
	float ymin = cy - yHalf;
	float ymax = cy + yHalf;
	if (py < ymin) return false; // 下辺より下側にいる
	if (py > ymax) return false; // 上辺より上側にいる
	if (getPointInLeft2D(px, py, cx-xHalf-xShear, ymin, cx-xHalf+xShear, ymax) > 0) return false; // 左下から左上を見たとき、左側にいる
	if (getPointInLeft2D(px, py, cx+xHalf-xShear, ymin, cx+xHalf+xShear, ymax) < 0) return false; // 右下から右上を見た時に右側にいる
	return true;
}
bool KCollisionMath::collisionCircleWithPoint2D(float cx, float cy, float cr, float px, float py, float skin, float *xAdj, float *yAdj) {
//	K_assert(cr > 0);
	K_assert(skin >= 0);
	float dx = cx - px;
	float dy = cy - py;
	float dist = hypotf(dx, dy);
	if (fabsf(dist) < cr+skin) {
		if (xAdj || yAdj) {
			// めり込み量
			float penetration_depth = cr - dist;
			if (penetration_depth <= 0) {
				// 押し戻し不要（実際には接触していないが、skin による拡大判定が接触した）
				if (xAdj) *xAdj = 0;
				if (yAdj) *yAdj = 0;
			} else {
				// 押し戻し方向の単位ベクトル
				float normal_x = (dist > 0) ? dx / dist : 1.0f;
				float normal_y = (dist > 0) ? dy / dist : 0.0f;
				// 押し戻し量
				if (xAdj) *xAdj = normal_x * penetration_depth;
				if (yAdj) *yAdj = normal_y * penetration_depth;
			}
		}
		return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithLine2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin, float *xAdj, float *yAdj) {
//	K_assert(cr > 0);
	K_assert(skin >= 0);
	float dist = getSignedDistanceOfLinePoint2D(cx, cy, x0, y0, x1, y1);
	if (fabsf(dist) < cr+skin) {
		if (xAdj || yAdj) {
			// めり込み量
			float penetration_depth = cr - fabsf(dist);
			if (penetration_depth < 0) {
				// 実際には接触していないが、skin による拡大判定が接触した。押し戻し不要
				if (xAdj) *xAdj = 0;
				if (yAdj) *yAdj = 0;
			} else {
				// 押し戻し方向の単位ベクトル
				// (x0, y0) から (x1, y1) を見たときに、左側を法線方向とする 
				float dx = x1 - x0;
				float dy = y1 - y0;
				float dr = hypotf(dx, dy);
				float normal_x = -dy / dr;
				float normal_y =  dx / dr;
				// 押し戻し量
				if (xAdj) *xAdj = normal_x * penetration_depth;
				if (yAdj) *yAdj = normal_y * penetration_depth;
			}
		}
		return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithLinearArea2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin, float *xAdj, float *yAdj) {
//	K_assert(cr > 0);
	K_assert(skin >= 0);
	float dist = getSignedDistanceOfLinePoint2D(cx, cy, x0, y0, x1, y1);

	if (dist < cr+skin) {
		if (xAdj || yAdj) {
			// めり込み量
			float penetration_depth = cr - dist;
			if (penetration_depth < 0) {
				// 実際には接触していないが、skin による拡大判定が接触した。押し戻し不要
				if (xAdj) *xAdj = 0;
				if (yAdj) *yAdj = 0;
			} else {
				// 押し戻し方向の単位ベクトル
				// (x0, y0) から (x1, y1) を見たときに、左側を法線方向とする 
				float dx = x1 - x0;
				float dy = y1 - y0;
				float dr = hypotf(dx, dy);
				float normal_x = -dy / dr;
				float normal_y =  dx / dr;
				// 押し戻し量
				if (xAdj) *xAdj = normal_x * penetration_depth;
				if (yAdj) *yAdj = normal_y * penetration_depth;
			}
		}
		return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithSegment2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin, float *xAdj, float *yAdj) {
	float k = getPerpendicularLinePoint2D(cx, cy, x0, y0, x1, y1);
	if (k < 0.0f) {
		if (collisionCircleWithPoint2D(cx, cy, cr, x0, y0, skin, xAdj, yAdj))
			return true;
	} else if (k < 1.0f) {
		if (collisionCircleWithLine2D(cx, cy, cr, x0, y0, x1, y1, skin, xAdj, yAdj))
			return true; 
	} else {
		if (collisionCircleWithPoint2D(cx, cy, cr, x1, y1, skin, xAdj, yAdj))
			return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithCircle2D(float cx, float cy, float cr, float other_x, float other_y, float other_r, float skin, float *xAdj, float *yAdj) {
//	K_assert(cr > 0);
	K_assert(skin >= 0);
	float dx = cx - other_x;
	float dy = cy - other_y;
	float dist = hypotf(dx, dy);
	float penetration_depth = cr + other_r - dist; // めり込み深さ
	if (penetration_depth <= -skin) {
		return false; // 非接触
	}
	if (penetration_depth < 0) {
		// 実際には接触していないが、skin による拡大判定が接触した。押し戻し不要
		if (xAdj) *xAdj = 0;
		if (yAdj) *yAdj = 0;
	} else {
		// 押し戻し方向の単位ベクトル
		float _cos = (dist > 0) ? dx / dist : 1;
		float _sin = (dist > 0) ? dy / dist : 0;
		// 押し戻し量
		if (xAdj) *xAdj = penetration_depth * _cos;
		if (yAdj) *yAdj = penetration_depth * _sin;
	}
	return true;
}
int KCollisionMath::collisionCircleWithRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float skin, float *xAdj, float *yAdj) {
	return collisionCircleWithXShearedRect2D(cx, cy, cr, xBox, yBox, xHalf, yHalf, skin, 0, xAdj, yAdj);
}
int KCollisionMath::collisionCircleWithXShearedRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float xShear, float skin, float *xAdj, float *yAdj) {
	float x0 = xBox - xHalf; // 左
	float y0 = yBox - yHalf; // 下
	float x1 = xBox + xHalf; // 右
	float y1 = yBox + yHalf; // 上
	// 多角形は時計回りにチェックする。角に重なった場合など複数の偏と衝突する場合があるので必ず全ての面の判定を行う
	float xOld = cx;
	float yOld = cy;
	// 左下からスタート

	// 1 左(X負）
	// 2 上(Y正）
	// 4 右(X正）
	// 8 下(Y負）
	int hit = 0;

	float adjx, adjy;
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x0-xShear, y0, x0+xShear, y1, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 1; // 左側
	}
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x0+xShear, y1, x1+xShear, y1, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 2; // 上側
	}
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x1+xShear, y1, x1-xShear, y0, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 4; // 右側
	}
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x1-xShear, y0, x0-xShear, y0, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 8; // 下側
	}
	if (xAdj) *xAdj = cx - xOld;
	if (yAdj) *yAdj = cy - yOld;
	return hit;
}
// r1, r2 -- radius
// k1, k2 -- penetratin response
bool KCollisionMath::resolveYAxisCylinderCollision(KVec3 *p1, float r1, float k1, KVec3 *p2, float r2, float k2, float skin) {
	float xAdj, zAdj;
	if (collisionCircleWithCircle2D(p1->x, p1->z, r1, p2->x, p2->z, r2, skin, &xAdj, &zAdj)) {
		if (k1 + k2 > 0) {
			p1->x += xAdj * k1;
			p1->z += zAdj * k1;
			p2->x -= xAdj * k2;
			p2->z -= zAdj * k2;
		}
		return true;
	}
	return false;
}
#pragma endregion // KCollisionMath





namespace Test {
void Test_bezier() {
	#define RND (100 + rand() % 400)
	for (int i=0; i<1000; i++) {
		KVec3 p[] = {
			KVec3(RND, RND, 0),
			KVec3(RND, RND, 0),
			KVec3(RND, RND, 0),
			KVec3(RND, RND, 0)
		};
		KCubicBezier b;
		b.addSegment(p[0], p[1], p[2], p[3]);

		// 多角線による近似
		float len1 = b.getLength_Test1(0, 100);
		// 再帰分割での近似
		float len2 = b.getLength_Test2(0);

		// それぞれ算出した長さ値が一定以上異なっていれば SVG 形式で曲線パラメータを出力する
		// なお、このテキストをそのまま
		// http://www.useragentman.com/tests/textpath/bezier-curve-construction-set.html
		// に貼りつければブラウザで確認できる
		if (fabsf(len1 - len2) >= 1.0f) {
			char s[1024];
			sprintf_s(s, sizeof(s), "M %g, %g C %g, %g, %g, %g, %g, %g\n", p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y, p[3].x, p[3].y);
			OutputDebugStringA(s);
		}
	}
	#undef RND
}
} // Test



#pragma endregion // KMath.cpp




} // namespace
