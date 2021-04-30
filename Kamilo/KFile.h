#pragma once

namespace Kamilo {

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



} // namespace
