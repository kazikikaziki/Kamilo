#include "keng_gameext.h"
//
#include <algorithm>
#include <dsound.h>
#include <mmsystem.h> // WAVEFORMATEX
#include <process.h> // _beginthreadex
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "KInternal.h"
#include "KKeyboard.h"
#include "KLocalTIme.h"
#include "KSig.h"
#include "KSolidBody.h"
#include "KSound.h"
#include "KStorage.h"
#include "KWindow.h"
#include "KZlib.h"

namespace Kamilo {

static const int SCENE_ID_LIMIT = 100;

#define TRAC(obj)  KLog::printInfo("%s: '%s'", __FUNCTION__, obj ? typeid(*obj).name() : "(nullptr)");


const KNamedValues * KScene::getParams() const {
	return &m_params;
}
KNamedValues * KScene::getParamsEditable() {
	return &m_params;
}
void KScene::setParams(const KNamedValues *new_params) {
	KNamedValues old_params = m_params;
	m_params.clear();
	m_params.append(new_params);
	onSetParams(&m_params, new_params, &old_params);
}







struct SSceneDef {
	KScene *scene;
	KNamedValues params;
	KSCENEID id;

	SSceneDef() {
		scene = nullptr;
		id = -1;
		params.clear();
	}
	void clear() {
		scene = nullptr;
		id = -1;
		params.clear();
	}
};

class CSceneMgr: public KManager, public KInspectorCallback {
	std::vector<KScene *> m_scenelist;
	SSceneDef m_curr_scene;
	SSceneDef m_next_scene;
	int m_clock;
	KGameSceneSystemCallback *m_cb;
public:
	CSceneMgr() {
		m_curr_scene.clear();
		m_next_scene.clear();
		m_cb = nullptr;
		m_clock = 0;
		KEngine::addManager(this);
		KEngine::addInspectorCallback(this, u8"シーン管理");
	}
	virtual ~CSceneMgr() {
		on_manager_end();
	}
	void setCallback(KGameSceneSystemCallback *cb) {
		m_cb = cb;
	}
	virtual void on_manager_end() override {
		if (m_curr_scene.scene) {
			call_scene_exit();
		}
		for (auto it=m_scenelist.begin(); it!=m_scenelist.end(); ++it) {
			if (*it) delete *it;
		}
		m_scenelist.clear();
		m_curr_scene.clear();
		m_next_scene.clear();
	}
	virtual void on_manager_appframe() override {
		// シーン切り替えが指定されていればそれを処理する
		// （これはポーズ中でも関係なく行う）
		process_switching();
	}
	virtual void onInspectorGui() override {
		const char *nullname = "(nullptr)";
		const char *s = m_curr_scene.scene ? typeid(*m_curr_scene.scene).name() : nullname;
		ImGui::Text("Clock: %d", m_clock);
		ImGui::Text("Current scene: %s", s);
		if (ImGui::TreeNode("Current scene params")) {
			if (m_curr_scene.scene) {
				const KNamedValues *nv = m_curr_scene.scene->getParams();
				for (int i=0; i<nv->size(); i++) {
					const char *key = nv->getName(i);
					const char *val = nv->getString(i);
					ImGui::Text("%s: %s", key, val);
				}
			}
			ImGui::TreePop();
		}
		for (size_t i=0; i<m_scenelist.size(); i++) {
			KScene *scene = m_scenelist[i];
			const char *name = scene ? typeid(*scene).name() : nullname;
			ImGui::PushID(KImGui::KImGui_ID(i));

			char s[256];
			sprintf_s(s, sizeof(s), "%d", i);
			if (ImGui::Button(s)) {
				setNextScene(i, nullptr);
			}
			ImGui::SameLine();
			ImGui::Text("%s", name);
			ImGui::PopID();
		}
		ImGui::Separator();
		if (m_curr_scene.scene) {
			m_curr_scene.scene->onSceneInspectorGui();
		}
	}
	int getClock() const {
		return m_clock;
	}
	KScene * getScene(KSCENEID id) const {
		if (id < 0) return nullptr;
		if (id >= (int)m_scenelist.size()) return nullptr;
		return m_scenelist[id];
	}
	KScene * getCurrentScene() const {
		return m_curr_scene.scene;
	}
	KSCENEID getCurrentSceneId() const {
		for (size_t i=0; i<m_scenelist.size(); i++) {
			if (m_scenelist[i] == m_curr_scene.scene) {
				return i;
			}
		}
		return -1;
	}
	void addScene(KSCENEID id, KScene *scene) {
		K_assert(id >= 0);
		K_assert(id < SCENE_ID_LIMIT);
		if (getScene(id)) {
			KLog::printWarning("W_SCENE_OVERWRITE: KScene with the same id '%d' already exists. The scene will be overwritten by new one.", id);
			if (m_curr_scene.scene == getScene(id)) {
				KLog::printWarning("W_DESTROY_RUNNING_SCENE: Running scene with id '%d' will immediatly destroy.", id);
			}
			if (m_scenelist[id]) delete m_scenelist[id];
		}
		if ((int)m_scenelist.size() <= id) {
			m_scenelist.resize(id + 1);
		}
		m_scenelist[id] = scene;
	}
	void call_scene_enter() {
		TRAC(m_curr_scene.scene);
		if (m_curr_scene.scene) {
			m_curr_scene.scene->setParams(&m_curr_scene.params);

			// onSceneEnter でさらにシーンが変更される可能性に注意
			m_curr_scene.scene->onSceneEnter();
		}
	}
	void call_scene_exit() {
		TRAC(m_curr_scene.scene);
		if (m_curr_scene.scene) {
			m_curr_scene.scene->onSceneExit();
		}
	}
	void setNextScene(KSCENEID id, const KNamedValues *params) {
		KScene *scene = getScene(id);
		if (m_next_scene.scene) {
			KLog::printWarning("W_SCENE_OVERWRITE: Queued KScene '%s' will be overwritten by new posted scene '%s'",
				typeid(*m_next_scene.scene).name(),
				(scene ? typeid(*scene).name() : "(nullptr)")
			);
			m_next_scene.scene = nullptr;
		}
		if (scene) {
			KLog::printInfo("KSceneManager::setNextScene: %s", typeid(*scene).name());
		}
		m_next_scene.scene = scene;
		m_next_scene.id = id;
		m_next_scene.params.clear();
		m_next_scene.params.append(params);
	}
	void restart() {
		KLog::printInfo("Restart!");
		setNextScene(m_curr_scene.id, &m_curr_scene.params);
	}
	void process_switching() {
		// シーン切り替えが指定されていればそれを処理する
		if (m_next_scene.scene == nullptr) {
			if (m_curr_scene.scene) {
				KSCENEID id = m_curr_scene.scene->onQueryNextScene();
				setNextScene(id, nullptr);
			}
		}
		if (m_next_scene.scene) {
			if (m_cb) {
				// シーン切り替え通知
				// ここで next.params が書き換えられる可能性に注意
				KSceneManagerSignalArgs args;
				args.curr_scene = m_curr_scene.scene;
				args.curr_id = m_curr_scene.id;
				args.next_id = m_next_scene.id;
				args.next_params = &m_next_scene.params;
				m_cb->on_scenemgr_scene_changing(&args);
			}
			if (m_curr_scene.scene) {
				call_scene_exit();
				m_curr_scene.scene = nullptr;
				m_curr_scene.id = -1;
			}
			m_clock = 0;
			m_curr_scene = m_next_scene;
			m_next_scene.clear();
			call_scene_enter();

		} else {
			m_clock++;
		}
	}
};

#pragma region KSceneManager
static CSceneMgr * g_SceneMgr = nullptr;

void KSceneManager::install() {
	K__Assert(g_SceneMgr == nullptr);
	g_SceneMgr = new CSceneMgr();
}
void KSceneManager::uninstall() {
	if (g_SceneMgr) {
		g_SceneMgr->drop();
		g_SceneMgr = nullptr;
	}
}
bool KSceneManager::isInstalled() {
	return g_SceneMgr != nullptr;
}
void KSceneManager::setCallback(KGameSceneSystemCallback *cb) {
	K__Assert(g_SceneMgr);
	g_SceneMgr->setCallback(cb);
}
void KSceneManager::restart() {
	K__Assert(g_SceneMgr);
	g_SceneMgr->restart();
}
int KSceneManager::getClock() {
	K__Assert(g_SceneMgr);
	return g_SceneMgr->getClock();
}
KScene * KSceneManager::getScene(KSCENEID id) {
	K__Assert(g_SceneMgr);
	return g_SceneMgr->getScene(id);
}
KScene * KSceneManager::getCurrentScene() {
	K__Assert(g_SceneMgr);
	return g_SceneMgr->getCurrentScene();
}
KSCENEID KSceneManager::getCurrentSceneId() {
	K__Assert(g_SceneMgr);
	return g_SceneMgr->getCurrentSceneId();
}
void KSceneManager::addScene(KSCENEID id, KScene *scene) {
	K__Assert(g_SceneMgr);
	g_SceneMgr->addScene(id, scene);
}
/// シーンを切り替える
/// @param id      次のシーンの識別子（addScene で登録したもの）
/// @param params  次のシーンに渡すパラメータ。ここで渡したパラメータは KScene::getParams で取得できる
void KSceneManager::setNextScene(KSCENEID id, const KNamedValues *params) {
	K__Assert(g_SceneMgr);
	g_SceneMgr->setNextScene(id, params);
}
void KSceneManager::setSceneParamInt(const char *key, int val) {
	KScene *scene = getCurrentScene();
	if (scene) {
		KNamedValues *nv = scene->getParamsEditable();
		nv->setInteger(key, val);
	}
}
int KSceneManager::getSceneParamInt(const char *key) {
	KScene *scene = getCurrentScene();
	if (scene) {
		return scene->getParams()->getInteger(key);
	}
	return 0;
}
#pragma endregion // KSceneManager
















static void _XorString(char *s, size_t len, const char *key) {
	K_assert(s);
	K_assert(key && key[0]); // 長さ１以上の文字列であること
	K_assert(len > 0);
	size_t keylen = strlen(key);
	for (size_t i=0; i<len; i++) {
		s[i] = s[i] ^ key[i % keylen];
	}
}

void KDataAct::attach(KNode *node) {
	node->setAction(new KDataAct());
}
KDataAct * KDataAct::of(KNode *node) {
	if (node) {
		return node->getActionT<KDataAct*>();
	}
	return nullptr;
}
KDataAct::KDataAct() {
}
void KDataAct::_clear(const KPathList &keys) {
	for (auto it=keys.begin(); it!=keys.end(); ++it) {
		const KPath &k = *it;
		auto kit = m_tags.find(k);
		if (kit != m_tags.end()) {
			m_tags.erase(kit);
		}
		m_values.remove(k.u8());
	}
}
void KDataAct::clearValues() {
	m_values.clear();
	m_tags.clear();
}
void KDataAct::clearValuesByTag(int tag) {
	KPathList keys;
	for (auto it=m_tags.begin(); it!=m_tags.end(); ++it) {
		if (it->second == tag) {
			keys.push_back(it->first);
		}
	}
	_clear(keys);
}
void KDataAct::clearValuesByPrefix(const char *prefix) {
	KPathList keys;
	for (int i=0; i<m_values.size(); i++) {
		const char *key = m_values.getName(i);
		if (KStringUtils::startsWith(key, prefix)) {
			keys.push_back(key);
		}
	}
	_clear(keys);
}
int KDataAct::getKeys(KPathList *keys) const {
	if (keys) {
		for (int i=0; i<m_values.size(); i++) {
			const char *key = m_values.getName(i);
			keys->push_back(key);
		}
	}
	return m_values.size();
}
bool KDataAct::hasKey(const KPath &key) const {
	return m_values.getString(key.u8()) != nullptr;
}
KPath KDataAct::getStr(const KPath &key, const KPath &def) const {
	return m_values.getString(key.u8(), def.u8());
}
void KDataAct::setStr(const KPath &key, const KPath &val, int tag) {
	m_values.setString(key.u8(), val.u8());
	m_tags[key] = tag;
}
bool KDataAct::saveToFileEx(const KNamedValues *nv, const KPath &filename, const char *password) {
	bool ret = false;
	
	KOutputStream output = KOutputStream::fromFileName(filename.u8());
	if (output.isOpen()) {
		std::string u8 = nv->saveToString();
		if (password && password[0]) {
			_XorString(&u8[0], u8.size(), password); // encode
		}
		if (!u8.empty()) {
			output.write(u8.data(), u8.size());
		}
		ret = true;
	}
	return ret;
}
bool KDataAct::saveToFile(const KPath &filename, const char *password) {
	return saveToFileEx(&m_values, filename, password);
}
bool KDataAct::loadFromFile(const KPath &filename, const char *password) {
	return peekFile(filename, password, &m_values);
}
bool KDataAct::loadFromFileCompress(const KPath &filename) {
	return loadFromFileCompressEx(&m_values, filename);
}
bool KDataAct::saveToFileCompress(const KPath &filename) {
	return saveToFileCompressEx(&m_values, filename);
}
bool KDataAct::saveToFileCompressEx(const KNamedValues *nv, const KPath &filename) {
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
bool KDataAct::loadFromFileCompressEx(KNamedValues *nv, const KPath &filename) const {
	KReader *file = KReader::createFromFileName(filename.u8());
	if (file == nullptr) return false;

	uint16_t uzsize = file->read_uint16();
	uint16_t zsize  = file->read_uint16();
	std::string zbin = file->read_bin(zsize);
	file->drop();

	std::string text_u8 = KZlib::uncompress_raw(zbin, uzsize);
	text_u8.push_back(0); // 終端文字を追加
	if (nv) {
		return nv->loadFromString(text_u8.c_str(), filename.u8());
	} else {
		return true;
	}
}
bool KDataAct::loadFromNamedValues(const KNamedValues *nv) {
	if (nv) {
		m_values.clear();
		m_values.append(nv);
		return true;
	}
	return false;
}
bool KDataAct::peekFile(const KPath &filename, const char *password, KNamedValues *nv) const {
	bool ret = false;
	KReader *file = KReader::createFromFileName(filename.u8());
	if (file) {
		std::string u8 = file->read_bin();
		if (password && password[0]) {
			_XorString(&u8[0], u8.size(), password); // decode
		}
		if (nv) {
			nv->loadFromString(u8.c_str(), filename.u8());
		}
		file->drop();
		ret = true;
	}
	return ret;
}
void KDataAct::onInspector() {
	int ww = (int)ImGui::GetContentRegionAvail().x;
	int w = KMath::min(64, ww/3);
	int id = 0;
	ImGui::Indent();
	for (int i=0; i<m_values.size(); i++) {
		const char *key = m_values.getName(i);
		const char *val = m_values.getString(i);
		char val_u8[256];
		strcpy_s(val_u8, sizeof(val_u8), val);
		ImGui::PushID(KImGui::KImGui_ID(id));
		ImGui::PushItemWidth((float)w);
		if (ImGui::InputText("", val_u8, sizeof(val_u8))) {
			m_values.setString(key, KPath::fromUtf8(val_u8).u8());
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::Text("%s", key);
		ImGui::PopID();
		id++;
	}
	ImGui::Unindent();
}



// KShadowAct
#define SHADOW_BASIC_RADIUS  64

static void kk_MakeCircleMesh(KMesh *mesh, const KVec2 &radius, int numvertices, const KColor &center_color, const KColor &outer_color) {
	K_assert(mesh);
	K_assert(numvertices >= 3);
	mesh->clear();
	mesh->setVertexCount(1+numvertices);
	if (1) {
		// 中心
		{
			mesh->setPosition(0, KVec3());
			mesh->setTexCoord(0, KVec2());
			mesh->setColor(0, center_color);
		}
		// 円周
		for (int i=0; i<numvertices; i++) {
			float t = KMath::PI * 2 * i / (numvertices-1);
			mesh->setPosition(1+i, KVec3(radius.x * cosf(t), radius.y * sinf(t), 0.0f));
			mesh->setTexCoord(1+i, KVec2());
			mesh->setColor(1+i, outer_color);
		}
	}
	{
		KSubMesh sub;
		sub.start = 0;
		sub.count = numvertices+1;
		sub.primitive = KPrimitive_TRIANGLE_FAN;
		mesh->addSubMesh(sub);
	}
}

static KShadowAct::GlobalSettings g_GlobalShadowSettings;

KShadowAct::GlobalSettings * KShadowAct::globalSettings() {
	return &g_GlobalShadowSettings;
}

void KShadowAct::attach(KNode *node) {
	node->setAction(new KShadowAct());
}
KShadowAct * KShadowAct::of(KNode *node) {
	if (node) {
		return node->getActionT<KShadowAct*>();
	}
	return nullptr;
}
KShadowAct::KShadowAct() {
	m_item.radius_x = g_GlobalShadowSettings.defaultRadiusX;
	m_item.radius_y = g_GlobalShadowSettings.defaultRadiusY;
	m_item.scale = 1.0f;
	m_item.scale_by_height = true;
	m_item.max_height = 0;
	m_item.use_sprite = false;
	m_item.enabled = true;
	m_item.delay = 2; // 最初のフレームでは、まだ影の描画に必要な情報が取得できていないので、念のために数フレーム経過してから表示するようにする
}
void KShadowAct::onEnterAction() {
	KNode *self = getSelf();

	self->setTransformInherit(false);
	self->setSpecularInherit(false); // スペキュラは継承しない
	self->setScale(KVec3()); // まだ影が安定していないので、生成したフレームでは描画されないようにする
	self->setPriority(g_GlobalShadowSettings.worldLayer);

	KColor center_color = g_GlobalShadowSettings.color;
	KColor outer_color = g_GlobalShadowSettings.color;
	if (g_GlobalShadowSettings.gradient) {
		outer_color.a = 0;
	}

	KMeshDrawable::attach(self);
	KMesh *mesh = KMeshDrawable::of(self)->getMesh();
	kk_MakeCircleMesh(mesh, KVec2(SHADOW_BASIC_RADIUS, SHADOW_BASIC_RADIUS), g_GlobalShadowSettings.vertexCount, center_color, outer_color);
	KMeshDrawable::of(self)->getSubMesh(0)->material.blend = g_GlobalShadowSettings.blend;
	m_item.shadow_tex_name = KPath::fromFormat("__shadowtex_%x", self->getId());
}
void KShadowAct::onExitAction() {
	// 影専用のテクスチャを消す
	KBank::getTextureBank()->removeTexture(m_item.shadow_tex_name);
}
void KShadowAct::onStepSystemAction() {
	update();
}
void KShadowAct::onStepAction() {
//	update();
}
void KShadowAct::update() {
	const KVec3 identity_scale(1, 1, 1);
	const KVec3 zero_scale(0, 0, 0);

	KNode *self = getSelf();

	// スプライト影が設定されている場合、親キャラクターの描画を
	// グループ化しておく必要がある。
	// （グループ化するとキャラクタの描画が一旦レンダーテクスチャに対して行われるので、
	// このレンダーテクスチャを利用して影を描画する）
	if (m_item.use_sprite) {
		KDrawable *owner_renderer = KDrawable::of(self->getParent());
		if (owner_renderer) owner_renderer->setGrouping(true);
	}

	// 遅延タイマーを更新
	if (m_item.delay > 0) {
		m_item.delay--;
	}

	{
		ITEM &item = m_item;
		KNode *owner_node = self->getParent();
		KNode *shadow_node = self;
		if (shadow_node == nullptr) return; // 何らかの理由で影ノードが外部から削除されている（例えばノードの子を一括削除してしまった場合など）

		item.altitude = -1;

		if (!item.enabled) {
			// 影無効
			// visible = false とやりたいところだが、影の表示・非表示が外部から直接指定されている可能性もあるので、
			// エンティティの visible や enabled はなるべく弄りたくない。サイズをゼロにしておく
			shadow_node->setScale(zero_scale);
			return;
		}

		// 影の位置と大きさを計算
		KVec3 shadow_pos, shadow_scale;
		if (!compute_shadow_transform(item, &shadow_pos, &shadow_scale, &item.altitude)) {
			// 影はどこにも映らない。
			// visible = false とやりたいところだが、影の表示・非表示が外部から直接指定されている可能性もあるので、
			// エンティティの visible や enabled はなるべく弄りたくない。サイズをゼロにしておく
			shadow_node->setScale(zero_scale);
			return;
		}

		if (item.delay > 0) {
			// まだ生成されて間もない。
			// スプライト影を描画するための情報が足りない（スプライト影は、全フレームでのキャラクタ描画結果を使う）
			// とりあえずシンプル影を表示しておく
			// ※スプライト影を表示するまで、メッシュは丸型になっている。
			// これは一度でもスプライト影を表示すると上書きされる
			shadow_node->setPosition(shadow_pos);
			shadow_node->setScale(shadow_scale);
			return;
		}

		if (item.use_sprite) {
			KDrawable *owner_renderer = KDrawable::of(owner_node);
			KDrawable *shadow_renderer = KDrawable::of(shadow_node);

			shadow_node->setPosition(shadow_pos);
			shadow_node->setScale(identity_scale);

			// グループ化された絵が描画されているテクスチャを得る
			// スプライト影を描画するには、キャラクターの描画モードをグループ化しておく必要がある
			owner_renderer->setGrouping(true);
			KTexture *owner_sprite_tex = KVideo::findTexture(owner_renderer->getGroupRenderTexture());
			K_assert(owner_sprite_tex);

			KVec3 pivot;
			int tex_w=0, tex_h=0;
			owner_renderer->getGroupingImageSize(&tex_w, &tex_h, &pivot);

			// ここで取得した owner_sprite_tex をそのまま影のテクスチャとして使ってはいけない。
			// キャラクターの絵が大きく変化する時、グループ化テクスチャは自動的にリサイズされる場合がある。
			// --> KDrawable::updateGroupRenderTextureAndMesh()
			// リサイズされた場合、ここで得た owner_sprite_tex は無効になってしまい、真っ白なテクスチャが描画されることになるのだが、
			// このリサイズのタイミングはいつになるかわからない。ここの処理を抜けて、実際に影が描画されるまでの間に発生するかもしれない。
			// その場合、今は正しくてもこのフレームの最後で影を描画しようとしたときは既に無効化されている場合もある。
			// そのため、必ず自前のテクスチャを用意して、キャラクターのスプライトをコピーしてから使う

			// tex_w, tex_h は変化する可能性があるので、texid を一番最初に取得して使いまわすという事はできない
			KTexture *shadow_tex = KVideo::findTexture(KBank::getTextureBank()->addRenderTexture(item.shadow_tex_name, tex_w, tex_h, KTextureBank::F_OVERWRITE_SIZE_NOT_MATCHED));
			KVideoUtils::blit(shadow_tex, owner_sprite_tex, nullptr);

			KMeshDrawable *meshRenderer = KMeshDrawable::of(shadow_node);
			if (meshRenderer) {
				KMesh *shadowMesh = meshRenderer->getMesh();
				MeshShape::makeRect(shadowMesh, KVec2(0, 0), KVec2(tex_w, tex_w), KVec2(0, 0), KVec2(1, 1), KColor::WHITE);

				KSubMesh *subMesh = meshRenderer->getSubMesh(0);
				if (subMesh) {
					subMesh->material.texture = shadow_tex ? shadow_tex->getId() : nullptr;
					subMesh->material.blend = KBlend_ALPHA;
					subMesh->material.color = KColor(0.0f, 0.0f, 0.0f, 0.4f);
				}
			}

		//	shadow_renderer->setRenderOffset(-pivot);
			shadow_renderer->setRenderOffset(KVec3(-pivot.x, -32.0f, 0.0f));

			// スケーリングを持ち主に合わせつつ、上下につぶす
			KVec3 owner_scale = owner_node->getScale();

			shadow_scale.x = owner_scale.x;
			shadow_scale.y = owner_scale.y * 0.2f;
			shadow_scale.z = owner_scale.z;

			// 奥に倒す
			KQuat shadow_rot = KQuat::fromXDeg(90);
			shadow_node->setScale(shadow_scale);
			shadow_node->setRotation(shadow_rot);

		} else {
			// シンプル影
			shadow_node->setPosition(shadow_pos);
			shadow_node->setScale(shadow_scale);
		}
	}
}
void KShadowAct::onInspector() {
	if (ImGui::CollapsingHeader("Global Settings")) {
		inspector_global();
	}

	bool mod = false;

	if (m_item.altitude >= 0) {
		ImGui::Text("Altitude: %d", m_item.altitude);
	} else {
		ImGui::Text("Altitude: (BOTTOMLESS)");
	}
	mod |= ImGui::DragFloat("RadiusX", &m_item.radius_x);
	mod |= ImGui::DragFloat("RadiusY", &m_item.radius_y);
	mod |= ImGui::DragFloat("Scale",   &m_item.scale);
	mod |= ImGui::DragFloat3("Offset", (float*)&m_item.offset);
	mod |= ImGui::Checkbox("Scale by height(True)", &m_item.scale_by_height);
	if (ImGui::DragFloat("MaxHeight", &m_item.max_height)) {
		m_item.max_height = KMath::max(m_item.max_height, 0.0f);
		mod = true;
	}
	if (mod) {
		update();
	}
}
void KShadowAct::inspector_global() {
#ifndef NO_IMGUI
	bool changed = false;
	if (ImGui::ColorEdit4("Color", reinterpret_cast<float*>(&g_GlobalShadowSettings.color))) {
		changed = true;
	}
	if (KDebugGui::K_DebugGui_InputBlend("Blend", &g_GlobalShadowSettings.blend)) {
		changed = true;
	}
	if (ImGui::DragInt("Vertices", &g_GlobalShadowSettings.vertexCount)) {
		g_GlobalShadowSettings.vertexCount = KMath::max(3, g_GlobalShadowSettings.vertexCount);
		changed = true;
	}
	if (ImGui::DragFloat("Max Height", &g_GlobalShadowSettings.maxAltitude)) {
		changed = true;
	}
	if (ImGui::Checkbox("Scale by height", &g_GlobalShadowSettings.scaleByAltitude)) {
		changed = true;
	}
#endif // !NO_IMGUI
}

void KShadowAct::setOffset(const KVec3 &value) {
	m_item.offset = value;
}
void KShadowAct::setRadius(float horz, float vert) {
	m_item.radius_x = horz;
	m_item.radius_y = vert;
}
void KShadowAct::getRadius(float *horz, float *vert) const {
	if (horz) *horz = m_item.radius_x;
	if (vert) *vert = m_item.radius_y;
}
void KShadowAct::setScaleFactor(float value) {
	m_item.scale = value;
}
void KShadowAct::setScaleByHeight(bool value, float maxheight) {
	m_item.scale_by_height = value;
	m_item.max_height = maxheight;
}
void KShadowAct::setUseSprite(bool value) {
	m_item.use_sprite = value;
}
void KShadowAct::setMatrix(const KMatrix4 &matrix) {
	KNode *self = getSelf();
	KDrawable *renderer = KDrawable::of(self);
	if (renderer) {
		renderer->setLocalTransform(matrix);
	}
}
bool KShadowAct::getAltitude(float *alt) {
	KNode *self = getSelf();
	KNode *owner = self->getParent();
	KVec3 wpos = owner->getWorldPosition();

	// Ｙ座標がそのまま高度を表す？
	if (g_GlobalShadowSettings.useYPositionAsAltitude) {
		if (alt) *alt = wpos.y;
		return true;
	}

	// Body を持つなら、Body の高度情報を利用する
	// ただし、enabled=false の場合高度情報が更新されないので注意
	KSolidBody *body = KSolidBody::of(owner);
	if (body && body->getBodyEnabled()) {
		return KSolidBody::getDynamicBodyAltitude(owner, alt);
	}

	// Body が利用できないなら、コリジョンワールドに高度を問い合わせる
	{

		float ground_y;
		float max_penetration = 16;
		if (KSolidBody::getGroundPoint(wpos, max_penetration, &ground_y)) {
			if (alt) *alt = wpos.y - ground_y;
			return true;
		}
	}

	// 地面がない。奈落。
	return false;
}
bool KShadowAct::compute_shadow_transform(ITEM &item, KVec3 *out_pos, KVec3 *out_scale, float *out_alt) {
	K_assert(out_pos);
	KNode *self = getSelf();
	KNode *owner = self->getParent();
	if (owner == nullptr) return false;
	if (item.scale <= 0) return false;
	if (item.radius_x == 0) return false;
	if (item.radius_y == 0) return false;

	// 高度を得る
	float alt;
	if (getAltitude(&alt)) {
		// 影サイズを設定
		{
			// 標準影サイズから各エンティティの標準サイズへのスケーリング
			float lscale_x = item.scale * item.radius_x / SHADOW_BASIC_RADIUS;
			float lscale_y = item.scale * item.radius_y / SHADOW_BASIC_RADIUS;
			// 高度による自動スケーリング値
			float hscale = 1.0f;
			if (g_GlobalShadowSettings.scaleByAltitude && item.scale_by_height) {

				// 最大高度。
				// ノードに設定されている値を使う。それが 0 なら規定値を使う
				float maxheight = item.max_height;
				if (maxheight <= 0) {
					maxheight = g_GlobalShadowSettings.maxAltitude;
				}

				float k = KMath::linearStep(maxheight, 0.0f, alt);
				hscale = KMath::lerp(0.0f, 1.0f, k);
			}
			// 設定
			if (out_scale) *out_scale = KVec3(lscale_x*hscale, lscale_y*hscale, 1.0f);
		}

		// キャラクターオブジェクトのAABB底面
		KSolidBody *body = KSolidBody::of(owner);
		float owner_body_bottom = 0;
		if (body && body->getBodyEnabled()) {
			KVec3 _min;
			body->getAabbLocal(&_min, nullptr);
			owner_body_bottom = _min.y;
		}

		// 影の位置
		KVec3 owner_pos = owner->getWorldPosition();
		KVec3 pos = owner_pos + item.offset;
		pos.y += owner_body_bottom - alt; // alt はAABB底面から地面までの距離であることに注意
		pos.y += 1; // 地面と完全に重なると描画順位が怪しくなるので、少しだけ浮かせる
		pos.z += 1; // 親オブジェクトと完全に座標が一致した場合でも奥側に描画されるよう、少しだけ座標をずらす

		if (out_pos) *out_pos = pos;
		if (out_alt) *out_alt = alt;
		return true;
	}
	return false;
}

























#pragma region KGizmoAct
void KGizmoAct::attach(KNode *node) {
	node->setAction(new KGizmoAct());
}
KGizmoAct * KGizmoAct::of(KNode *node) {
	if (node) {
		return node->getActionT<KGizmoAct*>();
	}
	return nullptr;
}
KGizmoAct * KGizmoAct::cast(KAction *act) {
	return dynamic_cast<KGizmoAct*>(act);
}


KGizmoAct::KGizmoAct() {
}
void KGizmoAct::onEnterAction() {
	KNode *self = getSelf();
	KMeshDrawable::attach(self);
}
void KGizmoAct::clear() {
	KNode *self = getSelf();
	KMeshDrawable *co = KMeshDrawable::of(self);
	KMesh *mesh = co ? co->getMesh() : nullptr;
	if (mesh) {
		mesh->clear();
	}
}
void KGizmoAct::addLine(const KVec3 &a, const KVec3 &b) {
	addLine(a, b, KColor32::WHITE, KColor32::WHITE);
}
void KGizmoAct::addLine(const KVec3 &a, const KVec3 &b, const KColor32 &color) {
	addLine(a, b, color, color);
}
void KGizmoAct::addLine(const KVec3 &a, const KVec3 &b, const KColor32 &color_a, const KColor32 &color_b) {
	KNode *self = getSelf();
	KMeshDrawable *co = KMeshDrawable::of(self);
	KMesh *mesh = co ? co->getMesh() : nullptr;
	if (mesh) {
		m_MeshBuf.resize2(mesh, 2, 0);
		m_MeshBuf.setPosition(0, a);
		m_MeshBuf.setPosition(1, b);
		m_MeshBuf.setColor32(0, color_a);
		m_MeshBuf.setColor32(1, color_b);
		m_MeshBuf.addToMesh(mesh, KPrimitive_LINES);
	}
}
void KGizmoAct::addRegularPolygon(const KVec3 &pos, float radius, int count, float start_degrees, const KColor32 &fill_color, const KColor32 &outline_color) {
	KNode *self = getSelf();
	KMeshDrawable *co = KMeshDrawable::of(self);
	KMesh *mesh = co ? co->getMesh() : nullptr;
	if (mesh && count >= 2) {
		m_MeshBuf.resize2(mesh, count+1, 0);
		for (int i=0; i<count+1; i++) { // 図形を閉じるため i=count までループする
			float rad = KMath::degToRad(start_degrees + i * 360.0f / count);
			KVec3 p;
			p.x = pos.x + radius * cosf(rad);
			p.y = pos.y + radius * sinf(rad);
			m_MeshBuf.setPosition(i, p);
			m_MeshBuf.setColor32(i, KColor32::WHITE);
		}
		if (fill_color != KColor32::ZERO) {
			KMaterial m;
			m.color = fill_color;
			m_MeshBuf.addToMesh(mesh, KPrimitive_TRIANGLE_FAN, &m);
		}
		if (outline_color != KColor32::ZERO) {
			KMaterial m;
			m.color = outline_color;
			m_MeshBuf.addToMesh(mesh, KPrimitive_LINE_STRIP, &m);
		}
	}
};
#pragma endregion // KGizmoAct



} // namespace
