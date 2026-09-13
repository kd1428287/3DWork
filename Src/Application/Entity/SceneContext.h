#pragma once
#include "Handle.h"

class EventBus;
class CameraComponent;
class ColliderRegistry;
class ObjectManager;
class GameObject;
class LockOnTargetComponent;

// ============================================================
// GameObjectが生成時に1つだけ紐付けられる、Scene全体で共有される
// 非所有参照の束。
//
// 「カメラ」のように"Sceneに1つだけの既知の対象"への参照が増える
// たびに、GameObject自体にポインタを1つずつ生やしていくと、
// 以前ComponentBaseが描画フックで肥大化したのと同じ問題が起きる。
// そのため、この手の「シーン全体に共有したい非所有参照」は
// すべてここにまとめ、GameObjectはSceneContext*を1つだけ持つ。
//
// 新しい共有参照(例: AudioListener等)が増えたら、ここにメンバを
// 1つ足すだけでよく、GameObject/World側は変更不要。
// ============================================================
struct SceneContext {
	EventBus* eventBus = nullptr;
	CameraComponent* activeCamera = nullptr;
	ColliderRegistry* colliderRegistry = nullptr;

	// 自分自身(あるいは他のGameObject)をObjectManager::Destroy()経由で
	// 破棄予約したいコンポーネントのために追加(死亡演出後の消滅処理等)。
	// ObjectManagerのコンストラクタでcontext_.objectManager = this;と
	// 自己登録される。
	ObjectManager* objectManager = nullptr;

	// シーンに1つだけの「現在ロックオン中の対象」への弱参照。activeCameraと
	// 同じ考え方(このファイル冒頭のコメント参照)で、Playerに限らず誰でも
	// ここを見ればロック状態を知れるようにする。
	//
	// 書き込むのはPlayerLockOnComponent(Playerの兄弟コンポーネント)だけ
	// (TryLockOn()/ClearLockOn()、および対象消失時の自動解除)。他の
	// システム(カメラ側のCameraOrbitComponent等)はPlayerLockOnComponentと
	// いう型を一切知らずに、ここを読むだけでロック対象を参照できる。
	//Handle<LockOnTargetComponent> lockedTarget;
	Handle<GameObject> lockedTarget;

	// スケールされていない、フレームの生の経過時間。
	// 個々のGameObjectのtimeScale_の影響を受けないため、
	// 「効果自体の残り時間」のカウントに使う。
	// (対象がスローされていても、効果の持続時間は実時間で進むようにするため)
	float unscaledDeltaTime = 0.0f;
};