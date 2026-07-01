#pragma once

// 前方宣言
class Registry;

//=============================================================================
// SystemBase
//
// ECSにおける「処理」の基底クラス。
//
// 【コンポーネント志向との違い】
//  コンポーネント志向: 処理はコンポーネント自身が持つ（comp->Update()）
//  ECS            : 処理はSystemが担い、コンポーネントはデータのみ
//
// 【Systemの責務】
//  ・Registryからクエリ(View)で対象Entityを取得する
//  ・取得したコンポーネントのデータを読み書きする
//  ・1つのSystemは1つの「関心事」だけを担う（単一責任）
//
// 【実行順序の管理】
//  SystemManagerがSystemのリストを順番に実行する。
//  依存関係（「AはBの後に実行しなければならない」）は
//  SystemManagerへの登録順で制御する。
//
// 例:
//   class CameraRotateSystem : public SystemBase
//   {
//   public:
//       void Update(Registry& registry) override { ... }
//   };
//=============================================================================
class SystemBase
{
public:
	virtual ~SystemBase() = default;

	// 毎フレーム呼ばれる更新処理
	// registryを通じて対象コンポーネントを取得して処理する
	virtual void Update(Registry& registry) = 0;
};