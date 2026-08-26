#pragma once
#include <string>

// ============================================================
// エフェクト1種類分のパラメータ。EnemyDefinitionと同じ考え方で、
// CSV/JSON等の外部データから読み込んで
// database(std::unordered_map<std::string, EffectDefinition>)を
// 組み立てる想定(EffectFactoryのコンストラクタ参照)。
//
// ※ここで扱うエフェクト用コンポーネント(VisualEffectComponent /
//   EffectLifetimeComponent等)はまだ実装が決まっていないため、
//   フィールド名・型は仮のものです。実装が固まり次第、実際の
//   コンポーネントのAPIに合わせて調整してください。
// ============================================================
struct EffectDefinition
{
	std::string name = "Effect"; // GameObjectの表示名

	// 見た目(パーティクル/VFXモデル)のアセットパス。
	// SkeletonComponent::SetModelData()と同じ要領で、
	// 仮称VisualEffectComponent::SetVfxData()にそのまま渡す想定。
	std::string vfxPath = "Asset/Effects/Placeholder/Placeholder.vfx";

	// エフェクトの生存時間(秒)。
	// 0以下の場合はEffectLifetimeComponentを付与せず、
	// 手動破棄(アニメーション終了イベント等)に任せる想定。
	float lifetime = 1.0f;

	// 再生スケール(見た目の大きさ倍率)。
	float scale = 1.0f;

	// 対象(発生源のGameObject)に追従させるか。
	// 例: ヒットスパークを敵の被弾位置に追従させたい場合等。
	// 現状BuildEffect()側では未使用(将来AttachToSocketComponent等と
	// 組み合わせる余地を残すためのフラグとしてだけ置いてある)。
	bool attachToOwner = false;
};
