#pragma once

class EventBus;
class CameraComponent;
class ColliderRegistry;

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

	// スケールされていない、フレームの生の経過時間。
	// 個々のGameObjectのtimeScale_の影響を受けないため、
	// 「効果自体の残り時間」のカウントに使う。
	// (対象がスローされていても、効果の持続時間は実時間で進むようにするため)
	float unscaledDeltaTime = 0.0f;
};