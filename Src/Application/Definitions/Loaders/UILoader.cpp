#include "UILoader.h"
#include "Application/Factories/PrefabFactory.h"
#include "../UI/UIData.h"

namespace
{
	// 親→子、兄弟は配列順に生成する(UIEditorのプレビューと同じ描画順)。親子は整理用なのでGameObjectの親子にはしない。
	void CreateElements(const std::vector<UIElement>& elements, UIId parentId, ObjectManager& objectManager)
	{
		for (const auto& element : elements)
		{
			if (element.parentId != parentId) continue;

			PrefabFactory::Create(objectManager, element);
			CreateElements(elements, element.id, objectManager);
		}
	}
}

bool UILoader::LoadUIFromJson(const std::string& jsonFilePath, ObjectManager& objectManager)
{
	UIFile uiFile;
	if (!LoadUIFile(jsonFilePath, uiFile))
	{
		std::cerr << "[UILoader Error] Failed to load UI JSON: " << jsonFilePath << std::endl;
		return false;
	}

	if (uiFile.elements.empty())
	{
		std::cerr << "[UILoader Warning] No elements (not a UIEditor file?): " << jsonFilePath << std::endl;
	}

	CreateElements(uiFile.elements, kInvalidUIId, objectManager);

	std::cout << "[UILoader] UI loaded: " << jsonFilePath
		<< " (elements: " << uiFile.elements.size() << ")" << std::endl;

	return true;
}
