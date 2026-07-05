#pragma once
#include "IRenderable.h"
// 新しいタグインターフェースを増やしたくなったら、
// #include を足した上で下のマクロにカンマ区切りで追加するだけでよい。
// GameObject.h自体は一切変更不要。
// 例: #include "ICollidable.h" した上で
//     #define KD_TAG_INTERFACES IRenderable, ICollidable

// ============================================================
// GameObjectが自動的にタグ登録の対象とするインターフェース一覧。
// GameObject::AddComponent<T>() は、Tがここに列挙された型のうち
// どれを実装しているかをコンパイル時に判定し、該当するものだけ
// 内部のタグレジストリに登録する。
// ============================================================
#define TAG_INTERFACES IRenderable	//, IDebugPrintable