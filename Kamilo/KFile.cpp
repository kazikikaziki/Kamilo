#include "KFile.h"
//
#include "KCrc32.h"
#include "KDirectoryWalker.h"
#include <vector>
#include <unordered_set>
#include <mutex>
#include <Windows.h>
#include <Shlwapi.h>
#include "KInternal.h"
#include "KString.h"


// ファイルまたはディレクトリをコピー、削除する場合に確認のダイアログを出す。テスト用
// （ファイルに書き込む場合はなにもしない）
#ifndef K_FILE_SAFE
#	define K_FILE_SAFE 0
#endif


namespace Kamilo {



#pragma region File Operation
namespace kns_fileop {
static bool K_file__confirm(const wchar_t *op, const wchar_t *path1, const wchar_t *path2) {
	if (K_FILE_SAFE) {
		wchar_t wmsg[1024] = {0};
		wchar_t fullpath1[MAX_PATH] = {0};
		wchar_t fullpath2[MAX_PATH] = {0};
		if (path1) {
			GetFullPathNameW(path1, MAX_PATH, fullpath1, nullptr);
		} else {
			wcscpy_s(fullpath1, MAX_PATH, L"<null>");
		}
		if (path2) {
			GetFullPathNameW(path2, MAX_PATH, fullpath2, nullptr);
		} else {
			wcscpy_s(fullpath2, MAX_PATH, L"<null>");
		}
		swprintf_s(wmsg, sizeof(wmsg)/sizeof(wchar_t),
			L"K_FILE_SAVE スイッチが有効になっているため、ファイルに対して操作する前に確認を行います。\n\n"
			L"次の操作を許可しますか？\n"
			L"Func  = %s\n"
			L"Path1 = %s\n"
			L"Path2 = %s\n",
			op,
			fullpath1,
			fullpath2
		);
		int btn = MessageBoxW(nullptr, wmsg, L"ファイルに対する操作", MB_ICONWARNING|MB_OKCANCEL);
		return btn == IDOK;
	} else {
		// 確認しない
		return true;
	}
}
static bool K_file__RemoveFileW(const wchar_t *wpath) {
	K__Assert(wpath);
	if (!PathFileExistsW(wpath)) {
		return true; // 該当パスが存在しない場合は、削除に成功したものとする
	}
	if (PathIsDirectoryW(wpath)) {
		return false; // ディレクトリは削除できない
	}
	if (!K_file__confirm(L"DeleteFile", wpath, nullptr)) {
		return false;
	}
	K__PrintW(L"DeleteFileW: %s", wpath);
	if (DeleteFileW(wpath)) {
		return true;
	}
	wchar_t full[MAX_PATH] = {0};
	GetFullPathNameW(wpath, MAX_PATH, full, nullptr);
	K__ErrorW(L"DeleteFileW FAIL!: %s (%s)", wpath, full);
	return false;
}
static bool K_file__RemoveEmptyDirectoryW(const wchar_t *wdir) {
	K__Assert(wdir);
	if (!PathFileExistsW(wdir)) {
		return true; // 該当パスが存在しない場合は、削除に成功したものとする
	}
	if (!PathIsDirectoryW(wdir)) {
		return false; // 非ディレクトリは削除できない
	}
	if (!K_file__confirm(L"RemoveDirectoryW", wdir, nullptr)) {
		return false;
	}
	K__PrintW(L"RemoveDirectoryW: %s", wdir);
	if (RemoveDirectoryW(wdir)) {
		return true;
	}
	wchar_t full[MAX_PATH] = {0};
	GetFullPathNameW(wdir, MAX_PATH, full, nullptr);
	K__ErrorW(L"RemoveDirectoryW FAIL!: %s (%s)", wdir, full);
	return false;
}
static bool K_file__IsRemovableDirectoryW(const wchar_t *wdir) {
	K__Assert(wdir);
	// カレントディレクトリは指定できない
	if (wdir[0]==0 || wcscmp(wdir, L".")==0 || wcscmp(wdir, L"./")==0) {
		K__PrintW(L"E_REMOVE_DIR_FILES: path includes myself: %s", wdir);
		return false;
	}
	// ディレクトリ階層を登るような相対パスは指定できない（意図しないフォルダを消す事故の軽減）
	// 念のため、文字列 ".." を含むパスを一律禁止する
	if (wcsstr(wdir, L"..") != nullptr) {
		K__PrintW(L"E_REMOVE_DIR_FILES: path includes parent directory: %s", wdir);
		return false;
	}
	// 絶対パスは指定できない（間違ってルートディレクトリを指定する事故の軽減）
	if (!PathIsRelativeW(wdir)) {
		K__PrintW(L"E_REMOVE_DIR_FILES: path is absolute: %s", wdir);
		return false;
	}
	// ディレクトリではなくファイル名だったらダメ
	if (PathFileExistsW(wdir) && !PathIsDirectoryW(wdir)) {
		K__PrintW(L"E_REMOVE_DIR_FILES: path is not a directory: %s", wdir);
		return false;
	}
	return true;
}
static bool K_file__RemoveEmptyDirectoryTreeW(const wchar_t *wdir) {
	K__Assert(wdir);
	if (! K_file__IsRemovableDirectoryW(wdir)) {
		return false;
	}

	// ディレクトリを走査
	std::vector<KDirectoryWalker::Item> list;
	KDirectoryWalker::scanFilesW(wdir, L"", list);

	// 空のサブディレクトリを削除
	bool all_ok = true;
	for (auto it=list.begin(); it!=list.end(); ++it) {
		if (it->isdir) {
			wchar_t wsub[MAX_PATH] = {0};
			PathAppendW(wsub, wdir);
			PathAppendW(wsub, it->pathw.c_str());
			PathAppendW(wsub, it->namew.c_str());
			all_ok &= K_file__RemoveEmptyDirectoryTreeW(wsub);
		}
	}

	// ディレクトリを削除
	all_ok &= K_file__RemoveEmptyDirectoryW(wdir);

	// 全てのファイルの削除に成功した時のみ true.
	// 初めからファイルが存在しない場合は成功とみなす
	return all_ok;
}
static bool K_file__RemoveNonDirFilesInDirectoryW(const wchar_t *wdir, bool subdir) {
	K__Assert(wdir);
	if (! K_file__IsRemovableDirectoryW(wdir)) {
		return false;
	}

	// ディレクトリを走査
	std::vector<KDirectoryWalker::Item> list;
	KDirectoryWalker::scanFilesW(wdir, L"", list);

	// 全ての非ディレクトリファイルの削除に成功した時のみ true.
	// 初めからファイルが存在しない場合は成功とみなす
	bool all_ok = true;

	// 非ディレクトリファイルを削除
	for (auto it=list.begin(); it!=list.end(); ++it) {
		wchar_t wsub[MAX_PATH] = {0};
		PathAppendW(wsub, wdir);
		PathAppendW(wsub, it->pathw.c_str());
		PathAppendW(wsub, it->namew.c_str());
		if (it->isdir) {
			if (subdir) {
				K_file__RemoveNonDirFilesInDirectoryW(wsub, true);
			}
		} else {
			all_ok &= K_file__RemoveFileW(wsub);
		}
	}
	return all_ok; // すべて削除できた場合のみ true を返す
}
} // namespace kns_fileop

/// ファイルをコピーする
///
/// @param src コピー元ファイル名
/// @param dest コピー先ファイル名
/// @param overwrite 上書きするかどうか
/// @arg true  コピー先に同名のファイルが存在するなら上書きし、成功したら true を返す
/// @arg false コピー先に同名のファイルがある場合にはコピーをせず、false を返す
/// @return 成功したかどうか
bool K_FileCopy(const char *src_u8, const char *dst_u8, bool overwrite) {
	K__Assert(src_u8);
	K__Assert(dst_u8);
	wchar_t wsrc[MAX_PATH] = {0};
	wchar_t wdst[MAX_PATH] = {0};
	K__Utf8ToWidePath(wsrc, sizeof(wsrc)/sizeof(wchar_t), src_u8);
	K__Utf8ToWidePath(wdst, sizeof(wdst)/sizeof(wchar_t), dst_u8);
	if (!kns_fileop::K_file__confirm(L"CopyFile", wsrc, wdst)) {
		return false;
	}
	if (!CopyFileW(wsrc, wdst, !overwrite)) {
		// エラー原因の確認用
		char err[256] = {0};
		K__Win32GetErrorString(GetLastError(), err, sizeof(err));
		K__Error("Faield to CopyFile(): %s", err);
		return false;
	}
	return true;
}

/// ディレクトリを作成する
///
/// 既にディレクトリが存在する場合は成功したとみなす。
/// ディレクトリが既に存在するかどうかを確認するためには directoryExists() を使う
bool K_FileMakeDir(const char *dir_u8) {
	K__Assert(dir_u8);
	wchar_t wdir[MAX_PATH] = {0};
	K__Utf8ToWidePath(wdir, MAX_PATH, dir_u8);
	if (PathIsDirectoryW(wdir)) {
		return true; // already exists
	}
	if (!kns_fileop::K_file__confirm(L"CreateDirectory", wdir, nullptr)) {
		return false;
	}
	if (CreateDirectoryW(wdir, nullptr)) {
		return true;
	}
	char err[256] = {0};
	K__Win32GetErrorString(GetLastError(), err, sizeof(err));
	K__Error("Faield to CreateDirectory(): %s", err);
	return false;
}

/// ファイルを削除する
///
/// 指定されたファイルを削除できたなら true を返す。
/// 指定されたファイルが初めから存在しない場合も、削除に成功したものとして true を返す。
/// それ以外の場合は false を返す
/// @attention ディレクトリは削除できない
bool K_FileRemove(const char *path_u8) {
	K__Assert(path_u8);
	wchar_t wpath[MAX_PATH] = {0};
	K__Utf8ToWidePath(wpath, MAX_PATH, path_u8);
	return kns_fileop::K_file__RemoveFileW(wpath);
}

/// 空のディレクトリを削除する
///
/// 指定されたディレクトリが空であればそれを削除する。成功したら true を返す。
/// ディレクトリが初めから存在しなかった場合も削除に成功したものとして true を返す。
/// それ以外の場合は false を返す
/// @note 空でないディレクトリは削除できない
bool K_FileRemoveEmptyDir(const char *dir_u8) {
	K__Assert(dir_u8);
	wchar_t wdir[MAX_PATH] = {0};
	K__Utf8ToWidePath(wdir, MAX_PATH, dir_u8);
	return kns_fileop::K_file__RemoveEmptyDirectoryW(wdir);
}

/// 空のディレクトリを再帰的に削除する
///
/// dir ディレクトリ内にあるすべての空ディレクトリを再帰的に削除する。
/// 空でないディレクトリは無視する。dir に含まれる全ディレクトリを削除できた場合に限り true を返す。
/// dir ディレクトリが初めから存在しなかった場合も削除に成功したものとして true を返す。
/// それ以外の場合は false を返す
bool K_FileRemoveEmptyDirTree(const char *dir_u8) {
	K__Assert(dir_u8);
	wchar_t wdir[MAX_PATH] = {0};
	K__Utf8ToWidePath(wdir, MAX_PATH, dir_u8);
	return kns_fileop::K_file__RemoveEmptyDirectoryTreeW(wdir);
}

/// dir ディレクトリ内にある全ての非ディレクトリファイルを削除する
///
/// 全てのファイルの削除に成功した時のみ true を返す。
/// 初めからファイルが存在しなかった場合は成功とみなす
/// この関数が成功すれば dir 内にはディレクトリだけが残る
bool K_FileRemoveFilesInDir(const char *dir_u8) {
	K__Assert(dir_u8);
	wchar_t wdir[MAX_PATH] = {0};
	K__Utf8ToWidePath(wdir, MAX_PATH, dir_u8);
	return kns_fileop::K_file__RemoveNonDirFilesInDirectoryW(wdir, false);
}

/// dir ディレクトリ内にある全ての非ディレクトリファイルを再帰的に削除する
/// 
/// 全てのファイルの削除に成功した時のみ true を返す。
/// 初めからファイルが存在しなかった場合は成功とみなす
/// この関数が成功すれば dir 内にはディレクトリ構造だけが残る
bool K_FileRemoveFilesInDirTree(const char *dir_u8) {
	K__Assert(dir_u8);
	wchar_t wdir[MAX_PATH] = {0};
	K__Utf8ToWidePath(wdir, MAX_PATH, dir_u8);
	return kns_fileop::K_file__RemoveNonDirFilesInDirectoryW(wdir, true);
}
#pragma endregion // File Operation





#pragma region KReader
namespace kns_reader {
class CFileStrmR: public KReader {
public:
	FILE *m_file;
	std::string m_name;

	CFileStrmR(FILE *fp, const char *name) {
		m_file = fp;
		m_name = name;
	}
	virtual ~CFileStrmR() {
		fclose(m_file);
	}
	virtual int tell() override {
		return ftell(m_file);
	}
	virtual int read(void *data, int size) {
		if (data) {
			return fread(data, 1, size, m_file);
		} else {
			return fseek(m_file, size, SEEK_CUR);
		}
	}
	virtual void seek(int pos) {
		fseek(m_file, pos, SEEK_SET);
	}
	virtual int size() {
		int i=ftell(m_file);
		fseek(m_file, 0, SEEK_END);
		int n=ftell(m_file);
		fseek(m_file, i, SEEK_SET);
		return n; 
	}
};
class CMemStrmR: public KReader {
	void *m_ptr;
	int m_size;
	int m_pos;
	bool m_copy;
public:
	CMemStrmR(const void *p, int size, bool copy) {
		if (copy) {
			m_ptr = malloc(size);
			memcpy(m_ptr, p, size);
			m_copy = true;
		} else {
			m_ptr = const_cast<void*>(p);
			m_copy = false;
		}
		m_size = size;
		m_pos = 0;
	}
	virtual ~CMemStrmR() {
		if (m_copy && m_ptr) {
			free(m_ptr);
		}
	}
	virtual int tell() override {
		return m_pos;
	}
	virtual int read(void *data, int size) {
		int n = 0;
		if (m_pos + size <= m_size) {
			n = size;
		} else if (m_pos < m_size) {
			n = m_size - m_pos;
		}
		if (n > 0) {
			if (data) memcpy(data, (uint8_t*)m_ptr + m_pos, n); // data=nullptr だと単なるシークになる
			m_pos += n;
			return n;
		}
		return 0;
	}
	virtual void seek(int pos) {
		if (pos < 0) {
			m_pos = 0;
		} else if (pos < m_size) {
			m_pos = pos;
		} else {
			m_pos = m_size;
		}
	}
	virtual int size() {
		return m_size;
	}
};
} // naemsapce kns_reader
KReader * KReader::createFromFileName(const char *filename) {
	FILE *fp = K__fopen_u8(filename, "rb");
	if (fp) {
		return new kns_reader::CFileStrmR(fp, filename);
	}
	return nullptr;
}
KReader * KReader::createFromMemory(const void *data, int size) {
	return new kns_reader::CMemStrmR(data, size, false); // No copy
}
KReader * KReader::createFromMemoryCopy(const void *data, int size) {
	return new kns_reader::CMemStrmR(data, size, true); // Copy
}

class CStreamReader: public KReader {
public:
	KInputStream m_Strm;

	CStreamReader(KInputStream &strm) {
		m_Strm = strm;
	}
	virtual int tell() {
		return m_Strm.tell();
	}
	virtual int read(void *data, int size) {
		return m_Strm.read(data, size);
	}
	virtual void seek(int pos) {
		m_Strm.seek(pos);
	}
	virtual int size() {
		return m_Strm.size();
	}
};
KReader * KReader::createFromStream(KInputStream &input) {
	return new CStreamReader(input);
}




std::string KReader::readBinFromFileName(const char *filename) {
	std::string bin;
	KReader *r = createFromFileName(filename);
	if (r) {
		bin = r->read_bin();
		r->drop();
	}
	return bin;
}
#pragma endregion // KReader

namespace Test {
void Test_reader() {
	const char *text = "hello, world.";
	KReader *r = KReader::createFromMemory(text, strlen(text));
	char s[32] = {0};
	
	K__Assert(r->read(s, 5) == 5);
	K__Assert(strncmp(s, "hello", 5) == 0);
	K__Assert(r->tell() == 5);
	
	K__Assert(r->read(s, 7) == 7);
	K__Assert(strncmp(s, ", world", 7) == 0);
	K__Assert(r->tell() == 12);

	K__Assert(r->read(s, 4) == 1);
	K__Assert(strncmp(s, ".", 1) == 0);
	K__Assert(r->tell() == 13);

	r->drop();
}
void Test_writer() {
	std::string s;
	KOutputStream w = KOutputStream::fromMemory(&s);
	K__Assert(w.write("abc", 3) == 3);
	K__Assert(w.write(" ",   1) == 1);
	K__Assert(w.write("def", 3) == 3);
	K__Assert(s.compare("abc def") == 0);
}
} // Test

#pragma region KWriter
class CFileStrmW: public KWriter {
public:
	FILE *m_file;
	std::string m_name;

	CFileStrmW(FILE *fp, const char *name) {
		m_file = fp;
		m_name = name;
	}
	virtual ~CFileStrmW() {
		fclose(m_file);
	}
	virtual int tell() override {
		return ftell(m_file);
	}
	virtual int write(const void *data, int size) {
		return fwrite(data, 1, size, m_file);
	}
	virtual void seek(int pos) {
		fseek(m_file, pos, SEEK_SET);
	}
};
class CMemStrmW: public KWriter {
	std::string *m_buf;
	int m_pos;
public:
	CMemStrmW(std::string *dest) {
		m_buf = dest;
		m_pos = 0;
	}
	virtual int tell() override {
		return m_pos;
	}
	virtual int write(const void *data, int size) {
		m_buf->resize(m_pos + size);
		memcpy((char*)m_buf->data() + m_pos, data, size);
		m_pos += size;
		return size;
	}
	virtual void seek(int pos) {
		if (pos < 0) {
			m_pos = 0;
		} else if (pos < (int)m_buf->size()) {
			m_pos = pos;
		} else {
			m_pos = m_buf->size();
		}
	}
};
KWriter * KWriter::createFromFileName(const char *filename_u8) {
	FILE *fp = K__fopen_u8(filename_u8, "wb");
	if (fp) {
		return new CFileStrmW(fp, filename_u8);
	}
	return nullptr;
}
KWriter * KWriter::createFromMemory(std::string *dest) {
	K__Assert(dest);
	return new CMemStrmW(dest);
}
#pragma endregion // KWriter



} // namespace
