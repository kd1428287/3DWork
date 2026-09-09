#pragma once
#include "../Transform/TransformComponent.h" 

// ============================================================
// カメラの追従対象になれるものが実装するインターフェース。
//
// IMovementSourceと同じ考え方: 「追従先の決め方」を抽象化し、
// CameraFollowComponent側はこれを通じて座標を問い合わせるだけで、
// 追従先の実体(プレイヤーか、特定のカードか、固定ポイントか)には
// 関知しない。
//
// KD_TAG_INTERFACES(GetTagged<T>()で使う"同一GameObject内の役割"を
// 探す仕組み)には登録しない。カメラが追従したいのは「シーン内の
// 特定の1体」であり、同一GameObject内を探す仕組みとは目的が違うため。
// 対象はCameraFollowComponent::SetTarget()で直接渡す(Push方式)。
// ============================================================
class ICameraTarget {
public:
	virtual ~ICameraTarget() = default;
	virtual Math::Vector3    GetTargetPosition() const = 0;
	virtual Math::Quaternion GetTargetRotation() const = 0;
};
