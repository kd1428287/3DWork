#pragma once
#include "TwoBoneIK.h"
#include "../Animation/SkeletonComponent.h"
#include "../Transform/TransformComponent.h"
#include "../Tags/IAnimationPostProcess.h"

// ============================================================
// TwoBoneIKComponent
//
// SkeletonComponentが計算したFK(アニメーション由来)のポーズに対し、
// 指定した3関節チェーン(root-mid-end。例: 肩-肘-手首、股関節-膝-足首)の
// 末端(end)をワールド空間の目標位置(target)へ近づけるよう、root/mid
// 関節のローカル回転を上書きする。腕・脚のどちらにも同じ実装を使い回せる
// (TwoBoneIK::Solveがチェーンの意味を一切知らないため)。
//
// --- 実行順序・複数チェーンについて -----------------------------------
// IAnimationPostProcessを実装しており、SolveIK()はSkeletonComponentの
// PostUpdate()(gameplayロジックのUpdate()より"後")から、具体的な型を
// 問わずGetTagged<IAnimationPostProcess>()経由で呼ばれる。SetTarget()が
// Update()内でのgameplay結果に依存するケース(照準・接地IK等)を想定した
// 順序。FKはこれより前(SkeletonComponent::PreUpdate())に完了済みなので、
// SolveIK()内で参照するFKポーズは同フレームのものであることが保証される。
//
// GameObjectは型ごとに1インスタンストまでしか持てない(GameObject.h
// 参照)ため、左腕・右腕のように複数チェーンを同一GameObjectへ同時に
// 付けたい場合は、本クラスをそのまま複数付けるのではなく、下記のように
// 型を分けるだけの薄いサブクラスを用意すること:
//
//   class LeftArmIKComponent : public TwoBoneIKComponent {
//   public:
//       using TwoBoneIKComponent::TwoBoneIKComponent;
//   };
//
// SolveIK()自体はローカル回転の書き込みのみ行い、CalcNodeMatrices()は
// 呼ばない(呼ぶと、同一SkeletonComponentに複数のIAnimationPostProcessが
// 付いている場合にその数だけ再計算が重複してしまうため)。書き換え後の
// 再計算は、SkeletonComponent::PostUpdate()が全SolveIK()呼び出しの後で
// 1回だけFinalize()を呼ぶことでまとめて行う。これにより、mid/end以下の
// 子ボーン(BoneSocketComponent経由で武器を追従させている手首等)の
// ワールド行列も同フレーム内で正しく更新される。
//
// --- 現在の実装の割り切り --------------------------------------------
// ・endボーン自身の向き(手首の捻り等)は補正しない。root/midの回転
//   だけを解いて、end位置をtargetへ近づけるところまでに留めている。
// ・ボーンの解決(FindWorkNode()による名前検索)は初回のSolveIK()呼び出し
//   時に一度だけ行い、以降は解決済みのNode*を直接使う(RootMotionExtractor
//   と同じ方式)。初回に見つからなかった場合は毎フレーム再探索しない。
// ・KdModelWork::FindNode()に、書き込み可能なNode*を返す非const版が
//   存在する前提で実装している。無ければSkeletonComponent側に
//   同等のpublicメソッドを1つ追加すること。
// ============================================================
class TwoBoneIKComponent : public ComponentBase, public IAnimationPostProcess {
public:
	// rootParentBoneName: rootボーンの親(例: 肩チェーンなら"mixamorig:RightShoulder")。
	//                     rootのローカル回転はこの親から見た相対回転のため必要。
	// rootBoneName/midBoneName/endBoneName: チェーン本体(例: RightArm/RightForeArm/RightHand)。
	TwoBoneIKComponent(GameObject* owner,
		std::string rootParentBoneName, std::string rootBoneName,
		std::string midBoneName, std::string endBoneName)
		: ComponentBase(owner)
		, rootParentBoneName_(std::move(rootParentBoneName))
		, rootBoneName_(std::move(rootBoneName))
		, midBoneName_(std::move(midBoneName))
		, endBoneName_(std::move(endBoneName)) {}

	void Start() override {
		skeleton_ = GetOwner()->GetComponent<SkeletonComponent>();
		ownerTransform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	// このフレームだけIKを効かせたい時に呼ぶ。毎フレーム呼び続ければ
	// 常時IKになる。呼ばれなかったフレームはFKのポーズがそのまま使われる。
	void SetTarget(const Math::Vector3& worldTarget, const Math::Vector3& poleVector) {
		hasTarget_ = true;
		target_ = worldTarget;
		poleVector_ = poleVector;
	}

	void ClearTarget() { hasTarget_ = false; }

	// IAnimationPostProcess実装。SkeletonComponent::PostUpdate()から、
	// gameplayロジックのUpdate()より後に呼ばれる。ローカル回転の
	// 書き込みのみを行い、CalcNodeMatrices()の呼び出しは呼び出し元
	// (SkeletonComponent::PostUpdate()の一括Finalize()呼び出し)に委ねる
	// (詳細はファイル冒頭の「実行順序・複数チェーンについて」参照)。
	void SolveIK() override {
		if (!hasTarget_ || skeleton_ == nullptr) return;

		ResolveBonesIfNeeded();
		if (rootParent_ == nullptr || root_ == nullptr || mid_ == nullptr || end_ == nullptr) return;
		KdModelWork::Node* rootParent = rootParent_;
		KdModelWork::Node* root = root_;
		KdModelWork::Node* mid = mid_;
		KdModelWork::Node* end = end_;

		// 現在(FK)のモデル空間での回転・位置を取得する。オーナー
		// (GameObject)の回転はrootParent/root/mid全てに等しく乗るだけで、
		// Inverse(parent)*childの計算過程で相殺されるため、ここでは
		// オーナーのTransformを合成せず、モデル空間のm_worldTransformを
		// そのまま扱っても最終的なローカル回転の結果は変わらない。
		Math::Vector3 dummyScale, dummyPos;
		Math::Quaternion rootParentWorldRot, rootWorldRot, midWorldRot;
		rootParent->m_worldTransform.Decompose(dummyScale, rootParentWorldRot, dummyPos);
		root->m_worldTransform.Decompose(dummyScale, rootWorldRot, dummyPos);
		mid->m_worldTransform.Decompose(dummyScale, midWorldRot, dummyPos);

		Math::Vector3 rootWorldPos, midWorldPos, endWorldPos;
		Math::Quaternion dummyRot;
		root->m_worldTransform.Decompose(dummyScale, dummyRot, rootWorldPos);
		mid->m_worldTransform.Decompose(dummyScale, dummyRot, midWorldPos);
		end->m_worldTransform.Decompose(dummyScale, dummyRot, endWorldPos);

		// targetはワールド空間で渡される想定のため、オーナーのUnscaled行列の
		// 逆行列でモデル空間へ変換する(座標系をrootPos等と揃えるため)。
		Math::Vector3 targetModelSpace = target_;
		Math::Vector3 poleModelSpace = poleVector_;
		if (ownerTransform_ != nullptr) {
			const Math::Matrix inv = ownerTransform_->GetUnscaledMatrix().Invert();
			targetModelSpace = Math::Vector3::Transform(target_, inv);
			poleModelSpace = Math::Vector3::TransformNormal(poleVector_, inv);
		}

		const TwoBoneIK::Result result = TwoBoneIK::Solve(
			rootWorldPos, midWorldPos, endWorldPos,
			rootWorldRot, midWorldRot,
			targetModelSpace, poleModelSpace);

		// ワールド(モデル空間)回転→ローカル回転への変換。
		Math::Quaternion rootParentWorldRotInv;
		rootParentWorldRot.Inverse(rootParentWorldRotInv);
		const Math::Quaternion rootNewLocalRot = rootParentWorldRotInv * result.rootWorldRotation;

		Math::Quaternion rootNewWorldRotInv;
		result.rootWorldRotation.Inverse(rootNewWorldRotInv);
		const Math::Quaternion midNewLocalRot = rootNewWorldRotInv * result.midWorldRotation;

		// 既存のm_localTransformから位置・スケール成分は引き継ぎ、
		// 回転だけを差し替える(IKの対象は回転のみ)。
		Math::Vector3 rootLocalScale, rootLocalPos, midLocalScale, midLocalPos;
		root->m_localTransform.Decompose(rootLocalScale, dummyRot, rootLocalPos);
		mid->m_localTransform.Decompose(midLocalScale, dummyRot, midLocalPos);

		root->m_localTransform =
			Math::Matrix::CreateScale(rootLocalScale)
			* Math::Matrix::CreateFromQuaternion(rootNewLocalRot)
			* Math::Matrix::CreateTranslation(rootLocalPos);
		mid->m_localTransform =
			Math::Matrix::CreateScale(midLocalScale)
			* Math::Matrix::CreateFromQuaternion(midNewLocalRot)
			* Math::Matrix::CreateTranslation(midLocalPos);

		// ここではCalcNodeMatrices()を呼ばない。SolveIK()はローカル回転の
		// 書き込みだけを行う関数であり、再計算はSkeletonComponent::
		// PostUpdate()側の一括Finalize()呼び出しで行われる想定
		// (詳細はファイル冒頭の「実行順序・複数チェーンについて」参照)。
	}

private:
	// ボーンの解決を初回のみ行い、以降は文字列検索せずポインタを直接使う
	// (RootMotionExtractorと同じ考え方。見つからなくても毎フレーム
	// 再探索はしない — モデル未ロード等で一時的に見つからない場合は
	// そのまま無効なチェーンとして扱われる)。
	void ResolveBonesIfNeeded() {
		if (bonesResolved_) return;

		KdModelWork& model = skeleton_->WorkModel();
		rootParent_ = model.FindWorkNode(rootParentBoneName_);
		root_ = model.FindWorkNode(rootBoneName_);
		mid_ = model.FindWorkNode(midBoneName_);
		end_ = model.FindWorkNode(endBoneName_);
		bonesResolved_ = true;
	}

	SkeletonComponent* skeleton_ = nullptr;
	TransformComponent* ownerTransform_ = nullptr; // 兄弟コンポーネント

	std::string rootParentBoneName_;
	std::string rootBoneName_;
	std::string midBoneName_;
	std::string endBoneName_;

	// 上記4つの名前から一度だけ解決したノードポインタのキャッシュ。
	KdModelWork::Node* rootParent_ = nullptr;
	KdModelWork::Node* root_ = nullptr;
	KdModelWork::Node* mid_ = nullptr;
	KdModelWork::Node* end_ = nullptr;
	bool bonesResolved_ = false;

	bool hasTarget_ = false;
	Math::Vector3 target_;
	Math::Vector3 poleVector_;
};