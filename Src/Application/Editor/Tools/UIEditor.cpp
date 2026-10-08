#include "Application/main.h"

#include "UIEditor.h"
#include "Application/Factories/ComponentRegistry.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

// プレビューRTの最大サイズ(これを超える基準解像度は実描画しない)
static constexpr float kMaxRefSize = 4096.0f;

// UI用テクスチャの置き場(登録一覧のパスはここからの相対)
static const std::string kTextureRoot = "Asset/Textures/UI/";

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期ドックレイアウト: 左Hierarchy / 右Inspector / 下UI Editor+UI Assets / 中央Canvas
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static void SetupUIDockLayout(ImGuiID dockspaceId, const ImVec2& size)
{
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, size);

	ImGuiID center = dockspaceId;

	ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f, nullptr, &center);
	ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, nullptr, &center);
	ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.15f, nullptr, &center);

	ImGui::DockBuilderDockWindow("UI Hierarchy", left);
	ImGui::DockBuilderDockWindow("UI Inspector", right);
	ImGui::DockBuilderDockWindow("UI Editor", bottom);
	ImGui::DockBuilderDockWindow("UI Assets", bottom);
	ImGui::DockBuilderDockWindow("UI Canvas", center);

	ImGui::DockBuilderFinish(dockspaceId);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 文字コード変換。エディタ内・JSON・ImGuiはUTF-8で持ち、既存ローダーに渡す時だけ変換する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static std::wstring Utf8ToWide(const std::string& s)
{
	if (s.empty()) return std::wstring();
	const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
	return w;
}

static std::string WideToUtf8(const std::wstring& w)
{
	if (w.empty()) return std::string();
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
	return s;
}

static std::string Utf8ToCodepage(const std::string& utf8, UINT codepage)
{
	const std::wstring w = Utf8ToWide(utf8);
	if (w.empty()) return std::string();
	const int n = WideCharToMultiByte(codepage, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(codepage, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
	return s;
}

// 登録一覧のパス → 実際に読み込むパス(UTF-8。絶対パスはそのまま、".."は畳んで正規化する)
static std::string FullTexturePath(const std::string& registered)
{
	const std::filesystem::path p(Utf8ToWide(registered));
	if (p.is_absolute()) return registered;
	return WideToUtf8((std::filesystem::path(kTextureRoot) / p).lexically_normal().generic_wstring());
}

// 正方形の枠にアスペクト比を保ってテクスチャを描く(未読み込みなら枠だけ)
static void DrawThumb(const KdTexture* tex, float box)
{
	const ImVec2 p = ImGui::GetCursorScreenPos();
	ImGui::Dummy(ImVec2(box, box));

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(p, ImVec2(p.x + box, p.y + box), IM_COL32(40, 40, 46, 255));

	if (!tex || !tex->GetSRView()) return;

	const float aspect = tex->GetAspectRatio();
	const ImVec2 sz = aspect >= 1.0f ? ImVec2(box, box / aspect) : ImVec2(box * aspect, box);
	const ImVec2 q(p.x + (box - sz.x) * 0.5f, p.y + (box - sz.y) * 0.5f);
	dl->AddImage((ImTextureID)(intptr_t)tex->GetSRView(), q, ImVec2(q.x + sz.x, q.y + sz.y));
}

// レジストリの初期値でコンポーネントを作る(未登録の型はparamsが空になる)
static ComponentEntry MakeEntry(const std::string& type)
{
	ComponentEntry entry;
	entry.type = type;
	entry.params = nlohmann::json::object();
	if (const auto* defaults = ComponentRegistry::Instance().FindDefaultParams(type))
	{
		entry.params = *defaults;
	}
	return entry;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 検索・階層ユーティリティ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
UIElement* UIEditor::FindElement(UIId id)
{
	if (id == kInvalidUIId) return nullptr;
	for (auto& e : m_elements) { if (e.id == id) return &e; }
	return nullptr;
}

const UIElement* UIEditor::FindElement(UIId id) const
{
	if (id == kInvalidUIId) return nullptr;
	for (auto& e : m_elements) { if (e.id == id) return &e; }
	return nullptr;
}

bool UIEditor::IsDescendant(UIId id, UIId ancestorId) const
{
	UIId cur = id;
	for (int i = 0; i < 64 && cur != kInvalidUIId; ++i)
	{
		const UIElement* e = FindElement(cur);
		if (!e) return false;
		cur = e->parentId;
		if (cur == ancestorId) return true;
	}
	return false;
}

void UIEditor::CollectVisibleOrder(UIId parentId, std::vector<UIId>& out, int depth) const
{
	if (depth > 32) return;

	for (auto& e : m_elements)
	{
		if (e.parentId != parentId || !e.visible) continue;
		out.push_back(e.id);
		CollectVisibleOrder(e.id, out, depth + 1);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// テキスト・テクスチャ・矩形
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
std::shared_ptr<KdTexture> UIEditor::GetTexture(const std::string& path)
{
	if (path.empty()) return nullptr;

	auto it = m_texCache.find(path);
	if (it != m_texCache.end()) return it->second;

	// 失敗はキャッシュしない(後からファイルを置いても拾えるように)。キャッシュキーはUTF-8、ローダーにはANSIを渡す
	const std::string native = Utf8ToCodepage(path, CP_ACP);
	if (!KdFileExistence(native)) return nullptr;

	std::shared_ptr<KdTexture> tex = KdAssets::Instance().m_textures.GetData(native);
	if (tex) { m_texCache[path] = tex; }
	return tex;
}

// TextComponent::Rebuild()と同じ手順。KdFontManagerはShift-JISで受け取る
const UIEditor::TextBlock* UIEditor::GetTextBlock(int fontNo, int antiAliasing, const std::string& textUtf8)
{
	fontNo = std::clamp(fontNo, 0, 9);
	antiAliasing = std::clamp(antiAliasing, 0, 3);

	const std::string key = std::to_string(fontNo) + "|" + std::to_string(antiAliasing) + "|" + textUtf8;

	auto it = m_textCache.find(key);
	if (it != m_textCache.end()) return &it->second;

	// 編集中の文字列ごとに増えるので、溜まりすぎたら捨てる
	if (m_textCache.size() > 256) { m_textCache.clear(); }

	const std::string sjis = Utf8ToCodepage(textUtf8, 932);

	TextBlock block;
	size_t start = 0;
	while (true)
	{
		const size_t end = sjis.find('\n', start);
		std::string line = sjis.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if (!line.empty() && line.back() == '\r') line.pop_back();

		auto sprite = KdFontManager::Instance().CreateFontTexture(fontNo, line, antiAliasing);
		if (sprite)
		{
			for (auto& ch : sprite->GetTexList())
			{
				if (ch->FontTex) block.lineHeight = (std::max)(block.lineHeight, (float)ch->FontTex->GetInfo().Height);
			}
			block.lines.push_back(sprite);
		}

		if (end == std::string::npos) break;
		start = end + 1;
	}

	return &(m_textCache[key] = std::move(block));
}

DirectX::SimpleMath::Vector2 UIEditor::GetIntrinsicSize(const UIElement& e)
{
	UILayout l;
	if (!ReadUILayout(e, l)) return DirectX::SimpleMath::Vector2(0.0f, 0.0f);

	// 文字: 文字ブロックの大きさ(TextComponent::DrawSpriteと同じ計算)
	if (const ComponentEntry* t = e.Find("Text"))
	{
		const TextBlock* tb = GetTextBlock(t->params.value("fontNo", 0), t->params.value("antiAliasing", 3),
			t->params.value("text", std::string()));
		if (!tb || tb->lines.empty()) return DirectX::SimpleMath::Vector2(0.0f, 0.0f);

		float w = 0.0f;
		for (auto& line : tb->lines) { w = (std::max)(w, line->GetTotalWidth() * l.scale.x); }
		return DirectX::SimpleMath::Vector2(w, tb->lineHeight * l.scale.y * (float)tb->lines.size());
	}

	// 画像: テクスチャの大きさ
	if (const ComponentEntry* i = e.Find("UIImage"))
	{
		if (const std::shared_ptr<KdTexture> tex = GetTexture(i->params.value("texture", std::string())))
		{
			return DirectX::SimpleMath::Vector2(tex->GetWidth() * l.scale.x, tex->GetHeight() * l.scale.y);
		}
	}

	return DirectX::SimpleMath::Vector2(0.0f, 0.0f);
}

bool UIEditor::GetElementRect(const UIElement& e, UIRect& out)
{
	UILayout l;
	if (!ReadUILayout(e, l) || l.world) return false;

	DirectX::SimpleMath::Vector2 s(l.size.x * l.scale.x, l.size.y * l.scale.y);
	if (l.size.x == 0.0f && l.size.y == 0.0f) { s = GetIntrinsicSize(e); }
	if (s.x <= 0.0f || s.y <= 0.0f) return false;

	out = ResolveUIBox(l, m_refSize, s);
	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Undo / Redo
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void UIEditor::PushUndo()
{
	UndoState state;
	state.elements = m_elements;
	state.selectedId = m_selectedId;

	m_undoStack.push_back(std::move(state));
	if (m_undoStack.size() > kMaxUndoDepth)
	{
		m_undoStack.erase(m_undoStack.begin());
	}

	m_redoStack.clear();
}

void UIEditor::Undo()
{
	if (m_undoStack.empty()) return;

	UndoState redoState;
	redoState.elements = m_elements;
	redoState.selectedId = m_selectedId;
	m_redoStack.push_back(std::move(redoState));

	UndoState prev = std::move(m_undoStack.back());
	m_undoStack.pop_back();

	m_elements = std::move(prev.elements);
	m_selectedId = prev.selectedId;
	m_dragMode = DragMode::None;
}

void UIEditor::Redo()
{
	if (m_redoStack.empty()) return;

	UndoState undoState;
	undoState.elements = m_elements;
	undoState.selectedId = m_selectedId;
	m_undoStack.push_back(std::move(undoState));

	UndoState next = std::move(m_redoStack.back());
	m_redoStack.pop_back();

	m_elements = std::move(next.elements);
	m_selectedId = next.selectedId;
	m_dragMode = DragMode::None;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレームの更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void UIEditor::Update()
{
	CheckHotReload();

	ImGuiViewport* mainViewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(
		ImVec2(mainViewport->Pos.x + mainViewport->Size.x + 20.0f, mainViewport->Pos.y),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(1100.0f, 700.0f), ImGuiCond_FirstUseEver);

	ImGui::Begin("UI Editor Window");
	{
		ImGuiID dockId = ImGui::GetID("UIDockSpace");

		if (ImGui::DockBuilderGetNode(dockId) == nullptr)
		{
			SetupUIDockLayout(dockId, ImGui::GetContentRegionAvail());
		}

		ImGui::DockSpace(dockId, ImVec2(0, 0));
	}
	ImGui::End();

	m_hasFocus = false;
	DrawMainMenu();
	DrawHierarchy();
	DrawInspector();
	DrawAssetPicker();
	DrawCanvas();

	// ショートカットはUIエディタにフォーカスがある時だけ(MapEditor等のCtrl+Zと二重に効かないように)
	if (m_hasFocus && !ImGui::GetIO().WantTextInput)
	{
		const bool ctrl = ImGui::GetIO().KeyCtrl;
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
		{
			if (ImGui::GetIO().KeyShift) { Redo(); }
			else { Undo(); }
		}
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) { Redo(); }

		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) { RemoveSelected(); }
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// プレビューの実描画
//	基準解像度のオフスクリーンへKdSpriteShaderで描く。UIImage・Textはゲーム側の
//	コンポーネント(UIImageComponent / TextComponent)と同じ計算で、座標も同じ
//	「画面中央が原点・Y上向き」で描く
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void UIEditor::RenderPreviewViewport()
{
	const int w = (int)m_refSize.x;
	const int h = (int)m_refSize.y;
	if (w <= 0 || h <= 0 || w > (int)kMaxRefSize || h > (int)kMaxRefSize) return;

	// 基準解像度が変わったらバッファを作り直す
	if (!m_previewTex.WorkRTView() || m_previewTex.GetWidth() != (UINT)w || m_previewTex.GetHeight() != (UINT)h)
	{
		if (!m_previewTex.CreateRenderTarget(w, h)) return;
	}

	ID3D11DeviceContext* ctx = KdDirect3D::Instance().WorkDevContext();

	// RT・ビューポートを退避(MRT構成でも壊さないよう全スロット分)
	ID3D11RenderTargetView* oldRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
	ID3D11DepthStencilView* oldDSV = nullptr;
	ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRTVs, &oldDSV);

	UINT oldVPNum = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	D3D11_VIEWPORT oldVPs[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
	ctx->RSGetViewports(&oldVPNum, oldVPs);

	// プレビューRTへ切り替え。背景はCanvasと同じ色で塗りつぶす
	ID3D11RenderTargetView* rtv = m_previewTex.WorkRTView();
	ctx->OMSetRenderTargets(1, &rtv, nullptr);

	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
	ctx->RSSetViewports(1, &vp);

	const float clearColor[4] = { 52.0f / 255.0f, 52.0f / 255.0f, 60.0f / 255.0f, 1.0f };
	ctx->ClearRenderTargetView(rtv, clearColor);

	// 描画
	KdSpriteShader& spr = KdShaderManager::Instance().m_spriteShader;

	// UIImageComponent::DrawSprite()と同じ
	auto drawImage = [&](const UIElement& e, const UILayout& l)
		{
			const ComponentEntry* c = e.Find("UIImage");
			if (!c) return;

			const std::shared_ptr<KdTexture> tex = GetTexture(c->params.value("texture", std::string()));

			DirectX::SimpleMath::Vector2 size(l.size.x * l.scale.x, l.size.y * l.scale.y);
			if (size.x == 0.0f && size.y == 0.0f && tex)
			{
				size = DirectX::SimpleMath::Vector2(tex->GetWidth() * l.scale.x, tex->GetHeight() * l.scale.y);
			}
			if (size.x <= 0.0f || size.y <= 0.0f) return;

			const DirectX::SimpleMath::Vector2 base = ResolveUIBase(l, m_refSize);
			const DirectX::SimpleMath::Vector2 min = base - DirectX::SimpleMath::Vector2(size.x * l.pivot.x, size.y * l.pivot.y);

			const DirectX::SimpleMath::Vector4 cv = UIDataDetail::ReadVec4(c->params, "color", DirectX::SimpleMath::Vector4(1, 1, 1, 1));
			const Math::Color color(cv.x, cv.y, cv.z, cv.w);

			if (tex)
			{
				spr.DrawTex(tex.get(), (int)min.x, (int)min.y, (int)(size.x + 0.5f), (int)(size.y + 0.5f), nullptr, &color, Math::Vector2(0.0f, 0.0f));
			}
			else
			{
				spr.DrawBox((int)(min.x + size.x * 0.5f), (int)(min.y + size.y * 0.5f),
					(int)(size.x * 0.5f), (int)(size.y * 0.5f), &color, true);
			}
		};

	// TextComponent::DrawSprite()と同じ
	auto drawText = [&](const UIElement& e, const UILayout& l)
		{
			const ComponentEntry* t = e.Find("Text");
			if (!t) return;

			const TextBlock* tb = GetTextBlock(t->params.value("fontNo", 0), t->params.value("antiAliasing", 3),
				t->params.value("text", std::string()));
			if (!tb || tb->lines.empty()) return;

			const DirectX::SimpleMath::Vector4 cv = UIDataDetail::ReadVec4(t->params, "color", DirectX::SimpleMath::Vector4(1, 1, 1, 1));
			const Math::Color color(cv.x, cv.y, cv.z, cv.w);
			const std::string align = t->params.value("align", std::string("Left"));

			const DirectX::SimpleMath::Vector2 base = ResolveUIBase(l, m_refSize);

			const float lineH = tb->lineHeight * l.scale.y;
			float blockW = 0.0f;
			for (auto& line : tb->lines) { blockW = (std::max)(blockW, line->GetTotalWidth() * l.scale.x); }
			const float blockH = lineH * (float)tb->lines.size();

			const float left = base.x - blockW * l.pivot.x;
			float y = base.y + blockH * (1.0f - l.pivot.y) - lineH;

			for (auto& line : tb->lines)
			{
				const float lineW = line->GetTotalWidth() * l.scale.x;

				float x = left;
				if (align == "Center")		x += (blockW - lineW) * 0.5f;
				else if (align == "Right")	x += blockW - lineW;

				for (auto& ch : line->GetTexList())
				{
					if (!ch->FontTex) continue;

					const float cw = ch->FontTex->GetInfo().Width * l.scale.x;
					const float chh = ch->FontTex->GetInfo().Height * l.scale.y;

					spr.DrawTex(ch->FontTex.get(), (int)x, (int)y, (int)(cw + 0.5f), (int)(chh + 0.5f), nullptr, &color, Math::Vector2(0.0f, 0.0f));
					x += cw;
				}
				y -= lineH;
			}
		};

	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Alpha);
	spr.Begin();

	std::vector<UIId> order;
	CollectVisibleOrder(kInvalidUIId, order);

	for (UIId id : order)
	{
		const UIElement* e = FindElement(id);
		if (!e) continue;

		UILayout l;
		if (!ReadUILayout(*e, l) || l.world) continue;

		drawImage(*e, l);
		drawText(*e, l);
	}

	spr.End();
	KdShaderManager::Instance().UndoBlendState();

	// RT・ビューポートを復元
	ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRTVs, oldDSV);
	ctx->RSSetViewports(oldVPNum, oldVPs);

	for (auto& v : oldRTVs) { KdSafeRelease(v); }
	KdSafeRelease(oldDSV);

	m_previewRendered = true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// メニュー(セーブ/ロード・Undo・キャンバス設定)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void UIEditor::DrawMainMenu()
{
	ImGui::Begin("UI Editor");
	if (ImGui::IsWindowFocused()) { m_hasFocus = true; }

	ImGui::InputText("Path", m_filePathBuf, sizeof(m_filePathBuf));

	if (ImGui::Button("Save")) { Save(m_filePathBuf); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { Load(m_filePathBuf); }
	ImGui::SameLine();
	ImGui::Checkbox("Auto Reload", &m_autoReload);

	ImGui::SameLine();
	ImGui::BeginDisabled(m_undoStack.empty());
	if (ImGui::Button("Undo")) { Undo(); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(m_redoStack.empty());
	if (ImGui::Button("Redo")) { Redo(); }
	ImGui::EndDisabled();

	ImGui::Separator();

	ImGui::SetNextItemWidth(160.0f);
	ImGui::DragFloat2("Reference Size", &m_refSize.x, 1.0f, 100.0f, kMaxRefSize, "%.0f");
	ImGui::SameLine();
	if (ImGui::Button("Fit")) { m_fitRequested = true; }
	ImGui::SameLine();
	ImGui::Checkbox("Snap", &m_useSnap);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(60.0f);
	ImGui::DragFloat("##snap", &m_snapValue, 0.5f, 1.0f, 256.0f, "%.0f");
	ImGui::SameLine();
	ImGui::Text("Elements : %d", (int)m_elements.size());

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 階層ウィンドウ(ツリー表示・追加/削除・並び替え・ドラッグで親子付け替え)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void UIEditor::DrawHierarchyNode(UIId id)
{
	const UIElement* e = FindElement(id);
	if (!e) return;

	std::vector<UIId> children;
	for (auto& c : m_elements) { if (c.parentId == id) children.push_back(c.id); }

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth
		| ImGuiTreeNodeFlags_DefaultOpen;
	if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
	if (m_selectedId == id) flags |= ImGuiTreeNodeFlags_Selected;

	// UITransform以外で最初のコンポーネント名を種別として表示する
	std::string tag = "-";
	for (auto& c : e->components) { if (c.type != "UITransform") { tag = c.type; break; } }

	const std::string label = "[" + tag + "] " + e->name + "##" + std::to_string(id);

	if (!e->visible) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
	if (!e->visible) ImGui::PopStyleColor();

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { m_selectedId = id; }

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("UI_ELEMENT", &id, sizeof(UIId));
		ImGui::TextUnformatted(e->name.c_str());
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("UI_ELEMENT"))
		{
			m_hasPendingReparent = true;
			m_pendingReparentId = *static_cast<const UIId*>(payload->Data);
			m_pendingParentId = id;
		}
		ImGui::EndDragDropTarget();
	}

	if (open)
	{
		for (UIId cid : children) { DrawHierarchyNode(cid); }
		ImGui::TreePop();
	}
}

void UIEditor::DrawHierarchy()
{
	ImGui::Begin("UI Hierarchy");
	if (ImGui::IsWindowFocused()) { m_hasFocus = true; }

	if (ImGui::Button("+ Image")) { AddElement("Image"); }
	ImGui::SameLine();
	if (ImGui::Button("+ Text")) { AddElement("Text"); }
	ImGui::SameLine();
	if (ImGui::Button("+ Button")) { AddElement("Button"); }

	ImGui::BeginDisabled(FindSelected() == nullptr);
	if (ImGui::Button("- Remove")) { RemoveSelected(); }
	ImGui::SameLine();
	if (ImGui::Button("Up")) { MoveSibling(m_selectedId, -1); }
	ImGui::SameLine();
	if (ImGui::Button("Down")) { MoveSibling(m_selectedId, +1); }
	ImGui::EndDisabled();

	ImGui::Separator();

	std::vector<UIId> roots;
	for (auto& e : m_elements) { if (e.parentId == kInvalidUIId) roots.push_back(e.id); }
	for (UIId id : roots) { DrawHierarchyNode(id); }

	// 空き領域へのドロップで最上位へ戻す
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	ImGui::InvisibleButton("##rootdrop", ImVec2(avail.x, avail.y > 24.0f ? avail.y : 24.0f));
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("UI_ELEMENT"))
		{
			m_hasPendingReparent = true;
			m_pendingReparentId = *static_cast<const UIId*>(payload->Data);
			m_pendingParentId = kInvalidUIId;
		}
		ImGui::EndDragDropTarget();
	}

	if (m_hasPendingReparent)
	{
		Reparent(m_pendingReparentId, m_pendingParentId);
		m_hasPendingReparent = false;
	}

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// インスペクター(名前 + MapEditorと同じコンポーネント編集)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void UIEditor::DrawInspector()
{
	ImGui::Begin("UI Inspector");
	if (ImGui::IsWindowFocused()) { m_hasFocus = true; }

	UIElement* e = FindSelected();
	if (!e)
	{
		ImGui::TextDisabled("要素が選択されていません");
		ImGui::End();
		return;
	}

	char nameBuf[128];
	strncpy_s(nameBuf, sizeof(nameBuf), e->name.c_str(), _TRUNCATE);
	if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) { e->name = nameBuf; }
	if (ImGui::IsItemActivated()) { PushUndo(); }

	ImGui::Checkbox("Visible (Editor)", &e->visible);
	if (ImGui::IsItemActivated()) { PushUndo(); }

	if (const UIElement* parent = FindElement(e->parentId))
	{
		ImGui::Text("Parent : %s", parent->name.c_str());
	}

	// 表示に必要な条件のヒント
	UILayout l;
	if (!ReadUILayout(*e, l))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "UITransformが無いため表示されません");
	}
	else if (l.world)
	{
		ImGui::TextDisabled("Space=Worldはエディタでは表示できません");
	}

	ImGui::Separator();
	ComponentInspector::DrawList(
		e->components,
		[this]() { PushUndo(); },
		[](const std::string&) {});

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// キャンバス(実描画プレビュー + ギズモ操作)
//	ホイール:ズーム / 中ボタンドラッグ:パン / 左ドラッグ:選択・移動(Shiftで軸固定)
//	/ 四辺・四隅のハンドルでリサイズ(sizeを持つ要素のみ)。水色の●=アンカー位置、橙の○=ピボット位置
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void UIEditor::DrawCanvas()
{
	using namespace DirectX::SimpleMath;

	ImGui::Begin("UI Canvas", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	if (ImGui::IsWindowFocused()) { m_hasFocus = true; }

	const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	if (canvasSize.x < 50.0f || canvasSize.y < 50.0f) { ImGui::End(); return; }
	const ImVec2 canvasEnd(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

	ImGui::InvisibleButton("##canvas", canvasSize,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();
	ImGuiIO& io = ImGui::GetIO();

	// Assetsからのドラッグ&ドロップ(要素の追加はtoRef定義後に行う)
	int dropTextureIdx = -1;
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("UI_TEXTURE"))
		{
			const int idx = *static_cast<const int*>(payload->Data);
			if (idx >= 0 && idx < (int)m_textureFileList.size()) { dropTextureIdx = idx; }
		}
		ImGui::EndDragDropTarget();
	}

	// 画面全体が収まるズーム・パンにする
	if (m_fitRequested)
	{
		m_zoom = (std::min)(canvasSize.x / m_refSize.x, canvasSize.y / m_refSize.y) * 0.9f;
		m_canvasPan = Vector2((canvasSize.x - m_refSize.x * m_zoom) * 0.5f,
			(canvasSize.y - m_refSize.y * m_zoom) * 0.5f);
		m_fitRequested = false;
	}

	// ホイールズーム(カーソル下の座標が動かないようにパンを補正する)
	if (hovered && io.MouseWheel != 0.0f)
	{
		const float newZoom = std::clamp(m_zoom * (io.MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f), 0.1f, 8.0f);
		const Vector2 mouseLocal(io.MousePos.x - canvasPos.x, io.MousePos.y - canvasPos.y);
		m_canvasPan = mouseLocal - (mouseLocal - m_canvasPan) * (newZoom / m_zoom);
		m_zoom = newZoom;
	}

	// 中ボタンでパン
	if (active && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
	{
		m_canvasPan.x += io.MouseDelta.x;
		m_canvasPan.y += io.MouseDelta.y;
	}

	const float zoom = m_zoom;
	const ImVec2 origin(canvasPos.x + m_canvasPan.x, canvasPos.y + m_canvasPan.y);
	auto toScreen = [&](const Vector2& p) { return ImVec2(origin.x + p.x * zoom, origin.y + p.y * zoom); };
	auto toRef = [&](const ImVec2& s) { return Vector2((s.x - origin.x) / zoom, (s.y - origin.y) / zoom); };
	// 要素の画面上の矩形。表示できない要素はfalse
	auto elementRect = [&](const UIElement& e, ImVec2& p0, ImVec2& p1)
		{
			UIRect r;
			if (!GetElementRect(e, r)) return false;
			p0 = toScreen(r.min);
			p1 = toScreen(r.min + r.size);
			return true;
		};
	// sizeを持つ要素だけリサイズできる(文字・画像の自動サイズは対象外)
	auto isResizable = [](const UIElement& e)
		{
			UILayout l;
			return ReadUILayout(e, l) && !l.world && (l.size.x != 0.0f || l.size.y != 0.0f);
		};
	// ハンドルの画面座標(hx,hy は -1/0/1)
	auto handlePoint = [](const ImVec2& p0, const ImVec2& p1, int hx, int hy)
		{
			return ImVec2(p0.x + (hx + 1) * 0.5f * (p1.x - p0.x), p0.y + (hy + 1) * 0.5f * (p1.y - p0.y));
		};
	auto snap = [&](float v)
		{
			return (m_useSnap && m_snapValue > 0.0f) ? std::round(v / m_snapValue) * m_snapValue : v;
		};

	if (dropTextureIdx >= 0)
	{
		AddImageFromTexture(FullTexturePath(m_textureFileList[dropTextureIdx]), toRef(io.MousePos));
	}

	std::vector<UIId> order;
	CollectVisibleOrder(kInvalidUIId, order);

	// 左クリック: ハンドル → 要素の順で判定して、ドラッグを開始する
	if (ImGui::IsItemActivated() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		m_dragMode = DragMode::None;
		const ImVec2 mp = io.MousePos;
		constexpr float kHandleHit = 7.0f;

		UIElement* sel = FindSelected();
		ImVec2 sp0, sp1;
		if (sel && sel->visible && isResizable(*sel) && elementRect(*sel, sp0, sp1))
		{
			for (int i = 0; i < 9 && m_dragMode == DragMode::None; ++i)
			{
				const int hx = i % 3 - 1, hy = i / 3 - 1;
				if (hx == 0 && hy == 0) continue;
				const ImVec2 hp = handlePoint(sp0, sp1, hx, hy);
				if (std::fabs(mp.x - hp.x) <= kHandleHit && std::fabs(mp.y - hp.y) <= kHandleHit)
				{
					m_dragMode = DragMode::Resize;
					m_dragHx = hx;
					m_dragHy = hy;
				}
			}
		}

		if (m_dragMode == DragMode::None)
		{
			UIId picked = kInvalidUIId;
			for (auto it = order.rbegin(); it != order.rend(); ++it)
			{
				const UIElement* e = FindElement(*it);
				if (!e) continue;
				ImVec2 p0, p1;
				if (!elementRect(*e, p0, p1)) continue;
				if (mp.x >= p0.x && mp.x <= p1.x && mp.y >= p0.y && mp.y <= p1.y) { picked = *it; break; }
			}
			m_selectedId = picked;
			if (picked != kInvalidUIId) { m_dragMode = DragMode::Move; }
		}

		UIRect r;
		if (UIElement* e = FindSelected(); e && m_dragMode != DragMode::None && GetElementRect(*e, r))
		{
			m_dragStartMin = r.min;
			m_dragStartSize = r.size;
			m_dragStartMouse = toRef(mp);
			m_dragMoved = false;
		}
		else
		{
			m_dragMode = DragMode::None;
		}
	}

	// ドラッグ中: 開始時点からの累積移動量で矩形を求め、UITransformのposition/sizeへ逆算して書き込む
	if (m_dragMode != DragMode::None)
	{
		UIElement* e = FindSelected();
		if (!e || !ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			m_dragMode = DragMode::None;
		}
		else
		{
			Vector2 d = toRef(io.MousePos) - m_dragStartMouse;
			if (m_dragMoved || d.x != 0.0f || d.y != 0.0f)
			{
				if (!m_dragMoved) { PushUndo(); m_dragMoved = true; }

				Vector2 mn = m_dragStartMin;
				Vector2 mx = m_dragStartMin + m_dragStartSize;

				if (m_dragMode == DragMode::Move)
				{
					// Shift押下中は移動量の大きい軸だけに固定する
					if (io.KeyShift)
					{
						if (std::fabs(d.x) >= std::fabs(d.y)) { d.y = 0.0f; }
						else { d.x = 0.0f; }
					}
					mn += d;
					mn.x = snap(mn.x);
					mn.y = snap(mn.y);
					mx = mn + m_dragStartSize;
				}
				else
				{
					if (m_dragHx < 0) { mn.x = snap(mn.x + d.x); mn.x = (std::min)(mn.x, mx.x - 1.0f); }
					if (m_dragHx > 0) { mx.x = snap(mx.x + d.x); mx.x = (std::max)(mx.x, mn.x + 1.0f); }
					if (m_dragHy < 0) { mn.y = snap(mn.y + d.y); mn.y = (std::min)(mn.y, mx.y - 1.0f); }
					if (m_dragHy > 0) { mx.y = snap(mx.y + d.y); mx.y = (std::max)(mx.y, mn.y + 1.0f); }
				}

				UIRect rect;
				rect.min = mn;
				rect.size = mx - mn;
				ApplyUIRect(*e, m_refSize, rect, m_dragMode == DragMode::Resize);
			}
		}
	}

	// ---- 描画 ----
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->PushClipRect(canvasPos, canvasEnd, true);
	dl->AddRectFilled(canvasPos, canvasEnd, IM_COL32(30, 30, 34, 255));

	const ImVec2 screenMin = toScreen(Vector2(0.0f, 0.0f));
	const ImVec2 screenMax = toScreen(m_refSize);
	dl->AddRectFilled(screenMin, screenMax, IM_COL32(52, 52, 60, 255));

	// 実描画プレビュー(UIImage・Text)。RenderPreviewViewport()が呼ばれるまでは背景のみ
	if (m_previewRendered && m_previewTex.WorkSRView() != nullptr)
	{
		dl->AddImage((ImTextureID)(intptr_t)m_previewTex.WorkSRView(), screenMin, screenMax);
	}
	dl->AddRect(screenMin, screenMax, IM_COL32(130, 130, 140, 255));

	// 各要素の範囲(Selectableは水色=当たり判定の範囲)
	for (UIId id : order)
	{
		const UIElement* e = FindElement(id);
		ImVec2 p0, p1;
		if (!e || !elementRect(*e, p0, p1)) continue;

		const bool selectable = e->Find("Selectable") != nullptr;
		dl->AddRect(p0, p1, selectable ? IM_COL32(80, 200, 255, 110) : IM_COL32(255, 255, 255, 40));
	}

	// 選択枠・リサイズハンドル・アンカー/ピボット
	if (const UIElement* sel = FindSelected(); sel && sel->visible)
	{
		ImVec2 p0, p1;
		if (elementRect(*sel, p0, p1))
		{
			dl->AddRect(p0, p1, IM_COL32(255, 210, 60, 255), 0.0f, 0, 2.0f);

			if (isResizable(*sel))
			{
				for (int i = 0; i < 9; ++i)
				{
					const int hx = i % 3 - 1, hy = i / 3 - 1;
					if (hx == 0 && hy == 0) continue;
					const ImVec2 hp = handlePoint(p0, p1, hx, hy);
					dl->AddRectFilled(ImVec2(hp.x - 4.0f, hp.y - 4.0f), ImVec2(hp.x + 4.0f, hp.y + 4.0f),
						IM_COL32(255, 210, 60, 255));
				}
			}
		}

		UILayout l;
		if (ReadUILayout(*sel, l) && !l.world)
		{
			// anchorは画面基準・Y上向き。エディタ座標(Y下向き)へ直して表示する
			dl->AddCircleFilled(toScreen(Vector2(l.anchor.x * m_refSize.x, (1.0f - l.anchor.y) * m_refSize.y)),
				5.0f, IM_COL32(80, 200, 255, 255));

			const Vector2 base = ResolveUIBase(l, m_refSize);
			dl->AddCircle(toScreen(Vector2(base.x + m_refSize.x * 0.5f, m_refSize.y * 0.5f - base.y)),
				6.0f, IM_COL32(255, 140, 60, 255), 12, 2.0f);
		}
	}

	char info[128];
	snprintf(info, sizeof(info), "Zoom %.0f%%  |  ホイール:ズーム  中ボタン:パン  Shift+移動:軸固定", zoom * 100.0f);
	dl->AddText(ImVec2(canvasPos.x + 8.0f, canvasPos.y + 6.0f), IM_COL32(180, 180, 190, 255), info);

	dl->PopClipRect();

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 要素の追加/削除/付け替え/並び替え
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void UIEditor::AddElement(const std::string& preset)
{
	PushUndo();

	UIElement e;
	e.id = m_nextId++;
	e.name = preset + std::to_string(e.id);

	// 選択中の要素があれば、その兄弟として追加する
	if (const UIElement* sel = FindSelected()) { e.parentId = sel->parentId; }

	ComponentEntry transform = MakeEntry("UITransform");

	if (preset == "Image")
	{
		transform.params["size"] = { 100.0f, 100.0f };
		e.components = { transform, MakeEntry("UIImage") };
	}
	else if (preset == "Text")
	{
		// sizeは0のまま(文字ブロックの大きさになる)
		ComponentEntry text = MakeEntry("Text");
		text.params["text"] = "Text";
		e.components = { transform, text };
	}
	else
	{
		// ボタン: sizeが当たり判定の範囲。文字は同じ基準点・pivotで中央揃えにする
		transform.params["size"] = { 200.0f, 60.0f };
		ComponentEntry text = MakeEntry("Text");
		text.params["text"] = "Button";
		text.params["align"] = "Center";
		e.components = { transform, MakeEntry("Selectable"), text };
	}

	m_selectedId = e.id;
	m_elements.push_back(std::move(e));
}

void UIEditor::RemoveSelected()
{
	if (!FindSelected()) return;

	PushUndo();

	// 子孫もまとめて削除する
	std::vector<UIId> removeIds = { m_selectedId };
	for (size_t i = 0; i < removeIds.size(); ++i)
	{
		for (auto& e : m_elements)
		{
			if (e.parentId == removeIds[i]) removeIds.push_back(e.id);
		}
	}

	m_elements.erase(
		std::remove_if(m_elements.begin(), m_elements.end(),
			[&](const UIElement& e) { return std::find(removeIds.begin(), removeIds.end(), e.id) != removeIds.end(); }),
		m_elements.end());

	m_selectedId = kInvalidUIId;
}

void UIEditor::Reparent(UIId id, UIId newParentId)
{
	UIElement* e = FindElement(id);
	if (!e || e->parentId == newParentId) return;

	// 自分自身・自分の子孫の下には入れない
	if (newParentId != kInvalidUIId && (newParentId == id || IsDescendant(newParentId, id))) return;

	PushUndo();
	e->parentId = newParentId;	// 座標は画面基準なので、付け替えても見た目は変わらない
}

void UIEditor::MoveSibling(UIId id, int dir)
{
	int idx = -1;
	for (int i = 0; i < (int)m_elements.size(); ++i) { if (m_elements[i].id == id) { idx = i; break; } }
	if (idx < 0) return;

	const UIId parentId = m_elements[idx].parentId;
	int other = -1;
	if (dir < 0)
	{
		for (int i = idx - 1; i >= 0; --i) { if (m_elements[i].parentId == parentId) { other = i; break; } }
	}
	else
	{
		for (int i = idx + 1; i < (int)m_elements.size(); ++i) { if (m_elements[i].parentId == parentId) { other = i; break; } }
	}
	if (other < 0) return;

	PushUndo();
	std::swap(m_elements[idx], m_elements[other]);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// セーブ/ロード
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void UIEditor::Save(const std::string& path)
{
	UIFile file;
	file.refSize = m_refSize;
	file.nextId = m_nextId;
	file.elements = m_elements;

	if (!SaveUIFile(path, file))
	{
		KdDebugGUI::Instance().AddLog("UIEditor: 保存に失敗 %s\n", path.c_str());
		return;
	}

	// 自分の保存を外部変更と誤検知して再ロードしないよう、更新日時を覚えておく
	FILETIME writeTime;
	if (JsonLoader::GetLastWriteTime(path, writeTime)) { m_lastWriteTime = writeTime; }

	KdDebugGUI::Instance().AddLog("UIEditor: 保存しました %s\n", path.c_str());
}

bool UIEditor::Load(const std::string& path)
{
	UIFile file;
	if (!LoadUIFile(path, file))
	{
		KdDebugGUI::Instance().AddLog("UIEditor: 読み込み失敗 %s\n", path.c_str());
		return false;
	}

	// 存在しない親を指す要素は最上位へ戻す
	for (auto& e : file.elements)
	{
		if (e.parentId == kInvalidUIId) continue;

		bool found = false;
		for (auto& p : file.elements) { if (p.id == e.parentId) { found = true; break; } }
		if (!found) e.parentId = kInvalidUIId;
	}

	const UIId keepSelectedId = m_selectedId;

	m_elements = std::move(file.elements);
	m_refSize = file.refSize;
	m_nextId = file.nextId;
	m_selectedId = FindElement(keepSelectedId) ? keepSelectedId : kInvalidUIId;

	m_undoStack.clear();
	m_redoStack.clear();
	m_dragMode = DragMode::None;
	m_texCache.clear();
	m_textCache.clear();

	FILETIME writeTime;
	if (JsonLoader::GetLastWriteTime(path, writeTime)) { m_lastWriteTime = writeTime; }

	KdDebugGUI::Instance().AddLog("UIEditor: 読み込みました %s\n", path.c_str());
	return true;
}

// 外部でJSONが更新されたら自動で再読み込みする(編集中の未保存の変更は破棄される)
void UIEditor::CheckHotReload()
{
	if (!m_autoReload) return;

	m_reloadCheckTimer += Application::Instance().GetDeltaTime();
	if (m_reloadCheckTimer < 0.5f) return;
	m_reloadCheckTimer = 0.0f;

	FILETIME writeTime;
	if (!JsonLoader::GetLastWriteTime(m_filePathBuf, writeTime)) return;

	if (CompareFileTime(&writeTime, &m_lastWriteTime) == 0) return;

	// 読み込み失敗時に毎回リトライしてログが溢れないよう、先に更新しておく
	m_lastWriteTime = writeTime;
	Load(m_filePathBuf);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンストラクタ：既存のUIデータがあれば自動ロードする
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
UIEditor::UIEditor()
{
	if (std::filesystem::exists(m_filePathBuf)) { Load(m_filePathBuf); }
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// テクスチャの登録・選択・利用
//	登録一覧(JSON)の持ち方・ファイルダイアログはMapEditorのアセット登録と同じ、
//	フォルダ走査の拡張子判定はEffectEditorと同じ。選択したテクスチャは
//	選択中の要素のUIImageのtextureへ書き込まれる(UIImageが無ければ追加する)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void UIEditor::DrawAssetPicker()
{
	ImGui::Begin("UI Assets");
	if (ImGui::IsWindowFocused()) { m_hasFocus = true; }

	if (!m_assetListLoaded)
	{
		LoadTextureRegistry(m_registryPathBuf);
		m_assetListLoaded = true;
	}

	if (ImGui::Button("+ Add File...")) { AddTextureViaFileDialog(); }
	ImGui::SameLine();
	if (ImGui::Button("Scan Folder")) { ScanTextureFolder(); }
	ImGui::SameLine();
	ImGui::BeginDisabled(m_selectedAsset < 0);
	if (ImGui::Button("- Remove")) { RemoveRegisteredTexture(m_selectedAsset); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("Scan: %s", kTextureRoot.c_str());

	ImGui::Separator();

	UIElement* sel = FindSelected();

	if (sel)
	{
		std::string current;
		if (const ComponentEntry* img = sel->Find("UIImage")) { current = img->params.value("texture", std::string()); }

		ImGui::Text("Current : %s", current.empty() ? "(None)" : current.c_str());

		if (!current.empty())
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Fit Size"))
			{
				const std::shared_ptr<KdTexture> tex = GetTexture(current);
				UILayout l;
				if (tex && ReadUILayout(*sel, l))
				{
					PushUndo();
					l.size = DirectX::SimpleMath::Vector2((float)tex->GetWidth(), (float)tex->GetHeight());
					WriteUILayout(*sel, l);
				}
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear"))
			{
				PushUndo();
				sel->Find("UIImage")->params["texture"] = "";
			}
		}
	}
	else
	{
		ImGui::TextDisabled("要素を選択してクリックで割り当て(UIImageが無ければ追加) / Canvasへドラッグで新規配置");
	}

	ImGui::Separator();

	if (m_textureFileList.empty())
	{
		ImGui::TextDisabled("登録されたテクスチャがありません。「+ Add File...」か「Scan Folder」で追加してください");
	}

	constexpr float kThumb = 32.0f;

	// 画面内の行だけ描画・読み込みする
	ImGuiListClipper clipper;
	clipper.Begin((int)m_textureFileList.size(), kThumb + ImGui::GetStyle().ItemSpacing.y);
	while (clipper.Step())
	{
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
		{
			const std::string& rel = m_textureFileList[i];

			ImGui::PushID(i);

			DrawThumb(GetTexture(FullTexturePath(rel)).get(), kThumb);
			ImGui::SameLine();

			if (ImGui::Selectable(rel.c_str(), m_selectedAsset == i, 0, ImVec2(0.0f, kThumb)))
			{
				m_selectedAsset = i;

				if (sel)
				{
					PushUndo();

					ComponentEntry* image = sel->Find("UIImage");
					if (!image)
					{
						sel->components.push_back(MakeEntry("UIImage"));
						image = &sel->components.back();
					}
					image->params["texture"] = FullTexturePath(rel);
				}
			}

			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload("UI_TEXTURE", &i, sizeof(int));
				ImGui::TextUnformatted(rel.c_str());
				ImGui::EndDragDropSource();
			}

			ImGui::PopID();
		}
	}

	ImGui::End();
}

void UIEditor::LoadTextureRegistry(const std::string& path)
{
	m_textureFileList.clear();
	m_selectedAsset = -1;

	nlohmann::json j;
	if (!JsonLoader::Load(path, j)) { return; }

	try
	{
		for (auto& e : j) { m_textureFileList.push_back(e.get<std::string>()); }
	}
	catch (const nlohmann::json::exception&)
	{
		m_textureFileList.clear();
		KdDebugGUI::Instance().AddLog("UIEditor: テクスチャ一覧の読み込みに失敗 %s\n", path.c_str());
	}
}

void UIEditor::SaveTextureRegistry(const std::string& path)
{
	nlohmann::json j = m_textureFileList;

	if (!JsonLoader::Save(path, j))
	{
		KdDebugGUI::Instance().AddLog("UIEditor: テクスチャ一覧の保存に失敗 %s\n", path.c_str());
	}
}

bool UIEditor::AddRegisteredTexture(const std::string& relPath)
{
	if (std::find(m_textureFileList.begin(), m_textureFileList.end(), relPath) != m_textureFileList.end())
	{
		return false;
	}

	m_textureFileList.push_back(relPath);
	return true;
}

// Windowsのファイル選択ダイアログで画像を1つ登録する
void UIEditor::AddTextureViaFileDialog()
{
	namespace fs = std::filesystem;

	fs::path savedCurrentDir = fs::current_path();

	// 日本語パスを正しく受け取るためワイド版を使う
	wchar_t fileBuf[MAX_PATH] = {};

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = Application::Instance().GetWindowHandle();
	ofn.lpstrFilter = L"Image Files (*.png;*.jpg;*.jpeg;*.dds;*.tga)\0*.png;*.jpg;*.jpeg;*.dds;*.tga\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = fileBuf;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	const bool result = GetOpenFileNameW(&ofn);

	fs::current_path(savedCurrentDir);

	if (!result) return;

	// kTextureRootからの相対パスにする(取れない場合は渡されたパスのまま)
	std::string relativePath;
	try
	{
		const fs::path rel = fs::relative(fs::absolute(fs::path(fileBuf)), fs::absolute(fs::path(kTextureRoot)));
		relativePath = WideToUtf8((rel.empty() ? fs::path(fileBuf) : rel).generic_wstring());
	}
	catch (...)
	{
		relativePath = WideToUtf8(fileBuf);
	}

	if (AddRegisteredTexture(relativePath)) { SaveTextureRegistry(m_registryPathBuf); }

	for (int i = 0; i < (int)m_textureFileList.size(); ++i)
	{
		if (m_textureFileList[i] == relativePath) { m_selectedAsset = i; break; }
	}
}

// kTextureRoot以下の画像をまとめて登録する
void UIEditor::ScanTextureFolder()
{
	namespace fs = std::filesystem;

	if (!fs::exists(kTextureRoot))
	{
		KdDebugGUI::Instance().AddLog("UIEditor: フォルダがありません %s\n", kTextureRoot.c_str());
		return;
	}

	static const std::vector<std::wstring> kExtensions = { L".png", L".dds", L".jpg", L".jpeg", L".tga" };

	int added = 0;
	for (auto& entry : fs::recursive_directory_iterator(kTextureRoot))
	{
		if (!entry.is_regular_file()) continue;

		std::wstring ext = entry.path().extension().wstring();
		for (auto& c : ext) c = (wchar_t)std::towlower(c);

		if (std::find(kExtensions.begin(), kExtensions.end(), ext) == kExtensions.end()) continue;

		if (AddRegisteredTexture(WideToUtf8(fs::relative(entry.path(), kTextureRoot).generic_wstring()))) { ++added; }
	}

	if (added > 0)
	{
		std::sort(m_textureFileList.begin(), m_textureFileList.end());
		m_selectedAsset = -1;
		SaveTextureRegistry(m_registryPathBuf);
	}

	KdDebugGUI::Instance().AddLog("UIEditor: %d件のテクスチャを登録しました\n", added);
}

// 一覧から除外するだけで、ファイル自体は削除しない
void UIEditor::RemoveRegisteredTexture(int index)
{
	if (index < 0 || index >= (int)m_textureFileList.size()) return;

	m_textureFileList.erase(m_textureFileList.begin() + index);
	m_selectedAsset = -1;

	SaveTextureRegistry(m_registryPathBuf);
}

void UIEditor::AddImageFromTexture(const std::string& fullPath, const DirectX::SimpleMath::Vector2& center)
{
	AddElement("Image");

	UIElement* e = FindSelected();
	if (!e) return;

	if (ComponentEntry* image = e->Find("UIImage")) { image->params["texture"] = fullPath; }

	UILayout l;
	if (!ReadUILayout(*e, l)) return;

	if (const std::shared_ptr<KdTexture> tex = GetTexture(fullPath))
	{
		l.size = DirectX::SimpleMath::Vector2((float)tex->GetWidth(), (float)tex->GetHeight());
	}
	WriteUILayout(*e, l);

	// ドロップ位置が矩形の中心になるように配置する
	UIRect rect;
	rect.size = DirectX::SimpleMath::Vector2(l.size.x * l.scale.x, l.size.y * l.scale.y);
	rect.min = center - rect.size * 0.5f;
	ApplyUIRect(*e, m_refSize, rect, false);
}
