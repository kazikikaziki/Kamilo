﻿#include "KExcel.h"

#include <unordered_map>
#include "KStream.h"
#include "KInternal.h"
#include "KZip.h"
#include "KLog.h"
#include "KXml.h"


namespace Kamilo {


#define EXCEL_ALPHABET_NUM 26
#define EXCEL_COL_LIMIT    (EXCEL_ALPHABET_NUM * EXCEL_ALPHABET_NUM)
#define EXCEL_ROW_LIMIT    1024
#define HAS_CHAR(str, chr) (strchr(str, chr) != nullptr)


// 文字列 s を xml に組み込めるようにする
static void _EscapeString(std::string &s) {
	KStringUtils::replace(s, "\\", "\\\\");
	KStringUtils::replace(s, "\"", "\\\"");
	KStringUtils::replace(s, "\n", "\\n");
	KStringUtils::replace(s, "\r", "");
	if (s.find(',') != std::string::npos) {
		s.insert(0, "\"");
		s.append("\"");
	}
}

// 文字列 s をエスケープする必要がある？
static bool _ShouldEscapeString(const char *s) {
	K__Assert(s);
	return HAS_CHAR(s, '<') || HAS_CHAR(s, '>') || HAS_CHAR(s, '"') || HAS_CHAR(s, '\'') || HAS_CHAR(s, '\n');
}

// ZIP 内のファイルを探してインデックスを返す
static int _FindZipEntry(KUnzipper &zr, const char *name) {
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
static KXmlElement * _LoadXmlFromZip(KUnzipper &zr, const char *zipname, const char *entry_name) {
	if (!zr.isOpen()) {
		KLog::printError("E_INVALID_ARGUMENT");
		return nullptr;
	}

	// XLSX の XML は常に UTF-8 で書いてある。それを信用する
	int fileid = _FindZipEntry(zr, entry_name);
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
		K__Assert(isalpha(c1));
		K__Assert(isalpha(c2));
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
		K__Assert(0 <= c && c < EXCEL_ALPHABET_NUM);
		r = strtol(s + 1, nullptr, 0);
		r--; // １起算 --> 0起算

	} else if (isalpha(s[0]) && isalpha(s[1]) && isdigit(s[2])) {
		// セル番号が [A-Z][A-Z][0-9].* にマッチしている。
		// 例えば "AA42" や "KZ1217" など
		int idx1 = toupper(s[0]) - 'A';
		int idx2 = toupper(s[1]) - 'A';
		K__Assert(0 <= idx1 && idx1 < EXCEL_ALPHABET_NUM);
		K__Assert(0 <= idx2 && idx2 < EXCEL_ALPHABET_NUM);
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
		K__Assert(0 <= c && c < EXCEL_COL_LIMIT);
		K__Assert(0 <= r && r < EXCEL_ROW_LIMIT);
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
			K__Assert(last_row_ <= row); // 行番号は必ず前回と等しいか、大きくなる
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
			if (_ShouldEscapeString(s)) { // xml禁止文字が含まれているなら CDATA 使う
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
					_EscapeString(ss);
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
		K__Assert(root_elm);

		const KXmlElement *sheets_xml = root_elm->findNode("sheets");
		K__Assert(sheets_xml);

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
					K__Assert(id == 1 + idx);
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
		K__Assert(root_elm);

		const KXmlElement *sheets_xml = root_elm->findNode("sheets");
		K__Assert(sheets_xml);

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
					K__Assert(id == 1 + idx);
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
		K__Assert(xdoc);

		const KXmlElement *xroot = xdoc->getNode(0);
		K__Assert(xroot);

		const KXmlElement *xdim = xroot->findNode("dimension");
		K__Assert(xdim);

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

		K__Assert(col);
		K__Assert(row);

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
		K__Assert(doc);

		const KXmlElement *root_elm = doc->getNode(0);
		K__Assert(root_elm);

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
		const KXmlElement *strings_doc = _LoadXmlFromZip(zr, xlsx_name, "xl/sharedStrings.xml");
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
		m_workbook_doc = _LoadXmlFromZip(zr, xlsx_name, "xl/workbook.xml");

		// ワークシートを取得
		for (int i=1; ; i++) {
			char s[256];
			sprintf_s(s, sizeof(s), "xl/worksheets/sheet%d.xml", i);
			if (_FindZipEntry(zr, s) < 0) break;
			KXmlElement *sheet_doc = _LoadXmlFromZip(zr, xlsx_name, s);
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
			K__Assert(sid >= 0);
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
					K__Assert(cidx >= 0 && ridx >= 0);
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
		K__Assert(doc);

		const KXmlElement *root_elm = doc->getNode(0);
		K__Assert(root_elm);

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
		K__Assert(type);
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
				col0_ = K__Min(col0_, col);
				col1_ = K__Max(col1_, col);
				row0_ = K__Min(row0_, row);
				row1_ = K__Max(row1_, row);
			}
		}
	};
};


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
		K__Assert(rows > 0);

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


} // namespace