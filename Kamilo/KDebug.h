/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <stdarg.h> // va_list
#include <string>


#ifdef _DEBUG
#  define K_assert(x)  K_verify(x)
#else
#  define K_assert(x)  ((void)0)
#endif


/// 式 expr が真であることを確認する。そうでない場合はエラー処理をする
/// assert とは異なり、デバッグ版でもリリース版でも関係なく常に式 expr が実行される
#define K_verify(expr)   ((expr) ? (void)0: K_assertion_failure(__FILE__, __LINE__, #expr))

/// ソースコード中のファイル名と行番号をログに出力する
#define K_trace()            KLog::printTrace(__FUNCTION__, __LINE__)
#define K_tracef(fmt, ...)   KLog::printTracef(__FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)


namespace Kamilo {

void K_assertion_failure(const char *file, int line, const char *expr); ///< アサーションエラー

#pragma region Log

/// 内部メッセージを捕まえる
void K_log_hook_internal_messages();

#pragma endregion // Log


} // namespace
