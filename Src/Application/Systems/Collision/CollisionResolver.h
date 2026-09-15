#pragma once

#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Components/Physics/Motion/MotionComposerComponent.h"

// ============================================================
// CollisionResolver
//
// 押し返し(位置補正)の「集約」と「適用」だけを担当するクラス。
//
// 以前はCollisionSystem::Update()の検出ループの中で、重なりを
// 見つけたその場でTranslate()していた。しかしこれだと、
//   ・ペア(A,B)を解決してAを動かした直後に、後続のペア(A,C)の
//     判定が既に動いた後の位置で行われ、コライダーの評価順
//     (=ObjectManagerへの登録順)で結果が変わる
//   ・その回の重なり量(overlap)としてStay通知に記録される値が、
//     押し返し「前」の値のままになる
// という問題があった。これはMotionComposerComponentを導入した理由
// (「各コンポーネントが個別にTransformを書き換えると呼び出し順に
//  結果が左右される」)と同じ形の問題である。
//
// そこでCollisionSystem::Update()の検出フェーズ(Phase 1)は、
// 位置を一切書き換えず、AddContact()で「このコライダーをこの向きに
// これだけ押し出すべき」という接触情報を溜めるだけにする。
// 全ペアの検出が終わった後にResolve()を1回呼び、コライダーごとに
// 接触をまとめて1回だけ位置を確定させる。
//
// --- 複数接触の集約方法 ----------------------------------------
// 1つのコライダーが同じフレームで複数の相手と接触した場合(角に
// 挟まれた等)、単純に全接触のpush量を合算すると二重に押し出されて
// しまう。かといって真の同時解決(全接触を連立させて解く)は
// 実装コストが高い。
//
// ここでは中間案として、「既にこの向きへどれだけ押し出し済みかを見て、
// 足りない分だけ追加する」という逐次的な集約を行う(Resolve()内の
// ループ参照)。直交する向きの接触同士は干渉せず両方フルに適用され、
// 同じ/近い向きの接触は二重に押し出されない。真の同時解決ではない
// 近似だが、ペアごとに逐次Translate()していた以前の方式より安定する。
// ============================================================

// 1件の接触が、片方のコライダーに要求する押し出し量。
struct PushContact
{
	ColliderComponent* collider = nullptr; // 押される側
	Math::Vector3 normal;                  // 押し出す向き(正規化済み)
	float depth = 0.0f;                    // その向きに押し出すべき量
};

class CollisionResolver
{
public:
	// CollisionSystem::Update()の検出ループが、押し返しが必要な重なりを
	// 見つけるたびに呼ぶ。depth<=0は無視する。
	void AddContact(ColliderComponent* collider, const Math::Vector3& normal, float depth)
	{
		if (collider == nullptr || depth <= 0.0f) return;
		contacts_.push_back({ collider, normal, depth });
	}

	// 全ペアの検出が終わった後に1回呼ぶ。コライダーごとに接触を集約し、
	// MotionComposerComponentがあればApplyCollisionCorrection()へ、
	// 無ければ直接Translate()する(MotionComposerを持たない単純な
	// オブジェクト向けのフォールバック)。
	void Resolve()
	{
		std::unordered_map<ColliderComponent*, Math::Vector3> corrections;

		for (const PushContact& contact : contacts_) {
			Math::Vector3& total = corrections[contact.collider];

			// 既にこの向きへどれだけ押し出し済みかを見て、不足分だけ足す。
			const float already = total.Dot(contact.normal);
			const float remaining = contact.depth - already;
			if (remaining > 0.0f) {
				total += contact.normal * remaining;
			}
		}

		for (auto& [collider, delta] : corrections) {
			if (delta.LengthSquared() <= 1e-10f) continue;

			if (MotionComposerComponent* composer =
				collider->GetOwner()->GetComponent<MotionComposerComponent>()) {
				composer->ApplyCollisionCorrection(delta);
			}
			else {
				collider->Translate(delta);
			}
		}

		contacts_.clear();
	}

private:
	std::vector<PushContact> contacts_;
};
