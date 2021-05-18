/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <inttypes.h> // uint32_t
#include <math.h> // fabsf
#include "KVec.h"
#include "KQuat.h"

#define K__MATH_ERROR  K__math_error(__FUNCTION__)


#pragma region Numeric
namespace Kamilo {

class KMatrix4;

void K__math_error(const char *func);

class KMath {
public:
	static constexpr float PI = 3.14159265358979323846f; ///< 円周率

	/// @var EPS
	/// 実数比較の時の最大許容誤差
	///
	/// KENG_STRICT_EPSILON が定義されている場合は標準ヘッダ float.h の FLT_EPSILON をそのまま使う。
	/// この場合 KVec3::isNormalized() などの判定がかなりシビアになるので注意すること。
	///
	/// KENG_STRICT_EPSILON が 0 の場合は適当な精度の機械イプシロンを使う。
	/// FLT_EPSILON の精度が高すぎて比較に失敗する場合は KENG_STRICT_EPSILON を 0 にしておく。
	#ifdef KENG_STRICT_EPSILON
	static constexpr float EPS = FLT_EPSILON;
	#else
	static constexpr float EPS = 1.0f / 8192; // 0.00012207031
	#endif

	static int min(int a, int b);
	static int min3(int a, int b, int c) { return min(min(a, b), c); }
	static int min4(int a, int b, int c, int d) { return min(min(a, b), min(c, d)); }
	
	static float min(float a, float b);
	static float min3(float a, float b, float c) { return min(min(a, b), c); }
	static float min4(float a, float b, float c, float d) { return min(min(a, b), min(c, d)); }

	static int max(int a, int b);
	static int max3(int a, int b, int c) { return max(max(a, b), c); }
	static int max4(int a, int b, int c, int d) { return max(max(a, b), max(c, d)); }
	
	static float max(float a, float b);
	static float max3(float a, float b, float c) { return max(max(a, b), c); }
	static float max4(float a, float b, float c, float d) { return max(max(a, b), max(c, d)); }


	/// x の符号を -1, 0, 1 で返す
	static float signf(float x);
	static int signi(int x);

	/// 度からラジアンに変換
	static float degToRad(float deg);

	/// ラジアンから度に変換
	static float radToDeg(float rad);

	/// x が 2 の整数乗になっているか
	static bool isPow2(int x);

	/// パーリンノイズを -1.0 以上 1.0 以下の範囲で生成する
	static float perlin(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap);

	/// パーリンノイズを 0.0 以上 1.0 以下の範囲で生成する
	static float perlin01(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap);

	/// 開始値 edge0 と終了値 edge1 を係数 x で線形補間する。x が edge0 と edge1 の範囲外にある場合でも戻り値を丸めない
	static float linearStepUnclamped(float edge0, float edge1, float x);
	
	/// 開始値 edge0 と終了値 edge1 を係数 x で線形補間する。いわゆる smoothstep の線形バージョン
	static float linearStep(float edge0, float edge1, float x);
	
	/// linearStep と同じだが、山型の補完になる (x=0.5 で終了値 edge1 に達し、x=0.0およびx=1.0で開始値 edge0 になる)
	static float linearStepBumped(float edge0, float edge1, float x);

	/// 開始値 edge0 と終了値 edge1 を係数 x でエルミート補完する。@see https://ja.wikipedia.org/wiki/Smoothstep
	static float smoothStep(float edge0, float edge1, float x);

	/// smoothStep と同じだが、山型の補完になる (x=0.5 で終了値 edge1 に達し、x=0.0およびx=1.0で開始値 edge0 になる)
	static float smoothStepBumped(float edge0, float edge1, float x);

	/// smoothStep よりもさらに滑らかな補間を行う @see https://en.wikipedia.org/wiki/Smoothstep
	static float smootherStep(float edge0, float edge1, float x); 
	static float smootherStepBumped(float edge0, float edge1, float x);

	/// 整数の範囲を lower と upper の範囲内に収めた値を返す
	static int clampi(int a, int lower, int upper);

	/// 実数の範囲を lower と upper の範囲内に収めた値を返す
	static float clampf(float a, float lower, float upper);
	static float clamp01(float a);

	/// 四捨五入した値を返す
	static float round(float a);

	/// a を b で割ったあまりを 0 以上 b 未満の値で求める
	/// a >= 0 の場合は a % b と全く同じ結果になる。
	/// a < 0 での挙動が a % b とは異なり、a がどんな値でも戻り値は必ず正の値になる
	static int repeati(int a, int b);

	/// a と b の剰余を 0 以上 b 未満の範囲にして返す
	/// a >= 0 の場合は fmodf(a, b) と全く同じ結果になる。
	/// a < 0 での挙動が fmodf(a, b) とは異なり、a がどんな値でも戻り値は必ず正の値になる
	static float repeatf(float a, float b);

	/// 値が 0..n-1 の範囲で繰り返されるように定義されている場合に、
	/// 値 a を基準としたときの b までの符号付距離を返す。
	/// 値 a, b は範囲 [0..n-1] 内に収まるように転写され、絶対値の小さいほうの符号付距離を返す。
	/// たとえば n=10, a=4, b=6 としたとき、a から b までの距離は 2 と -8 の二種類ある。この場合 2 を返す
	/// a=6, b=4 だった場合、距離は -2 と 8 の二種類で、この場合は -2 を返す。
	static int delta_repeati(int a, int b, int n);

	/// 範囲 [a..b] である値 t を範囲 [a..b] でスケール変換した値を返す。
	/// t が範囲外だった場合、戻り値も範囲外になる
	/// @param a 開始値
	/// @param b 終了値
	/// @param t 補間係数
	/// @return  補間結果
	static float lerpUnclamped(float a, float b, float t);
	static float lerp(float a, float b, float t);

	/// 角度（度）を -180 度以上 +180 未満に正規化する
	static float normalize_deg(float deg);

	/// 角度（ラジアン）を -PI 以上 +PI 未満に正規化する
	static float normalize_rad(float rad);

	/// a と b の差が maxerr 以下（未満ではない）であれば true を返す
	/// maxerr を 0 に設定した場合は a == b と同じことになる
	/// @param a   比較値1
	/// @param b   比較値2
	/// @param eps 同値判定の精度
	static bool equals(float a, float b, float eps=KMath::EPS);

	/// 実数が範囲 lower .. upper と等しいかその内側にあるなら true を返す
	static bool inRange(float a, float lower=0.0f, float upper=1.0f);
	static bool inRange(int a, int lower, int upper);

	/// 実数がゼロとみなせるならば true を返す
	/// @param a   比較値
	/// @param eps 同値判定の精度
	static bool isZero(float a, float eps=KMath::EPS);

	/// 三角波を生成する
	/// cycle で与えられた周期で 0 .. 1 の間を線形に往復する。
	/// t=0       ---> 0.0
	/// t=cycle/2 ---> 1.0
	/// t=cycle   ---> 0.0
	static float triangle_wave(float t, float cycle);
	static float triangle_wave(int t, int cycle);
};




namespace Test {
void Test_math();
}
#pragma endregion // Numeric






#pragma region Math classes






class KRecti {
public:
	KRecti() { x=y=w=h=0; }
	int x, y;
	int w, h;
};

#pragma endregion // Math classes


class KGeom {
public:
	static constexpr float K_GEOM_EPS = 0.0000001f;
	static bool K_GEOM_ALMOST_ZERO(float x) { return fabsf(x) < K_GEOM_EPS; }
	static bool K_GEOM_ALMOST_SAME(float x, float y) { return K_GEOM_ALMOST_ZERO(x - y); }

	// AABBと点の最短距離
	static float K_GeomDistancePointToAabb(const KVec3 &p, const KVec3 &aabb_min, const KVec3 &aabb_max);

	/// XY平面上で (ax, ay) から (bx, by) を見ている時に点(px, py) が視線の左右どちらにあるか
	/// 右なら正の値、左なら負の値、視線上にあるなら 0 を返す
	static float K_GeomPointIsRight2D(float px, float py, float ax, float ay, float bx, float by);

	/// 三角形 abc の法線ベクトルを得る（時計回りの並びを表とする）
	/// 点または線分に縮退している場合は法線を定義できず false を返す。成功すれば out_cw_normalized_normal に正規化された法線をセットする
	static bool K_GeomTriangleNormal(const KVec3 &a, const KVec3 &b, const KVec3 &c, KVec3 *out_cw_normalized_normal);

	/// 点 p が三角形 abc 内にあるか判定する
	static bool K_GeomPointInTriangle(const KVec3 &p, const KVec3 &a, const KVec3 &b, const KVec3 &c);

	/// 点 p から直線 ab におろした垂線の交点
	/// @param out_dist_a[out] a から交点までの距離
	/// @param out_pos[out]    交点座標. これは a + (b-a).getNormalized() * out_dist_from_a に等しい
	static bool K_GeomPerpendicularToLine(const KVec3 &p, const KVec3 &a, const KVec3 &b, float *out_dist_from_a, KVec3 *out_pos);

	/// 点 p から直線 ab におろした垂線のベクトル(点 p から交点座標までのベクトル)
	static KVec3 K_GeomPerpendicularToLineVector(const KVec3 &p, const KVec3 &a, const KVec3 &b);

	/// 点 p から平面（点 plane_pos を通り法線 plane_normal を持つ）におろした垂線の交点
	static KVec3 K_GeomPerpendicularToPlane(const KVec3 &p, const KVec3 &plane_pos, const KVec3 &plane_normal);

	/// 点 p から三角形 abc におろした垂線の交点
	static bool K_GeomPerpendicularToTriangle(const KVec3 &p, const KVec3 &a, const KVec3 &b, const KVec3 &c, KVec3 *out_pos);

	/// 点 p と直線 ab の最短距離
	static float K_GeomDistancePointToLine(const KVec3 &p, const KVec3 &a, const KVec3 &b);

	/// 点 p と線分 ab との距離を得る
	/// p から直線 ab におろした垂線が a  の外側にあるなら zone に -1 をセットして距離 pa を返す
	/// p から直線 ab におろした垂線が ab の内側にあるなら zone に  0 をセットして p と直線 ab の距離を返す
	/// p から直線 ab におろした垂線が b  の外側にあるなら zone に  1 をセットして距離 pb を返す
	static float K_GeomDistancePointToLineSegment(const KVec3 &p, const KVec3 &a, const KVec3 &b, int *zone=nullptr);

	static float K_GeomDistancePointToCapsule(const KVec3 &p, const KVec3 &capsule_p0, const KVec3 &capsule_p1, float capsule_r);
	static float K_GeomDistancePointToObb(const KVec3 &p, const KVec3 &obb_pos, const KVec3 &obb_halfsize, const KMatrix4 &obb_matrix);
	static bool  K_GeomDistanceLineToLine(const KVec3 &A_pos, const KVec3 &A_dir, const KVec3 &B_pos, const KVec3 &B_dir, float *out_dist, float *out_A, float *out_B);

	/// aabb1 と aabb2 の交差範囲を求める。
	/// 交差ありの場合は交差区間をセットして true を返す。なお。交差ではなく接触状態の場合は交差区間のサイズが 0 になることに注意
	/// 交差なしの場合は false を返す
	static bool K_GeomIntersectAabb(
		const KVec3 &aabb1_min, const KVec3 &aabb1_max,
		const KVec3 &aabb2_min, const KVec3 &aabb2_max,
		KVec3 *out_intersect_min, KVec3 *out_intersect_max
	);

	/// 平面同士の交線
	static bool K_GeomIntersectPlane(
		const KVec3 &plane1_pos, const KVec3 &plane1_nor,
		const KVec3 &plane2_pos, const KVec3 &plane2_nor,
		KVec3 *out_dir, KVec3 *out_pos
	);
};



class KRay {
public:
	KRay();
	KRay(const KVec3 &_pos, const KVec3 &_dir);

	KVec3 pos;
	KVec3 dir;
};

struct KRayDesc {
	KRayDesc() {
		dist = 0;
	}
	float dist;   // レイ始点からの距離
	KVec3 pos;    // 交点座標
	KVec3 normal; // 交点座標における対象面の法線ベクトル（正規化済み）
};

struct KSphere {
	KSphere();
	KSphere(const KVec3 &_pos, float _r);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	float perpendicularPoint(const KVec3 &p, KVec3 *out_pos) const;
	float distance(const KVec3 &p) const;

	KVec3 pos;
	float r;
};

struct KPlane {
	KPlane();
	KPlane(const KVec3 &_pos, const KVec3 &_normal);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_point) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	float perpendicularPoint(const KVec3 &p, KVec3 *out_pos) const; // 点pから平面におろした垂線の交点
	float distance(const KVec3 &p) const;
	void normalizeThis();

	KVec3 m_pos;
	KVec3 m_normal;
};

struct KTriangle {
	KTriangle();
	KTriangle(const KVec3 &_a, const KVec3 &_b, const KVec3 &_c);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_point) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	bool perpendicularPoint(const KVec3 &p, float *out_dist, KVec3 *out_pos) const; // 点pから三角形におろした垂線の交点
	bool isPointInside(const KVec3 &p) const;
	KVec3 getNormal() const;

	KVec3 a, b, c;
};

struct KAabb {
	static bool intersect(const KAabb &a, const KAabb &b, KAabb *out);

	KAabb();
	KAabb(const KVec3 &_pos, const KVec3 &_halfsize);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	float distance(const KVec3 &p) const; // 点pとの最短距離
	KVec3 getMin() const;
	KVec3 getMax() const;

	KVec3 pos;
	KVec3 halfsize;
};

struct KCylinder {
	KCylinder();
	KCylinder(const KVec3 &_p0, const KVec3 &_p1, float _r);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	float distance(const KVec3 &p) const;

	KVec3 p0;
	KVec3 p1;
	float r;
};

struct KCapsule {
	KCapsule();
	KCapsule(const KVec3 &_p0, const KVec3 &_p1, float _r);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	float distance(const KVec3 &p) const;

	KVec3 p0;
	KVec3 p1;
	float r;
};


} // namespace
