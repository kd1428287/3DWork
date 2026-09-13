#pragma once

// Handle<T>の宣言場所に合わせてinclude/前方宣言を調整してください
template <typename T> class Handle;

class GameObject;
class ObjectManager;
class SkeletonComponent;

// ============================================================
// Player/Enemy等、複数キャラクターのFactoryから共通して使う
// GameObject組み立て処理をまとめたユーティリティ。
// 特定キャラクター種別の知識は持たず、ここに置くのは
// 「ボーンソケット用の空GameObjectを作る」等、汎用的な処理のみ。
// ============================================================
namespace CharacterFactoryUtil
{
	// boneNameのボーンに追従するBoneSocketComponentだけを持つGameObjectを生成する。
	GameObject* CreateSocket(ObjectManager& objectManager, const std::string& objID, Handle<SkeletonComponent>& handle);
}
