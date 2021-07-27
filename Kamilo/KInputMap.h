﻿/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "KRef.h"
#include "KJoystick.h"
#include "KMouse.h"
#include "KKeyboard.h"
#include "KAction.h"

namespace Kamilo {

class KEngine;


enum PollFlag {
	POLLFLAG_NO_KEYBOARD = 1,
	POLLFLAG_NO_JOYSTICK = 2,
	POLLFLAG_NO_MOUSE    = 4,
	POLLFLAG_FORCE_PUSH  = 8,
};
typedef int KPollFlags;




#pragma region // Buttons
class IKeyElm: public KRef {
public:
	IKeyElm() {}
	virtual ~IKeyElm() {}
	virtual bool isPressed(float *val, KPollFlags flags) const = 0;
	virtual bool isConflictWith(const IKeyElm *k) const = 0;
	bool hasTag(const std::string &tag) const { return m_Tag == tag; }
	void setTag(const std::string &tag) { m_Tag = tag; }

private:
	std::string m_Tag;
};

class IKeyboardKeyElm: public IKeyElm {
public:
	virtual void set_key(KKeyboard::Key key) = 0;
	virtual KKeyboard::Key get_key() const = 0;
	virtual KKeyboard::Modifiers get_modifiers() const = 0;
};

class IJoystickKeyElm: public IKeyElm {
public:
	virtual void set_button(KJoystick::Button key) = 0;
	virtual KJoystick::Button get_button() const = 0;
};

enum KButtonFlag {
	KButtonFlag_REPEAT = 1, // オートリピートあり
};
typedef int KButtonFlags;



namespace Test {
void Test_button();
}
#pragma endregion // Buttons



class KInputMap {
public:
	static void install();
	static void uninstall();

	static void addAppButton(const std::string &button, KButtonFlags flags=0);
	static bool isAppButtonDown(const std::string &button);
	static bool getAppButtonDown(const std::string &button);

	static void addGameButton(const std::string &button, KButtonFlags flags=0);
	static bool isGameButtonDown(const std::string &button);
	static bool getGameButtonDown(const std::string &button);
	static float getGameButtonAnalog(const std::string &button);

	static bool isButtonDown(const std::string &button);
	static bool getButtonDown(const std::string &button);
	static void postButtonDown(const std::string &button);
	static float getButtonAnalog(const std::string &button);

	// 互換性
	static void addButton(const std::string &button, KButtonFlags flags=0) {
		addGameButton(button, flags);
	}

	static void bindAppKey(const std::string &button, KKeyboard::Key key, KKeyboard::Modifiers mods=KKeyboard::MODIF_DONTCARE);
	static void bindKeyboardKey(const std::string &button, KKeyboard::Key key, KKeyboard::Modifiers mods=KKeyboard::MODIF_DONTCARE, const std::string &tag="");
	static void bindJoystickKey(const std::string &button, KJoystick::Button joybtn, const std::string &tag="");
	static void bindJoystickAxis(const std::string &button, KJoystick::Axis axis, int halfrange, const std::string &tag="", float threshold=0.2f);
	static void bindJoystickPov(const std::string &button, int xsign, int ysign, const std::string &tag="");
	static void bindMouseKey(const std::string &button, KMouse::Button mousebtn, const std::string &tag="");
	static void bindKeySequence(const std::string &button, const char *keys[], const std::string &tag="");
	static void unbindByTag(const std::string &button, const std::string &tag);
	static int isConflict(const std::string &button1, const std::string &button2);
	static void resetAllButtonStates();
	static IKeyboardKeyElm * findKeyboardByTag(const std::string &button, const std::string &tag);
	static IJoystickKeyElm * findJoystickByTag(const std::string &button, const std::string &tag);
	static std::string getJoystickName(KJoystick::Button joybtn);
	static std::string getKeyboardName(KKeyboard::Key key);
	static bool getKeyboardFromName(const std::string &s, KKeyboard::Key *key);
	static bool getJoystickFromName(const std::string &s, KJoystick::Button *btn);
	static void setPollFlags(KPollFlags flags);
};


} // namespace

