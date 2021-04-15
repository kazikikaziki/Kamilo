#include "KRef.h"

// 参照カウンタの整合性を調べる
#ifndef K_REFCNT_DEBUG
#	define K_REFCNT_DEBUG 1
#endif

#include <unordered_set>
#include <mutex>
#include "KInternal.h"

namespace Kamilo {


class CRefChecker {
	std::unordered_set<KRef*> mLockedObjects;
public:
	CRefChecker() {
	}
	~CRefChecker() {
		check_leak();
	}
	void check_leak() {
		if (mLockedObjects.empty()) return;

		if (K__IsDebuggerPresent()) {
			for (auto it=mLockedObjects.begin(); it!=mLockedObjects.end(); ++it) {
				KRef *ref = *it;
				// ログ出力中に参照カウンタが操作されないように注意
				const char *s = ref->getReferenceDebugString();
				K__Print("%s: %s", typeid(*ref).name(), s ? s : "");
			}
		}
		K__Print("%d object(s) are still remain.", mLockedObjects.size());
		if (K__IsDebuggerPresent()) {
			// ここに到達してブレークした場合、参照カウンタがまだ残っているオブジェクトがある。
			// デバッガーで locked_objects_ の中身をチェックすること。
			mLockedObjects; // <-- 中身をチェック
			K__Assert(0);
		}
	}
	void add(KRef *ref) {
		K__Assert(ref);
		mLockedObjects.insert(ref);
	}
	void del(KRef *ref) {
		K__Assert(ref);
		mLockedObjects.erase(ref);
	}
};



static CRefChecker g_RefChecker;
static std::mutex g_RefMutex;


#pragma region KRef
KRef::KRef() {
	mRefCnt = 1;
	mDebubBreakRefCnt = -1;
	if (K_REFCNT_DEBUG) {
		// 参照カウンタの整合性をチェック
		g_RefMutex.lock();
		g_RefChecker.add(this);
		g_RefMutex.unlock();
	}
}
KRef::~KRef() {
	K__Assert(mRefCnt == 0); // ここで引っかかった場合、 drop しないで直接 delete してしまっている
	if (K_REFCNT_DEBUG) {
		// 参照カウンタの整合性をチェック
		g_RefMutex.lock();
		g_RefChecker.del(this);
		g_RefMutex.unlock();
	}
}
void KRef::grab() const {
	g_RefMutex.lock();
	mRefCnt++;
	g_RefMutex.unlock();
}
void KRef::drop() const {
	g_RefMutex.lock();
	mRefCnt--;
	g_RefMutex.unlock();
	K__Assert(mRefCnt >= 0);
	if (mRefCnt == mDebubBreakRefCnt) { // 参照カウンタが mDebubBreakRefCnt になったら中断する
		K__Break();
	}
	if (mRefCnt == 0) {
		delete this;
	}
}
int KRef::getReferenceCount() const {
	return mRefCnt;
}
const char * KRef::getReferenceDebugString() const {
	return nullptr;
}
void KRef::setReferenceDebugBreak(int cond_refcnt) {
	mDebubBreakRefCnt = cond_refcnt;
}
#pragma endregion // KRef


} // namespace
