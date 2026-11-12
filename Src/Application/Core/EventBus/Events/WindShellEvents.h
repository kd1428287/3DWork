#pragma once

#include <string>

// 風シェル用イベント(配置先: Application/Core/EventBus/Events/)。EventとMath::Vector3が見える状態でインクルードする事
namespace Events
{
	// 風シェルを出す。同じキーが既にあれば置き換える
	struct WindShellAttach : Event
	{
		std::string		InstanceKey;
		std::string		PresetName;
		Math::Vector3	Position;
		Math::Vector3	Axis = { 0.0f, 1.0f, 0.0f };	// 風の軸(進行方向)
	};

	// 毎フレームの変換更新。Progress(0〜1)はチャージ量などの進行度
	struct WindShellUpdate : Event
	{
		std::string		InstanceKey;
		Math::Vector3	Position;
		Math::Vector3	Axis = { 0.0f, 1.0f, 0.0f };
		float			Progress = 0.0f;
	};

	// 消去(ディゾルブ)へ移す
	struct WindShellStop : Event
	{
		std::string		InstanceKey;
	};

	// 即時破棄
	struct WindShellDetach : Event
	{
		std::string		InstanceKey;
	};
}