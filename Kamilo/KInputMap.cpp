#include "KInputMap.h"
//
#include "KSig.h"
#include "KInternal.h"
#include "KJoystick.h"
#include "KKeyboard.h"
#include "KThread.h"
#include "KImGui.h"
#include "keng_game.h"

namespace Kamilo {

static int _sign(float f) {
	if (f < 0.0f) return -1;
	if (f > 0.0f) return  1;
	return 0;
}

#pragma region Button Decls

class KButtonItem: public IKeyElm {
public:
	virtual const char * get_name() const = 0;
	virtual KButtonFlags get_flags() const = 0;
	virtual void clear() = 0;
	virtual int add_key(IKeyElm *key) = 0;
	virtual void del_key(int index) = 0;
	virtual IKeyElm * get_key(int index) const = 0;
	virtual int get_key_count() const = 0;

	// 新しいフレームに入る。
	// 前回 newFrame() を呼んでから発生した入力は、
	// すべて同時に起こったものとみなす。
	// newFrame() の呼び出しの間に最低 1 度は poll を呼ぶこと。
	virtual void newFrame() = 0;

	// 入力状態を更新し、入力状態を得る。
	// newFrame を呼ぶまでは poll による入力状態が蓄積され続け、
	// その間のすべての入力は同時に行われたものとして扱う。
	// 例えば 30FPS のゲームの場合 newFrame() と poll() を 1/30 秒ごとに 1 回ずつ呼べば良いが、
	// 1/30秒の間にボタンを押して離してしまうと、ボタンが押されていないと判断される場合がある。
	// このような時、例えば poll() を 1/60秒ごとに呼べば入力精度が上がる (1 回の newFrame() に対して poll() を 2 回呼ぶ）
	virtual void poll(KPollFlags flags) = 0;

	// poll() 前後で入力が発生したかどうか
	virtual bool isJustPressedAtPoll(float *val) const = 0;

	// newFrame() 前後で入力が発生したかどうか
	virtual bool isJustPressedAtFrame(float *val, bool check_autorepeat) const = 0;
	
	// 現在のボタンの状態を得る
	virtual bool isRawPressed(float *val, KPollFlags flags) const = 0;
};


/// ゲーム用のボタン入力を統合するためのクラス。
///
/// 1つのボタンに対して複数のキーボードのキーやジョイスティックボタン等を割り当てることができる
/// @snippet K_button.cpp test
class KButton: public virtual KRef {
public:
	static KButton * create();

	/// 仮想ボタンを登録する
	virtual void addButton(const char *button, KButtonFlags flags=0) = 0;

	/// 仮想ボタンを削除
	virtual void removeButton(const char *button) = 0;

	/// ボタン数を返す
	virtual int getButtonCount() const = 0;

	/// ボタンを得る
	virtual KButtonItem * getButtonItem(int index) const = 0;
	virtual KButtonItem * findButtonItem(const char *button) const = 0;

	/// アクションボタンが押されているかどうか
	/// poll() した段階で入力が成立しているなら true を返す
	virtual bool isButtonDownByPoll(const char *button, float *out_value) const = 0;

	/// アクションボタンが押されているかどうか
	/// ボタンが押されている状態ならば、ボタンの入力値を value にセットして true を返す
	/// newFrame によって確定された入力だけを扱う。入力途中 (poll) の状態を調べたい場合は isButtonDownByPoll を使う
	virtual bool isButtonDown(const char *button, float *value) const = 0;

	/// アクションボタンを押した瞬間かどうか
	/// ボタンが押した瞬間（直後）ならば、ボタンの入力値を value にセットして true を返す
	/// 押された瞬間かどうかは、直前に newFrame() を呼び出した時点でのボタンの状態で決まる。
	virtual bool getButtonDown(const char *button, float *value) const = 0;

	virtual int bindKey(const char *button, IKeyElm *key) = 0;
	virtual void unbindKey(const char *button, int index) = 0;
	virtual IKeyElm * getKeyBinding(const char *button, int index) = 0;
	virtual int getKeyCount(const char *button) = 0;

	/// キーバインドが衝突しているか。
	/// ２個の仮想ボタンに共通のキーバインドがあるか調べ、
	/// あるキーを押したときに button1 と button2 の両方が ON になるなら true を返す
	virtual bool isConflict(const char *button1, const char *button2) const = 0;

	/// 仮想ボタンにキーボードのキーを割り当てる
	/// @param btn_id アクション識別子
	/// @param key 割り当てるキー (@KKeyboard::KEY_A など)
	virtual IKeyElm * createKeyboardKey(KKeyboard::Key key, KKeyboard::Modifiers mod=KKeyboard::MODIF_NONE) = 0;

	/// 仮想ボタンにジョイスティックのキーを割り当てる
	virtual IKeyElm * createJoystickKey(KJoystick::Button joybtn) = 0;

	/// 仮想ボタンにマウスのキーを割り当てる
	virtual IKeyElm * createMouseKey(KMouse::Button btn) = 0;

	/// 仮想ボタンにジョイスティックの軸を割り当てる
	/// @param axis 割り当てる軸 (@K_JOYAXIS_X など)
	virtual IKeyElm * createJoystickAxis(KJoystick::Axis axis, int halfrange) = 0;

	/// 仮想ボタンにジョイスティックのハットボタン(POV)を割り当てる
	/// @param xsign ハットボタンの x 符号 (-1, 0, 1)
	/// @param ysign ハットボタンの y 符号 (-1, 0, 1)
	virtual IKeyElm * createJoystickPov(int xsign, int ysign) = 0;

	/// 仮想ボタンにコマンド入力を割り当てる。
	///
	/// @param keys コマンド配列。仮想ボタン名を並べた配列を指定する。
	/// 終端記号として配列の末尾に 0 を設定しておくこと。
	virtual IKeyElm * createCommand(const char *buttons[]) = 0;

	virtual void setPollFlags(KPollFlags flags) = 0;

	/// 新しい入力フレームを開始する
	///
	/// この呼び出しを境にして「ボタンが押された瞬間」を判定する
	virtual void newFrame() = 0;

	/// 入力状態を更新する
	/// newFrame よりも多い回数呼ぶと入力判定の精度が上がる
	virtual void poll() = 0;
};
#pragma endregion // Button Decls


#pragma region Buttons
namespace kns_btn {
static const int MAX_SEQUENCE_SIZE = 12;
static const int MAX_HISTORY_SIZE = 16;
static const int TIMEOUT_MSEC = 500; // シーケンスキーのキー要素同士の最大受け入れ時間

struct SKeyHistoryItem {
	std::vector<const IKeyElm *> pressed_buttons;
	int timestamp_msec;

	SKeyHistoryItem() {
		timestamp_msec = 0;
	}

	bool contains(const IKeyElm *elm) const {
		for (size_t i=0; i<pressed_buttons.size(); i++) {
			if (pressed_buttons[i] == elm) {
				return true;
			}
		}
		return false;
	}
};

class IKeyHistory {
public:
	virtual const SKeyHistoryItem * get_history_item(int index) const = 0;
	virtual int get_history_size() const = 0;
};


// keyboard key
class CKbKeyElm: public IKeyboardKeyElm {
	// このボタンに割り当てられたキーボードキー。
	// 割り当てなしの場合は KKeyboard::KEY_NONE
	KKeyboard::Key m_key;
	KKeyboard::Modifiers m_modifiers;

public:
	CKbKeyElm() {
		m_key = KKeyboard::KEY_NONE;
		m_modifiers = KKeyboard::MODIF_NONE;
	}
	CKbKeyElm(KKeyboard::Key key, KKeyboard::Modifiers mod) {
		K__Assert(key != KKeyboard::KEY_NONE);
		m_key = key;
		m_modifiers = mod;
	}

	virtual KKeyboard::Key get_key() const override {
		return m_key;
	}
	virtual void set_key(KKeyboard::Key key) override {
		m_key = key;
	}
	virtual KKeyboard::Modifiers get_modifiers() const override {
		return m_modifiers;
	}

	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CKbKeyElm *obj = dynamic_cast<const CKbKeyElm *>(k);
		return obj && obj->m_key == m_key;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (flags & POLLFLAG_NO_KEYBOARD) return false;
		if (m_key == KKeyboard::KEY_NONE) return false;
		if (KKeyboard::isKeyDown(m_key)) {
			if (KKeyboard::matchModifiers(m_modifiers)) {
				if (val) *val = 1;
				return true;
			}
		}
		return false;
	}
};

// joystick key
class CJoyKeyElm: public IJoystickKeyElm {
	// このボタンに割り当てられたジョイスティックボタン。
	// 割り当てなしの場合は KJoyKey_NONE
	KJoystick::Button m_joybtn;

public:
	CJoyKeyElm() {
		m_joybtn = KJoystick::BUTTON_NONE;
	}
	CJoyKeyElm(KJoystick::Button joybtn) {
		m_joybtn = joybtn;
	}
	virtual KJoystick::Button get_button() const override {
		return m_joybtn;
	}
	virtual void set_button(KJoystick::Button btn) override {
		m_joybtn = btn;
	}

	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CJoyKeyElm *obj= dynamic_cast<const CJoyKeyElm *>(k);
		return obj && obj->m_joybtn == m_joybtn;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (flags & POLLFLAG_NO_JOYSTICK) return false;
		if (m_joybtn < 0) return false;
		for (int i=0; i<KJoystick::MAX_CONNECT; i++) {
			if (KJoystick::isConnected(i)) {
				if (KJoystick::getButton(i, m_joybtn)) {
					if (val) *val = 1;
					return true;
				}
			}
		}
		return false;
	}
};

// mouse key
class CMouseKeyElm: public IKeyElm {
	// このボタンに割り当てられたマウスボタン。
	// 割り当てなしの場合は KMouseKey_NONE
	KMouse::Button m_btn;

public:
	CMouseKeyElm() {
		m_btn = KMouse::NONE;
	}
	CMouseKeyElm(KMouse::Button btn) {
		m_btn = btn;
	}

	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CMouseKeyElm *obj= dynamic_cast<const CMouseKeyElm *>(k);
		return obj && obj->m_btn == m_btn;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (flags & POLLFLAG_NO_MOUSE) return false;
		if (m_btn < 0) return false;
		if (KMouse::isButtonDown(m_btn)) {
			if (val) *val = 1;
			return true;
		}
		return false;
	}
};

// joystick axis
class CJoyAxisKeyElm: public IKeyElm {
	// このボタンに割り当てられたジョイスティック軸。
	// 割り当てなしの場合は KJoyAxis_NONE
	KJoystick::Axis m_axis;
	int m_halfrange;
public:
	CJoyAxisKeyElm() {
		m_axis = KJoystick::AXIS_NONE;
		m_halfrange = 0;
	}
	CJoyAxisKeyElm(KJoystick::Axis axis, int halfrange) {
		m_axis = axis;
		m_halfrange = halfrange;
	}
	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CJoyAxisKeyElm *obj= dynamic_cast<const CJoyAxisKeyElm *>(k);
		return obj && obj->m_axis == m_axis;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (flags & POLLFLAG_NO_JOYSTICK) return false;
		if (m_axis == KJoystick::AXIS_NONE) return false;
		float axisval = 0.0f;
		for (int i=0; i<KJoystick::MAX_CONNECT; i++) {
			if (KJoystick::isConnected(i)) {
				float val = KJoystick::getAxis(i, m_axis);
				if (fabsf(axisval) < fabsf(val)) {
					axisval = val;
				}
			}
		}
		if (m_halfrange < 0) {
			// in[-1..0] ==> out[0..1]
			if (axisval < 0) {
				if (val) *val = -axisval;
				return true;
			}
		}
		if (m_halfrange > 0) {
			// in[0..1] ==> out[0..1]
			if (axisval > 0) {
				if (val) *val = axisval;
				return true;
			}
		}
		return false;
	}
};

// joystick pov
class CJoyPovKeyElm: public IKeyElm {
	int m_povx;
	int m_povy;
public:
	CJoyPovKeyElm() {
		m_povx = 0;
		m_povy = 0;
	}
	CJoyPovKeyElm(int povX, int povY) {
		m_povx = povX;
		m_povy = povY;
	}
	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CJoyPovKeyElm *obj = dynamic_cast<const CJoyPovKeyElm *>(k);
		return obj && obj->m_povx==m_povx && obj->m_povy==m_povy;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (flags & POLLFLAG_NO_JOYSTICK) return false;
		if (m_povx == 0 && m_povy == 0) return false;
		for (int i=0; i<KJoystick::MAX_CONNECT; i++) {
			if (KJoystick::isConnected(i)) {
				int x=0, y=0;
				if (KJoystick::getPov(i, &x, &y, nullptr)) {
				#if 1
					// 設定値と入力値の符号の一致を確認する.
					// ただし設定値が 0 の場合は常に一致するとみなす
					bool xmatch = m_povx ? (x*m_povx > 0) : true; 
					bool ymatch = m_povy ? (y*m_povy > 0) : true;
				#else
					// 設定値と入力値の符号の一致を確認する.
					// 設定値が 0 の場合、入力値も 0 でないといけない
					bool xmatch = KMath::signf(x)==KMath::signf(m_povx);
					bool ymatch = KMath::signf(y)==KMath::signf(m_povy);
				#endif
					if (xmatch && ymatch) {
						if (val) *val = 1;
						return true;
					}
				}
			}
		}
		return false;
	}
};

// command button sequence
class CSquenceKeyElm: public IKeyElm {
	std::vector<const IKeyElm *> m_keys;
	const IKeyHistory *m_history; // 入力履歴
	mutable bool m_ok;
public:
	explicit CSquenceKeyElm(const IKeyHistory *hist) {
		m_history = hist;
		m_ok = false;
	}
	bool empty() const {
		return m_keys.empty();
	}
	void clear() {
		m_keys.clear();
		m_ok = false;
	}
	void add_seq(const IKeyElm *btn) {
		K__Assert(btn);
		m_keys.push_back(btn);
		m_ok = false;
	}

	// IKeyElm
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		int histsize = m_history->get_history_size();
		int seqsize = (int)m_keys.size();

		// 既に成立している場合、最後に押したキーが押されている間は
		// 成立したままであるとみなす。たとえば↓→というコマンドの場合、
		// ↓→と押して、→を押しっぱなしにしている限りはコマンド入力が続いているものとする
		if (m_ok) {
			// キーコマンドの最終入力
			const IKeyElm *last = m_keys[seqsize - 1];

			// そのキーをまだ押し続けているならOK
			if (last->isPressed(nullptr, flags)) {
				// 入力が継続している
				if (val) *val = 1;
				return true;
			}

			// ボタンを離した
			m_ok = false;
			return false;
		}

		// キーシーケンスが、キー入力履歴の末尾の並びと一致するか調べる
		if (histsize < seqsize) {
			m_ok = false;
			return false;
		}

		for (int i=0; i<seqsize; i++) {

			// 期待している入力
			const IKeyElm *refkey = m_keys[i];

			// 履歴のボタン
			const SKeyHistoryItem *histitem = m_history->get_history_item(histsize - seqsize + i);

			// 期待している入力が履歴に含まれていればOK
			if (!histitem->contains(refkey)) {
				m_ok = false;
				return false; // ボタンが違う
			}

			// 時刻チェック
			// 直前のキー入力から一定時間経過している場合は連続した入力として受け付けない
			if (i > 0) {
				const SKeyHistoryItem *hist_prev = m_history->get_history_item(histsize - seqsize + i - 1);
				int time0 = hist_prev->timestamp_msec;
				int time1 = histitem->timestamp_msec;
				if (time0 + TIMEOUT_MSEC <= time1) {
					m_ok = false;
					return false; // timeout
				}
			}
		}

		// 入力が成立している
		m_ok = true;
		if (val) *val = 1;
		return true;
	}
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CSquenceKeyElm *obj= dynamic_cast<const CSquenceKeyElm *>(k);
		if (obj) {
			// コマンド（自分）をコマンド（他）と比べる

			// 長さ比較
			size_t sz1 = m_keys.size();
			size_t sz2 = obj->m_keys.size();
			if (sz1 != sz2) {
				return false;
			}

			// 要素比較
			for (size_t i=0; i<m_keys.size(); i++) {
				const IKeyElm *elm1 = m_keys[i];
				const IKeyElm *elm2 = obj->m_keys[i];
				if (elm1 != elm2) { // ポインタを比較するだけで良く、ボタンの設定内容まで見る必要はない
					return false;
				}
			}

			return true;

		} else {
			// コマンド（自分）と非コマンド（相手）を比べる
			// 相手キーが自分のキーシーケンスに含まれているなら
			// 衝突しているとする
			for (size_t i=0; i<m_keys.size(); i++) {
				if (m_keys[i]->isConflictWith(k)) {
					return true;
				}
			}
			return false;
		}
	}
};

const int AUTOREPEAT_DELAY_MSEC = 300;
const int AUTOREPEAT_INTERVAL_MSEC = 100;

class CActionButtonKeyElm: public KButtonItem {
public:
	std::string m_name; // このアクションの名前
	std::vector<IKeyElm *> m_keys; // このボタンにバインドするキー
	float m_tmp;      // 受付中の入力値。未確定値。次のフレームでの確定値になる
	float m_curr;     // 確定した入力値。現在のフレームでの入力として扱う値
	float m_last;     // 直前のフレームでの入力値
	float m_raw_curr; // 最新のポーリングで得られた入力値
	float m_raw_last; // 直前のポーリングで得られた入力値
	KButtonFlags m_flags;
	int m_timestamp_press; // ボタン状態が OFF から ON に変化したときの時刻 
	int m_timestamp_nextrepeat; // 次のオートリピート予定時刻
	bool m_repeat; // キーリピートによる入力が発生した

	CActionButtonKeyElm(const char *name, KButtonFlags flags) {
		m_name = name;
		m_flags = flags;
		m_tmp = 0.0f;
		m_curr = 0.0f;
		m_last = 0.0f;
		m_timestamp_press = 0;
		m_timestamp_nextrepeat = 0;
		m_repeat = false;
	}
	virtual ~CActionButtonKeyElm() {
		clear();
	}
	virtual void clear() override {
		m_tmp = 0.0f;
		m_curr = 0.0f;
		m_last = 0.0f;
		m_raw_curr = 0.0f;
		m_raw_last = 0.0f;
		m_timestamp_press = 0;
		m_timestamp_nextrepeat = 0;
		m_repeat = false;
		for (auto it=m_keys.begin(); it!=m_keys.end(); ++it) {
			IKeyElm *key = *it;
			key->drop();
		}
		m_keys.clear();
	}
	virtual int add_key(IKeyElm *key) override {
		if (key == nullptr) return -1;
		key->grab();
		m_keys.push_back(key);
		return (int)m_keys.size() - 1;
	}
	virtual void del_key(int index) override {
		if (0 <= index && index < (int)m_keys.size()) {
			m_keys[index]->drop();
			m_keys.erase(m_keys.begin() + index);
		}
	}
	virtual IKeyElm * get_key(int index) const override {
		if (0 <= index && index < (int)m_keys.size()) {
			return m_keys[index];
		}
		return nullptr;
	}
	virtual int get_key_count() const override {
		return (int)m_keys.size();
	}
	bool has_cmd() const {
		for (auto it=m_keys.begin(); it!=m_keys.end(); ++it) {
			if (dynamic_cast<CSquenceKeyElm*>(*it) != nullptr) {
				return true;
			}
		}
		return false;
	}
	virtual const char * get_name() const override {
		return m_name.c_str();
	}
	virtual KButtonFlags get_flags() const override {
		return m_flags;
	}

	// 新しいフレームに入る。
	// 前回 newFrame() を呼んでから発生した入力は、
	// すべて同時に起こったものとみなす。
	// newFrame() の呼び出しの間に最低 1 度は poll を呼ぶこと。
	virtual void newFrame() override {
		m_last = m_curr;
		m_curr = m_tmp;
		m_tmp = 0.0f;
		m_repeat = false;
	}

	// 入力状態を更新し、入力状態を得る。
	// newFrame を呼ぶまでは poll による入力状態が蓄積され続け、
	// その間のすべての入力は同時に行われたものとして扱う。
	// 例えば 30FPS のゲームの場合 newFrame() と poll() を 1/30 秒ごとに 1 回ずつ呼べば良いが、
	// 1/30秒の間にボタンを押して離してしまうと、ボタンが押されていないと判断される場合がある。
	// このような時、例えば poll() を 1/60秒ごとに呼べば入力精度が上がる (1 回の newFrame() に対して poll() を 2 回呼ぶ）
	virtual void poll(KPollFlags flags) override {
		m_raw_last = m_raw_curr;
		m_raw_curr = 0.0f;

		// シーケンス入力の状態を更新する
		// 現在のボタンの状態を得る
		float raw = 0.0f;
		if (isRawPressed(&raw, flags)) {
			m_raw_curr = raw;

			if (m_tmp == 0.0f) {
				// 最初の入力。無条件で値をセットする
				m_tmp = raw;

			} else if (m_tmp * raw > 0) {
				// 同符号での入力。絶対値が大きければ書き換える
				if (fabsf(m_tmp) < fabsf(raw)) {
					m_tmp = raw;
				}

			} else {
				// 異符号での入力。
				// 後から入力された方を優先する
				m_tmp = raw;
			}
		}
	}

	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *other) const override {
		K__Assert(0); // 未使用。このメソッドは呼ばれない
		return false;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (m_curr != 0.0f) {
			if (val) *val = m_curr;
			return true;
		}
		return false;
	}

	// poll() 前後で入力が発生したかどうか
	virtual bool isJustPressedAtPoll(float *val) const override{
		// ゼロから非ゼロに変化したか、符号が変わっていたら新たな入力が発生したものとする
		int s0 = _sign(m_raw_last);
		int s1 = _sign(m_raw_curr);
		if ((s0 == 0 && s1 != 0) || (s0 * s1 < 0)) {
			if (val) *val = m_raw_curr;
			return true;
		}
		return false;
	}

	// newFrame() 前後で入力が発生したかどうか
	virtual bool isJustPressedAtFrame(float *val, bool check_autorepeat) const override {
		// ゼロから非ゼロに変化したか、符号が変わっていたら新たな入力が発生したものとする
		int s0 = _sign(m_last);
		int s1 = _sign(m_curr);
		if ((s0 == 0 && s1 != 0) || (s0 * s1 < 0)) {
			if (val) *val = m_curr;
			return true; // OFF から ON へ変化している
		}
		if (check_autorepeat && s1 && m_repeat) {
			return true; // ボタンは押しっぱなしの状態だがオートリピート機能により入力が発生した
		}
		return false;
	}

	// 現在のボタンの状態を得る
	virtual bool isRawPressed(float *val, KPollFlags flags) const override {
		for (auto it=m_keys.begin(); it!=m_keys.end(); ++it) {
			if ((*it)->isPressed(val, flags)) {
				return true;
			}
		}
		return false;
	}
};

class CButtonMgrImpl: public KButton, public IKeyHistory {
	mutable std::mutex m_mutex;
	std::vector<CActionButtonKeyElm*> m_buttons;

	// フレーム単位での履歴。
	// newFrame するたびに更新される
	// フレーム更新の間に押されたボタンがマージされる
	std::vector<SKeyHistoryItem> m_history_per_frame;

	// ポーリング単位での入力履歴。
	// poll するたびに更新される
	std::vector<SKeyHistoryItem> m_history_per_poll;

	KPollFlags m_poll_flags;

public:
	CButtonMgrImpl() {
		m_history_per_frame.clear();
		m_history_per_poll.clear();
		m_buttons.clear();
		m_poll_flags = 0;
	}
	virtual ~CButtonMgrImpl() {
		for (size_t i=0; i<m_buttons.size(); i++) {
			m_buttons[i]->drop();
		}
		m_buttons.clear();
	}

	#pragma region IKeyHistory inherit
	virtual const SKeyHistoryItem * get_history_item(int index) const override {
		return &m_history_per_poll[index];
	}
	virtual int get_history_size() const override {
		return (int)m_history_per_poll.size();
	}
	#pragma endregion // IKeyHistory inherit

	#pragma region KButton inherit
	virtual void setPollFlags(KPollFlags flags) override {
		m_poll_flags = flags;
	}
	virtual void addButton(const char *button, KButtonFlags flags) override {
		m_mutex.lock();
		if (get_button(button) == nullptr) {
			m_buttons.push_back(new CActionButtonKeyElm(button, flags));
		} else {
			get_button(button)->clear(); // リセット
		}
		m_mutex.unlock();
	}
	virtual void removeButton(const char *button) override {
		m_mutex.lock();
		for (size_t i=0; i<m_buttons.size(); i++) {
			if (m_buttons[i]->m_name.compare(button) == 0) {
				m_buttons[i]->drop();
				m_buttons.erase(m_buttons.begin() + i);
				break;
			}
		}
		m_mutex.unlock();
	}
	virtual int getButtonCount() const override {
		return (int)m_buttons.size();
	}
	virtual KButtonItem * findButtonItem(const char *button) const override {
		for (size_t i=0; i<m_buttons.size(); i++) {
			if (m_buttons[i]->m_name.compare(button) == 0) {
				return m_buttons[i];
			}
		}
		return nullptr;
	}
	virtual KButtonItem * getButtonItem(int index) const override {
		if (0 <= index && index < (int)m_buttons.size()) {
			return m_buttons[index];
		}
		return nullptr;
	}
	virtual bool isButtonDownByPoll(const char *button, float *out_value) const override {
		// ボタンが押されているかどうか。
		// 入力を受付済みならば true を返す。これは次回 newFrame 呼び出し時に正式な入力として確定されるもの。
		// 押されているなら value に入力値をセットして true を返す
		bool retval = false;
		m_mutex.lock();
		{
			const CActionButtonKeyElm *btn = get_button(button);
			if (btn && btn->isRawPressed(out_value, m_poll_flags)) {
				retval = true;
			}
		}
		m_mutex.unlock();
		return retval;
	}
	virtual bool isButtonDown(const char *button, float *out_value) const override {
		// ボタンが押されているかどうか。
		// 押されているなら value に入力値をセットして true を返す
		bool retval = false;
		m_mutex.lock();
		{
			const CActionButtonKeyElm *btn = get_button(button);
			if (btn && btn->isPressed(out_value, m_poll_flags)) {
				retval = true;
			}
		}
		m_mutex.unlock();
		return retval;
	}
	virtual bool getButtonDown(const char *button, float *out_value) const override {
		// ボタンが今押されたばかりかどうか。
		// 押された瞬間ならば value に入力値をセットして true を返す
		bool retval = false;
		m_mutex.lock();
		{
			const CActionButtonKeyElm *btn = get_button(button);
			if (btn) {
				if (btn->isJustPressedAtFrame(out_value, true)) {
					retval = true;
				}
			}
		}
		m_mutex.unlock();
		return retval;
	}
	virtual int bindKey(const char *button, IKeyElm *key) override {
		if (key == nullptr) {
			K__Error("invalid key");
			return -1;
		}
		CActionButtonKeyElm *btn = get_button(button);
		if (btn == nullptr) {
			K__Error(u8"E_BIND_KEY: 未登録のボタン(%s)にキーを割り当てようとしました", button);
			return -1;
		}
		m_mutex.lock();
		btn->add_key(key);
		m_mutex.unlock();
		return (int)btn->m_keys.size() - 1;
	}
	virtual void unbindKey(const char *button, int index) override {
		CActionButtonKeyElm *btn = get_button(button);
		if (btn == nullptr) {
			btn->del_key(index);
		}
	}
	virtual int getKeyCount(const char *button) override {
		CActionButtonKeyElm *btn = get_button(button);
		return btn ? (int)btn->m_keys.size() : 0;
	}
	virtual IKeyElm * getKeyBinding(const char *button, int index) override {
		CActionButtonKeyElm *btn = get_button(button);
		if (btn && 0<=index && index<(int)btn->m_keys.size()) {
			return btn->m_keys[index];
		}
		return nullptr;
	}
	virtual IKeyElm * createKeyboardKey(KKeyboard::Key key, KKeyboard::Modifiers mod) override {
		if (key == KKeyboard::KEY_NONE) {
			K__Error("invalid key");
			return nullptr;
		}
		return new CKbKeyElm(key, mod);
	}
	virtual IKeyElm * createJoystickKey(KJoystick::Button joybtn) override {
		if (!KJoystick::isInit()) {
			K__Error("no joystick support");
			return nullptr;
		}
		return new CJoyKeyElm(joybtn);
	}
	virtual IKeyElm * createJoystickAxis(KJoystick::Axis axis, int halfrange) override {
		if (!KJoystick::isInit()) {
			K__Error("no joystick support");
			return nullptr;
		}
		return new CJoyAxisKeyElm(axis, halfrange);
	}
	virtual IKeyElm * createJoystickPov(int xsign, int ysign) override {
		if (!KJoystick::isInit()) {
			K__Error("no joystick support");
			return nullptr;
		}
		return new CJoyPovKeyElm(xsign, ysign);
	}
	virtual IKeyElm * createMouseKey(KMouse::Button btn) override {
		return new CMouseKeyElm(btn);
	}
	virtual IKeyElm * createCommand(const char *buttons[]) override {
		// コマンド入力を割り当てる
		// keys にはボタン番号とその符号を並べた配列を指定し、最後に 0 を置く。
		if (buttons == nullptr || buttons[0] == 0) {
			K__Error("empty sequence");
			return nullptr;
		}
		// 事前チェック
		for (int i=0; buttons[i]; i++) {
			const char *name = buttons[i];
			const CActionButtonKeyElm *act_elm = get_button(name);
			if (act_elm == nullptr) {
				K__Error(u8"キーコマンドに含まれているボタン(%d)は未登録です", name);
				return nullptr;
			}
			if (act_elm->has_cmd()) {
				K__Error(u8"キーコマンドに含まれているボタン(%d)には既に別のコマンドを持っています（再帰禁止）", name);
				return nullptr;
			}
			if (i >= MAX_SEQUENCE_SIZE) {
				K__Error(u8"キーコマンドが長すぎます");
				return nullptr;
			}
		}

		CSquenceKeyElm *cmdkey = new CSquenceKeyElm(this /*IKeyHistory*/);
		// コマンド列
		for (int i=0; buttons[i]; i++) {
			const char *name = buttons[i];
			cmdkey->add_seq(get_button(name)); // ボタンを追加
		}
		return cmdkey;
	}
	virtual bool isConflict(const char *button1, const char *button2) const override {
		// action1 と btn_action2d2 に同一のキーがバインドされているなら true
		// 存在しない KActionButtonId を指定した場合は必ず false になる
		bool retval = false;
		m_mutex.lock();
		{
			const CActionButtonKeyElm *btn1 = get_button(button1);
			const CActionButtonKeyElm *btn2 = get_button(button2);
			retval = check_conflict(btn1, btn2);
		}
		m_mutex.unlock();
		return retval;
	}
	virtual void newFrame() override {
		m_mutex.lock();

		// 全ボタンのフレームを更新
		for (size_t i=0; i<m_buttons.size(); i++) {
			CActionButtonKeyElm *btn = m_buttons[i];
			btn->newFrame();
		}

		// 現在時刻
		int now_msec = get_clock_msec();

		// 新フレームに入った時点で押されているボタン
		SKeyHistoryItem histitem;
		histitem.timestamp_msec = now_msec;
		for (size_t i=0; i<m_buttons.size(); i++) {
			CActionButtonKeyElm *btn = m_buttons[i];
			if (btn->isJustPressedAtFrame(nullptr, false)) {
				// ボタンが今押された
				btn->m_timestamp_press = now_msec; // ボタンONの検出時刻
				btn->m_timestamp_nextrepeat = now_msec + AUTOREPEAT_DELAY_MSEC;
				histitem.pressed_buttons.push_back(btn);
			
			} else if (btn->isPressed(nullptr, m_poll_flags)) {
				// ボタンが以前から押され続けている。
				// キーリピートありなら連続入力処理する
				if (btn->m_flags & KButtonFlag_REPEAT) {
					if (btn->m_timestamp_nextrepeat <= now_msec) { // オートリピート開始時刻を過ぎている？
						int delta = now_msec - btn->m_timestamp_nextrepeat;
						if (delta < AUTOREPEAT_INTERVAL_MSEC*2) {
							btn->m_repeat = true; // オートリピート発動
							btn->m_timestamp_nextrepeat += AUTOREPEAT_INTERVAL_MSEC; // 次回のオートリピート時刻
						} else {
							// あまりにも時間が経ちすぎているのでリピート無効
						}
					}
				}
			}
		}

		// 押されたボタンがあったらフレーム単位の履歴に追加する
		if (histitem.pressed_buttons.size() > 0) {
			m_history_per_frame.push_back(histitem);
		}

		// 履歴のサイズが一定数を超えないように
		while (m_history_per_frame.size() > MAX_HISTORY_SIZE) {
			m_history_per_frame.erase(m_history_per_frame.begin());
		}

		m_mutex.unlock();
	}
	#pragma endregion // KButton inherit

private:
	void poll() {
		if (KJoystick::isInit()) {
			for (int i=0; i<KJoystick::MAX_CONNECT; i++) {
				KJoystick::poll(i);
			}
		}
		m_mutex.lock();

		// 全ボタンの入力状態を更新
		for (size_t i=0; i<m_buttons.size(); i++) {
			CActionButtonKeyElm *btn = m_buttons[i];
			btn->poll(m_poll_flags);
		}

		// 現在時刻
		int time = get_clock_msec();
		
		// ポーリングの時点で押されているボタン
		SKeyHistoryItem histitem;
		histitem.timestamp_msec = time;
		for (size_t i=0; i<m_buttons.size(); i++) {
			CActionButtonKeyElm *btn = m_buttons[i];
			if (btn->isJustPressedAtPoll(nullptr)) {
				btn->m_timestamp_press = time; // ボタンONの検出時刻
				histitem.pressed_buttons.push_back(btn);

			} else {
				// ボタンが押され続けている or 全く押されていない。
				// ポーリングではキーリピート処理しないので、ここでは何もしない
			}
		}

		// 押されたボタンがあったらポーリング単位の履歴に追加する
		if (histitem.pressed_buttons.size() > 0) {
			m_history_per_poll.push_back(histitem);
		}

		// 履歴のサイズが一定数を超えないように
		while (m_history_per_poll.size() > MAX_HISTORY_SIZE) {
			m_history_per_poll.erase(m_history_per_poll.begin());
		}

		m_mutex.unlock();
	}
	bool check_conflict(const CActionButtonKeyElm *b1, const CActionButtonKeyElm *b2) const {
		// b1 と b2 に同一のキーがバインドされているなら true を返す
		// (つまりキーを押したときに b1 と b2 の両方が反応するなら ture を返す）
		if (b1 == nullptr) return false;
		if (b2 == nullptr) return false;
		for (size_t i=0; i+1<b1->m_keys.size(); i++) {
			for (size_t j=i+1; j<b2->m_keys.size(); j++) {
				const IKeyElm *k1 = b1->m_keys[i];
				const IKeyElm *k2 = b2->m_keys[j];
				if (k1 && k2) {
					if (k1->isConflictWith(k2)) {
						return true;
					}
				}
			}
		}
		return false;
	}
	bool has_conflict() const {
		// バインドが重複しているボタンがあるなら true
		// vector 内のすべての組み合わせについて衝突を調べる
		for (size_t i=0; i+1<m_buttons.size(); i++) {
			for (size_t j=i+1; j<m_buttons.size(); j++) {
				const CActionButtonKeyElm *k1 = m_buttons[i];
				const CActionButtonKeyElm *k2 = m_buttons[j];
				if (check_conflict(k1, k2)) {
					return true;
				}
			}
		}
		return false;
	}
	CActionButtonKeyElm * get_button(const char *action) {
		K__ASSERT_RETURN_ZERO(action);
		// ボタンを取得
		if (strlen(action) == 0) return nullptr;
		for (size_t i=0; i<m_buttons.size(); i++) {
			if (m_buttons[i]->m_name.compare(action) == 0) {
				return m_buttons[i];
			}
		}
		return nullptr;
	}
	const CActionButtonKeyElm * get_button(const char *action) const {
		K__ASSERT_RETURN_ZERO(action);
		// ボタンを取得
		if (strlen(action) == 0) return nullptr;
		for (size_t i=0; i<m_buttons.size(); i++) {
			if (m_buttons[i]->m_name.compare(action) == 0) {
				return m_buttons[i];
			}
		}
		return nullptr;
	}

	// 連続入力のタイムアウト計測用時刻
	int get_clock_msec() const {
		return (int)K__ClockMsec32();
	}
};

} // namespace

KButton * KButton::create() {
	kns_btn::CButtonMgrImpl *mgr = new kns_btn::CButtonMgrImpl();
	return mgr;
}
#pragma endregion // Buttons







#define MAX_KEY_SEQUENCE_LENGTH 8

typedef std::unordered_map<KJoystick::Button, std::string> JOYNAMES;

static JOYNAMES & _GetJoyNames() {
	static JOYNAMES s_Names;
	if (s_Names.empty()) {
		s_Names[KJoystick::BUTTON_1 ] = "Button1";
		s_Names[KJoystick::BUTTON_2 ] = "Button2";
		s_Names[KJoystick::BUTTON_3 ] = "Button3";
		s_Names[KJoystick::BUTTON_4 ] = "Button4";
		s_Names[KJoystick::BUTTON_5 ] = "Button5";
		s_Names[KJoystick::BUTTON_6 ] = "Button6";
		s_Names[KJoystick::BUTTON_7 ] = "Button7";
		s_Names[KJoystick::BUTTON_8 ] = "Button8";
		s_Names[KJoystick::BUTTON_9 ] = "Button9";
		s_Names[KJoystick::BUTTON_10] = "Button10";
		s_Names[KJoystick::BUTTON_11] = "Button11";
		s_Names[KJoystick::BUTTON_12] = "Button12";
		s_Names[KJoystick::BUTTON_13] = "Button13";
		s_Names[KJoystick::BUTTON_14] = "Button14";
		s_Names[KJoystick::BUTTON_15] = "Button15";
		s_Names[KJoystick::BUTTON_16] = "Button16";
	}
	return s_Names;
}
static void _UpdateButtonGui(KButton *mgr) {
	size_t num = mgr->getButtonCount();
	for (size_t i=0; i<num; i++) {

		// ボタンIDを得る
		const char *name = mgr->getButtonItem(i)->get_name();

		// ボタンが入力状態かどうか？
		if (mgr->isButtonDown(name, nullptr)) {
			// 入力確定
			KImGui::KImGui_PushTextColor(KImGui::KImGui_COLOR_DEFAULT());
			ImGui::Text("[%s]", name);
			KImGui::KImGui_PopTextColor();

		} else if (mgr->isButtonDownByPoll(name, nullptr)) {
			// 受付済み
			KImGui::KImGui_PushTextColor(KImGui::KImGui_COLOR_WARNING);
			ImGui::Text("[%s]", name);
			KImGui::KImGui_PopTextColor();

		} else {
			// 入力なし
			KImGui::KImGui_PushTextColor(KImGui::KImGui_COLOR_DISABLED());
			ImGui::Text("[%s]", name);
			KImGui::KImGui_PopTextColor();
		}
	}
}

class CInputMap: public KManager, public KInspectorCallback {
	KButton *m_game_buttons; // ゲーム内入力用のボタンマネージャ
	KButton *m_app_buttons; // アプリ操作用のボタンマネージャ
public:
	CInputMap() {
		m_game_buttons = KButton::create();
		m_app_buttons = KButton::create();
		KEngine::addManager(this);
		KEngine::addInspectorCallback(this); // KInspectorCallback
	}
	virtual ~CInputMap() {
		K_Drop(m_game_buttons);
		K_Drop(m_app_buttons);
	}
	virtual void onInspectorGui() override { // KInspectorCallback
		ImGui::Text("App buttons");
		ImGui::Separator();
		ImGui::Indent();
		_UpdateButtonGui(m_app_buttons);
		ImGui::Unindent();

		ImGui::Text("Game buttons");
		ImGui::Separator();
		ImGui::Indent();
		_UpdateButtonGui(m_game_buttons);
		ImGui::Unindent();
	}
	virtual void on_manager_appframe() override {
		{
			// ウィンドウが非アクティブだったりIMGUIにフォーカスが行っている場合に
			// キーボード入力を拾ってしまわないように調整する
			KPollFlags flags = 0;
			if (KEngine::getStatus(KEngine::ST_KEYBOARD_BLOCKED)) {
				flags |= POLLFLAG_NO_KEYBOARD;
				flags |= POLLFLAG_NO_MOUSE;
			}
			m_app_buttons->setPollFlags(flags);
			m_game_buttons->setPollFlags(flags);
		}

		m_app_buttons->newFrame();
		m_app_buttons->poll();

		for (int i=0; i<m_app_buttons->getButtonCount(); i++) {
			KButtonItem *btn = m_app_buttons->getButtonItem(i);
			const char *name = btn->get_name();
			KButtonFlags flags = btn->get_flags();
			if (m_app_buttons->getButtonDown(name, nullptr)) {
				KSig sig(K_SIG_INPUT_APP_BUTTON);
				sig.setString("button", name);
				KEngine::broadcastSignal(sig);
			}
		}
		if (0) {
			// ゲームボタンのポーリングを続ける.
			// デバッグによる一時停止中にキーシーケンスを入力し、完了することができるようになる
			m_game_buttons->poll();
		}
	}
	virtual void on_manager_frame() override {
		m_game_buttons->newFrame();
		m_game_buttons->poll();
	}
	void setPollFlags(KPollFlags flags) {
		m_app_buttons->setPollFlags(flags);
		m_game_buttons->setPollFlags(flags);
	}
	void addAppButton(const char *button, KButtonFlags flags) {
		m_app_buttons->addButton(button, flags);
	}
	void addGameButton(const char *button, KButtonFlags flags) {
		m_game_buttons->addButton(button, flags);
	}
	bool isAppButtonDown(const char *button) const {
		return m_app_buttons->isButtonDown(button, nullptr);
	}
	bool getAppButtonDown(const char *button) const {
		return m_app_buttons->getButtonDown(button, nullptr);
	};
	bool isGameButtonDown(const char *button) const {
		return m_game_buttons->isButtonDown(button, nullptr);
	}
	bool getGameButtonDown(const char *button) const {
		return m_game_buttons->getButtonDown(button, nullptr);
	};
	int isConflict(const char *button1, const char *button2) const {
		return m_game_buttons->isConflict(button1, button2);
	}
	void unbindByTag(const char *button, int tag) {
		KButtonItem *btn = m_game_buttons->findButtonItem(button);
		if (btn) {
			for (int i=0; i<btn->get_key_count(); i++) {
				IKeyElm *key = btn->get_key(i);
				if (key->getTag() == tag) {
					btn->del_key(i);
					return;
				}
			}
		}
	}
	void bindAppKey(const char *button, KKeyboard::Key key, KKeyboard::Modifiers mods) {
		IKeyElm *elm = m_app_buttons->createKeyboardKey(key, mods);
		if (elm) {
			m_app_buttons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindKeyboardKey(const char *button, KKeyboard::Key key, KKeyboard::Modifiers mods, int tag) {
		IKeyElm *elm = m_game_buttons->createKeyboardKey(key, mods);
		if (elm) {
			elm->setTag(tag);
			m_game_buttons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindJoystickKey(const char *button, KJoystick::Button joybtn, int tag) {
		IKeyElm *elm = m_game_buttons->createJoystickKey(joybtn);
		if (elm) {
			elm->setTag(tag);
			m_game_buttons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindJoystickAxis(const char *button, KJoystick::Axis axis, int halfrange, int tag) {
		IKeyElm *elm = m_game_buttons->createJoystickAxis(axis, halfrange);
		if (elm) {
			elm->setTag(tag);
			m_game_buttons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindJoystickPov(const char *button, int xsign, int ysign, int tag) {
		IKeyElm *elm = m_game_buttons->createJoystickPov(xsign, ysign);
		if (elm) {
			elm->setTag(tag);
			m_game_buttons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindMouseKey(const char *button, KMouse::Button mouse_btn, int tag) {
		IKeyElm *elm = m_game_buttons->createMouseKey(mouse_btn);
		if (elm) {
			elm->setTag(tag);
			m_game_buttons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindKeySequence(const char *button, const char *keys[], int tag) {
		IKeyElm *elm = m_game_buttons->createCommand(keys);
		if (elm) {
			elm->setTag(tag);
			m_game_buttons->bindKey(button, elm);
			elm->drop();
		}
	}
	IKeyboardKeyElm * findKeyboardByTag(const char *button, int tag) {
		KButtonItem *btn = m_game_buttons->findButtonItem(button);
		if (btn == nullptr) return nullptr;

		for (int i=0; i<btn->get_key_count(); i++) {
			IKeyElm *key = btn->get_key(i);
			if (key->getTag() == tag) {
				IKeyboardKeyElm *kbkey = dynamic_cast<IKeyboardKeyElm*>(key);
				if (kbkey) {
					return kbkey;
				}
			}
		}
		return nullptr;
	}
	IJoystickKeyElm * findJoystickByTag(const char *button, int tag) {
		KButtonItem *btn = m_game_buttons->findButtonItem(button);
		if (btn == nullptr) return nullptr;

		for (int i=0; i<btn->get_key_count(); i++) {
			IKeyElm *key = btn->get_key(i);
			if (key->getTag() == tag) {
				IJoystickKeyElm *jskey = dynamic_cast<IJoystickKeyElm*>(key);
				if (jskey) {
					return jskey;
				}
			}
		}
		return nullptr;
	}
	void resetAllButtonStates() {
	}
	const char * getJoystickName(KJoystick::Button btn) const {
		return _GetJoyNames()[btn].c_str();
	}
	const char * getKeyboardName(KKeyboard::Key key) const {
		return KKeyboard::getKeyName(key); 
	}
	bool getJoystickFromName(const char *s, KJoystick::Button *btn) const {
		const JOYNAMES &names = _GetJoyNames();
		for (auto it=names.begin(); it!=names.end(); ++it) {
			if (it->second.compare(s) == 0) {
				if (btn) *btn = it->first;
				return true;
			}
		}
		return false;
	}
	bool getKeyboardFromName(const char *s, KKeyboard::Key *key) const {
		KKeyboard::Key k = KKeyboard::findKeyByName(s);
		if (k != KKeyboard::KEY_NONE) {
			if (key) *key = k;
			return true;
		}
		return false;
	}
}; // CInputMap

#pragma region KInputMap
static CInputMap *g_InputMap = nullptr;

void KInputMap::install() {
	K__Assert(g_InputMap == nullptr);
	g_InputMap = new CInputMap();
}
void KInputMap::uninstall() {
	if (g_InputMap) {
		g_InputMap->drop();
		g_InputMap = nullptr;
	}
}

/// アプリケーションボタン（ポーズに影響されない）を登録する
/// @node ここで登録するのは仮想ボタンであり、まだどの物理キーにもバインドされていない。
/// この仮想ボタンを物理キーに関連付けるためには bindAppKey を使う 
/// （アプリケーションボタンにバインドできるのはキーボードのキーのみ）
void KInputMap::addAppButton(const char *button, KButtonFlags flags) {
	K__Assert(g_InputMap);
	g_InputMap->addAppButton(button, flags);
}

/// ゲームボタン（ポーズに影響されない）が押された状態になっているか？
bool KInputMap::isAppButtonDown(const char *button) {
	K__Assert(g_InputMap);
	return g_InputMap->isAppButtonDown(button);
}

/// アプリボタン（ポーズに影響されない）がたった今押されたか？
bool KInputMap::getAppButtonDown(const char *button) {
	K__Assert(g_InputMap);
	return g_InputMap->getAppButtonDown(button);
}

/// ゲームボタン（ポーズに影響される）を登録する
/// @node ここで登録するのは仮想ボタンであり、まだどの物理キーにもバインドされていない。
/// この仮想ボタンを物理キーに関連付けるためにはバインド関数を使う
/// @see bindKeyboardKey, bindJoystickKey, bindJoystickAxis, bindJoystickPov, bindMouseKey, bindKeySequence
void KInputMap::addGameButton(const char *button, KButtonFlags flags) {
	K__Assert(g_InputMap);
	g_InputMap->addGameButton(button, flags);
}

/// ゲームボタン（ポーズに影響される）が押された状態になっているか？
bool KInputMap::isGameButtonDown(const char *button) {
	K__Assert(g_InputMap);
	return g_InputMap->isGameButtonDown(button);
}

/// ゲームボタン（ポーズに影響される）がたった今押されたか？
bool KInputMap::getGameButtonDown(const char *button) {
	K__Assert(g_InputMap);
	return g_InputMap->getGameButtonDown(button);
}

/// 指定されたボタンが押された状態になっているか？
/// （アプリボタンとゲームボタンのどちらにも対応する）
bool KInputMap::isButtonDown(const char *button) {
	return isAppButtonDown(button) || isGameButtonDown(button);
}

/// 指定されたボタンががたった今押されたか？
/// （アプリボタンとゲームボタンのどちらにも対応する）
bool KInputMap::getButtonDown(const char *button) {
	return getAppButtonDown(button) || getGameButtonDown(button);
}







/// addAppButton で登録した仮想ボタンに対してキーボードのキーをバインドする
void KInputMap::bindAppKey(const char *button, KKeyboard::Key key, KKeyboard::Modifiers mods) {
	K__Assert(g_InputMap);
	g_InputMap->bindAppKey(button, key, mods);
}

/// addButton で登録した仮想ボタンに対してキーボードのキーをバインドする
/// @note tag に適当な整数値を指定した場合、後でその値を検索キーとして特定のキーバインドを探すことができる。
/// /
/// @see unbindByTag, findKeyboardByTag, findJoystickByTag
void KInputMap::bindKeyboardKey(const char *button, KKeyboard::Key key, KKeyboard::Modifiers mods, int tag) {
	K__Assert(g_InputMap);
	g_InputMap->bindKeyboardKey(button, key, mods, tag);
}
void KInputMap::bindJoystickKey(const char *button, KJoystick::Button joybtn, int tag) {
	K__Assert(g_InputMap);
	g_InputMap->bindJoystickKey(button, joybtn, tag);
}
void KInputMap::bindJoystickAxis(const char *button, KJoystick::Axis axis, int halfrange, int tag) {
	K__Assert(g_InputMap);
	g_InputMap->bindJoystickAxis(button, axis, halfrange, tag);
}
void KInputMap::bindJoystickPov(const char *button, int xsign, int ysign, int tag) {
	K__Assert(g_InputMap);
	g_InputMap->bindJoystickPov(button, xsign, ysign, tag);
}
void KInputMap::bindMouseKey(const char *button, KMouse::Button mousebtn, int tag) {
	K__Assert(g_InputMap);
	g_InputMap->bindMouseKey(button, mousebtn, tag);
}
void KInputMap::bindKeySequence(const char *button, const char *keys[], int tag) {
	K__Assert(g_InputMap);
	g_InputMap->bindKeySequence(button, keys, tag);
}
void KInputMap::unbindByTag(const char *button, int tag) {
	K__Assert(g_InputMap);
	g_InputMap->unbindByTag(button, tag);
}
int KInputMap::isConflict(const char *button1, const char *button2) {
	K__Assert(g_InputMap);
	return g_InputMap->isConflict(button1, button2);
}
void KInputMap::resetAllButtonStates() {
	K__Assert(g_InputMap);
	g_InputMap->resetAllButtonStates();
}
IKeyboardKeyElm * KInputMap::findKeyboardByTag(const char *button, int tag) {
	K__Assert(g_InputMap);
	return g_InputMap->findKeyboardByTag(button, tag);
}
IJoystickKeyElm * KInputMap::findJoystickByTag(const char *button, int tag) {
	K__Assert(g_InputMap);
	return g_InputMap->findJoystickByTag(button, tag);
}
const char * KInputMap::getJoystickName(KJoystick::Button joybtn) {
	K__Assert(g_InputMap);
	return g_InputMap->getJoystickName(joybtn);
}
const char * KInputMap::getKeyboardName(KKeyboard::Key key) {
	K__Assert(g_InputMap);
	return g_InputMap->getKeyboardName(key);
}
bool KInputMap::getKeyboardFromName(const char *s, KKeyboard::Key *key) {
	K__Assert(g_InputMap);
	return g_InputMap->getKeyboardFromName(s, key);
}
bool KInputMap::getJoystickFromName(const char *s, KJoystick::Button *btn) {
	K__Assert(g_InputMap);
	return g_InputMap->getJoystickFromName(s, btn);
}
void KInputMap::setPollFlags(KPollFlags flags) {
	K__Assert(g_InputMap);
	g_InputMap->setPollFlags(flags);
}
#pragma endregion // KInputMapAct

} // namespace
