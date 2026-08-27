//#include "EffectFactory.h"
//#include "EffectDefinition.h"
//
//#include "../../Components/Transform/TransformComponent.h"
//
//// ------------------------------------------------------------
//// TODO: 以下2つは未実装のエフェクト用コンポーネント(仮称・仮API)。
//// 実際の実装が決まり次第、コンストラクタ引数・Setter名・includeパスを
//// 差し替えること。他のコードはこの2つのAPIにしか依存していないため、
//// 差し替え箇所はここと下のBuildEffect()内だけで済むはず。
////
////   VisualEffectComponent   : パーティクル/VFXの見た目再生を担当。
////                             SkeletonComponent::SetModelData()の
////                             エフェクト版イメージ。
////   EffectLifetimeComponent : 一定時間経過後にGameObjectを自動破棄する。
////                             コンストラクタで秒数を受け取る想定。
//// ------------------------------------------------------------
//#include "../../Components/Effects/VisualEffectComponent.h"
//#include "../../Components/Effects/EffectLifetimeComponent.h"
//
//EffectFactory::EffectFactory(const std::unordered_map<std::string, EffectDefinition>& database) {
//	for (const auto& [id, def] : database) {
//		const EffectDefinition* defPtr = &def;
//
//		registry_.Register(id, [defPtr](ObjectManager& objectManager, Math::Vector3 position, Math::Quaternion rotation) {
//			return BuildEffect(objectManager, *defPtr, position, rotation);
//			});
//	}
//}
//
//GameObject* EffectFactory::BuildEffect(ObjectManager& objectManager, const EffectDefinition& def,
//	const Math::Vector3& position, const Math::Quaternion& rotation) {
//	GameObject* effect = objectManager.Instantiate(def.name);
//	if (!effect) return nullptr;
//
//	// PlayerFactory/EnemyFactoryと同じ流儀:
//	// AddComponent後にSetPosition/SetRotation/SetScaleで設定する。
//	auto* transform = effect->AddComponent<TransformComponent>();
//	transform->SetPosition(position);
//	transform->SetRotation(rotation);
//	transform->SetScale({ def.scale, def.scale, def.scale });
//
//	// --- 仮実装 -----------------------------------------------------
//	// VisualEffectComponentのコンストラクタ/SetVfxData()は未確定のため、
//	// SkeletonComponent::SetModelData()に倣った形にしてある。
//	auto* visual = effect->AddComponent<VisualEffectComponent>();
//	visual->SetVfxData(def.vfxPath);
//
//	// lifetime<=0のものはループ再生 or 手動破棄前提のため付与しない。
//	if (def.lifetime > 0.0f) {
//		effect->AddComponent<EffectLifetimeComponent>(def.lifetime);
//	}
//
//	// attachToOwnerを使った追従(AttachToSocketComponent等との連携)は
//	// 発生源GameObjectを引数として受け取る必要があるため、現時点では
//	// 未実装。必要になったらCreateEffect()にowner引数を追加し、
//	// EnemyFactory::CreateWeaponSocket()と同様の形で組み込む想定。
//
//	return effect;
//}
