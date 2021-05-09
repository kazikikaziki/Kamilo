#include <Kamilo.h>

using namespace Kamilo;

void GameMain() {
	Test::Test_internal_path();
	

	K::sysSetCurrentDir(K::sysGetCurrentExecDir()); // exe の場所をカレントディレクトリにする
	KEngineDef def;
	def.resolution_x = 800;
	def.resolution_y = 600;
	if (KEngine::create(&def)) {
		KEngine::run();
		KEngine::destroy();
	}
}
