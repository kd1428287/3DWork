#pragma once

// Math::Vector3 / Math::Quaternion / Math::Matrix は既存のMath名前空間の型を想定
//
// 親子関係のアタッチ/デタッチ判断（循環参照チェック、
// ワールド座標を維持したままの付け替え、ソケットとの同期など）は
// このクラスの責務にせず、外部の AttachmentSystem / SocketSystem 側で行う。
// このクラスは「今の親が誰か」を保持し、行列を合成するだけに専念する。

class TransformComponent : public ComponentBase {
public:
	explicit TransformComponent(GameObject* owner, Math::Vector3 pos = {}, Math::Quaternion rot = {}, Math::Vector3 scale = { 1.f,1.f,1.f })
		: ComponentBase(owner), position_(pos), rotation_(rot), scale_(scale) {};

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

	// ---------------------------------------------------------------
	// ゲッター
	// ---------------------------------------------------------------

	const Math::Vector3& GetPosition() const { return position_; }
	const Math::Quaternion& GetRotation() const { return rotation_; }
	const Math::Vector3& GetScale()    const { return scale_; }

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

protected:
	void MarkDirty() {
		++localVersion_;
	}

	virtual uint32_t GetVersion() const {
		return localVersion_;
	}

	virtual void RefreshCacheIfNeeded() const {
		const uint32_t currentVersion = GetVersion();
		if (currentVersion == cachedVersion_) {
			return;
		}

		cachedUnscaledMatrix_ =
			Math::Matrix::CreateFromQuaternion(rotation_)
			* Math::Matrix::CreateTranslation(position_);
		
		cachedWorldMatrix_ =
			Math::Matrix::CreateScale(scale_)
			* Math::Matrix::CreateFromQuaternion(rotation_)
			* Math::Matrix::CreateTranslation(position_);

		cachedVersion_ = currentVersion;
	}

	Math::Vector3    position_{ 0.0f, 0.0f, 0.0f };
	Math::Quaternion rotation_ = Math::Quaternion::Identity;
	Math::Vector3    scale_{ 1.0f, 1.0f, 1.0f };

	mutable Math::Matrix cachedWorldMatrix_ = Math::Matrix::Identity;
	mutable Math::Matrix cachedUnscaledMatrix_ = Math::Matrix::Identity;
	mutable uint32_t     cachedVersion_ = 0xFFFFFFFFu; // 初回は必ず再計算させる
	uint32_t             localVersion_ = 0;

	friend class SocketComponent;
};