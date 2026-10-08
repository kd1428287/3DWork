#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <random>
#include <utility>

// 古典的Perlinノイズ(出力はおおよそ-1〜1)
// 通常は Shared() を使う。再現性が必要な用途だけ、シードを指定して個別に生成する。
class PerlinNoise {
public:
	static constexpr unsigned int kDefaultSeed = 1337u;

	explicit PerlinNoise(unsigned int seed = kDefaultSeed) {
		// 乱数エンジンと自前シャッフルで、環境差なく同じ並びを得る
		std::iota(p_.begin(), p_.begin() + 256, 0);
		std::mt19937 rng(seed);
		for (int i = 255; i > 0; --i) {
			std::swap(p_[i], p_[rng() % (i + 1)]);
		}
		// 添字のラップを省くため前半を後半へ複製
		std::copy(p_.begin(), p_.begin() + 256, p_.begin() + 256);
	}

	// プロセス共通のデフォルトインスタンス
	static const PerlinNoise& Shared() {
		static const PerlinNoise instance;
		return instance;
	}

	// GPU転送用に順列テーブル(512要素)を公開する
	const std::array<int, 512>& GetTable() const { return p_; }

	// 3Dノイズ(ベース)
	float noise(const Math::Vector3& pos) const {
		float x = pos.x, y = pos.y, z = pos.z;

		const int X = static_cast<int>(std::floor(x)) & 255;
		const int Y = static_cast<int>(std::floor(y)) & 255;
		const int Z = static_cast<int>(std::floor(z)) & 255;

		x -= std::floor(x);
		y -= std::floor(y);
		z -= std::floor(z);

		const float u = Fade(x), v = Fade(y), w = Fade(z);

		const int A = p_[X] + Y, AA = p_[A] + Z, AB = p_[A + 1] + Z;
		const int B = p_[X + 1] + Y, BA = p_[B] + Z, BB = p_[B + 1] + Z;

		// z=0面 → z=1面の順に、x方向→y方向へ補間する
		const float y0z0 = Lerp(Lerp(Grad(p_[AA], x, y, z),         Grad(p_[BA], x - 1, y, z),         u),
		                        Lerp(Grad(p_[AB], x, y - 1, z),     Grad(p_[BB], x - 1, y - 1, z),     u), v);
		const float y0z1 = Lerp(Lerp(Grad(p_[AA + 1], x, y, z - 1), Grad(p_[BA + 1], x - 1, y, z - 1), u),
		                        Lerp(Grad(p_[AB + 1], x, y - 1, z - 1), Grad(p_[BB + 1], x - 1, y - 1, z - 1), u), v);
		return Lerp(y0z0, y0z1, w);
	}

	// Vector2版(Z=0として3Dノイズを呼ぶ)
	float noise(const Math::Vector2& pos) const {
		return noise(Math::Vector3(pos.x, pos.y, 0.0f));
	}

	// Vector2向けFBM(結果は-1〜1に正規化)
	float fbm(const Math::Vector2& pos, int octaves, float persistence = 0.5f, float lacunarity = 2.0f) const {
		float total = 0.0f;
		float frequency = 1.0f;
		float amplitude = 1.0f;
		float maxValue = 0.0f;
		const Math::Vector3 basePos(pos.x, pos.y, 0.0f);

		for (int i = 0; i < octaves; ++i) {
			total += noise(basePos * frequency) * amplitude;
			maxValue += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return total / maxValue;
	}

private:
	static float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
	static float Lerp(float a, float b, float t) { return a + t * (b - a); }
	static float Grad(int hash, float x, float y, float z) {
		const int h = hash & 15;
		const float u = h < 8 ? x : y;
		const float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
		return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
	}

	std::array<int, 512> p_{};
};
