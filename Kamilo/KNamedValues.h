#pragma once
#include "KRef.h"
#include "KLog.h"
#include "KStream.h"
#include "KString.h"
#include "KXml.h"
#include "KMath.h"
#include "keng_ext.h"

#define NV_TEST 0 // 0=OLD 1=PureVirtual


namespace Kamilo {


/// 「名前＝値」の形式のデータを順序無しで扱う
///
/// - 順序を考慮する場合は KOrderedParameters を使う。
/// - 行と列から成る二次元的なテーブルを扱いたい場合は KTable を使う
class KNamedValues {
	typedef std::pair<KPath, std::string> PAIR;
	std::vector<PAIR> mItems;
public:
	static KNamedValues * create();
#if NV_TEST
	virtual size_t size() const = 0;
	virtual void clear() = 0;
	virtual void remove(const char *name) = 0;
	virtual bool loadFromString(const char *xml_u8, const char *filename) = 0;
	virtual bool loadFromXml(KXmlReader *xml, bool packed_in_attr=false) = 0;
	virtual std::string saveToString(bool pack_in_attr=false) const = 0;
	virtual const char * getName(int index) const = 0;
	virtual void setString(const char *name, const char *value) = 0;
	virtual const char * getString(int index) const = 0;
#else
	virtual int size() const {
		return (int)mItems.size();
	}
	virtual void clear() {
		mItems.clear();
	}
	virtual void remove(const char *name) {
		int index = find(name);
		if (index >= 0) {
			mItems.erase(mItems.begin() + index);
		}
	}
	virtual bool loadFromString(const char *xml_u8, const char *filename) {
		clear();

		bool result = false;
		KXmlElement *xml = KXmlElement::createFromString(xml_u8, filename);
		if (xml) {
			result = loadFromXml(xml->getNode(0), false);
			xml->drop();
		} else {
			KLog::printError("E_XML: Failed to load: %s", filename);
		}

		return result;
	}
	virtual bool loadFromXml(KXmlElement *elm, bool pack_in_attr=false) {
		clear();
		if (pack_in_attr) {
			// <XXX AAA="BBB" CCC="DDD" EEE="FFF" ... >
			int numAttrs = elm->getAttrCount();
			for (int i=0; i<numAttrs; i++) {
				const char *key = elm->getAttrName(i);
				const char *val = elm->getAttrValue(i);
				if (key && val) {
					setString(key, val);
				}
			}
		} else {
			// <XXX name="AAA">BBB</XXX>/>
			for (int i=0; i<elm->getNodeCount(); i++) {
				KXmlElement *xItem = elm->getNode(i);
				const char *key = xItem->findAttr("name");
				const char *val = xItem->getText();
				if (key && val) {
					setString(key, val);
				}
			}
		}
		return true;
	}

	virtual void saveToXml(KXmlElement *elm, bool pack_in_attr=false) const {
		if (pack_in_attr) {
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				elm->setAttr(it->first.u8(), it->second.c_str());
			}
		} else {
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				KXmlElement *xPair = elm->addNode("Pair");
				xPair->setAttr("name", it->first.u8());
				xPair->setText(it->second.c_str());
			}
		}
	}

	virtual std::string saveToString(bool pack_in_attr=false) const {
		std::string s;
		if (pack_in_attr) {
			s += "<KNamedValues\n";
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				s += KStringUtils::K_sprintf("\t%s = '%s'\n", it->first.u8(), it->second.c_str());
			}
			s += "/>\n";
		} else {
			s += "<KNamedValues>\n";
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				s += KStringUtils::K_sprintf("  <Pair name='%s'>%s</Pair>\n", it->first.u8(), it->second.c_str());
			}
			s += "</KNamedValues>\n";
		}
		return s;
	}
	virtual const char * getName(int index) const {
		if (0 <= index && index < (int)mItems.size()) {
			return mItems[index].first.u8();
		} else {
			return nullptr;
		}
	}
	virtual const char * getString(int index) const {
		if (0 <= index && index < (int)mItems.size()) {
			return mItems[index].second.c_str();
		} else {
			return nullptr;
		}
	}
	virtual void setString(const char *name, const char *value) {
		if (name && name[0]) {
			int index = find(name);
			if (index >= 0) {
				mItems[index].second = value ? value : "";
			} else {
				mItems.push_back(PAIR(name, value));
			}
		}
	}
#endif
	virtual KNamedValues * clone() const {
		KNamedValues *nv = KNamedValues::create();
		nv->append(this);
		return nv;
	}
	virtual bool loadFromFile(const char *filename) {
		bool success = false;
		KInputStream file = KInputStream::fromFileName(filename);
		if (file.isOpen()) {
			std::string xml_bin = file.readBin();
			std::string xml_u8 = KConv::toUtf8(xml_bin);
			if (! xml_u8.empty()) {
				success = loadFromString(xml_u8.c_str(), filename);
			}
		}
		return success;
	}
	virtual void saveToFile(const char *filename, bool pack_in_attr=false) const {
		KOutputStream file = KOutputStream::fromFileName(filename);
		if (file.isOpen()) {
			std::string xml_u8 = saveToString(pack_in_attr);
			file.write(xml_u8.c_str(), xml_u8.size());
		}
	}
	virtual void append(const KNamedValues *nv) {
		if (nv) {
			for (int i=0; i<nv->size(); i++) {
				setString(nv->getName(i), nv->getString(i));
			}
		}
	}
	virtual int find(const char *name) const {
		for (int i=0; i<size(); i++) {
			if (strcmp(getName(i), name) == 0) {
				return i;
			}
		}
		return -1;
	}
	virtual bool contains(const char *name) const {
		return find(name) >= 0;
	}
	virtual const char * getString(const char *name, const char *defaultValue=nullptr) const {
		int index = find(name);
		if (index >= 0) {
			return getString(index);
		} else {
			return defaultValue;
		}
	}

	#pragma region Bool
	virtual void setBool(const char *name, bool value) {
		setString(name, value ? "1" : "0");
	}
	virtual bool queryBool(const char *name, bool *outValue) const {
		const char *s = getString(name);
		if (s) {
			assert(strcmp(s, "on" ) != 0);
			assert(strcmp(s, "off" ) != 0);
			int val = 0;
			if (KStringUtils::toIntTry(s, &val)) {
				if (outValue) *outValue = val != 0;
				return true;
			}
		}
		return false;
	}
	virtual bool getBool(const char *name, bool defaultValue=false) const {
		bool result = defaultValue;
		queryBool(name, &result);
		return result;
	}
	#pragma endregion // Bool
	
	#pragma region Int
	virtual void setInteger(const char *name, int value) {
		char s[32] = {0};
		sprintf_s(s, sizeof(s), "%d", value);
		setString(name, s);
	}
	virtual bool queryInteger(const char *name, int *outValue) const {
		const char *s = getString(name);
		if (s && KStringUtils::toIntTry(s, outValue)) {
			return true;
		}
		return false;
	}
	virtual int getInteger(const char *name, int defaultValue=0) const {
		int result = defaultValue;
		queryInteger(name, &result);
		return result;
	}
	#pragma endregion // Int

	#pragma region UInt
	virtual void setUInt(const char *name, unsigned int value) {
		char s[32] = {0};
		sprintf_s(s, sizeof(s), "%u", value);
		setString(name, s);
	}
	virtual bool queryUInt(const char *name, unsigned int *outValue) const {
		const char *s = getString(name);
		if (s && KStringUtils::toUintTry(s, outValue)) {
			return true;
		}
		return false;
	}
	virtual int getUInt(const char *name, unsigned int defaultValue=0) const {
		unsigned int result = defaultValue;
		queryUInt(name, &result);
		return result;
	}
	#pragma endregion // Int

	#pragma region Float
	virtual void setFloat(const char *name, float value) {
		char s[32] = {0};
		sprintf_s(s, sizeof(s), "%f", value);
		setString(name, s);
	}
	virtual bool queryFloat(const char *name, float *outValue) const {
		const char *s = getString(name);
		if (s && KStringUtils::toFloatTry(s, outValue)) {
			return true;
		}
		return false;
	}
	virtual float getFloat(const char *name, float defaultValue=0.0f) const {
		float result = defaultValue;
		queryFloat(name, &result);
		return result;
	}
	#pragma endregion // Float

	#pragma region FloatArray
	virtual void setFloatArray(const char *name, const float *values, int count) {
		std::string s;
		if (count >= 1) {
			s = KStringUtils::K_sprintf("%g", values[0]);
		}
		for (int i=1; i<count; i++) {
			s += KStringUtils::K_sprintf(", %g", values[i]);
		}
		setString(name, s.c_str());
	}
	virtual int getFloatArray(const char *name, float *outValues, int maxout) const {
		const char *s = getString(name);
		KToken tok(s, ", ");
		if (outValues) {
			int idx = 0;
			while (idx<(int)tok.size() && idx<maxout) {
				outValues[idx] = KStringUtils::toFloat(tok[idx]);
				idx++;
			}
			return idx;
		} else {
			return (int)tok.size();
		}
	}
	#pragma endregion

	#pragma region Binary
	virtual void setBinary(const char *name, const void *data, int size) {
		std::string s;
		s = KStringUtils::K_sprintf("%08x:", size);
		for (int i=0; i<size; i++) {
			s += KStringUtils::K_sprintf("%02x", ((uint8_t*)data)[i]);
		}
		setString(name, s.c_str());
	}
	virtual int getBinary(const char *name, void *out, int maxsize) const {
		const char *s = getString(name);

		// コロンよりも左側の文字列を得る。データのバイト数が16進数表記されている
		int colonPos = KStringUtils::findChar(s, ':');
		if (colonPos <= 0) {
			return 0; // NO DATA 
		}

		// 得られたバイト数HEX文字列から整数値を得る
		char szstr[16] = {0};
		strncpy(szstr, s, colonPos);
		szstr[colonPos] = '\0';
		int sz = KStringUtils::toInt(szstr);

		if (sz <= 0) {
			return 0; // Empty
		}
		if (out) {
			int start = colonPos + 1; // ':' の次の文字へ移動
			int copysize = KMath::min(sz, maxsize);
			uint8_t *out_u8 = (uint8_t*)out;
			for (int i=0; i<copysize; i++) {
				char hex[3] = {s[start+i*2], s[start+i*2+1], '\0'};
				out_u8[i] = KStringUtils::toInt(hex);
			}
			return copysize;
		} else {
			return sz;
		}
	}
	#pragma endregion
};

inline void KNamedValues_del(KNamedValues *nv) {
	if (nv) delete nv;
}



} // namespace
