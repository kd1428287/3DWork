#pragma once
#include "../Transform/TransformComponent.h"
#include "../../Effect/SlashTrailEvents.h"

class SlashTrailComponent : public ComponentBase, public IPolygonRenderSource {
public:
	explicit SlashTrailComponent(GameObject* owner, const std::string& baseColTexName = "")
		: ComponentBase(owner)
	{}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void Update(float dt)override
	{
		if (!emitting_ || transform_ == nullptr)return;

		PublishSlashTrailPositionUpdate(
			*GetOwner()->GetContext()->eventBus, instantKey_, tip_, base_);
	}

	void SetBaseTip(Math::Vector3 base, Math::Vector3 tip) 
	{
		base_ = base; 
		tip_ = tip; 
	}

	void SetKey(std::string key) { instantKey_ = key; }

	void StartTrail() {};
	void EndTrail() {};

private:
	TransformComponent* transform_ = nullptr;

	std::string instantKey_ = "";
	Math::Vector3 base_{};
	Math::Vector3 tip_{};

	bool emitting_ = false;
};