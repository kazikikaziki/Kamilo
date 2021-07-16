#include <Kamilo.h>

using namespace Kamilo;

class CMainMgr: public KManager {
public:
	void install() {
		KEngine::addManager(this);
		KEngine::setManagerGuiEnabled(this, true);
	}
	void uninstall() {
		// KEngine からマネージャを外すメソッドは存在しない
		// KEngine が終了すれば自動的にマネージャも外れる
	}
	virtual void on_manager_start() override {
	}
	virtual void on_manager_end() override {
	}
	virtual void on_manager_beginframe() override {
	}
	virtual void on_manager_endframe() override {
	}
	virtual void on_manager_gui() override {
	}
	virtual void on_manager_signal(KSig &sig) override {
	//	if (sig.check(K_SIG_WINDOW_KEY_DOWN) {}
	//	if (sig.check(K_SIG_WINDOW_KEY_UP) {}
	//	if (sig.check(K_SIG_WINDOW_KEY_CHAR)) {}
	//	if (sig.check(K_SIG_WINDOW_DROPFILE)) {}
	}
};



void GameMain() {
//	K::win32_MemoryLeakCheck(); // メモリリークの自動ダンプ
//	K::win32_ImmDisableIME(); // IMEを無効化
	
//	KLog::init();
//	KLog::setOutputConsole(true);  // コンソールウィンドウを使う
//	KLog::setOutputDebugger(true); // デバッガーに文字を流せるように

//	K::sysSetCurrentDir(K::sysGetCurrentExecDir()); // exe の場所をカレントディレクトリにする
//	KStorage::getGlobal().addFolder("data"); // data フォルダからファイルをロードできるようにする

	KEngineDef def;
	def.resolution_x = 800;
	def.resolution_y = 600;
	if (KEngine::create(&def)) {
//		CMainMgr mgr;
//		mgr.install();
		KEngine::run();
//		mgr.uninstall();
		KEngine::destroy();
	}
}
