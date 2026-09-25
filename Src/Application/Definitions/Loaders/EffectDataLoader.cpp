#include "Application/main.h"
#include "EffectDataLoader.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// BlendMode/EmitModeを文字列で保存する(値の並び替え・追加に強くする為)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static const char* BlendModeToString(ParticleBlendMode m)
{
	switch (m)
	{
	case ParticleBlendMode::Add:   return "Add";
	case ParticleBlendMode::Alpha: return "Alpha";
	}
	return "Add";
}
static ParticleBlendMode BlendModeFromString(const std::string& s)
{
	if (s == "Alpha") { return ParticleBlendMode::Alpha; }
	return ParticleBlendMode::Add;
}
static const char* EmitModeToString(ParticleEmitMode m)
{
	switch (m)
	{
	case ParticleEmitMode::Burst:      return "Burst";
	case ParticleEmitMode::Continuous: return "Continuous";
	}
	return "Burst";
}
static ParticleEmitMode EmitModeFromString(const std::string& s)
{
	if (s == "Continuous") { return ParticleEmitMode::Continuous; }
	return ParticleEmitMode::Burst;
}
static const char* BillboardModeToString(ParticleBillboardMode m)
{
	switch (m)
	{
	case ParticleBillboardMode::Normal:  return "Normal";
	case ParticleBillboardMode::Stretch: return "Stretch";
	}
	return "Normal";
}
static ParticleBillboardMode BillboardModeFromString(const std::string& s)
{
	if (s == "Stretch") { return ParticleBillboardMode::Stretch; }
	return ParticleBillboardMode::Normal;
}
static const char* DrawPassToString(ParticleDrawPass p)
{
	switch (p)
	{
	case ParticleDrawPass::Default:   return "Default";
	case ParticleDrawPass::Bright: return "Bright";
	}
	return "Default";
}
// 単一のパス名文字列 → フラグ1個分(旧形式JSONの後方互換読み込み専用)
static ParticleDrawPass DrawPassFromString(const std::string& s)
{
	if (s == "Bright") { return ParticleDrawPass::Bright; }
	return ParticleDrawPass::Default;
}

// DrawPassFlags(ビットフラグ) → JSON配列("Default"/"Brightのうち立っているものだけを列挙)
static nlohmann::json DrawPassFlagsToJson(ParticleDrawPass flags)
{
	nlohmann::json arr = nlohmann::json::array();
	if (KdHasDrawPassFlag(flags, ParticleDrawPass::Default)) { arr.push_back(DrawPassToString(ParticleDrawPass::Default)); }
	if (KdHasDrawPassFlag(flags, ParticleDrawPass::Bright)) { arr.push_back(DrawPassToString(ParticleDrawPass::Bright)); }
	return arr;
}

// "drawPass"キーからDrawPassFlagsを読む。
static ParticleDrawPass DrawPassFlagsFromJson(const nlohmann::json& j, ParticleDrawPass defaults)
{
	if (!j.contains("drawPass")) { return defaults; }

	const nlohmann::json& v = j.at("drawPass");

	if (v.is_array())
	{
		ParticleDrawPass flags = static_cast<ParticleDrawPass>(0);
		bool any = false;

		for (const auto& entry : v)
		{
			if (!entry.is_string()) { continue; }

			const std::string s = entry.get<std::string>();
			if (s == "Default") { flags |= ParticleDrawPass::Default;   any = true; }
			else if (s == "Bright") { flags |= ParticleDrawPass::Bright; any = true; }
		}

		return any ? flags : defaults;
	}

	if (v.is_string())
	{
		return DrawPassFromString(v.get<std::string>());
	}

	return defaults;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 壊れたJSONに対する安全な配列読み取りヘルパー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// nlohmann::jsonは「配列のはずが違う型」「要素数が足りない」場合、
// 生の j[i] アクセスや誤った value<T>() 呼び出しで例外(type_error / out_of_range)を
// 投げてくる。これまでのコードはこの例外を一切catchしておらず、
// 手編集や外部ツールが吐いた壊れたJSONを読んだ瞬間に呼び出し元(エディタ本体)ごと
// 落ちてしまっていた。
//
// ここではキーの存在・配列であること・要素数・各要素が数値であることを
// 全て確認してから読み取り、条件を満たさない場合は例外を投げずfalseを返して
// outには触れない(＝呼び出し側のデフォルト値がそのまま生き残る)方針にする。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static bool TryReadVector3(const nlohmann::json& j, const char* key, DirectX::SimpleMath::Vector3& out)
{
	if (!j.contains(key)) { return false; }

	const nlohmann::json& v = j.at(key);
	if (!v.is_array() || v.size() != 3) { return false; }
	for (const auto& e : v) { if (!e.is_number()) { return false; } }

	out = { v[0].get<float>(), v[1].get<float>(), v[2].get<float>() };
	return true;
}

static bool TryReadVector4(const nlohmann::json& j, const char* key, DirectX::SimpleMath::Vector4& out)
{
	if (!j.contains(key)) { return false; }

	const nlohmann::json& v = j.at(key);
	if (!v.is_array() || v.size() != 4) { return false; }
	for (const auto& e : v) { if (!e.is_number()) { return false; } }

	out = { v[0].get<float>(), v[1].get<float>(), v[2].get<float>(), v[3].get<float>() };
	return true;
}

// 想定外の壊れ方(型不一致でのvalue()例外等)を報告する場所を1箇所に集約しておく。
// TODO: プロジェクト側にロガーがあれば、ここをそちらの呼び出しに差し替える
static void ReportLoadWarning(const std::string& context, const std::exception& e)
{
#ifdef _DEBUG
	std::string msg = "[EffectDataLoader] " + context + " : " + e.what() + "\n";
	OutputDebugStringA(msg.c_str());
#else
	(void)context; (void)e;
#endif
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// DirectionalEmitShape ⇔ JSON
//	GPUParticleLayerから共用する(鍔迫り合いの火花もこの形状定義だけで表現される為、
//	専用のSparkLayer変換関数は不要になった)。
//	見た目(Size/Life/Color)はParticleAppearance側に分離済み。JSON上は引き続き
//	Layerと同じフラットなオブジェクトにキーを併存させる(後方互換のため)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static nlohmann::json ShapeToJson(const DirectionalEmitShape& s)
{
	return nlohmann::json{
		{ "dirScaleMin", s.DirScaleMin },
		{ "dirScaleMax", s.DirScaleMax },
		{ "offsetMin",   { s.OffsetMin.x, s.OffsetMin.y, s.OffsetMin.z } },
		{ "offsetMax",   { s.OffsetMax.x, s.OffsetMax.y, s.OffsetMax.z } },
	};
}

static DirectionalEmitShape ShapeFromJson(const nlohmann::json& j, const DirectionalEmitShape& defaults)
{
	DirectionalEmitShape s = defaults;

	// スカラー値：型不一致(文字列が入っている等)ならvalue()が例外を投げるが、
	// それはこの関数の呼び出し元(LayerFromJson)がtry/catchで受け止め、
	// この1レイヤーぶんだけデフォルトへフォールバックさせる想定
	s.DirScaleMin = j.value("dirScaleMin", s.DirScaleMin);
	s.DirScaleMax = j.value("dirScaleMax", s.DirScaleMax);

	TryReadVector3(j, "offsetMin", s.OffsetMin);
	TryReadVector3(j, "offsetMax", s.OffsetMax);

	return s;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ParticleAppearance ⇔ JSON
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// "sizeMin"/"sizeMax"は従来通り発生時サイズ(SizeStart)として読み書きする(キー名は
// 後方互換のため維持)。"sizeEndMin"/"sizeEndMax"が記載されていない旧JSONは、
// 消滅時サイズ＝発生時サイズとして扱う(＝これまで通りサイズが変化しない挙動)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static nlohmann::json AppearanceToJson(const ParticleAppearance& a)
{
	return nlohmann::json{
		{ "lifeMin",       a.LifeMin },
		{ "lifeMax",       a.LifeMax },
		{ "sizeMin",       a.SizeStartMin },
		{ "sizeMax",       a.SizeStartMax },
		{ "sizeEndMin",    a.SizeEndMin },
		{ "sizeEndMax",    a.SizeEndMax },
		{ "colorStartMin", { a.ColorStartMin.x, a.ColorStartMin.y, a.ColorStartMin.z, a.ColorStartMin.w } },
		{ "colorStartMax", { a.ColorStartMax.x, a.ColorStartMax.y, a.ColorStartMax.z, a.ColorStartMax.w } },
		{ "colorMin",      { a.ColorMin.x, a.ColorMin.y, a.ColorMin.z, a.ColorMin.w } },
		{ "colorMax",      { a.ColorMax.x, a.ColorMax.y, a.ColorMax.z, a.ColorMax.w } },
	};
}

static ParticleAppearance AppearanceFromJson(const nlohmann::json& j, const ParticleAppearance& defaults)
{
	ParticleAppearance a = defaults;

	a.LifeMin = j.value("lifeMin", a.LifeMin);
	a.LifeMax = j.value("lifeMax", a.LifeMax);

	// 後方互換："sizeMin"/"sizeMax"は発生時サイズ(SizeStart)として読む
	a.SizeStartMin = j.value("sizeMin", a.SizeStartMin);
	a.SizeStartMax = j.value("sizeMax", a.SizeStartMax);

	// "sizeEndMin/Max"が無い場合はSizeStartと同値にする(旧仕様＝サイズ変化なしを再現)
	a.SizeEndMin = j.value("sizeEndMin", a.SizeStartMin);
	a.SizeEndMax = j.value("sizeEndMax", a.SizeStartMax);

	// 旧形式("color"単一キー)からの読み込みにも対応し、既存の保存済みJSONを壊さない
	{
		DirectX::SimpleMath::Vector4 legacyColor;
		if (TryReadVector4(j, "color", legacyColor))
		{
			a.ColorMin = legacyColor;
			a.ColorMax = legacyColor;
		}
	}
	TryReadVector4(j, "colorStartMin", a.ColorStartMin);
	TryReadVector4(j, "colorStartMax", a.ColorStartMax);
	TryReadVector4(j, "colorMin", a.ColorMin);
	TryReadVector4(j, "colorMax", a.ColorMax);

	return a;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// GPUParticleLayer ⇔ JSON
//	Shape/Appearanceそれぞれの変換結果を1つのフラットなJSONオブジェクトへ統合する。
//	(以前からJSON側は分割されておらず1階層のオブジェクトなので、スキーマ自体は
//	 変えずに済む。既存の保存済みJSONもそのまま読める)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static nlohmann::json LayerToJson(const GPUParticleLayer& l)
{
	nlohmann::json j = ShapeToJson(l.Shape);
	j.update(AppearanceToJson(l.Appearance));
	j["count"] = l.Count;
	j["billboardMode"] = BillboardModeToString(l.BillboardMode);
	j["stretchScale"] = l.StretchScale;
	return j;
}

static GPUParticleLayer LayerFromJson(const nlohmann::json& j, const GPUParticleLayer& defaults)
{
	GPUParticleLayer l;
	l.Shape = ShapeFromJson(j, defaults.Shape);
	l.Appearance = AppearanceFromJson(j, defaults.Appearance);
	l.Count = j.value("count", defaults.Count);
	// 旧形式のJSON(billboardMode未記載)はNormal扱いになる(後方互換)
	l.BillboardMode = BillboardModeFromString(j.value("billboardMode", std::string("Normal")));
	l.StretchScale = j.value("stretchScale", defaults.StretchScale);
	return l;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// GPUParticleParams ⇔ JSON
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static nlohmann::json ParamsToJson(const GPUParticleParams& p)
{
	nlohmann::json layersJson = nlohmann::json::array();
	for (auto& layer : p.Layers)
	{
		layersJson.push_back(LayerToJson(layer));
	}

	return nlohmann::json{
		{ "maxParticleNum", p.MaxParticleNum },
		{ "emitMode",       EmitModeToString(p.EmitMode) },
		{ "emitInterval",   p.EmitInterval },
		{ "layers",         layersJson },
		{ "gravity",        { p.Gravity.x, p.Gravity.y, p.Gravity.z } },
		{ "texture",        p.TexturePath },
		{ "blendMode",      BlendModeToString(p.BlendMode) },
		{ "drawPass",       DrawPassFlagsToJson(p.DrawPassFlags) },
	};
}

static GPUParticleParams ParamsFromJson(const nlohmann::json& j)
{
	GPUParticleParams p;
	p.MaxParticleNum = j.value("maxParticleNum", p.MaxParticleNum);
	p.EmitMode = EmitModeFromString(j.value("emitMode", std::string("Burst")));
	p.EmitInterval = j.value("emitInterval", p.EmitInterval);

	// レイヤーは1件ずつtry/catchする：1レイヤーの記述が壊れていても、
	// そのレイヤーだけ諦めて他のレイヤーは正常に読み込む(全滅させない)
	if (j.contains("layers") && j.at("layers").is_array() && !j.at("layers").empty())
	{
		std::vector<GPUParticleLayer> layers;
		layers.reserve(j.at("layers").size());

		for (const auto& entry : j.at("layers"))
		{
			if (!entry.is_object())
			{
				ReportLoadWarning("layer entry is not an object, skipped", std::runtime_error("invalid layer entry"));
				continue;
			}

			try
			{
				layers.push_back(LayerFromJson(entry, GPUParticleLayer{}));
			}
			catch (const std::exception& e)
			{
				ReportLoadWarning("failed to parse a layer entry, skipped", e);
			}
		}

		// 1件も読めなかった場合はデフォルトの1層構成を維持する(空Layersにはしない)
		if (!layers.empty())
		{
			p.Layers = std::move(layers);
		}
	}

	TryReadVector3(j, "gravity", p.Gravity);

	p.TexturePath = j.value("texture", std::string());
	p.BlendMode = BlendModeFromString(j.value("blendMode", std::string("Add")));
	// 旧形式のJSON(drawPass未記載)はLitのみ扱いになる(後方互換)
	p.DrawPassFlags = DrawPassFlagsFromJson(j, ParticleDrawPass::Default);

	return p;
}

static nlohmann::json DefinitionToJson(const EffectDefinition& def)
{
	return nlohmann::json{
		{ "name",   def.Name },
		{ "pos",    { def.Pos.x, def.Pos.y, def.Pos.z } },
		{ "rotate", { def.Rotate.x, def.Rotate.y, def.Rotate.z } },
		{ "scale",  { def.Scale.x, def.Scale.y, def.Scale.z } },
		{ "params", ParamsToJson(def.Params) },
	};
}

static EffectDefinition DefinitionFromJson(const nlohmann::json& j)
{
	EffectDefinition def;
	def.Name = j.value("name", std::string());

	TryReadVector3(j, "pos", def.Pos);
	TryReadVector3(j, "rotate", def.Rotate);
	TryReadVector3(j, "scale", def.Scale);

	if (j.contains("params") && j.at("params").is_object())
	{
		def.Params = ParamsFromJson(j.at("params"));
	}

	return def;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクトデータ一式の読み込み/書き出し
//	JSON全体は { "effects": [...] } という形の1オブジェクト
//	(以前あった専用の"weaponClash"キーは廃止。鍔迫り合いの火花もeffects配列内の
//	 "WeaponClashParry"/"WeaponClashBlock"という名前の通常エントリとして保存される)
//
//	【堅牢性について】
//	・1エフェクト分の記述が壊れていても、そのエフェクトだけスキップして
//	  残りは正常に読み込む(部分復旧)。
//	・想定外の例外(壊れたJSON構造、型不一致でのvalue()例外等)は
//	  Load()の外へ一切伝播させず、失敗時はfalseを返す(outは変更しない)。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDataLoader::Load(const std::string& path, EffectDataFile& out)
{
	try
	{
		nlohmann::json j;
		if (!JsonLoader::Load(path, j)) { return false; }

		EffectDataFile data;

		if (j.contains("effects") && j.at("effects").is_array())
		{
			for (const auto& entry : j.at("effects"))
			{
				if (!entry.is_object())
				{
					ReportLoadWarning("effect entry is not an object, skipped", std::runtime_error("invalid effect entry"));
					continue;
				}

				try
				{
					data.Effects.push_back(DefinitionFromJson(entry));
				}
				catch (const std::exception& e)
				{
					// 1件だけ壊れていても他のエフェクトは読み込みたいので、ここで打ち切らずcontinueする
					ReportLoadWarning("failed to parse an effect entry, skipped", e);
				}
			}
		}

		out = std::move(data);
		return true;
	}
	catch (const std::exception& e)
	{
		// JsonLoader::Load自体が投げた場合や、想定していない構造による例外の最終防衛ライン。
		// ここに来た場合はファイル全体を信用せず、outには一切触れずfalseを返す
		ReportLoadWarning("Load() failed with an unexpected exception, aborted: " + path, e);
		return false;
	}
}

bool EffectDataLoader::Save(const std::string& path, const EffectDataFile& data)
{
	try
	{
		nlohmann::json effectsJson = nlohmann::json::array();
		for (auto& def : data.Effects)
		{
			effectsJson.push_back(DefinitionToJson(def));
		}

		nlohmann::json j = {
			{ "effects", effectsJson },
		};

		return JsonLoader::Save(path, j);
	}
	catch (const std::exception& e)
	{
		ReportLoadWarning("Save() failed with an unexpected exception, aborted: " + path, e);
		return false;
	}
}