/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <string>
#include "KRef.h"
#include "KString.h"
#include "KStream.h"

#define K_FILE_CHECK_REFCNT 1 // K_REFCNT_DEBUG


namespace Kamilo {





#pragma region File operation
/// @name File
/// @{

/// ファイルをコピーする
bool K_FileCopy(const char *src_u8, const char *dest_u8, bool overwrite);

/// ディレクトリを作成する
bool K_FileMakeDir(const char *dir_u8);

/// ファイルを削除する
bool K_FileRemove(const char *path_u8);

/// 空のディレクトリを削除する
bool K_FileRemoveEmptyDir(const char *dir_u8);

/// 空のディレクトリを再帰的に削除する
bool K_FileRemoveEmptyDirTree(const char *dir_u8);

/// dir ディレクトリ内にある全ての非ディレクトリファイルを削除する
bool K_FileRemoveFilesInDir(const char *dir_u8);

/// dir ディレクトリ内にある全ての非ディレクトリファイルを再帰的に削除する
bool K_FileRemoveFilesInDirTree(const char *dir_u8);

/// @}
#pragma endregion // File operation





#pragma region KReader
/// ストリームからデータを読みだす
class KReader: public virtual KRef {
public:
	static KReader * createFromFileName(const char *filename);
	static KReader * createFromMemory(const void *data, int size);
	static KReader * createFromMemoryCopy(const void *data, int size);
	static KReader * createFromStream(KInputStream &input);
	static std::string readBinFromFileName(const char *filename);

	/// 読み取り位置。先頭からのオフセットバイト数
	virtual int tell() = 0;

	/// sizeバイトを読み取って data にコピーする。
	/// data が NULLの場合は単なるシークになる
	virtual int read(void *data, int size) = 0;
	
	/// 読み取り位置を設定する。先頭からのオフセットバイト数で指定する
	/// 0 <= pos <= size()
	virtual void seek(int pos) = 0;

	/// 先頭から末尾までのバイト数
	virtual int size() = 0;

	virtual uint16_t read_uint16() {
		const int size = 2;
		uint16_t val = 0;
		if (read(&val, size) == size) {
			return val;
		}
		return 0;
	}
	virtual uint32_t read_uint32() {
		const int size = 4;
		uint32_t val = 0;
		if (read(&val, size) == size) {
			return val;
		}
		return 0;
	}
	virtual std::string read_bin(int readsize=-1) {
		if (readsize < 0) {
			readsize = size(); // 現在位置から終端までのサイズ
		}
		if (readsize > 0) {
			std::string bin(readsize, '\0');
			int sz = read((void*)bin.data(), bin.size());
			bin.resize(sz);// bin.size() が実際のデータ長さを示すように
			return bin;
		}
		return std::string(); // 読み取りサイズゼロ
	}

};
#pragma endregion // KReader


#pragma region KWriter
/// ストリームにデータを書き込む
class KWriter: public virtual KRef {
public:
	static KWriter * createFromFileName(const char *filename);
	static KWriter * createFromMemory(std::string *dest);

	virtual ~KWriter() {}

	/// 現在の書き込み位置をストリーム先頭からのバイト数で返す
	virtual int tell() = 0;

	/// 現在の書き込み位置にデータを書き込む
	virtual int write(const void *data, int size) = 0;

	/// 16ビット符号なし整数値を書き込む
	virtual int write_uint16(uint16_t value) {
		return write(&value, sizeof(value));
	}

	/// 32ビット符号なし整数値を書き込む
	virtual int write_uint32(uint32_t value) {
		return write(&value, sizeof(value));
	}

	/// ヌル終端文字列を書き込む。
	/// 文字コードなどは一切考慮せず s で指定されたままのバイナリを書き込む
	virtual int write_string(const char *s) {
		return write(s, strlen(s));
	}
};
#pragma endregion // KWriter




namespace Test {
void Test_reader();
void Test_writer();
}

} // namespace
