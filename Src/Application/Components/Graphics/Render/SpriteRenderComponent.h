#pragma once

class SpriteRenderComponent : public ComponentBase {
public:
	explicit SpriteRenderComponent(GameObject* owner) :ComponentBase(owner) {}

	void Awake() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

private:
	TransformComponent* transform_ = nullptr;
	std::unique_ptr<KdTexture> texture_ = nullptr;
};