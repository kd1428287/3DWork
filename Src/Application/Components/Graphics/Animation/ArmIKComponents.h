#pragma once
#include "TwoBoneIKComponent.h"

// ============================================================
// TwoBoneIKComponentは中身は同じでも、GameObjectが型ごとに1インスタンス
// までしか持てない制約(GameObject.h参照)があるため、左腕・右腕のように
// 同種のチェーンを同一GameObjectへ同時に付けたい場合はこうして型を
// 分けるだけでよい。コンストラクタ引数(ボーン名)は各キャラクター側の
// セットアップコードで渡すため、ここでは中継するだけ。
//
// SkeletonComponent::PostUpdate()はIAnimationPostProcessタグ経由で
// 具体的な型を知らずに拾うため、新しいチェーンを増やしてもここに
// 数行足すだけでよく、SkeletonComponent側の変更は不要。
// ============================================================
class LeftArmIKComponent : public TwoBoneIKComponent {
public:
	using TwoBoneIKComponent::TwoBoneIKComponent;
};

class RightArmIKComponent : public TwoBoneIKComponent {
public:
	using TwoBoneIKComponent::TwoBoneIKComponent;
};
