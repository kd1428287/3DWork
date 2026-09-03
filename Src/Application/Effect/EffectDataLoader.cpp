#include "../main.h"
#include "EffectDataLoader.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// BlendMode/EmitModeを文字列で保存する(値の並び替え・追加に強くする為)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static const char* BlendModeToString(KdParticleBlendMode m)
{
	switch (m)
	{
	case KdParticleBlendMode::Add:   return "Add";
	case KdParticleBlendMode::Alpha: return "Alpha";
	}
	return "Add";
}
static KdParticleBlendMode BlendModeFromString(const std::string& s)
{
	if (s == "Alpha") { return KdParticleBlendMode::Alpha; }
	return KdParticleBlendMode::Add;
}
static const char* EmitModeToString(KdParticleEmitMode m)
{
	switch (m)
	{
	case KdParticleEmitMode::Burst:      return "Burst";
	case KdParticleEmitMode::Continuous: return "Continuous";
	}
	return "Burst";
}
static KdParticleEmitMode EmitModeFromString(const std::string& s)
{
	if (s == "Continuous") { return KdParticleEmitMode::Continuous; }
	return KdParticleEmitMode::Burst;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// DirectionalEmitShape ⇔ JSON
//	GPUParticleLayer / DirectionalSparkLayer の両方から共用する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static nlohmann::json ShapeToJson(const DirectionalEmitShape& s)
{
	return nlohmann::json{
		{ "dirScaleMin", s.DirScaleMin },
		{ "dirScaleMax", s.DirScaleMax },
		{ "offsetMin",   { s.OffsetMin.x, s.OffsetMin.y, s.OffsetMin.z } },
		{ "offsetMax",   { s.OffsetMax.x, s.OffsetMax.y, s.OffsetMax.z } },
		{ "sizeMin",     s.SizeMin },
		{ "sizeMax",     s.SizeMax },
		{ "lifeMin",     s.LifeMin },
		{ "lifeMax",     s.LifeMax },
		{ "colorMin",    { s.ColorMin.x, s.ColorMin.y, s.ColorMin.z, s.ColorMin.w } },
		{ "colorMax",    { s.ColorMax.x, s.ColorMax.y, s.ColorMax.z, s.ColorMax.w } },
	};
}

static DirectionalEmitShape ShapeFromJson(const nlohmann::json& j, const DirectionalEmitShape& defaults)
{
	DirectionalEmitShape s = defaults;

	s.DirScaleMin = j.value("dirScaleMin", s.DirScaleMin);
	s.DirScaleMax = j.value("dirScaleMax", s.DirScaleMax);

	if (j.contains("offsetMin")) { auto& v = j.at("offsetMin"); s.OffsetMin = { v[0], v[1], v[2] }; }
	if (j.contains("offsetMax")) { auto& v = j.at("offsetMax"); s.OffsetMax = { v[0], v[1], v[2] }; }

	s.SizeMin = j.value("sizeMin", s.SizeMin);
	s.SizeMax = j.value("sizeMax", s.SizeMax);
	s.LifeMin = j.value("lifeMin", s.LifeMin);
	s.LifeMax = j.value("lifeMax", s.LifeMax);

	// 旧形式("color"単一キー)からの読み込みにも対応し、既存の保存済みJSONを壊さない
	if (j.contains("color"))
	{
		auto& c = j.at("color");
		DirectX::SimpleMath::Vector4 v = { c[0], c[1], c[2], c[3] };
		s.ColorMin = v;
		s.ColorMax = v;
	}
	if (j.contains("colorMin")) { auto& c = j.at("colorMin"); s.ColorMin = { c[0], c[1], c[2], c[3] }; }
	if (j.contains("colorMax")) { auto& c = j.at("colorMax"); s.ColorMax = { c[0], c[1], c[2], c[3] }; }

	return s;
}
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// GPUParticleLayer ⇔ JSON
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static nlohmann::json LayerToJson(const GPUParticleLayer& l)
{
	nlohmann::json j = ShapeToJson(l.Shape);
	j["count"] = l.Count;
	return j;
}

static GPUParticleLayer LayerFromJson(const nlohmann::json& j, const GPUParticleLayer& defaults)
{
	GPUParticleLayer l;
	l.Shape = ShapeFromJson(j, defaults.Shape);
	l.Count = j.value("count", defaults.Count);
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
	};
}

static GPUParticleParams ParamsFromJson(const nlohmann::json& j)
{
	GPUParticleParams p;
	p.MaxParticleNum = j.value("maxParticleNum", p.MaxParticleNum);
	p.EmitMode = EmitModeFromString(j.value("emitMode", std::string("Burst")));
	p.EmitInterval = j.value("emitInterval", p.EmitInterval);

	if (j.contains("layers") && j.at("layers").is_array() && !j.at("layers").empty())
	{
		p.Layers.clear();
		for (auto& entry : j.at("layers"))
		{
			p.Layers.push_back(LayerFromJson(entry, GPUParticleLayer{}));
		}
	}

	if (j.contains("gravity"))
	{
		auto& g = j.at("gravity");
		p.Gravity = { g[0], g[1], g[2] };
	}

	p.TexturePath = j.value("texture", std::string());
	p.BlendMode = BlendModeFromString(j.value("blendMode", std::string("Add")));

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

	if (j.contains("pos"))
	{
		auto& v = j.at("pos");
		def.Pos = { v[0], v[1], v[2] };
	}
	if (j.contains("rotate"))
	{
		auto& v = j.at("rotate");
		def.Rotate = { v[0], v[1], v[2] };
	}
	if (j.contains("scale"))
	{
		auto& v = j.at("scale");
		def.Scale = { v[0], v[1], v[2] };
	}
	if (j.contains("params"))
	{
		def.Params = ParamsFromJson(j.at("params"));
	}

	return def;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// DirectionalSparkLayer / WeaponClashEffectParams ⇔ JSON
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static nlohmann::json SparkLayerToJson(const DirectionalSparkLayer& l)
{
	nlohmann::json j = ShapeToJson(l.Shape);
	j["countParry"] = l.CountParry;
	j["countBlock"] = l.CountBlock;
	return j;
}

// defaultsで渡された値を初期値として使い、JSON側に無いキーは元の値のまま残す
// (WeaponClashEffectParamsのコンストラクタが持つデフォルト値をベースにできるようにする為)
static DirectionalSparkLayer SparkLayerFromJson(const nlohmann::json& j, const DirectionalSparkLayer& defaults)
{
	DirectionalSparkLayer l;
	l.Shape = ShapeFromJson(j, defaults.Shape);
	l.CountParry = j.value("countParry", defaults.CountParry);
	l.CountBlock = j.value("countBlock", defaults.CountBlock);
	return l;
}

static nlohmann::json WeaponClashToJson(const WeaponClashEffectParams& p)
{
	return nlohmann::json{
		{ "maxParticleNum", p.MaxParticleNum },
		{ "main",  SparkLayerToJson(p.Main) },
		{ "ember", SparkLayerToJson(p.Ember) },
	};
}

// jが空(キー無し)の場合はdefaults(=WeaponClashEffectParams()のデフォルト値)をそのまま返す
static WeaponClashEffectParams WeaponClashFromJson(const nlohmann::json& j)
{
	WeaponClashEffectParams p;	// デフォルト値(元のOnWeaponClash()ハードコード値と同一)

	p.MaxParticleNum = j.value("maxParticleNum", p.MaxParticleNum);

	if (j.contains("main"))
	{
		p.Main = SparkLayerFromJson(j.at("main"), p.Main);
	}
	if (j.contains("ember"))
	{
		p.Ember = SparkLayerFromJson(j.at("ember"), p.Ember);
	}

	return p;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクトデータ一式の読み込み/書き出し
//	JSON全体は { "effects": [...], "weaponClash": {...} } という形の1オブジェクト
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDataLoader::Load(const std::string& path, EffectDataFile& out)
{
	nlohmann::json j;
	if (!JsonLoader::Load(path, j)) { return false; }

	EffectDataFile data;

	if (j.contains("effects"))
	{
		for (auto& entry : j.at("effects"))
		{
			data.Effects.push_back(DefinitionFromJson(entry));
		}
	}

	if (j.contains("weaponClash"))
	{
		data.WeaponClash = WeaponClashFromJson(j.at("weaponClash"));
	}
	// weaponClashキーが無い場合はWeaponClashEffectParams()のデフォルト値のまま
	// (元のOnWeaponClash()ハードコード値と同じ見た目になる)

	out = std::move(data);
	return true;
}

bool EffectDataLoader::Save(const std::string& path, const EffectDataFile& data)
{
	nlohmann::json effectsJson = nlohmann::json::array();
	for (auto& def : data.Effects)
	{
		effectsJson.push_back(DefinitionToJson(def));
	}

	nlohmann::json j = {
		{ "effects",     effectsJson },
		{ "weaponClash", WeaponClashToJson(data.WeaponClash) },
	};

	return JsonLoader::Save(path, j);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Name ⇔ EffectId
// ※EffectIdに項目を追加した場合はここにも追加すること
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDataLoader::NameToEffectId(const std::string& name, EffectId& out)
{
	if (name == "HitSpark") { out = EffectId::HitSpark;      return true; }
	if (name == "FootDust") { out = EffectId::FootDust;      return true; }
	if (name == "BloodSplatter") { out = EffectId::BloodSplatter; return true; }
	return false;
}

std::string EffectDataLoader::EffectIdToName(EffectId id)
{
	switch (id)
	{
	case EffectId::HitSpark:      return "HitSpark";
	case EffectId::FootDust:      return "FootDust";
	case EffectId::BloodSplatter: return "BloodSplatter";
	}
	return "";
}