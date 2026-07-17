#pragma once

#include "../../Core/Handle.h"
#include "TransformComponent.h"

// ============================================================
// SocketComponent: GameObjectにぶら下がる純粋な階層ノード。
//
// 役割は「親のTransformComponentを介した行列合成」のみに専念させる。
// 「誰かをこのソケットに追従させる」橋渡しの責務は持たない
// (それはAttachToSocketComponent側の仕事)。
//
// 親への参照はHandle<TransformComponent>で保持し、GetVersion()/
// RefreshCacheIfNeeded()の呼び出しのたびに毎回Resolve()し直す。
// 生ポインタをメンバにキャッシュしない(=Start()で一度だけ解決して
// 使い回す、ということをしない)のは、以下の2つの事故を避けるため:
//   1. 親が破棄された後もdangling pointerを踏み続けてしまう(UAF)
//   2. GetParentHandle()経由で外部が親を差し替えても反映されない
// ============================================================
class SocketComponent : public TransformComponent {
public:
	explicit SocketComponent(GameObject* owner, Handle<TransformComponent>& handle)
		: TransformComponent(owner), handle_(handle) {}

	// SocketComponentを明示的に破棄するとき(=依存を解除したいとき)
	// 自身のワールド座標を渡したTransformComponentをアタッチする
	void OnDestroy()override {
		Math::Vector3 position;
		Math::Quaternion rotation;
		Math::Vector3 scale;
		if (GetWorldMatrix().Decompose(position, rotation, scale)) {
			GetOwner()->RequestAddComponent<TransformComponent>(position, rotation, scale);
		}
	}

	Handle<TransformComponent>& GetParentHandle() { return handle_; }

protected:

	// 親を毎回Resolve()する。親が破棄されていればnullptrが返るため、
	// 「破棄済みの親を指したまま」というUAF状態には陥らない。
	uint32_t GetVersion() const override {
		TransformComponent* parent = handle_.Resolve();
		return localVersion_ + (parent ? parent->GetVersion() : 0);
	}

	void RefreshCacheIfNeeded() const override {
		const uint32_t currentVersion = GetVersion();
		if (currentVersion == cachedVersion_) {
			return;
		}

		TransformComponent* parent = handle_.Resolve();

		Math::Matrix parentUnscaled = Math::Matrix::Identity;
		if (parent != nullptr) {
			parentUnscaled = parent->GetUnscaledMatrix();
		}

		cachedUnscaledMatrix_ =
			Math::Matrix::CreateFromQuaternion(rotation_)
			* Math::Matrix::CreateTranslation(position_)
			* parentUnscaled;

		cachedWorldMatrix_ =
			Math::Matrix::CreateScale(scale_)
			* Math::Matrix::CreateFromQuaternion(rotation_)
			* Math::Matrix::CreateTranslation(position_)
			* parentUnscaled;

		cachedVersion_ = currentVersion;
	}

private:
	Handle<TransformComponent> handle_;
};