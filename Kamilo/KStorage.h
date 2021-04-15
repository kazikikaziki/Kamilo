#pragma once
#include <string>
#include "KFile.h"
#include "KStream.h"

namespace Kamilo {
	
class KReader;
class CFileLoaderImpl; // internal

class KArchive: public virtual KRef {
public:
	static KArchive * createFolderReader(const char *dir);
	static KArchive * createZipReader(const char *zip, const char *password=nullptr);
	static KArchive * createPacReader(const char *filename);
	static KArchive * createEmbeddedReader();
	static KArchive * createEmbeddedZipReader(const char *filename, const char *password=nullptr);
	static KArchive * createEmbeddedPacReader(const char *filename);
public:
	/// ファイルが存在するかどうか
	virtual bool contains(const char *filename) = 0;

	/// ファイルを取得しようとしたときに呼ばれる
	virtual KInputStream createFileReader(const char *filename) = 0;

	/// ロード可能なファイル数を返す
	virtual int getFileCount() = 0;

	/// ロード可能なファイル名を列挙する。
	/// 列挙できた場合は names にファイル名を追加して true を返す。（ロード可能なファイルが存在しない場合でも成功したとみなす）
	/// れっきょできない場合は false を返す
	virtual const char * getFileName(int index) = 0;
};

class KStorage {
public:
	static void clear();
	static bool empty() { return getLoaderCount() == 0; }

	/// ファイルをロードしようとしたときなどに呼ばれるコールバックを登録する
	/// @note 必ず new したコールバックオブジェクトを渡すこと。
	/// ここで渡したポインタに対してはデストラクタまたは clear() で delete が呼ばれる
	static void addArchive(KArchive *cb);

	/// 通常フォルダを検索対象に追加する
	static bool addFolder(const char *dir);

	/// Zipファイルを検索対象に追加する
	/// @see KPacFileReader
	static bool addZipFile(const char *filename, const char *password);

	/// パックファイルを検索対象に追加する
	/// @see KPacFileReader
	static bool addPacFile(const char *filename);

	/// アプリケーションに埋め込まれたファイル（リソースファイル）を検索対象に追加する
	/// @see KEmbeddedFiles
	static bool addEmbeddedFiles();

	/// アプリケーションに埋め込まれたパックファイルを検索対象に追加する
	/// @see KPacFileReader
	/// @see KEmbeddedFiles
	static bool addEmbeddedPacFileLoader(const char *filename);

	/// 指定した名前のファイルがあれば KReader を返す
	/// should_exists が true の場合、ファイルが見つからなければエラーログを出す
	static KReader * createReader(const char *filename, bool should_exists=true);
	static KInputStream getInputStream(const char *filename, bool should_exists=true);
	
	/// 指定されたファイルが存在すれば、その内容をすべてバイナリモードで読み取る
	/// 戻り値は文字列型だが、ファイル内容のバイナリを無加工で保持している
	/// should_exists が true の場合、ファイルが見つからなければエラーログを出す
	static std::string loadBinary(const char *filename, bool should_exists=true);

	/// 指定されたファイルが存在するか調べる
	static bool contains(const char *filename);

	static KArchive * getLoader(int index);
	static int getLoaderCount();
};

} // namespace
