#pragma once
// ボーンのローカル空間で、実際に「前後移動」「左右移動」に対応しているのが
// x/y/zのどれかを指定するための軸指定。モデルデータの座標変換過程
// (Y-up/Z-up変換等)によって、エンジン側が期待する軸(前後=Z、上下=Y)と
// ボーンデータ側の軸が食い違っていることがあるため、決め打ちにせず
// 外から指定できるようにしている。
enum class RootMotionAxis { X, Y, Z };

// ============================================================
// ルートモーション(アニメーションクリップに焼き込まれた移動)抽出用の
// 補助クラス。ModelAnimatorComponentから「AdvanceTime()の前後でどれだけ
// 位置が動いたか」を計算する部分だけを切り出したもの。
//
// ComponentBaseは継承しない、ただのデータ+ロジッククラス
// (ModelAnimatorComponentが1個だけメンバとして持つ)。
//
// AdvanceTime()を挟む形でしか値を計算できない(前後のボーン位置を比較する
// 必要があるため)性質上、完全に別コンポーネントには切り出せない。その
// ため、呼び出し側(ModelAnimatorComponent::Update())からPrepareFrame()/
// FinalizeFrame()という形で挟んでもらう作りにしている。
// ============================================================
class RootMotionExtractor
{
public:
	// 抽出元にするボーン名(通常はHip/Root)を指定する。空文字で無効化。
	void SetBoneName(std::string_view name) {
		boneName_ = name;
		node_ = nullptr;
		boneResolved_ = false;
		needsResync_ = true;
	}

	bool IsEnabled() const { return !boneName_.empty(); }

	// 抽出したデルタに掛ける倍率(単位変換用)。実測して合わせ込むこと。
	void SetUnitScale(float scale) { unitScale_ = scale; }

	// ボーンのローカル空間で「前後移動」に対応する軸を指定する
	// (既定: Z、符号+1)。このモデルの実データがどの軸に載っているかは
	// 実機で確認して合わせ込む必要がある。
	void SetForwardAxis(RootMotionAxis axis, float sign = 1.0f) {
		forwardAxis_ = axis;
		forwardSign_ = sign;
	}

	// ボーンのローカル空間で「左右移動」に対応する軸を指定する
	// (既定: X、符号+1)。
	void SetRightAxis(RootMotionAxis axis, float sign = 1.0f) {
		rightAxis_ = axis;
		rightSign_ = sign;
	}

	// アニメーションが切り替わった(Play()で別クリップになった)ことを通知する。
	// 次のPrepareFrame()で基準位置を取り直すだけにし、旧クリップの終端→
	// 新クリップの先頭への差分を移動量として誤って計上しないようにする。
	void NotifyAnimationChanged() { needsResync_ = true; }

	// ModelAnimatorComponent::Update()の冒頭、AdvanceTime()より前に呼ぶ。
	// ボーンの解決だけを行う(基準位置の取り直しはここでは行わない。
	// この時点ではまだAdvanceTime()が走っておらず、ボーンが直前の
	// アニメーションの最後のポーズのままのため、ここで基準を取ると
	// 「旧アニメーションの最後のポーズ」と「新アニメーションの現在の
	// ポーズ」の差分がまるごと1フレームの移動量として計上されてしまう。
	// 実際のリセットはFinalizeFrame側、AdvanceTime()の後で行う)。
	void PrepareFrame(KdModelWork& model) {
		if (boneName_.empty()) return;

		if (!boneResolved_) {
			node_ = model.FindWorkNode(boneName_);
			boneResolved_ = true;
		}
	}

	// AdvanceTime()の直後に呼ぶ。timeBeforeAdvance/timeAfterAdvanceは
	// KdAnimator::GetTime()の前後の値で、ループ巻き戻り検知に使う。
	//
	// forwardAxis_/rightAxis_で指定された2軸だけを「移動」として扱い、
	// キャラクターのTransformへ渡すdelta_(エンジン規約: x=左右, y=常に0,
	// z=前後)に詰め直す。残るもう1軸(通常は上下)は一切触らない
	// (重力・接地判定は別コンポーネントが担うため。詳細はSetForwardAxis/
	// SetRightAxisのコメント参照)。
	//
	// 重要: 移動量を取り出した後、ボーンのローカル変換のforward/right軸
	// 成分をlockedLocalPos_(このアニメーション開始時点の値)へ固定し直す。
	// これをしないと、スキンメッシュはボーン自身の移動でそのまま動き、
	// さらにその移動量が呼び出し側でキャラクターのTransformにも加算される
	// ため、同じ移動が二重にかかって見た目がどんどん基準位置からずれて
	// いく(実際に発生した不具合)。触っていない軸は固定せず、アニメーション
	// 本来の動きをそのまま残す。
	void FinalizeFrame(float timeBeforeAdvance, float timeAfterAdvance) {
		if (node_ == nullptr) return;

		const Math::Vector3 rawLocalPos = node_->m_localTransform.Translation();

		if (needsResync_) {
			// アニメーションが切り替わった直後の最初のフレーム。
			// AdvanceTime()によって「旧アニメーションの最後のポーズ」から
			// 「新アニメーションの現在のポーズ」へ不連続にジャンプした
			// 直後なので、この1フレームだけデルタを捨てる。あわせて、
			// 以降このアニメーションの間forward/right軸を固定しておく
			// 基準位置(lockedLocalPos_)もここで新しく記録し直す。
			delta_ = Math::Vector3::Zero;
			lockedLocalPos_ = rawLocalPos;
			needsResync_ = false;
		}
		else if (timeAfterAdvance < timeBeforeAdvance) {
			// ループして先頭に巻き戻った瞬間。素の差分を取ると「終端→先頭」
			// への巻き戻り量がそのまま1フレームの移動量として計上され、
			// キャラクターが一瞬逆方向へワープして見えてしまうため、
			// このフレームだけデルタを捨てる。
			delta_ = Math::Vector3::Zero;
		}
		else {
			const float forwardDelta =
				(GetAxis(rawLocalPos, forwardAxis_) - GetAxis(lastLocalPos_, forwardAxis_)) * forwardSign_;
			const float rightDelta =
				(GetAxis(rawLocalPos, rightAxis_) - GetAxis(lastLocalPos_, rightAxis_)) * rightSign_;

			// エンジン規約(TransformComponent::GetForward()等)に合わせて、
			// x=右方向、z=前方向としてdelta_を組み立てる。yは常に0
			// (上下は重力・接地判定に完全に任せる)。
			delta_ = Math::Vector3(rightDelta, 0.0f, forwardDelta) * unitScale_;
		}
		lastLocalPos_ = rawLocalPos;

		// ボーンの見た目上のforward/right方向の移動をキャンセルする
		// (二重適用防止)。触っていない軸(通常は上下)はrawLocalPosの
		// ままにして、アニメーション本来の動きを残す。
		Math::Vector3 lockedRaw = rawLocalPos;
		SetAxis(lockedRaw, forwardAxis_, GetAxis(lockedLocalPos_, forwardAxis_));
		SetAxis(lockedRaw, rightAxis_, GetAxis(lockedLocalPos_, rightAxis_));
		node_->m_localTransform.Translation(lockedRaw);
	}
	// このフレームで蓄積された移動量(ボーンのローカル空間、まだワールド
	// 回転を反映していない値)を取得し、内部を0にリセットする。
	// 呼ぶと消費されるので、1フレームにつき1回だけ呼ぶこと(二重適用防止)。
	Math::Vector3 ConsumeDelta() {
		Math::Vector3 d = delta_;
		delta_ = Math::Vector3::Zero;
		return d;
	}

private:
	// v(ボーンのローカル並進ベクトル)から、指定した軸の値を取り出す。
	static float GetAxis(const Math::Vector3& v, RootMotionAxis axis) {
		switch (axis) {
		case RootMotionAxis::X: return v.x;
		case RootMotionAxis::Y: return v.y;
		case RootMotionAxis::Z: return v.z;
		}
		return 0.0f;
	}

	// vの指定した軸だけをvalueへ書き換える。
	static void SetAxis(Math::Vector3& v, RootMotionAxis axis, float value) {
		switch (axis) {
		case RootMotionAxis::X: v.x = value; break;
		case RootMotionAxis::Y: v.y = value; break;
		case RootMotionAxis::Z: v.z = value; break;
		}
	}

	std::string			boneName_;             // 空文字なら無効
	KdModelWork::Node* node_ = nullptr;       // 対象ボーンのワークノード(未解決/未検出ならnullptr)
	bool				boneResolved_ = false; // 一度探索を試みたか(見つからなくても毎フレーム再探索しない)
	bool				needsResync_ = false;  // 次のPrepareFrame()で基準位置を取り直す必要があるか
	Math::Vector3		lastLocalPos_{};       // 前フレームのボーンローカル位置
	Math::Vector3		lockedLocalPos_{};     // このアニメーションの間、forward/right軸を固定しておく位置(二重適用防止用)
	Math::Vector3		delta_{};              // このフレームで蓄積された移動量(未消費分、エンジン規約: x=右, y=0, z=前)
	float				unitScale_ = 1.0f;     // 抽出したデルタに掛ける倍率(単位変換用)
	RootMotionAxis		forwardAxis_ = RootMotionAxis::Z; // ボーンローカル空間で「前後」に対応する軸
	float				forwardSign_ = 1.0f;
	RootMotionAxis		rightAxis_ = RootMotionAxis::X;   // ボーンローカル空間で「左右」に対応する軸
	float				rightSign_ = 1.0f;
};