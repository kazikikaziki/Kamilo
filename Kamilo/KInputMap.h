/// @file
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
};
typedef int KPollFlags;




#pragma region // Buttons
class IKeyElm: public KRef {
public:
	IKeyElm() { 
		m_tag = 0;
	}
	virtual ~IKeyElm() {}
	virtual bool isPressed(float *val, KPollFlags flags) const = 0;
	virtual bool isConflictWith(const IKeyElm *k) const = 0;
	int getTag() { return m_tag; }
	void setTag(int value) { m_tag = value; }

private:
	int m_tag;
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



class KButton; // internal

class KInputMap {
public:
	static void install();
	static void uninstall();
	static void addAppButton(const char *button, KButtonFlags flags=0);
	static bool isAppButtonDown(const char *button);
	static bool getAppButtonDown(const char *button);
	static void addButton(const char *button, KButtonFlags flags=0);
	static bool isButtonDown(const char *button);
	static bool getButtonDown(const char *button);
	static int getButtonCount();
	static void bindAppKey(const char *button, KKeyboard::Key key, KKeyboard::Modifiers mods=KKeyboard::MODIF_DONTCARE);
	static void bindKeyboardKey(const char *button, KKeyboard::Key key, KKeyboard::Modifiers mods=KKeyboard::MODIF_DONTCARE, int tag=0);
	static void bindJoystickKey(const char *button, KJoystick::Button joybtn, int tag=0);
	static void bindJoystickAxis(const char *button, KJoystick::Axis axis, int halfrange, int tag=0);
	static void bindJoystickPov(const char *button, int xsign, int ysign, int tag=0);
	static void bindMouseKey(const char *button, KMouse::Button mousebtn, int tag=0);
	static void bindKeySequence(const char *button, const char *keys[], int tag=0);
	static void unbindByTag(const char *button, int tag);
	static int isConflict(const char *button1, const char *button2);
	static void resetAllButtonStates();
	static IKeyboardKeyElm * findKeyboardByTag(const char *button, int tag);
	static IJoystickKeyElm * findJoystickByTag(const char *button, int tag);
	static const char * getJoystickName(KJoystick::Button joybtn);
	static const char * getKeyboardName(KKeyboard::Key key);
	static bool getKeyboardFromName(const char *s, KKeyboard::Key *key);
	static bool getJoystickFromName(const char *s, KJoystick::Button *btn);
	static void setPollFlags(KPollFlags flags);
};


} // namespace

