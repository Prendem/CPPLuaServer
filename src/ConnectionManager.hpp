#pragma once
#include <string>
#include <memory>
#include <optional>
#include "BufferPool.hpp"
#include "ConfigManager.hpp"
#include "PlatformSockets.hpp"

enum class RequestMethod
{
	get,
	post,
	unsupported,
};

class ConnectionManager
{
public:
	ConnectionManager(SocketType client, std::shared_ptr<BufferPool> bufferPool);
	~ConnectionManager();
	void processConnection();

private:
	static constexpr int m_maxHeaderSize{ 256 }; //total length of headers and status line should not exceed this for file transfers, re-evaluate when adding support for new headers
	SocketType m_clientSocket;
	std::shared_ptr<BufferPool> m_bufferPool;
	const ConfigManager& s_configManager;
	std::shared_ptr<char[]> m_requestBuffer;
	std::optional<std::string_view> m_headersView;
	std::string_view m_bufferView;
	RequestMethod extractRequestMethod();
	std::string_view extractUrl();
	void extractHeaders();
	std::optional<std::string_view> getHeaderValue(std::string_view header);
	void handleGetRequest(std::string_view url);
	void handleGetFile(const std::filesystem::path& path);
	void handleScript(const std::filesystem::path& path);
	void sendBinaryFile(const std::filesystem::path& path, const FileType& fileType);
	void sendTextFile(const std::filesystem::path& path, const FileType& fileType);
	void handlePostRequest(std::string_view url);
};
