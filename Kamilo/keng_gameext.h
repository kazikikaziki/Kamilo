﻿/// @file
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
#include "KNamedValues.h"

#define SCENE_TEST 0



namespace Kamilo {




class KNamedValues;








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








class KGizmoAct: public KComp {
public:
	static void install();
	static void uninstall();
	static void attach(KNode *node);
	static KGizmoAct * of(KNode *node);
public:
	KGizmoAct();
	void clear();

	// 直線
	void addLine(const KVec3 &a, const KVec3 &b);
	void addLine(const KVec3 &a, const KVec3 &b, const KColor32 &color);
	void addLine(const KVec3 &a, const KVec3 &b, const KColor32 &color_a, const KColor32 &color_b);

	// 正多角形
	void addRegularPolygon(const KVec3 &pos, float radius, int count, float start_degrees=0, const KColor32 &outline_color=KColor32::WHITE, const KColor32 &fill_color=KColor32::ZERO);

private:
	KMeshDrawable * getGizmoMeshDrawable();
	CMeshBuf m_MeshBuf;
};

} // namespace

