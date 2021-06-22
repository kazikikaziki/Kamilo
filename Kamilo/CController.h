#pragma once

class CController {
	static float _signf(float x) {
		if (x < 0.0f) return -1.0f;
		if (x > 0.0f) return  1.0f;
		return 0.0f;
	}
	static float _min(float x, float y) {
		return (x < y) ? x : y;
	}
	static float _max(float x, float y) {
		return (x > y) ? x : y;
	}
public:
	CController() {
		m_axis_x = 0;
		m_axis_y = 0;
		m_axis_z = 0;
		m_axis_reduce = 0.2f; // 1フレーム当たりの軸入力の消失値（正の値を指定）
		m_peeked.clear();
		m_buttons.clear();
		m_tirgger_timeout = 10;
		m_readtrigger = 0;
	}
	virtual void setTriggerTimeout(int val) {
		m_tirgger_timeout = val;
	}
	virtual void clearInputs() {
		m_axis_x = 0;
		m_axis_y = 0;
		m_axis_z = 0;
		m_buttons.clear();
		m_peeked.clear();
	}

	// トリガー入力をリセットする
	// なお beginReadTrigger/endReadTrigger で読み取りモードにしている場合でも clearTrighgers は関係なく動作する
	virtual void clearTriggers() {
		for (auto it=m_buttons.begin(); it!=m_buttons.end(); /*++it*/) {
			if (it->second >= 0) {
				it = m_buttons.erase(it);
			} else {
				it++;
			}
		}
		m_peeked.clear();
	}
	virtual void setInputAxisX(int i) {
		m_axis_x = (float)i;
	}
	virtual void setInputAxisY(int i) {
		m_axis_y = (float)i;
	}
	virtual void setInputAxisZ(int i) {
		m_axis_z = (float)i;
	}
	virtual void setInputAxisX_Analog(float value) {
		m_axis_x = value;
	}
	virtual void setInputAxisY_Analog(float value) {
		m_axis_y = value;
	}
	virtual void setInputAxisZ_Analog(float value) {
		m_axis_z = value;
	}
	virtual int getInputAxisX() {
		return (int)_signf(m_axis_x); // returns -1, 0, 1
	}
	virtual int getInputAxisY() {
		return (int)_signf(m_axis_y); // returns -1, 0, 1
	}
	virtual int getInputAxisZ() {
		return (int)_signf(m_axis_z); // returns -1, 0, 1
	}
	virtual float getInputAxisX_Analog() {
		return m_axis_x; // returns -1, 0, 1
	}
	virtual float getInputAxisZ_Analog() {
		return m_axis_z; // returns -1, 0, 1
	}

	virtual void setInputTrigger(const std::string &button) {
		if (button.empty()) return;
		m_buttons[button] = m_tirgger_timeout;
	}
	virtual void setInputBool(const std::string &button, bool pressed) {
		if (button.empty()) return;
		if (pressed) {
			m_buttons[button] = -1;
		} else {
			m_buttons.erase(button);
		}
	}
	virtual bool getInputBool(const std::string &button) {
		if (button.empty()) return false;
		return m_buttons.find(button) != m_buttons.end();
	}

	/// トリガーボタンの状態を得て、トリガー状態を false に戻す。
	/// トリガーをリセットしたくない場合は peekInputTrigger を使う。
	/// また、即座にリセットするのではなく特定の時点までトリガーのリセットを
	/// 先延ばしにしたい場合は beginReadTrigger() / endReadTrigger() を使う
	virtual bool getInputTrigger(const std::string &button) {
		if (button.empty()) return false;
		auto it = m_buttons.find(button);
		if (it != m_buttons.end()) {
			if (m_readtrigger > 0) {
				// PEEKモード中。
				// トリガーを false に戻さず、参照されたことだけを記録しておく
				m_peeked.insert(button);
			} else {
				// トリガー状態を false に戻す
				m_buttons.erase(it);
			}
			return true;
		}
		return false;
	}
	
	/// トリガーの読み取りモードを開始する
	/// endReadTrigger が呼ばれるまでの間は getInputTrigger を呼んでもトリガー状態が false に戻らない
	virtual void beginReadTrigger() {
		m_readtrigger++;
	}

	/// トリガーの読み取りモードを終了する。
	/// beginReadTrigger が呼ばれた後に getInputTrigger されたトリガーをすべて false に戻す
	virtual void endReadTrigger() { 
		m_readtrigger--;
		if (m_readtrigger == 0) {
			for (auto it=m_peeked.begin(); it!=m_peeked.end(); ++it) {
				const std::string &btn = *it;
				m_buttons.erase(btn);
			}
			m_peeked.clear();
		}
	}
	virtual void tickInput() {
		// 入力状態を徐々に消失させる
		m_axis_x = reduce(m_axis_x, m_axis_reduce);
		m_axis_y = reduce(m_axis_y, m_axis_reduce);
		m_axis_z = reduce(m_axis_z, m_axis_reduce);

		// トリガー入力のタイムアウトを処理する
		for (auto it=m_buttons.begin(); it!=m_buttons.end(); /*EMPTY*/) {
			if (it->second > 0) {
				it->second--;
				it++;
			} else if (it->second == 0) {
				it = m_buttons.erase(it);
			} else {
				it++; // タイムアウトが負の値の場合なら何もしない
			}
		}
	}

private:
	float reduce(float val, float delta) const {
		if (val > 0) {
			val = _max(val - delta, 0.0f);
		}
		if (val < 0) {
			val = _min(val + delta, 0.0f);
		}
		return val;
	}
	std::unordered_map<std::string, int> m_buttons;
	std::unordered_set<std::string> m_peeked;
	float m_axis_x;
	float m_axis_y;
	float m_axis_z;
	float m_axis_reduce;
	int m_tirgger_timeout;
	int m_readtrigger;
};
