/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <inttypes.h>
#include <memory>
#include <mutex>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "KMath.h"
#include "KVideo.h"
#include "KLua.h"
#include "KDebug.h"
#include "KString.h"
#include "KStream.h"
#include "KImGui.h"
#include "KLog.h"
#include "KXml.h"

namespace Kamilo {

class KTextureBank;

#pragma region String.h

class KConv {
public:
	/// toWideTry と同じだが、変換結果を文字列として返す。
	/// どの方法でも変換できなかった場合、入力データをそのまま std::wstring に拡張したものを返す
	/// (data が char 配列であるとみなして、それぞれの要素を wchar_t に置き換えたもの）
	static std::wstring toWide(const void *data, int size);
	static std::wstring toWide(const std::string &data);
	static std::string  toUtf8(const void *data, int size);
	static std::string  toUtf8(const std::string &data);
};


#pragma endregion String.h






#pragma region File.h


/// 実行中の環境におけるファイル操作
class KFiles {
public:
	/// カレントディレクトリを得る
	static KPath getCurrentDirectory();

	/// カレントディレクトリを変更する
	static bool setCurrentDirectory(const KPath &path);

	/// パスが実在し、ディレクトリかどうか調べる
	static bool isDirectory(const KPath &path);

	/// パスが実在し、それが非ディレクトリかどうか調べる
	static bool isFile(const KPath &path);

	/// パスが実在するか調べる
	static bool exists(const KPath &path);

	/// ファイルをコピーする
	///
	/// @param src コピー元ファイル名
	/// @param dest コピー先ファイル名
	/// @param overwrite 上書きするかどうか
	/// @arg true  コピー先に同名のファイルが存在するなら上書きし、成功したら true を返す
	/// @arg false コピー先に同名のファイルがある場合にはコピーをせず、false を返す
	/// @return 成功したかどうか
	static bool copy(const KPath &src, const KPath &dest, bool overwrite);

	/// ディレクトリを作成する
	///
	/// 既にディレクトリが存在する場合は成功したとみなす。
	/// ディレクトリが既に存在するかどうかを確認するためには directoryExists() を使う
	static bool makeDirectory(const KPath &dir, int *err=nullptr);

	/// ファイルを削除する
	///
	/// 指定されたファイルを削除できたなら true を返す。
	/// 指定されたファイルが初めから存在しない場合も、削除に成功したものとして true を返す。
	/// それ以外の場合は false を返す
	/// @attention ディレクトリは削除できない
	static bool removeFile(const KPath &path);

	/// ディレクトリを削除する
	///
	/// 指定されたディレクトリが空であればそれを削除する。成功したら true を返す。
	/// ディレクトリが初めから存在しなかった場合も削除に成功したものとして true を返す。
	/// それ以外の場合は false を返す
	/// @note 空でないディレクトリは削除できない
	static bool removeDirectory(const KPath &dir);

	/// ディレクトリを再帰的に削除する
	///
	/// dir ディレクトリを再帰的に操作し、最も深いディレクトリから順番に削除していく。
	/// 空でないディレクトリは無視する。dir 以下に含まれるすべてのディレクトリを削除できた場合に限り true を返す。
	/// dir ディレクトリが初めから存在しなかった場合も削除に成功したものとして true を返す。
	/// それ以外の場合は false を返す
	static bool removeDirectoryTree(const KPath &dir);

	/// dir ディレクトリ内にある全ての非ディレクトリファイルを削除する
	///
	/// subdir が true の場合は子ディレクトリに対して再帰的に removeFilesInDirectory を実行する。
	/// 全てのファイルの削除に成功した時のみ true を返す。
	/// 初めからファイルが存在しなかった場合は成功とみなす
	static bool removeFilesInDirectory(const KPath &dir, bool subdir);

	/// ファイルの最終更新日時を得る
	static time_t getLastModificationTime(const KPath &filename);

	/// ファイルサイズを得る
	static size_t getFileSize(const KPath &filename);

	/// dir 直下にあるファイルおよびディレクトリの名前リストを返す
	///
	/// 各ファイル名は dir からの相対パスの形式になっている
	static KPathList scanFiles(const KPath &dir);

	/// dir 及びそのサブフォルダに含まれているファイルおよびディレクトリの名前リストを返す
	///
	/// 各ファイル名は dir からの相対パスの形式になっている
	/// @snippet KFile.cpp Test_files_scan
	static KPathList scanFilesInTree(const KPath &dir);

};



typedef uint16_t chunk_id_t;
typedef uint32_t chunk_size_t;

/// チャンクを連結した形式のファイルの読み込みを行う。
/// ここで言うチャンク構造とは、次のブロックが連続しているフォーマットを言う
/// <pre>
/// +---------------------+
/// | Siganture (2 bytes) |  <--- Header block
/// | Size (4 bytes)      |
/// +---------------------+
/// | Data (n bytes)      |  <--- Data block
/// +---------------------+
///
/// 入れ子も
/// +---------------------+
/// | Siganture (2 bytes) |
/// | Size (4 bytes)      |
/// +---------------------+
/// | Data (n bytes) <-- 子チャンクの総サイズ
/// |                     |
/// |　　　+---------------------+
/// |　　　| Siganture (2 bytes) |
/// |　　　| Size (4 bytes)      |
/// |　　　+---------------------+
/// |　　　| Data (n bytes)      |
/// |　　　+---------------------+
/// |                     |
/// |　　　+---------------------+
/// |　　　| Siganture (2 bytes) |
/// |　　　| Size (4 bytes)      |
/// |　　　+---------------------+
/// |　　　| Data (n bytes)      |
/// |　　　+---------------------+
/// |                     |
/// +---------------------+
/// </pre>
/// データの書き込みには KChunkedFileWriter を使う
/// @snippet KFile.cpp Test_chunk
class KChunkedFileReader {
public:
	KChunkedFileReader();

	/// 読み取り対象のバイナリデータをセットし、読み取り位置を初期化する
	void init(const void *buf, size_t len);

	/// 読み取り位置がファイル終端に達しているかどうか
	bool eof() const;

	/// 次のチャンク識別子が id と一致するかどうか
	bool checkChunkId(chunk_id_t id) const;

	/// 次のチャンクの識別子とサイズを得る（読み取りポインタは移動しない）
	/// EOFに達しているか、次のチャンクが存在しない場合は何もせずに false を返す
	bool getChunkHeader(chunk_id_t *out_id, chunk_size_t *out_size) const;

	/// チャンクの入れ子を開始する
	/// チャンク識別子が id と一致しない場合はエラーになる
	void openChunk(chunk_id_t id);

	/// チャンクの入れ子を終了する
	void closeChunk();

	/// 次のチャンクをロードし、データを data にコピーする。
	/// @param id        確認用のチャンク識別子。この識別子と一致しない場合はエラーになる
	/// @param[out] data チャンクデータのコピー先。nullptrでも良い。読み取ったデータサイズは data->size() で得る
	void readChunk(chunk_id_t id, std::string *data);

	/// 次のチャンクをロードし、データを data にコピーする。
	/// @param id         readChunkHeader と同じ
	/// @param size       readChunkHeader と同じ
	/// @param[out] data  チャンクデータのコピー先。少なくとも size バイトが必要。
	void readChunk(chunk_id_t id, chunk_size_t size, void *data);

	/// 次のチャンクをロードし、データを data にコピーする。
	/// @param id         readChunkHeader と同じ
	/// @param size       readChunkHeader と同じ
	/// @param[out] data  チャンクデータのコピー先。nullptrでも良い。読み取ったデータサイズは data->size() で得る
	void readChunk(chunk_id_t id, chunk_size_t size, std::string *data);

	std::string readChunk(chunk_id_t id);
	uint8_t readChunk1(chunk_id_t id);
	uint16_t readChunk2(chunk_id_t id);
	uint32_t readChunk4(chunk_id_t id);

private:
	/// チャンクヘッダを読み取り、データ部先頭にポインタを進める。
	/// @param id    チャンクヘッダの識別子確認用。識別子が id と一致しない場合はエラーになる。確認不要の場合は 0 にする
	/// @param size  チャンクデータのサイズ確認用。サイズが size と一致しない場合はエラーになる。確認不要の場合は 0 にする
	/// 一致しない場合はエラーになる。
	void readChunkHeader(chunk_id_t id, chunk_size_t size);

	const uint8_t *m_ptr; // 現在のデータ読み取り位置
	const uint8_t *m_end; // データ終端
	const uint8_t *m_chunk_data; // 現在のチャンクのデータ部分先頭アドレス
	chunk_id_t m_chunk_id;   // 現在のチャンク識別子
	chunk_size_t m_chunk_size; // 現在のチャンクのデータ部分バイト数
	std::stack<const uint8_t *> m_nest;
};

/// チャンクを連結した形式のファイルの書き込みを行う。
/// チャンクの読み込みとフォーマットについては KChunkedFileReader を参照
/// @snippet KFile.cpp Test_chunk
class KChunkedFileWriter {
public:
	KChunkedFileWriter();
	void clear();

	/// 入れ子チャンクを開始する
	void openChunk(chunk_id_t id);

	/// 入れ子チャンクを閉じる
	void closeChunk();

	/// 1バイトデータを持つチャンクを追加する
	void writeChunk1(chunk_id_t id, uint8_t data);

	/// 2バイトデータを持つチャンクを追加する
	void writeChunk2(chunk_id_t id, uint16_t data);

	/// 4バイトデータを持つチャンクを追加する
	void writeChunk4(chunk_id_t id, uint32_t data);

	/// 不定長バイトデータを持つチャンクを追加する
	/// @param id チャンク識別子
	/// @param size バイト数
	/// @param data データ
	void writeChunkN(chunk_id_t id, chunk_size_t size, const void *data);

	/// 不定長バイトデータを持つチャンクを追加する。
	/// チャンクサイズは data.size() と同じになる
	void writeChunkN(chunk_id_t id, const std::string &data);

	/// 不定長バイトデータを持つチャンクを追加する。
	/// チャンクサイズは strlen(data) と同じになる
	void writeChunkN(chunk_id_t id, const char *data);

	/// ファイルに書き込むべきチャンクデータを返す
	const void * data() const;

	/// ファイルに書き込むべきチャンクデータのサイズを返す
	size_t size() const;

private:
	uint32_t m_pos;
	std::string m_buf;
	std::stack<uint32_t> m_stack;
};



















#pragma endregion // File.h



#pragma region Table.h

/// セル巡回用のコールバック
class KExcelScanCellsCallback {
public:
	/// セル巡回時に呼ばれる
	/// col 列番号（Excelの画面とは異なり、0起算であることに注意）
	/// row 行番号（Excelの画面とは異なり、0起算であることに注意）
	/// s セルの文字列(UTF8)
	virtual void onCell(int col, int row, const char *s) = 0;
};

class CCoreExcelReader; // internal class

class KExcelFile {
public:
	KExcelFile();

	bool empty() const;

	/// 元のファイル名を返す
	const KPath & getFileName() const;

	/// .XLSX ファイルをロードする
	bool loadFromFile(KInputStream &file, const char *xlsx_name);
	bool loadFromFileName(const char *name);
	bool loadFromMemory(const void *bin, size_t size, const char *name);

	/// シート数を返す
	int getSheetCount() const;

	/// シート名からシートを探し、見つかればゼロ起算のシート番号を返す。
	/// 見つからなければ -1 を返す
	int getSheetByName(const char *name) const;

	/// シート名を得る
	KPath getSheetName(int sheet) const;

	/// シート内で有効なセルの存在する範囲を得る。みつかれば true を返す
	/// sheet   : シート番号（ゼロ起算）
	/// col     : 左上の列番号（ゼロ起算）
	/// row     : 左上の行番号（ゼロ起算）
	/// colconut: 列数
	/// rowcount: 行数
	bool getSheetDimension(int sheet, int *col, int *row, int *colcount, int *rowcount) const;

	/// 指定されたセルの文字列を返す。文字列を含んでいない場合は "" を返す
	/// sheet: シート番号（ゼロ起算）
	/// col: 列番号（ゼロ起算）
	/// row: 行番号（ゼロ起算）
	/// retval: utf8文字列またはnullptr
	const char * getDataString(int sheet, int col, int row) const;

	/// 文字列を完全一致で検索する。みつかったらセルの行列番号を row, col にセットして true を返す
	/// 空文字列を検索することはできない。
	/// col: 見つかったセルの列番号（ゼロ起算）
	/// row: 見つかったセルの行番号（ゼロ起算）
	bool getCellByText(int sheet, const char *s, int *col, int *row) const;

	/// 全てのセルを巡回する
	/// sheet   : シート番号（ゼロ起算）
	/// cb      : セル巡回時に呼ばれるコールバックオブジェクト
	void scanCells(int sheet, KExcelScanCellsCallback *cb) const;

private:
	std::shared_ptr<CCoreExcelReader> m_impl;
	mutable KPath m_name;
};

class KExcel {
public:
	/// excel オブジェクトのセル文字列を XML 形式でエクスポートする
	static bool exportXml(const KExcelFile &excel, KOutputStream &output, bool comment=true);

	/// .xlsx ファイルをロードし、セル文字列を XML 形式でエクスポートする
	static void exportXml(const char *input_xlsx_filename, const char *output_xml_filename, bool comment=true);

	/// excel オブジェクトのセル文字列をテキスト形式でエクスポートする
	static std::string exportText(const KExcelFile &excel);

	/// .xlsx ファイルをロードし、セル文字列をテキスト形式でエクスポートする
	static std::string exportText(const char *xlsx_filenamee);

	///行列インデックス（0起算）から "A1" や "AB12" などのセル名を得る
	static bool encodeCellName(int col, int row, KPath *name);
	static KPath encodeCellName(int col, int row);

	/// "A1" や "AB12" などのセル名から、行列インデックス（0起算）を取得する
	static bool decodeCellName(const char *s, int *col, int *row);

}; // class



/// 行と列で定義されるデータテーブルを扱う。
/// 単純な「名前＝値」の形式のテーブルで良ければ KNamedValues または KOrderedParameters を使う
class KTable {
public:
	KTable();
	bool empty() const;

	/// Excel シートからテーブルオブジェクトを作成する
	/// 
	/// @param file             Excel オブジェクト
	/// @param sheet_name       シート名
	/// @param top_cell_text    テーブル範囲の左上にあるセルのテキスト。このテキストに一致するセルをシートから探し、テーブル範囲上端として設定する。
	///                         このパラメータを空文字列 "" にした場合、文字列 "@BEGIN" を指定したのと同じになる
	/// @param bottom_cell_text テーブル範囲の左下にあるセルのテキスト。このテキストに一致するセルをシートから探し、テーブル範囲下端として設定する。
	///                         このパラメータを空文字列 "" にした場合、文字列 "@END" を指定したのと同じになる
	///
	/// テーブルの行範囲は top_cell_text と btm_cell_text で挟まれた部分になる。
	/// テーブルの列範囲は top_cell_text の右隣から始まるカラムIDテキストが終わるまで
	///（top_cell_text の右側のセルを順番に見ていったとき、空白セルがあればそこがテーブルの右端になる）
	///
	/// 例えばデータシートを次のように記述する:
	/// @code
	///      | [A]    | [B]   | [C] | [D] |
	/// -----+--------+-------+-----+-----+--
	///    1 | @BEGIN | KEY   | VAL |     |
	/// -----+--------+-------+-----+-----+--
	///    2 |        | one   | 100 |     |
	/// -----+--------+-------+-----+-----+--
	///    3 | //     | two   | 200 |     |
	/// -----+--------+-------+-----+-----+--
	///    4 |        | three | 300 |     |
	/// -----+--------+-------+-----+-----+--
	///    5 |        | four  | 400 |     |
	/// -----+--------+-------+-----+-----+--
	///    6 |        | five  | 500 |     |
	/// -----+--------+-------+-----+-----+--
	///    7 | @END   |       |     |     |
	/// -----+--------+-------+-----+-----+--
	///    8 |        |       |     |     |
	/// @endcode
	/// この状態で loadFromExcelFile(excel, sheetname, "@BEGIN", "@END") を呼び出すと、A1 から C7 の範囲を KTable に読み込むことになる
	/// このようにして取得した KTable は以下のような値を返す
	/// @code
	///     KTable::getDataColIndexByName("KEY") ==> 0
	///     KTable::getDataColIndexByName("VAL") ==> 1
	///     KTable::getDataString(0, 0) ==> "one"
	///     KTable::getDataString(1, 0) ==> "100"
	///     KTable::getDataString(0, 2) ==> "three"
	///     KTable::getDataString(1, 2) ==> "300"
	///     KTable::getRowMarker(0) ==> nullptr
	///     KTable::getRowMarker(1) ==> "//"
	/// @endcode
	bool loadFromExcelFile(const KExcelFile &file, const char *sheet_name, const char *top_cell_text, const char *bottom_cell_text);

	/// テーブルを作成する。詳細は loadFromExcelFile を参照
	/// @param xmls .xlsx ファイルオブジェクト
	/// @param filename ファイル名（エラーメッセージの表示などで使用）
	/// @param sheetname シート名
	/// @param top_cell_text テーブル範囲の左上にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左上とする
	/// @param btm_cell_text テーブル範囲の左下（右下ではない）にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左下とする
	bool loadFromFile(KInputStream &xmls, const char *filename, const char *sheetname, const char *top_cell_text, const char *btm_cell_text);

	/// テーブルを作成する。詳細は loadFromExcelFile を参照
	/// @param xlsx_bin  .xlsx ファイルのバイナリデータ
	/// @param xlsx_size .xlsx ファイルのバイナリバイト数
	/// @param filename  ファイル名（エラーメッセージの表示などで使用）
	/// @param sheetname シート名
	/// @param top_cell_text テーブル範囲の左上にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左上とする
	/// @param btm_cell_text テーブル範囲の左下（右下ではない）にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左下とする
	bool loadFromExcelMemory(const void *xlsx_bin, size_t xlsx_size, const char *filename, const char *sheetname, const char *top_cell_text, const char *btm_cell_text);

	/// カラム名のインデックスを返す。カラム名が存在しない場合は -1 を返す
	int getDataColIndexByName(const char *column_name) const;

	/// 指定された列に対応するカラム名を返す
	KPath getColumnName(int col) const;
	
	/// データ列数
	int getDataColCount() const;

	/// データ行数
	int getDataRowCount() const;

	/// データ行にユーザー定義のマーカーが設定されているなら、それを返す。
	/// 例えば行全体がコメントアウトされている時には "#" や "//" を返すなど。
	const char * getRowMarker(int row) const;

	/// このテーブルのデータ文字列を得る(UTF8)
	/// @param col データ列インデックス（ゼロ起算）
	/// @param row データ行インデックス（ゼロ起算）
	/// @retval utf8文字列またはnullptr
	const char * getDataString(int col, int row) const;

	/// このテーブルのデータ整数を得る
	/// @param col データ列インデックス（ゼロ起算）
	/// @param row データ行インデックス（ゼロ起算）
	/// @param def セルが存在しない時の戻り値
	int getDataInt(int col, int row, int def=0) const;

	/// このテーブルのデータ実数を得る
	/// @param col データ列インデックス（ゼロ起算）
	/// @param row データ行インデックス（ゼロ起算）
	/// @param def セルが存在しない時の戻り値
	float getDataFloat(int col, int row, float def=0.0f) const;

	/// データ列とデータ行を指定し、それが定義されている列と行を得る
	bool getDataSource(int col, int row, int *col_in_file, int *row_in_file) const;
private:
	class Impl;
	std::shared_ptr<Impl> m_impl;
};


#define NV_TEST 0 // 0=OLD 1=PureVirtual



/// 「名前＝値」の形式のデータを順序無しで扱う
///
/// - 順序を考慮する場合は KOrderedParameters を使う。
/// - 行と列から成る二次元的なテーブルを扱いたい場合は KTable を使う
class KNamedValues {
	typedef std::pair<KPath, std::string> PAIR;
	std::vector<PAIR> mItems;
public:
	static KNamedValues * create();
#if NV_TEST
	virtual size_t size() const = 0;
	virtual void clear() = 0;
	virtual void remove(const char *name) = 0;
	virtual bool loadFromString(const char *xml_u8, const char *filename) = 0;
	virtual bool loadFromXml(KXmlReader *xml, bool packed_in_attr=false) = 0;
	virtual std::string saveToString(bool pack_in_attr=false) const = 0;
	virtual const char * getName(int index) const = 0;
	virtual void setString(const char *name, const char *value) = 0;
	virtual const char * getString(int index) const = 0;
#else
	virtual int size() const {
		return (int)mItems.size();
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
	virtual bool loadFromString(const char *xml_u8, const char *filename) {
		clear();

		bool result = false;
		KXmlElement *xml = KXmlElement::createFromString(xml_u8, filename);
		if (xml) {
			result = loadFromXml(xml->getNode(0), false);
			xml->drop();
		} else {
			KLog::printError("E_XML: Failed to load: %s", filename);
		}

		return result;
	}
	virtual bool loadFromXml(KXmlElement *elm, bool pack_in_attr=false) {
		clear();
		if (pack_in_attr) {
			// <XXX AAA="BBB" CCC="DDD" EEE="FFF" ... >
			int numAttrs = elm->getAttrCount();
			for (int i=0; i<numAttrs; i++) {
				const char *key = elm->getAttrName(i);
				const char *val = elm->getAttrValue(i);
				if (key && val) {
					setString(key, val);
				}
			}
		} else {
			// <XXX name="AAA">BBB</XXX>/>
			for (int i=0; i<elm->getNodeCount(); i++) {
				KXmlElement *xItem = elm->getNode(i);
				const char *key = xItem->findAttr("name");
				const char *val = xItem->getText();
				if (key && val) {
					setString(key, val);
				}
			}
		}
		return true;
	}

	virtual void saveToXml(KXmlElement *elm, bool pack_in_attr=false) const {
		if (pack_in_attr) {
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				elm->setAttr(it->first.u8(), it->second.c_str());
			}
		} else {
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				KXmlElement *xPair = elm->addNode("Pair");
				xPair->setAttr("name", it->first.u8());
				xPair->setText(it->second.c_str());
			}
		}
	}

	virtual std::string saveToString(bool pack_in_attr=false) const {
		std::string s;
		if (pack_in_attr) {
			s += "<KNamedValues\n";
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				s += KStringUtils::K_sprintf("\t%s = '%s'\n", it->first.u8(), it->second.c_str());
			}
			s += "/>\n";
		} else {
			s += "<KNamedValues>\n";
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				s += KStringUtils::K_sprintf("  <Pair name='%s'>%s</Pair>\n", it->first.u8(), it->second.c_str());
			}
			s += "</KNamedValues>\n";
		}
		return s;
	}
	virtual const char * getName(int index) const {
		if (0 <= index && index < (int)mItems.size()) {
			return mItems[index].first.u8();
		} else {
			return nullptr;
		}
	}
	virtual const char * getString(int index) const {
		if (0 <= index && index < (int)mItems.size()) {
			return mItems[index].second.c_str();
		} else {
			return nullptr;
		}
	}
	virtual void setString(const char *name, const char *value) {
		if (name && name[0]) {
			int index = find(name);
			if (index >= 0) {
				mItems[index].second = value ? value : "";
			} else {
				mItems.push_back(PAIR(name, value));
			}
		}
	}
#endif
	virtual KNamedValues * clone() const {
		KNamedValues *nv = KNamedValues::create();
		nv->append(this);
		return nv;
	}
	virtual bool loadFromFile(const char *filename) {
		bool success = false;
		KInputStream file = KInputStream::fromFileName(filename);
		if (file.isOpen()) {
			std::string xml_bin = file.readBin();
			std::string xml_u8 = KConv::toUtf8(xml_bin);
			if (! xml_u8.empty()) {
				success = loadFromString(xml_u8.c_str(), filename);
			}
		}
		return success;
	}
	virtual void saveToFile(const char *filename, bool pack_in_attr=false) const {
		KOutputStream file = KOutputStream::fromFileName(filename);
		if (file.isOpen()) {
			std::string xml_u8 = saveToString(pack_in_attr);
			file.write(xml_u8.c_str(), xml_u8.size());
		}
	}
	virtual void append(const KNamedValues *nv) {
		if (nv) {
			for (int i=0; i<nv->size(); i++) {
				setString(nv->getName(i), nv->getString(i));
			}
		}
	}
	virtual int find(const char *name) const {
		for (int i=0; i<size(); i++) {
			if (strcmp(getName(i), name) == 0) {
				return i;
			}
		}
		return -1;
	}
	virtual bool contains(const char *name) const {
		return find(name) >= 0;
	}
	virtual const char * getString(const char *name, const char *defaultValue=nullptr) const {
		int index = find(name);
		if (index >= 0) {
			return getString(index);
		} else {
			return defaultValue;
		}
	}

	#pragma region Bool
	virtual void setBool(const char *name, bool value) {
		setString(name, value ? "1" : "0");
	}
	virtual bool queryBool(const char *name, bool *outValue) const {
		const char *s = getString(name);
		if (s) {
			assert(strcmp(s, "on" ) != 0);
			assert(strcmp(s, "off" ) != 0);
			int val = 0;
			if (KStringUtils::toIntTry(s, &val)) {
				if (outValue) *outValue = val != 0;
				return true;
			}
		}
		return false;
	}
	virtual bool getBool(const char *name, bool defaultValue=false) const {
		bool result = defaultValue;
		queryBool(name, &result);
		return result;
	}
	#pragma endregion // Bool
	
	#pragma region Int
	virtual void setInteger(const char *name, int value) {
		char s[32] = {0};
		sprintf_s(s, sizeof(s), "%d", value);
		setString(name, s);
	}
	virtual bool queryInteger(const char *name, int *outValue) const {
		const char *s = getString(name);
		if (s && KStringUtils::toIntTry(s, outValue)) {
			return true;
		}
		return false;
	}
	virtual int getInteger(const char *name, int defaultValue=0) const {
		int result = defaultValue;
		queryInteger(name, &result);
		return result;
	}
	#pragma endregion // Int

	#pragma region UInt
	virtual void setUInt(const char *name, unsigned int value) {
		char s[32] = {0};
		sprintf_s(s, sizeof(s), "%u", value);
		setString(name, s);
	}
	virtual bool queryUInt(const char *name, unsigned int *outValue) const {
		const char *s = getString(name);
		if (s && KStringUtils::toUintTry(s, outValue)) {
			return true;
		}
		return false;
	}
	virtual int getUInt(const char *name, unsigned int defaultValue=0) const {
		unsigned int result = defaultValue;
		queryUInt(name, &result);
		return result;
	}
	#pragma endregion // Int

	#pragma region Float
	virtual void setFloat(const char *name, float value) {
		char s[32] = {0};
		sprintf_s(s, sizeof(s), "%f", value);
		setString(name, s);
	}
	virtual bool queryFloat(const char *name, float *outValue) const {
		const char *s = getString(name);
		if (s && KStringUtils::toFloatTry(s, outValue)) {
			return true;
		}
		return false;
	}
	virtual float getFloat(const char *name, float defaultValue=0.0f) const {
		float result = defaultValue;
		queryFloat(name, &result);
		return result;
	}
	#pragma endregion // Float

	#pragma region FloatArray
	virtual void setFloatArray(const char *name, const float *values, int count) {
		std::string s;
		if (count >= 1) {
			s = KStringUtils::K_sprintf("%g", values[0]);
		}
		for (int i=1; i<count; i++) {
			s += KStringUtils::K_sprintf(", %g", values[i]);
		}
		setString(name, s.c_str());
	}
	virtual int getFloatArray(const char *name, float *outValues, int maxout) const {
		const char *s = getString(name);
		KToken tok(s, ", ");
		if (outValues) {
			int idx = 0;
			while (idx<(int)tok.size() && idx<maxout) {
				outValues[idx] = KStringUtils::toFloat(tok[idx]);
				idx++;
			}
			return idx;
		} else {
			return (int)tok.size();
		}
	}
	#pragma endregion

	#pragma region Binary
	virtual void setBinary(const char *name, const void *data, int size) {
		std::string s;
		s = KStringUtils::K_sprintf("%08x:", size);
		for (int i=0; i<size; i++) {
			s += KStringUtils::K_sprintf("%02x", ((uint8_t*)data)[i]);
		}
		setString(name, s.c_str());
	}
	virtual int getBinary(const char *name, void *out, int maxsize) const {
		const char *s = getString(name);

		// コロンよりも左側の文字列を得る。データのバイト数が16進数表記されている
		int colonPos = KStringUtils::findChar(s, ':');
		if (colonPos <= 0) {
			return 0; // NO DATA 
		}

		// 得られたバイト数HEX文字列から整数値を得る
		char szstr[16] = {0};
		strncpy(szstr, s, colonPos);
		szstr[colonPos] = '\0';
		int sz = KStringUtils::toInt(szstr);

		if (sz <= 0) {
			return 0; // Empty
		}
		if (out) {
			int start = colonPos + 1; // ':' の次の文字へ移動
			int copysize = KMath::min(sz, maxsize);
			uint8_t *out_u8 = (uint8_t*)out;
			for (int i=0; i<copysize; i++) {
				char hex[3] = {s[start+i*2], s[start+i*2+1], '\0'};
				out_u8[i] = KStringUtils::toInt(hex);
			}
			return copysize;
		} else {
			return sz;
		}
	}
	#pragma endregion
};

inline void KNamedValues_del(KNamedValues *nv) {
	if (nv) delete nv;
}


class KOrderedParameters {
public:
	KOrderedParameters();
	void clear();
	int size() const;
	bool contains(const KPath &key) const;
	void add(const KPath &key, const KPath &val);
	void add(const KPath &key, int val);
	void add(const KPath &key, float val);
	void add(const KOrderedParameters &other);
	bool queryString(const KPath &key, KPath *val) const;
	bool queryInteger(const KPath &key, int *val) const;
	bool queryFloat(const KPath &key, float *val) const;
	bool getParameter(int index, KPath *key, KPath *val) const;
private:
	struct Item {
		KPath key;
		KPath val;
	};
	void _add(const Item &item);
	const Item * _get(const KPath &key) const;
	const Item * _at(int index) const;
private:
	std::vector<Item> m_items;
	std::unordered_map<KPath, int> m_key_index_map; // Key => Table index
};


#pragma endregion // Table.h



#pragma region KClock

/// 時間計測のためのクラス
class KClock {
public:
	/// システム時刻をナノ秒単位で得る
	static uint64_t getSystemTimeNano64();

	/// システム時刻をミリ秒単位で取得する
	static uint64_t getSystemTimeMsec64();
	static int getSystemTimeMsec();
	static float getSystemTimeMsecF();

	/// システム時刻を秒単位で取得する
	static int getSystemTimeSec();
	static float getSystemTimeSecF();

public:
	KClock();

	/// カウンタをゼロにセットする
	/// @note
	/// ここで前回のリセットからの経過時間を返せば便利そうだが、
	/// ミリ秒なのかナノ秒なのかがあいまいなので何も返さないことにする
	void reset();

	/// reset() を呼んでからの経過時間をナノ秒単位で返す
	uint64_t getTimeNano64() const;

	/// reset() を呼んでからの経過時間をミリ秒単位で返す
	uint64_t getTimeMsec64() const;

	/// reset() を呼んでからの経過時間を秒単位で返す
	float getTimeSecondsF() const;

	/// getTimeNano64() の int 版
	int getTimeNano() const;

	/// getTimeMsec64() の int 版
	int getTimeMsec() const;

	/// getTimeMsec64() の float 版
	float getTimeMsecF() const;

private:
	uint64_t m_start;
};

#pragma endregion // KClock








class KLuaBankCallback {
public:
	virtual void on_luabank_load(lua_State *ls, const char *name) = 0;
};

class KLuaBank {
public:
	KLuaBank();
	~KLuaBank();

	void setCallback(KLuaBankCallback *cb);

	lua_State * addEmptyScript(const char *name);
	lua_State * findScript(const char *name) const;
	lua_State * queryScript(const char *name, bool reload=false);
	lua_State * makeThread(const char *name);
	bool contains(const char *name) const;
	void remove(const char *name);
	void clear();
	bool addScript(const char *name, const char *code, size_t size);

private:
	std::unordered_map<std::string, lua_State *> m_items;
	mutable std::recursive_mutex m_mutex;
	KLuaBankCallback *m_cb;
};



#pragma region Math.h






class KRect {
public:
	KRect();
	KRect(const KRect &rect);
	KRect(int _xmin, int _ymin, int _xmax, int _ymax);
	KRect(float _xmin, float _ymin, float _xmax, float _ymax);
	KRect(const KVec2 &pmin, const KVec2 &pmax);
	KRect getOffsetRect(const KVec2 &p) const;
	KRect getOffsetRect(float dx, float dy) const;

	/// 外側に dx, dy だけ広げた矩形を返す（負の値を指定した場合は内側に狭める）
	KRect getInflatedRect(float dx, float dy) const;

	/// rc と交差しているか判定する
	bool isIntersectedWith(const KRect &rc) const;

	/// rc と交差しているならその範囲を返す
	KRect getIntersectRect(const KRect &rc) const;
	KRect getUnionRect(const KVec2 &p) const;
	KRect getUnionRect(const KRect &rc) const;
	bool isZeroSized() const;
	bool isEqual(const KRect &rect) const;
	bool contains(const KVec2 &p) const;
	bool contains(float x, float y) const;
	bool contains(int x, int y) const;
	KVec2 getCenter() const;
	KVec2 getMinPoint() const;
	KVec2 getMaxPoint() const;
	float getSizeX() const;
	float getSizeY() const;

	/// 外側に向けて dx, dy だけ膨らませる
	void inflate(float dx, float dy);

	/// rect 全体を dx, dy だけ移動する
	void offset(float dx, float dy);
	void offset(int dx, int dy);

	/// rect を含むように矩形を拡張する
	/// unionX X方向について拡張する
	/// unionY Y方向について拡張する
	void unionWith(const KRect &rect, bool unionX=true, bool unionY=true);

	/// 数直線上で座標 x を含むように xmin, xmax を拡張する
	void unionWithX(float x);

	/// 数直線上で座標 y を含むように ymin, ymax を拡張する
	void unionWithY(float y);

	/// xmin <= xmax かつ ymin <= ymax であることを保証する
	void sort();
	//
	float xmin;
	float ymin;
	float xmax;
	float ymax;
};


#pragma region KCubicBezier
/// 3次のベジェ曲線を扱う
///
/// 3次ベジェ曲線は、4個の制御点から成る。
/// 制御点0 : 始点 (= アンカー)
/// 制御点1 : 始点側ハンドル
/// 制御点2 : 終点側ハンドル
/// 制御点3 : 終点
/// <pre>
/// handle            handle
///  (1)                (2)
///   |    segment       |
///   |  +------------+  |
///   | /              \ |
///   |/                \|
///  [0]                [3]
/// anchor             anchor
/// </pre>
/// セグメントを連続してつなげていき、1本の連続する曲線を表す。
/// そのため、セグメント接続部分での座標と傾きは一致している必要がある
/// 次の図で言うと、セグメントAの終端点[3]と、セグメントBの始点[0]は同一座標に無いといけない。
/// また、滑らかにつながるには、セグメントAの終端傾き (2)→[3] とセグメントBの始点傾き [0]→(1)
/// が一致していないといけない
/// <pre>
///  (1)                (2)
///   |    segment A     |
///   |  +------------+  |
///   | /              \ |
///   |/                \|
///  [0]                [3]
///                     [0]                [3]
///                      |\                /|
///                      | \              / |
///                      |  +------------+  |
///                     (1)    segment B   (2)
/// </pre>
/// @see https://ja.javascript.info/bezier-curve
/// @see https://postd.cc/bezier-curves/
class KCubicBezier {
public:
	KCubicBezier();

	void clear();
	bool empty() const;

	/// 区間インデックス seg における３次ベジェ曲線の、係数 t における座標を得る。
	/// 成功した場合は pos に座標をセットして true を返す。
	/// 範囲外のインデックスを指定した場合は何もせずに false を返す
	/// @param      seg 区間インデックス
	/// @param      t   区間内の位置を表す値。区間を 0.0 以上 1.0 以下で正規化したもの
	/// @param[out] pos 曲線上の座標
	bool getCoordinates(int seg, float t, KVec3 *pos) const;
	KVec3 getCoordinates(int seg, float t) const;
	KVec3 getCoordinatesEx(float t) const;

	/// 区間インデックス seg における３次ベジェ曲線の、係数 t における傾きを得る。
	/// 成功した場合は tangent に傾きをセットして true を返す。
	/// 範囲外のインデックスを指定した場合は何もせずに false を返す
	/// @param      seg     区間インデックス
	/// @param      t       区間内の位置を表す値。区間を 0.0 以上 1.0 以下で正規化したもの
	/// @param[out] tangent 傾きの値
	bool getTangent(int seg, float t, KVec3 *tangent) const;

	/// getTangent(int, float, KVec3*) と同じだが、傾きを直接返す
	KVec3 getTangent(int seg, float t) const;

	/// 区間インデックス seg における３次ベジェ曲線の長さを得る
	/// @param seg 区間インデックス
	float getLength(int seg) const;
	
	/// テスト用。曲線を numdiv 個の直線に分割して近似長さを求める
	float getLength_Test1(int seg, int numdiv) const;
	
	/// テスト用。曲線を等価なベジェ曲線に再帰分割して近似長さを求める
	float getLength_Test2(int seg) const;

	/// 全体の長さを返す
	float getWholeLength() const;

	int getPointCount() const;

	/// 区間数を返す
	int getSegmentCount() const;
	
	/// 制御点の数を変更する
	void setSegmentCount(int count);

	/// 区間を追加する。
	/// １区間には４個の制御点が必要になる。
	/// @param a 始点アンカー
	/// @param b 始点ハンドル。始点での傾きを決定する
	/// @param c 終点ハンドル。終点での傾きを決定する
	/// @param d 終点アンカー
	void addSegment(const KVec3 &a, const KVec3 &b, const KVec3 &c, const KVec3 &d);

	/// 区間インデックス seg の index 番目にある制御点の座標を設定する
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param point  制御点番号。制御点は4個で、始点から順番に 0, 1, 2, 3 となる
	/// @param pos    制御点座標
	void setPoint(int seg, int point, const KVec3 &pos);

	/// コントロールポイントの通し番号を指定して座標を指定する。
	/// 例えば 0 は区間[0]の始点を、4は区間[1]の始点を、15は区間[3]の終点を表す
	void setPoint(int serial_index, const KVec3 &pos);

	/// 区間インデックス seg の index 番目にある制御点の座標を返す
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param point  制御点番号。制御点は4個で、始点から順番に 0, 1, 2, 3
	KVec3 getPoint(int seg, int point) const;

	/// コントロールポイントの通し番号を指定して座標を取得する
	KVec3 getPoint(int serial_index) const;

	/// 区間インデックス seg における３次ベジェ曲線の始点側アンカーとハンドルポイントを得る
	/// @param      seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param[out] anchor 始点側アンカー
	/// @param[out] handle 始点側アンカーのハンドルポイント
	bool getFirstAnchor(int seg, KVec3 *anchor, KVec3 *handle) const;

	/// 区間インデックス seg における３次ベジェ曲線の終点側アンカーとハンドルポイントを得る
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param[out] anchor 終点側アンカー
	/// @param[out] handle 終点側アンカーのハンドルポイント
	bool getSecondAnchor(int seg, KVec3 *handle, KVec3 *anchor) const;

	/// 区間インデックス seg における３次ベジェ曲線の始点側アンカーとハンドルポイントを指定する
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param anchor 始点側アンカー
	/// @param handle 始点側アンカーのハンドルポイント
	void setFirstAnchor(int seg, const KVec3 &anchor, const KVec3 &handle);

	/// 区間インデックス seg における３次ベジェ曲線の終点側アンカーとハンドルポイントを指定する
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param anchor 終点側アンカー
	/// @param handle 終点側アンカーのハンドルポイント
	void setSecondAnchor(int seg, const KVec3 &handle, const KVec3 &anchor);

private:
	std::vector<KVec3> points_; /// 制御点の配列
	mutable float length_; /// 曲線の始点から終点までの経路長さ（ループしている場合は一周の長さ）
	mutable bool dirty_length_;
};
#pragma endregion // KCubicBezier


class KCollisionMath {
public:
	/// 三角形の法線ベクトルを返す
	///
	/// 三角形法線ベクトル（正規化済み）を得る
	/// 時計回りに並んでいるほうを表とする
	static bool getTriangleNormal(KVec3 *result, const KVec3 &o, const KVec3 &a, const KVec3 &b);

	static bool isAabbIntersected(const KVec3 &pos1, const KVec3 &halfsize1, const KVec3 &pos2, const KVec3 &halfsize2, KVec3 *intersect_center=nullptr, KVec3 *intersect_halfsize=nullptr);

	/// 直線 ab で b 方向を見たとき、点(px, py)が左にあるなら正の値を、右にあるなら負の値を返す
	static float getPointInLeft2D(float px, float py, float ax, float ay, float bx, float by);

	/// 直線と点の有向距離（点AからBに向かって左側が負、右側が正）
	static float getSignedDistanceOfLinePoint2D(float px, float py, float ax, float ay, float bx, float by);

	/// 点 p から直線 ab へ垂線を引いたときの、垂線と直線の交点 h を求める。
	/// 戻り値は座標ではなく、点 a b に対する点 h の内分比を返す（点 h は必ず直線 ab 上にある）。
	/// 戻り値を k としたとき、交点座標は b * k + a * (1 - k) で求まる。
	/// 戻り値が 0 未満ならば交点は a の外側にあり、0 以上 1 未満ならば交点は ab 間にあり、1 以上ならば交点は b の外側にある。
	static float getPerpendicularLinePoint(const KVec3 &p, const KVec3 &a, const KVec3 &b);
	static float getPerpendicularLinePoint2D(float px, float py, float ax, float ay, float bx, float by);

	/// 点が矩形の内部であれば true を返す
	static bool isPointInRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf);

	/// 点が剪断矩形（X方向）の内部であれば true を返す
	static bool isPointInXShearedRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf, float xShear);

	/// 円が点に衝突しているか確認し、衝突している場合は円の補正移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円を (xAdj, yAdj) だけ移動させるか、点を (-xAdj, -yAdj) だけ移動させる。
	static bool collisionCircleWithPoint2D(float cx, float cy, float cr, float px, float py, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);

	/// 円が直線に衝突しているか確認し、衝突している場合は、円を直線の左側に押し出すための移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円を (xAdj, yAdj) だけ移動させるか、直線を (-xAdj, -yAdj) だけ移動させる。
	static bool collisionCircleWithLine2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);
	
	/// collisionCircleWithLine2D と似ているが、円が直線よりも右側にある場合は常に接触しているとみなし、直線の左側に押し出す
	static bool collisionCircleWithLinearArea2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);

	/// 円が線分に衝突しているか確認し、衝突している場合は円の補正移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円を (xAdj, yAdj) だけ移動させるか、直線を (-xAdj, -yAdj) だけ移動させる。
	static bool collisionCircleWithSegment2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);

	/// 円同士が衝突しているか確認し、衝突している場合は円の補正移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円1を (xAdj, yAdj) だけ移動させるか、円2を (-xAdj, -yAdj) だけ移動させる。
	/// 双方の円を平均的に動かして解消するなら、円1を (xAdj/2, yAdj/2) 動かし、円2を(-xAdj/2, -yAdj/2) だけ動かす
	static bool collisionCircleWithCircle2D(float x1, float y1, float r1, float x2, float y2, float r2, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);

	/// 円と矩形が衝突しているなら、矩形のどの辺と衝突したかの値をフラグ組汗で返す
	/// 1 左(X負）
	/// 2 上(Y正）
	/// 4 右(X正）
	/// 8 下(Y負）
	static int collisionCircleWithRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);
	static int collisionCircleWithXShearedRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float xShear, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);

	/// p1 を通り Y 軸に平行な半径 r1 の円柱と、
	/// p2 を通り Y 軸に平行な半径 r2 の円柱の衝突判定と移動を行う。
	/// 衝突しているなら p1 と p2 の双方を X-Z 平面上で移動させ、移動後の座標を p1 と p2 にセットして true を返す
	/// 衝突していなければ何もせずに false を返す
	/// k1, k2 には衝突した場合のめり込み解決感度を指定する。
	///     0.0 を指定すると全く移動しない。1.0 だと絶対にめり込まず、硬い感触になる
	///     0.2 だとめり込み量を 20% だけ解決する。めり込みの解決に時間がかかり、やわらかい印象になる。
	///     0.8 だとめり込み量を 80% だけ解決する。めり込みの解決が早く、硬めの印象になる
	/// skin 衝突判定用の許容距離を指定する。0.0 だと振動しやすくなる
	static bool resolveYAxisCylinderCollision(KVec3 *p1, float r1, float k1, KVec3 *p2, float r2, float k2, float skin);
};


#pragma endregion // Math.h













class KController {
public:
	KController() {
		m_axis_x = 0;
		m_axis_y = 0;
		m_axis_z = 0;
		m_axis_reduce = 0.2f; // 1フレーム当たりの軸入力の消失値（正の値を指定）
		m_peeked.clear();
		m_buttons.clear();
		m_tirgger_timeout = 10;
		m_readtrigger = 0;
	}
	virtual void setTriggerTimeout(int val) {
		m_tirgger_timeout = val;
	}
	virtual void clearInputs() {
		m_axis_x = 0;
		m_axis_y = 0;
		m_axis_z = 0;
		m_buttons.clear();
		m_peeked.clear();
	}

	// トリガー入力をリセットする
	// なお beginReadTrigger/endReadTrigger で読み取りモードにしている場合でも clearTrighgers は関係なく動作する
	virtual void clearTriggers() {
		for (auto it=m_buttons.begin(); it!=m_buttons.end(); /*++it*/) {
			if (it->second >= 0) {
				it = m_buttons.erase(it);
			} else {
				it++;
			}
		}
		m_peeked.clear();
	}
	virtual void setInputAxisX(int i) {
		m_axis_x = (float)i;
	}
	virtual void setInputAxisY(int i) {
		m_axis_y = (float)i;
	}
	virtual void setInputAxisZ(int i) {
		m_axis_z = (float)i;
	}
	virtual int getInputAxisX() {
		return (int)KMath::signf(m_axis_x); // returns -1, 0, 1
	}
	virtual int getInputAxisY() {
		return (int)KMath::signf(m_axis_y); // returns -1, 0, 1
	}
	virtual int getInputAxisZ() {
		return (int)KMath::signf(m_axis_z); // returns -1, 0, 1
	}

	virtual void setInputTrigger(const char *button) {
		if (KStringUtils::isEmpty(button)) return;
		m_buttons[button] = m_tirgger_timeout;
	}
	virtual void setInputBool(const char *button, bool pressed) {
		if (KStringUtils::isEmpty(button)) return;
		if (pressed) {
			m_buttons[button] = -1;
		} else {
			m_buttons.erase(button);
		}
	}
	virtual bool getInputBool(const char *button) {
		if (KStringUtils::isEmpty(button)) return false;
		return m_buttons.find(button) != m_buttons.end();
	}

	/// トリガーボタンの状態を得て、トリガー状態を false に戻す。
	/// トリガーをリセットしたくない場合は peekInputTrigger を使う。
	/// また、即座にリセットするのではなく特定の時点までトリガーのリセットを
	/// 先延ばしにしたい場合は beginReadTrigger() / endReadTrigger() を使う
	virtual bool getInputTrigger(const char *button) {
		if (KStringUtils::isEmpty(button)) return false;
		auto it = m_buttons.find(button);
		if (it != m_buttons.end()) {
			if (m_readtrigger > 0) {
				// PEEKモード中。
				// トリガーを false に戻さず、参照されたことだけを記録しておく
				m_peeked.insert(button);
			} else {
				// トリガー状態を false に戻す
				m_buttons.erase(it);
			}
			return true;
		}
		return false;
	}
	
	/// トリガーの読み取りモードを開始する
	/// endReadTrigger が呼ばれるまでの間は getInputTrigger を呼んでもトリガー状態が false に戻らない
	virtual void beginReadTrigger() {
		m_readtrigger++;
	}

	/// トリガーの読み取りモードを終了する。
	/// beginReadTrigger が呼ばれた後に getInputTrigger されたトリガーをすべて false に戻す
	virtual void endReadTrigger() { 
		m_readtrigger--;
		if (m_readtrigger == 0) {
			for (auto it=m_peeked.begin(); it!=m_peeked.end(); ++it) {
				const std::string &btn = *it;
				m_buttons.erase(btn);
			}
			m_peeked.clear();
		}
	}
	virtual void tickInput() {
		// 入力状態を徐々に消失させる
		m_axis_x = reduce(m_axis_x, m_axis_reduce);
		m_axis_y = reduce(m_axis_y, m_axis_reduce);
		m_axis_z = reduce(m_axis_z, m_axis_reduce);

		// トリガー入力のタイムアウトを処理する
		for (auto it=m_buttons.begin(); it!=m_buttons.end(); /*EMPTY*/) {
			if (it->second > 0) {
				it->second--;
				it++;
			} else if (it->second == 0) {
				it = m_buttons.erase(it);
			} else {
				it++; // タイムアウトが負の値の場合なら何もしない
			}
		}
	}

private:
	float reduce(float val, float delta) const {
		if (val > 0) {
			val = KMath::max(val - delta, 0.0f);
		}
		if (val < 0) {
			val = KMath::min(val + delta, 0.0f);
		}
		return val;
	}
	std::unordered_map<std::string, int> m_buttons;
	std::unordered_set<std::string> m_peeked;
	float m_axis_x;
	float m_axis_y;
	float m_axis_z;
	float m_axis_reduce;
	int m_tirgger_timeout;
	int m_readtrigger;
};









namespace Test {
void Test_files_scan();
void Test_chunk();
void Test_bezier();
void Test_luapp();

}


} // namespace


