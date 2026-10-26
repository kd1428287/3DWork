#pragma once
#include "SkeletonComponent.h"
#include "../../Tags/IAnimationPostProcess.h"
#include "EventBus.h"
#include "CombatEvents.h"

// 名前付きボーン鎖(RightArm等)の設定。バネと姿勢固定の値もここで渡す(Prefab/JSON)。
struct BoneOffsetChainConfig
{
	std::string					name;					// 鎖の名前(攻撃データから参照される)
	std::vector<std::string>	bones;					// 根元→先端の順(親子で連続させる)
	std::vector<float>			weights;				// 関節ごとの回転量の倍率(不足分は1)
	float						impulse = 0.0f;			// 初速(rad/s)
	float						stiffness = 0.0f;		// バネ係数
	float						damping = 0.0f;			// 減衰係数
	float						maxAngle = 0.0f;		// 最大回転量(rad)
	float						blendInSeconds = 0.0f;	// パリィ時の姿勢で止まるまでの時間
	float						holdSeconds = 0.0f;		// 止めた姿勢を保つ時間
	float						releaseSeconds = 0.0f;	// アニメーションへ戻るまでの時間
};

struct BoneOffsetConfig
{
	std::vector<BoneOffsetChainConfig> chains;
};

// パリィ時、鎖のアニメーションをその姿勢で止め、バネの回転を重ねて弾く。時間が経つとアニメーションへ戻る。
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

		// 自分の攻撃がパリィされたときだけ届く(解除はScopedSubscriberの破棄で自動)
		parrySub_ = GetOwner()->GetLocalEventBus().SubscribeScoped<Events::ParrySucceeded>(
			[this](const Events::ParrySucceeded& e) { OnParrySucceeded(e); });
	}

	// 鎖chainNameを弾く。recoilDirWorldは押し戻したい向き(ワールド)、strengthはimpulseへの倍率。
	void Trigger(std::string_view chainName, const Math::Vector3& recoilDirWorld, float strength = 1.0f)
	{
		if (!skeleton_ || !transform_) { return; }
		ResolveBones();

		const auto& nodes = skeleton_->WorkModel().GetNodes();
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

			// 固定中なら固定姿勢のまま保持時間だけ延ばし、そうでなければ次のSolveIKで姿勢を取る
			if (c.frozen) { c.elapsed = std::min(c.elapsed, c.cfg.blendInSeconds); }
			else { c.elapsed = 0.0f; c.frozen = true; c.needCapture = true; }
		}
	}

	// タイマーとバネの積分だけ行う(ボーンへの反映はSolveIK)
	void PreUpdate(float deltaTime) override
	{
		for (auto& c : chains_) {
			if (c.frozen) {
				c.elapsed += deltaTime;
				if (c.elapsed >= TotalSeconds(c.cfg)) { c.frozen = false; }
			}

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

	// アニメ更新後のポーズに、固定姿勢へのブレンドと回転オフセットを反映する
	void SolveIK() override
	{
		if (!skeleton_) { return; }

		bool anyActive = false;
		for (const auto& c : chains_) { anyActive |= IsActive(c); }
		if (!anyActive) { return; }	// 何も無い時はWorkNodes()を呼ばず、再計算を起こさない

		ResolveBones();
		auto& nodes = skeleton_->WorkModel().WorkNodes();

		for (auto& c : chains_) {
			if (!IsActive(c) || c.bones.empty()) { continue; }

			if (c.needCapture) { CaptureFrozenPose(c, nodes); }

			const float angle = c.angle.Length();
			const Math::Vector3 axis = (angle > 0.0f) ? c.angle / angle : Math::Vector3::Zero;
			const float blend = c.frozen ? Envelope(c) : 0.0f;

			for (size_t i = 0; i < c.bones.size(); ++i) {
				ApplyToBone(c, i, nodes, axis, angle, blend);
			}
		}
	}

private:
	struct BoneRef
	{
		int		index = 0;
		int		parent = -1;	// 親ノード番号(無ければ-1)
		float	weight = 1.0f;
	};

	struct Chain
	{
		BoneOffsetChainConfig			cfg;
		std::vector<BoneRef>			bones;		// 解決済みボーン(見つからない名前は除外)
		std::vector<Math::Quaternion>	frozenRot;	// 止める姿勢(ローカル回転)
		Math::Vector3					angle;		// 回転ベクトル(モデル空間)
		Math::Vector3					omega;		// 角速度
		float							elapsed = 0.0f;
		bool							frozen = false;
		bool							needCapture = false;
	};

	static bool IsActive(const Chain& c) { return c.frozen || c.angle.LengthSquared() > 0.0f; }

	static float TotalSeconds(const BoneOffsetChainConfig& g) { return g.blendInSeconds + g.holdSeconds + g.releaseSeconds; }

	static float Smooth(float x) { return x * x * (3.0f - 2.0f * x); }

	// 固定姿勢への重み: 0→1(blendIn)→1(hold)→0(release)
	static float Envelope(const Chain& c)
	{
		const BoneOffsetChainConfig& g = c.cfg;
		float t = c.elapsed;

		if (t < g.blendInSeconds) { return Smooth(t / g.blendInSeconds); }
		t -= g.blendInSeconds;

		if (t < g.holdSeconds) { return 1.0f; }
		t -= g.holdSeconds;

		if (t < g.releaseSeconds) { return 1.0f - Smooth(t / g.releaseSeconds); }
		return 0.0f;
	}

	void OnParrySucceeded(const Events::ParrySucceeded& e)
	{
		if (e.reaction.recoilChain.empty()) { return; }

		Trigger(e.reaction.recoilChain, -e.incomingDir, e.reaction.recoilStrength);
	}

	// この時点のローカル回転はアニメ更新直後(オフセット前)なので、そのまま固定姿勢にする
	void CaptureFrozenPose(Chain& c, const std::vector<KdModelWork::Node>& nodes)
	{
		c.frozenRot.assign(c.bones.size(), Math::Quaternion::Identity);

		for (size_t i = 0; i < c.bones.size(); ++i) {
			Math::Matrix m = nodes[c.bones[i].index].m_localTransform;
			Math::Vector3 scale, trans;
			Math::Quaternion rot;
			if (m.Decompose(scale, rot, trans)) { c.frozenRot[i] = rot; }
		}
		c.needCapture = false;
	}

	// 回転を[アニメ→固定姿勢]へブレンドし、親空間へ写したオフセットを後掛けする。ワールドも更新して子へ渡す
	void ApplyToBone(Chain& c, size_t i, std::vector<KdModelWork::Node>& nodes,
		const Math::Vector3& axis, float angle, float blend)
	{
		const BoneRef& bone = c.bones[i];
		auto& node = nodes[bone.index];

		Math::Matrix local = node.m_localTransform;
		Math::Vector3 scale, trans;
		Math::Quaternion rot;
		if (!local.Decompose(scale, rot, trans)) { return; }

		if (blend > 0.0f && i < c.frozenRot.size()) {
			Math::Quaternion frozen = c.frozenRot[i];
			if (rot.Dot(frozen) < 0.0f) { frozen = Math::Quaternion(-frozen.x, -frozen.y, -frozen.z, -frozen.w); }
			rot = Math::Quaternion::Slerp(rot, frozen, blend);
		}

		const Math::Matrix parentWorld = (bone.parent >= 0) ? nodes[bone.parent].m_worldTransform : Math::Matrix::Identity;

		if (angle > 0.0f) {
			Math::Matrix pm = parentWorld;
			Math::Vector3 ps, pt;
			Math::Quaternion parentRot;
			if (pm.Decompose(ps, parentRot, pt)) {
				Math::Quaternion parentInv;
				parentRot.Inverse(parentInv);
				const Math::Quaternion offset = Math::Quaternion::CreateFromAxisAngle(axis, angle * bone.weight);
				rot = rot * (parentRot * offset * parentInv);
			}
		}
		rot.Normalize();

		node.m_localTransform = Math::Matrix::CreateScale(scale) * Math::Matrix::CreateFromQuaternion(rot) * Math::Matrix::CreateTranslation(trans);
		node.m_worldTransform = node.m_localTransform * parentWorld;
	}

	// ボーン名→ノード番号。モデル未設定の間は解決せず次回に回す
	void ResolveBones()
	{
		if (resolved_) { return; }

		auto& model = skeleton_->WorkModel();
		const auto& nodes = model.GetNodes();
		if (nodes.empty()) { return; }

		const auto& dataNodes = model.GetDataNodes();

		for (auto& c : chains_) {
			c.bones.clear();
			for (size_t i = 0; i < c.cfg.bones.size(); ++i) {
				const auto* node = model.FindNode(c.cfg.bones[i]);
				if (node == nullptr) { continue; }

				BoneRef ref;
				ref.index = static_cast<int>(node - nodes.data());
				ref.parent = dataNodes[ref.index].m_parent;
				ref.weight = (i < c.cfg.weights.size()) ? c.cfg.weights[i] : 1.0f;
				c.bones.push_back(ref);
			}
		}
		resolved_ = true;
	}

	SkeletonComponent* skeleton_ = nullptr;
	TransformComponent* transform_ = nullptr;
	std::vector<Chain>	chains_;
	bool				resolved_ = false;
	ScopedSubscriber	parrySub_;
};