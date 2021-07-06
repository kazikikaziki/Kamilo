#pragma once
#include <string>

/// Pac ファイル情報を書き出す
bool ez_ExportPacFileInfo(const std::string &pac_filename_u8);

/// XLSX 内のテキストを抜き出す
bool ez_ExportTextFromXLSX(const std::string &xlsx_filename);

/// EDGE2 で出力した PAL ファイルを XML でエクスポートする
bool ez_ExportPalXmlFromEdge2(const std::string &edg_filename);

/// EDGE2 ファイルの中身をエクスポートする
bool ez_ExportEdge2(const std::string &edg_filename);

/// フォルダ内の .png を全てパックする
bool ez_PackPngInDir(const std::string &dir);
