#pragma once

// ※ ImGui / DirectXTK(SimpleMath) は既存のPCH等で読み込まれている前提です。
#include <memory>
#include <unordered_map>
#include "Application/Definitions/UI/UIData.h"
#include "../Common/ComponentInspector.h"

class KdFontSprite;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// UIエディタ本体(HUD・メニュー共通。画面ごとに1つのJSONを編集する)
//	KdDebugGUI::GuiProcess() の中から Update() を呼び出して使用する。
//	UI要素はMapEditorと同じ components 方式(UITransform / Text / UIImage / Selectable など)で、
//	座標・文字・画像はゲーム側のコンポーネントと同じ規則で解決・描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class UIEditor
{
public:

	// 毎フレームの更新・描画(ImGuiウィンドウ一式)
	void Update();

	// 基準解像度のオフスクリーンへ、実際のKdSpriteShaderでUIを描画する(UI Canvasに表示される)。
	//	3D描画パス側から1フレームに1回、KdSpriteShaderのBegin～End外で呼ぶこと。
	//	呼び出し前後でRT/ビューポートは退避・復元する
	void RenderPreviewViewport();

	// 編集中の要素一覧(読み取り専用)
	const std::vector<UIElement>& GetElements() const { return m_elements; }

private:

	void DrawMainMenu();
	void DrawHierarchy();
	void DrawHierarchyNode(UIId id);
	void DrawInspector();
	void DrawCanvas();
	void DrawAssetPicker();

	// m_elements内をIdで探す(見つからなければnullptr)
	UIElement* FindElement(UIId id);
	const UIElement* FindElement(UIId id) const;
	UIElement* FindSelected() { return FindElement(m_selectedId); }

	// idがancestorIdの子孫かどうか
	bool IsDescendant(UIId id, UIId ancestorId) const;

	// 親を辿って表示される要素だけを、描画順(親→子、兄弟は配列順)で集める
	void CollectVisibleOrder(UIId parentId, std::vector<UIId>& out, int depth = 0) const;

	//=====================================================
	// テキスト・テクスチャ(TextComponent / UIImageと同じ手順でプレビューに使う)
	//=====================================================
	struct TextBlock
	{
		std::vector<std::shared_ptr<KdFontSprite>>	lines;
		float										lineHeight = 0.0f;
	};

	// 文字列(UTF-8)から、行ごとのフォントスプライトを作る。同じ内容はキャッシュする
	const TextBlock* GetTextBlock(int fontNo, int antiAliasing, const std::string& textUtf8);

	// プレビュー用テクスチャ取得。読み込めたものだけキャッシュする(失敗時はnullptr)
	std::shared_ptr<KdTexture> GetTexture(const std::string& path);

	// UITransformのsizeが0の要素(文字・画像)が自分で持つ大きさ(拡大率適用後)
	DirectX::SimpleMath::Vector2 GetIntrinsicSize(const UIElement& e);

	// 要素の矩形(エディタ座標)。UITransformが無い・Worldの要素や、大きさ0の要素はfalse
	bool GetElementRect(const UIElement& e, UIRect& out);

	// テクスチャ登録一覧(MapEditorのアセット登録と同じ方式。JSONに保存される)
	void LoadTextureRegistry(const std::string& path);
	void SaveTextureRegistry(const std::string& path);
	bool AddRegisteredTexture(const std::string& relPath);	// 登録済みならfalse
	void AddTextureViaFileDialog();
	void ScanTextureFolder();
	void RemoveRegisteredTexture(int index);

	// 指定テクスチャのImage要素を、基準解像度上のcenterを中心に新規追加する(サイズはテクスチャに合わせる)
	void AddImageFromTexture(const std::string& fullPath, const DirectX::SimpleMath::Vector2& center);

	// presetは"Image" / "Text" / "Button"。それぞれのコンポーネント構成で要素を追加する
	void AddElement(const std::string& preset);
	void RemoveSelected();
	void Reparent(UIId id, UIId newParentId);
	void MoveSibling(UIId id, int dir);	// dir: -1=前へ(奥へ) / +1=後ろへ(手前へ)

	//=====================================================
	// Undo / Redo (スナップショット方式。MapEditorと同じ)
	//=====================================================
	struct UndoState
	{
		std::vector<UIElement>	elements;
		UIId					selectedId = kInvalidUIId;
	};

	static constexpr size_t kMaxUndoDepth = 50;

	std::vector<UndoState>	m_undoStack;
	std::vector<UndoState>	m_redoStack;

	void PushUndo();
	void Undo();
	void Redo();

	void Save(const std::string& path);
	bool Load(const std::string& path);
	void CheckHotReload();

	std::vector<UIElement>			m_elements;
	UIId							m_selectedId = kInvalidUIId;
	UIId							m_nextId = 1;
	DirectX::SimpleMath::Vector2	m_refSize = { 1920.0f, 1080.0f };

	char	m_filePathBuf[260] = "Asset/Data/UI/UIData.json";

	// UIエディタのいずれかのウィンドウにフォーカスがあるか(他エディタとのショートカット二重発火を防ぐ)
	bool	m_hasFocus = false;

	// Hierarchyのドラッグ&ドロップによる親子付け替え(描画後にまとめて適用)
	bool	m_hasPendingReparent = false;
	UIId	m_pendingReparentId = kInvalidUIId;
	UIId	m_pendingParentId = kInvalidUIId;

	//=====================================================
	// プレビュー(実描画)
	//=====================================================
	KdTexture	m_previewTex;				// 基準解像度のオフスクリーンRT
	bool		m_previewRendered = false;	// RenderPreviewViewport()が1度でも描けたか

	std::unordered_map<std::string, std::shared_ptr<KdTexture>>	m_texCache;
	std::unordered_map<std::string, TextBlock>					m_textCache;

	// 登録済みテクスチャ一覧(Asset/Textures/UI/ からの相対パス)
	std::vector<std::string>	m_textureFileList;
	bool						m_assetListLoaded = false;
	int							m_selectedAsset = -1;
	char						m_registryPathBuf[260] = "Asset/Data/UI/TextureAssets.json";

	//=====================================================
	// キャンバス(ズーム・パン・矩形ドラッグ)
	//=====================================================
	enum class DragMode { None, Move, Resize };

	float							m_zoom = 0.5f;
	DirectX::SimpleMath::Vector2	m_canvasPan = { 0.0f, 0.0f };	// キャンバス左上からの画面オフセット
	bool							m_fitRequested = true;

	bool	m_useSnap = false;
	float	m_snapValue = 8.0f;

	DragMode						m_dragMode = DragMode::None;
	int								m_dragHx = 0;	// リサイズハンドルの方向(-1,0,1)
	int								m_dragHy = 0;
	bool							m_dragMoved = false;
	DirectX::SimpleMath::Vector2	m_dragStartMouse = { 0.0f, 0.0f };	// 基準解像度座標
	DirectX::SimpleMath::Vector2	m_dragStartMin = { 0.0f, 0.0f };
	DirectX::SimpleMath::Vector2	m_dragStartSize = { 0.0f, 0.0f };

	// ホットリロード
	bool		m_autoReload = true;
	float		m_reloadCheckTimer = 0.0f;
	FILETIME	m_lastWriteTime = {};

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	UIEditor();
	~UIEditor() {}

public:
	static UIEditor& Instance() {
		static UIEditor instance;
		return instance;
	}
};
