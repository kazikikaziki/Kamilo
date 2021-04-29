#pragma once
#include "KString.h"
#include <memory>

namespace Kamilo {

class KInputStream;
class KOutputStream;

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




} // namespace
