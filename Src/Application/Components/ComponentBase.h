#pragma once
#include "ComponentTypeID.h"

//=============================================================================
// ComponentBase
//
// ECSにおけるコンポーネントの基底クラス。
//
// 【前回のコンポーネント志向との最大の違い】
//
//  コンポーネント志向:
//    ComponentBaseは「Update()」「OnEnable()」等の振る舞いを持っていた。
//    コンポーネント自身が「何をするか」を知っていた。
//
//  ECS（このクラス）:
//    ComponentBaseは「型IDの払い出し」だけを担う。
//    振る舞いは一切持たない。
//    「何をするか」はSystemが担う。
//    ComponentBaseの派生クラスは「データの構造体」として実装する。
//
// 【なぜvirtual デストラクタだけ持つのか】
//  RegistryがComponentBaseのポインタ経由で派生クラスを削除するため、
//  仮想デストラクタが必要。それ以外のvirtual関数は持たない。
//=============================================================================
class ComponentBase
{
public:
	virtual ~ComponentBase() = default;

	// コピー・ムーブ禁止（Registryが所有権を持つため）
	ComponentBase(const ComponentBase&) = delete;
	ComponentBase& operator=(const ComponentBase&) = delete;
	ComponentBase(ComponentBase&&) = delete;
	ComponentBase& operator=(ComponentBase&&) = delete;

protected:
	ComponentBase() = default;
};

//=============================================================================
// ComponentData<T>
//
// 派生クラスを作る際に継承するCRTPテンプレート。
// 継承するだけで自動的に型IDが管理される。
//
// 使い方:
//   struct TransformComponent : public ComponentData<TransformComponent>
//   {
//       Math::Vector3 position;
//       Math::Vector3 rotation;
//   };
//=============================================================================
template<class T>
class ComponentData : public ComponentBase
{
public:
	// この型のコンポーネントIDを返す（Registry内の格納場所の添字になる）
	static size_t GetTypeID()
	{
		return ComponentTypeID::Get<T>();
	}
};