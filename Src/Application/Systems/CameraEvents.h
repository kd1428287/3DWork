#pragma once

namespace Events
{
	namespace Camera
	{
		// 座標だけで表現できる単純なエフェクトの汎用発生イベント(一撃だけ、その場で発生)
		struct CameraShakeEvent : public Event
		{
			float trauma = 0.f;
		};

		struct CameraShakeSettingEvent : public Event
		{
			float perSecond = -1.f;								// 外傷値が減衰しきるまでの体感速度。大きいほどすぐ揺れが収まる。
			float frequency = -1.f;								// 揺れの細かさ。大きいほど震え、小さいほどゆったりうねる。
			Math::Vector3 posAmplitude = { 0.f,0.f,0.f };		// 位置・回転それぞれの最大振幅(trauma=1.0時の値)。
			Math::Vector3 rotAmplitude = { 0.f,0.f,0.f };		// 位置は奥行き(進行方向)を揺らさないのが一般的(酔いにくい)。
		};
	}
}

// Publishヘルパー
inline void RequestCameraShake(EventBus& bus, const float& trauma)
{
	Events::Camera::CameraShakeEvent e;
	e.trauma = trauma;
	bus.Publish(e);
}

inline void CameraShakeSettings(EventBus& bus, const float& perSecond = -1.0f, const float& frequency = -1.0f, 
	const Math::Vector3& posAmplitude = { 0.f,0.f,0.f }, const Math::Vector3& rotAmplitude = { 0.f,0.f,0.f })
{
	Events::Camera::CameraShakeSettingEvent e;
	e.perSecond = perSecond;
	e.frequency = frequency;
	e.posAmplitude = posAmplitude;
	e.rotAmplitude = rotAmplitude;
	bus.Publish(e);
}