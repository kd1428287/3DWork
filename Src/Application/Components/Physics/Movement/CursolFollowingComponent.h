#pragma once

class CursolFollowingComponent : public ComponentBase {
public:
	explicit CursolFollowingComponent(GameObject* owner) : ComponentBase(owner) {};

	void Awake() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void Update(float dt) override {

	}

public:
	TransformComponent* transform_ = nullptr;

};