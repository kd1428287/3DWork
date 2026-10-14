// UI(HUD・ワールドゲージ・テキスト・画像・ボタン)のコンポーネント登録。

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"
#include "Application/Factories/Registrations/RegistrationHelpers.h"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"

// UI(配置フォルダは実際の場所に合わせて調整)
#include "Application/Components/Graphics/UI/Gauge/PlayerHudGaugeComponent.h"
#include "Application/Components/Graphics/UI/Gauge/EnemyWorldGaugeComponent.h"
#include "Application/Components/Graphics/UI/UITransformComponent.h"
#include "Application/Components/Graphics/UI/Text/TextComponent.h"
#include "Application/Components/Graphics/UI/Image/UIImageComponent.h"
#include "Application/Components/Graphics/UI/Button/SelectableComponent.h"

using namespace Registration;

// UITransformComponent::Space: JSONでは"Screen"/"World"で指定する。未知の値はScreenになる。
NLOHMANN_JSON_SERIALIZE_ENUM(UITransformComponent::Space, {
	{ UITransformComponent::Space::Screen, "Screen" },
	{ UITransformComponent::Space::World, "World" },
	})

// TextComponent::Align: JSONでは"Left"/"Center"/"Right"で指定する。未知の値はLeftになる。
NLOHMANN_JSON_SERIALIZE_ENUM(TextComponent::Align, {
	{ TextComponent::Align::Left, "Left" },
	{ TextComponent::Align::Center, "Center" },
	{ TextComponent::Align::Right, "Right" },
	})

// 接続を伴う型のparams。ODR違反を避けるため、マクロも含めて無名名前空間に入れる。
namespace
{
	// UTF-8をShift-JIS(CP932)へ変換する。変換できない文字は'?'になる。
	std::string JsonUtf8ToSjis(const std::string& utf8)
	{
		if (utf8.empty()) return {};

		const int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
		if (wlen <= 0) return utf8;
		std::wstring wide(wlen, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), wide.data(), wlen);

		const int len = WideCharToMultiByte(932, 0, wide.c_str(), wlen, nullptr, 0, nullptr, nullptr);
		if (len <= 0) return utf8;
		std::string sjis(len, '\0');
		WideCharToMultiByte(932, 0, wide.c_str(), wlen, sjis.data(), len, nullptr, nullptr);
		return sjis;
	}

	// UI用の座標。spaceがWorldのときはTransformの位置を投影した点が基準(位置はUITransformに任せる)。
	// anchor/pivotは0～1の割合、position/sizeはピクセル、rotationは度数法。worldOffsetは所有者のscaleに合わせる。
	struct UITransformParams
	{
		UITransformComponent::Space space = UITransformComponent::Space::Screen;

		std::array<float, 2> anchor = { 0.5f, 0.5f };
		std::array<float, 2> pivot = { 0.5f, 0.5f };
		std::array<float, 2> position = { 0.0f, 0.0f };
		std::array<float, 2> size = { 0.0f, 0.0f };
		std::array<float, 2> scale = { 1.0f, 1.0f };
		float                rotation = 0.0f;
		Math::Vector3        worldOffset = { 0.0f, 0.0f, 0.0f };
	};
	COMPONENT_PARAMS_DEFINE_TYPE(UITransformParams, space, anchor, pivot, position, size, scale, rotation, worldOffset)

	void BuildUITransform(BuildContext& ctx, const UITransformParams& p)
	{
		auto* ui = ctx.Add<UITransformComponent>();
		ui->SetSpace(p.space);
		ui->SetAnchor(Math::Vector2(p.anchor[0], p.anchor[1]));
		ui->SetPivot(Math::Vector2(p.pivot[0], p.pivot[1]));
		ui->SetPosition(Math::Vector2(p.position[0], p.position[1]));
		ui->SetSize(Math::Vector2(p.size[0], p.size[1]));
		ui->SetScale(Math::Vector2(p.scale[0], p.scale[1]));
		ui->SetRotation(p.rotation);
		// ワールドのオフセットはscale=1基準。モデルの拡大率に合わせる。
		ui->SetWorldOffset(p.worldOffset * ctx.OwnerScale());
	}

	// 文字列表示。textはUTF-8で書き、読み込み時にShift-JISへ変換する。colorはRGBA。
	// 位置・pivot・拡大率は、同じオブジェクトのUITransformで指定する(必須)。
	struct TextParams
	{
		std::string text;
		int         fontNo = 0;
		int         antiAliasing = 3;

		TextComponent::Align align = TextComponent::Align::Left;

		std::array<float, 4> color = { 1.0f, 1.0f, 1.0f, 1.0f };
	};
	COMPONENT_PARAMS_DEFINE_TYPE(TextParams, text, fontNo, antiAliasing, align, color)

	void BuildText(BuildContext& ctx, const TextParams& p)
	{
		auto* text = ctx.Add<TextComponent>();
		text->SetFontNo(p.fontNo);
		text->SetAntiAliasing(p.antiAliasing);
		text->SetText(JsonUtf8ToSjis(p.text));
		text->SetAlign(p.align);
		text->SetColor(Math::Color(p.color[0], p.color[1], p.color[2], p.color[3]));
	}

	// 画像表示。textureはUTF-8のパスで書き、読み込み時にShift-JISへ変換する。空なら単色の矩形になる。
	// 位置・pivot・拡大率は、同じオブジェクトのUITransformで指定する(必須)。sizeが0ならテクスチャの大きさで表示する。
	struct UIImageParams
	{
		std::string          texture;
		std::array<float, 4> color = { 1.0f, 1.0f, 1.0f, 1.0f };
	};
	COMPONENT_PARAMS_DEFINE_TYPE(UIImageParams, texture, color)

	void BuildUIImage(BuildContext& ctx, const UIImageParams& p)
	{
		auto* image = ctx.Add<UIImageComponent>();
		image->SetColor(Math::Color(p.color[0], p.color[1], p.color[2], p.color[3]));
		image->SetTexture(JsonUtf8ToSjis(p.texture));
	}

	// 選択できるUI(ボタンなど)。当たり判定は、同じオブジェクトのUITransformの矩形(必須)。
	// onXxxは各タイミングで発行するアクションID(空なら何もしない)。clickParamはonClickに添えるparam。
	struct SelectableParams
	{
		bool        interactable = true;
		int         layer = 0;
		int         group = 0;

		std::string onHoverEnter;
		std::string onHoverExit;
		std::string onSelect;
		std::string onDeselect;
		std::string onClick;
		std::string clickParam;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(SelectableParams, interactable, layer, group,
		onHoverEnter, onHoverExit, onSelect, onDeselect, onClick, clickParam)

	void BuildSelectable(BuildContext& ctx, const SelectableParams& p)
	{
		auto* selectable = ctx.Add<SelectableComponent>();
		selectable->SetInteractable(p.interactable);
		selectable->SetLayer(p.layer);
		selectable->SetGroup(p.group);

		if (!p.onHoverEnter.empty()) selectable->AddAction(UITrigger::HoverEnter, p.onHoverEnter);
		if (!p.onHoverExit.empty())  selectable->AddAction(UITrigger::HoverExit, p.onHoverExit);
		if (!p.onSelect.empty())     selectable->AddAction(UITrigger::Select, p.onSelect);
		if (!p.onDeselect.empty())   selectable->AddAction(UITrigger::Deselect, p.onDeselect);
		if (!p.onClick.empty())      selectable->AddAction(UITrigger::Click, p.onClick, p.clickParam);
	}

	// 親の敵をターゲットにした頭上ゲージUIを組み立てる。
	void BuildEnemyWorldGauge(BuildContext& ctx, const nlohmann::json&)
	{
		ctx.Add<EnemyWorldGaugeComponent>()->SetTarget(Handle<GameObject>(RequireParent(ctx, "EnemyWorldGauge")));
	}
}  // namespace

void RegisterUIComponents(ComponentRegistry& registry)
{
	// PlayerHudGaugeは独立Prefab(Hud.json)で生成し、ctx->playerを追う。EnemyWorldGaugeは敵Prefabの子として使う。
	registry.Add<PlayerHudGaugeComponent>("PlayerHudGauge");
	registry.AddRaw("EnemyWorldGauge", BuildEnemyWorldGauge);
	registry.AddWithParams<UITransformParams>("UITransform", BuildUITransform);
	registry.AddWithParams<TextParams>("Text", BuildText);
	registry.AddWithParams<UIImageParams>("UIImage", BuildUIImage);
	registry.AddWithParams<SelectableParams>("Selectable", BuildSelectable);
}
