#include "PlayerStatusUIComponent.h"
#include "../../Character/Data/HealthComponent.h"
#include "../../Character/Data/PostureComponent.h"

void PlayerStatusUIComponent::Start()
{
	// HealthComponentは必須想定。取得できない場合は設計ミスの可能性が
	// 高いため、assert等で早期に気づけるようにしてもよい。
	health_ = GetOwner()->GetComponent<HealthComponent>();

	// PostureComponentは任意(ガード/パリィを行わないキャラクターは
	// アタッチしなくてよい想定)。取れなければ体幹バーは描画しない。
	posture_ = GetOwner()->GetComponent<PostureComponent>();
}

void PlayerStatusUIComponent::Update(float deltaTime)
{
	if (health_) {
		GaugeBarRenderer::Draw(healthBarPos_, healthBarSize_, health_->GetRatio(), healthStyle_);
	}

	if (posture_) {
		GaugeBarRenderer::Draw(postureBarPos_, postureBarSize_, posture_->GetRatio(), postureStyle_);
	}
}