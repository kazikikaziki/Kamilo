#pragma once
#include "KRef.h"

namespace Kamilo {

class KInputStream;
class KOutputStream;

/// 簡単な XML パーサ
/// ※バックエンドとして TinyXml を必要とする
/// @see KXmlElement::create
/// @see KXmlElement::createFromString
/// @see K_createXmlElementFromFile
/// @see K_createXmlElementFromFileName
class KXmlElement: public virtual KRef {
public:
	static KXmlElement * create(const std::string &tag="");

	/// XMLテキストからXMLエレメントツリーを構築し、そのルートエレメントを返す。
	/// 例えば "<node1><aaa/></node1><node2><bbb/></node2>" をパースしたとき、
	/// 帰ってくるエレメントは node1 と node2 の親になっており、タグ名も属性も持たない。
	/// ※ドキュメントに対応する
	/// KXmlElement はすべてを XMLエレメントとして扱うため、<?xml ?> などのドキュメント宣言は解析せずアクセスもできない
	/// ドキュメント宣言が必要な場合は別の方法で自力で取得する必要がある
	static KXmlElement * createFromString(const std::string &xml_u8, const std::string &filename);

	static KXmlElement * createFromStream(KInputStream &input, const std::string &filename);
	static KXmlElement * createFromFileName(const std::string &filename);

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
	virtual KXmlElement * clone() const = 0;

	#pragma region Helper
	virtual bool writeDoc(KOutputStream &output) const;
	virtual bool hasTag(const char *tag) const;
	virtual const char * findAttr(const char *name, const char *def=nullptr) const;
	virtual float findAttrFloat(const char *name, float def=0.0f) const;
	virtual int findAttrInt(const char *name, int def=0) const;
	virtual int getAttrIndex(const char *name, int start=0) const;
	virtual void setAttrInt(const char *name, int value);
	virtual void setAttrFloat(const char *name, float value);
	virtual bool deleteNode(KXmlElement *node, bool in_tree);
	virtual int getNodeIndex(const char *tag, int start=0) const;
	virtual const KXmlElement * findNode(const char *tag, const KXmlElement *start=nullptr) const;
	virtual KXmlElement * findNode(const char *tag, const KXmlElement *start=nullptr);
	const KXmlElement * _findnode_const(const char *tag, const KXmlElement *start=nullptr) const;
	#pragma endregion
};

namespace Test {
void Test_xml();
}

} // namespace
