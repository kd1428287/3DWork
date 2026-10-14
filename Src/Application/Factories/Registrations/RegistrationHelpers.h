#pragma once

#include "Application/Factories/ComponentRegistry.h"

class SkeletonComponent;

// 複数のRegister*.cppで共有する、登録用のヘルパー。
namespace Registration
{
	// ctxの親オブジェクトを返す。親がいなければ例外にする(whoはエラーメッセージ用の名前)。
	GameObject* RequireParent(BuildContext& ctx, const char* who);

	// objのSkeletonComponentを返す。無ければ例外にする。
	SkeletonComponent* RequireSkeleton(GameObject* obj, const char* who);

	// boneのボーンに追従するBoneSocketComponentだけを持つオブジェクトを生成する。
	GameObject* CreateBoneSocket(ObjectManager& objectManager, SkeletonComponent* skeleton, const std::string& bone);

	// WireFrameComponentが未追加の場合だけ追加する。
	void AddWireFrameOnce(GameObject& obj);

	// 生成時のスケールのうち水平(XZ)の拡大率。速度や水平距離をモデルの大きさに合わせる。
	float HorizontalScale(const BuildContext& ctx);
}
