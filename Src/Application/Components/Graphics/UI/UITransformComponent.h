#pragma once

// 2D UI用の座標コンポーネント
// 座標系は描画と同じ中心原点・Y上向き(ピクセル)。Screenは画面アンカー基準、Worldは投影した点が基準
class UITransformComponent : public ComponentBase {
public:
	enum class Space { Screen, World };

	explicit UITransformComponent(GameObject* owner) :ComponentBase(owner) {}

	void Awake() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	// 基準点(pivotの位置)を2D描画用の座標で返す。World時にカメラの後ろ等ならfalse
	bool Resolve(Math::Vector2& outPos) const;

	// 左下の座標と、scale適用後のサイズを返す(pivot考慮)。ゲージや画像用
	bool ResolveRect(Math::Vector2& outMin, Math::Vector2& outSize) const;

	// 基準点まわりのZ回転行列。KdSpriteShader::SetMatrixに渡し、描画後はIdentityに戻すこと
	Math::Matrix GetRotationMatrix(const Math::Vector2& pivotPos) const;

	//===========================================
	// 設定
	//===========================================

	void SetSpace(Space space) { space_ = space; }
	// Screen: アンカーからのピクセル距離 / World: 投影後のピクセルオフセット
	void SetPosition(const Math::Vector2& pos) { position_ = pos; }
	// Screenのみ有効。画面の割合で指定する(0,0)=左下 (1,1)=右上 (0.5,0.5)=中央
	void SetAnchor(const Math::Vector2& anchor) { anchor_ = anchor; }
	// 自分のどこを基準点にするか。(0,0)=左下 (1,1)=右上
	void SetPivot(const Math::Vector2& pivot) { pivot_ = pivot; }
	// 矩形のサイズ(ピクセル)。文字のように大きさを自分で持つ場合は0のままでよい
	void SetSize(const Math::Vector2& size) { size_ = size; }
	void SetScale(const Math::Vector2& scale) { scale_ = scale; }
	// 度数法
	void SetRotation(float degrees) { rotation_ = degrees; }
	// Worldのみ有効。Transformの位置に足す(頭上に出す分など)
	void SetWorldOffset(const Math::Vector3& offset) { worldOffset_ = offset; }

	Space					GetSpace() const { return space_; }
	const Math::Vector2& GetPosition() const { return position_; }
	const Math::Vector2& GetAnchor() const { return anchor_; }
	const Math::Vector2& GetPivot() const { return pivot_; }
	const Math::Vector2& GetSize() const { return size_; }
	const Math::Vector2& GetScale() const { return scale_; }
	float					GetRotation() const { return rotation_; }

	//===========================================
	// 共通処理(他のUIからも使える)
	//===========================================

	// World表示の投影に使うカメラ行列。カメラ更新時に毎フレーム設定する
	static void SetCamera(const Math::Matrix& view, const Math::Matrix& proj);
	// ワールド座標を2D描画用の座標へ変換。カメラの後ろならfalse
	static bool WorldToScreen(const Math::Vector3& world, Math::Vector2& out);
	// 現在のビューポートのサイズ(ピクセル)
	static Math::Vector2 GetViewportSize();
	// マウスカーソルの位置を2D描画用の座標へ変換。ウィンドウ外ならfalse
	static bool GetMousePos(Math::Vector2& out);

private:
	TransformComponent* transform_ = nullptr;

	Space				space_ = Space::Screen;
	Math::Vector2		position_ = { 0.0f, 0.0f };
	Math::Vector2		anchor_ = { 0.5f, 0.5f };
	Math::Vector2		pivot_ = { 0.5f, 0.5f };
	Math::Vector2		size_ = { 0.0f, 0.0f };
	Math::Vector2		scale_ = { 1.0f, 1.0f };
	float				rotation_ = 0.0f;
	Math::Vector3		worldOffset_ = { 0.0f, 0.0f, 0.0f };
};