#include <Kamilo.h>

void GameMain() {
	KLog::setOutputConsole(true);

	K__SetCurrentDirU8(K__GetExecDirU8()); // exe の場所をカレントディレクトリにする
	KStorage::addFolder("data");

	Data::openConfig();

	KEngineDef def;
	def.resolution_x = Data::getConfigInt("RESOLUTION_W");
	def.resolution_y = Data::getConfigInt("RESOLUTION_H");
	if (KEngine::create(&def)) {
		KWindowDragger::install();
		KWindowResizer::install();
		KAutoHideCursor::install();
		KInputMap::install();
		KEngine::run();
		KInputMap::uninstall();
		KAutoHideCursor::uninstall();
		KWindowResizer::uninstall();
		KWindowDragger::uninstall();
		KEngine::destroy();
	}
}
