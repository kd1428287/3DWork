#pragma once
#include "../Components/Tags/IRenderable.h"
#include "../Components/Tags/IModelRenderSource.h"
#include "../Components/Tags/IPolygonRenderSource.h"
#include "../Components/Tags/IRenderStateModifier.h"
#include "../Components/Tags/ICollidable.h"
#include "../Components/Tags/IAnimationPostProcess.h"
#include "../Components/Tags/ICameraTarget.h"
#include "../Components/Tags/IMovementSource.h"

// ============================================================
// GameObjectが自動的にタグ登録の対象とするインターフェース一覧。
// GameObject::AddComponent<T>() は、Tがここに列挙された型のうち
// どれを実装しているかをコンパイル時に判定し、該当するものだけ
// 内部のタグレジストリに登録する。
// ============================================================
#define TAG_INTERFACES IRenderable,ICollidable,IPolygonRenderSource,IAnimationPostProcess,IModelRenderSource,IRenderStateModifier,IMovementSource