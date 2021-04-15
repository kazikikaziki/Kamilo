#pragma once
#include "KRef.h"
#include "KStream.h"

namespace Kamilo {

/// 簡単な XML パーサ
/// ※バックエンドとして TinyXml を必要とする
/// @see KXmlElement::create
/// @see KXmlElement::createFromString
/// @see K_createXmlElementFromFile
/// @see K_createXmlElementFromFileName
class KXmlElement: public virtual KRef {
public:
	static KXmlElement * create(const char *tag=nullptr);

	/// XMLテキストからXMLエレメントツリーを構築し、そのルートエレメントを返す。
	/// 例えば "<node1><aaa/></node1><node2><bbb/></node2>" をパースしたとき、
	/// 帰ってくるエレメントは node1 と node2 の親になっており、タグ名も属性も持たない。
	/// ※ドキュメントに対応する
	/// KXmlElement はすべてを XMLエレメントとして扱うため、<?xml ?> などのドキュメント宣言は解析せずアクセスもできない
	/// ドキュメント宣言が必要な場合は別の方法で自力で取得する必要がある
	static KXmlElement * createFromString(const char *xml_u8, const char *filename);

	static KXmlElement * createFromStream(KInputStream &input, const char *filename);
	static KXmlElement * createFromFileName(const char *filename);

public:
	virtual const char *getTag() const = 0;
	virtual void setTag(const char *tag) = 0;

	virtual int getAttrCount() const = 0;
	virtual const char * getAttrName(int index) const = 0;
	virtual const char * getAttrValue(int index) const = 0;
	virtual void setAttr(const char *name, const char *value) = 0;
	virtual void delAttr(const char *name) = 0;

	virtual const char * getText(const char *def=nullptr) const = 0;
	virtual void setText(const char *text) = 0;

	virtual int getNodeCount() const = 0;
	virtual const KXmlElement * getNode(int index) const = 0;
	virtual KXmlElement * getNode(int index) = 0;
	virtual KXmlElement * addNode(const char *tag, int pos=-1) = 0; // ノードを追加する。挿入したい場合は挿入インデックスを pos に指定する. pos=-1 の場合は末尾に追加
	virtual void addNode(KXmlElement *newnode, int pos=-1) = 0; // ノードを追加する。挿入したい場合は挿入インデックスを pos に指定する. pos=-1 の場合は末尾に追加
	virtual void deleteNode(int index) = 0;
	virtual int getChildIndex(const KXmlElement *child) const = 0;

	virtual int getLineNumber() const = 0;
	virtual bool write(KOutputStream &output, int indent=0) const = 0;

	virtual bool writeDoc(KOutputStream &output) const {
		if (!output.isOpen()) return false;
		output.writeString("<?xml version=\"1.0\" encoding=\"utf8\" ?>\n");
		int num = getNodeCount();
		for (int i=0; i<num; i++) { // ルートではなくその子を書き出す
			if (!getNode(i)->write(output, 0)) {
				return false;
			}
		}
		return true;
	}

	virtual KXmlElement * clone() const = 0;

	#pragma region Helper
	virtual bool hasTag(const char *tag) const {
		const char *mytag = getTag();
		return mytag && tag && strcmp(mytag, tag)==0;
	}
	virtual const char * findAttr(const char *name, const char *def=nullptr) const {
		int i = getAttrIndex(name);
		return i>=0 ? getAttrValue(i) : def;
	}
	virtual float findAttrFloat(const char *name, float def=0.0f) const {
		const char *s = findAttr(name);
		if (s == nullptr) return def;
		char *err = 0;
		float result = strtof(s, &err);
		if (err==s || err[0]) return def;
		return result;
	}
	virtual int findAttrInt(const char *name, int def=0) const {
		const char *s = findAttr(name);
		if (s == nullptr) return def;
		char *err = 0;
		int result = strtol(s, &err, 0);
		if (err==s || *err) return def;
		return result;
	}
	virtual int getAttrIndex(const char *name, int start=0) const {
		if (name && name[0]) {
			for (int i=start; i<getAttrCount(); i++) {
				if (strcmp(getAttrName(i), name) == 0) {
					return i;
				}
			}
		}
		return -1;
	}
	virtual void setAttrInt(const char *name, int value) {
		char s[32] = {0};
		sprintf_s(s, sizeof(s), "%d", value);
		setAttr(name, s);
	}
	virtual void setAttrFloat(const char *name, float value) {
		char s[32] = {0};
		sprintf_s(s, sizeof(s), "%g", value);
		setAttr(name, s);
	}
	virtual bool deleteNode(KXmlElement *node, bool in_tree) {
		if (node == nullptr) return false;
		int n = getNodeCount();

		// 子ノードから探す
		for (int i=0; i<n; i++) {
			KXmlElement *nd = getNode(i);
			if (nd == node) {
				deleteNode(i);
				return true;
			}
		}

		// 子ツリーから探す
		if (in_tree) {
			for (int i=0; i<n; i++) {
				KXmlElement *nd = getNode(i);
				if (nd->deleteNode(node, true)) {
					return true;
				}
			}
		}
		return false;
	}
	virtual int getNodeIndex(const char *tag, int start=0) const {
		int n = getNodeCount();
		for (int i=start; i<n; i++) {
			const KXmlElement *elm = getNode(i);
			if (elm->hasTag(tag)) {
				return i;
			}
		}
		return -1;
	}
	virtual const KXmlElement * findNode(const char *tag, const KXmlElement *start=nullptr) const {
		return _findnode_const(tag, start);
	}
	virtual KXmlElement * findNode(const char *tag, const KXmlElement *start=nullptr) {
		const KXmlElement *elm = _findnode_const(tag, start);
		return const_cast<KXmlElement*>(elm);
	}
	const KXmlElement * _findnode_const(const char *tag, const KXmlElement *start=nullptr) const {
		int index = 0;
		int n = getNodeCount();
		if (start) {
			for (int i=0; i<n; i++) {
				if (getNode(i) == start) {
					index = i+1;
					break;
				}
			}
		}
		for (int i=index; i<n; i++) {
			const KXmlElement *elm = getNode(i);
			if (elm->hasTag(tag)) {
				return elm;
			}
		}
		return nullptr;
	}
	#pragma endregion
};

namespace Test {
void Test_xml();
}

} // namespace
