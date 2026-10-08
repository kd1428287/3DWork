#include "Framework/KdFramework.h"

#include "UIImageComponent.h"
#include "Application/Components/Graphics/UI/UITransformComponent.h"

void UIImageComponent::Awake()
{
	ui_ = GetOwner()->GetComponent<UITransformComponent>();
}

void UIImageComponent::SetTexture(const std::string& path)
{
	m_texture = nullptr;
	if (path.empty()) return;

	if (!KdFileExistence(path))
	{
		OutputDebugStringA(("UIImageComponent: texture not found: " + path + "\n").c_str());
		return;
	}

	m_texture = KdAssets::Instance().m_textures.GetData(path);
}

void UIImageComponent::DrawSprite()
{
	// UITransformがAwake後に追加された場合に備えて、ここでも探す
	if (!ui_) ui_ = GetOwner()->GetComponent<UITransformComponent>();
	if (!ui_)
	{
		if (!m_warned)
		{
			m_warned = true;
			OutputDebugStringA("UIImageComponent: UITransformComponent is required\n");
		}
		return;
	}

	Math::Vector2 base;
	if (!ui_->Resolve(base)) return;

	const Math::Vector2 scale = ui_->GetScale();
	const Math::Vector2 pivot = ui_->GetPivot();

	// sizeが0ならテクスチャの大きさで表示する
	Math::Vector2 size(ui_->GetSize().x * scale.x, ui_->GetSize().y * scale.y);
	if (size.x == 0.0f && size.y == 0.0f && m_texture)
	{
		size = Math::Vector2(m_texture->GetWidth() * scale.x, m_texture->GetHeight() * scale.y);
	}
	if (size.x <= 0.0f || size.y <= 0.0f) return;

	// 左下の座標(Y上向き)
	const Math::Vector2 min = base - Math::Vector2(size.x * pivot.x, size.y * pivot.y);

	auto& shader = KdShaderManager::Instance().m_spriteShader;

	// 回転は基準点まわり。描画後にIdentityへ戻す
	const bool rotated = ui_->GetRotation() != 0.0f;
	if (rotated) shader.SetMatrix(ui_->GetRotationMatrix(base));

	if (m_texture)
	{
		// pivot(0,0)で左下基準になる
		shader.DrawTex(m_texture.get(), (int)min.x, (int)min.y, (int)(size.x + 0.5f), (int)(size.y + 0.5f), nullptr, &m_color, Math::Vector2(0.0f, 0.0f));
	}
	else
	{
		shader.DrawBox((int)(min.x + size.x * 0.5f), (int)(min.y + size.y * 0.5f),
			(int)(size.x * 0.5f), (int)(size.y * 0.5f), &m_color, true);
	}

	if (rotated) shader.SetMatrix(Math::Matrix::Identity);
}
