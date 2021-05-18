#include "KShadow.h"
//
#include "KImGui.h"
#include "KInternal.h"
#include "KMeshDrawable.h"
#include "KRes.h"
#include "KSolidBody.h"
#include "keng_game.h"

namespace Kamilo {

static KShadow::GlobalSettings g_GlobalShadowSettings;

class CShadowMgr: public KManagerTmpl<KShadow>, public KInspectorCallback {
public:
	CShadowMgr() {
		KEngine::addInspectorCallback(this);
	}
	virtual void onCompAdded(KNode *node, KShadow *comp) override {
		comp->_EnterAction();
	}
	virtual void onCompRemoving(KNode *node, KShadow *comp) override {
		comp->_ExitAction();
	}
	virtual void on_manager_appframe() override {
		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			it->second->_StepSystemAction();
		}
	}
	virtual void on_manager_nodeinspector(KNode *node) override {
		KShadow *comp = getComp(node);
		comp->_Inspector();
	}

	// KInspectorCallback
	virtual void onInspectorGui() override {
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
	}
};


static CShadowMgr *g_ShadowMgr = nullptr;

#define SHADOW_BASIC_RADIUS  64

static void _MakeCircleMesh(KMesh *mesh, const KVec2 &radius, int numvertices, const KColor &center_color, const KColor &outer_color) {
	K__Assert(mesh);
	K__Assert(numvertices >= 3);
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


KShadow::GlobalSettings * KShadow::globalSettings() {
	return &g_GlobalShadowSettings;
}

void KShadow::install() {
	K__Assert(g_ShadowMgr == nullptr);
	g_ShadowMgr = new CShadowMgr();
}
void KShadow::uninstall() {
	if (g_ShadowMgr) {
		g_ShadowMgr->drop();
		g_ShadowMgr = nullptr;
	}
}
void KShadow::attach(KNode *node) {
	KShadow *co = new KShadow();
	g_ShadowMgr->addComp(node, co);
	co->drop();
}
KShadow * KShadow::of(KNode *node) {
	return g_ShadowMgr->getComp(node);
}



KShadow::KShadow() {
	m_item.radius_x = g_GlobalShadowSettings.defaultRadiusX;
	m_item.radius_y = g_GlobalShadowSettings.defaultRadiusY;
	m_item.scale = 1.0f;
	m_item.scale_by_height = true;
	m_item.max_height = 0;
	m_item.use_sprite = false;
	m_item.enabled = true;
	m_item.delay = 2; // 最初のフレームでは、まだ影の描画に必要な情報が取得できていないので、念のために数フレーム経過してから表示するようにする
}
void KShadow::_EnterAction() {
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
	_MakeCircleMesh(mesh, KVec2(SHADOW_BASIC_RADIUS, SHADOW_BASIC_RADIUS), g_GlobalShadowSettings.vertexCount, center_color, outer_color);
	KMeshDrawable::of(self)->getSubMesh(0)->material.blend = g_GlobalShadowSettings.blend;
	m_item.shadow_tex_name = KPath::fromFormat("__shadowtex_%x", self->getId());
}
void KShadow::_ExitAction() {
	// 影専用のテクスチャを消す
	KBank::getTextureBank()->removeTexture(m_item.shadow_tex_name);
}
void KShadow::_StepSystemAction() {
	update();
}
void KShadow::update() {
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
			K__Assert(owner_sprite_tex);

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
void KShadow::_Inspector() {
	if (ImGui::CollapsingHeader("Global Settings")) {
		g_ShadowMgr->onInspectorGui();
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
void KShadow::setOffset(const KVec3 &value) {
	m_item.offset = value;
}
void KShadow::setRadius(float horz, float vert) {
	m_item.radius_x = horz;
	m_item.radius_y = vert;
}
void KShadow::getRadius(float *horz, float *vert) const {
	if (horz) *horz = m_item.radius_x;
	if (vert) *vert = m_item.radius_y;
}
void KShadow::setScaleFactor(float value) {
	m_item.scale = value;
}
void KShadow::setScaleByHeight(bool value, float maxheight) {
	m_item.scale_by_height = value;
	m_item.max_height = maxheight;
}
void KShadow::setUseSprite(bool value) {
	m_item.use_sprite = value;
}
void KShadow::setMatrix(const KMatrix4 &matrix) {
	KNode *self = getSelf();
	KDrawable *renderer = KDrawable::of(self);
	if (renderer) {
		renderer->setLocalTransform(matrix);
	}
}
bool KShadow::getAltitude(float *alt) {
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
bool KShadow::compute_shadow_transform(ITEM &item, KVec3 *out_pos, KVec3 *out_scale, float *out_alt) {
	K__Assert(out_pos);
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



} // namespace
