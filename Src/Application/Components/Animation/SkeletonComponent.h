#pragma once
#include "../../Core/Handle.h"
#include "../Tags/IModelRenderSource.h"
#include "../Tags/IAnimationPostProcess.h"

// ポインタで持つだけなのでヘッダの取り込みは不要。ModelAnimatorComponent.h
// 側がSkeletonComponent.hをincludeしているため、ここでincludeすると
// 循環インクルードになる。
class ModelAnimatorComponent;

// ============================================================
// SkeletonComponent: 骨格アニメーション(FK)を担当する。
// KdModelWorkを保持し、毎フレームボーンのローカル空間行列を計算する。
//
// 注意: KdModelWorkのm_worldTransformは「モデルローカル空間」
// (ルートノード基準)であり、GameObjectのワールド座標ではない。
// ワールド座標が欲しい場合はTryGetBoneWorldMatrix()を使い、
// 所有者のTransformComponentと合成する。
//
// 既にメッシュ描画用に別のKdModelWorkを持つコンポーネントがある場合は
// 二重計算・アニメのズレを避けるため、そちらではなくこの
// SkeletonComponentが持つKdModelWorkを描画側からも参照すること。
//
// --- FK/IKのオーケストレーターとしての役割について ---------------------
// このコンポーネントは自分自身のFK計算だけでなく、同じGameObject上の
// アニメーション関連コンポーネント(ModelAnimatorComponent、および
// IAnimationPostProcessを実装するIK群)を明示的に呼び出す司令塔を兼ねる。
// GameObject::PreUpdate()/PostUpdate()という既存の2フェーズに乗せる
// (グローバルなレジストリやECS的な一括処理は導入しない。単一の
// GameObject内で完結する処理のため、既存のcomponentOrder_駆動で十分)。
//
//   PreUpdate(dt):  ModelAnimatorComponent::AdvanceFK(dt) → Finalize()
//   (通常の)Update(dt): gameplayロジック(この時点でルートモーション量は
//                        今フレーム分が確定済み。1フレーム遅延なし)
//   PostUpdate(dt): 全IAnimationPostProcess::SolveIK() → Finalize()
//                   (SetTarget()がgameplayロジックの結果に依存する
//                    照準・接地IK等を、同フレームの目標で解ける)
//
// IK側は具体的な型(TwoBoneIKComponentのどのサブクラスか)を一切知らず、
// GetTagged<IAnimationPostProcess>()で拾う。左腕/右腕のように同種を
// 複数同時に使う場合は、GameObjectが型ごとに1インスタンスまでという
// 制約(GameObject.h参照)があるため、TwoBoneIKComponentをサブクラス化
// して型を分けること(例: LeftArmIKComponent : public TwoBoneIKComponent)。
//
// Finalize()は上記2箇所それぞれの終端で呼ばれる。NeedCalcNodeMatrices()
// による内部ダーティ判定があるため、書き込みが発生しなかった側の呼び出し
// (IKが1つも付いていないオブジェクトのPostUpdate()側など)は実質的に
// コストゼロになる。
// ============================================================
class SkeletonComponent : public ComponentBase, IModelRenderSource {
public:
	explicit SkeletonComponent(GameObject* owner) : ComponentBase(owner) {}

	void SetModelData(std::string_view fileName) { m_modelWork.SetModelData(KdAssets::Instance().m_modeldatas.GetData(fileName)); }
	void SetModelData(const std::shared_ptr<KdModelData>& data) { m_modelWork.SetModelData(data); }

	// 【重要】ModelAnimatorComponentは上で前方宣言しているだけなので、
	// この2つの本体はここに書けない。Start()内のGetComponent<ModelAnimatorComponent>()
	// (内部でstatic_cast<ModelAnimatorComponent*>する)、およびPreUpdate()内の
	// animator_->AdvanceFK()呼び出しは、どちらもModelAnimatorComponentの
	// 完全な型定義を要求する。循環インクルード(ModelAnimatorComponent.hが
	// このファイルをincludeしている)を避けつつ両方を成立させるため、
	// 定義本体はModelAnimatorComponent.hの末尾、両クラスの定義が出揃った
	// 後に置いている。
	void Start() override;
	void PreUpdate(float deltaTime) override;

	// IKステージ: gameplayロジックのUpdate()より後に呼ばれる想定
	// (GameObject::PostUpdate()経由)。IAnimationPostProcessを実装する
	// 全コンポーネント(IK等)にローカル回転を書き込ませてから、
	// 1回だけFinalize()で確定させる(個々のSolveIK()側はCalcNodeMatrices()
	// 相当を呼ばない約束になっている。詳細はIAnimationPostProcess.h参照)。
	void PostUpdate(float deltaTime) override {
		for (IAnimationPostProcess* postProcess : GetOwner()->GetTagged<IAnimationPostProcess>()) {
			postProcess->SolveIK();
		}
		Finalize();
	}

	// CalcNodeMatrices()の呼び出しと、BoneSocketComponent側キャッシュ
	// 無効化用のバージョン更新をここに集約する。PreUpdate()/PostUpdate()
	// それぞれの終端から呼ばれるため、1フレームに最大2回呼ばれる。
	// 実際に書き込みが起きた(NeedCalcNodeMatrices()がtrueの)時だけ
	// バージョンを進めることで、無駄な再計算誘発を避ける。
	void Finalize() {
		if (m_modelWork.NeedCalcNodeMatrices())
		{
			m_modelWork.CalcNodeMatrices();
			++m_boneVersion;
		}
	}

	// ボーンのモデルローカル空間行列
	bool TryGetBoneLocalMatrix(std::string_view boneName, Math::Matrix& outMatrix) const
	{
		const KdModelWork::Node* node = m_modelWork.FindNode(boneName);
		if (node == nullptr) { return false; }
		outMatrix = node->m_worldTransform;
		return true;
	}

	// ボーンのワールド空間行列(所有者のTransformComponentと合成)
	bool TryGetBoneWorldMatrix(std::string_view boneName, Math::Matrix& outMatrix) const
	{
		Math::Matrix localMat;
		if (!TryGetBoneLocalMatrix(boneName, localMat)) { return false; }

		// スケールの二重伝播を避けるためUnscaled行列を使う
		Math::Matrix ownerMat = selfTransform_ ? selfTransform_->GetUnscaledMatrix() : Math::Matrix::Identity;
		ownerMat = selfTransform_->GetWorldMatrix();

		outMatrix = localMat * ownerMat;
		return true;
	}

	uint32_t GetBoneVersion() const { return m_boneVersion; }

	KdModelWork& WorkModel() { return m_modelWork; }
	const KdModelWork& GetModel() const { return m_modelWork; }

	KdModelWork* GetModel() override { return &m_modelWork; }

private:
	KdModelWork m_modelWork;
	TransformComponent* selfTransform_ = nullptr; // 兄弟コンポーネント
	ModelAnimatorComponent* animator_ = nullptr;   // 兄弟コンポーネント(付いていなければnullptr)
	uint32_t m_boneVersion = 0;
};