#include "ConnectionManager.hpp"
#include "ConfigManager.hpp"
#include "PlatformSockets.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sol/sol.hpp>
#pragma comment(lib, "lua.lib")

ConnectionManager::ConnectionManager(SocketType client, std::shared_ptr<BufferPool> bufferPool) : m_clientSocket{ client }, m_bufferPool{ bufferPool },
	s_configManager{ConfigManager::instance()}
{
	m_requestBuffer = m_bufferPool->obtainPointer();
}

ConnectionManager::~ConnectionManager()
{
	m_bufferPool->returnPointer(m_requestBuffer);
}

void ConnectionManager::processConnection()
{
	if (!m_requestBuffer) //obtainBuffer() threw bad_alloc
		return;

	while (true)
	{
		int length = recv(m_clientSocket, m_requestBuffer.get(), s_configManager.getRequestBufferSize(), 0);

		if (length < 1)
		{
			if (length == 0)
			{
				std::cerr << "connection closed on client end\n";
				break;
			}

			if (checkForTimeout())
			{
				std::cout << "connection timed out\n";
				break;
			}

			std::cerr << "Socket error: " << getLastError() << "\n";
			break;
		}

		m_bufferView = std::string_view(m_requestBuffer.get(), length);
		extractHeaders();

		switch (extractRequestMethod())
		{
		case RequestMethod::get:
			handleGetRequest(extractUrl());
			break;
		case RequestMethod::post:
			handlePostRequest(extractUrl());
			break;
		default:
			{
				std::cout << "method is unsupported\n";
				std::string message{ "HTTP / 1.1 501 Not Implemented\r\n\r\n" };
				send(m_clientSocket, message.c_str(), (int)message.length(), 0);
				break;
			}
		}
	}

	closeClientSocket(m_clientSocket);
}

RequestMethod ConnectionManager::extractRequestMethod()
{
	size_t firstSpace{ m_bufferView.find(' ')};


	if (firstSpace == std::string_view::npos)
	{
		std::cerr << "Malformed http request, unable to extract request method\n";
		return RequestMethod::unsupported;
	}

	std::string_view requestMethod{ m_bufferView.substr(0, firstSpace) };

	if (requestMethod == "GET")
		return RequestMethod::get;
	if (requestMethod == "POST")
		return RequestMethod::post;

	return RequestMethod::unsupported;
}

std::string_view ConnectionManager::extractUrl()
{
	size_t firstSpace{ m_bufferView.find(' ') };
	size_t secondSpace{ m_bufferView.find(' ', firstSpace + 1) };
	size_t length{ secondSpace - firstSpace - 1 };
	return m_bufferView.substr(firstSpace + 1, length);
}

void ConnectionManager::extractHeaders()
{
	size_t headerStart{ m_bufferView.find("\r\n") };
	if (headerStart == std::string_view::npos)
		m_headersView = std::nullopt;

	size_t headerEnd{ m_bufferView.find("\r\n\r\n") };
	if (headerEnd == std::string_view::npos)
		m_headersView = std::nullopt;

	size_t length = headerEnd - headerStart;
	if (!length)
		m_headersView = std::nullopt;

	m_headersView = m_bufferView.substr(headerStart + 2, length);
}

std::optional<std::string_view> ConnectionManager::getHeaderValue(std::string_view header)
{
	if (!m_headersView)
		return std::nullopt;

	size_t headerStart{ m_headersView->find(header)};

	if (headerStart == std::string_view::npos)
		return std::nullopt;

	size_t headerEnd{ m_headersView->find("\r\n", headerStart) };
	size_t length{ headerEnd - (headerStart + header.size()) };
	return m_headersView->substr(headerStart + header.size(), length);
}

void ConnectionManager::handleGetRequest(std::string_view url)
{
	auto it{ s_configManager.getGetMap().find(url) };

	if (it == s_configManager.getGetMap().end())
	{
		std::string message{ "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n" };
		send(m_clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
		return;
	}

	try
	{
		if (it->second.type == EndpointType::file)
			handleGetFile(it->second.path);
		else if (it->second.type == EndpointType::script)
			handleScript(it->second.path);
	}

	catch (const sol::error& e)
	{
		std::cerr << "Lua error: " << e.what() << "\n";
		std::string message{ "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n" };
		send(m_clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
	}

	catch (const std::exception& e)
	{
		std::cerr << "Lua error: " << e.what() << "\n";
		std::string message{ "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n" };
		send(m_clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
	}
}

void ConnectionManager::handleGetFile(const std::filesystem::path& path)
{
	const std::array<FileType, 19>& fileTypes{ s_configManager.getSupportedFileTypes() };
	auto it{ std::find_if(fileTypes.begin(), fileTypes.end(),
		[&path](const FileType& ft)
		{
			return ft.extension == path.extension().string();
		}) };


	
	if (it->fileTransferMode == FileTransferMode::binary)
		sendBinaryFile(path, *it);
	else if (it->fileTransferMode == FileTransferMode::text)
		sendTextFile(path, *it);
}

void ConnectionManager::handleScript(const std::filesystem::path& path)
{
	sol::state luaState;
	luaState.open_libraries(sol::lib::base, sol::lib::package, sol::lib::coroutine,sol::lib::string, sol::lib::table, sol::lib::math,sol::lib::io, sol::lib::os, sol::lib::debug);
	luaState.script_file(path.string());
	sol::function execute{ luaState["execute"].get<sol::function>() };
	sol::object result{ execute(m_bufferView) };

	//for text based responses
	if (result.is<std::string>())
	{
		std::string response{ result.as<std::string>() };
		size_t bytesTransmitted{ 0 };
		while (bytesTransmitted < response.size())
		{
			int sent{ send(m_clientSocket, response.c_str(), static_cast<int>(response.size() - bytesTransmitted), 0) };
			if (sent < 1)
				throw std::runtime_error("Socket send failed or connection closed");

			bytesTransmitted += sent;
		}

		return;
	}

	//for binary responses
	if (result.is<sol::table>())
	{
		std::vector<uint8_t> response;
		sol::table table{ result };
		for (auto kvp : table)
		{
			response.push_back(kvp.second.as<uint8_t>());
		}

		size_t bytesTransmitted{ 0 };
		while (bytesTransmitted < response.size())
		{
			int sent{ send(m_clientSocket, reinterpret_cast<char*>(response.data()) + bytesTransmitted, static_cast<int>((response.size() - bytesTransmitted)), 0) };
			if (sent < 1)
				throw std::runtime_error("Socket send failed or connection closed");

			bytesTransmitted += sent;
		}
	}
}

void ConnectionManager::sendBinaryFile(const std::filesystem::path& path, const FileType& fileType)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
		throw std::runtime_error("Unable to open file " + path.string());

	std::streampos pos{ file.tellg() };
	if (pos == std::streampos(-1))
		throw std::runtime_error("tellg() call failed for " + path.string());

	size_t contentLength{ static_cast<size_t>(pos) };
	file.seekg(0);
	
	std::string headerSection;

	headerSection.reserve(m_maxHeaderSize);
	headerSection.append("HTTP/1.1 200 OK\r\nContent-Length: ");
	headerSection.append(std::to_string(contentLength));
	headerSection.append("\r\nContent-Type: ");
	headerSection.append(fileType.mimeType);
	headerSection.append("\r\nConnection: keep-alive");
	headerSection.append("\r\n\r\n");

	std::vector<uint8_t> byteArray(headerSection.size() + contentLength);
	std::memcpy(byteArray.data(), headerSection.data(), headerSection.size());
	file.read(reinterpret_cast<char*>(byteArray.data() + headerSection.size()), contentLength);

	if (file.fail())
		throw std::runtime_error("File read failed for file " + path.string());

	size_t bytesTransmitted{ 0 };
	while (bytesTransmitted < byteArray.size())
	{
		int sent{ send(m_clientSocket, reinterpret_cast<char*>(byteArray.data()) + bytesTransmitted, static_cast<int>((byteArray.size() - bytesTransmitted)), 0) };
		if (sent < 1)
			throw std::runtime_error("Socket send failed or connection closed");

		bytesTransmitted += sent;
	}
	
	file.close();
}

void ConnectionManager::sendTextFile(const std::filesystem::path& path, const FileType& fileType)
{
	std::ifstream file(path, std::iostream::ate);
	if (!file)
		throw std::runtime_error("Unable to open file " + path.string());

	std::streampos pos{ file.tellg() };
	if (pos == std::streampos(-1))
		throw std::runtime_error("tellg() call failed for " + path.string());

	size_t contentLength{ static_cast<size_t>(pos) };
	file.seekg(0);

	std::string message;
	
	message.reserve(m_maxHeaderSize + contentLength);
	message.append("HTTP/1.1 200 OK\r\nContent-Length: ");
	message.append(std::to_string(contentLength));
	message.append("\r\nContent-Type: ");
	message.append(fileType.mimeType);
	message.append("\r\nConnection: keep-alive");
	message.append("\r\n\r\n");
	
	size_t headerSize{ message.size() };
	message.resize(headerSize + contentLength);

	file.read(message.data() + headerSize, contentLength);

	size_t bytesTransmitted{ 0 };
	while (bytesTransmitted < message.size())
	{
		int sent{ send(m_clientSocket, message.c_str(), static_cast<int>(message.size() - bytesTransmitted), 0) };
		if(sent < 1)
			throw std::runtime_error("Socket send failed or connection closed");

		bytesTransmitted += sent;
	}

	file.close();
}

void ConnectionManager::handlePostRequest(std::string_view url)
{
	auto it{ s_configManager.getPostMap().find(url) };

	if (it == s_configManager.getPostMap().end())
	{
		std::string message{ "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n" };
		send(m_clientSocket, message.c_str(), (int)message.length(), 0);
		return;
	}

	try
	{
		handleScript(it->second.path);
	}

	catch (const sol::error& e)
	{
		std::cerr << "Lua error: " << e.what() << "\n";
		std::string message{ "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n" };
		send(m_clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
	}

	catch (const std::exception& e)
	{
		std::cerr << "Lua error: " << e.what() << "\n";
		std::string message{ "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n" };
		send(m_clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
	}
}