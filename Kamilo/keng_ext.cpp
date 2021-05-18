#include "keng_ext.h"

#include <math.h>
#include <algorithm>
#include <unordered_set>
#include <mutex>
#include <stdarg.h>
#include <locale.h> // set_locale for Test

#ifdef _WIN32
#	include <Windows.h>
#	include <DbgHelp.h> // SymInitialize
#	include <tlhelp32.h> // CreateToolhelp32Snapshot
#	include <Shlobj.h> // SHGetFolderPath
#	include <Shlwapi.h>
#	include <d3d9.h>
#	pragma comment(lib, "shlwapi.lib")
#endif

#pragma region IMGUI
// ImGui::GetFont が win32api の GetFont マクロと競合するため、
// Windows.h よりも前に include する
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h" // ImGui::GetCurrentWindow()
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#pragma endregion // IMGUI

#include "KInternal.h"
#include "KFile.h"
#include "KExcel.h"
#include "KStorage.h"
#include "KRes.h"
#include "KDirectoryWalker.h"
#include "KZlib.h"
#include "KZip.h"


#define CORE_SYS 1



namespace Kamilo {


// 格納可能な文字数を超えたら、もう何もできない。
// 下手に長さを切り詰めたまま処理を続行すると、
// 意図しないファイル名やディレクトリになったりして
// 危険すぎるため、ここで強制終了するのが良い
#define TOO_LONG_PATH_ERROR()  KLog::printFatal("Too long path string")

} // namespace
