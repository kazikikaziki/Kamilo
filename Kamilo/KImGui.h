#pragma once
#include <inttypes.h>

// Dear ImGui
// - MIT lisence
// - https://github.com/ocornut/imgui
#include "imgui/imgui.h"

#include "KVideo.h"
#include "KString.h"

namespace Kamilo {

/// @file
/// Dear ImGui のサポート関数群。
///
/// Dear ImGui ライブラリは既にソースツリーに同梱されているので、改めてダウンロード＆インストールする必要はない。
/// 利用可能な関数などについては imgui/imgui.h を直接参照すること。また、詳しい使い方などは imgui/imgui.cpp に書いてある。
namespace KImGui {
	struct ImgParams {
		ImgParams() {
			w = 0;
			h = 0;
			u0 = 0.0f;
			v0 = 0.0f;
			u1 = 1.0f;
			v1 = 1.0f;
			color = KColor(1, 1, 1, 1);
			border_color = KColor(0, 0, 0, 0);
			keep_aspect = true;
			linear_filter = false;
		}
		int w;
		int h;
		float u0;
		float v0;
		float u1;
		float v1;
		KColor color;
		KColor border_color;
		bool keep_aspect;
		bool linear_filter;
	};

	const ImVec4 KImGui_COLOR_STRONG = {1.0f, 0.8f, 0.2f, 1.0f}; // 強調テキスト色
	const ImVec4 KImGui_COLOR_WARNING(1.0f, 0.8f, 0.2f, 1.0f); // 警告テキスト色
	const ImVec4 KImGui_COLOR_ERROR  (1.0f, 0.5f, 0.5f, 1.0f); // エラー色

	bool KImGui_ButtonRepeat(const char *label, ImVec2 size=ImVec2(0, 0));
	bool KImGui_Combo(const char *label, int *current_item, const KPathList &items, int height_in_items=-1);
	bool KImGui_Combo(const char *label, const KPathList &pathlist, const KPath &selected, int *out_selected_index);
	int  KImGui_FontCount();
	void KImGui_Image(KTEXID tex, ImgParams *p);
	void KImGui_Image(KTEXID tex, const ImVec2& size, const ImVec2& uv0 = ImVec2(0,0), const ImVec2& uv1 = ImVec2(1,1), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0));
	void KImGui_ImageExportButton(const char *label, KTEXID tex, const KPath &filename, bool alphamask);
	void KImGui_HSpace(int size);
	void KImGui_PushFont(int index);// index: an index of ImFont in ImGui::GetIO().Fonts->Fonts
	void KImGui_PopFont();
	void KImGui_StyleKK();
	void KImGui_VSpace(int size=8);
	bool KImGui_InitWithDX9(void *hWnd, void *d3ddev9);
	void KImGui_Shutdown();
	bool KImGui_IsActive();
	void KImGui_DeviceLost();
	void KImGui_DeviceReset();
	long KImGui_WndProc(void *hWnd, uint32_t msg, uintptr_t wp, uintptr_t lp);
	void KImGui_BeginRender();
	void KImGui_EndRender();
	ImFont * KImGui_AddFontFromMemoryTTF(const void *data, size_t size, float size_pixels);
	ImFont * KImGui_AddFontFromFileTTF(const char *filename_u8, int ttc_index, float size_pixels);
	void KImGui_SetFilterPoint(); // ImGui::Image のフィルターを POINT に変更
	void KImGui_SetFilterLinear(); // ImGui::Image のフィルターを LINEAR に変更
	void KImGui_PushTextColor(const ImVec4 &col);
	void KImGui_PopTextColor();

	template <typename T> void * KImGui_ID(T x) { return (void*)x; }
	ImVec4 KImGui_COLOR_DEFAULT();
	ImVec4 KImGui_COLOR_DISABLED();

	void KImGui_ShowExampleAppCustomNodeGraph(bool* opened);
} // namespace KImGui

class KImGuiCombo {
	typedef std::pair<std::string, int> Pair;
	std::vector<Pair> mItems;
	std::vector<const char *> mPChars;
	int mUpdating;
public:
	KImGuiCombo();
	void begin();
	void end();
	void addItem(const char *s, int value=0);
	int indexOfText(const char *s) const;
	int indexOfValue(int value) const;
	const char * getText(int index) const;
	int getValue(int index) const;
	const char ** getItemData() const;
	int getCount() const;
	bool showGui(const char *label, std::string &value) const;
	bool showGui(const char *label, int *value) const;
};

} // namespace
