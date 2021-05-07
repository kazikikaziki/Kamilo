#pragma once
#include "KRef.h"
#include "KLog.h"
#include "KStream.h"
#include "KString.h"
#include "KXml.h"
#include "KMath.h"
#include "keng_ext.h"

namespace Kamilo {

class CNamedValuesImpl; // internal

/// 「名前＝値」の形式のデータを順序無しで扱う
///
/// - 順序を考慮する場合は KOrderedParameters を使う。
/// - 行と列から成る二次元的なテーブルを扱いたい場合は KTable を使う
class KNamedValues {
public:
	KNamedValues();
	int size() const;
	void clear();
	void remove(const char *name);
	bool loadFromString(const char *xml_u8, const char *filename);
	bool loadFromXml(KXmlElement *elm, bool pack_in_attr=false);
	bool loadFromFile(const char *filename);
	void saveToFile(const char *filename, bool pack_in_attr=false) const;
	void saveToXml(KXmlElement *elm, bool pack_in_attr=false) const;
	std::string saveToString(bool pack_in_attr=false) const;
	const char * getName(int index) const;
	const char * getString(int index) const;
	void setString(const char *name, const char *value);
	KNamedValues clone() const;
	void append(const KNamedValues &nv);
	int find(const char *name) const;
	bool contains(const char *name) const;
	const char * getString(const char *name, const char *defaultValue=nullptr) const;

	void setBool(const char *name, bool value);
	bool queryBool(const char *name, bool *outValue) const;
	bool getBool(const char *name, bool defaultValue=false) const;
	
	void setInteger(const char *name, int value);
	bool queryInteger(const char *name, int *outValue) const;
	int getInteger(const char *name, int defaultValue=0) const;

	void setUInt(const char *name, unsigned int value);
	bool queryUInt(const char *name, unsigned int *outValue) const;
	int getUInt(const char *name, unsigned int defaultValue=0) const;

	void setFloat(const char *name, float value);
	bool queryFloat(const char *name, float *outValue) const;
	float getFloat(const char *name, float defaultValue=0.0f) const;

	void setFloatArray(const char *name, const float *values, int count);
	int getFloatArray(const char *name, float *outValues, int maxout) const;

	void setBinary(const char *name, const void *data, int size);
	int getBinary(const char *name, void *out, int maxsize) const;

private:
	int _size() const;
	void _clear();
	void _remove(const char *name);
	const char * _getName(int index) const;
	const char * _getString(int index) const;
	void _setString(const char *name, const char *value);
	int _find(const char *name) const;
	typedef std::pair<std::string, std::string> Pair;
	std::vector<Pair> m_Items;
};


} // namespace
