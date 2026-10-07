#include "UITransformComponent.h"
#include "Application/main.h"

namespace
{
	Math::Matrix s_view = Math::Matrix::Identity;
	Math::Matrix s_proj = Math::Matrix::Identity;
}

void UITransformComponent::SetCamera(const Math::Matrix& view, const Math::Matrix& proj)
{
	s_view = view;
	s_proj = proj;
}

Math::Vector2 UITransformComponent::GetViewportSize()
{
	UINT num = 1;
	D3D11_VIEWPORT vp{};
	KdDirect3D::Instance().WorkDevContext()->RSGetViewports(&num, &vp);
	return Math::Vector2(vp.Width, vp.Height);
}

bool UITransformComponent::GetMousePos(Math::Vector2& out)
{
	HWND hwnd = Application::Instance().GetWindowHandle();
	if (!hwnd) return false;

	POINT pt;
	RECT rc;
	if (!GetCursorPos(&pt) || !ScreenToClient(hwnd, &pt) || !GetClientRect(hwnd, &rc)) return false;

	const float cw = (float)rc.right;
	const float ch = (float)rc.bottom;
	if (cw <= 0.0f || ch <= 0.0f) return false;
	if (pt.x < 0 || pt.y < 0 || pt.x >= rc.right || pt.y >= rc.bottom) return false;

	// 左上原点のクライアント座標を、ビューポート基準の中心原点・Y上向きへ変換する
	const Math::Vector2 vp = GetViewportSize();
	out.x = ((float)pt.x / cw - 0.5f) * vp.x;
	out.y = (0.5f - (float)pt.y / ch) * vp.y;
	return true;
}

bool UITransformComponent::WorldToScreen(const Math::Vector3& world, Math::Vector2& out)
{
	Math::Vector4 clip = Math::Vector4::Transform(Math::Vector4(world.x, world.y, world.z, 1.0f), s_view * s_proj);
	if (clip.w <= 0.0f) return false;

	const Math::Vector2 vp = GetViewportSize();
	out.x = clip.x / clip.w * vp.x * 0.5f;
	out.y = clip.y / clip.w * vp.y * 0.5f;
	return true;
}

bool UITransformComponent::Resolve(Math::Vector2& outPos) const
{
	if (space_ == Space::World)
	{
		if (!transform_) return false;

		Math::Vector2 sp;
		if (!WorldToScreen(transform_->GetPosition() + worldOffset_, sp)) return false;

		outPos = sp + position_;
		return true;
	}

	// アンカー(0～1)を中心原点の座標へ変換する。(0.5,0.5)が画面中央
	const Math::Vector2 vp = GetViewportSize();
	outPos = Math::Vector2((anchor_.x - 0.5f) * vp.x, (anchor_.y - 0.5f) * vp.y) + position_;
	return true;
}

bool UITransformComponent::ResolveRect(Math::Vector2& outMin, Math::Vector2& outSize) const
{
	Math::Vector2 pos;
	if (!Resolve(pos)) return false;

	outSize = Math::Vector2(size_.x * scale_.x, size_.y * scale_.y);
	outMin = pos - Math::Vector2(outSize.x * pivot_.x, outSize.y * pivot_.y);
	return true;
}

Math::Matrix UITransformComponent::GetRotationMatrix(const Math::Vector2& pivotPos) const
{
	// 基準点を原点へ移し、回転してから戻す
	return Math::Matrix::CreateTranslation(-pivotPos.x, -pivotPos.y, 0.0f)
		* Math::Matrix::CreateRotationZ(DirectX::XMConvertToRadians(rotation_))
		* Math::Matrix::CreateTranslation(pivotPos.x, pivotPos.y, 0.0f);
}
