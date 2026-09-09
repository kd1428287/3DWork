#pragma once

// ============================================================
// IAnimationPostProcess
//
// FK(SkeletonComponentによるボーンのローカル→ワールド確定)が終わった
// 後に、そのポーズに対して追加の補正を行うコンポーネントが実装するタグ。
// 現時点での実装例はTwoBoneIKComponent(左腕/右腕など、サブクラス化して
// 複数チェーンを同一GameObjectに同時に付ける想定)。将来的にラグドール
// ブレンドやルックアットIK等を追加する場合も、このタグを実装するだけで
// SkeletonComponent側は変更不要になる。
//
// SkeletonComponentは自身のPostUpdate()で
// GetOwner()->GetTagged<IAnimationPostProcess>() を使い、具体的な型を
// 一切知らずに実装コンポーネントを全部集めてSolveIK()を呼ぶ
// (TagInterfaces.hの仕組みに乗るため、GameObject.h側の変更は不要)。
//
// 実装側の約束事: SolveIK()内でCalcNodeMatrices()相当の再計算を
// 呼ばないこと。同一SkeletonComponentに複数のIAnimationPostProcess
// (左腕IK・右腕IK等)が付いている場合、全員の書き込みが終わってから
// SkeletonComponent側が1回だけ再計算するため、個々が呼ぶと重複計算になる。
// ============================================================
class IAnimationPostProcess {
public:
	virtual ~IAnimationPostProcess() = default;

	// FK確定後、SkeletonComponent::PostUpdate()から呼ばれる。
	// ボーンのローカル変換を書き換えるところまでが責務。
	virtual void SolveIK() = 0;
};
