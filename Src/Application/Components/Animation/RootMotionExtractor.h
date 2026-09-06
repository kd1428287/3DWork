#pragma once

enum class RootMotionAxis { X, Y, Z };

class RootMotionExtractor
{
public:
	// 抽出元にするボーン名(通常はHip/Root)を指定する。空文字で無効化。
	void SetBoneName(std::string_view name) {
		boneName_ = name;
		node_ = nullptr;
		boneResolved_ = false;
		needsResync_ = true;
	}

	bool IsEnabled() const { return !boneName_.empty(); }
	void SetUnitScale(float scale) { unitScale_ = scale; }

	void SetForwardAxis(RootMotionAxis axis, float sign = 1.0f) {
		forwardAxis_ = axis;
		forwardSign_ = sign;
	}

	void SetRightAxis(RootMotionAxis axis, float sign = 1.0f) {
		rightAxis_ = axis;
		rightSign_ = sign;
	}

	// アニメーションが切り替わったことを通知する。
	void NotifyAnimationChanged() { needsResync_ = true; }

	// ボーンの解決だけを行う(基準位置の取り直しはここでは行わない。
	void PrepareFrame(KdModelWork& model) {
		if (boneName_.empty()) return;

		if (!boneResolved_) {
			node_ = model.FindWorkNode(boneName_);
			boneResolved_ = true;
		}
	}

	void FinalizeFrame(float timeBeforeAdvance, float timeAfterAdvance) {
		if (node_ == nullptr) return;

		const Math::Vector3 rawLocalPos = node_->m_localTransform.Translation();

		if (needsResync_) {
			// アニメーションが切り替わった直後の最初のフレーム。
			delta_ = Math::Vector3::Zero;
			lockedLocalPos_ = rawLocalPos;
			needsResync_ = false;
		}
		else if (timeAfterAdvance < timeBeforeAdvance) {
			// ループして先頭に巻き戻った瞬間
			delta_ = Math::Vector3::Zero;
		}
		else {
			const float forwardDelta =
				(GetAxis(rawLocalPos, forwardAxis_) - GetAxis(lastLocalPos_, forwardAxis_)) * forwardSign_;
			const float rightDelta =
				(GetAxis(rawLocalPos, rightAxis_) - GetAxis(lastLocalPos_, rightAxis_)) * rightSign_;
			delta_ = Math::Vector3(rightDelta, 0.0f, forwardDelta) * unitScale_;
		}
		lastLocalPos_ = rawLocalPos;

		Math::Vector3 lockedRaw = rawLocalPos;
		SetAxis(lockedRaw, forwardAxis_, GetAxis(lockedLocalPos_, forwardAxis_));
		SetAxis(lockedRaw, rightAxis_, GetAxis(lockedLocalPos_, rightAxis_));
		node_->m_localTransform.Translation(lockedRaw);

		if (extractRotation_) {
			Math::Matrix rawMatrix = node_->m_localTransform; 
			Math::Vector3 rawScale, rawTrans;
			Math::Quaternion rawRot;
			rawMatrix.Decompose(rawScale, rawRot, rawTrans);

			const Math::Vector3 lastForward = Math::Vector3::Transform(Math::Vector3::Forward, lastLocalRot_);
			const Math::Vector3 rawForward = Math::Vector3::Transform(Math::Vector3::Forward, rawRot);

			if (needsResync_) {
				yawDelta_ = 0.0f;
				lockedLocalRot_ = rawRot; 
			}
			else if (timeAfterAdvance < timeBeforeAdvance) {
				yawDelta_ = 0.0f; 
			}
			else {
				yawDelta_ = ComputeHorizontalAngleTo(lastForward, rawForward) * rotationSign_;
			}
			lastLocalRot_ = rawRot;

			const Math::Vector3 lockedForward = Math::Vector3::Transform(Math::Vector3::Forward, lockedLocalRot_);
			const float driftYaw = ComputeHorizontalAngleTo(rawForward, lockedForward);
			const Math::Quaternion correction = Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, driftYaw);
			const Math::Quaternion lockedRot = correction * rawRot;

			node_->m_localTransform = Math::Matrix::CreateScale(rawScale)
				* Math::Matrix::CreateFromQuaternion(lockedRot)
				* Math::Matrix::CreateTranslation(rawTrans);
		}
		else {
			yawDelta_ = 0.0f;
		}
	}
	
	Math::Vector3 ConsumeDelta() {
		Math::Vector3 d = delta_;
		delta_ = Math::Vector3::Zero;
		return d;
	}

	float ConsumeYawDelta() {
		float d = yawDelta_;
		yawDelta_ = 0.0f;
		return d;
	}

	void SetRotationSign(float sign) { rotationSign_ = sign; }

	void SetExtractRotation(bool enabled) { extractRotation_ = enabled; }
	bool IsExtractingRotation() const { return extractRotation_; }

private:
	// v(ボーンのローカル並進ベクトル)から、指定した軸の値を取り出す。
	static float GetAxis(const Math::Vector3& v, RootMotionAxis axis) {
		switch (axis) {
		case RootMotionAxis::X: return v.x;
		case RootMotionAxis::Y: return v.y;
		case RootMotionAxis::Z: return v.z;
		}
		return 0.0f;
	}

	// vの指定した軸だけをvalueへ書き換える。
	static void SetAxis(Math::Vector3& v, RootMotionAxis axis, float value) {
		switch (axis) {
		case RootMotionAxis::X: v.x = value; break;
		case RootMotionAxis::Y: v.y = value; break;
		case RootMotionAxis::Z: v.z = value; break;
		}
	}

	std::string			boneName_;         
	KdModelWork::Node*	node_ = nullptr;     
	bool				boneResolved_ = false;
	bool				needsResync_ = false;
	Math::Vector3		lastLocalPos_{};      
	Math::Vector3		lockedLocalPos_{};   
	Math::Vector3		delta_{};            
	float				unitScale_ = 1.0f;   
	RootMotionAxis		forwardAxis_ = RootMotionAxis::Z;
	float				forwardSign_ = 1.0f;
	RootMotionAxis		rightAxis_ = RootMotionAxis::X;  
	float				rightSign_ = 1.0f;
	bool				extractRotation_ = false;
	Math::Quaternion	lastLocalRot_{};
	Math::Quaternion	lockedLocalRot_{};
	float				yawDelta_ = 0.0f;
	float				rotationSign_ = 1.0f;
};