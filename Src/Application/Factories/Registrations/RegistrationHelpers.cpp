#include "Application/Factories/Registrations/RegistrationHelpers.h"

#include <algorithm>
#include <stdexcept>

#include "Application/Components/Core/BoneSocketComponent.h"
#include "Application/Components/Graphics/Render/WireFrameComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"

namespace Registration
{
	GameObject* RequireParent(BuildContext& ctx, const char* who)
	{
		if (!ctx.parent) throw std::runtime_error(std::string(who) + " requires a parent object");
		return ctx.parent;
	}

	SkeletonComponent* RequireSkeleton(GameObject* obj, const char* who)
	{
		auto* skeleton = obj->GetComponent<SkeletonComponent>();
		if (!skeleton) throw std::runtime_error(std::string(who) + " requires SkeletonComponent on the parent");
		return skeleton;
	}

	GameObject* CreateBoneSocket(ObjectManager& objectManager, SkeletonComponent* skeleton, const std::string& bone)
	{
		GameObject* socket = objectManager.Instantiate(bone);
		Handle<SkeletonComponent> handle(skeleton);
		socket->AddComponent<BoneSocketComponent>(handle, bone);
		return socket;
	}

	void AddWireFrameOnce(GameObject& obj)
	{
		if (!obj.HasComponent<WireFrameComponent>()) obj.AddComponent<WireFrameComponent>();
	}

	float HorizontalScale(const BuildContext& ctx)
	{
		const Math::Vector3 s = ctx.OwnerScale();
		return std::max(s.x, s.z);
	}
}
