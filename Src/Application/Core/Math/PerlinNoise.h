#pragma once

class PerlinNoise {
private:
	std::vector<int> p;

public:
	explicit PerlinNoise(unsigned int seed = std::default_random_engine::default_seed) {
		p.resize(256);
		std::iota(p.begin(), p.end(), 0);
		std::default_random_engine engine(seed);
		std::shuffle(p.begin(), p.end(), engine);
		p.insert(p.end(), p.begin(), p.end());
	}

	// Vector3版 (ベースとなる3Dノイズ)
	float noise(const Math::Vector3& pos) const {
		float x = pos.x, y = pos.y, z = pos.z;

		int X = static_cast<int>(std::floor(x)) & 255;
		int Y = static_cast<int>(std::floor(y)) & 255;
		int Z = static_cast<int>(std::floor(z)) & 255;

		x -= std::floor(x);
		y -= std::floor(y);
		z -= std::floor(z);

		float u = fade(x), v = fade(y), w = fade(z);

		int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
		int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;

		return lerp(w, lerp(v, lerp(u, grad(p[AA], x, y, z),
			grad(p[BA], x - 1, y, z)),
			lerp(u, grad(p[AB], x, y - 1, z),
				grad(p[BB], x - 1, y - 1, z))),
			lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1),
				grad(p[BA + 1], x - 1, y, z - 1)),
				lerp(u, grad(p[AB + 1], x, y - 1, z - 1),
					grad(p[BB + 1], x - 1, y - 1, z - 1))));
	}

	// Vector2版 (Z=0として3Dノイズを呼び出すオーバーロード)
	float noise(const Math::Vector2& pos) const {
		return noise(Math::Vector3(pos.x, pos.y, 0.0f));
	}

	// Vector2向けのFBM (フラクタルブラウン運動)
	float fbm(const Math::Vector2& pos, int octaves, float persistence = 0.5f, float lacunarity = 2.0f) const {
		float total = 0.0f;
		float frequency = 1.0f;
		float amplitude = 1.0f;
		float maxValue = 0.0f;
		Math::Vector3 currentPos(pos.x, pos.y, 0.0f);

		for (int i = 0; i < octaves; ++i) {
			total += noise(currentPos * frequency) * amplitude;
			maxValue += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return total / maxValue;
	}

private:
	static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
	static float lerp(float t, float a, float b) { return a + t * (b - a); }
	static float grad(int hash, float x, float y, float z) {
		int h = hash & 15;
		float u = h < 8 ? x : y;
		float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
		return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
	}
};