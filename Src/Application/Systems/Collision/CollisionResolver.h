#pragma once

#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Components/Physics/Movement/MovementResolverComponent.h"

// 押し返し(位置補正)の「集約」と「適用」だけを担当するクラス

// 1件の接触が、片方のコライダーに要求する押し出し量。
struct PushContact
{
	ColliderComponent* collider = nullptr; // 押される側
	Math::Vector3 normal;                  // 押し出す向き(正規化済み)
	float depth = 0.0f;                    // その向きに押し出すべき量
};

class CollisionResolver
{
public:
	// CollisionSystem::Update()の検出ループが、押し返しが必要な重なりを
	// 見つけるたびに呼ぶ。depth<=0は無視する。
	void AddContact(ColliderComponent* collider, const Math::Vector3& normal, float depth)
	{
		if (collider == nullptr || depth <= 0.0f) return;
		contacts_.push_back({ collider, normal, depth });
	}

	// 全ペアの検出が終わった後に1回呼ぶ。コライダーごとに接触を集約し、
	// MovementResolverComponentがあればApplyCollisionCorrection()へ、
	// 無ければ直接Translate()する(MovementResolverを持たない単純な
	// オブジェクト向けのフォールバック)。
	void Resolve()
	{
		std::unordered_map<ColliderComponent*, Math::Vector3> corrections;

		for (const PushContact& contact : contacts_) {
			Math::Vector3& total = corrections[contact.collider];

			// 既にこの向きへどれだけ押し出し済みかを見て、不足分だけ足す。
			const float already = total.Dot(contact.normal);
			const float remaining = contact.depth - already;
			if (remaining > 0.0f) {
				total += contact.normal * remaining;
			}
		}

		for (auto& [collider, delta] : corrections) {
			if (delta.LengthSquared() <= 1e-10f) continue;

			if (MovementResolverComponent* moveResolver =
				collider->GetOwner()->GetComponent<MovementResolverComponent>()) {
				moveResolver->ApplyCollisionCorrection(delta);
			}
			else {
				collider->Translate(delta);
			}
		}

		contacts_.clear();
	}

private:
	std::vector<PushContact> contacts_;
};
