#include "KSnapshotter.h"
//
#include "keng_game.h"
#include "KInternal.h"
#include "KSig.h"
#include "KKeyboard.h"
#include "KLocalTime.h"
#include "KScreen.h"
#include "KRes.h" // KBank

namespace Kamilo {




class CSnapshot: public KManager, public KInspectorCallback {
	mutable KPath m_next_output_name;
	KPath m_texture_name;
	KPath m_capture_filename;
	KPath m_last_saved_filename;
	bool m_do_shot;
	KPath m_basename;
	int m_index;
	bool m_with_time;
	bool m_with_frame;
public:
	CSnapshot() {
		m_with_time  = false;
		m_with_frame = false;
		m_basename = "__snapshot";
		m_do_shot = false;
		m_index = 0;
		KEngine::addManager(this);
		KEngine::addInspectorCallback(this); // KInspectorCallback
	}
	virtual void on_manager_signal(KSig &sig) override {
		if (sig.check(KSignalType_WINDOW_KEY_DOWN)) {
			// ウィンドウイベントは別スレッドから飛んでくる場合があることに注意
			if (sig.getInt("key") == KKeyboard::KEY_SNAPSHOT) {
				m_do_shot = true;
			}
			return;
		}
	}
	virtual void on_manager_start() override {
		m_with_time  = true;
		m_with_frame = true;
		m_do_shot = false;
	}
	virtual void on_manager_appframe() override {
		// システムノードとして登録されている場合に呼ばれる。
		// デバッグ用の強瀬ポーズの影響を受けず（ゲームが停止していても）常に呼ばれる
		// KNode の KNode::FLAG_SYSTEM フラグが有効になっているノードでのみ呼ばれる
		if (m_do_shot) {
			capture("");
			captureNow(m_capture_filename.u8());
			m_do_shot = false;
		}
	}
	virtual void onInspectorGui() override { // KInspectorCallback
		if (m_with_time || m_with_frame) {
			// ファイル名にフレーム番号または時刻を含んでいる。
			// 時間によってファイル名が変化するため GUI を表示しているあいだは常に更新する
			m_next_output_name.clear(); // consumed this name
		}
		ImGui::Text("Next output file: ");
		ImGui::Text("%s", getNextName().u8());
		if (ImGui::Checkbox("Include local time", &m_with_time)) {
			m_next_output_name.clear(); // consumed this name
		}
		if (ImGui::Checkbox("Include frame number", &m_with_frame)) {
			m_next_output_name.clear(); // consumed this name
		}
		if (ImGui::Button("Snap shot!")) {
			m_do_shot = true;
		}
		if (KFiles::exists(m_last_saved_filename)) {
			char s[256];
			sprintf_s(s, sizeof(s), "Open: %s", m_last_saved_filename.u8());
			if (ImGui::Button(s)) {
				K__ShellOpenU8(m_last_saved_filename.u8());
			}
		}
	}
	void capture(const char *_filename) {
		KPath filename = _filename;
		if (filename.empty()) {
			m_capture_filename = getNextName();
			m_next_output_name.clear(); // consumed this name
		} else {
			m_capture_filename = filename;
		}
	}
	void captureNow(const char *_filename) {
		KPath filename = _filename;
		if (filename.empty()) {
			filename = getNextName();
			m_next_output_name.clear(); // consumed this name
		}

		if (!m_texture_name.empty()) {
			// キャプチャするテクスチャが指定されている
			KTEXID texid = KBank::getTextureBank()->getTexture(m_texture_name);
			KTexture *tex = KVideo::findTexture(texid);
			if (tex) {
				KImage img = tex->exportTextureImage();
				std::string png = img.saveToMemory();
				
				KOutputStream output = KOutputStream::fromFileName(filename.u8());
				output.write(png.data(), png.size());

				KLog::printInfo("Texture image saved %s", filename.getFullPath().u8());
				m_last_saved_filename = filename;
			}
		} else {
			// キャプチャ対象が未指定
			// 既定の画面を書き出す
			KScreen::postExportScreenTexture(filename); // 次回更新時に保存される。本当に保存されるか確定できないのでログはまだ出さない
			m_last_saved_filename = filename;
		}
	}
	void setCaptureTargetRenderTexture(const char *texname) {
		m_texture_name = texname;
	}
	const KPath & getNextName() const {
		if (m_next_output_name.empty()) {
			KPath name = m_basename;

			// 時刻情報を付加する
			if (m_with_time) {
				std::string mb = KLocalTime::now().format("(%y%m%d-%H%M%S)");
				KPath time = KPath::fromUtf8(mb.c_str());
				name = name.joinString(time);
			}

			// フレーム番号を付加する
			if (m_with_frame) {
				KPath frame = KPath::fromFormat("[04%d]", getFrameNumber());
				name = name.joinString(frame);
			}

			// ファイル名を作成
			m_next_output_name = name.joinString(".png");

			// 重複を確認する。重複していればインデックス番号を付加する
			int num = 1;
			while (KFiles::exists(m_next_output_name)) {
				m_next_output_name = KPath::fromFormat("%s_%d.png", name.u8(), num);
				num++;
			}
		}
		return m_next_output_name;
	}
	int getFrameNumber() const {
		return KEngine::getStatus(KEngine::ST_FRAME_COUNT_GAME);
	}
};

#pragma region KSnapshotter
static CSnapshot *g_Snapshot = nullptr;

void KSnapshotter::install() {
	g_Snapshot = new CSnapshot();
}
void KSnapshotter::uninstall() {
	if (g_Snapshot) {
		g_Snapshot->drop();
		g_Snapshot = nullptr;
	}
}
void KSnapshotter::capture(const char *filename) {
	K__Assert(g_Snapshot);
	g_Snapshot->capture(filename);
}
void KSnapshotter::captureNow(const char *filename) {
	K__Assert(g_Snapshot);
	g_Snapshot->captureNow(filename);
}
void KSnapshotter::setCaptureTargetRenderTexture(const char *texname) {
	K__Assert(g_Snapshot);
	g_Snapshot->setCaptureTargetRenderTexture(texname);
}
#pragma endregion // KSnapshotter


} // namespace
