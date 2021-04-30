#include "KXml.h"
#include "KString.h"
#include "KInternal.h"
#include "KStream.h"

// Use tinyxml2.h
#define K_USE_TINYXML 1


#if K_USE_TINYXML
	// TinyXML-2
	// http://leethomason.github.io/tinyxml2/
	#include "tinyxml/tinyxml2.h"
#endif // K_USE_TINYXML



namespace Kamilo {


#pragma region KXmlElement
static bool _LoadTinyXml(const std::string &xlmtext_u8, const std::string &debug_name, tinyxml2::XMLDocument *tinyxml2_doc, std::string *errmsg) {
	if (xlmtext_u8.empty()) {
		*errmsg = KStringUtils::K_sprintf(u8"E_XML: Xml document has no text: %s", debug_name.c_str());
		return false;
	}

	// TinyXML は SJIS に対応していない。
	// あらかじめUTF8に変換しておく。また、UTF8-BOM にも対応していないことに注意
	// 末尾が改行文字で終わるようにしておかないとパースエラーになる
	//
	// http://www.grinninglizard.com/tinyxmldocs/
	// For example, Japanese systems traditionally use SHIFT-JIS encoding.
	// Text encoded as SHIFT-JIS can not be read by TinyXML. 
	// A good text editor can import SHIFT-JIS and then save as UTF-8.
	tinyxml2::XMLError xerr = tinyxml2_doc->Parse(K__SkipUtf8Bom(xlmtext_u8.c_str()));
	if (xerr == tinyxml2::XML_SUCCESS) {
		*errmsg = "";
		return true;
	}

	const char *err_name = tinyxml2_doc->ErrorName();
	const char *err_msg = tinyxml2_doc->ErrorStr();
	if (tinyxml2_doc->ErrorLineNum() > 0) {
		// 行番号があるとき
		char s[1024];
		sprintf_s(s, sizeof(s),
			"E_XML: An error occurred while parsing XML document %s(%d): \n%s\n%s\n",
			debug_name.c_str(), tinyxml2_doc->ErrorLineNum(), err_name, err_msg
		);
		*errmsg = s;
		return false;

	} else {
		// 行番号がないとき（空文字列をパースしようとしたときなど）
		char s[1024];
		sprintf_s(s, sizeof(s),
			"E_XML: An error occurred while parsing XML document %s: \n%s\n%s\n",
			debug_name.c_str(), err_name, err_msg
		);
		*errmsg = s;
		return false;
	}
}

class CXNode: public KXmlElement {
	typedef std::pair<std::string, std::string> PairStrStr;
	std::string mTag;
	std::string mText;
	std::vector<PairStrStr> mAttrs;
	std::vector<KXmlElement *> mNodes;
	int mLine;
public:
	CXNode() {
		mLine = 0;
	}
	virtual ~CXNode() {
		for (auto it=mNodes.begin(); it!=mNodes.end(); ++it) {
			(*it)->drop();
		}
	}
	virtual const char * getTag() const override {
		return mTag.c_str();
	}
	virtual void setTag(const char *tag) override {
		if (!KStringUtils::isEmpty(tag)) {
			mTag = tag;
		}
	}
	virtual int getAttrCount() const override {
		return (int)mAttrs.size();
	}
	virtual const char * getAttrName(int index) const override {
		return mAttrs[index].first.c_str();
	}
	virtual const char * getAttrValue(int index) const override {
		return mAttrs[index].second.c_str();
	}
	virtual void setAttr(const char *name, const char *value) override {
		if (value == nullptr) { delAttr(name); return; }
		int index = getAttrIndex(name);
		if (index >= 0) {
			mAttrs[index].second = value; // 既存の属性を変更
		} else {
			mAttrs.push_back(PairStrStr(name, value)); // 属性を追加
		}
	}
	virtual void delAttr(const char *name) override {
		int index = getAttrIndex(name);
		if (index >= 0) {
			mAttrs.erase(mAttrs.begin() + index);
		}
	}
	virtual const char * getText(const char *def) const override {
		if (mText.empty()) {
			return def;
		} else {
			return mText.c_str();
		}
	}
	virtual void setText(const char *text) override {
		mText = text ? text : "";
	}
	virtual int getNodeCount() const override {
		return (int)mNodes.size();
	}
	virtual const KXmlElement * getNode(int index) const override {
		if (0 <= index && index < (int)mNodes.size()) {
			return mNodes[index];
		}
		return nullptr;
	}
	virtual KXmlElement * getNode(int index) override {
		if (0 <= index && index < (int)mNodes.size()) {
			return mNodes[index];
		}
		return nullptr;
	}
	virtual KXmlElement * addNode(const char *tag, int pos) override {
		if (tag && tag[0]) {
			CXNode *newnode = new CXNode();
			newnode->mTag = tag;
			addNode(newnode, pos);
			return newnode;
		}
		return nullptr;
	}
	virtual void addNode(KXmlElement *newnode, int pos) override {
		if (newnode) {
			newnode->grab();
			if (pos >= 0) {
				mNodes.insert(mNodes.begin() + pos, newnode);
			} else {
				mNodes.push_back(newnode);
			}
		}
	}
	virtual void deleteNode(int index) override {
		if (0 <= index && index < (int)mNodes.size()) {
			mNodes[index]->drop();
			mNodes.erase(mNodes.begin() + index);
		}
	}
	virtual int getChildIndex(const KXmlElement *child) const {
		for (int i=0; i<(int)mNodes.size(); i++) {
			if (mNodes[i] == child) {
				return i;
			}
		}
		return -1;
	}
	virtual int getLineNumber() const override {
		return mLine;
	}
	virtual bool write(KOutputStream &output, int indent) const override {
		if (!output.isOpen()) return false;
		if (indent < 0) indent = 0;
		char s[1024] = {0};

		// Tag
		sprintf_s(s, sizeof(s), "%*s<%s", indent*2, "", mTag.c_str());
		output.writeString(s);

		// Attr
		for (size_t i=0; i<mAttrs.size(); i++) {
			const char *k = mAttrs[i].first.c_str();
			const char *v = mAttrs[i].second.c_str();
			if (k && k[0] && v) {
				sprintf_s(s, sizeof(s), " %s=\"%s\"", k, v);
				output.writeString(s);
			}
		}

		// Text
		if (mText.size() > 0) {
			output.writeString("<![CDATA[");
			output.writeString(mText.c_str());
			output.writeString("]]>");
		}

		// Sub nodes
		if (mNodes.empty()) {
			output.writeString("/>\n");
		} else {
			if (mText.size() > 0) {
				// テキスト属性と子ノードは両立しない。
				K__Error(u8"Xml element cannot have both Text Element and Child Elements");

			} else {
				output.writeString(">\n");
				for (size_t i=0; i<mNodes.size(); i++) {
					mNodes[i]->write(output, indent+1);
				}
				sprintf_s(s, sizeof(s), "%*s</%s>\n", indent*2, "", mTag.c_str());
				output.writeString(s);
			}
		}
		return true;
	}
	virtual KXmlElement * clone() const override {
		CXNode *result = new CXNode();
		result->mTag = this->mTag;
		result->mLine = this->mLine;
		result->mAttrs = this->mAttrs;
		result->mText = this->mText;
		for (auto it=mNodes.begin(); it!=mNodes.end(); ++it) {
			KXmlElement *elm = *it;
			result->mNodes.push_back(elm->clone());
		}
		return result;
	}

	static CXNode * createFromTinyXml(const tinyxml2::XMLNode *tiNode) {
		CXNode *result = new CXNode();

		const tinyxml2::XMLDocument *tiDoc = tiNode->ToDocument();
		const tinyxml2::XMLElement *tiElm = tiNode->ToElement();

		// 位置情報
		result->mLine = tiNode->GetLineNum();

		// タグ
		if (tiElm) {
			const char *tag = tiElm->Name();
			result->mTag = tag ? tag : "";
		}

		// 属性
		if (tiElm) {
			for (const tinyxml2::XMLAttribute *attr=tiElm->FirstAttribute(); attr!=nullptr; attr=attr->Next()) {
				const char *k = attr->Name();
				const char *v = attr->Value();
				result->mAttrs.push_back(PairStrStr(k, v));
			}
		}

		// ノードテキスト
		if (tiElm) {
			const char *text = tiElm->GetText();
			if (text && text[0]) {
				result->mText = text;
			}
		}

		// 子ノード
		for (const tinyxml2::XMLElement *sub=tiNode->FirstChildElement(); sub!=nullptr; sub=sub->NextSiblingElement()) {
			CXNode *subnode = createFromTinyXml(sub);
			if (subnode) {
				result->mNodes.push_back(subnode);
			}
		}

		return result;
	}
};

KXmlElement * KXmlElement::create(const std::string &tag) {
	CXNode *xnode = new CXNode();
	xnode->setTag(tag.c_str());
	return xnode;
}
KXmlElement * KXmlElement::createFromFileName(const std::string &filename) {
	KXmlElement *elm = nullptr;
	KInputStream input = KInputStream::fromFileName(filename);
	elm = createFromStream(input, filename);
	return elm;
}
KXmlElement * KXmlElement::createFromStream(KInputStream &input, const std::string &filename) {
	if (!input.isOpen()) {
		K__Error("file is nullptr: %s", filename.c_str());
		return nullptr;
	}
	std::string bin = input.readBin();
	if (bin.empty()) {
		K__Error("file is empty: %s", filename.c_str());
		return nullptr;
	}
	return createFromString(bin.data(), filename);
}

KXmlElement * KXmlElement::createFromString(const std::string &xmlTextU8, const std::string &filename) {
	tinyxml2::XMLDocument tiDoc;
	std::string tiErrMsg;
	if (_LoadTinyXml(xmlTextU8, filename, &tiDoc, &tiErrMsg)) {
		return CXNode::createFromTinyXml(&tiDoc);
	}
	K__Error("Failed to read xml: %s: %s", filename, tiErrMsg.c_str());
	return nullptr;
}

bool KXmlElement::writeDoc(KOutputStream &output) const {
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
bool KXmlElement::hasTag(const char *tag) const {
	const char *mytag = getTag();
	return mytag && tag && strcmp(mytag, tag)==0;
}
const char * KXmlElement::findAttr(const char *name, const char *def) const {
	int i = getAttrIndex(name);
	return i>=0 ? getAttrValue(i) : def;
}
float KXmlElement::findAttrFloat(const char *name, float def) const {
	const char *s = findAttr(name);
	if (s == nullptr) return def;
	char *err = 0;
	float result = strtof(s, &err);
	if (err==s || err[0]) return def;
	return result;
}
int KXmlElement::findAttrInt(const char *name, int def) const {
	const char *s = findAttr(name);
	if (s == nullptr) return def;
	char *err = 0;
	int result = strtol(s, &err, 0);
	if (err==s || *err) return def;
	return result;
}
int KXmlElement::getAttrIndex(const char *name, int start) const {
	if (name && name[0]) {
		for (int i=start; i<getAttrCount(); i++) {
			if (strcmp(getAttrName(i), name) == 0) {
				return i;
			}
		}
	}
	return -1;
}
void KXmlElement::setAttrInt(const char *name, int value) {
	char s[32] = {0};
	sprintf_s(s, sizeof(s), "%d", value);
	setAttr(name, s);
}
void KXmlElement::setAttrFloat(const char *name, float value) {
	char s[32] = {0};
	sprintf_s(s, sizeof(s), "%g", value);
	setAttr(name, s);
}
bool KXmlElement::deleteNode(KXmlElement *node, bool in_tree) {
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
int KXmlElement::getNodeIndex(const char *tag, int start) const {
	int n = getNodeCount();
	for (int i=start; i<n; i++) {
		const KXmlElement *elm = getNode(i);
		if (elm->hasTag(tag)) {
			return i;
		}
	}
	return -1;
}
const KXmlElement * KXmlElement::findNode(const char *tag, const KXmlElement *start) const {
	return _findnode_const(tag, start);
}
KXmlElement * KXmlElement::findNode(const char *tag, const KXmlElement *start) {
	const KXmlElement *elm = _findnode_const(tag, start);
	return const_cast<KXmlElement*>(elm);
}
const KXmlElement * KXmlElement::_findnode_const(const char *tag, const KXmlElement *start) const {
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






namespace Test {
void Test_xml() {
	KXmlElement *elm = KXmlElement::createFromString(
		"<node1 pi='314'>"
		"	<aaa/>"
		"	<bbb/>"
		"	<ccc/>"
		"</node1>"
		"<node2>"
		"	<ddd>Hello world!</ddd>"
		"</node2>"
		, ""
	);
	K__Verify(elm);
	K__Verify(elm->hasTag(""));
	K__Verify(elm->getNodeCount() == 2);

	KXmlElement *node1 = elm->getNode(0);
	K__Verify(node1);
	K__Verify(node1 == elm->findNode("node1"));
	K__Verify(node1->hasTag("node1"));
	K__Verify(node1->getNodeCount() == 3); // aaa, bbb, ccc
	K__Verify(node1->getAttrCount() == 1);
	K__Verify(strcmp(node1->getAttrName(0), "pi")==0);
	K__Verify(strcmp(node1->getAttrValue(0), "314")==0);
	K__Verify(node1->findAttrInt("pi") == 314);

	KXmlElement *node2 = elm->getNode(1);
	K__Verify(node2);
	K__Verify(node2 == elm->findNode("node2"));
	K__Verify(node2->hasTag("node2"));
	K__Verify(strcmp(node2->findNode("ddd")->getText(""), "Hello world!") == 0);

	elm->drop();
}
} // Test
#pragma endregion // KXmlElement



} // namespace
