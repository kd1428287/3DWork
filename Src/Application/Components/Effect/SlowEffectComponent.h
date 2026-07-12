#pragma once

#include "ComponentBase.h"
#include "../Objects/GameObject.h"
#include "../Objects/SceneContext.h"

// ============================================================
// 対象のtimeScale_を一定時間だけ下げる「スロウ」効果。
// アタッチされている間だけ効果が有効で、時間切れで自動的に
// 自分自身を外し、元のtimeScaleに復元する。
//
// PoisonComponentと同型の「時限コンポーネント」パターン:
//   Awake()   … 効果を適用
//   Update()  … 残り時間を減算し、切れたら自身の削除を予約
//   OnDestroy() … 効果を元に戻す(削除経路によらず必ず呼ばれる)
// ============================================================
class SlowEffectComponent : public ComponentBase {
public:
	SlowEffectComponent(GameObject* owner, float slowScale, float duration)
		: ComponentBase(owner), slowScale_(slowScale), duration_(duration) {}

	void Awake() override {
		// 元の値を保存してから適用する
		previousTimeScale_ = GetOwner()->GetTimeScale();
		GetOwner()->SetTimeScale(slowScale_);
	}

	void Update(float /*deltaTime*/) override {
		// 自分自身の持続時間は「スケールされていない」経過時間で数える。
		// (このオブジェクト自体が遅くなっていても、効果はちゃんと実時間で切れる)
		elapsed_ += GetOwner()->GetContext()->unscaledDeltaTime;

		if (elapsed_ >= duration_) {
			// Update走査中の自己削除はRequestRemoveComponentで予約する
			// (直接RemoveComponentを呼ぶとcomponentOrder_のイテレータが壊れる)
			GetOwner()->RequestRemoveComponent<SlowEffectComponent>();
		}
	}

	void OnDestroy() override {
		// 効果終了(または途中でRemoveされた)時に元の速度へ戻す
		GetOwner()->SetTimeScale(previousTimeScale_);
	}

private:
	float slowScale_ = 1.0f;
	float duration_ = 0.0f;
	float elapsed_ = 0.0f;
	float previousTimeScale_ = 1.0f;
};
