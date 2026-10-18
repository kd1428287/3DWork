#pragma once

#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Components/Physics/Collision/ColliderComponent.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Colliderコンポーネントのparams。実行時(BuildCollider)・各種エディタで共有する唯一の定義
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

// Colliderの1形状分。shapeは"Sphere"/"Capsule"/"Box"(未知の値はBox)。
// Capsuleはoffsetが始点、capsuleEndが終点。マスクはColliderCategoryNames.hの名前の配列で指定する。
struct ColliderShapeParams
{
	std::string name = "shape";
	std::string shape = "Box";

	Math::Vector3 offset = { 0.0f, 0.0f, 0.0f };
	float         radius = 0.5f;
	Math::Vector3 capsuleEnd = { 0.0f, 1.0f, 0.0f };
	Math::Vector3 halfExtents = { 0.5f, 0.5f, 0.5f };

	std::vector<std::string> categoryMask = { "Bump" };
	bool                     useDefaultCollideMask = true;
	std::vector<std::string> collideMask;

	bool enabled = true;
	bool isTrigger = false;
	bool isStatic = false;
	bool wantsStayEvent = false;
};
COMPONENT_PARAMS_DEFINE_TYPE(ColliderShapeParams,
	name, shape, offset, radius, capsuleEnd, halfExtents,
	categoryMask, useDefaultCollideMask, collideMask, enabled, isTrigger, isStatic, wantsStayEvent)

// wireFrameがtrueなら、デバッグ用のWireFrameも一緒に付ける(未追加の場合のみ)。
// ignoreParentがtrueなら、親オブジェクトとの衝突を無視する(武器など)。
struct ColliderParams
{
	std::vector<ColliderShapeParams> shapes;
	std::string                      bodyShapeName;
	bool                             wireFrame = false;
	bool                             ignoreParent = false;
};
COMPONENT_PARAMS_DEFINE_TYPE(ColliderParams, shapes, bodyShapeName, wireFrame, ignoreParent)

// shape名 → ColliderShape(未知の値はBox)
inline ColliderShape ColliderShapeFromName(const std::string& name)
{
	if (name == "Sphere")  return ColliderShape::Sphere;
	if (name == "Capsule") return ColliderShape::Capsule;
	return ColliderShape::Box;
}

// 形状表示用のCollisionShapeEntryを作る。カテゴリ/マスクは未設定(ワイヤー描画専用)
inline CollisionShapeEntry ToPreviewEntry(const ColliderShapeParams& s)
{
	CollisionShapeEntry e;
	e.name = s.name;
	e.shape = ColliderShapeFromName(s.shape);
	e.offset = s.offset;
	e.radius = s.radius;
	e.capsuleEnd = s.capsuleEnd;
	e.halfExtents = s.halfExtents;
	e.enabled = s.enabled;
	e.isTrigger = s.isTrigger;
	e.isStatic = s.isStatic;
	e.wantsStayEvent = s.wantsStayEvent;
	return e;
}
