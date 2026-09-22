#pragma once

enum class RootMotionAxis { X, Y, Z };

// ルートモーション抽出の設定。まとめて渡せるのでデータから読み込みやすい。
struct RootMotionConfig
{
	std::string		boneName;                        // 抽出元ボーン(通常はHip/Root)。空なら無効
	float			unitScale = 1.0f;                // 抽出した移動量に掛ける倍率
	RootMotionAxis	forwardAxis = RootMotionAxis::Z; // ボーンのローカル空間で「前後」にあたる軸
	float			forwardSign = 1.0f;
	RootMotionAxis	rightAxis = RootMotionAxis::X;   // 同じく「左右」にあたる軸
	float			rightSign = 1.0f;
	bool			extractRotation = false;         // Yaw回転も抽出するか
	float			yawSign = 1.0f;
};

class RootMotionExtractor
{
public:
	// 設定を差し替える。ボーン名が変わったときだけボーンを再解決する。
	void SetConfig(const RootMotionConfig& config) {
		if (config.boneName != config_.boneName) { InvalidateBone(); }
		config_ = config;
		RequestResync(FrameKind::Relock);
	}
	const RootMotionConfig& GetConfig() const { return config_; }

	bool IsEnabled() const { return active_ && !config_.boneName.empty(); }
	bool IsExtractingRotation() const { return config_.extractRotation; }

	// 実行時のON/OFF切替。ボーン名などの設定は保持され、再ONで固定位置を取り直す。
	void SetActive(bool active) {
		if (active && !active_) { RequestResync(FrameKind::Relock); }
		active_ = active;
	}

	// 回転抽出だけを切り替える。OFF→ONで直前の向きを取り直す。
	void SetExtractRotation(bool enabled) {
		if (enabled && !config_.extractRotation) { RequestResync(FrameKind::Rebase); }
		config_.extractRotation = enabled;
	}

	// アニメーションが切り替わったことを通知する。
	// continuesClipがtrueなら同じクリップの続き(フェーズ切替)として、固定位置を維持する。
	void NotifyAnimationChanged(bool continuesClip) {
		RequestResync(continuesClip ? FrameKind::Rebase : FrameKind::Relock);
	}

	// ボーンポインタを破棄する。モデル差し替え後に呼ぶ。
	void InvalidateBone() {
		node_ = nullptr;
		boneResolved_ = false;
		RequestResync(FrameKind::Relock);
	}

	// AdvanceTime前に呼ぶ。ボーンの解決だけを行う。
	void PrepareFrame(KdModelWork& model) {
		if (config_.boneName.empty() || boneResolved_) return;

		node_ = model.FindWorkNode(config_.boneName);
		boneResolved_ = true;
	}

	// AdvanceTime直後に呼ぶ。wrappedはループで先頭へ巻き戻ったフレームかどうか。
	// 移動量・Yaw量を蓄積し、ボーンの該当成分を固定位置/向きに固定する。
	void FinalizeFrame(bool wrapped) {
		if (!active_ || node_ == nullptr) return;

		FrameKind kind = pendingKind_;
		if (kind == FrameKind::Normal && wrapped) { kind = FrameKind::Wrapped; }
		pendingKind_ = FrameKind::Normal;

		if (kind == FrameKind::Relock) {
			positionLocked_ = false;
			rotationLocked_ = false;
		}

		ExtractTranslation(kind);
		if (config_.extractRotation) {
			ExtractYaw(kind);
		}
	}

	// 開始座標(区間ロック時のボーンローカル位置)を原点とした、現在までの累積移動量
	Math::Vector3 GetPositionFromStart() const {
		if (node_ == nullptr || !positionLocked_) { return Math::Vector3::Zero; }

		const Math::Vector3 raw = node_->m_localTransform.Translation();
		const float forward =
			(GetAxis(raw, config_.forwardAxis) - GetAxis(startLocalPos_, config_.forwardAxis)) * config_.forwardSign;
		const float right =
			(GetAxis(raw, config_.rightAxis) - GetAxis(startLocalPos_, config_.rightAxis)) * config_.rightSign;
		return Math::Vector3(right, 0.0f, forward) * config_.unitScale;
	}

	// 蓄積された移動量(ボーンのローカル基準)を取り出す。呼ぶと0に戻る。
	Math::Vector3 ConsumeDelta() {
		const Math::Vector3 d = delta_;
		delta_ = Math::Vector3::Zero;
		return d;
	}

	// 蓄積されたYaw量を取り出す。呼ぶと0に戻る。
	float ConsumeYawDelta() {
		const float d = yawDelta_;
		yawDelta_ = 0.0f;
		return d;
	}

private:
	// フレームの扱い。値が大きいほど優先(RequestResyncで強い方が残る)。
	// Normal: 通常 / Wrapped: ループ巻き戻り(差分は捨てる)
	// Rebase: 同一クリップの続き(直前位置だけ取り直し、固定位置は維持)
	// Relock: 別クリップ・設定変更(固定位置も現在に取り直す)
	enum class FrameKind { Normal, Wrapped, Rebase, Relock };

	void RequestResync(FrameKind kind) {
		if (kind > pendingKind_) { pendingKind_ = kind; }
	}

	void ExtractTranslation(FrameKind kind) {
		const Math::Vector3 raw = node_->m_localTransform.Translation();

		if (!positionLocked_) {
			startLocalPos_ = raw; // この区間の原点(開始座標)として記録
			lockedLocalPos_ = raw;
			positionLocked_ = true;
		}
		else if (kind == FrameKind::Normal) {
			const float forward =
				(GetAxis(raw, config_.forwardAxis) - GetAxis(lastLocalPos_, config_.forwardAxis)) * config_.forwardSign;
			const float right =
				(GetAxis(raw, config_.rightAxis) - GetAxis(lastLocalPos_, config_.rightAxis)) * config_.rightSign;
			delta_ += Math::Vector3(right, 0.0f, forward) * config_.unitScale;
		}
		lastLocalPos_ = raw;

		Math::Vector3 locked = raw;
		SetAxis(locked, config_.forwardAxis, GetAxis(lockedLocalPos_, config_.forwardAxis));
		SetAxis(locked, config_.rightAxis, GetAxis(lockedLocalPos_, config_.rightAxis));
		node_->m_localTransform.Translation(locked);
	}

	void ExtractYaw(FrameKind kind) {
		Math::Matrix matrix = node_->m_localTransform;
		Math::Vector3 scale, trans;
		Math::Quaternion rot;
		matrix.Decompose(scale, rot, trans);

		const Math::Vector3 rawForward = Math::Vector3::Transform(Math::Vector3::Forward, rot);

		if (!rotationLocked_) {
			lockedLocalRot_ = rot;
			rotationLocked_ = true;
		}
		else if (kind == FrameKind::Normal) {
			const Math::Vector3 lastForward = Math::Vector3::Transform(Math::Vector3::Forward, lastLocalRot_);
			yawDelta_ += ComputeHorizontalAngleTo(lastForward, rawForward) * config_.yawSign;
		}
		lastLocalRot_ = rot;

		// 固定向きとの水平方向のズレを打ち消して、Yawを固定する
		const Math::Vector3 lockedForward = Math::Vector3::Transform(Math::Vector3::Forward, lockedLocalRot_);
		const float driftYaw = ComputeHorizontalAngleTo(rawForward, lockedForward);
		const Math::Quaternion correction = Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, driftYaw);
		const Math::Quaternion lockedRot = correction * rot;

		node_->m_localTransform = Math::Matrix::CreateScale(scale)
			* Math::Matrix::CreateFromQuaternion(lockedRot)
			* Math::Matrix::CreateTranslation(trans);
	}

	static float GetAxis(const Math::Vector3& v, RootMotionAxis axis) {
		switch (axis) {
		case RootMotionAxis::X: return v.x;
		case RootMotionAxis::Y: return v.y;
		case RootMotionAxis::Z: return v.z;
		}
		return 0.0f;
	}

	static void SetAxis(Math::Vector3& v, RootMotionAxis axis, float value) {
		switch (axis) {
		case RootMotionAxis::X: v.x = value; break;
		case RootMotionAxis::Y: v.y = value; break;
		case RootMotionAxis::Z: v.z = value; break;
		}
	}

	RootMotionConfig	config_;

	KdModelWork::Node* node_ = nullptr;
	bool				active_ = true;
	bool				boneResolved_ = false;
	FrameKind			pendingKind_ = FrameKind::Normal; // 次のFinalizeFrameで行う取り直し

	Math::Vector3		startLocalPos_{};
	Math::Vector3		startLocalRot_{};

	// 並進: 直前フレームの生の位置 / 固定する位置 / 蓄積された移動量
	Math::Vector3		lastLocalPos_{};
	Math::Vector3		lockedLocalPos_{};
	Math::Vector3		delta_{};
	bool				positionLocked_ = false;

	// 回転: 直前フレームの生の向き / 固定する向き / 蓄積されたYaw量
	Math::Quaternion	lastLocalRot_{};
	Math::Quaternion	lockedLocalRot_{};
	float				yawDelta_ = 0.0f;
	bool				rotationLocked_ = false;
};