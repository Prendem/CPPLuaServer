#include "ConfigManager.hpp"
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <iostream>
#include <regex>

const ConfigManager& ConfigManager::instance()
{
    static ConfigManager s_instance;
    return s_instance;
}

const std::array<FileType, 19>& ConfigManager::getSupportedFileTypes()
{
    return s_supportedFileTypes;
}

const std::optional<std::reference_wrapper<const FileType>> ConfigManager::getFileType(const std::filesystem::path& filePath)
{
    std::string fileExtension{ filePath.extension().string() };
    if (fileExtension.empty())
        return std::nullopt;

    int left{ 0 };
    int right{ static_cast<int>(s_supportedFileTypes.size()) - 1 };
    while (left <= right)
    {
        int mid{ left + (right - left) / 2 };
        if (fileExtension < s_supportedFileTypes[mid].extension)
        {
            right = mid - 1;
            continue;
        }

        if (fileExtension > s_supportedFileTypes[mid].extension)
        {
            left = mid + 1;
            continue;
        }

        return std::cref(s_supportedFileTypes[mid]);
    }

    return std::nullopt;
}

const std::unordered_map<std::string, Endpoint, TransparentHash, TransparentComparison>& ConfigManager::getGetMap() const
{
    return m_getMap;
}

const std::unordered_map<std::string, Endpoint, TransparentHash, TransparentComparison>& ConfigManager::getPostMap() const
{
    return m_postMap;
}

const uint32_t ConfigManager::getRequestBufferSize() const
{
    return m_maxRequestBufferSize;
}

uint16_t ConfigManager::getPort() const
{
    return m_port;
}

const std::optional<std::filesystem::path>& ConfigManager::getRootDirectory() const
{
    return m_rootDirectory;
}

void ConfigManager::truncateInlineComments(std::string& line, char commentCharacter)
{
    size_t position{ line.find(commentCharacter) };

    while (position != std::string::npos)
    {
        //check if comment character is enclosed within double quotes, if so it is ignored
        int quotationCount{ 0 };

        for (size_t character{ 0 }; character < position; ++character)
        {
            if (line[character] == '"')
                ++quotationCount;
        }

        if (!(quotationCount % 2))
        {
            line.erase(position);
            break;
        }

        position = line.find(commentCharacter, position + 1);
    }
}

//checks if the ini line contains a given key, if so returns the value, if not returns nullopt
std::optional<std::string> ConfigManager::iniCheckForKey(std::string_view line, std::string_view key)
{
    if (line.substr(0, key.length()) == key)
    {
        size_t valueStart{ key.length() + 1 };
        if (valueStart > line.length())
            return std::nullopt;

        //both "key = value" and "key=value" are syntactically valid for ini, so we need to account for that
        while (line[valueStart] == ' ' || line[valueStart] == '=')
        {
            if (++valueStart >= line.length())
                return std::nullopt;
        }

        std::string value{ line.substr(valueStart) };
        truncateInlineComments(value, '#');
        truncateInlineComments(value, ';');
        return value;
    }

    return std::nullopt;
}

void ConfigManager::populateSettings()
{
    std::ifstream infile("config.ini");

    if (!infile.is_open())
    {
        std::cerr << "config.ini not found in executable directory, config settings will retain default values\n";
        return;
    }

    std::string currentLine;
    std::optional<std::string> setting;

    while (std::getline(infile, currentLine))
    {
        //skip comment lines
        if (currentLine[0] == ';' || currentLine[0] == '#')
            continue;

        setting = iniCheckForKey(currentLine, "RootDirectory");
        if (setting)
        {
            if (std::filesystem::exists(setting.value()) && std::filesystem::is_directory(setting.value()))
                m_rootDirectory = setting.value();
            else
                std::cerr << "Path " << setting.value() << " does not exist or is not a directory, root directory will remain unset.\n";
            continue;
        }

        setting = iniCheckForKey(currentLine, "Port");
        if (setting)
        {
            unsigned long portNum;

            try
            {
                portNum = std::stoul(setting.value());
            }

            catch (const std::invalid_argument)
            {
                std::cerr << "Non numerical value supplied for port, using default 4221\n";
                continue;
            }

            catch (const std::out_of_range)
            {
                std::cerr << "Supplied port number is out of range, using default 4221\n";
                continue;
            }

            if (portNum > std::numeric_limits<uint16_t>::max())
            {
                std::cerr << "Supplied port number is out of range, using default 4221\n";
                continue;
            }

            m_port = static_cast<uint16_t>(portNum);
            continue;
        }

        setting = iniCheckForKey(currentLine, "MaxRequestBufferSize");
        if (setting)
        {
            unsigned long maxBufferNum;

            try
            {
                maxBufferNum = std::stoul(setting.value());
            }

            catch (const std::invalid_argument)
            {
                std::cerr << "Non numerical value supplied for max buffer size, using default 8192\n";
                continue;
            }

            catch (const std::out_of_range)
            {
                std::cerr << "Supplied max buffer size is out of range, using default 8192\n";
                continue;
            }

            if (maxBufferNum > std::numeric_limits<uint32_t>::max())
            {
                std::cerr << "Supplied max buffer size is out of range, using default 8192\n";
                continue;
            }

            m_maxRequestBufferSize = static_cast<uint32_t>(maxBufferNum);
            continue;
        }
    }

    std::cout << "Rootdirectory is " << ((m_rootDirectory) ? (" set to " + m_rootDirectory->string()) : "not set, file endpoints will not be loaded.") << "\n";
    std::cout << "Port is set to " << m_port << "\n";
    std::cout << "Max buffer size is set to " << m_maxRequestBufferSize << "\n";

    infile.close();
}

bool ConfigManager::isSupported(const std::filesystem::path& filepath)
{
    std::string fileExtension{ filepath.extension().string() };
    if (fileExtension.empty())
        return false;

    int left{ 0 };
    int right{ static_cast<int>(s_supportedFileTypes.size()) - 1 };
    while (left <= right)
    {
        int mid{ left + (right - left) / 2 };
        if (fileExtension < s_supportedFileTypes[mid].extension)
        {
            right = mid - 1;
            continue;
        }

        if (fileExtension > s_supportedFileTypes[mid].extension)
        {
            left = mid + 1;
            continue;
        }

        return true;
    }

    return false;
}

//changes "\" characters in the relative file path to "/" so windows paths are using the right seperator in their endpoint names
void ConfigManager::switchPathSeperator(std::string& endpointName)
{
    size_t index{ endpointName.find_first_of('\\') };
    while (index != std::string::npos)
    {
        endpointName[index] = '/';
        index = endpointName.find_first_of('\\', index + 1);
    }
}

void ConfigManager::populateFileEndpoints(const std::filesystem::path& path)
{
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_directory())
        {
            populateFileEndpoints(entry.path());
            continue;
        }

        if (entry.path().string().erase(0, m_rootDirectory->string().length()) == "\\index.html" || entry.path().string().erase(0, m_rootDirectory->string().length()) == "/index.html")
        {
            m_getMap.emplace("/", Endpoint{ EndpointType::file, entry.path() });
        }

        if (isSupported(entry.path()))
        {
            std::string endpointName{ entry.path().string().erase(0, m_rootDirectory->string().length()) };
            switchPathSeperator(endpointName);
            m_getMap.emplace(endpointName, Endpoint{ EndpointType::file, entry.path() });
        }
    }
}

void ConfigManager::yamlReplaceEscapeCharacter(std::string& inputString)
{
    //Major violation of DRY, consider implementing a find and replace function
    //backslash
    size_t position{ inputString.find("\\\\") };
    while (position != std::string::npos)
    {
        inputString.replace(position, 2, "\\");
        position = inputString.find("\\\\");
    }

    //double quote
    position = inputString.find("\\\"");
    while (position != std::string::npos)
    {
        inputString.replace(position, 2, "\"");
        position = inputString.find("\\\"");
    }

    //single quote
    position = inputString.find("\\\'");
    while (position != std::string::npos)
    {
        inputString.replace(position, 2, "\'");
        position = inputString.find("\\\'");
    }

    //newline
    position = inputString.find("\\\n");
    while (position != std::string::npos)
    {
        inputString.replace(position, 2, "\n");
        position = inputString.find("\\\n");
    }

    //tab
    position = inputString.find("\\\t");
    while (position != std::string::npos)
    {
        inputString.replace(position, 2, "\t");
        position = inputString.find("\\\t");
    }

    //hash
    position = inputString.find("\\#");
    while (position != std::string::npos)
    {
        inputString.replace(position, 2, "#");
        position = inputString.find("\\#");
    }

    //carriage return
    position = inputString.find("\\\r");
    while (position != std::string::npos)
    {
        inputString.replace(position, 2, "\r");
        position = inputString.find("\\\r");
    }
}

void ConfigManager::populateScriptEndpoints()
{
    std::cout << "Populating scripts\n";
    std::ifstream inFile("endpoints.yaml");

    if (!inFile.is_open())
    {
        std::cout << "Unable to open endpoints.yaml, script endpoints will not be populated\n";
    }

    std::regex keyMatch("^(?!\\s)[a-zA-Z\\/.\\-_\\?\\=\\&\\%]+:");
    std::regex typeMatch(" +type: \\w+");
    std::regex pathMatch(" +path: .*");
    std::string currentLine, nextLine, thirdLine, key, type, path;

    //until eof is reached
    while (std::getline(inFile, currentLine))
    {
        //if yaml key is found condition is met
        if (std::regex_match(currentLine, keyMatch))
        {
            //check the next line (assuming it's not eof)
            if(std::getline(inFile, nextLine))
            {
                //next line should be the type subkey
                if (!std::regex_match(nextLine, typeMatch))
                    throw std::runtime_error("Invalid endpoint.yaml file, no type subkey found for " + currentLine.substr(0, currentLine.length() - 1));

                if (std::getline(inFile, thirdLine))
                {
                    //next line should be the path subkey
                    if (!std::regex_match(thirdLine, pathMatch))
                        throw std::runtime_error("Invalid endpoint.yaml file, no path subkey found for " + currentLine.substr(0, currentLine.length() - 1));

                    size_t colonIndex{ thirdLine.find(':') };
                    path = thirdLine.substr(colonIndex + 2);
                    truncateInlineComments(path, '#');
                    yamlReplaceEscapeCharacter(path);
                    //consider remove enclosing quotes by checking path.front() and path.back() and taking substring, would allow more flexibility in the yaml file
                    if (!std::filesystem::exists(path))
                    {
                        std::cerr << "path " << path << " specified in endpoint.yaml file does not exist, this endpoint will be skipped.\n";
                        continue;
                    }
                }

                else
                    throw std::runtime_error("Invalid endpoint.yaml file, no path subkey found for " + currentLine.substr(0, currentLine.length() - 1));

                size_t colonIndex{ nextLine.find(':') };
                type = nextLine.substr(colonIndex + 2);
                truncateInlineComments(type, '#');
            }

            else
                throw std::runtime_error("Invalid endpoint.yaml file, no type subkey found for " + currentLine);

            key = currentLine.substr(0, currentLine.length() - 1);

            if (key[0] != '/')
                key.insert(key.begin(), '/');

            if (m_getMap.contains(key))
                continue;

            if (type == "GET")
                m_getMap.emplace(key, Endpoint{ EndpointType::script, path });
            else if (type == "POST")
                m_postMap.emplace(key, Endpoint{ EndpointType::script, path });
        }
    }
}

ConfigManager::ConfigManager()
{
    populateSettings();
    if (m_rootDirectory)
        populateFileEndpoints(m_rootDirectory.value());

    try
    {
        populateScriptEndpoints();
    }

    catch(const std::exception& e)
    {
        std::cerr << e.what() << " not all script endpoints have been loaded\n";
    }
}