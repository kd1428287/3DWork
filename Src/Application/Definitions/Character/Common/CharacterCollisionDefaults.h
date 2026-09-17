#pragma once

// ============================================================
// キャラクター(人型オブジェクト)の体格に関わる、複数箇所で
// 一致していなければならない値をここに集約する。
//
// 経緯: 「Transform原点から足裏までの距離」という同じ意味の値が、
// 以前はGroundSensorComponentのデフォルト値・EnemyFactory・
// PlayerFactoryにそれぞれ独立した定数として書かれていた。
// GroundSensorComponentが接地レイを飛ばす基準の高さと、Bodyコライダー
// (物理的な押し返しに使う形状)の下端の高さは本来1つの依存関係で
// あるにもかかわらず別々に管理されていたため、片方だけ変更すると
// 「Bodyコライダーで地面に静止しているのに、接地レイの発射点が
// ズレていて接地を検出できず、重力が際限なく積み上がって最終的に
// 地面を貫通する」という不具合を引き起こした(実際に発生した不具合)。
//
// 使い方:
//   - GroundSensorComponentはfootOffsetの既定値としてここを参照する。
//   - 各Factory(EnemyFactory/PlayerFactory等)は、Bodyコライダーの
//     下端(・対称配置の場合は上端も)の位置をここから計算する。
//   - GroundSensorComponentに明示的に別の値をセットした場合は、
//     必ずBodyコライダー側の寸法もそれに合わせて変更すること
//     (この一元管理はあくまで「デフォルトの食い違い」を防ぐためのもので、
//      個別にカスタマイズすること自体は妨げない)。
//
// 将来、体格の異なるキャラクター種別(小型/大型の敵など)が増える場合は、
// この単一定数ではなく、EnemyDefinition側に身長・footOffset相当の
// フィールドを持たせる形へ拡張すること。その場合もFactory側が
// 「GroundSensorComponentの値」と「Bodyコライダーの寸法」を必ず
// 同じ変数から計算するという、ここで示す一元管理の原則自体は維持すること。
// ============================================================
namespace CharacterCollisionDefaults
{
	// Transform原点(通常はキャラクターの胴体中心あたり)から
	// 足裏までの距離。GroundSensorComponent::footOffsetの既定値、
	// および各キャラクターのBodyコライダー(Capsule/Box)の下端
	// (対称配置の場合は上端も)の計算に使う。
	constexpr float kFootOffset = 0.9f;

	// 身長。足裏から頭頂までの全高(kFootOffsetのように「原点から
	// 片側だけの距離」ではなく、キャラクター全体の高さそのもの)。
	// Bodyコライダー(Capsule)の上端位置(=足裏 + kHeight で頭頂位置が
	// 求まる)や、カメラの注視点オフセット等、体格に依存する複数箇所で
	// 一致していなければならない値としてここに集約する。
	constexpr float kHeight = 1.8f;

	// 体格(横幅)。Bodyコライダーの半径(kBodyWidth * 0.5f)の基準として使う。
	// kFootOffset/kHeightと同じ理由で、Factory側の当たり判定生成と
	// 見た目のスケール感を一致させるためにここへ置く。
	constexpr float kBodyWidth = 0.6f;

	// 目線の高さ(足裏からの距離)。kHeight(頭頂)より低い値を想定。
	// カメラの注視点オフセット(PlayerFactory::CameraTargetComponent等)や、
	// ロックオン時にカメラが狙う高さの基準として使う。現状は
	// PlayerFactory.cpp側にマジックナンバー(1.5f)で直書きされている値を
	// ここへ一元化する想定。
	constexpr float kEyeHeight = 1.6f;
}
