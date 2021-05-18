#include "KMath.h"
#include "KVec.h"
#include "KMatrix.h"

// Use stb_perlin.h
#define K_USE_STB_PERLIN

// Use SIMD
#define K_USE_SIMD



#ifdef K_USE_SIMD
#	include <xmmintrin.h> // 128bit simd
#endif

#ifdef K_USE_STB_PERLIN
	// stb_image
	// https://github.com/nothings/stb/blob/master/stb_image.h
	// license: public domain
	#define STB_PERLIN_IMPLEMENTATION
	#include "stb_perlin.h"
#endif

#include <assert.h>
#include "KInternal.h"


namespace Kamilo {

void K__math_error(const char *func) {
	if (K__IsDebuggerPresent()) {
		K__Break();
		assert(0);
		K__Error("K__math_error at %s", func);
	} else {
		K__Error("K__math_error at %s", func);
	}
}

























#pragma region KMath
int KMath::max(int a, int b) {
	return a > b ? a : b;
}
float KMath::max(float a, float b) {
	return a > b ? a : b;
}
int KMath::min(int a, int b) {
	return a < b ? a : b;
}
float KMath::min(float a, float b) {
	return a < b ? a : b;
}
float KMath::signf(float x) {
	return (x < 0) ? -1.0f : (x > 0) ? 1.0f : 0.0f;
}
int KMath::signi(int x) {
	return (x < 0) ? -1 : (x > 0) ? 1 : 0;
}

/// 角度（度）からラジアンを求める
float KMath::degToRad(float deg) {
	return PI * deg / 180.0f;
}
/// 角度（ラジアン）から度を求める
float KMath::radToDeg(float rad) {
	return 180.0f * rad / PI;
}

/// x が 2 の整数乗であれば true を返す
bool KMath::isPow2(int x) {
	return fmodf(log2f((float)x), 1.0f) == 0.0f;
}

/// パーリンノイズ値を -1.0 ～ 1.0 の値で得る
/// x: ノイズ取得座標。周期は 1.0
/// x_wrap: 変化の周波数。0 だと繰り返さないが、
///         1 以上の値を指定すると境界でノイズがつながるようになる。
///         2のべき乗であること。最大値は256。大きな値を指定すると相対的に細かい変化になる
/// ヒント
/// x, y, z のどれかを 0 に固定すると正弦波のようになるため、あまりランダム感がない
/// wrap に 0 を指定するとかなり平坦な値になる
/// float noise = KMath::perlin(t, 0, z, 8, 0, 0);
float KMath::perlin(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap) {
#ifdef K_USE_STB_PERLIN
	// x, y, z: ノイズ取得座標。周期は 1.0
	// x_wrap, y_wrap, z_wrap: 折り畳み値（周期）
	// 2のべき乗を指定する. 最大値は256
	// -1 以上 1 以下の値を返す
	float xx = (x_wrap > 0) ? (x * x_wrap) : x;
	float yy = (y_wrap > 0) ? (y * x_wrap) : y;
	float zz = (z_wrap > 0) ? (z * x_wrap) : z;
	return stb_perlin_noise3(xx, yy, zz, x_wrap, y_wrap, z_wrap);
#else
	K__Warning("No perlin support");
	return 0.0f;
#endif
}

/// KMath::perlin と同じだが、戻り値が 0.0 以上 1.0 以下の範囲に射影する
float KMath::perlin01(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap) {
	float p = perlin(x, y, z, x_wrap, y_wrap, z_wrap);
	return linearStep(-1.0f, 1.0f, p);
}

float KMath::linearStepUnclamped(float edge0, float edge1, float x) {
	if (edge1 == edge0) {
		K__MATH_ERROR;
		return 1.0f; // xが既に終端 edge1 に達しているとみなす
	}
	return (x - edge0) / (edge1 - edge0);
}
// smoothstep の線形版。normalize。x の区間 [edge0 .. edge1] を [0..1] に再マッピングする
float KMath::linearStep(float edge0, float edge1, float x) {
	if (edge1 == edge0) {
		K__MATH_ERROR;
		return 1.0f; // xが既に終端 edge1 に達しているとみなす
	}
	return clamp01((x - edge0) / (edge1 - edge0));
}
float KMath::linearStepBumped(float edge0, float edge1, float x) {
	return 1.0f - fabsf(linearStep(edge0, edge1, x) - 0.5f) * 2.0f;
}
float KMath::smoothStep(float edge0, float edge1, float x) {
	x = linearStep(edge0, edge1, x);
	return x * x * (3 - 2 * x);
}
float KMath::smoothStepBumped(float edge0, float edge1, float x) {
	x = linearStepBumped(edge0, edge1, x);
	return x * x * (3 - 2 * x);
}
float KMath::smootherStep(float edge0, float edge1, float x) {
	// smoothstepよりもさらに緩急がつく
	// https://en.wikipedia.org/wiki/Smoothstep
	x = linearStep(edge0, edge1, x);
	return x * x * x * (x * (x * 6 - 15) + 10);
}
float KMath::smootherStepBumped(float edge0, float edge1, float x) {
	x = linearStepBumped(edge0, edge1, x);
	return x * x * x * (x * (x * 6 - 15) + 10);
}

int KMath::clampi(int a, int lower, int upper) {
	return (a < lower) ? lower : (a < upper) ? a : upper;
}
float KMath::clampf(float a, float lower, float upper) {
	return (a < lower) ? lower : (a < upper) ? a : upper;
}
float KMath::clamp01(float a) {
	return clampf(a, 0, 1);
}
float KMath::round(float a) {
	return (a >= 0) ? floorf(a + 0.5f) : -round(fabsf(a));
}
int KMath::repeati(int a, int b) {
	if (b == 0) return 0;
	if (a >= 0) return a % b;
	int B = (-a / b + 1) * b; // b の倍数のうち、-a を超えるもの (B+a>=0を満たすB)
	return (B + a) % b;
}
float KMath::repeatf(float a, float b) {
	if (b == 0) return 0;
	if (a >= 0) return fmodf(a, b);
	float B = ceilf(-a / b + 1.0f) * b; // b の倍数のうち、-a を超えるもの (B+a>=0を満たすB)
	return fmodf(B + a, b);
}
int KMath::delta_repeati(int a, int b, int n) {
	int aa = repeati(a, n);
	int bb = repeati(b, n);
	if (aa == bb) return 0;
	int d1 = bb - aa;
	int d2 = bb - aa + n;
	return (abs(d1) < abs(d2)) ? d1 : d2;
}
float KMath::lerpUnclamped(float a, float b, float t) {
	return a + (b - a) * t;
}
float KMath::lerp(float a, float b, float t) {
	return lerpUnclamped(a, b, clamp01(t));
}
float KMath::normalize_deg(float deg) {
	return repeatf(deg + 180.0f, 360.0f) - 180.0f;
}
float KMath::normalize_rad(float rad) {
	return repeatf(rad + PI, PI*2) - PI;
}
bool KMath::equals(float a, float b, float maxerr) {
	// a と b の差が maxerr 以下（未満ではない）であれば true を返す。
	// maxerr を 0 に設定した場合は a == b と同じことになる
	// maxerr=0の場合でも判定できるように等号を含める
	return fabsf(a - b) <= maxerr;
}
bool KMath::inRange(float a, float lower, float upper) {
	return lower <= a && a <= upper;
}
bool KMath::inRange(int a, int lower, int upper) {
	return lower <= a && a <= upper;
}
bool KMath::isZero(float a, float eps) {
	return equals(a, 0, eps);
}
float KMath::triangle_wave(float t, float cycle) {
	float m = fmodf(t, cycle);
	m /= cycle;
	if (m < 0.5f) {
		return m / 0.5f;
	} else {
		return (1.0f - m) / 0.5f;
	}
}
float KMath::triangle_wave(int t, int cycle) {
	return triangle_wave((float)t, (float)cycle);
}
#pragma endregion // KMath




















#pragma region Math classes










#pragma endregion // Math classes





#pragma region KGeom
static float _SQ(float x) { return x * x; }

float KGeom::K_GeomPointIsRight2D(float px, float py, float ax, float ay, float bx, float by) {
	// XY平面上で (ax, ay) から (bx, by) を見ている時に点(px, py) が視線の左右どちらにあるか
	// 右なら正の値、左なら負の値、視線上にあるなら 0 を返す
	float Ax = bx - ax;
	float Ay = by - ay;
	float Bx = px - ax;
	float By = py - ay;
	return Ax * By  - Ay * Bx;
}

bool KGeom::K_GeomTriangleNormal(const KVec3 &a, const KVec3 &b, const KVec3 &c, KVec3 *out_cw_normalized_normal) {
	// 三角形 abc の法線ベクトルを得る（時計回りの並びを表とする）
	// out_cw_normalized_normal に正規化された法線をセットする
	KVec3 ab = b - a;
	KVec3 ac = c - a;
	KVec3 no = ab.cross(ac);
	return no.getNormalizedSafe(out_cw_normalized_normal);
}
bool KGeom::K_GeomPointInTriangle(const KVec3 &p, const KVec3 &a, const KVec3 &b, const KVec3 &c) {
	// 点 p が三角形 abc 内にあるか判定する
	KVec3 ab = b - a;
	KVec3 bp = p - b;

	KVec3 bc = c - b;
	KVec3 cp = p - c;

	KVec3 ca = a - c;
	KVec3 ap = p - a;

	KVec3 v1, v2, v3;

	KVec3 ab_x_bp = ab.cross(bp);
	KVec3 bc_x_cp = bc.cross(cp);
	KVec3 ca_x_ap = ca.cross(ap);

	bool on_edge = false;
	if (!ab_x_bp.getNormalizedSafe(&v1)) on_edge = true;
	if (!bc_x_cp.getNormalizedSafe(&v2)) on_edge = true;
	if (!ca_x_ap.getNormalizedSafe(&v3)) on_edge = true;
	if (on_edge) {
		// p が abc の辺に重なっている場合は三角形内部とみなす
		// ※連続する三角形から成る図形について判定したいときに、境界線の部分を図形内部として判定できるように
		return true;
	}

	// この三つの外積ベクトル v1 v2 v3 の向きがそろっていればよい。
	// 向きが同じかどうかを内積で調べる
	float dot12 = v1.dot(v2);
	float dot13 = v1.dot(v3);

	return (dot12 > 0 && dot13 > 0); // 両方とも正の値（同一方向）ならOK
}
KVec3 KGeom::K_GeomPerpendicularToLineVector(const KVec3 &p, const KVec3 &a, const KVec3 &b) {
	// 点 p から直線 ab におろした垂線のベクトル
	KVec3 dir;
	if ((b - a).getNormalizedSafe(&dir)) {
		float dist_a = dir.dot(p - a); // [点a] から [点pを通る垂線との交点] までの距離
		KVec3 H = a + dir * dist_a; // 交点
		return H - p; // p から H へのベクトル
	} else {
		K__MATH_ERROR;
		return KVec3();
	}
}
bool KGeom::K_GeomPerpendicularToLine(const KVec3 &p, const KVec3 &a, const KVec3 &b, float *out_dist_a, KVec3 *out_pos) {
	// 点 p から直線 ab におろした垂線の交点
	// out_dist_a: aから交点までの距離
	// out_pos: 交点座標

	// 幾何計算（２次元・３次元）の計算公式
	// https://www.eyedeal.co.jp/img/eyemLib_caliper.pdf
	// 指定点を通り，指定直線に垂直な直線
	KVec3 dir;
	if ((b - a).getNormalizedSafe(&dir)) {
		float dist_a = dir.dot(p - a); // [点a] から [点pを通る垂線との交点] までの距離
		if (out_dist_a) *out_dist_a = dist_a;
		if (out_pos) *out_pos = a + dir * dist_a;
		return true;
	}
	return false;
}
KVec3 KGeom::K_GeomPerpendicularToPlane(const KVec3 &p, const KVec3 &plane_pos, const KVec3 &plane_normal) {
	// https://math.keicode.com/vector-calculus/distance-from-plane-to-point.php
	// 点 p と、原点を通る法線 n の平面との距離は
	// dist = |p dot n|
	// で求まる。これで点 p から法線交点までの距離がわかるので、法線ベクトルを利用して
	// 点 p から dist だけ離れた位置の点を得る。
	// dist = p + nor * |p dot n|
	// ただしここで使いたいのは点から面に向かうベクトルであり、面の法線ベクトルとは逆向きなので
	// 符号を反転させておく
	// dist = p - nor * |p dot n|
	KVec3 nor;
	if (plane_normal.getNormalizedSafe(&nor)) {
		KVec3 relpos = p - plane_pos;
		float dist = relpos.dot(nor);
		return p - nor * dist;
	} else {
		K__MATH_ERROR;
		return KVec3();
	}
}

bool KGeom::K_GeomPerpendicularToTriangle(const KVec3 &p, const KVec3 &a, const KVec3 &b, const KVec3 &c, KVec3 *out_pos) {
	// 点 p から三角形 abc におろした垂線の交点
	
	// 三角形の法線
	KVec3 nor;
	if (!K_GeomTriangleNormal(a, b, c, &nor)) {
		return false;
	}

	// 三角形を含む平面に降ろした垂線
	KVec3 q = K_GeomPerpendicularToPlane(p, a, nor);

	// 交点 q は三角形の内部にあるか？
	if (!K_GeomPointInTriangle(q, a, b, c)) {
		return false;
	}
	if (out_pos) *out_pos = q;
	return true;
}

float KGeom::K_GeomDistancePointToLine(const KVec3 &p, const KVec3 &a, const KVec3 &b) {
	// 点 p と直線 ab の最短距離
	KVec3 H;
	if (K_GeomPerpendicularToLine(p, a, b, nullptr, &H)) {
		return (H - p).getLength();
	} else {
		K__MATH_ERROR;
		return 0;
	}
}

float KGeom::K_GeomDistancePointToAabb(const KVec3 &p, const KVec3 &aabb_min, const KVec3 &aabb_max) {
	// AABBと点の最短距離
	// http://marupeke296.com/COL_3D_No11_AABBvsPoint.html
	float sqlen = 0;
	if (p.x < aabb_min.x) sqlen += _SQ(p.x - aabb_min.x);
	if (p.x > aabb_max.x) sqlen += _SQ(p.x - aabb_max.x);
	if (p.y < aabb_min.y) sqlen += _SQ(p.y - aabb_min.y);
	if (p.y > aabb_max.y) sqlen += _SQ(p.y - aabb_max.y);
	if (p.z < aabb_min.z) sqlen += _SQ(p.z - aabb_min.z);
	if (p.z > aabb_max.z) sqlen += _SQ(p.z - aabb_max.z);
	return sqrtf(sqlen);
}
float KGeom::K_GeomDistancePointToObb(const KVec3 &p, const KVec3 &obb_pos, const KVec3 &obb_halfsize, const KMatrix4 &obb_matrix) {
	// OBBと点の最短距離
	// http://marupeke296.com/COL_3D_No12_OBBvsPoint.html
	KVec3 v;
	if (obb_halfsize.x > 0) {
		KVec3 axisdir = obb_matrix.transform(KVec3(1, 0, 0));
		float s = (p - obb_pos).dot(axisdir);
		if (fabsf(s) > 1.0f) {
			v += axisdir * (1 - s) * obb_halfsize.x;
		}
	}
	if (obb_halfsize.y > 0) {
		KVec3 axisdir = obb_matrix.transform(KVec3(0, 1, 0));
		float s = (p - obb_pos).dot(axisdir);
		if (fabsf(s) > 1.0f) {
			v += axisdir * (1 - s) * obb_halfsize.y;
		}
	}
	if (obb_halfsize.z > 0) {
		KVec3 axisdir = obb_matrix.transform(KVec3(0, 0, 1));
		float s = (p - obb_pos).dot(axisdir);
		if (fabsf(s) > 1.0f) {
			v += axisdir * (1 - s) * obb_halfsize.z;
		}
	}
	return v.getLength();
}
float KGeom::K_GeomDistancePointToLineSegment(const KVec3 &p, const KVec3 &a, const KVec3 &b, int *zone) {
	// p から直線abに垂線を下したときの点 Q を求める
	float Q_from_a = FLT_MAX; // p から垂線交点 Q までの距離
	KVec3 Q_pos; // 垂線交点 Q の座標
	if (!K_GeomPerpendicularToLine(p, a, b, &Q_from_a, &Q_pos)) {
		return -1;
	}

	float len = (b - a).getLength();

	if (Q_from_a < 0) {
		// Q が a よりも外側にある
		if (zone) *zone = -1;
		return (p - a).getLength();

	} else if (Q_from_a < len) {
		// Q が a と b の間にある
		if (zone) *zone = 0;
		return (p - Q_pos).getLength();

	} else {
		// Q が b よりも外側にある
		if (zone) *zone = 1;
		return (p - b).getLength();
	}
}
float KGeom::K_GeomDistancePointToCapsule(const KVec3 &p, const KVec3 &capsule_p0, const KVec3 &capsule_p1, float capsule_r) {
	float dist = K_GeomDistancePointToLineSegment(p, capsule_p0, capsule_p1, nullptr);
	if (dist >= 0) {
		return dist - capsule_r;
	} else {
		return 0;
	}
}


// 直線が平行になっている場合は out_dist だけセットして false を返す
// それ以外の場合は out_dist および out_A, out_B をセットして true を返す
// 直線A上の最接近点 = A_pos + A_dir * out_A
// 直線B上の最接近点 = B_pos + B_dir * out_B
bool KGeom::K_GeomDistanceLineToLine(const KVec3 &A_pos, const KVec3 &A_dir, const KVec3 &B_pos, const KVec3 &B_dir, float *out_dist, float *out_A, float *out_B) {
	// 直線同士の最接近距離と最接近点を知る
	// http://marupeke296.com/COL_3D_No19_LinesDistAndPos.html
	KVec3 delta = B_pos - A_pos;
	float A_dot = delta.dot(A_dir);
	float B_dot = delta.dot(B_dir);
	float A_cross_B_lensq = A_dir.cross(B_dir).getLengthSq();
	if (K_GEOM_ALMOST_ZERO(A_cross_B_lensq)) {
		// 平行。距離は求まるが点は求まらない
		if (out_dist) *out_dist = delta.cross(A_dir).getLength();
		return false;
	}
	float AB = A_dir.dot(B_dir);
	float At = (A_dot - B_dot * AB) / (1.0f - AB * AB);
	float Bt = (B_dot - A_dot * AB) / (AB * AB - 1.0f);
	KVec3 A_result = A_pos + A_dir * At;
	KVec3 B_result = B_pos + B_dir * Bt;
	if (out_dist) *out_dist = (B_result - A_result).getLength();
	if (out_A) *out_A = At;
	if (out_A) *out_A = Bt;
	return true;
}

/// aabb1 と aabb2 の交差範囲
bool KGeom::K_GeomIntersectAabb(
	const KVec3 &aabb1_min, const KVec3 &aabb1_max,
	const KVec3 &aabb2_min, const KVec3 &aabb2_max,
	KVec3 *out_intersect_min, KVec3 *out_intersect_max)
{
	if (aabb1_max.x < aabb2_min.x) return false; // 交差・接触なし
	if (aabb1_max.y < aabb2_min.y) return false;
	if (aabb1_max.z < aabb2_min.z) return false;
			
	if (aabb2_max.x < aabb1_min.x) return false; // 交差・接触なし
	if (aabb2_max.y < aabb1_min.y) return false;
	if (aabb2_max.z < aabb1_min.z) return false;

	// 交差あり、交差範囲を設定する（交差ではなく接触の場合はサイズ 0 になることに注意）
	if (out_intersect_min) {
		out_intersect_min->x = KMath::max(aabb1_min.x, aabb2_min.x);
		out_intersect_min->y = KMath::max(aabb1_min.y, aabb2_min.y);
		out_intersect_min->z = KMath::max(aabb1_min.z, aabb2_min.z);
	}
	if (out_intersect_max) {
		out_intersect_max->x = KMath::min(aabb1_max.x, aabb2_max.x);
		out_intersect_max->y = KMath::min(aabb1_max.y, aabb2_max.y);
		out_intersect_max->z = KMath::min(aabb1_max.z, aabb2_max.z);
	}
	return true;
}

/// 平面同士の交線
bool KGeom::K_GeomIntersectPlane(
	const KVec3 &plane1_pos, const KVec3 &plane1_nor, 
	const KVec3 &plane2_pos, const KVec3 &plane2_nor,
	KVec3 *out_dir, KVec3 *out_pos)
{
	KVec3 nor1, nor2;
	if (!plane1_nor.getNormalizedSafe(&nor1)) return false;
	if (!plane2_nor.getNormalizedSafe(&nor2)) return false;

	float dot1 = plane1_pos.dot(nor1);
	float dot2 = plane2_pos.dot(nor2);

	KVec3 cross = nor1.cross(nor2);
	if (cross.z != 0) {
		if (out_dir) *out_dir = cross;
		if (out_pos) {
			out_pos->x = (dot1 * plane2_nor.y - dot2 * plane1_nor.y) / cross.z;
			out_pos->y = (dot1 * plane2_nor.x - dot2 * plane1_nor.x) / -cross.z;
			out_pos->z = 0;
			return true;
		}
	}
	if (cross.y != 0) {
		if (out_dir) *out_dir = cross;
		if (out_pos) {
			out_pos->x = (dot1 * plane2_nor.z - dot2 * plane1_nor.z) / -cross.y;
			out_pos->y = 0;
			out_pos->z = (dot1 * plane2_nor.x - dot2 * plane1_nor.x) / cross.y;
			return true;
		}
	}
	if (cross.x != 0) {
		if (out_dir) *out_dir = cross;
		if (out_pos) {
			out_pos->x = 0;
			out_pos->y = (dot1 * plane2_nor.z - dot2 * plane1_nor.z) / cross.x;
			out_pos->z = (dot1 * plane2_nor.y - dot2 * plane1_nor.y) / -cross.x;
			return true;
		}
	}
	return false; // cross の成分がすべてゼロ。２平面は平行で交線なし
}

#pragma endregion // KGeom



#pragma region KRay
KRay::KRay() {
}
KRay::KRay(const KVec3 &_pos, const KVec3 &_dir) {
	pos = _pos;
	dir = _dir;
}
#pragma endregion // Ray



#pragma region KSphere
KSphere::KSphere() {
	this->r = 0;
}
KSphere::KSphere(const KVec3 &_pos, float _r) {
	this->pos = _pos;
	this->r = _r;
}
float KSphere::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc, nullptr)) {
		return desc.dist;
	}
	return -1;
}
bool KSphere::rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const {
	// レイと球の貫通点
	// http://marupeke296.com/COL_3D_No24_RayToSphere.html
	if (this->r <= 0) {
		return false;
	}
	KVec3 ray_nor;
	if (!ray.dir.getNormalizedSafe(&ray_nor)) {
		return false;
	}
	KVec3 p = this->pos - ray.pos;
	float A = ray_nor.dot(ray_nor);
	float B = ray_nor.dot(p);
	float C = p.dot(p) - this->r * this->r;
	float sq = B * B - A * C;
	if (sq < 0) {
		return false; // 衝突なし
	}
	float s = sqrtf(sq);
	float dist1 = (B - s) / A;
	float dist2 = (B + s) / A;
	if (dist1 < 0 || dist2 < 0) {
		return false; // レイ始点よりも後ろ側で衝突
	}
	if (out_near) {
		out_near->dist = dist1;
		out_near->pos = ray.pos + ray_nor * dist1;
		out_near->normal = (out_near->pos - this->pos).getNormalized();
	}
	if (out_far) {
		out_far->dist = dist2;
		out_far->pos = ray.pos + ray_nor * dist2;
		out_far->normal = (out_far->pos - this->pos).getNormalized();
	}
	return true;
}
bool KSphere::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	KVec3 delta = sphere.pos - this->pos;
	float len = delta.getLength();
	if (len < sphere.r + this->r) {
		if (out_dist) *out_dist = len - (sphere.r + this->r);
		if (out_normal) delta.getNormalizedSafe(out_normal);
		return true;
	}
	return false;
}
float KSphere::perpendicularPoint(const KVec3 &p, KVec3 *out_pos) const {
	K__ASSERT_RETURN_VAL(this->r >= 0, -1);
	KVec3 delta = p - this->pos;
	float len = delta.getLength();
	if (this->r == 0) {
		if (out_pos) *out_pos = this->pos;
		return len;
	}
	if (this->r <= len) {
		KVec3 delta_nor = delta.getNormalized();
		if (out_pos) *out_pos = this->pos + delta_nor * this->r;
		return len - this->r;
	}
	return -1;
}
float KSphere::distance(const KVec3 &p) const {
	return perpendicularPoint(p, nullptr);
}
#pragma endregion // KSphere



#pragma region KPlane
KPlane::KPlane() {
}
KPlane::KPlane(const KVec3 &_pos, const KVec3 &_normal) {
	m_pos = _pos;
	m_normal = _normal;
	normalizeThis();
}
void KPlane::normalizeThis() {
	m_normal = m_normal.getNormalized();
}
float KPlane::perpendicularPoint(const KVec3 &p, KVec3 *out_pos) const {
	// https://math.keicode.com/vector-calculus/distance-from-plane-to-point.php
	// 点 p と、原点を通る法線 n の平面との距離は
	// dist = |p dot n|
	// で求まる。これで点 p から法線交点までの距離がわかるので、法線ベクトルを利用して
	// 点 p から dist だけ離れた位置の点を得る。
	// dist = p + nor * |p dot n|
	// ただしここで使いたいのは点から面に向かうベクトルであり、面の法線ベクトルとは逆向きなので
	// 符号を反転させておく
	// dist = p - nor * |p dot n|
	if (!m_normal.isZero()) {
		KVec3 relpos = p - m_pos;
		float dist = relpos.dot(m_normal);
		if (out_pos) *out_pos = p - m_normal * dist;
		return dist;
	}
	K__MATH_ERROR;
	return 0;
}
float KPlane::distance(const KVec3 &p) const {
	KVec3 relpos = p - m_pos;
	return relpos.dot(m_normal);
}
float KPlane::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc)) {
		return desc.dist;
	}
	return -1;
}
bool KPlane::rayTest(const KRay &ray, KRayDesc *out_point) const {
	// https://gdbooks.gitbooks.io/3dcollisions/content/Chapter3/raycast_plane.html
	// https://t-pot.com/program/93_RayTraceTriangle/index.html
	KVec3 plane_nor;
	KVec3 ray_nor;
	if (!m_normal.getNormalizedSafe(&plane_nor)) return false;
	if (!ray.dir.getNormalizedSafe(&ray_nor)) return false;
	KVec3 delta = m_pos - ray.pos;
	float pn = plane_nor.dot(delta);
	float nd = plane_nor.dot(ray_nor);
	if (nd == 0) {
		return false; // レイは平面に平行
	}
	float t = pn / nd;
	if (t < 0) {
		return false; // 交点はレイ始点の後方にある
	}
	if (out_point) {
		out_point->dist = t;
		out_point->pos = ray.pos + ray_nor * t;
		out_point->normal = plane_nor;
	}
	return true;
}
bool KPlane::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	// wpos からこの平面への垂線の交点
	KVec3 foot_wpos = KGeom::K_GeomPerpendicularToPlane(sphere.pos, m_pos, m_normal); // 回転を考慮しないので法線は変わらない

//	if (is_point_in_trim(foot_wpos - this->pos)) {
		KVec3 delta = sphere.pos - foot_wpos; // 平面から球への垂線ベクトル
		float dist = delta.getLength();

		if (delta.dot(m_normal) < 0) {
			// 球は平面の裏側にある。負の距離を使う
			dist = -dist;
		}

		if (dist <= sphere.r) {
			if (out_dist) *out_dist = dist;
			if (out_normal) *out_normal = m_normal;
			return true;
		}
//	}

	// TrimAABB のエッジ部分との衝突は考慮しない
	return false;
}
#pragma endregion // KPlane



#pragma region KTriangle
KTriangle::KTriangle() {
}
KTriangle::KTriangle(const KVec3 &_a, const KVec3 &_b, const KVec3 &_c) {
	this->a = _a;
	this->b = _b;
	this->c = _c;
}
KVec3 KTriangle::getNormal() const {
	KVec3 nor;
	if (KGeom::K_GeomTriangleNormal(this->a, this->b, this->c, &nor)) {
		return nor;
	}
	return KVec3();
}
bool KTriangle::isPointInside(const KVec3 &p) const {
	return KGeom::K_GeomPointInTriangle(p, a, b, c);
}
bool KTriangle::perpendicularPoint(const KVec3 &p, float *out_dist, KVec3 *out_pos) const {
	return KGeom::K_GeomPerpendicularToTriangle(p, a, b, c, out_pos);
}
float KTriangle::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc)) {
		return desc.dist;
	}
	return -1;
}
bool KTriangle::rayTest(const KRay &ray, KRayDesc *out_point) const {
	KVec3 ray_nor;
	if (!ray.dir.getNormalizedSafe(&ray_nor)) {
		return false;
	}
	// 三角形の面法線を求める
	KVec3 nor;
	if (!KGeom::K_GeomTriangleNormal(this->a, this->b, this->c, &nor)) {
		return false;
	}

	// [レイ]と[三角形を含む平面]との交点距離
	KRayDesc hit;
	KPlane plane(this->a, nor);
	if (!plane.rayTest(ray, &hit)) {
		return false;
	}

	// 交点が三角形の内部にあるかどうか
	if (!KGeom::K_GeomPointInTriangle(hit.pos, this->a, this->b, this->c)) {
		return false;
	}

	if (out_point) {
		out_point->dist = hit.dist;
		out_point->pos = hit.pos;
		out_point->normal = nor;
	}
	return true;
}
bool KTriangle::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	float dist = 0;
	if (perpendicularPoint(sphere.pos, &dist, nullptr)) {
		if (dist < sphere.r) {
			if (out_dist) *out_dist = dist;
			if (out_normal) * out_normal = getNormal();
			return true;
		}
	}
	return false;
}
#pragma endregion // KTriangle



#pragma region KAabb
KAabb::KAabb() {
}
KAabb::KAabb(const KVec3 &_pos, const KVec3 &_halfsize) {
	this->pos = _pos;
	this->halfsize = _halfsize;
}
KVec3 KAabb::getMin() const {
	return pos - halfsize;
}
KVec3 KAabb::getMax() const {
	return pos + halfsize;
}
float KAabb::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc, nullptr)) {
		return desc.dist;
	}
	return -1;
}
bool KAabb::rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const {
	// 直線とAABB
	// http://marupeke296.com/COL_3D_No18_LineAndAABB.html
	// https://gdbooks.gitbooks.io/3dcollisions/content/Chapter3/raycast_aabb.html]
	// https://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms
	// 
	KVec3 ray_nor;
	if (!ray.dir.getNormalizedSafe(&ray_nor)) {
		return false;
	}
	const KVec3 minpos = this->pos - this->halfsize;
	const KVec3 maxpos = this->pos + this->halfsize;
	const float pos[] = {ray.pos.x, ray.pos.y, ray.pos.z};
	const float dir[] = {ray_nor.x, ray_nor.y, ray_nor.z };
	const float aabbmin[] = {minpos.x, minpos.y, minpos.z};
	const float aabbmax[] = {maxpos.x, maxpos.y, maxpos.z};
	float t_near = -FLT_MAX;
	float t_far = FLT_MAX;
	int hit_near = 0; // どの面と交差しているか？
	int hit_far = 0;
	for (int i=0; i<3; i++) {
		if (dir[i] == 0) {
			if (pos[i] < aabbmin[i] || pos[i] > aabbmax[i]) {
				return false; // 交差なし
			}
		}
		else {
			// スラブとの距離を算出
			float inv_dir = 1.0f / dir[i];
			float dist_min = (aabbmin[i] - pos[i]) * inv_dir; // min面との距離
			float dist_max = (aabbmax[i] - pos[i]) * inv_dir; // max面との距離
			if (dist_min > dist_max) {
				std::swap(dist_min, dist_max);
			}
			if (dist_min > t_near) {
				t_near = dist_min;
				hit_near = i;
			}
			if (dist_max < t_far) {
				t_far = dist_max;
				hit_far = i;
			}
			// スラブ交差チェック
			if (t_near > t_far) {
				return false;
			}
		}
	}
	// 交差あり
	// *out_pos_near = ray_pos + ray_nor * t_near;
	// *out_pos_far = ray_pos + ray_nor * t_far;

	// AABBの法線ベクトル
	static const KVec3 NORMALS[] = {
		KVec3(-1, 0, 0), // xmin
		KVec3( 1, 0, 0), // xmax
		KVec3( 0,-1, 0), // ymin
		KVec3( 0, 1, 0), // ymax
		KVec3( 0, 0,-1), // zmin
		KVec3( 0, 0, 1), // zmax
	};
	if (out_near) {
		out_near->dist = t_near;
		out_near->pos = ray.pos + ray_nor * t_near;

		// 近交点におけるAABB法線。
		// [レイの発射点 < aabbmin] なら min 側に当たっている。
		// [レイの発射点 > aabbmax] なら max 側に当たっている。
		// [aabbmin <= レイの発射点 <= aabbmax] のパターンは既に「交差なし」として除外されている筈なので考慮しない
		// ※hit_near には 0, 1, 2 のいずれかが入っていて、それぞれ X,Y,Z に対応する
		assert(0 <= hit_near && hit_near < 3);
		if (pos[hit_near] < aabbmin[hit_near]) {
			out_near->normal = NORMALS[hit_near * 2 + 0]; // min 側
		} else {
			out_near->normal = NORMALS[hit_near * 2 + 1]; // max 側
		}
	}
	if (out_far) {
		out_far->dist = t_far;
		out_far->pos = ray.pos + ray_nor * t_far;

		// 遠交点におけるAABB法線。
		// [レイの発射点 < aabbmin] なら min 側に当たっている。
		// [レイの発射点 > aabbmax] なら max 側に当たっている。
		// [aabbmin <= レイの発射点 <= aabbmax] のパターンは既に「交差なし」として除外されている筈なので考慮しない
		// ※hit_far には 0, 1, 2 のいずれかが入っていて、それぞれ X,Y,Z に対応する
		assert(0 <= hit_far && hit_far < 3);
		if (pos[hit_far] < aabbmin[hit_far]) {
			out_far->normal = NORMALS[hit_far * 2 + 0]; // min 側
		} else {
			out_far->normal = NORMALS[hit_far * 2 + 1]; // max 側
		}
	}
	return true;
}
bool KAabb::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	KVec3 delta = sphere.pos - this->pos;
	if (fabsf(delta.x) > this->halfsize.x + sphere.r) return false;
	if (fabsf(delta.y) > this->halfsize.y + sphere.r) return false;
	if (fabsf(delta.z) > this->halfsize.z + sphere.r) return false;

	if (fabsf(delta.y) <= this->halfsize.y && fabsf(delta.z) <= this->halfsize.z) {
		// a または b の位置にいる = X軸方向にめり込んでいる。左右にずらして衝突を解決する
		//
		//   z
		//   |       |
		//---+-------+---
		// a | AABB  | b
		//---0-------+---> x
		//   |       |
		//
		if (out_dist) *out_dist = fabsf(delta.x) - this->halfsize.x;
		if (out_normal) {
			out_normal->x = (delta.x >= 0) ? 1.0f : -1.0f;
			out_normal->y = 0;
			out_normal->z = 0;
		}
		return true;
	}
	if (fabsf(delta.x) <= this->halfsize.x && fabsf(delta.z) <= this->halfsize.z) {
		// a  の位置にいる（上面または下面） = Y軸方向にめり込んでいる。上下にずらして衝突を解決する
		//
		//   z
		//   |       |
		//---+-------+---
		//   |AA(a)BB|  
		//---0-------+---> x
		//   |       |
		//
		if (out_dist) *out_dist = fabsf(delta.y) - this->halfsize.y;
		if (out_normal) {
			out_normal->x = 0;
			out_normal->y = (delta.y >= 0) ? 1.0f : -1.0f;
			out_normal->z = 0;
		}
		return true;
	}
	if (fabsf(delta.x) <= this->halfsize.x && fabsf(delta.y) <= this->halfsize.y) {
		// a または b の位置にいる = Z軸方向にめり込んでいる。前後にずらして衝突を解決する
		//
		//   z
		//   |       |
		//---+-------+---
		// a | AABB  | b
		//---0-------+---> x
		//   |       |
		//
		if (out_dist) *out_dist = fabsf(delta.z) - this->halfsize.z;
		if (out_normal) {
			out_normal->x = 0;
			out_normal->y = 0;
			out_normal->z = (delta.z >= 0) ? 1.0f : -1.0f;
		}
		return true;
	}
	return false;
}
bool KAabb::intersect(const KAabb &a, const KAabb &b, KAabb *out) {
	KVec3 amin = a.getMin();
	KVec3 amax = a.getMax();
	KVec3 bmin = b.getMin();
	KVec3 bmax = b.getMax();
	KVec3 cmin, cmax;
	if (KGeom::K_GeomIntersectAabb(amin, amax, bmin, bmax, &cmin, &cmax)) {
		if (out) *out = KAabb((cmin+cmax)*0.5f, (cmax-cmin)*0.5f);
		return true;
	}
	return false;
}
float KAabb::distance(const KVec3 &p) const {
	KVec3 minpos = getMin();
	KVec3 maxpos = getMax();
	return KGeom::K_GeomDistancePointToAabb(p, minpos, maxpos);
}
#pragma endregion // KAabb



#pragma region KCylinder
KCylinder::KCylinder() {
	this->r = 0;
}
KCylinder::KCylinder(const KVec3 &_p0, const KVec3 &_p1, float _r) {
	this->p0 = _p0;
	this->p1 = _p1;
	this->r = _r;
}
float KCylinder::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc, nullptr)) {
		return desc.dist;
	}
	return -1;
}
bool KCylinder::rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const {
	// レイと無限円柱の貫通点
	// http://marupeke296.com/COL_3D_No25_RayToSilinder.html
	if (this->p0 == this->p1) {
		return false;
	}
	if (this->r <= 0) {
		return false;
	}
	KVec3 ray_nor;
	if (!ray.dir.getNormalizedSafe(&ray_nor)) {
		return false;
	}
	KVec3 c0 = this->p0 - ray.pos;
	KVec3 c1 = this->p1 - ray.pos;
	KVec3 s = c1 - c0;
	float Dvv = ray_nor.dot(ray_nor);
	float Dsv = ray_nor.dot(s);
	float Dpv = ray_nor.dot(c0);
	float Dss = s.dot(s);
	float Dps = s.dot(c0);
	float Dpp = c0.dot(c0);
	float A = Dvv - Dsv * Dsv / Dss;
	float B = Dpv - Dps * Dsv / Dss;
	float C = Dpp - Dps * Dps / Dss - this->r * this->r;
	float d = B * B - A * C;
	if (d < 0) {
		return false; // 衝突なし
	}
	d = sqrtf(d);
	if (out_near) {
		out_near->dist = (B - d) / A;
		out_near->pos = ray.pos + ray_nor * out_near->dist;
		out_near->normal = KGeom::K_GeomPerpendicularToLineVector(out_near->pos, this->p0, this->p1);
	}
	if (out_far) {
		out_far->dist = (B + d) / A;
		out_far->pos = ray.pos + ray_nor * out_far->dist;
		out_far->normal = KGeom::K_GeomPerpendicularToLineVector(out_far->pos, this->p0, this->p1);
	}
	return true;
}
bool KCylinder::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	KVec3 a = this->p0;
	KVec3 b = this->p1;
	float dist = KGeom::K_GeomDistancePointToLine(sphere.pos, a, b);
	if (dist < 0) return false;
	if (dist < this->r + sphere.r) {
		if (out_dist) *out_dist = dist - this->r;
		if (out_normal) {
			KVec3 delta = KGeom::K_GeomPerpendicularToLineVector(sphere.pos, a, b);
			delta = -delta; // 逆向きに
			if (!delta.getNormalizedSafe(out_normal)) {
				*out_normal = KVec3();
			}
		}
		return true;
	}
	return false;
}
float KCylinder::distance(const KVec3 &p) const {
	return KGeom::K_GeomDistancePointToLine(p, this->p0, this->p1);
}



#pragma endregion // KCylinder



#pragma region KCapsule
KCapsule::KCapsule() {
	this->r = 0;
}
KCapsule::KCapsule(const KVec3 &_p0, const KVec3 &_p1, float _r) {
	this->p0 = _p0;
	this->p1 = _p1;
	this->r = _r;
}
float KCapsule::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc, nullptr)) {
		return desc.dist;
	}
	return -1;
}
bool KCapsule::rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const {
	// レイとカプセルの貫通点
	// http://marupeke296.com/COL_3D_No26_RayToCapsule.html

	#define CHECK(a, b, c) ((a.x - b.x) * (c.x - b.x) + (a.y - b.y) * (c.y - b.y) + (a.z - b.z) * (c.z - b.z))

	if (ray.dir.isZero()) return false;
	if (this->p0 == this->p1) return false;
	if (this->r <= 0) return false;

	const KSphere sphere0(this->p0, this->r); // カプセル p0 側の球体
	const KSphere sphere1(this->p1, this->r); // カプセル p1 側の球体
	const KCylinder cylinder(this->p0, this->p1, this->r); // カプセル円筒形
	
	// 両端の球、円柱（無限長さ）との判定を行う
	KRayDesc ray_near, ray0_far, ray1_far, rayc_far;
	bool hit_p0 = sphere0.rayTest(ray, &ray_near, &ray0_far);
	bool hit_p1 = sphere1.rayTest(ray, &ray_near, &ray1_far);
	bool hit_cy = cylinder.rayTest(ray, &ray_near, &rayc_far);

	// 突入点について
	if (hit_p0 && CHECK(this->p1, this->p0, ray_near.pos) <= 0) {
		// this->p0 側の半球面に突入している
		if (out_near) *out_near = ray_near;

	} else if (hit_p1 && CHECK(this->p0, this->p1, ray_near.pos) <= 0) {
		// this->p1 側の半球面に突入している
		if (out_near) *out_near = ray_near;

	} else if (hit_cy && CHECK(this->p0, this->p1, ray_near.pos) > 0 && CHECK(this->p1, this->p0, ray_near.pos) > 0) {
		// 円柱面に突入している
		if (out_near) *out_near = ray_near;

	} else {
		return false; // 衝突なし
	}

	// 脱出点について調べる
	if (CHECK(this->p1, this->p0, ray0_far.pos) <= 0) {
		// this->p0 側の半球面から脱出している
		// this->p0 の球と判定したときの結果を使う
		if (out_far) *out_far = ray0_far;

	} else if (CHECK(this->p0, this->p1, ray1_far.pos) <= 0) {
		// this->p1 側の半球面から脱出している
		// this->p1 の球と判定したときの結果を使う
		if (out_far) *out_far = ray1_far;

	} else {
		// 円柱面から脱出している
		// 無限長さ円柱と判定したときの結果を使う
		if (out_far) *out_far = rayc_far;
	}
	return true;

	#undef CHECK
}
bool KCapsule::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	KVec3 a = this->p0;
	KVec3 b = this->p1;
	int zone = 0;
	float dist = KGeom::K_GeomDistancePointToLineSegment(sphere.pos, a, b, &zone);
	if (dist < 0) return false;
	if (zone < 0) {
		if (dist < this->r + sphere.r) {
			// a 半球側に衝突
			if (out_dist) *out_dist = dist - this->r;
			if (out_normal) {
				KVec3 delta = sphere.pos - a;
				if (!delta.getNormalizedSafe(out_normal)) {
					*out_normal = KVec3();
				}
			}
			return true;
		}
	}
	if (zone < 0) {
		if (dist < this->r + sphere.r) {
			// b 半球側に衝突
			if (out_dist) *out_dist = dist - this->r;
			if (out_normal) {
				KVec3 delta = sphere.pos - b;
				if (!delta.getNormalizedSafe(out_normal)) {
					*out_normal = KVec3();
				}
			}
			return true;
		}
	}
	if (zone == 0) {
		if (dist < this->r + sphere.r) {
			// 円筒部分に衝突
			if (out_dist) *out_dist = dist - this->r;
			if (out_normal) {
				KVec3 delta = KGeom::K_GeomPerpendicularToLineVector(sphere.pos, a, b);
				delta = -delta; // 逆向きに
				if (!delta.getNormalizedSafe(out_normal)) {
					*out_normal = KVec3();
				}
			}
			return true;
		}
	}
	return false;
}
float KCapsule::distance(const KVec3 &p) const {
	return KGeom::K_GeomDistancePointToCapsule(p, this->p0, this->p1, this->r);
}
#pragma endregion // KCapsule



namespace Test {
void Test_num() {
	K__Verify(KMath::max(0,  0) == 0);
	K__Verify(KMath::max(0, 10) == 10);
	K__Verify(KMath::max(10, 0) == 10);

	K__Verify(KMath::min(0,  0) == 0);
	K__Verify(KMath::min(0, 10) == 0);
	K__Verify(KMath::min(10, 0) == 0);

	K__Verify(KMath::clamp01(-1.0f) == 0.0f);
	K__Verify(KMath::clamp01( 0.0f) == 0.0f);
	K__Verify(KMath::clamp01( 0.5f) == 0.5f);
	K__Verify(KMath::clamp01( 1.0f) == 1.0f);
	K__Verify(KMath::clamp01( 2.0f) == 1.0f);

	K__Verify(KMath::lerp(10, 20,-0.5f) == 10);
	K__Verify(KMath::lerp(10, 20, 0.0f) == 10);
	K__Verify(KMath::lerp(10, 20, 0.5f) == 15);
	K__Verify(KMath::lerp(10, 20, 1.0f) == 20);
	K__Verify(KMath::lerp(10, 20, 1.5f) == 20);

	K__Verify(KMath::lerp(20, 10,-0.5f) == 20);
	K__Verify(KMath::lerp(20, 10, 0.0f) == 20);
	K__Verify(KMath::lerp(20, 10, 0.5f) == 15);
	K__Verify(KMath::lerp(20, 10, 1.0f) == 10);
	K__Verify(KMath::lerp(20, 10, 1.5f) == 10);

	K__Verify(KMath::linearStep(-10, 10,-20) == 0.0f);
	K__Verify(KMath::linearStep(-10, 10,-10) == 0.0f);
	K__Verify(KMath::linearStep(-10, 10,  0) == 0.5f);
	K__Verify(KMath::linearStep(-10, 10, 10) == 1.0f);
	K__Verify(KMath::linearStep(-10, 10, 20) == 1.0f);

	K__Verify(KMath::linearStepBumped(-10, 10,-20) == 0.0f);
	K__Verify(KMath::linearStepBumped(-10, 10,-10) == 0.0f);
	K__Verify(KMath::linearStepBumped(-10, 10, -5) == 0.5f);
	K__Verify(KMath::linearStepBumped(-10, 10,  0) == 1.0f);
	K__Verify(KMath::linearStepBumped(-10, 10,  5) == 0.5f);
	K__Verify(KMath::linearStepBumped(-10, 10, 10) == 0.0f);
	K__Verify(KMath::linearStepBumped(-10, 10, 20) == 0.0f);

	K__Verify(KMath::smoothStep(-10, 10,-20) == 0.0f);
	K__Verify(KMath::smoothStep(-10, 10,-10) == 0.0f);
	K__Verify(KMath::smoothStep(-10, 10,  0) == 0.5f);
	K__Verify(KMath::smoothStep(-10, 10, 10) == 1.0f);
	K__Verify(KMath::smoothStep(-10, 10, 20) == 1.0f);

	K__Verify(KMath::smoothStep(10, -10,-20) == 1.0f);
	K__Verify(KMath::smoothStep(10, -10,-10) == 1.0f);
	K__Verify(KMath::smoothStep(10, -10,  0) == 0.5f);
	K__Verify(KMath::smoothStep(10, -10, 10) == 0.0f);
	K__Verify(KMath::smoothStep(10, -10, 20) == 0.0f);

	K__Verify(KMath::smoothStepBumped(-10, 10,-20) == 0.0f);
	K__Verify(KMath::smoothStepBumped(-10, 10,-10) == 0.0f);
	K__Verify(KMath::smoothStepBumped(-10, 10, -5) == 0.5f);
	K__Verify(KMath::smoothStepBumped(-10, 10,  0) == 1.0f);
	K__Verify(KMath::smoothStepBumped(-10, 10,  5) == 0.5f);
	K__Verify(KMath::smoothStepBumped(-10, 10, 10) == 0.0f);
	K__Verify(KMath::smoothStepBumped(-10, 10, 20) == 0.0f);

	K__Verify(KMath::smoothStepBumped(10, -10,-20) == 0.0f);
	K__Verify(KMath::smoothStepBumped(10, -10,-10) == 0.0f);
	K__Verify(KMath::smoothStepBumped(10, -10, -5) == 0.5f);
	K__Verify(KMath::smoothStepBumped(10, -10,  0) == 1.0f);
	K__Verify(KMath::smoothStepBumped(10, -10,  5) == 0.5f);
	K__Verify(KMath::smoothStepBumped(10, -10, 10) == 0.0f);
	K__Verify(KMath::smoothStepBumped(10, -10, 20) == 0.0f);

	K__Verify(KMath::smoothStep(-10, 10,-20) == 0.0f);
	K__Verify(KMath::smoothStep(-10, 10,-10) == 0.0f);
	K__Verify(KMath::smoothStep(-10, 10,  0) == 0.5f);
	K__Verify(KMath::smoothStep(-10, 10, 10) == 1.0f);
	K__Verify(KMath::smoothStep(-10, 10, 20) == 1.0f);

	K__Verify(KMath::smootherStepBumped(-10, 10,-20) == 0.0f);
	K__Verify(KMath::smootherStepBumped(-10, 10,-10) == 0.0f);
	K__Verify(KMath::smootherStepBumped(-10, 10, -5) == 0.5f);
	K__Verify(KMath::smootherStepBumped(-10, 10,  0) == 1.0f);
	K__Verify(KMath::smootherStepBumped(-10, 10,  5) == 0.5f);
	K__Verify(KMath::smootherStepBumped(-10, 10, 10) == 0.0f);
	K__Verify(KMath::smootherStepBumped(-10, 10, 20) == 0.0f);
}
void Test_math() {
	Test_num();
}
} // Test


} // namespace
