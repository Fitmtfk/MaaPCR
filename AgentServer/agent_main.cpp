#include "globals.h"

#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <nlohmann/json.hpp>

#include "MaaAgentServer/MaaAgentServerAPI.h"
#include "MaaFramework/MaaAPI.h"

#include "route_recognition.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

std::string GetEnvVar(const std::string &name)
{
    // std::getenv 内部管理内存，不需要 free()
    const char *env_val = std::getenv(name.c_str());

    if (env_val != nullptr)
    {
        NONE(name + ":" + env_val);
        return std::string(env_val);
    }
    else
    {
        NONE("环境变量 " + name + " 不存在");
        return "";
    }
}

std::vector<std::string> GetResourcePaths()
{
    std::vector<std::string> paths;

    try
    {
        std::string pi_resource = GetEnvVar("PI_RESOURCE");
        if (!pi_resource.empty())
        {
            json pi_json = json::parse(pi_resource);
            NONE("pi_json:" + pi_json.dump());
            if (!pi_json.empty() && pi_json.contains("path") && pi_json["path"].is_array())
            {
                for (const auto &item : pi_json["path"])
                {
                    if (item.is_string())
                    {
                        paths.push_back(item.get<std::string>());
                    }
                }
            }
        }
    }
    catch (const json::parse_error &e)
    {
        NONE(std::string("JSON 解析失败: ") + e.what());
    }

    if (paths.empty())
    {
        std::string fallback_path = GetEnvVar("VSCODE_MAAFW_AGENT_RESOURCE");
        if (!fallback_path.empty())
        {
            paths.push_back(fallback_path);
        }
        else
        {
            paths.push_back("./resource");
        }
    }

    return paths;
}

int main(int argc, char **argv)
{
    Logger::Instance().Init("agent.log");

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <socket_id>" << std::endl;
        return 1;
    }

    std::vector<std::string> resource_paths = GetResourcePaths();
    // std::cout << "Paths:" << std::endl;
    INFO("Paths:");
    for (const auto &path : resource_paths)
    {
        // std::cout << "  - " << path << std::endl;
        INFO("  - " + path);
    }
    resource_path = resource_paths[0];

    MaaAgentServerRegisterCustomRecognition("route_recognition", RouteRecognitionCallback, nullptr);

    const char *identifier = argv[1];
    MaaAgentServerStartUp(identifier);

    MaaAgentServerJoin();

    MaaAgentServerShutDown();

    return 0;
}
