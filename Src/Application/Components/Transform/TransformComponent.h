#pragma once

#include <cstdint>

// Math::Vector3 / Math::Quaternion / Math::Matrix は既存のMath名前空間の型を想定
//
// 親子関係のアタッチ/デタッチ判断（循環参照チェック、
// ワールド座標を維持したままの付け替え、ソケットとの同期など）は
// このクラスの責務にせず、外部の AttachmentSystem / SocketSystem 側で行う。
// このクラスは「今の親が誰か」を保持し、行列を合成するだけに専念する。

class TransformComponent : public ComponentBase {
public:
	explicit TransformComponent(GameObject* owner, TransformComponent* parent = nullptr)
		: ComponentBase(owner), parent_(parent) {}

	// ---------------------------------------------------------------
	// セッター（値を変更する唯一の入口。すべて MarkDirty() を通す）
	// ---------------------------------------------------------------

	void SetPosition(const Math::Vector3& position) {
		position_ = position;
		MarkDirty();
	}

	void SetRotation(const Math::Quaternion& rotation) {
		rotation_ = rotation;
		MarkDirty();
	}

	void SetScale(const Math::Vector3& scale) {
		scale_ = scale;
		MarkDirty();
	}

	// 親の差し替え。ローカル座標はそのまま（ワールド座標は変わりうる）。
	// ワールド座標を維持したい付け替えや循環参照のチェックは
	// 呼び出し側（AttachmentSystem等）の責務とする。
	void SetParent(TransformComponent* parent) {
		parent_ = parent;
		MarkDirty();
	}

	// ---------------------------------------------------------------
	// ゲッター
	// ---------------------------------------------------------------

	const Math::Vector3& GetPosition() const { return position_; }
	const Math::Quaternion& GetRotation() const { return rotation_; }
	const Math::Vector3& GetScale()    const { return scale_; }
	TransformComponent* GetParent()   const { return parent_; }

	// ---------------------------------------------------------------
	// 差分操作（頻出するため用意）
	// ---------------------------------------------------------------

	void Translate(const Math::Vector3& delta) {
		position_ += delta;
		MarkDirty();
	}

	void Rotate(const Math::Quaternion& delta) {
		rotation_ = delta * rotation_;
		MarkDirty();
	}

	// ---------------------------------------------------------------
	// 行列取得（バージョン番号方式でキャッシュ）
	// ---------------------------------------------------------------

	Math::Matrix GetWorldMatrix() const {
		RefreshCacheIfNeeded();
		return cachedWorldMatrix_;
	}

	// 親のスケールを無視した行列（子へのスケール伝播を避けたい場合に使用）
	Math::Matrix GetUnscaledMatrix() const {
		RefreshCacheIfNeeded();
		return cachedUnscaledMatrix_;
	}

	Math::Vector3 GetForward() const { return Math::Vector3::Transform(Math::Vector3::Forward, rotation_); }
	Math::Vector3 GetUp()      const { return Math::Vector3::Transform(Math::Vector3::Up, rotation_); }
	Math::Vector3 GetRight()   const { return Math::Vector3::Transform(Math::Vector3::Right, rotation_); }

private:
	void MarkDirty() {
		++localVersion_;
	}

	// 自分＋親チェーン全体のバージョンを合算した値
	// （親か自分のどちらかが変わればこの値が変化する）
	uint32_t GetVersion() const {
		return localVersion_ + (parent_ != nullptr ? parent_->GetVersion() : 0);
	}

	void RefreshCacheIfNeeded() const {
		const uint32_t currentVersion = GetVersion();
		if (currentVersion == cachedVersion_) {
			return;
		}

		Math::Matrix parentUnscaled = Math::Matrix::Identity;
		if (parent_ != nullptr) {
			parentUnscaled = parent_->GetUnscaledMatrix();
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

	TransformComponent* parent_ = nullptr;

	Math::Vector3    position_{ 0.0f, 0.0f, 0.0f };
	Math::Quaternion rotation_ = Math::Quaternion::Identity;
	Math::Vector3    scale_{ 1.0f, 1.0f, 1.0f };

	mutable Math::Matrix cachedWorldMatrix_ = Math::Matrix::Identity;
	mutable Math::Matrix cachedUnscaledMatrix_ = Math::Matrix::Identity;
	mutable uint32_t     cachedVersion_ = 0xFFFFFFFFu; // 初回は必ず再計算させる
	uint32_t             localVersion_ = 0;
};