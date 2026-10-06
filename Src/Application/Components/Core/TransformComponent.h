#pragma once

// 位置情報を管理
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

	// 自身のSetPosition/Rotation/Scale等で進む変更カウンタ(他コンポーネントのキャッシュ無効化用)。
	uint32_t GetLocalVersion() const { return localVersion_; }

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

	// ---------------------------------------------------------------
	// ワールドスケール（ワールド行列から取り出すので、子クラスのスケール伝播も含む）
	// GetScale()はローカル値。他コンポーネントが使うのはこちら。
	// ---------------------------------------------------------------

	Math::Vector3 GetWorldScale() const {
		Math::Vector3 scale;
		Math::Quaternion rotation;
		Math::Vector3 translation;
		// 分解に失敗したらローカルスケールで代用する(0スケールを返さない)。
		if (!GetWorldMatrix().Decompose(scale, rotation, translation)) scale = scale_;
		return { std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) };
	}

	// 球/カプセル半径など、一様スケールで近似したい用途向け（最大成分）。
	float GetMaxWorldScale() const {
		const Math::Vector3 s = GetWorldScale();
		return std::max({ s.x, s.y, s.z });
	}

	// 水平移動量の拡大率（XZの大きい方）。ルートモーションやステップ距離の補正用。
	float GetHorizontalWorldScale() const {
		const Math::Vector3 s = GetWorldScale();
		return std::max(s.x, s.z);
	}

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
};