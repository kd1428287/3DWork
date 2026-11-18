#include "Application/main.h"

#include "EditorUtil.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <commdlg.h>

namespace fs = std::filesystem;

namespace EditorUtil
{
	std::wstring Utf8ToWide(const std::string& utf8)
	{
		if (utf8.empty()) { return std::wstring(); }
		const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
		std::wstring wide(len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), wide.data(), len);
		return wide;
	}

	std::string WideToUtf8(const std::wstring& wide)
	{
		if (wide.empty()) { return std::string(); }
		const int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
		std::string utf8(len, '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), utf8.data(), len, nullptr, nullptr);
		return utf8;
	}

	std::string MakeUniqueName(const std::string& base, const std::function<bool(const std::string&)>& exists)
	{
		if (!exists(base)) { return base; }

		for (int i = 2;; ++i)
		{
			const std::string candidate = base + std::to_string(i);
			if (!exists(candidate)) { return candidate; }
		}
	}

	std::vector<std::string> ScanTextureFiles(const std::string& root)
	{
		std::vector<std::string> result;

		std::error_code ec;
		if (!fs::exists(root, ec)) { return result; }

		static const std::vector<std::wstring> kExtensions = { L".png", L".dds", L".jpg", L".jpeg", L".tga" };

		for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec))
		{
			if (ec) { break; }
			if (!it->is_regular_file()) { continue; }

			std::wstring ext = it->path().extension().wstring();
			for (auto& c : ext) { c = (wchar_t)std::towlower(c); }
			if (std::find(kExtensions.begin(), kExtensions.end(), ext) == kExtensions.end()) { continue; }

			result.push_back(WideToUtf8(fs::relative(it->path(), root).generic_wstring()));
		}

		std::sort(result.begin(), result.end());
		return result;
	}

	bool PickFileRelative(const wchar_t* filter, const std::string& rootDir, std::string& outRelative)
	{
		// OFN_NOCHANGEDIRでも変わる環境があるため、カレントを退避して戻す
		const fs::path savedCurrentDir = fs::current_path();

		wchar_t fileBuf[MAX_PATH] = {};

		OPENFILENAMEW ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = Application::Instance().GetWindowHandle();
		ofn.lpstrFilter = filter;
		ofn.lpstrFile = fileBuf;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

		const bool picked = GetOpenFileNameW(&ofn);

		fs::current_path(savedCurrentDir);

		if (!picked) { return false; }

		// rootDirからの相対パスにする(取れない場合は選択パスのまま)
		try
		{
			const fs::path rel = fs::relative(fs::absolute(fs::path(fileBuf)), fs::absolute(fs::path(Utf8ToWide(rootDir))));
			outRelative = WideToUtf8((rel.empty() ? fs::path(fileBuf) : rel).generic_wstring());
		}
		catch (...)
		{
			outRelative = WideToUtf8(fileBuf);
		}
		return true;
	}
}
