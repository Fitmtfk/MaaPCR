#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <filesystem>

enum class NodeType {
    EMPTY,
    CHAR,
    EVENT,
    NORMAL
};

struct Node {
    NodeType type = NodeType::EMPTY;
    std::pair<int, int> pos = {0, 0};
};

struct RouteInfo {
    std::vector<std::string> node_path;
    std::vector<std::string> type_chain;
    int chars_count = 0;
    int events_count = 0;
    int normals_count = 0;
};

class GameGraph {
public:
    void add_node(int col, int row, NodeType type);
    void add_edge(int from_col, int from_row, int to_col, int to_row);
    NodeType get_type(const std::string& node_id) const;

    const std::map<std::string, NodeType>& nodes() const { return nodes_; }
    const std::map<std::string, std::vector<std::string>>& adjacency_list() const { return adjacency_list_; }

private:
    std::map<std::string, NodeType> nodes_;
    std::map<std::string, std::vector<std::string>> adjacency_list_;
};

class RouteRecognition {
public:
    RouteRecognition();

    bool load_templates(const std::filesystem::path& base_path);
    std::vector<RouteInfo> analyze(const cv::Mat& image);

private:
    void find_nodes(const cv::Mat& img, std::vector<std::vector<Node>>& grid_data,
                    const cv::Mat& template_img, NodeType type, float threshold = 0.75f);

    bool is_line_existing(const cv::Mat& img,
                          const std::pair<int, int>& pos_a,
                          const std::pair<int, int>& pos_b,
                          int row_curr, int row_next,
                          float threshold = 0.82f);

    std::vector<std::vector<std::string>> find_all_paths(const GameGraph& graph,
                                                         const std::string& current_node,
                                                         NodeType end_type,
                                                         const std::vector<std::string>& current_path);

    std::vector<RouteInfo> plan_routes(const GameGraph& graph);

    cv::Mat tpl_char_;
    cv::Mat tpl_event_;
    cv::Mat tpl_normal_;
    cv::Mat tpl_road_;
    cv::Mat tpl_road_bgr_;
    cv::Mat tpl_road_alpha_;

    static const std::vector<std::pair<int, int>> columns_x_;
    static const std::vector<std::pair<int, int>> rows_y_;
};
