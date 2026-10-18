#pragma once

class ObjectManager;

class UILoader
{
public:
	// UIEditorが保存したJSONからUIを読み込み、オブジェクトを生成する。
	// ファイルが無い/JSONとして壊れている場合のみfalse(要素単位の破損はUIData側でスキップされる)。
	bool LoadUIFromJson(const std::string& jsonFilePath, ObjectManager& objectManager);
};
