#include <Kamilo.h>

#define TEST_LOG     1
#define TEST_STORAGE 1
#define TEST_GUI     1
#define TEST_MANAGER 1

using namespace Kamilo;

class CSampleMgr: public KManager {
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
		#if TEST_GUI
		ImGui::Text("Hello World!");
		#endif
	}
	virtual void on_manager_signal(KSig &sig) override {
	//	if (sig.check(K_SIG_WINDOW_KEY_DOWN) {}
	//	if (sig.check(K_SIG_WINDOW_KEY_UP) {}
	//	if (sig.check(K_SIG_WINDOW_KEY_CHAR)) {}
	//	if (sig.check(K_SIG_WINDOW_DROPFILE)) {}
	}
};



void GameMain() {
	K::win32_MemoryLeakCheck(); // メモリリークの自動ダンプ
	K::win32_ImmDisableIME(); // IMEを無効化
	K::sysSetCurrentDir(K::sysGetCurrentExecDir()); // exe の場所をカレントディレクトリにする

	#if TEST_LOG
	KLogger::init();
	KLogger::get()->getEmitter()->setConsoleOutput(true);  // コンソールウィンドウを使う
	KLogger::get()->getEmitter()->setDebuggerOutput(true); // デバッガーに文字を流せるように
	#endif

	#if TEST_STORAGE
	KStorage *sto = Kamilo::createStorage();
	sto->addFolder("data"); // data フォルダからファイルをロードできるようにする
	#endif

	KEngineDef def;
	def.resolution_x = 800;
	def.resolution_y = 600;
	if (KEngine::create(&def)) {

		#if TEST_MANAGER
		CSampleMgr sampleMgr;
		sampleMgr.install();
		#endif
		
		KEngine::run();

		#if TEST_MANAGER
		sampleMgr.uninstall();
		#endif

		KEngine::destroy();
	}

	#if TEST_STORAGE
	sto->drop();
	#endif

	#if TEST_LOG
	KLogger::shutdown();
	#endif
}
