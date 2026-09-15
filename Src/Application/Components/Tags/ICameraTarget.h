#pragma once

// カメラの追従対象になれるものが実装するインターフェース
class ICameraTarget {
public:
	virtual ~ICameraTarget() = default;
	virtual Math::Vector3    GetFixationPoint() const = 0;
	virtual Math::Quaternion GetTargetRotation() const = 0;
};
