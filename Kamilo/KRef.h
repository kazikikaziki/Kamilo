/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <unordered_set>
#include <vector>

#define K_Grab(a)  ((a) ? ((a)->grab(),     nullptr) : nullptr) // call KRef::grab()
#define K_Drop(a)  ((a) ? ((a)->drop(), (a)=nullptr) : nullptr) // call KRef::drop()

namespace Kamilo {


class KRef {
public:
	KRef();
	void grab() const;
	void drop() const;
	int getReferenceCount() const;
	virtual const char * getReferenceDebugString() const;
	void setReferenceDebugBreak(int cond_refcnt);

protected:
	virtual ~KRef();

private:
	mutable int mRefCnt;
	int mDebubBreakRefCnt;
};




template <class _KRef> class KRefSet {
public:
	KRefSet() {
	}
	bool empty() const {
		return mItems.empty();
	}
	size_t size() const {
		return mItems.size();
	}
	void clear() {
		for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
			_KRef *ref = *it;
			if (ref) ref->drop();
		}
		mItems.clear();
	}
	void insert(_KRef *ref) {
		auto pair = mItems.insert(ref);
		if (pair.second) {
			ref->grab(); // item added
		} else {
			// item alredy exists
		}
	}
	bool contains(_KRef *ref) {
		auto it = mItems.find(ref);
		return it != mItems.end();
	}
	void erase(_KRef *ref) {
		auto it = mItems.find(ref);
		if (it != mItems.end()) {
			if (ref) ref->drop();
			mItems.erase(it);
		}
	}
#if 0
	std::unordered_set<_KRef *>::iterator begin() {
		return mItems.begin();
	}
	std::unordered_set<_KRef *>::iterator end() {
		return mItems.end();
	}
#else
	std::unordered_set<_KRef *> items() {
		return mItems;
	}
#endif

private:
	std::unordered_set<_KRef *> mItems;
};

template <class _KRef> class KRefArray {
public:
	KRefArray() {
	}
	bool empty() const {
		return mItems.empty();
	}
	size_t size() const {
		return mItems.size();
	}
	void clear() {
		for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
			_KRef *ref = *it;
			if (ref) ref->drop();
		}
		mItems.clear();
	}
	void push_back(_KRef *ref) {
		if (ref) ref->grab();
		mItems.push_back(ref);
	}
	bool contains(_KRef *ref) {
		auto it = mItems.find(ref);
		return it != mItems.end();
	}
	void erase(_KRef *ref) {
		auto it = mItems.find(ref);
		if (it != mItems.end()) {
			if (ref) ref->drop();
			mItems.erase(it);
		}
	}
	_KRef * operator[] (int index) const {
		return mItems[index];
	}
#if 0
	std::vector<_KRef *>::iterator begin() {
		return mItems.begin();
	}
	std::vector<_KRef *>::iterator end() {
		return mItems.end();
	}
#endif
	std::vector<_KRef *> items() {
		return mItems;
	}

private:
	std::vector<_KRef *> mItems;
};

template <class _KRef> class KAutoRef {
public:
	KAutoRef() {
		mRef = nullptr;
	}
	KAutoRef(const _KRef *ref) {
		mRef = const_cast<_KRef*>(ref);
		K_Grab(mRef);
	}
	KAutoRef(const KAutoRef<_KRef> &autoref) {
		mRef = const_cast<_KRef*>(autoref.mRef);
		K_Grab(mRef);
	}
	~KAutoRef() {
		K_Drop(mRef);
	}
	void make() {
		K_Drop(mRef);
		mRef = new _KRef();
	}
	_KRef * get() const {
		return mRef;
	}
	void set(const _KRef *ref) {
		K_Drop(mRef);
		mRef = const_cast<_KRef*>(ref);
		K_Grab(mRef);
	}
	KAutoRef<_KRef> operator = (_KRef *ref) {
		set(ref);
		return *this;
	}
	KAutoRef<_KRef> operator = (const KAutoRef<_KRef> &autoref) {
		set(autoref.mRef);
		return *this;
	}
#if 1
	operator _KRef * () const {
		return mRef;
	}
	_KRef * operator()() const {
		return mRef;
	}
#endif
	bool operator == (const _KRef *ref) const {
		return mRef == ref;
	}
	bool operator == (const KAutoRef<_KRef> &autoref) const {
		return mRef == autoref.mRef;
	}
	bool operator != (const _KRef *ref) const {
		return mRef != ref;
	}
	bool operator != (const KAutoRef<_KRef> &autoref) const {
		return mRef != autoref.mRef;
	}
	bool operator < (const _KRef *ref) const {
		return mRef < ref;
	}
	bool operator < (const KAutoRef<_KRef> &autoref) const {
		return mRef < autoref.mRef;
	}
	_KRef * operator->() const {
		if (mRef) {
			return mRef;
		} else {
			__debugbreak();
			mRef = new _KRef();
			return mRef;
		}
	}
	bool isValid() const {
		return mRef != nullptr;
	}
	bool isNull() const {
		return mRef == nullptr;
	}
private:
	mutable _KRef *mRef;
};


} // namespace
