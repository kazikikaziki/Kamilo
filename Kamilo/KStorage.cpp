#include "KStorage.h"

#include <vector>
#include "KDirectoryWalker.h"
#include "KEmbeddedFiles.h"
#include "KInternal.h"
#include "KPac.h"
#include "KZip.h"


// 大小文字の指定ミスを検出
#ifndef K_CASE_CHECK
#	define K_CASE_CHECK 1
#endif

const int PAC_CASE_CEHCK = 1;


namespace Kamilo {




#pragma region CWin32ResourceArchive
class CWin32ResourceArchive: public KArchive {
public:
	virtual bool contains(const char *name) override {
		return KEmbeddedFiles::contains(name);
	}
	virtual KInputStream createFileReader(const char *name) override {
		return KEmbeddedFiles::createInputStream(name);
	}
	virtual int getFileCount() override {
		return KEmbeddedFiles::count();
	}
	virtual const char * getFileName(int index) override {
		return KEmbeddedFiles::name(index);
	}
};
KArchive * KArchive::createEmbeddedReader() {
	return new CWin32ResourceArchive();
}
#pragma endregion // CWin32ResourceArchive


#pragma region CFolderArchive
class CFolderArchive: public KArchive, public KDirectoryWalker::Callback {
	std::string dir_;
	std::vector<std::string> names_;
public:
	CFolderArchive(const char *dir, int *err) {
		dir_ = dir;
		if (!KPathUtils::K_PathIsDir(dir)) {
			K__Error("CFolderArchive: Directory not exists: '%s'", dir);
			*err = 1;
		} else {
			*err = 0;
		}
	}

	virtual void onFile(const char *name_u8, const char *parent_u8) override {
		// KDirectoryWalker::walk でファイル（ディレクトリではない）を列挙するたびに呼ばれる。ファイルリストに登録する
		char path[KPathUtils::MAX_SIZE] = {0};
		KPathUtils::K_PathPushLast(path, sizeof(path), parent_u8);
		KPathUtils::K_PathPushLast(path, sizeof(path), name_u8);
		names_.push_back(path);
	}

	// 大小文字だけが異なる同名ファイルがあった時に警告する
	void check_filename_case(const char *name) {
		if (names_.empty()) {
			KDirectoryWalker::walk(dir_.c_str(), this);
		}
		for (auto it=names_.begin(); it!=names_.end(); ++it) {
			if (KPathUtils::K_PathCompare(it->c_str(), name, true, false) == 0) {
				// [大小文字区別なし]で比較した
				// 念のために。[大小文字区別あり]でも比較する
				if (KPathUtils::K_PathCompare(it->c_str(), name, false, false) != 0) {
					K__Print(
						u8"W_FILEANME_CASE: ファイル名 '%s' が指定されましたが、実際のファイル名は '%s' です。大小文字だけが異なる同名ファイルは"
						u8"アーカイブ化したときに正しくロードできない可能性があります。必ず大小文字も一致させてください", name, it->c_str()
					);
				}
			}
		}
	}

	// ファイルが存在する？
	virtual bool contains(const char *name) override {
		// 大小文字が異なるだけの同名ファイルをチェック
		if (K_CASE_CHECK) {
			check_filename_case(name);
		}

		// 実際のファイル名を得る
		char realname[KPathUtils::MAX_SIZE] = {0};
		KPathUtils::K_PathPushLast(realname, sizeof(realname), dir_.c_str());
		KPathUtils::K_PathPushLast(realname, sizeof(realname), name);

		// ファイルの存在を確認
		if (!KPathUtils::K_PathExists(realname)) {
			return false;
		}

		// ディレクトリ名でないことを確認
		if (KPathUtils::K_PathIsDir(realname)) {
			return false;
		}

		// ファイルとして存在する
		return true;
	}
	virtual KInputStream createFileReader(const char *name) override {
		// 大小文字が異なるだけの同名ファイルをチェック
		if (K_CASE_CHECK) {
			check_filename_case(name);
		}

		// 実際のファイル名を得る
		char realname[KPathUtils::MAX_SIZE] = {0};
		KPathUtils::K_PathPushLast(realname, sizeof(realname), dir_.c_str());
		KPathUtils::K_PathPushLast(realname, sizeof(realname), name);

		// KInputStream を取得
		KInputStream file = KInputStream::fromFileName(realname);
		if (file.isOpen()) {
			K__OutputDebugString( __FUNCTION__, ": ", realname, " ==> OK");
			return file;
		}
		return KInputStream();
	}
	virtual int getFileCount() override {
		return names_.size();
	}
	virtual const char * getFileName(int index) override {
		return names_[index].c_str();
	}
};
KArchive * KArchive::createFolderReader(const char *dir) {
	int err = 0;
	CFolderArchive *ar = new CFolderArchive(dir, &err);
	if (err == 0) {
		return ar;
	}
	ar->drop();
	return nullptr;
}
#pragma endregion // CFolderArchive



#pragma region CZipArchive
class CZipArchive: public KArchive {
	KUnzipper m_unzip;
	std::string m_pass;
	std::string m_tmp;
public:
	CZipArchive(KInputStream &input, const char *password, int *err) {
		m_unzip.open(input);
		if (!m_unzip.isOpen()) {
			*err = 1;
			return;
		}
		m_pass = password ? password : "";
	}
	virtual bool contains(const char *filename) override {
		KInputStream r = createFileReader(filename);
		return r.isOpen();
	}
	virtual KInputStream createFileReader(const char *filename) override {
		for (int i=0; i<m_unzip.getEntryCount(); i++) {
			const char *s = getFileName(i);
			if (KPathUtils::K_PathCompare(s, filename, false, false) == 0) {
				std::string bin;
				m_unzip.getEntryData(i, m_pass.c_str(), &bin);

				return KInputStream::fromMemoryCopy(bin.data(), bin.size());
			}
		}
		return KInputStream();
	}
	virtual int getFileCount() override {
		return m_unzip.getEntryCount();
	}
	virtual const char * getFileName(int index) override {
		std::string rawname;
		m_unzip.getEntryName(index, &rawname);
		if (m_unzip.getEntryParamInt(index, KUnzipper::WITH_UTF8)) {
			// zip内のファイル名が utf8 で記録されている。変換しなくてよい
			m_tmp = rawname;
		} else {
			// zip内のファイル名が utf8 以外で記録されている
			m_tmp = KStringUtils::ansiToUtf8(rawname.c_str());
		}
		return m_tmp.c_str();
	}
};

KArchive * KArchive::createZipReader(const char *zip, const char *password) {
	KArchive *ar = nullptr;
	KInputStream file = KInputStream::fromFileName(zip);
	if (file.isOpen()) {
		int err = 0;
		ar = new CZipArchive(file, password, &err);
		if (err) {
			ar->drop();
			ar = nullptr;
		}
	}
	return ar;
}
#pragma endregion // CZipArchive


#pragma region CPacFile
class CPacFile: public KArchive {
	KPacFileReader mReader;
	std::string mTmp;
public:
	explicit CPacFile(KPacFileReader &reader) {
		mReader = reader;
	}

	// 大小文字だけが異なる同名ファイルがあった時に警告する
	void check_filename_case(const KPath &name) {
		if (mReader.getIndexByName(name, false, false) < 0) {
			int i = mReader.getIndexByName(name, true, false);
			if (i >= 0) {
				KPath s = mReader.getName(i);
				K__Print(
					u8"W_PAC_FILEANME_CASE: ファイル名 '%s' の代わりに '%s' が存在します。大小文字を間違えていませんか？", name.u8(), s.u8());
			}
		}
	}

	virtual bool contains(const char *name) override {
		if (PAC_CASE_CEHCK) {
			check_filename_case(name);
		}
		return mReader.getIndexByName(name, false, false) >= 0;
	}
	virtual KInputStream createFileReader(const char *name) override {
		if (PAC_CASE_CEHCK) {
			check_filename_case(name);
		}
		int index = mReader.getIndexByName(name, false, false);
		if (index < 0) {
			return KInputStream();
		}
		std::string bin = mReader.getData(index);
		return KInputStream::fromMemoryCopy(bin.data(), bin.size());
	}
	virtual int getFileCount() override {
		return mReader.getCount();
	}
	virtual const char * getFileName(int index) override {
		mTmp = mReader.getName(index).u8();
		return mTmp.c_str();
	}
};
KArchive * KArchive::createPacReader(const char *filename) {
	KArchive *archive = nullptr;
	KInputStream file = KInputStream::fromFileName(filename);
	KPacFileReader reader = KPacFileReader::fromStream(file);
	if (reader.isOpen()) {
		archive = new CPacFile(reader);
	} else {
		K__Error("E_FILE_FAIL: Failed to open a pac file: '%s'", filename);
	}
	return archive;
}
#pragma endregion // CPacFile


// リソースに埋め込まれた zip ファイルからロード
KArchive * KArchive::createEmbeddedZipReader(const char *filename, const char *password) {
	// リソースとして埋め込まれた zip ファイルを得る
	KInputStream file = KEmbeddedFiles::createInputStream(filename);

	// KArchive インターフェースに適合させる
	KArchive *ar = nullptr;
	if (file.isOpen()) {
		int err = 0;
		ar = new CZipArchive(file, password, &err);
		if (err) {
			ar->drop();
			ar = nullptr;
		}
	}
	return ar;
}

// リソースに埋め込まれた pac ファイルからロード
KArchive * KArchive::createEmbeddedPacReader(const char *filename) {
	CPacFile *ar = nullptr;

	// リソースとして埋め込まれた pac ファイルを得る
	KInputStream file = KEmbeddedFiles::createInputStream(filename); // リソースとして埋め込まれた pac ファイル

	// その file を使って pac ファイルリーダーを作る
	KPacFileReader reader = KPacFileReader::fromStream(file);

	// KArchive インターフェースに適合させる
	if (reader.isOpen()) {
		ar = new CPacFile(reader);
	}
	return ar;
}







class CFileLoaderImpl {
	std::vector<KArchive *> m_archives;
public:
	CFileLoaderImpl() {
	}
	~CFileLoaderImpl() {
		clear();
	}
	void clear() {
		for (size_t i=0; i<m_archives.size(); i++) {
			m_archives[i]->drop();
		}
		m_archives.clear();
	}
	void addArchive(KArchive *ar) {
		if (ar) {
			ar->grab();
			m_archives.push_back(ar);
		}
	}
	bool addFolder(const char *dir) {
		KArchive *ar = KArchive::createFolderReader(dir);
		if (ar) {
			addArchive(ar);
			ar->drop();
			K__Print(u8"検索パスにディレクトリ %s を追加しました", dir);
			return true;
		} else {
			K__Warning("E_FILE_FAIL: Failed to open a dir: '%s'", dir);
			return false;
		}
	}
	bool addZipFile(const char *filename, const char *password) {
		KArchive *ar = KArchive::createZipReader(filename, password);
		if (ar) {
			addArchive(ar);
			ar->drop();
			K__Print(u8"検索パスにZIPファイル %s を追加しました", filename);
			return true;
		} else {
			K__Warning("E_FILE_FAIL: Failed to open a zip file: '%s'", filename);
			return false;
		}
	}
	bool addPacFile(const char *filename) {
		KArchive *ar = KArchive::createPacReader(filename);
		if (ar) {
			addArchive(ar);
			ar->drop();
			K__Print(u8"検索パスにPACファイル %s を追加しました", filename);
			return true;
		} else {
			K__Warning("E_FILE_FAIL: Failed to open a pac file: '%s'", filename);
			return false;
		}
	}
	bool addEmbeddedFiles() {
		KArchive *ar = KArchive::createEmbeddedReader();
		if (ar) {
			addArchive(ar);
			ar->drop();
			K__Print(u8"検索パスに埋め込みリソースを追加しました");
			return true;
		} else {
			K__Warning("E_FILE_FAIL: Failed to open a embedded files");
			return false;
		}
	}
	bool addEmbeddedPacFileLoader(const char *filename) {
		KArchive *ar = KArchive::createEmbeddedPacReader(filename);
		if (ar) {
			addArchive(ar);
			ar->drop();
			K__Print(u8"検索パスに埋め込み pac ファイルを追加しました: %s", filename);
			return true;
		} else {
			K__Warning("E_FILE_FAIL: Failed to open a embedded pac file: '%s'", filename);
			return false;
		}
	}
	KInputStream createReader(const char *filename, bool should_exists) {
		// 絶対パスで指定されている場合は普通のファイルとして開く
		if (!KPath(filename).isRelative()) {
			KInputStream file = KInputStream::fromFileName(filename);
			return file;
		}

		if (m_archives.empty()) {
			// ローダーが一つも設定されていない。
			// 一番基本的な方法で開く
			KInputStream file = KInputStream::fromFileName(filename);
			if (file.isOpen()) {
				return file;
			}
		} else {
			// ローダーを順番に試す
			for (size_t i=0; i<m_archives.size(); i++) {
				KArchive *ar = m_archives[i];
				KInputStream file = ar->createFileReader(filename);
				if (file.isOpen()) {
					return file;
				}
			}
		}
		if (should_exists) {
			K__Error("Failed to open file: %s", filename);
		}
		return KInputStream();
	}
	bool contains(const char *filename) {
		KInputStream file = createReader(filename, false);
		return file.isOpen();
	}
	std::string loadBinary(const char *filename, bool should_exists) {
		KInputStream file = createReader(filename, should_exists);
		return file.readBin();
	}
	KArchive * getLoader(int index) const {
		return m_archives[index];
	}
	int getLoaderCount() const {
		return (int)m_archives.size();
	}
}; // CFileLoaderImpl

static KStorage g_Storage;

#pragma region KStorage
KStorage & KStorage::getGlobal() {
	return g_Storage;
}

KStorage::KStorage() {
	m_Impl = std::shared_ptr<CFileLoaderImpl>(new CFileLoaderImpl());
}
void KStorage::clear() {
	return m_Impl->clear();
}
void KStorage::addArchive(KArchive *cb) {
	return m_Impl->addArchive(cb);
}
bool KStorage::addFolder(const char *dir) {
	return m_Impl->addFolder(dir);
}
bool KStorage::addZipFile(const char *filename, const char *password) {
	return m_Impl->addZipFile(filename, password);
}
bool KStorage::addPacFile(const char *filename) {
	return m_Impl->addPacFile(filename);
}
bool KStorage::addEmbeddedFiles() {
	return m_Impl->addEmbeddedFiles();
}
bool KStorage::addEmbeddedPacFileLoader(const char *filename) {
	return m_Impl->addEmbeddedPacFileLoader(filename);
}
KReader * KStorage::createReader(const char *filename, bool should_exists) {
	KInputStream strm = m_Impl->createReader(filename, should_exists);
	return KReader::createFromStream(strm);
}
KInputStream KStorage::getInputStream(const char *filename, bool should_exists) {
	return m_Impl->createReader(filename, should_exists);
}
std::string KStorage::loadBinary(const char *filename, bool should_exists) {
	return m_Impl->loadBinary(filename, should_exists);
}
bool KStorage::contains(const char *filename) {
	return m_Impl->contains(filename);
}
KArchive * KStorage::getLoader(int index) {
	return m_Impl->getLoader(index);
}
int KStorage::getLoaderCount() {
	return m_Impl->getLoaderCount();
}
#pragma endregion // KStorage

} // namespace
