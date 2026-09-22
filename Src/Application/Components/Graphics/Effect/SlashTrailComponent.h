#pragma once
#include "Application/Core/EventBus/Events/SlashTrailEvents.h"

// 初期設定(Prefab/JSONから渡す値)。base/tipはローカル座標。emitOnStartがtrueならAwakeで発光を開始する。
struct SlashTrailConfig
{
	std::string   trailName;
	Math::Vector3 base = { 0.0f, 0.0f, 0.0f };
	Math::Vector3 tip = { 0.0f, 0.0f, 0.0f };
	std::string   key;
	bool          emitOnStart = false;
};

class SlashTrailComponent : public ComponentBase {
public:
	using Config = SlashTrailConfig;

	explicit SlashTrailComponent(GameObject* owner, const std::string& trailName = "")
		: ComponentBase(owner), trailName_(trailName)
	{
	}

	void SetConfig(const Config& config)
	{
		trailName_ = config.trailName;
		SetBaseTip(config.base, config.tip);
		SetKey(config.key);
		emitOnStart_ = config.emitOnStart;
	}

	void Awake() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (emitOnStart_) StartEmit();
	}

	void Update(float dt)override
	{
		if (!emitting_ || transform_ == nullptr)return;

		// ローカル座標のTip/Baseをワールド座標に変換してから送る
		const Math::Matrix& worldMat = transform_->GetWorldMatrix();
		Math::Vector3 worldTip = Math::Vector3::Transform(tip_, worldMat);
		Math::Vector3 worldBase = Math::Vector3::Transform(base_, worldMat);

		PublishSlashTrailPositionUpdate(
			*GetOwner()->GetContext()->eventBus, instanceKey_, worldTip, worldBase);
	}

	void SetBaseTip(Math::Vector3 base, Math::Vector3 tip)
	{
		base_ = base;
		tip_ = tip;
	}

	void SetKey(std::string key) { instanceKey_ = key; }

	void StartEmit()
	{
		if (emitting_) return;
		emitting_ = true;
		PublishSlashTrailBegin(*GetOwner()->GetContext()->eventBus, instanceKey_, trailName_);
	}

	void StopEmit()
	{
		if (!emitting_) return;
		emitting_ = false;
		PublishSlashTrailEnd(*GetOwner()->GetContext()->eventBus, instanceKey_);
	}

private:
	TransformComponent* transform_ = nullptr;

	std::string instanceKey_ = "";
	std::string trailName_ = "";
	Math::Vector3 base_{};
	Math::Vector3 tip_{};

	bool emitting_ = false;
	bool emitOnStart_ = false;
};