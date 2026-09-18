#include "Application/main.h"

#include "BTEditor.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンストラクタ：エディタコンテキストを生成し、動作確認用のRootノードを1つ置いておく
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
BTEditor::BTEditor()
{
	ed::Config config;
	config.SettingsFile = nullptr;	// 現段階ではノード配置を永続化しない

	context_ = ed::CreateEditor(&config);

	ed::SetCurrentEditor(context_);

	EditorNode& root = AddNode(BTNodeType::Sequence, "Root", true);
	ed::SetNodePosition(root.id, ImVec2(100.0f, 100.0f));

	ed::SetCurrentEditor(nullptr);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// KdDebugGUI::GuiRelease() から、ImGui本体を破棄する前に明示的に呼び出すこと
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void BTEditor::Shutdown()
{
	if (!context_) return;

	ed::DestroyEditor(context_);
	context_ = nullptr;
}

void BTEditor::Update()
{
	if (!context_) return;

	ed::SetCurrentEditor(context_);

	ImGui::Begin("BT Editor");

	ed::Begin("BTEditorCanvas");

	for (auto& node : nodes_)
	{
		DrawNode(node);
	}

	for (auto& link : links_)
	{
		ed::Link(link.id, link.outputPin, link.inputPin);
	}

	HandleCreateLink();
	HandleDelete();

	// 背景を右クリックしたらノード追加メニューを開く。
	//	node-editorのキャンバス内でImGuiのポップアップを正しく描くには
	//	Suspend()/Resume()で挟む必要がある(キャンバス内は入力・描画領域を
	//	node-editor側が独自に扱っているため)
	ed::Suspend();
	if (ed::ShowBackgroundContextMenu())
	{
		contextMenuPos_ = ImGui::GetMousePos();
		ImGui::OpenPopup("CreateNodeMenu");
	}

	if (ImGui::BeginPopup("CreateNodeMenu"))
	{
		DrawCreateNodeMenu(contextMenuPos_);
		ImGui::EndPopup();
	}
	ed::Resume();

	ed::End();

	ImGui::End();

	ed::SetCurrentEditor(nullptr);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ノード1個分の描画(種類名 + 名前 + 入力ピン + (あれば)出力ピン)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void BTEditor::DrawNode(EditorNode& node)
{
	ed::PushStyleColor(ed::StyleColor_NodeBorder, GetNodeBorderColor(node.type));

	ed::BeginNode(node.id);
	ImGui::PushID(node.id.AsPointer());

	ImGui::TextDisabled("%s", GetNodeTypeName(node.type));
	ImGui::Text("%s", node.name.c_str());

	ed::BeginPin(node.inputPin, ed::PinKind::Input);
	ImGui::Text("-> In");
	ed::EndPin();

	if (node.hasOutput)
	{
		ImGui::SameLine();
		ed::BeginPin(node.outputPin, ed::PinKind::Output);
		ImGui::Text("Out ->");
		ed::EndPin();
	}

	EditorLink* incoming = FindLinkByInputPin(node.inputPin);
	if (incoming)
	{
		ImGui::SameLine();
		ImGui::Text("[%d]", incoming->order);
		ImGui::SameLine();
		if (ImGui::SmallButton("^")) { MoveChildOrder(node, -1); }
		ImGui::SameLine();
		if (ImGui::SmallButton("v")) { MoveChildOrder(node, +1); }
	}

	ImGui::PopID();
	ed::EndNode();

	ed::PopStyleColor();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ノード追加メニュー(右クリックメニューの中身)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void BTEditor::DrawCreateNodeMenu(const ImVec2& spawnScreenPos)
{
	ed::NodeId newNodeId;
	bool created = false;

	if (ImGui::MenuItem("Sequence"))
	{
		newNodeId = AddNode(BTNodeType::Sequence, "Sequence", true).id;
		created = true;
	}
	if (ImGui::MenuItem("Selector"))
	{
		newNodeId = AddNode(BTNodeType::Selector, "Selector", true).id;
		created = true;
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Condition"))
	{
		newNodeId = AddNode(BTNodeType::Condition, "Condition", false).id;
		created = true;
	}
	if (ImGui::MenuItem("Action"))
	{
		newNodeId = AddNode(BTNodeType::Action, "Action", false).id;
		created = true;
	}

	if (created)
	{
		// ライブラリのバージョンによってはScreenToCanvas()が無い場合がある。
		// その場合はed::SetNodePosition(newNodeId, spawnScreenPos)にそのまま
		// 差し替えて構わない(初期配置なので多少ズレても実害はない)
		ed::SetNodePosition(newNodeId, ed::ScreenToCanvas(spawnScreenPos));
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// リンク新規作成(ドラッグでピン同士をつないだ時の検証)。
//	ツリー構造として不正な接続(同一ノード同士、入力ピンへの複数接続)は弾く
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void BTEditor::HandleCreateLink()
{
	if (ed::BeginCreate())
	{
		ed::PinId pinA, pinB;
		if (ed::QueryNewLink(&pinA, &pinB) && pinA && pinB)
		{
			EditorNode* nodeA = FindNodeByPin(pinA);
			EditorNode* nodeB = FindNodeByPin(pinB);

			bool aIsOutput = nodeA && nodeA->hasOutput && nodeA->outputPin == pinA;
			bool bIsOutput = nodeB && nodeB->hasOutput && nodeB->outputPin == pinB;

			// ドラッグの始点・終点どちらが出力/入力かは操作順で変わるので、
			// ここで「出力→入力」の向きに揃える
			ed::PinId outputPin = aIsOutput ? pinA : (bIsOutput ? pinB : ed::PinId());
			ed::PinId inputPin = aIsOutput ? pinB : (bIsOutput ? pinA : ed::PinId());

			bool sameNode = (nodeA == nodeB);
			bool bothResolved = outputPin && inputPin;

			bool inputAlreadyUsed = std::any_of(links_.begin(), links_.end(),
				[&](const EditorLink& link) { return link.inputPin == inputPin; });

			if (!bothResolved || sameNode || inputAlreadyUsed)
			{
				ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
			}
			else if (ed::AcceptNewItem())
			{
				// 既にこの出力ピンにぶら下がっている兄弟の数 = 新しいリンクの順番
				int order = (int)std::count_if(links_.begin(), links_.end(),
					[&](const EditorLink& l) { return l.outputPin == outputPin; });

				links_.push_back({ GetNextLinkId(), outputPin, inputPin, order });
			}
		}
	}
	ed::EndCreate();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ノード・リンクの削除(Deleteキー等、node-editor標準の削除操作から呼ばれる)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void BTEditor::HandleDelete()
{
	if (ed::BeginDelete())
	{
		ed::LinkId deletedLinkId;
		while (ed::QueryDeletedLink(&deletedLinkId))
		{
			if (ed::AcceptDeletedItem())
			{
				links_.erase(std::remove_if(links_.begin(), links_.end(),
					[&](const EditorLink& link) { return link.id == deletedLinkId; }),
					links_.end());
			}
		}

		ed::NodeId deletedNodeId;
		while (ed::QueryDeletedNode(&deletedNodeId))
		{
			if (ed::AcceptDeletedItem())
			{
				EditorNode* deletedNode = FindNodeById(deletedNodeId);

				if (deletedNode)
				{
					// ノードを消す前に、そのノードに繋がっていたリンクも一緒に消す
					links_.erase(std::remove_if(links_.begin(), links_.end(),
						[&](const EditorLink& link) {
							return link.outputPin == deletedNode->outputPin
								|| link.inputPin == deletedNode->inputPin;
						}),
						links_.end());
				}

				nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
					[&](const EditorNode& node) { return node.id == deletedNodeId; }),
					nodes_.end());
			}
		}
	}
	ed::EndDelete();
}

BTEditor::EditorNode& BTEditor::AddNode(BTNodeType type, const std::string& name, bool hasOutput)
{
	EditorNode node;
	node.id = GetNextNodeId();
	node.type = type;
	node.name = name;
	node.inputPin = GetNextPinId();
	node.hasOutput = hasOutput;
	node.outputPin = hasOutput ? GetNextPinId() : ed::PinId();

	nodes_.push_back(node);
	return nodes_.back();
}

BTEditor::EditorNode* BTEditor::FindNodeById(ed::NodeId id)
{
	for (auto& node : nodes_)
	{
		if (node.id == id) return &node;
	}
	return nullptr;
}

BTEditor::EditorNode* BTEditor::FindNodeByPin(ed::PinId pin)
{
	for (auto& node : nodes_)
	{
		if (node.inputPin == pin) return &node;
		if (node.hasOutput && node.outputPin == pin) return &node;
	}
	return nullptr;
}

BTEditor::EditorLink* BTEditor::FindLinkByInputPin(ed::PinId inputPin)
{
	for (auto& link : links_)
	{
		if (link.inputPin == inputPin) return &link;
	}
	return nullptr;
}

std::vector<BTEditor::EditorLink*> BTEditor::GetSortedChildLinks(ed::PinId parentOutputPin)
{
	std::vector<EditorLink*> result;
	for (auto& link : links_)
	{
		if (link.outputPin == parentOutputPin) result.push_back(&link);
	}
	std::sort(result.begin(), result.end(),
		[](const EditorLink* a, const EditorLink* b) { return a->order < b->order; });
	return result;
}

void BTEditor::NormalizeChildOrder(ed::PinId parentOutputPin)
{
	auto children = GetSortedChildLinks(parentOutputPin);
	for (int i = 0; i < (int)children.size(); ++i)
	{
		children[i]->order = i;
	}
}

void BTEditor::MoveChildOrder(EditorNode& childNode, int direction)
{
	EditorLink* self = FindLinkByInputPin(childNode.inputPin);
	if (!self) return;

	int targetOrder = self->order + direction;

	for (auto& link : links_)
	{
		if (link.outputPin == self->outputPin && link.order == targetOrder)
		{
			std::swap(link.order, self->order);
			return;
		}
	}
}

const char* BTEditor::GetNodeTypeName(BTNodeType type) const
{
	switch (type)
	{
	case BTNodeType::Sequence:	return "Sequence";
	case BTNodeType::Selector:	return "Selector";
	case BTNodeType::Condition:	return "Condition";
	case BTNodeType::Action:	return "Action";
	}
	return "Unknown";
}

ImColor BTEditor::GetNodeBorderColor(BTNodeType type) const
{
	switch (type)
	{
	case BTNodeType::Sequence:	return ImColor(100, 150, 255);	// 青系：複合(順番に実行)
	case BTNodeType::Selector:	return ImColor(120, 200, 255);	// 水色系：複合(選択して実行)
	case BTNodeType::Condition:	return ImColor(255, 190, 80);	// 黄系：条件判定
	case BTNodeType::Action:	return ImColor(255, 130, 90);	// 橙系：実際の行動
	}
	return ImColor(200, 200, 200);
}