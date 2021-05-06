#include "KNamedValues.h"

namespace Kamilo {

#pragma region KNamedValues
#if NV_TEST
class CNamedValuesImpl: public KNamedValues {
	typedef std::pair<KPath, std::string> PAIR;
	std::vector<PAIR> mItems;
public:
	virtual size_t size() const override {
		return mItems.size();
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
	virtual bool loadFromString(const char *xml_u8, const char *filename) override {
		clear();

		bool result = false;
		char errmsg[1024] = {0};
		KXmlReader *xml = K_createXmlReader(xml_u8, filename, errmsg, sizeof(errmsg));
		if (xml) {
			if (xml->enterFirstChild()) {
				result = loadFromXml(xml, false);
				xml->exitChild();
			}
			xml->drop();
		} else {
			KLog::printError("E_XML: %s", errmsg);
			KLog::printError("E_XML: Failed to load: %s", filename);
		}

		return result;
	}
	virtual bool loadFromXml(KXmlReader *xml, bool packed_in_attr=false) override {
		clear();
		if (packed_in_attr) {
			// <XXX AAA="BBB" CCC="DDD" EEE="FFF" ... >
			int numAttrs = xml->getAttrCount();
			for (int i=0; i<numAttrs; i++) {
				const char *key = xml->getAttrName(i);
				const char *val = xml->getAttrValue(i);
				if (key && val) {
					setString(key, val);
				}
			}
		} else {
			// <XXX name="AAA">BBB</XXX>/>
			if (xml->enterFirstChild()) {
				for (; xml->isValid(); xml->nextSibling()) {
					const char *key = xml->getAttrValue("name");
					const char *val = xml->getText();
					if (key && val) {
						setString(key, val);
					}
				}
				xml->exitChild();
			}
		}
		return true;
	}
	virtual std::string saveToString(bool pack_in_attr=false) const override {
		std::string s;
		if (pack_in_attr) {
			s += "<KNamedValues\n";
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				s += K_sprintf("\t%s = '%s'\n", it->first.u8(), it->second.c_str());
			}
			s += "/>\n";
		} else {
			s += "<KNamedValues>\n";
			for (auto it=mItems.begin(); it!=mItems.end(); ++it) {
				s += K_sprintf("  <Pair name='%s'>%s</Pair>\n", it->first.u8(), it->second.c_str());
			}
			s += "</KNamedValues>\n";
		}
		return s;
	}
	virtual const char * getName(int index) const override {
		if (0 <= index && index < (int)mItems.size()) {
			return mItems[index].first.u8();
		} else {
			return nullptr;
		}
	}
	virtual void setString(const char *name, const char *value) override {
		if (name && name[0]) {
			int index = find(name);
			if (index >= 0) {
				mItems[index].second = value ? value : "";
			} else {
				mItems.push_back(PAIR(name, value));
			}
		}
	}
	virtual const char * getString(int index) const override {
		if (0 <= index && index < (int)mItems.size()) {
			return mItems[index].second.c_str();
		} else {
			return nullptr;
		}
	}
};
KNamedValues * KNamedValues::create() {
	return new CNamedValuesImpl();
}
#else
KNamedValues * KNamedValues::create() {
	return new KNamedValues();
}
#endif
#pragma endregion // KNamedValues


} // namespace
