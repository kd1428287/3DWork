#pragma once

#include "../../Core/Handle.h"
#include "TransformComponent.h"

// ============================================================
// AttachToSocketComponent: 自身のGameObjectが持つTransformComponentを、
// 別のGameObject上のソケット(SocketComponent、あるいは単なる
// TransformComponentでもよい)のワールド行列へ毎フレーム同期させる。
//
// 責務の分離:
//   - SocketComponent側は「自分が誰にぶら下がっているか」の
//     階層構造(親のTransformComponentとの行列合成)だけに専念する。
//     "誰かを自分に追従させる"仕事は一切持たない。
//   - こちらAttachToSocketComponent側が「追従する側」の
//     GameObjectにアタッチされ、"ソケットの位置を読み取って
//     自分のTransformComponentへコピーする"仕事だけを持つ。
//
// 例: プレイヤーの手のSocketObject(SocketComponentのみを持つ)に対して、
//     Weapon(WeaponModelComponent + TransformComponent + この
//     AttachToSocketComponent)がsocketHandleとして参照を持ち、
//     毎フレーム自分のTransformComponentへワールド座標をコピーする。
//     こうすることで、WeaponModelComponent等の既存の描画/物理コードは
//     今まで通りGetComponent<TransformComponent>()を呼ぶだけで済み、
//     GameObjectの「型ごとに1インスタンス」というコア設計にも
//     一切手を加えずに済む。
//
// 参照先(ソケット)は"別のGameObject"かつ"フレームをまたいで
// 生存期間の保証がない"相手なので、生ポインタではなくHandle<T>で持つ。
// 一方、自分自身のTransformComponentは同じGameObject上の兄弟であり
// 生存期間が結びついているため、Handleを介さず生ポインタで保持する
// (Handle.hの使い分け方針に準拠)。
// ============================================================
class AttachToSocketComponent : public ComponentBase {
public:
	explicit AttachToSocketComponent(GameObject* owner, Handle<TransformComponent> socketHandle)
		: ComponentBase(owner), socketHandle_(socketHandle) {}

	void Start() override {
		selfTransform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	// Update内でコピーする。ソケット側の親子階層(SocketComponentの
	// RefreshCacheIfNeeded)はGetWorldMatrix()呼び出し時に遅延評価される
	// ため、このコンポーネントの実行順序がソケット側より先でも後でも
	// 最新の行列を取得できる(1フレーム遅れは発生しない)。
	void Update(float deltaTime) override {
		if (selfTransform_ == nullptr) {
			return; // 自分にTransformComponentが無ければ何もしない
		}

		TransformComponent* socket = socketHandle_.Resolve();
		if (socket == nullptr) {
			// ソケット(または、ソケットの持ち主自体)が破棄されている。
			// 追従を諦め、直前のTransformの値をそのまま保持する。
			return;
		}

		Math::Vector3 position;
		Math::Quaternion rotation;
		Math::Vector3 scale;
		if (socket->GetWorldMatrix().Decompose(scale, rotation, position)) {
			selfTransform_->SetPosition(position);
			selfTransform_->SetRotation(rotation);
			// スケールは意図的に同期しない。武器モデル自体のスケールは
			// 武器側のTransformComponentが独自に持つ値を尊重するため
			// (ソケット側のスケールを引き継ぐと、キャラクターの
			//  スケール変更が武器サイズにも伝播してしまう)。
		}
	}

	Handle<TransformComponent>& GetSocketHandle() { return socketHandle_; }
	void SetSocketHandle(Handle<TransformComponent> socketHandle) { socketHandle_ = socketHandle; }

private:
	Handle<TransformComponent> socketHandle_;
	TransformComponent* selfTransform_ = nullptr; // 兄弟コンポーネントなのでHandle不要
};
