#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>

#include "MaaAgentServer/MaaAgentServerAPI.h"
#include "MaaFramework/MaaAPI.h"
#include "MaaFramework/Utility/MaaBuffer.h"

#include "route_recognition.h"

namespace fs = std::filesystem;

static RouteRecognition g_recognition;

MaaBool RouteRecognitionCallback(
    MaaContext* context,
    MaaTaskId task_id,
    const char* node_name,
    const char* custom_recognition_name,
    const char* custom_recognition_param,
    const MaaImageBuffer* image,
    const MaaRect* roi,
    void* trans_arg,
    /* out */ MaaRect* out_box,
    /* out */ MaaStringBuffer* out_detail)
{
    if (std::string(custom_recognition_name) != "route_recognition") {
        return true;
    }

    const void* data_ptr = MaaImageBufferGetRawData(image);
    int width = MaaImageBufferWidth(image);
    int height = MaaImageBufferHeight(image);
    int channels = MaaImageBufferChannels(image);

    int cv_type;
    switch (channels) {
        case 3: cv_type = CV_8UC3; break;
        case 4: cv_type = CV_8UC4; break;
        default: cv_type = CV_8UC1; channels = 1; break;
    }

    cv::Mat mat(height, width, cv_type, const_cast<void*>(data_ptr), static_cast<size_t>(width * channels));
    mat = mat.clone();

    std::string pi_resource_str;
    if (custom_recognition_param && std::string(custom_recognition_param).size() > 0) {
        pi_resource_str = std::string(custom_recognition_param);
    }

    std::string base_path = "./resource";
    if (!pi_resource_str.empty()) {
        auto path_pos = pi_resource_str.find("\"path\"");
        if (path_pos != std::string::npos) {
            auto colon_pos = pi_resource_str.find(":", path_pos);
            auto quote1 = pi_resource_str.find("\"", colon_pos);
            auto quote2 = pi_resource_str.find("\"", quote1 + 1);
            if (quote1 != std::string::npos && quote2 != std::string::npos) {
                base_path = pi_resource_str.substr(quote1 + 1, quote2 - quote1 - 1);
            }
        }
    }

    std::cerr << "[DEBUG] Working directory: " << fs::current_path() << std::endl;
    std::cerr << "[DEBUG] base_path: " << fs::path(base_path) << std::endl;
    std::cerr << "[DEBUG] pi_resource_str: " << pi_resource_str << std::endl;
    
    if (!g_recognition.load_templates(fs::path(base_path))) {
        std::string detail = R"({"详情":"模板加载失败"})";
        MaaStringBufferSetEx(out_detail, detail.c_str(), detail.size());
        return true;
    }

    std::vector<RouteInfo> routes = g_recognition.analyze(mat);

    std::ostringstream oss;
    oss << R"({"详情":""") << (routes.empty() ? "路线不佳" : "路线优秀") << R"(", "routes": [)";

    for (size_t i = 0; i < routes.size(); i++) {
        if (i > 0) oss << ", ";
        oss << R"({ "path": [)";
        for (size_t j = 0; j < routes[i].node_path.size(); j++) {
            if (j > 0) oss << ", ";
            oss << "\"" << routes[i].node_path[j] << "\"";
        }
        oss << R"(], "types": [)";
        for (size_t j = 0; j < routes[i].type_chain.size(); j++) {
            if (j > 0) oss << ", ";
            oss << "\"" << routes[i].type_chain[j] << "\"";
        }
        oss << R"(], "chars)" << routes[i].chars_count
            << ", " << "\"events\"" << routes[i].events_count
            << ", " << "\"normals\"" << routes[i].normals_count
            << "}";
    }

    oss << R"(]})" << std::endl;

    MaaStringBufferSetEx(out_detail, oss.str().c_str(), oss.str().size());

    if (!routes.empty()) {
        MaaRect box = {0, 0, 0, 0};
        *out_box = box;
    }

    return true;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <socket_id>" << std::endl;
        return 1;
    }

    std::string log_dir = "./debug";
    MaaGlobalSetOption(MaaGlobalOption_LogDir, static_cast<void*>(log_dir.data()), log_dir.size());

    MaaAgentServerRegisterCustomRecognition("route_recognition", RouteRecognitionCallback, nullptr);

    const char* identifier = argv[1];
    MaaAgentServerStartUp(identifier);

    MaaAgentServerJoin();

    MaaAgentServerShutDown();

    return 0;
}
