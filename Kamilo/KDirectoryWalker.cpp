#include "KDirectoryWalker.h"

#include <vector>
#include <Windows.h>
#include <Shlwapi.h>
#include "KInternal.h"

namespace Kamilo {

void KDirectoryWalker::scanFiles(const char *top_u8, const char *dir_u8, std::vector<Item> &list) {
	std::wstring wtop = K::strUtf8ToWide(top_u8);
	std::wstring wdir = K::strUtf8ToWide(dir_u8);
	scanFilesW(wtop.c_str(), wdir.c_str(), list);
}
void KDirectoryWalker::scanFilesW(const wchar_t *wtop, const wchar_t *wdir, std::vector<Item> &list) {
	K__Assert(wdir);

	// 検索パターンを作成
	wchar_t wpattern[MAX_PATH] = {0};
	{
		wchar_t wtmp[MAX_PATH] = {0};
		PathAppendW(wtmp, wtop);
		PathAppendW(wtmp, wdir);
		GetFullPathNameW(wtmp, MAX_PATH, wpattern, NULL);
		PathAppendW(wpattern, L"*");
	}
	// ここで wpattern は
	// "d:\\system\\*"
	// のような文字列になっている。ワイルドカードに注意！！
	WIN32_FIND_DATAW fdata;
	HANDLE hFind = FindFirstFileW(wpattern, &fdata);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (fdata.cFileName[0] != L'.') {
				Item fitem;
				fitem.namew = fdata.cFileName;
				fitem.parentw = wdir;
				fitem.nameu = K::strWideToUtf8(fitem.namew);
				fitem.parentu = K::strWideToUtf8(fitem.parentw);
				fitem.isdir = (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
				list.push_back(fitem);
			}
		} while (FindNextFileW(hFind, &fdata));
		FindClose(hFind);
	}
}
void KDirectoryWalker::scanW(const wchar_t *wtop, const wchar_t *wdir, Callback *cb) {
	K__Assert(wtop);
	K__Assert(wdir);
	K__Assert(cb);
	std::vector<Item> list;
	scanFilesW(wtop, wdir, list);
	for (auto it=list.begin(); it!=list.end(); ++it) {
		if (it->isdir) {
			bool enter = false;
			cb->onDir(it->nameu.c_str(), it->parentu.c_str(), &enter);
			if (enter) {
				wchar_t wsub[MAX_PATH] = {0};
				wcscpy_s(wsub, MAX_PATH, wdir);
				PathAppendW(wsub, it->namew.c_str());
				scanW(wtop, wsub, cb);
				cb->onDirExit(it->nameu.c_str(), it->parentu.c_str());
			}
		} else {
			cb->onFile(it->nameu.c_str(), it->parentu.c_str());
		}
	}
}
void KDirectoryWalker::walk(const char *dir_u8, KDirectoryWalker::Callback *cb) {
	K__Assert(dir_u8);
	K__Assert(cb);
	wchar_t wdir[MAX_PATH] = {0};
	K__Utf8ToWidePath(wdir, MAX_PATH, dir_u8);
	scanW(wdir, L"", cb);
}

} // namespace
