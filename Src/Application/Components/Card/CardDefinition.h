#pragma once

// これはComponentではない。読み込み専用のマスターデータ。
struct CardDefinition {
	std::string cardId;      // "card_001" など
	std::string name;
	int cost = 0;
	std::string cardType;    // "Creature" / "Spell" など
	std::string effectScript; // 効果解決に使う識別子
	// ...アートワークのパス、フレーバーテキスト等
};