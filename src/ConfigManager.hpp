#pragma once
#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <filesystem>
#include <array>
#include <iostream>
#include <functional>

#include "CompileTimeQuickSort.hpp"


enum class EndpointType
{
	file,
	script,
};

struct Endpoint
{
	const EndpointType type;
	const std::filesystem::path path;
};

enum class FileTransferMode
{
	text,
	binary,
};

struct FileType
{
	std::string_view extension;
	std::string_view mimeType;
	FileTransferMode fileTransferMode;
	constexpr friend bool operator<(const FileType& fileType1, const FileType& fileType2) { return fileType1.extension < fileType2.extension; }
};

//The following two functors enable transparent lookup using std::string_view in our hash tables with std::string keys to avoid unnecessary string construction
struct TransparentHash
{
	using is_transparent = void; //enables heterogeneous lookup
	size_t operator()(std::string_view sv) const
	{
		return std::hash<std::string_view>{}(sv);
	}
};

struct TransparentComparison
{
	using is_transparent = void;
	bool operator()(std::string_view lhs, std::string_view rhs) const
	{
		return lhs == rhs;
	}
};

class ConfigManager
{
public:
	[[nodiscard]] static const ConfigManager& instance();
	[[nodiscard]] static const std::array<FileType, 19>& getSupportedFileTypes();
	[[nodiscard]] static const std::optional<std::reference_wrapper<const FileType>> getFileType(const std::filesystem::path& filePath);
	[[nodiscard]] const std::unordered_map<std::string, Endpoint, TransparentHash, TransparentComparison>& getGetMap() const;
	[[nodiscard]] const std::unordered_map<std::string, Endpoint, TransparentHash, TransparentComparison>& getPostMap() const;
	[[nodiscard]] const uint32_t getRequestBufferSize() const;

	[[nodiscard]] uint16_t getPort() const;
	[[nodiscard]] const std::optional<std::filesystem::path>& getRootDirectory() const;

	ConfigManager(const ConfigManager&) = delete;
	ConfigManager(const ConfigManager&&) = delete;
	ConfigManager& operator=(const ConfigManager&) = delete;

private:
	ConfigManager();
	void truncateInlineComments(std::string& line, char commentCharacter);
	std::optional<std::string> iniCheckForKey(std::string_view line, std::string_view key);
	void populateSettings();
	bool isSupported(const std::filesystem::path& filepath);
	void switchPathSeperator(std::string& endpoint);
	void populateFileEndpoints(const std::filesystem::path& path);
	void yamlReplaceEscapeCharacter(std::string& inputString);
	void populateScriptEndpoints();

	uint16_t m_port{ 4221 };
	std::optional<std::filesystem::path> m_rootDirectory{ std::nullopt };
	uint32_t m_maxRequestBufferSize{ 8192 };
	std::unordered_map<std::string, Endpoint, TransparentHash, TransparentComparison> m_getMap;
	std::unordered_map<std::string, Endpoint, TransparentHash, TransparentComparison> m_postMap;

	//sorted at compile time so binary search can be used for lookups
	constexpr static std::array<FileType, 19> s_supportedFileTypes = []() constexpr
	{
		std::array<FileType, 19> temp
		{
			{
				{".html", "text/html", FileTransferMode::text},
				{".css", "text/css", FileTransferMode::text},
				{".js", "application/javascript", FileTransferMode::binary},
				{".txt", "text/plain", FileTransferMode::text},
				{".json", "application/json", FileTransferMode::text},
				{".xml", "text/xml", FileTransferMode::text},
				{".svg", "image/svg+xml", FileTransferMode::binary},
				{".png", "image/png", FileTransferMode::binary},
				{".jpg", "image/jpeg", FileTransferMode::binary},
				{".gif", "image/gif", FileTransferMode::binary},
				{".webp", "image/webp", FileTransferMode::binary},
				{".ico", "image/vnd.microsoft.icon", FileTransferMode::binary},
				{".mp3", "audio/mpeg", FileTransferMode::binary},
				{".wav", "audio/wav", FileTransferMode::binary},
				{".mp4", "video/mp4", FileTransferMode::binary},
				{".webm", "video/webm", FileTransferMode::binary},
				{".pdf", "application/pdf", FileTransferMode::binary},
				{".zip", "application/zip", FileTransferMode::binary},
				{".gzip", "application/gzip", FileTransferMode::binary}
			}
		};
	
		quickSort(temp, 0, (int)temp.size() - 1);
		return temp;
	}();

};
