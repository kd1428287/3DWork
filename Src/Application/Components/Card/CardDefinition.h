#pragma once

enum class CardType
{
	Attack,
	Defence,
	Move,
	Special
};

// これはComponentではない。読み込み専用のマスターデータ。
struct CardDefinition {
	int cardId = 0;      
	std::string name;
	CardType type;
	int cost = 0;
	int latency = 0;
	std::string effectScript; // 効果解決に使う識別子
	// ...アートワークのパス、フレーバーテキスト等
};