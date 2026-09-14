#pragma once

#include "Application/Core/EventBus/Events/CollisionEvents.h"
#include "CollisionMath.h"
#include "Application/Components/Physics/Collision/ColliderComponent.h"

// ============================================================
// 当たり判定イベントの生成と配信だけを担当するクラス。
//
// 以前はCollisionSystem::Update()が
//   ・検出(誰と誰の、どの形状同士が重なっているか)
//   ・押し返し(位置補正)
//   ・前フレームとの差分によるEnter/Exit/Stayの記録
//   ・イベントの生成と配信
// を1クラスに抱え込んでいた。このうち「重なり結果(OverlapResult)が
// 確定した後、それをEnter/Exit/Stayのイベントに変換して適切な宛先に
// 届ける」という通知の責務だけをこのクラスに切り出している。
//
// CollisionSystemは「誰と誰が、いつ重なったか」の判定と記録に専念し、
// 通知の方法(誰に、どんな形で届けるか)を変えたい場合はこのクラスだけを
// 触ればよい形にするのが狙い。
//
// 宛先は各GameObjectのローカルバス(GameObject::GetLocalEventBus())。
// シーン共有のEventBusに投げると、無関係な購読者にまで毎回イベントが
// 届いてself==自分かのフィルタが必要になるため、宛先が確定している
// このケースでは最初からローカルバスへ直接届ける。
// ============================================================
class CollisionEventPublisher {
public:
	using CollisionEnterEvent = Events::Collision::CollisionEnterEvent;
	using CollisionExitEvent = Events::Collision::CollisionExitEvent;
	using CollisionStayEvent = Events::Collision::CollisionStayEvent;

	// overlapのhitNormalは「aをbから押し出す向き」で計算されている前提。
	// a視点のイベントはそのまま、b視点のイベントは向きを反転させて使う。
	// それぞれ自分自身(self)のローカルバスにだけ発行する。
	static void PublishEnter(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB,
		const CollisionMath::OverlapResult& overlap) {

		PublishToOwner(a, MakeEnterEvent(a, shapeA, b, shapeB, overlap, false));
		PublishToOwner(b, MakeEnterEvent(b, shapeB, a, shapeA, overlap, true));
	}

	static void PublishExit(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB) {

		PublishToOwner(a, MakeExitEvent(a, shapeA, b, shapeB));
		PublishToOwner(b, MakeExitEvent(b, shapeB, a, shapeA));
	}

	// wantsStayA/wantsStayBは呼び出し側(CollisionSystem::Update())が
	// 「shapeAだけ欲しい」「shapeBだけ欲しい」というケースを区別済みで渡す。
	// 片側だけ発行することを許すために個別に受け取る。
	static void PublishStay(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB,
		const CollisionMath::OverlapResult& overlap,
		bool wantsStayA, bool wantsStayB) {

		if (wantsStayA) {
			PublishToOwner(a, MakeStayEvent(a, shapeA, b, shapeB, overlap, false));
		}
		if (wantsStayB) {
			PublishToOwner(b, MakeStayEvent(b, shapeB, a, shapeA, overlap, true));
		}
	}

private:
	static CollisionEnterEvent MakeEnterEvent(
		ColliderComponent* self, const CollisionShapeEntry& selfShape,
		ColliderComponent* other, const CollisionShapeEntry& otherShape,
		const CollisionMath::OverlapResult& hitResult, bool flipNormal) {

		CollisionEnterEvent e;
		e.selfObject = self->GetOwner();
		e.selfCollider = self;
		e.selfShapeName = selfShape.name;
		e.otherObject = other->GetOwner();
		e.otherCollider = other;
		e.otherShapeName = otherShape.name;
		e.hitResult = hitResult;
		if (flipNormal) {
			e.hitResult.hitNormal = -e.hitResult.hitNormal;
		}
		return e;
	}

	static CollisionExitEvent MakeExitEvent(
		ColliderComponent* self, const CollisionShapeEntry& selfShape,
		ColliderComponent* other, const CollisionShapeEntry& otherShape) {

		CollisionExitEvent e;
		e.selfObject = self->GetOwner();
		e.selfCollider = self;
		e.selfShapeName = selfShape.name;
		e.otherObject = other->GetOwner();
		e.otherCollider = other;
		e.otherShapeName = otherShape.name;
		return e;
	}

	static CollisionStayEvent MakeStayEvent(
		ColliderComponent* self, const CollisionShapeEntry& selfShape,
		ColliderComponent* other, const CollisionShapeEntry& otherShape,
		const CollisionMath::OverlapResult& hitResult, bool flipNormal) {

		// フィールド構成はCollisionEnterEventと同一。型を分けているのは
		// 購読側がEnter/Stayを別々に選べるようにするため。
		CollisionStayEvent e;
		e.selfObject = self->GetOwner();
		e.selfCollider = self;
		e.selfShapeName = selfShape.name;
		e.otherObject = other->GetOwner();
		e.otherCollider = other;
		e.otherShapeName = otherShape.name;
		e.hitResult = hitResult;
		if (flipNormal) {
			e.hitResult.hitNormal = -e.hitResult.hitNormal;
		}
		return e;
	}

	// 宛先(self)のGameObjectが持つローカルバスにだけ発行する。
	static void PublishToOwner(ColliderComponent* self, auto&& event) {
		self->GetOwner()->GetLocalEventBus().Publish(std::forward<decltype(event)>(event));
	}
};
