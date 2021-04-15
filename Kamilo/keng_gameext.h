/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "keng_game.h"
#include "KTextNode.h"
#include "KInspector.h"
#include "KAction.h"
#include "KThread.h"
#include "KSound.h"

#define SCENE_TEST 0



namespace Kamilo {

class KScene {
public:
	explicit KScene() {}
	virtual ~KScene() {}
	virtual int onQueryNextScene() = 0;
	virtual void onSceneEnter() {}
	virtual void onSceneExit() {}
	virtual void onSceneInspectorGui() {}

	/// パラメータがセットされるときに呼ばれる
	virtual void onSetParams(KNamedValues *new_params, const KNamedValues *specified_params, const KNamedValues *old_params) {}

	/// KSceneManager::setNextScene や setParams で設定されたパラメータを得る
	const KNamedValues * getParams() const;
	KNamedValues * getParamsEditable();
	void setParams(const KNamedValues *new_params);

private:
	KNamedValues m_params;
};

typedef int KSCENEID;

struct KSceneManagerSignalArgs {
	KSCENEID curr_id;   // 現在のシーン番号
	KSCENEID next_id;   // 次のシーン番号
	KScene *curr_scene; // 現在のシーン
	KNamedValues *next_params; // 次のシーンに設定されるパラメータ群

	KSceneManagerSignalArgs() {
		curr_id = 0;
		next_id = 0;
		curr_scene = nullptr;
		next_params = nullptr;
	}
};

class KGameSceneSystemCallback {
public:
	virtual void on_scenemgr_scene_changing(KSceneManagerSignalArgs *args) = 0;
};

class KSceneManager {
public:
	static void install();
	static void uninstall();
	static bool isInstalled();
	static void setCallback(KGameSceneSystemCallback *cb);
	static void restart(); // 現在のシーンを再スタートする
	static int getClock(); // シーン時刻（現在のシーンが始まってからの経過フレーム数）
	static KScene * getScene(KSCENEID id); // 登録済みのシーンを返す
	static KScene * getCurrentScene(); // 現在実行中のシーンを返す
	static KSCENEID getCurrentSceneId(); // 現在実行中のシーンの識別子を返す
	static void addScene(KSCENEID id, KScene *scene); // 新しいシーンを登録する（あくまでも登録するだけで、実行はされない。登録済みのシーンを実行するには setNextScene を使う）
	static void setNextScene(KSCENEID id, const KNamedValues *params=nullptr); // シーンを切り替える
	static void setSceneParamInt(const char *key, int val);
	static int getSceneParamInt(const char *key);
};



class KDataAct: public KAction {
public:
	static void attach(KNode *node);
	static KDataAct * of(KNode *node);
public:
	KDataAct();
	virtual void onInspector() override;
	void clearValues(); /// すべての値を削除する
	void clearValuesByTag(int tag); /// 指定されたタグの値をすべて削除する
	void clearValuesByPrefix(const char *prefix); /// 指定された文字で始まる名前の値をすべて削除する
	KPath getStr(const KPath &key, const KPath &def="") const; /// 文字列を得る
	void setStr(const KPath &key, const KPath &val, int tag=0); /// 文字列を設定する
	bool hasKey(const KPath &key) const; /// キーが存在するかどうか
	int getKeys(KPathList *keys) const; /// 全てのキーを得る

	int getInt(const KPath &key, int def=0) const {
		KPath s = getStr(key);
		return KStringUtils::toInt(s.u8(), def);
	}
	void setInt(const KPath &key, int val, int tag=0) {
		KPath pval = KPath::fromFormat("%d", val);
		setStr(key, pval, tag);
	}

	/// 値を保存する。password に文字列を指定した場合はファイルを暗号化する
	bool saveToFile(const KPath &filename, const char *password="");
	bool saveToFileCompress(const KPath &filename);

	/// 値をロードする。暗号化されている場合は password を指定する必要がある
	bool loadFromFile(const KPath &filename, const char *password="");
	bool loadFromFileCompress(const KPath &filename);

	/// 実際にロードせずに、保存内容だけを見る
	bool peekFile(const KPath &filename, const char *password, KNamedValues *nv) const;
private:
	void _clear(const KPathList &keys);
	bool saveToFileEx(const KNamedValues *nv, const KPath &filename, const char *password);
	bool saveToFileCompressEx(const KNamedValues *nv, const KPath &filename);
	bool loadFromFileCompressEx(KNamedValues *nv, const KPath &filename) const;
	bool loadFromNamedValues(const KNamedValues *nv);

	KNamedValues m_values;
	std::unordered_map<KPath, int> m_tags;
};


/// 2D用の簡易影管理
class KShadowAct: public KAction {
public:
	struct GlobalSettings {
		KColor color; // 影の色
		KBlend blend; // 影の合成方法
		int vertexCount; // 影の楕円の頂点数
		float maxAltitude; // 影が映る最大高度のデフォルト値
		float defaultRadiusX;
		float defaultRadiusY;
		int worldLayer;
		bool useYPositionAsAltitude;

		// 高度による自動スケーリング値を使う
		// 有効にした場合、max_height の高さでの影のサイズを 0.0、
		// 接地しているときの影のサイズを 1.0 としてサイズが変化する
		// maxheight が 0 の場合は規定値を使う
		bool scaleByAltitude; // 高度によって影の大きさを変える

		bool gradient; // 不透明から透明へグラデさせる

		GlobalSettings() {
			defaultRadiusX = 32;
			defaultRadiusY = 12;
			worldLayer = 1;
			useYPositionAsAltitude = false;
			vertexCount = 24;
			maxAltitude = 400;
			color = KColor(0.0f, 0.0f, 0.0f, 0.7f);
			blend = KBlend_ALPHA;
			scaleByAltitude = true;
			gradient = true;
		}
	};

	static void attach(KNode *node);
	static KShadowAct * of(KNode *node);
	static GlobalSettings * globalSettings();
public:
	KShadowAct();
	virtual void onEnterAction() override;
	virtual void onExitAction() override;
	virtual void onStepAction() override;
	virtual void onStepSystemAction() override;
	virtual void onInspector() override;

	/// 地上での影の表示位置
	void setOffset(const KVec3 &value);

	/// 楕円影の標準サイズ
	/// @see setScaleFactor
	void setRadius(float horz, float vert);
	void getRadius(float *horz, float *vert) const;

	/// 楕円影のスケーリング
	/// @see setRadius
	void setScaleFactor(float value);

	/// スプライト影の変形
	void setMatrix(const KMatrix4 &matrix);

	/// スプライト影を使う
	void setUseSprite(bool value);

	void setScaleByHeight(bool value, float maxheight=0);
	bool getAltitude(float *altitude);
private:
	struct ITEM {
		KVec3 offset;
		EID shadow_e;
		KPath shadow_tex_name;
		float radius_x;
		float radius_y;
		float scale;
		float max_height;
		float altitude; // read only
		bool scale_by_height;
		bool enabled;
		bool use_sprite;
		int delay;

		ITEM() {
			shadow_e = nullptr;
			radius_x = 0;
			radius_y = 0;
			scale = 0;
			max_height = 0;
			scale_by_height = false;
			enabled = false;
			use_sprite = false;
			altitude = -1;
			delay = 0;
		}
	};
	void update();
	void inspector_global();
	bool compute_shadow_transform(ITEM &item, KVec3 *out_pos, KVec3 *out_scale, float *out_alt);
	ITEM m_item;
};




























class KGizmoAct: public KAction {
public:
	static void attach(KNode *node);
	static KGizmoAct * of(KNode *node);
	static KGizmoAct * cast(KAction *act);
public:
	KGizmoAct();
	virtual void onEnterAction() override;

	void clear();

	// 直線
	void addLine(const KVec3 &a, const KVec3 &b);
	void addLine(const KVec3 &a, const KVec3 &b, const KColor32 &color);
	void addLine(const KVec3 &a, const KVec3 &b, const KColor32 &color_a, const KColor32 &color_b);

	// 正多角形
	void addRegularPolygon(const KVec3 &pos, float radius, int count, float start_degrees=0, const KColor32 &outline_color=KColor32::WHITE, const KColor32 &fill_color=KColor32::ZERO);
private:
	CMeshBuf m_MeshBuf;
};

} // namespace

