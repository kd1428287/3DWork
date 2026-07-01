#pragma once
// 毎ターンの更新処理（例: ターン開始フェイズ）
class LatencyUpdateSystem : public SystemBase
{
public:
	void Update(Registry& registry) override
	{
		//registry.View<CameraRotateComponent, TransformComponent>(
		//	[](Entity e, CameraRotateComponent& rotate, TransformComponent& transform)
		//	{
		//		// マウス差分を取得（実際はフレームワークのInput経由）
		//		// ここでは概念として記述
		//		float mouseX = 0.f; // Input::GetMouseDeltaX()
		//		float mouseY = 0.f; // Input::GetMouseDeltaY()

		//		transform.rotX += mouseY * rotate.sensitivity;
		//		transform.rotY += mouseX * rotate.sensitivity;

		//		// 回転制限
		//		if (transform.rotX > 45.f) { transform.rotX = 45.f; }
		//		if (transform.rotX < -45.f) { transform.rotX = -45.f; }
		//	});
	}
};