﻿#include "KAnimation.h"
#include "KSpriteDrawable.h"
#include "KDebug.h"
#include "KInternal.h"
#include "KSig.h"

namespace Kamilo {


class CPlayback {
public:
	static void clipStart(KNode *node, const KClipRes *clip) {
		KSpriteDrawable *co = KSpriteDrawable::of(node);
		if (co) {
			// 必要なレイヤー数を用意する
			// そうしないと、ページごとにレイヤー枚数が異なった場合に面倒なことになるほか、
			// 前回のクリップよりもレイヤー数が少なかった場合に、今回使われないレイヤーがそのまま残ってしまう
			int m = clip->getMaxLayerCount();
			co->setLayerCount(m);
		}
	}
	static void clipAnimate(KNode *node, const KClipRes *clip, float frame) {
		KSpriteDrawable *co = KSpriteDrawable::of(node);
		if (co == nullptr) {
			KLog::printWarning("SPRITE ANIMATION BUT NO SPRITE RENDERER!");
			return;
		}
		int page_index = clip->getPageByFrame(frame, nullptr);

		const KClipRes::SPRITE_KEY *key = clip->getKey(page_index);
		if (key == nullptr) return;

		co->setLayerCount(key->num_layers);
		for (int i=0; i<key->num_layers; i++) {
			co->setLayerSprite(i, key->layers[i].sprite);
			co->setLayerLabel(i, key->layers[i].label);

			KSpriteAuto sprite = co->getLayerSprite(i);
			if (sprite != nullptr && sprite->mDefaultBlend != KBlend_INVALID) {
				co->getLayerMaterialAnimatable(i)->blend = sprite->mDefaultBlend;
			}
			#if 0
			// "@blend=add @blend=screen
			if (!key->layer_commands[i].empty()) {
				if (key->layer_commands[i].compare("blend=screen") == 0) {
					co->getLayerMaterialAnimatable(i)->blend = KBlend_SCREEN;
				} else {
					co->getLayerMaterialAnimatable(i)->blend = KBlend_ALPHA;
				}
			}
			#endif
		}
	}

public:
	enum Flag {
		PLAYBACKFLAG__NONE = 0,
		PLAYBACKFLAG__CAN_PREDEF_SEEK = 1, // クリップに埋め込まれたジャンプ命令に従って、指定されたフレームとは異なるフレームに再設定してもよい
	};
	typedef int Flags;

	CPlayback() {
		mCB = nullptr;
		mClip = nullptr;
		mFlags = 0;
		mFrame = 0.0f;
		mIsPlaying = false;
		mSleepTime = 0;
		mTarget = nullptr;
		mPostNextClip = "";
		mPostNextPage = 0;
	}
	KClipRes *mClip;
	bool mIsPlaying; // 再生中かどうか
	float mFrame;
	int mSleepTime; // 指定された時間だけ再生を停止する。-1 で無期限停止
	KClipRes::Flags mFlags;
	KPlaybackCallback *mCB;
	KNode *mTarget;
	KPath mPostNextClip;
	int mPostNextPage;


	void clearClip() {
		if (mClip) {
			mClip->drop();
			mClip = nullptr;
		}
		mFlags = 0;
		mFrame = 0.0f;
		mIsPlaying = false;
		mSleepTime = 0;
		mPostNextClip = "";
		mPostNextPage = 0;
		// mCB は初期化しない
	}

	bool getFlag(KClipRes::Flag f) const {
		return (mFlags & f) != 0;
	}

	void setFlag(KClipRes::Flag f, bool value) {
		if (value) {
			mFlags |= f;
		} else {
			mFlags &= ~f;
		}
	}

	void callRepeat() {
		if (mCB) {
			KPlaybackSignalArgs args;
			args.node = mTarget;
			mCB->on_playback_repeat(&args);
		}
	}

	void callTick(int page) {
		if (mCB) {
			KPlaybackSignalArgs args;
			args.node = mTarget;
			args.page = page;
			mCB->on_playback_tick(&args);
		}
	}

	void callEnterClip(int page) {
		if (mCB) {
			KPlaybackSignalArgs args;
			args.node = mTarget;
			args.clip = mClip;
			args.page = page;
			mCB->on_playback_enter_page(&args);
		}
	}
	void callEnterPage(int new_page) {
		if (mCB) {
			KPlaybackSignalArgs args;
			args.node = mTarget;
			args.clip = mClip;
			args.page = new_page;
			mCB->on_playback_enter_page(&args);
		}
	}
	void callExitPage(int old_page) {
		if (mCB) {
			if (mClip) {
				KPlaybackSignalArgs args;
				args.node = mTarget;
				args.clip = mClip;
				args.page = old_page;
				mCB->on_playback_exit_page(&args);
			}
		}
	}
	void callExitClip() {
		if (mCB) {
			KPlaybackSignalArgs args;
			args.node = mTarget;
			mCB->on_playback_exit_clip(&args);
		}
	}

	// playback の内容を初期化する
	// keep_clip: clip を drop & 削除せずに残しておく（アニメが終わったことを通知するだけで、クリップを外さない）
	void playbackClear(bool keep_clip) {
		if (mClip) {

			// スプライトページの終了を通知
			callExitPage(mClip->getPageByFrame(mFrame, nullptr));

			// アニメクリップの解除を通知
			callExitClip();

			// アニメクリップを解除
			if (!keep_clip) {
				mClip->drop();
				mClip = nullptr;
			}
			mIsPlaying = false;
		}
	}
	// 現在の再生位置にあるコマンドをシグナル送信する
	// e: playback のコールバックに渡すパラメータ
	void playbackSignal() const {
		if (mClip && mTarget) {
			const KClipRes::SPRITE_KEY *key = mClip->getKeyByFrame(mFrame);
			if (key) {
				// コマンドを通知する
				const KNamedValues &nv = key->user_parameters;
				for (int i=0; i<nv.size(); i++) {
					KSig sig(K_SIG_ANIMATION_COMMAND);
					sig.setNode("target", mTarget);
					sig.setString("cmd", nv.getName(i));
					sig.setString("val", nv.getString(i));
					sig.setString("clipname", mClip ? mClip->getName() : "");
					sig.setPointer("clipptr", mClip);
					mTarget->getRoot()->broadcastSignalToChildren(sig);
				}
			}
		}
	}

	// e: playback のコールバックに渡すパラメータ
	// flags: シークオプション
	bool playbackSeek(float frame, Flags flags) {
		// スプライトアニメ
		if (playbackSeekSprite(frame, flags)) {
			return true;
		}

		// アニメを完全に終了する

		// 終了処理よりも前にフラグを取得しておく
		const bool kill = getFlag(KClipRes::FLAG_KILL_SELF);

		// 終了処理
		playbackClear(true); // アニメは終了するが、クリップは設定したままにしておく

		// エンティティの自動削除
		if (kill) {
			mTarget->markAsRemove();
		}
		return false;
	}

	bool playbackSeekSprite(float new_frame, Flags flags) {
		K_assert(mClip);

		// アニメ全体の長さ
		int total_length = mClip->getLength();

		// シーク前のページ
		int old_page = mClip->getPageByFrame(mFrame, nullptr);

		// シーク後のページ
		int new_page = -1;
		if (new_frame < total_length) {
			new_page = mClip->getPageByFrame(new_frame, nullptr);
		}

		// ページ切り替えの有無
		if (old_page == new_page) {
			// シーク前とシーク後でページが変化していない
			mIsPlaying = true;
			mFrame = new_frame;

			// アニメ対象の状態を更新
			if (mClip) {
				clipAnimate(mTarget, mClip, mFrame);
			}
			callTick(new_page);
			return true;
		}

		// アニメ終端に達していて、ループが指定されているなら先頭へシークする
		if (new_page < 0 && getFlag(KClipRes::FLAG_LOOP)) {
			new_page = 0;
		}

		// アニメクリップで定義されたページジャンプに従って、シーク先ページを再設定する
		if (flags & PLAYBACKFLAG__CAN_PREDEF_SEEK) {
			const KClipRes::SPRITE_KEY *key = mClip->getKey(old_page);
			KPath jump_clip;
			int jump_page = -1;
			if (mClip->getNextPage(old_page, key->next_mark, &jump_clip, &jump_page)) {

				if (jump_clip.empty()) {
					// 同じクリップ内でジャンプする
					new_page = jump_page; // アニメクリップにより行き先が変更された
				} else {
					// 異なるクリップにジャンプする。現在のクリップをいったん終了させる
					new_page = -1;
					mPostNextClip = jump_clip;
					mPostNextPage = jump_page;
				}
			}
		}

		// 新しいページに入る。
		// なお、結果として old_page == new_page になった場合（同ページ内ループ）でも
		// 必ずページ切り替えイベントを生させる
		if (new_page >= 0) {
			mIsPlaying = true;

			// 古いページから離れることを通知
			callExitPage(old_page);

			// 新しいフレーム位置を設定
			mFrame = (float)mClip->getKeyTime(new_page);

			// ページ切り替え時のシグナル送信
			if (old_page != new_page) {
				playbackSignal();
			}

			// アニメ対象の状態を更新
			if (mClip) {
				clipAnimate(mTarget, mClip, mFrame);
			}

			// 新しいページに入ったことを通知
			callEnterPage(new_page);
			return true;

		}

		// 新しいページなし。そのままアニメ終了する。

		// ここでは on_playback_exit_page を呼ばない。
		// アニメが終了した場合は、クリップを変更するまで最終ページがそのまま有効になり続ける。
		// 最後のページに対する on_playback_exit_page が呼ばれるのは、クリップが削除された時である
		// playbackClear を参照すること

		// 完全に終了
		return false;
	}

	bool playback_seek_non_sprite(float new_frame, Flags flags) {
		const int total_length = mClip->getLength(); // アニメ全体の長さ

		if (new_frame<total_length) {
			// まだ終端に達していない or 無限長さ
			mIsPlaying = true;
			mFrame = new_frame;
			// アニメ対象の状態を更新
			if (mClip) {
				clipAnimate(mTarget, mClip, mFrame);
			}
			return true;
		}
		// 再生位置がアニメ終端に達した。
		// ループまたは終了処理する
		if (getFlag(KClipRes::FLAG_LOOP)) {
			// 先頭に戻る
			mIsPlaying = true;
			mFrame = 0;
			// リピート通知
			callRepeat();
			// アニメ対象の状態を更新
			if (mClip) {
				clipAnimate(mTarget, mClip, mFrame);
			}
			return true;
		}
		// 完全に終了
		return false;
	}
};
// CPlayback



class CAnimationMgr: public KManager {
public:
	std::unordered_map<KNode*, KAnimation*> m_Nodes;
	mutable std::recursive_mutex m_Mutex;

	CAnimationMgr() {
		KEngine::addManager(this);
	//	KEngine::addInspectorCallback(this); // KInspectorCallback
	}
	~CAnimationMgr() {
	}

	void lock() const {
	#if K_THREAD_SAFE
		m_Mutex.lock();
	#endif
	}
	void unlock() const {
	#if K_THREAD_SAFE
		m_Mutex.unlock();
	#endif
	}
	void _addAni(KNode *node, KAnimation *ani) {
		auto it = m_Nodes.find(node);
		if (it == m_Nodes.end()) {
			m_Nodes[node] = ani;
			m_Nodes[node]->grab();
		}
	}
	void on_manager_end() {
		K_assert(m_Nodes.empty()); // 正しく on_manager_detach が呼ばれていれば、この時点でノード数はゼロのはず
	}
	bool on_manager_isattached(KNode *node) {
		return getAni(node) != nullptr;
	}
	void on_manager_detach(KNode *node) {
		lock();
		{
			auto it = m_Nodes.find(node);
			if (it != m_Nodes.end()) {
				KAnimation *ani = it->second;
				ani->_setNode(nullptr);
				ani->drop();
				m_Nodes.erase(it);
			}
		}
		unlock();
	}
	void on_manager_frame() {
		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			KNode *node = it->first;
			if (node->getEnableInTree() && !node->getPauseInTree()) {
				KAnimation *ani = it->second;
				ani->tickTracks();
			}
		}
	}
	void on_manager_nodeinspector(KNode *node) {
		getAni(node)->updateInspector();
	}
	KAnimation * getAni(KNode *node) {
		auto it = m_Nodes.find(node);
		if (it != m_Nodes.end()) {
			return it->second;
		}
		return nullptr;
	}
};

static CAnimationMgr *g_AnimationMgr = nullptr;


#pragma region KAnimation
void KAnimation::install() {
	K__ASSERT_RETURN(g_AnimationMgr == nullptr);
	g_AnimationMgr = new CAnimationMgr();
}
void KAnimation::uninstall() {
	if (g_AnimationMgr) {
		g_AnimationMgr->drop();
		g_AnimationMgr = nullptr;
	}
}
void KAnimation::attach(KNode *node) {
	if (node && !isAttached(node)) {
		KAnimation *ani = new KAnimation();
		ani->_setNode(node);
		g_AnimationMgr->_addAni(node, ani);
		ani->drop();
	}
}
bool KAnimation::isAttached(KNode *node) {
	return of(node) != nullptr;
}
KAnimation * KAnimation::of(KNode *node) {
	return g_AnimationMgr->getAni(node);
}
KAnimation::KAnimation() {
	K__ASSERT_RETURN(KBank::getAnimationBank());
	m_Node = nullptr;
	m_ClipSpeed = 1.0f;
	m_AutoResetClipSpeed = false;
	m_MainPlayback = new CPlayback();
}
KAnimation::~KAnimation() {
	clearMainClip();
	delete m_MainPlayback;
}
void KAnimation::_setNode(KNode *node) {
	m_Node = node;
	m_MainPlayback->mTarget = node;
}
KNode * KAnimation::getNode() {
	return m_Node;
}
void KAnimation::clearMainClip() {
	m_MainPlayback->playbackClear(false);
}
const KClipRes * KAnimation::getMainClip() const {
	const KPath &name = getMainClipName();
	return KBank::getAnimationBank()->getClipResource(name.u8());
}
KPath KAnimation::getMainClipName() const {
	if (m_MainPlayback->mClip == nullptr) return KPath::Empty;
	return m_MainPlayback->mClip->getName();
}
/// アニメを指定フレームの間だけ停止する。
/// @param duration 停止フレーム数。負の値を指定すると無期限に停止する
void KAnimation::setMainClipSleep(int duration) {
	m_MainPlayback->mSleepTime = duration;
}
bool KAnimation::isMainClipSleep() const {
	return m_MainPlayback->mSleepTime != 0;
}
void KAnimation::seekMainClipBegin() {
	seekMainclip(0, -1, 0);
}
void KAnimation::seekMainClipEnd() {
	if (m_MainPlayback->mClip) {
		int len = m_MainPlayback->mClip->getLength();
		if (m_MainPlayback->playbackSeek((float)len, CPlayback::PLAYBACKFLAG__CAN_PREDEF_SEEK)) {
		//	this->node_update_commands();
		}
	}
}
bool KAnimation::seekMainClipFrame(int frame) {
	return seekMainclip((float)frame, -1, 0);
}
bool KAnimation::seekMainClipKey(int key) {
	return seekMainclip(-1, key, 0);
}
/// マーカーを指定して移動
/// ※ mark として有効なのは 1 以上の値のみ
bool KAnimation::seekMainClipToMark(int mark) {
	return seekMainclip(-1, -1, mark);
}
/// 指定したマーカーがついているページを返す
/// マーカーが存在しなければ -1 を返す
int KAnimation::findPageByMark(int mark) const {
	if (m_MainPlayback->mClip == nullptr) {
		return -1;
	}
	if (m_MainPlayback->mClip == nullptr) {
		return -1;
	}
	return m_MainPlayback->mClip->findPageByMark(mark);
}
/// アニメクリップを再生中の場合、そのページ番号を返す。
/// 失敗した場合は -1 を返す
/// out_pageframe ページ先頭からの経過フレーム数を返す
int KAnimation::getMainClipPage(int *out_pageframe) const {
	const KClipRes *clip = m_MainPlayback->mClip;
	if (clip == nullptr) return -1;

	float frame = m_MainPlayback->mFrame;
	if (frame < 0) return -1;

	int page = clip->getPageByFrame(frame, out_pageframe);
	return page;
}
bool KAnimation::isMainClipPlaying(const char *name_or_alias, KPath *post_next_clip, int *post_next_page) const {
	if (m_MainPlayback->mIsPlaying) {
		// メインアニメ再生中
		if (KStringUtils::isEmpty(name_or_alias)) {
			return true;
		}

		const KPath &playing_clip_name = m_MainPlayback->mClip->getName();

		// 名前が指定されている場合は、再生中のクリップ名も確認する
		K_assert(m_MainPlayback->mClip);
		auto it = m_AliasMap.find(name_or_alias);
		if (it != m_AliasMap.end()) {
			// name_or_alias はエイリアス名で指定されている
			const KPath &clipname = it->second;
			if (playing_clip_name.compare(clipname) == 0) {
				return true;
			}

		} else {
			// name_or_alias はクリップ名で指定されている
			const KPath &clipname = name_or_alias;
			if (playing_clip_name.compare(clipname) == 0) {
				return true;
			}
		}
	} else {
		if (post_next_clip) *post_next_clip = m_MainPlayback->mPostNextClip;
		if (post_next_page) *post_next_page = m_MainPlayback->mPostNextPage;
	}
	return false;
}
/// スプライトアニメをアタッチする
/// @param name アタッチするアニメクリップの名前
/// @param keep 既に同じアニメが設定されている場合の挙動を決める。
///             true なら既存のアニメがそのまま進行する。false ならリスタートする
bool KAnimation::setMainClipName(const char *name, bool keep) {
	KClipRes *clip = KBank::getAnimationBank()->find_clip(name);
	return setMainClip(clip, keep);
}
bool KAnimation::setMainClipAlias(const char *alias, bool keep) {
	// 空文字列が指定された場合はクリップを外す
	if (KStringUtils::isEmpty(alias)) {
		return setMainClip(nullptr);
	}
	// エイリアスから元のクリップ名を得る
	KPath name = m_AliasMap[alias];
	if (name.empty()) {
		KLog::printWarning("Animation alias named '%s' does not exist", alias);
		return setMainClip(nullptr);
	}
	// 新しいクリップを設定する
	KClipRes *clip = KBank::getAnimationBank()->find_clip(name.u8());
	return setMainClip(clip, keep);
}
bool KAnimation::setMainClip(KClipRes *clip, bool keep) {
	// 同じアニメが既に再生中の場合は何もしない
	if (keep && clip) {
		if (m_MainPlayback->mClip == clip) {
			return false;
		}
	}

	// 再生中のクリップを終了して後始末する
	{
		m_MainPlayback->playbackClear(false);

		// 再生中のクリップにだけアニメ速度変更が適用されているなら
		// 元の速度に戻しておく
		if (m_AutoResetClipSpeed) {
			m_ClipSpeed = 1.0f;
			m_AutoResetClipSpeed = false;
		}
		m_MainPlayback->clearClip();
	}

	if (clip) {
		// アニメ適用
		m_AutoResetClipSpeed = false;
		m_MainPlayback->clearClip();
		m_MainPlayback->mClip = clip;
		m_MainPlayback->mClip->grab();
		m_MainPlayback->mIsPlaying = true;

		// アニメで設定されている再生フラグをコピー
		m_MainPlayback->setFlag(KClipRes::FLAG_LOOP, clip->getFlag(KClipRes::FLAG_LOOP));
		m_MainPlayback->setFlag(KClipRes::FLAG_KILL_SELF, clip->getFlag(KClipRes::FLAG_KILL_SELF));

		// コマンド更新
	//	this->node_update_commands();

		if (m_MainPlayback->mCB) {
			KPlaybackSignalArgs args;
			args.node = getNode();
			m_MainPlayback->mCB->on_playback_enter_clip(&args);
		}

		if (m_MainPlayback->mClip) {
			// 開始イベントを呼ぶ
			CPlayback::clipStart(getNode(), m_MainPlayback->mClip);

			// アニメ更新関数を通しておく
			CPlayback::clipAnimate(getNode(), m_MainPlayback->mClip, m_MainPlayback->mFrame);
		}
		// 新しいクリップが設定されたことを通知
		if (1) {
			KSig sig(K_SIG_ANIMATION_NEWCLIP);
			sig.setNode("target", getNode());
			sig.setString("clipname", clip->getName());
			KNodeTree::broadcastSignal(sig);
		}

		// ページ開始
		if (m_MainPlayback->mClip) {
			int page = m_MainPlayback->mClip->getPageByFrame(m_MainPlayback->mFrame, nullptr);
			m_MainPlayback->callEnterClip(page);
		}

		// コマンド送信
		m_MainPlayback->playbackSignal();
	}
	return true;
}
void KAnimation::setMainClipCallback(KPlaybackCallback *cb) {
	m_MainPlayback->mCB = cb;
}
void KAnimation::tickTracks() {
	if (m_MainPlayback->mClip) {
		if (m_MainPlayback->mSleepTime > 0) {
			m_MainPlayback->mSleepTime--;

		} else if (m_MainPlayback->mSleepTime == 0) {

			float newframe = m_MainPlayback->mFrame + m_ClipSpeed;
			if (m_MainPlayback->playbackSeek(newframe, CPlayback::PLAYBACKFLAG__CAN_PREDEF_SEEK)) {
			//	this->node_update_commands();
			}
		}
	}
}
void KAnimation::setAlias(const KPath &alias, const KPath &name) {
	m_AliasMap[alias] = name;
}
const char * KAnimation::getClipNameByAlias(const KPath &alias) const {
	auto it = m_AliasMap.find(alias);
	if (it != m_AliasMap.end()) {
		return it->second.u8();
	}
	return nullptr;
}
/// アニメ速度の倍率を設定する。
/// speed; アニメ速度倍率。標準値は 1.0
/// current_clip_only: 現在再生中のクリップにだけ適用する。
///                    true にすると、異なるクリップが設定された時に自動的に速度設定が 1.0 に戻る。
///                    false を指定した場合、再び setSpeed を呼ぶまで速度倍率が変更されたままになる
void KAnimation::setSpeedScale(float speed, bool current_clip_only) {
	m_ClipSpeed = speed;
	m_AutoResetClipSpeed = current_clip_only;
}
float KAnimation::getSpeedScale() const {
	return m_ClipSpeed;
}
bool KAnimation::getCurrentParameterBool(const char *name) const {
	auto nv = getCurrentUserParameters();
	return nv ? nv->getBool(name) : false;
}
int KAnimation::getCurrentParameterInt(const char *name, int def) const {
	auto nv = getCurrentUserParameters();
	return nv ? nv->getInteger(name, def) : def;
}
float KAnimation::getCurrentParameterFloat(const char *name, float def) const {
	auto nv = getCurrentUserParameters();
	return nv ? nv->getFloat(name, def) : def;
}
const char * KAnimation::getCurrentParameter(const char *name) const {
	auto nv = getCurrentUserParameters();
	return nv ? nv->getString(name) : nullptr;
}
bool KAnimation::queryCurrentParameterInt(const char *name, int *value) const {
	auto nv = getCurrentUserParameters();
	return nv ? nv->queryInteger(name, value) : false;
}
void KAnimation::setCurrentParameter(const char *key, const char *value) {
	auto nv = getCurrentUserParametersEdit();
	if (nv) nv->setString(key, value);
}
const KXmlElement * KAnimation::getCurrentDataXml() const {
	const KClipRes *clip = m_MainPlayback->mClip;
	if (clip) {
		const KClipRes::SPRITE_KEY *key = clip->getKeyByFrame(m_MainPlayback->mFrame);
		if (key) {
			return key->xml_data;
		}
	}
	return nullptr;
}
KXmlElement * KAnimation::getCurrentDataXmlEdit() {
	KClipRes *clip = m_MainPlayback->mClip;
	if (clip) {
		KClipRes::SPRITE_KEY *key = clip->getKeyByFrame(m_MainPlayback->mFrame);
		if (key) {
			return key->xml_data;
		}
	}
	return nullptr;
}
const KNamedValues * KAnimation::getCurrentUserParameters() const {
	const KClipRes *clip = m_MainPlayback->mClip;
	if (clip) {
		const KClipRes::SPRITE_KEY *key = clip->getKeyByFrame(m_MainPlayback->mFrame);
		if (key) {
			return &key->user_parameters;
		}
	}
	return nullptr;
}
KNamedValues * KAnimation::getCurrentUserParametersEdit() {
	KClipRes *clip = m_MainPlayback->mClip;
	if (clip) {
		KClipRes::SPRITE_KEY *key = clip->getKeyByFrame(m_MainPlayback->mFrame);
		if (key) {
			return &key->user_parameters;
		}
	}
	return nullptr;
}
bool KAnimation::updateClipGui() {
#ifndef NO_IMGUI
	const KClipRes *clip = m_MainPlayback->mClip;
	if (clip == nullptr) {
		ImGui::Text("(NO CLIP)");
		return false;
	}
	bool changed = false;
	if (m_MainPlayback->mSleepTime == 0) {
		ImGui::Text("Frame %.1f/%d", m_MainPlayback->mFrame, clip->getLength());
	} else {
		KImGui::KImGui_PushTextColor(KImGui::KImGui_COLOR_WARNING);
		ImGui::Text("Frame %.1f/%d (Sleep)", m_MainPlayback->mFrame, clip->getLength());
		KImGui::KImGui_PopTextColor();
	}
	{
		const auto FLAG = KClipRes::FLAG_LOOP;
		bool a = m_MainPlayback->getFlag(FLAG);
		if (ImGui::Checkbox("Loop", &a)) {
			m_MainPlayback->setFlag(FLAG, a);
			changed = true;
		}
	}
	{
		const auto FLAG = KClipRes::FLAG_KILL_SELF;
		bool a = m_MainPlayback->getFlag(FLAG);
		if (ImGui::Checkbox("Remove entity at end", &a)) {
			m_MainPlayback->setFlag(FLAG, a);
			changed = true;
		}
	}
	{
		float spd = m_ClipSpeed;
		if (ImGui::DragFloat("Speed scale", &spd, 0.01f, 0.0f, 10.0f)) {
			m_ClipSpeed = spd;
		}
	}
	if (m_MainPlayback->mClip) {
		if (ImGui::TreeNodeEx("%s", ImGuiTreeNodeFlags_DefaultOpen, typeid(*m_MainPlayback->mClip).name())) {
			m_MainPlayback->mClip->on_track_gui_state(m_MainPlayback->mFrame);
			ImGui::TreePop();
		}
	}
	return changed;
#else // !NO_IMGUI
	return false;
#endif // NO_IMGUI
}
void KAnimation::updateInspector() {
	if (m_MainPlayback->mSleepTime == 0) {
		KImGui::KImGui_PushTextColor(KImGui::KImGui_COLOR_DEFAULT());
		ImGui::Text("Sleep: Off");
	} else if (m_MainPlayback->mSleepTime > 0) {
		KImGui::KImGui_PushTextColor(KImGui::KImGui_COLOR_WARNING);
		ImGui::Text("Sleep: On (%d)", m_MainPlayback->mSleepTime);
	} else {
		KImGui::KImGui_PushTextColor(KImGui::KImGui_COLOR_WARNING);
		ImGui::Text("Sleep: On (INF)");
	}
	KImGui::KImGui_PopTextColor();

	{
		KPath name = m_MainPlayback->mClip ? m_MainPlayback->mClip->getName() : KPath::Empty;
		if (ImGui::TreeNode("Clip: %s", name.u8())) {
			updateClipGui();
			ImGui::TreePop();
		}
	}
	if (ImGui::TreeNode("Parameters")) {
		auto nv = getCurrentUserParameters();
		if (nv) {
			for (int i=0; i<nv->size(); i++) {
				const char *k = nv->getName(i);
				const char *v = nv->getString(i);
				ImGui::Text("%s: %s", k, v);
			}
		}
		ImGui::TreePop();
	}
	ImGui::Separator();
	if (ImGui::TreeNode("Alias Map")) {
		for (auto it=m_AliasMap.cbegin(); it!=m_AliasMap.cend(); ++it) {
			ImGui::Text("%s: %s", it->first.u8(), it->second.u8());
		}
		ImGui::TreePop();
	}
}
bool KAnimation::seekMainclip(float frame, int key, int mark) {
	if (m_MainPlayback->mClip == nullptr) {
		return false;
	}

	// マークが指定されているならキーに変換する
	if (mark > 0) { // mark == 0 は「マーク無し」を意味するので、探さない
		key = m_MainPlayback->mClip->findPageByMark(mark);
		if (key < 0) return false; // マークが見つからない
	}

	// キーが指定されているならフレームに変換する
	if (key >= 0) {
		frame = (float)m_MainPlayback->mClip->getKeyTime(key);
	}

	if (frame >= 0) {
		// わざわざフレームを指定してシークしているので、
		// KClipPageCmd_NEXTA フラグなどで勝手に移動しないようにする
		if (m_MainPlayback->playbackSeek((float)frame, CPlayback::PLAYBACKFLAG__NONE)) {
		//	this->node_update_commands();
		}
	}
	return true;
}
#pragma endregion KAnimation


} // namespace
