#include "KUserData.h"
//
#include "KInspector.h"
#include "KInternal.h"
#include "KZlib.h"
#include "KNamedValues.h"
#include "keng_game.h"

namespace Kamilo {



class CUserDataImpl: public KInspectorCallback {
	static std::string encryptString(const std::string &data, const std::string &key) {
		std::string s = data;
		xorString((char*)s.data(), s.size(), key.c_str());
		return s;
	}
	static std::string decryptString(const std::string &data, const std::string &key) {
		std::string s = data;
		xorString((char*)s.data(), s.size(), key.c_str());
		return s;
	}
	static void xorString(char *s, size_t len, const char *key) {
		K__Assert(s);
		K__Assert(key && key[0]); // 長さ１以上の文字列であること
		K__Assert(len > 0);
		size_t keylen = strlen(key);
		for (size_t i=0; i<len; i++) {
			s[i] = s[i] ^ key[i % keylen];
		}
	}

public:
	KNamedValues m_Values;
	std::unordered_map<KPath, int> m_Tags;

	CUserDataImpl() {
		KEngine::addInspectorCallback(this);
	}
	virtual void onInspectorGui() override {
		int ww = (int)ImGui::GetContentRegionAvail().x;
		int w = KMath::min(64, ww/3);
		int id = 0;
		ImGui::Indent();
		for (int i=0; i<m_Values.size(); i++) {
			const char *key = m_Values.getName(i);
			const char *val = m_Values.getString(i);
			char val_u8[256];
			strcpy_s(val_u8, sizeof(val_u8), val);
			ImGui::PushID(KImGui::KImGui_ID(id));
			ImGui::PushItemWidth((float)w);
			if (ImGui::InputText("", val_u8, sizeof(val_u8))) {
				m_Values.setString(key, KPath::fromUtf8(val_u8).u8());
			}
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::Text("%s", key);
			ImGui::PopID();
			id++;
		}
		ImGui::Unindent();
	}

	void _clear(const KPathList &keys) {
		for (auto it=keys.begin(); it!=keys.end(); ++it) {
			const KPath &k = *it;
			auto kit = m_Tags.find(k);
			if (kit != m_Tags.end()) {
				m_Tags.erase(kit);
			}
			m_Values.remove(k.u8());
		}
	}
	void clearValues() {
		m_Values.clear();
		m_Tags.clear();
	}
	void clearValuesByTag(int tag) {
		KPathList keys;
		for (auto it=m_Tags.begin(); it!=m_Tags.end(); ++it) {
			if (it->second == tag) {
				keys.push_back(it->first);
			}
		}
		_clear(keys);
	}
	void clearValuesByPrefix(const char *prefix) {
		KPathList keys;
		for (int i=0; i<m_Values.size(); i++) {
			const char *key = m_Values.getName(i);
			if (KStringUtils::startsWith(key, prefix)) {
				keys.push_back(key);
			}
		}
		_clear(keys);
	}
	int getKeys(KPathList *keys) const {
		if (keys) {
			for (int i=0; i<m_Values.size(); i++) {
				const char *key = m_Values.getName(i);
				keys->push_back(key);
			}
		}
		return m_Values.size();
	}
	bool hasKey(const KPath &key) const {
		return m_Values.getString(key.u8()) != nullptr;
	}
	KPath getString(const KPath &key, const KPath &def) const {
		return m_Values.getString(key.u8(), def.u8());
	}
	void setString(const KPath &key, const KPath &val, int tag) {
		m_Values.setString(key.u8(), val.u8());
		m_Tags[key] = tag;
	}
	bool saveToFileEx(const KNamedValues *nv, const KPath &filename, const char *password) {
		bool ret = false;
	
		KOutputStream output = KOutputStream::fromFileName(filename.u8());
		if (output.isOpen()) {
			std::string u8 = nv->saveToString();
			if (password && password[0]) {
				u8 = encryptString(u8, password);
			}
			if (!u8.empty()) {
				output.write(u8.data(), u8.size());
			}
			ret = true;
		}
		return ret;
	}
	bool saveToFile(const KPath &filename, const char *password) {
		return saveToFileEx(&m_Values, filename, password);
	}
	bool saveToFileCompress(const KPath &filename) {
		return saveToFileCompressEx(&m_Values, filename);
	}
	bool saveToFileCompressEx(const KNamedValues *nv, const KPath &filename) {
		if (nv == nullptr) return false;
		std::string u8 = nv->saveToString();
		if (u8.empty()) return false;

		std::string zbin = KZlib::compress_raw(u8, 1);
		if (zbin.empty()) return false;

		KOutputStream output = KOutputStream::fromFileName(filename.u8());
		if (output.isOpen()) {
			output.writeUint16((uint16_t)u8.size());
			output.writeUint16((uint16_t)zbin.size());
			output.write(zbin.data(), zbin.size());
			return true;
		}
		return false;
	}
	bool loadFromFile(const KPath &filename, const char *password) {
		return peekFile(filename, password, &m_Values);
	}
	bool loadFromFileCompress(const KPath &filename) {
		return loadFromFileCompressEx(&m_Values, filename);
	}
	bool loadFromFileCompressEx(KNamedValues *nv, const KPath &filename) const {
		KInputStream file = KInputStream::fromFileName(filename.u8());
		if (!file.isOpen()) return false;

		uint16_t uzsize = file.readUint16();
		uint16_t zsize  = file.readUint16();
		std::string zbin = file.readBin(zsize);
		std::string text_u8 = KZlib::uncompress_raw(zbin, uzsize);
		text_u8.push_back(0); // 終端文字を追加
		if (nv) {
			return nv->loadFromString(text_u8.c_str(), filename.u8());
		} else {
			return true;
		}
	}
	bool loadFromNamedValues(const KNamedValues *nv) {
		if (nv) {
			m_Values.clear();
			m_Values.append(nv);
			return true;
		}
		return false;
	}
	bool peekFile(const KPath &filename, const char *password, KNamedValues *nv) const {
		bool ret = false;
		KInputStream file = KInputStream::fromFileName(filename.u8());
		if (file.isOpen()) {
			std::string u8 = file.readBin();
			if (password && password[0]) {
				u8 = decryptString(u8, password);
			}
			if (nv) {
				nv->loadFromString(u8.c_str(), filename.u8());
			}
			ret = true;
		}
		return ret;
	}
};


static CUserDataImpl *g_UserData = nullptr;


void KUserData::install() {
	K__Assert(g_UserData == nullptr);
	g_UserData = new CUserDataImpl();
}
void KUserData::uninstall() {
	if (g_UserData) {
		delete g_UserData;
		g_UserData = nullptr;
	}
}
void KUserData::clearValues() {
	K__Assert(g_UserData);
	g_UserData->clearValues();
}
void KUserData::clearValuesByTag(int tag) {
	K__Assert(g_UserData);
	g_UserData->clearValuesByTag(tag);
}
void KUserData::clearValuesByPrefix(const char *prefix) {
	K__Assert(g_UserData);
	g_UserData->clearValuesByPrefix(prefix);
}
KPath KUserData::getString(const KPath &key, const KPath &def) {
	K__Assert(g_UserData);
	return g_UserData->getString(key, def);
}
void KUserData::setString(const KPath &key, const KPath &val, int tag) {
	K__Assert(g_UserData);
	g_UserData->setString(key, val, tag);
}
bool KUserData::hasKey(const KPath &key) {
	K__Assert(g_UserData);
	return g_UserData->hasKey(key);
}
int KUserData::getKeys(KPathList *keys) {
	K__Assert(g_UserData);
	return g_UserData->getKeys(keys);
}
int KUserData::getInt(const KPath &key, int def) {
	KPath s = getString(key);
	return KStringUtils::toInt(s.u8(), def);
}
void KUserData::setInt(const KPath &key, int val, int tag) {
	KPath pval = KPath::fromFormat("%d", val);
	setString(key, pval, tag);
}
bool KUserData::saveToFile(const KPath &filename, const char *password) {
	K__Assert(g_UserData);
	return g_UserData->saveToFile(filename, password);
}
bool KUserData::saveToFileCompress(const KPath &filename) {
	K__Assert(g_UserData);
	return g_UserData->saveToFileCompress(filename);
}
bool KUserData::loadFromFile(const KPath &filename, const char *password) {
	K__Assert(g_UserData);
	return g_UserData->loadFromFile(filename, password);
}
bool KUserData::loadFromFileCompress(const KPath &filename) {
	K__Assert(g_UserData);
	return g_UserData->loadFromFileCompress(filename);
}
bool KUserData::peekFile(const KPath &filename, const char *password, KNamedValues *nv) {
	K__Assert(g_UserData);
	return g_UserData->peekFile(filename, password, nv);
}


} // namespace
