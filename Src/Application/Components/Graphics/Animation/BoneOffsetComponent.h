#pragma once
#include "SkeletonComponent.h"
#include "../../Tags/IAnimationPostProcess.h"

// 名前付きボーン鎖(RightArm等)とバネの設定(Prefab/JSONから渡す値)
struct BoneOffsetChainConfig
{
	std::string					name;				// 鎖の名前(攻撃データから参照される)
	std::vector<std::string>	bones;				// 根元→先端の順
	std::vector<float>			weights;			// ボーンごとの回転の掛かり具合(不足分は1)
	float						impulse = 0.0f;		// 初速(rad/s)
	float						stiffness = 0.0f;	// バネ係数
	float						damping = 0.0f;		// 減衰係数
	float						maxAngle = 0.0f;	// 最大回転量(rad)
};

struct BoneOffsetConfig
{
	std::vector<BoneOffsetChainConfig> chains;
};

// アニメーション後のボーンへ減衰バネの回転オフセットを重ねる(パリィ時の腕の跳ね返し等)
class BoneOffsetComponent : public ComponentBase, public IAnimationPostProcess
{
public:
	using Config = BoneOffsetConfig;

	explicit BoneOffsetComponent(GameObject* owner) : ComponentBase(owner) {}

	void SetConfig(const Config& config)
	{
		chains_.clear();
		for (const auto& c : config.chains) {
			Chain chain;
			chain.cfg = c;
			chains_.push_back(std::move(chain));
		}
		resolved_ = false;
	}

	void Awake() override
	{
		skeleton_ = GetOwner()->GetComponent<SkeletonComponent>();
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	// 名前chainNameの鎖を跳ね返す。recoilDirWorldは押し戻したい向き(ワールド)、strengthはimpulseへの倍率
	void Trigger(std::string_view chainName, const Math::Vector3& recoilDirWorld, float strength = 1.0f)
	{
		if (!skeleton_ || !transform_) { return; }
		ResolveBones();

		const auto& nodes = skeleton_->WorkModel().WorkNodes();
		const Math::Vector3 recoil = Math::Vector3::TransformNormal(recoilDirWorld, transform_->GetWorldMatrix().Invert());

		for (auto& c : chains_) {
			if (c.cfg.name != chainName || c.bones.size() < 2) { continue; }

			const Math::Vector3 armDir =
				nodes[c.bones.back().index].m_worldTransform.Translation() -
				nodes[c.bones.front().index].m_worldTransform.Translation();

			// DirectXは回転の向きが逆なので、押し戻し方向×腕方向を軸にする
			Math::Vector3 axis = recoil.Cross(armDir);
			if (axis.LengthSquared() < FLT_EPSILON) { continue; }
			axis.Normalize();

			c.omega += axis * (c.cfg.impulse * strength);
		}
	}

	// バネの積分だけ行う(ボーンへの反映はSolveIK)
	void PreUpdate(float deltaTime) override
	{
		for (auto& c : chains_) {
			c.omega += (c.angle * -c.cfg.stiffness - c.omega * c.cfg.damping) * deltaTime;
			c.angle += c.omega * deltaTime;

			const float len = c.angle.Length();
			if (len > c.cfg.maxAngle) { c.angle *= c.cfg.maxAngle / len; }

			// 静止したら完全に止める
			if (c.angle.LengthSquared() + c.omega.LengthSquared() < FLT_EPSILON) {
				c.angle = Math::Vector3::Zero;
				c.omega = Math::Vector3::Zero;
			}
		}
	}

	// アニメ更新後のローカル行列へ回転オフセットを上乗せする
	void SolveIK() override
	{
		if (!skeleton_) { return; }
		ResolveBones();

		auto& nodes = skeleton_->WorkModel().WorkNodes();
		for (auto& c : chains_) {
			const float angle = c.angle.Length();
			if (angle <= 0.0f) { continue; }
			const Math::Vector3 axis = c.angle / angle;

			for (const BoneRef& bone : c.bones) {
				auto& node = nodes[bone.index];

				// 親のモデル空間回転は現在のローカル/ワールドから逆算する
				Math::Matrix parentRot = node.m_localTransform.Invert() * node.m_worldTransform;
				parentRot.Translation(Math::Vector3::Zero);

				// モデル空間の回転を親空間へ写して、ローカルの回転部分だけに後掛けする
				const Math::Matrix offset = parentRot * Math::Matrix::CreateFromAxisAngle(axis, angle * bone.weight) * parentRot.Invert();

				const Math::Vector3 pos = node.m_localTransform.Translation();
				node.m_localTransform.Translation(Math::Vector3::Zero);
				node.m_localTransform = node.m_localTransform * offset;
				node.m_localTransform.Translation(pos);
			}
		}
	}

private:
	struct BoneRef
	{
		int		index = 0;
		float	weight = 1.0f;
	};

	struct Chain
	{
		BoneOffsetChainConfig	cfg;
		std::vector<BoneRef>	bones;	// 解決済みボーン(見つからない名前は除外)
		Math::Vector3			angle;	// 回転ベクトル(モデル空間)
		Math::Vector3			omega;	// 角速度
	};

	// ボーン名→ノード番号。モデル未設定の間は解決せず次回に回す
	void ResolveBones()
	{
		if (resolved_) { return; }

		auto& model = skeleton_->WorkModel();
		const auto& nodes = model.WorkNodes();
		if (nodes.empty()) { return; }

		for (auto& c : chains_) {
			c.bones.clear();
			for (size_t i = 0; i < c.cfg.bones.size(); ++i) {
				const auto* node = model.FindNode(c.cfg.bones[i]);
				if (node == nullptr) { continue; }
				BoneRef ref;
				ref.index = static_cast<int>(node - nodes.data());
				ref.weight = (i < c.cfg.weights.size()) ? c.cfg.weights[i] : 1.0f;
				c.bones.push_back(ref);
			}
		}
		resolved_ = true;
	}

	SkeletonComponent*	skeleton_ = nullptr;
	TransformComponent*	transform_ = nullptr;
	std::vector<Chain>	chains_;
	bool				resolved_ = false;
};
