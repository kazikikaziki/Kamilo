#include "KDebug.h"

#include "KInternal.h"
#include "KLog.h"



namespace Kamilo {


#pragma region Debug
void K_assertion_failure(const char *file, int line, const char *expr) {
	// KLog::emitf からコールバックがよばれ、そこでさらに assert に失敗した場合を考慮する
	static int s_locked = 0;
	if (s_locked == 0) {
		s_locked++;
		KLog::emitf(KLog::LEVEL_AST, "[ASSERTION_FAILURE]: %s(%d): %s", file, line, expr);
		s_locked--;
	} else {
		K__RawPrintf("[ASSERTION_FAILURE] (Recucsively): %s(%d): %s", file, line, expr);
		K__Break();
	}
}
#pragma endregion // Debug


void K_dialog(const char *u8) {
	K__Dialog(u8);
}
void K_notify(const char *u8) {
	K__Notify(u8);
}



} // namespace
