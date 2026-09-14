#pragma once

// ※ ImGui は既存のPCH等で読み込まれている前提です。
//    node-editor(imgui-node-editor)は本ファイルでのみ使うため明示的にインクルードします。
#include "../ThirdParty/imgui-node-editor/imgui_node_editor.h"

namespace ed = ax::NodeEditor;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 敵の行動を制御するBehavior Treeを、ノードグラフで組み立てるためのエディタ。
//	KdDebugGUI::GuiProcess() の中(ImGui::NewFrame() 後 ～ ImGui::Render() 前)から
//	Update() を呼び出して使用する。MapEditor/EffectEditorと同じ、機能ごとに独立した
//	シングルトンとして構成している。
//
//	現段階では「ノードを置く・線でつなぐ・消す」というグラフ編集の土台のみ実装。
//	ここで組んだグラフをJSON等へシリアライズし、ゲーム側の実行時BTNode<T>へ変換する
//	処理(ノードファクトリ)は未実装
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class BTEditor
{
public:

	// 毎フレームの更新・描画
	void Update();

	// ImGui本体(ImGui::DestroyContext等)が破棄される前に、必ず明示的に呼び出すこと。
	//	KdDebugGUI::GuiRelease() の中で、ImGui_ImplDX11_Shutdown() より前に呼ぶ想定。
	//	(静的破棄順序に任せると、ImGuiコンテキスト破棄後にDestroyEditor()が
	//	 走ってしまう可能性があるため、デストラクタでは片付けない)
	void Shutdown();

private:

	// ///// ///// ///// ///// ///// ///// ///// /////
	// BTノードの種類
	//	Sequence/Selectorは子を複数持てる(=出力ピンを持つ)複合ノード、
	//	Condition/Actionは子を持たない(=出力ピンを持たない)リーフノード
	// ///// ///// ///// ///// ///// ///// ///// /////
	enum class BTNodeType
	{
		Sequence,
		Selector,
		Condition,
		Action,
	};

	// エディタ上の1ノード分の情報。
	//	あくまで編集用データであり、実行時のBTNode<T>そのものではない
	//	(MapObject/MapEntityの関係と同じで、後でシリアライズ用データと
	//	 ランタイム表現を分離する設計にする想定)
	struct EditorNode
	{
		ed::NodeId	id;
		BTNodeType	type = BTNodeType::Sequence;
		std::string	name;

		ed::PinId	inputPin;			// 親から自分へ(全ノード共通で持つ)
		ed::PinId	outputPin;			// 自分から子へ(Sequence/Selectorのみ有効)
		bool		hasOutput = false;
	};

	// エディタ上の1本のリンク(親の出力ピン→子の入力ピン)
	struct EditorLink
	{
		ed::LinkId	id;
		ed::PinId	outputPin;	// 親側
		ed::PinId	inputPin;	// 子側
	};

	void DrawNode(EditorNode& node);
	void DrawCreateNodeMenu(const ImVec2& spawnScreenPos);

	void HandleCreateLink();
	void HandleDelete();

	EditorNode& AddNode(BTNodeType type, const std::string& name, bool hasOutput);
	EditorNode* FindNodeById(ed::NodeId id);
	EditorNode* FindNodeByPin(ed::PinId pin);

	const char* GetNodeTypeName(BTNodeType type) const;
	ImColor GetNodeBorderColor(BTNodeType type) const;

	ed::NodeId GetNextNodeId() { return ed::NodeId(nextId_++); }
	ed::PinId  GetNextPinId() { return ed::PinId(nextId_++); }
	ed::LinkId GetNextLinkId() { return ed::LinkId(nextId_++); }

	ed::EditorContext* context_ = nullptr;

	std::vector<EditorNode>	nodes_;
	std::vector<EditorLink>	links_;

	uintptr_t	nextId_ = 1;				// Node/Pin/Linkの全IDで共有する採番カウンタ

	ImVec2		contextMenuPos_ = { 0, 0 };	// 右クリックでノード追加メニューを開いた位置

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	BTEditor();
	~BTEditor() {}	// 後片付けはShutdown()で明示的に行う(静的破棄順序に依存させない)

public:
	static BTEditor& Instance() {
		static BTEditor instance;
		return instance;
	}
};