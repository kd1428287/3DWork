#pragma once
#include <memory>
#include <string>

// 前方宣言でコンパイル時間を短縮
class GameObject;
class ObjectManager;

// ============================================================
// プレイヤー（自機）の生成に特化したファクトリークラス。
// 無駄なレジストリを排除し、明快な単一責任を持つ。
// ============================================================
class PlayerFactory {
public:
	PlayerFactory() = default;
	~PlayerFactory() = default;

	// コピー・ムーブ禁止
	PlayerFactory(const PlayerFactory&) = delete;
	PlayerFactory& operator=(const PlayerFactory&) = delete;

	/**
	 * @brief プレイヤーキャラクターを生成し、必要なコンポーネントをアタッチして返す
	 * @param objectManager オブジェクト管理システム
	 * @param ownerPlayerId マルチプレイ等を見据えたプレイヤー識別ID（不要なら削除可）
	 * @return 生成されたGameObjectのスマートポインタ
	 */
	GameObject* CreatePlayer(ObjectManager& objectManager, int ownerPlayerId = 0);
};