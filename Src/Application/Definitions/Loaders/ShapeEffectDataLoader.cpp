#include "Application/main.h"
#include "Application/Definitions/Loaders/EffectDataLoader.h"	// JsonLoader/nlohmannをEffectDataLoaderと同じ経路で使う為
#include "ShapeEffectDataLoader.h"

namespace
{
	using Json = nlohmann::json;

	// キーの配列が「要素数count・全て数値」のときだけ読む(違えばfalseでoutに触れない)
	bool TryReadFloats(const Json& j, const char* key, float* out, size_t count)
	{
		if (!j.contains(key)) { return false; }

		const Json& v = j.at(key);
		if (!v.is_array() || v.size() != count) { return false; }
		for (const auto& e : v) { if (!e.is_number()) { return false; } }

		for (size_t i = 0; i < count; ++i) { out[i] = v[i].get<float>(); }
		return true;
	}

	float ReadFloat(const Json& j, const char* key, float defaultValue)
	{
		if (!j.contains(key) || !j.at(key).is_number()) { return defaultValue; }
		return j.at(key).get<float>();
	}

	std::string ReadString(const Json& j, const char* key)
	{
		if (!j.contains(key) || !j.at(key).is_string()) { return std::string(); }
		return j.at(key).get<std::string>();
	}

	bool ReadBool(const Json& j, const char* key, bool defaultValue)
	{
		if (!j.contains(key) || !j.at(key).is_boolean()) { return defaultValue; }
		return j.at(key).get<bool>();
	}

	Json Vec3ToJson(const Math::Vector3& v) { return Json::array({ v.x, v.y, v.z }); }

	// ShapeEffectShaderが扱えるのはAlphaと加算のみ(それ以外の文字列は加算になる)
	ParticleBlendMode BlendFromString(const std::string& s)
	{
		return (s == "Alpha") ? ParticleBlendMode::Alpha : ParticleBlendMode::Add;
	}

	const char* BlendToString(ParticleBlendMode m)
	{
		return (m == ParticleBlendMode::Alpha) ? "Alpha" : "Add";
	}

	// DrawPassFlagsはEffectDataLoaderと同じく、"drawPass"へ["Default","Bright"]の配列で持つ
	ParticleDrawPass DrawPassFlagsFromJson(const Json& j, ParticleDrawPass defaults)
	{
		if (!j.contains("drawPass") || !j.at("drawPass").is_array()) { return defaults; }

		ParticleDrawPass flags = static_cast<ParticleDrawPass>(0);
		bool any = false;
		for (const auto& entry : j.at("drawPass"))
		{
			if (!entry.is_string()) { continue; }

			const std::string s = entry.get<std::string>();
			if (s == "Default") { flags |= ParticleDrawPass::Default; any = true; }
			else if (s == "Bright") { flags |= ParticleDrawPass::Bright; any = true; }
		}
		return any ? flags : defaults;
	}

	Json DrawPassFlagsToJson(ParticleDrawPass flags)
	{
		Json arr = Json::array();
		if (KdHasDrawPassFlag(flags, ParticleDrawPass::Default)) { arr.push_back("Default"); }
		if (KdHasDrawPassFlag(flags, ParticleDrawPass::Bright)) { arr.push_back("Bright"); }
		return arr;
	}

	EffectKey KeyFromJson(const Json& j)
	{
		EffectKey key;
		float f[4] = {};

		if (TryReadFloats(j, "pos", f, 3)) { key.pos = { f[0], f[1], f[2] }; }
		if (TryReadFloats(j, "rotDeg", f, 3)) { key.rotDeg = { f[0], f[1], f[2] }; }
		if (TryReadFloats(j, "scale", f, 2)) { key.scale = { f[0], f[1] }; }
		key.alpha = ReadFloat(j, "alpha", key.alpha);
		key.ease = EaseTypeFromName(ReadString(j, "ease"));
		key.duration = std::max(0.0f, ReadFloat(j, "duration", key.duration));
		return key;
	}

	Json KeyToJson(const EffectKey& key)
	{
		return Json{
			{ "pos",      Vec3ToJson(key.pos) },
			{ "rotDeg",   Vec3ToJson(key.rotDeg) },
			{ "scale",    Json::array({ key.scale.x, key.scale.y }) },
			{ "alpha",    key.alpha },
			{ "ease",     EaseTypeName(key.ease) },
			{ "duration", key.duration },
		};
	}

	// ノイズはeffectmap.jsonと同じ形式("flags"は"Mask"/"Dissolve"の文字列配列)
	ParticleNoiseParams NoiseFromJson(const Json& j)
	{
		ParticleNoiseParams n;
		float f[4] = {};

		if (j.contains("flags") && j.at("flags").is_array())
		{
			for (const auto& entry : j.at("flags"))
			{
				if (!entry.is_string()) { continue; }

				const std::string flag = entry.get<std::string>();
				if (flag == "Mask") { n.flags |= ParticleNoiseFlag_Mask; }
				else if (flag == "Dissolve") { n.flags |= ParticleNoiseFlag_Dissolve; }
			}
		}

		n.noiseScale = ReadFloat(j, "noiseScale", n.noiseScale);
		if (TryReadFloats(j, "scroll", f, 2)) { n.scroll = { f[0], f[1] }; }
		n.maskPower = ReadFloat(j, "maskPower", n.maskPower);
		n.dissolveStart = ReadFloat(j, "dissolveStart", n.dissolveStart);
		n.dissolveSoftness = ReadFloat(j, "dissolveSoftness", n.dissolveSoftness);
		n.edgeWidth = ReadFloat(j, "edgeWidth", n.edgeWidth);
		if (TryReadFloats(j, "edgeColor", f, 4)) { n.edgeColor = { f[0], f[1], f[2], f[3] }; }

		// 0除算・範囲外を避ける(EffectDataLoaderと同じ下限/上限)
		n.noiseScale = std::max(n.noiseScale, 0.01f);
		n.maskPower = std::max(n.maskPower, 0.01f);
		n.dissolveStart = std::clamp(n.dissolveStart, 0.0f, 0.99f);
		n.dissolveSoftness = std::max(n.dissolveSoftness, 0.001f);
		n.edgeWidth = std::max(n.edgeWidth, 0.001f);
		return n;
	}

	Json NoiseToJson(const ParticleNoiseParams& n)
	{
		Json flags = Json::array();
		if (n.flags & ParticleNoiseFlag_Mask) { flags.push_back("Mask"); }
		if (n.flags & ParticleNoiseFlag_Dissolve) { flags.push_back("Dissolve"); }

		return Json{
			{ "flags",            flags },
			{ "noiseScale",       n.noiseScale },
			{ "scroll",           Json::array({ n.scroll.x, n.scroll.y }) },
			{ "maskPower",        n.maskPower },
			{ "dissolveStart",    n.dissolveStart },
			{ "dissolveSoftness", n.dissolveSoftness },
			{ "edgeWidth",        n.edgeWidth },
			{ "edgeColor",        Json::array({ n.edgeColor.x, n.edgeColor.y, n.edgeColor.z, n.edgeColor.w }) },
		};
	}

	ShapeEffectData EntryFromJson(const Json& j)
	{
		ShapeEffectData data;
		data.Name = ReadString(j, "name");

		if (j.contains("params") && j.at("params").is_object())
		{
			const Json& p = j.at("params");
			float f[4] = {};

			data.Params.TexturePath = ReadString(p, "texture");
			data.Params.BlendMode = BlendFromString(ReadString(p, "blendMode"));
			data.Params.DrawPassFlags = DrawPassFlagsFromJson(p, data.Params.DrawPassFlags);
			if (TryReadFloats(p, "color", f, 4)) { data.Params.Color = { f[0], f[1], f[2], f[3] }; }
			data.Params.Billboard = BillboardModeFromName(ReadString(p, "billboard"));
			data.Params.DepthOffset = ReadFloat(p, "depthOffset", data.Params.DepthOffset);
			data.Params.DepthTest = ReadBool(p, "depthTest", data.Params.DepthTest);
			data.Params.NoiseTexturePath = ReadString(p, "noiseTexture");
			if (p.contains("noise") && p.at("noise").is_object()) { data.Params.Noise = NoiseFromJson(p.at("noise")); }
		}

		if (j.contains("keys") && j.at("keys").is_array())
		{
			for (const auto& k : j.at("keys"))
			{
				if (k.is_object()) { data.Keys.push_back(KeyFromJson(k)); }
			}
		}
		return data;
	}

	Json EntryToJson(const ShapeEffectData& data)
	{
		Json keys = Json::array();
		for (const EffectKey& key : data.Keys) { keys.push_back(KeyToJson(key)); }

		return Json{
			{ "name", data.Name },
			{ "params", {
				{ "texture",   data.Params.TexturePath },
				{ "blendMode", BlendToString(data.Params.BlendMode) },
				{ "drawPass",  DrawPassFlagsToJson(data.Params.DrawPassFlags) },
				{ "color",     Json::array({ data.Params.Color.x, data.Params.Color.y, data.Params.Color.z, data.Params.Color.w }) },
				{ "billboard",   BillboardModeName(data.Params.Billboard) },
				{ "depthOffset", data.Params.DepthOffset },
				{ "depthTest",   data.Params.DepthTest },
				{ "noiseTexture", data.Params.NoiseTexturePath },
				{ "noise",        NoiseToJson(data.Params.Noise) },
			} },
			{ "keys", keys },
		};
	}
}

bool ShapeEffectDataLoader::Load(const std::string& path, ShapeEffectDataFile& out)
{
	try
	{
		Json j;
		if (!JsonLoader::Load(path, j)) { return false; }

		ShapeEffectDataFile data;

		if (j.contains("shapeEffects") && j.at("shapeEffects").is_array())
		{
			for (const auto& entry : j.at("shapeEffects"))
			{
				try
				{
					if (!entry.is_object()) { throw std::runtime_error("entry is not an object"); }

					ShapeEffectData effect = EntryFromJson(entry);
					if (effect.Name.empty()) { throw std::runtime_error("entry has no name"); }

					data.Effects.push_back(std::move(effect));
				}
				catch (const std::exception& e)
				{
					// 1件だけ壊れていても他は読み込みたいので、打ち切らずcontinueする
					std::cerr << "[ShapeEffectDataLoader] skipped an entry: " << e.what() << std::endl;
				}
			}
		}

		out = std::move(data);
		return true;
	}
	catch (const std::exception& e)
	{
		std::cerr << "[ShapeEffectDataLoader] Load failed: " << path << " : " << e.what() << std::endl;
		return false;
	}
}

bool ShapeEffectDataLoader::Save(const std::string& path, const ShapeEffectDataFile& data)
{
	try
	{
		Json effects = Json::array();
		for (const ShapeEffectData& effect : data.Effects) { effects.push_back(EntryToJson(effect)); }

		Json j = { { "shapeEffects", effects } };
		return JsonLoader::Save(path, j);
	}
	catch (const std::exception& e)
	{
		std::cerr << "[ShapeEffectDataLoader] Save failed: " << path << " : " << e.what() << std::endl;
		return false;
	}
}
