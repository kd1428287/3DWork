// UI(HUD・ワールドゲージ・テキスト・画像・ボタン)のコンポーネント登録。

#include <sstream>

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"
#include "Application/Factories/Registrations/RegistrationHelpers.h"
#include "Application/Definitions/Json/KdJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Factories/Json/UIConfigJson.h"
#include "Application/main.h"
#include "Application/Core/EventBus/EventBus.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"
#include "Application/Core/EventBus/Events/AudioEvents.h"
#include "Application/Core/EventBus/Events/EffectEvents.h"
#include "Application/Systems/SelectableManager.h"

// UI(配置フォルダは実際の場所に合わせて調整)
#include "Application/Components/Graphics/UI/Gauge/PlayerHudGaugeComponent.h"
#include "Application/Components/Graphics/UI/Gauge/EnemyWorldGaugeComponent.h"
#include "Application/Components/Graphics/UI/Gauge/EnemyOverheadGaugeComponent.h"
#include "Application/Components/Graphics/UI/Gauge/EnemyHudGaugeComponent.h"
#include "Application/Components/Graphics/UI/UITransformComponent.h"
#include "Application/Components/Graphics/UI/Text/TextComponent.h"
#include "Application/Components/Graphics/UI/Image/UIImageComponent.h"
#include "Application/Components/Graphics/UI/Button/SelectableComponent.h"

using namespace Registration;

// 不明な文字列は先頭のClickになる
NLOHMANN_JSON_SERIALIZE_ENUM(UITrigger, {
	{ UITrigger::Click, "Click" },
	{ UITrigger::HoverEnter, "HoverEnter" },
	{ UITrigger::HoverExit, "HoverExit" },
	{ UITrigger::Select, "Select" },
	{ UITrigger::Deselect, "Deselect" },
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

		int                  outlineWidth = 0;
		std::array<float, 4> outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	};
	COMPONENT_PARAMS_DEFINE_TYPE(TextParams, text, fontNo, antiAliasing, align, color, outlineWidth, outlineColor)

		void BuildText(BuildContext& ctx, const TextParams& p)
	{
		auto* text = ctx.Add<TextComponent>();
		text->SetFontNo(p.fontNo);
		text->SetAntiAliasing(p.antiAliasing);
		text->SetText(JsonUtf8ToSjis(p.text));
		text->SetAlign(p.align);
		text->SetColor(Math::Color(p.color[0], p.color[1], p.color[2], p.color[3]));
		text->SetOutline(p.outlineWidth, Math::Color(p.outlineColor[0], p.outlineColor[1], p.outlineColor[2], p.outlineColor[3]));
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

	// Selectableのアクション1件。idは登録済みのアクションID、paramはその引数
	struct SelectableActionParams
	{
		UITrigger   trigger = UITrigger::Click;
		std::string id;
		std::string param;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(SelectableActionParams, trigger, id, param)

		// 選択できるUI(ボタンなど)。当たり判定は、同じオブジェクトのUITransformの矩形(必須)。
		// actionsは配列の順に発行する。
		struct SelectableParams
	{
		bool        interactable = true;
		int         layer = 0;
		int         group = 0;

		// ホバー中の拡大率(1で無効)と追従の速さ(1/秒)
		float       hoverScale = 1.0f;
		float       hoverSpeed = 12.0f;

		std::vector<SelectableActionParams> actions;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(SelectableParams, interactable, layer, group, hoverScale, hoverSpeed, actions)

		void BuildSelectable(BuildContext& ctx, const SelectableParams& p)
	{
		auto* selectable = ctx.Add<SelectableComponent>();
		selectable->SetInteractable(p.interactable);
		selectable->SetLayer(p.layer);
		selectable->SetGroup(p.group);
		selectable->SetHoverScale(p.hoverScale, p.hoverSpeed);

		for (const auto& action : p.actions)
		{
			if (!action.id.empty()) selectable->AddAction(action.trigger, action.id, action.param);
		}
	}

	// 敵Prefabへ直接付け、自分のHP/体幹ゲージをworldOffsetだけずらした位置に描く。
	// worldOffsetはscale=1基準で所有者の拡大率に合わせる。barSizeはピクセル。
	struct EnemyOverheadGaugeParams
	{
		Math::Vector3        worldOffset = { 0.0f, 2.0f, 0.0f };
		std::array<float, 2> healthBarSize = { 80.0f, 10.0f };
		std::array<float, 2> postureBarSize = { 80.0f, 6.0f };
	};
	COMPONENT_PARAMS_DEFINE_TYPE(EnemyOverheadGaugeParams, worldOffset, healthBarSize, postureBarSize)

		void BuildEnemyOverheadGauge(BuildContext& ctx, const EnemyOverheadGaugeParams& p)
	{
		auto* gauge = ctx.Add<EnemyOverheadGaugeComponent>();
		gauge->SetWorldOffset(p.worldOffset * ctx.OwnerScale());
		gauge->SetBarSizes(Math::Vector2(p.healthBarSize[0], p.healthBarSize[1]),
			Math::Vector2(p.postureBarSize[0], p.postureBarSize[1]));
	}

	// 敵Prefabの子として付け、親の敵をターゲットにした画面固定のHP/体幹ゲージ(ボス戦の画面上部など)を描く。
	// 位置・サイズはピクセル(画面中心原点、PlayerHudGaugeと同じ座標系)。
	struct EnemyHudGaugeParams
	{
		std::array<float, 2> healthBarPos = { -600.0f, 300.0f };
		std::array<float, 2> healthBarSize = { 300.0f, 14.0f };
		std::array<float, 2> postureBarPos = { -250.0f, 300.0f };
		std::array<float, 2> postureBarSize = { 500.0f, 10.0f };
	};
	COMPONENT_PARAMS_DEFINE_TYPE(EnemyHudGaugeParams, healthBarPos, healthBarSize, postureBarPos, postureBarSize)

		void BuildEnemyHudGauge(BuildContext& ctx, const EnemyHudGaugeParams& p)
	{
		auto* gauge = ctx.Add<EnemyHudGaugeComponent>();
		gauge->SetTarget(Handle<GameObject>(RequireParent(ctx, "EnemyHudGauge")));
		gauge->SetHealthRect(Math::Vector2(p.healthBarPos[0], p.healthBarPos[1]),
			Math::Vector2(p.healthBarSize[0], p.healthBarSize[1]));
		gauge->SetPostureRect(Math::Vector2(p.postureBarPos[0], p.postureBarPos[1]),
			Math::Vector2(p.postureBarSize[0], p.postureBarSize[1]));
	}

	// 親の敵をターゲットにした頭上ゲージUIを組み立てる。
	void BuildEnemyWorldGauge(BuildContext& ctx, const nlohmann::json&)
	{
		ctx.Add<EnemyWorldGaugeComponent>()->SetTarget(Handle<GameObject>(RequireParent(ctx, "EnemyWorldGauge")));
	}

	// paramは遷移先のシーン名(Title / Tutorial / Game / Result)
	void RegisterSceneActions(SelectableManager& manager)
	{
		manager.RegisterAction("ChangeScene", [](const std::string& param)
			{
				static const std::unordered_map<std::string, SceneType> kScenes = {
					{ "Title",    SceneType::Title },
					{ "Tutorial", SceneType::Tutorial },
					{ "Game",     SceneType::Game },
					{ "Result",   SceneType::Result },
				};

				const auto it = kScenes.find(param);
				if (it == kScenes.end())
				{
					KdDebugGUI::Instance().AddLog("ChangeScene: 不明なシーン %s\n", param.c_str());
					return;
				}
				GLOBALEVENT->Publish(Events::SceneChangeRequest{ it->second });
			});
	}

	// paramはサウンドID
	void RegisterAudioActions(SelectableManager& manager)
	{
		manager.RegisterAction("PlaySE", [](const std::string& param)
			{
				if (param.empty()) return;
				Events::PublishSoundPlayEvent(*GLOBALEVENT, param);
			});
	}

	// paramは "エフェクトID" または "エフェクトID,x,y,z"(座標省略時は原点)
	void RegisterEffectActions(SelectableManager& manager)
	{
		manager.RegisterAction("PlayEffect", [](const std::string& param)
			{
				std::istringstream stream(param);
				std::string id;
				if (!std::getline(stream, id, ',') || id.empty()) return;

				Math::Vector3 pos;
				std::string value;
				for (float* axis : { &pos.x, &pos.y, &pos.z })
				{
					if (!std::getline(stream, value, ',')) break;
					*axis = std::strtof(value.c_str(), nullptr);
				}
				Events::PublishGenericEffect(*GLOBALEVENT, id, pos);
			});
	}

	// UIから発行するアクションの登録
	void RegisterUIActions()
	{
		auto& manager = SelectableManager::Instance();
		RegisterSceneActions(manager);
		RegisterAudioActions(manager);
		RegisterEffectActions(manager);
	}
}  // namespace

void RegisterUIComponents(ComponentRegistry& registry)
{
	// PlayerHudGaugeは独立Prefab(Hud.json)で生成し、ctx->playerを追う。EnemyWorldGaugeは敵Prefabの子として使う。
	registry.Add<PlayerHudGaugeComponent>("PlayerHudGauge");
	registry.AddRaw("EnemyWorldGauge", BuildEnemyWorldGauge);
	registry.AddWithParams<EnemyHudGaugeParams>("EnemyHudGauge", BuildEnemyHudGauge);
	registry.AddWithParams<EnemyOverheadGaugeParams>("EnemyOverheadGauge", BuildEnemyOverheadGauge);
	registry.AddWithParams<UITransformParams>("UITransform", BuildUITransform);
	registry.AddWithParams<TextParams>("Text", BuildText);
	registry.AddWithParams<UIImageParams>("UIImage", BuildUIImage);
	registry.AddWithParams<SelectableParams>("Selectable", BuildSelectable);

	RegisterUIActions();
}