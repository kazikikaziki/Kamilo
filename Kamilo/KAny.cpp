#include "KAny.h"
namespace Kamilo {

KAny::KAny() {
	mType = TP_NONE;
	mValue.p = 0;
}
KAny::KAny(bool val) {
	mType = TP_INT;
	mValue.i = val;
}
KAny::KAny(int val) {
	mType = TP_INT;
	mValue.i = val;
}
KAny::KAny(float val) {
	mType = TP_FLT;
	mValue.f = val;
}
KAny::KAny(const char *val) {
	mType = TP_STR;
	mValue.s = val;
}
KAny::KAny(void *val) {
	mType = TP_PTR;
	mValue.p = val;
}
KAny::KAny(KNode *n) {
	mType = TP_NOD;
	mValue.n = n;
}
KAny::KAny(const KColor &color) {
	mType = TP_COL;
	mValue.f4[0] = color.r;
	mValue.f4[1] = color.g;
	mValue.f4[2] = color.b;
	mValue.f4[3] = color.a;
}
bool KAny::isInt() const {
	return mType == TP_INT;
}
bool KAny::isFloat() const {
	return mType == TP_FLT;
}
bool KAny::isNumber() const {
	return isInt() || isFloat();
}
bool KAny::isString() const {
	return mType == TP_STR;
}
bool KAny::isPointer() const {
	return mType == TP_PTR;
}
bool KAny::isNode() const {
	return mType == TP_NOD;
}
bool KAny::isColor() const {
	return mType == TP_COL;
}
bool KAny::getBool(bool def) const {
	return getInt(def) != 0; 
}
int KAny::getInt(int def) const {
	if (isInt()) {
		return mValue.i;
	}
	if (isFloat()) {
		return (int)mValue.f;
	}
	return def;
}
float KAny::getFloat(float def) const {
	if (isFloat()) {
		return mValue.f;
	}
	if (isInt()) {
		return (float)mValue.i;
	}
	return def;
}
KColor KAny::getColor(const KColor &def) const {
	if (isColor()) {
		return KColor(
			mValue.f4[0],
			mValue.f4[1],
			mValue.f4[2],
			mValue.f4[3]
		);
	}
	return def;
}
const char * KAny::getString(const char *def) const {
	if (isString()) {
		return mValue.s;
	}
	return def;
}
void * KAny::getPointer() const {
	if (isPointer()) {
		return mValue.p;
	}
	return nullptr;
}
KNode * KAny::getNode() const {
	if (isNode()) {
		return mValue.n;
	}
	return nullptr;
}

} // namespace
