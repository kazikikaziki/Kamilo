#include "KFile.h"
//
#include <vector>
#include <unordered_set>
#include <mutex>
#include <Windows.h>
#include <Shlwapi.h>
#include "KCrc32.h"
#include "KDirectoryWalker.h"
#include "KInternal.h"
#include "KString.h"


// ファイルまたはディレクトリをコピー、削除する場合に確認のダイアログを出す。テスト用
// （ファイルに書き込む場合はなにもしない）
#ifndef K_FILE_SAFE
#	define K_FILE_SAFE 0
#endif


namespace Kamilo {


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
			PathAppendW(wsub, it->parentw.c_str());
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
		PathAppendW(wsub, it->parentw.c_str());
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



} // namespace
