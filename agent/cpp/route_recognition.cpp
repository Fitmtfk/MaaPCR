#include "route_recognition.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

const std::vector<std::pair<int, int>> RouteRecognition::columns_x_ = {
    {350, 600}, {950, 1200}, {1450, 1700}
};

const std::vector<std::pair<int, int>> RouteRecognition::rows_y_ = {
    {200, 400}, {450, 650}, {700, 900}
};

void GameGraph::add_node(int col, int row, NodeType type) {
    std::string node_id = std::to_string(col) + "_" + std::to_string(row);
    nodes_[node_id] = type;
    if (adjacency_list_.find(node_id) == adjacency_list_.end()) {
        adjacency_list_[node_id] = {};
    }
}

void GameGraph::add_edge(int from_col, int from_row, int to_col, int to_row) {
    std::string from_id = std::to_string(from_col) + "_" + std::to_string(from_row);
    std::string to_id = std::to_string(to_col) + "_" + std::to_string(to_row);
    auto it = adjacency_list_.find(from_id);
    if (it != adjacency_list_.end()) {
        auto& edges = it->second;
        if (std::find(edges.begin(), edges.end(), to_id) == edges.end()) {
            edges.push_back(to_id);
        }
    }
}

NodeType GameGraph::get_type(const std::string& node_id) const {
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        return it->second;
    }
    return NodeType::EMPTY;
}

RouteRecognition::RouteRecognition() {}

static cv::Mat imread_unicode(const std::filesystem::path& p, int flags) {
    std::ifstream file(p, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: [imread_unicode] 无法打开文件: " << p << std::endl;
        return {};
    }
    const std::vector<uchar> buffer(std::istreambuf_iterator<char>(file), {});
    file.close();
    if (buffer.empty()) {
        std::cerr << "Error: [imread_unicode] 文件为空: " << p << std::endl;
        return {};
    }
    try {
        cv::Mat img = cv::imdecode(buffer, flags);
        if (img.empty()) {
            std::cerr << "Error: [imread_unicode] cv::imdecode 解码失败 (图像为空): " << p << std::endl;
        }
        return img;
    } catch (const cv::Exception& ex) {
        std::cerr << "Error: [imread_unicode] cv::imdecode 失败: " << ex.what() << std::endl;
        return {};
    }
}

bool RouteRecognition::load_templates(const std::filesystem::path& base_path) {

    std::filesystem::path char_path = base_path / "image" / L"连结印记.png";    
    std::filesystem::path event_path = base_path / "image" / "EVENT.png";
    std::filesystem::path normal_path = base_path / "image" / L"战斗-普通.png";
    std::filesystem::path road_path = base_path / "image" / L"道路.png";

    tpl_char_ = imread_unicode(char_path, cv::IMREAD_COLOR);
    tpl_event_ = imread_unicode(event_path, cv::IMREAD_COLOR);
    tpl_normal_ = imread_unicode(normal_path, cv::IMREAD_COLOR);
    tpl_road_ = imread_unicode(road_path, cv::IMREAD_UNCHANGED);

    if (tpl_char_.empty()) {
        std::cerr << "Failed to load: " << char_path.string() << std::endl;
        return false;
    }
    if (tpl_event_.empty()) {
        std::cerr << "Failed to load: " << event_path.string() << std::endl;
        return false;
    }
    if (tpl_normal_.empty()) {
        std::cerr << "Failed to load: " << normal_path.string() << std::endl;
        return false;
    }
    if (tpl_road_.empty()) {
        std::cerr << "Failed to load: " << road_path.string() << std::endl;
        return false;
    }

    if (tpl_road_.channels() == 4) {
        tpl_road_bgr_ = tpl_road_.clone();
        cv::cvtColor(tpl_road_bgr_, tpl_road_bgr_, cv::COLOR_BGRA2BGR);
        std::vector<cv::Mat> channels;
        cv::split(tpl_road_, channels);
        tpl_road_alpha_ = channels[3];
    } else {
        tpl_road_bgr_ = tpl_road_.clone();
        tpl_road_alpha_ = cv::Mat();
    }

    return true;
}

void RouteRecognition::find_nodes(const cv::Mat& img, std::vector<std::vector<Node>>& grid_data,
                                   const cv::Mat& template_img, NodeType type, float threshold) {
    if (template_img.empty()) {
        std::cerr << "Template not loaded for type: " << static_cast<int>(type) << std::endl;
        return;
    }

    cv::Mat tpl = template_img;

    int h = tpl.rows;
    int w = tpl.cols;

    cv::Mat res;
    cv::matchTemplate(img, tpl, res, cv::TM_CCOEFF_NORMED);

    cv::Mat binary;
    cv::threshold(res, binary, static_cast<double>(threshold), 1.0, cv::THRESH_BINARY);
    std::vector<cv::Point> locations;
    cv::findNonZero(binary, locations);

    std::cerr << "[DEBUG] find_nodes for type=" << static_cast<int>(type) << " found " << locations.size() << " locations, tpl=" << w << "x" << h << std::endl;

    std::vector<std::pair<int, int>> unique_positions;
    int out_of_range = 0;
    int sample_count = 0;
    for (const auto& pt : locations) {
        int center_x = pt.x + w / 2;
        int center_y = pt.y + h / 2;

        if (type == NodeType::EVENT && sample_count < 5) {
            std::cerr << "[DEBUG]   EVENT sample: pt=(" << pt.x << "," << pt.y << ") center=(" << center_x << "," << center_y << ")" << std::endl;
            sample_count++;
        }

        int matched_col = -1;
        int matched_row = -1;

        for (int col_idx = 0; col_idx < static_cast<int>(columns_x_.size()); col_idx++) {
            if (center_x >= columns_x_[col_idx].first && center_x <= columns_x_[col_idx].second) {
                matched_col = col_idx;
                break;
            }
        }

        for (int row_idx = 0; row_idx < static_cast<int>(rows_y_.size()); row_idx++) {
            if (center_y >= rows_y_[row_idx].first && center_y <= rows_y_[row_idx].second) {
                matched_row = row_idx;
                break;
            }
        }

        if (matched_col >= 0 && matched_row >= 0) {
            if (std::find(unique_positions.begin(), unique_positions.end(), std::make_pair(matched_col, matched_row)) == unique_positions.end()) {
                unique_positions.push_back({matched_col, matched_row});
            }
            if (grid_data[matched_col][matched_row].type == NodeType::EMPTY) {
                grid_data[matched_col][matched_row].type = type;
                grid_data[matched_col][matched_row].pos = {center_x, center_y};
            }
        } else {
            out_of_range++;
        }
    }
    std::cerr << "[DEBUG]   unique grid positions: ";
    for (const auto& [col, row] : unique_positions) {
        std::cerr << "(" << col << "," << row << ") ";
    }
    std::cerr << ", out_of_range=" << out_of_range << std::endl;
}

bool RouteRecognition::is_line_existing(const cv::Mat& img,
                                        const std::pair<int, int>& pos_a,
                                        const std::pair<int, int>& pos_b,
                                        int row_curr, int row_next,
                                        float threshold) {
    if (pos_a.first == 0 && pos_a.second == 0 && pos_b.first == 0 && pos_b.second == 0) {
        return false;
    }

    if (tpl_road_bgr_.empty()) {
        return false;
    }

    int x1 = pos_a.first;
    int y1 = pos_a.second;
    int x2 = pos_b.first;
    int y2 = pos_b.second;
    int dy = y2 - y1;

    bool is_horizontal = (row_curr == row_next);

    cv::Mat current_tpl;
    cv::Mat current_mask;
    cv::Mat* current_mask_ptr = nullptr;

    if (is_horizontal) {
        current_tpl = tpl_road_bgr_;
        if (!tpl_road_alpha_.empty()) {
            current_mask_ptr = &tpl_road_alpha_;
        }
    } else {
        int fixed_width = 325;
        cv::Mat resized_bgr;
        cv::resize(tpl_road_bgr_, resized_bgr, cv::Size(fixed_width, tpl_road_bgr_.rows), 0, 0, cv::INTER_LINEAR);

        cv::Mat resized_alpha;
        if (!tpl_road_alpha_.empty()) {
            cv::resize(tpl_road_alpha_, resized_alpha, cv::Size(fixed_width, tpl_road_alpha_.rows), 0, 0, cv::INTER_LINEAR);
        }

        double fixed_angle = dy > 0 ? -31.0 : 31.0;

        cv::Point2f center(resized_bgr.cols / 2.0f, resized_bgr.rows / 2.0f);
        cv::Mat rot_matrix = cv::getRotationMatrix2D(center, fixed_angle, 1.0);

        double cos_val = std::abs(rot_matrix.at<double>(0, 0));
        double sin_val = std::abs(rot_matrix.at<double>(0, 1));
        int new_w = static_cast<int>(resized_bgr.rows * sin_val + resized_bgr.cols * cos_val);
        int new_h = static_cast<int>(resized_bgr.rows * cos_val + resized_bgr.cols * sin_val);

        rot_matrix.at<double>(0, 2) += (new_w / 2.0) - center.x;
        rot_matrix.at<double>(1, 2) += (new_h / 2.0) - center.y;

        cv::warpAffine(resized_bgr, current_tpl, rot_matrix, cv::Size(new_w, new_h),
                       cv::INTER_LINEAR);

        if (!resized_alpha.empty()) {
            cv::warpAffine(resized_alpha, current_mask, rot_matrix, cv::Size(new_w, new_h),
                           cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(0));
            current_mask_ptr = &current_mask;
        }
    }

    int mid_x = (x1 + x2) / 2;
    int mid_y = (y1 + y2) / 2;

    int roi_y_start = std::max(0, mid_y - 140);
    int roi_y_end = std::min(img.rows, mid_y + 140);
    int roi_x_start = std::max(0, mid_x - 190);
    int roi_x_end = std::min(img.cols, mid_x + 190);

    cv::Mat roi = img(cv::Rect(roi_x_start, roi_y_start, roi_x_end - roi_x_start, roi_y_end - roi_y_start));

    if (roi.rows < current_tpl.rows || roi.cols < current_tpl.cols) {
        return false;
    }

    double max_val;
    cv::Mat res;
    if (current_mask_ptr) {
        cv::matchTemplate(roi, current_tpl, res, cv::TM_CCOEFF_NORMED, *current_mask_ptr);
    } else {
        cv::matchTemplate(roi, current_tpl, res, cv::TM_CCOEFF_NORMED);
    }
    cv::minMaxLoc(res, nullptr, &max_val, nullptr, nullptr);

    return max_val > threshold;
}

std::vector<std::vector<std::string>> RouteRecognition::find_all_paths(
    const GameGraph& graph,
    const std::string& current_node,
    NodeType end_type,
    const std::vector<std::string>& current_path) {

    std::vector<std::string> new_path = current_path;
    new_path.push_back(current_node);

    NodeType current_type = graph.get_type(current_node);

    if (current_type == end_type && new_path.size() > 1) {
        return {new_path};
    }

    auto it = graph.adjacency_list().find(current_node);
    if (it == graph.adjacency_list().end() || it->second.empty()) {
        return {};
    }

    std::vector<std::vector<std::string>> all_paths;
    for (const auto& next_node : it->second) {
        if (std::find(new_path.begin(), new_path.end(), next_node) == new_path.end()) {
            std::vector<std::vector<std::string>> sub_paths = find_all_paths(graph, next_node, end_type, new_path);
            all_paths.insert(all_paths.end(), sub_paths.begin(), sub_paths.end());
        }
    }

    return all_paths;
}

std::vector<RouteInfo> RouteRecognition::plan_routes(const GameGraph& graph) {
    std::vector<RouteInfo> results;

    auto nodes = graph.nodes();
    std::vector<std::string> start_nodes;
    for (const auto& [id, type] : nodes) {
        if (type == NodeType::CHAR) {
            start_nodes.push_back(id);
        }
    }

    if (start_nodes.empty()) {
        return results;
    }

    for (const auto& start_node : start_nodes) {
        std::vector<std::vector<std::string>> valid_paths = find_all_paths(graph, start_node, NodeType::CHAR, {});

        for (const auto& path : valid_paths) {
            RouteInfo info;
            info.node_path = path;

            for (const auto& node_id : path) {
                std::string type_str;
                NodeType t = graph.get_type(node_id);
                switch (t) {
                    case NodeType::CHAR: type_str = "CHAR"; break;
                    case NodeType::EVENT: type_str = "EVENT"; break;
                    case NodeType::NORMAL: type_str = "NORMAL"; break;
                    default: type_str = "UNKNOWN"; break;
                }
                info.type_chain.push_back(type_str);

                if (t == NodeType::CHAR) info.chars_count++;
                else if (t == NodeType::EVENT) info.events_count++;
                else if (t == NodeType::NORMAL) info.normals_count++;
            }

            results.push_back(info);
        }
    }

    return results;
}

std::vector<RouteInfo> RouteRecognition::analyze(const cv::Mat& image) {
    std::cerr << "[DEBUG] analyze() called" << std::endl;
    std::cerr << "[DEBUG] image size: " << image.cols << "x" << image.rows << ", channels=" << image.channels() << std::endl;

    if (image.empty()) {
        std::cerr << "[DEBUG] ERROR: input image is empty!" << std::endl;
        return {};
    }

    std::vector<RouteInfo> results;

    std::vector<std::vector<Node>> grid_data(3, std::vector<Node>(3));

    std::cerr << "[DEBUG] columns_x_: " << columns_x_.size() << " columns" << std::endl;
    for (size_t i = 0; i < columns_x_.size(); i++) {
        std::cerr << "[DEBUG]   col " << i << ": (" << columns_x_[i].first << ", " << columns_x_[i].second << ")" << std::endl;
    }
    std::cerr << "[DEBUG] rows_y_: " << rows_y_.size() << " rows" << std::endl;
    for (size_t i = 0; i < rows_y_.size(); i++) {
        std::cerr << "[DEBUG]   row " << i << ": (" << rows_y_[i].first << ", " << rows_y_[i].second << ")" << std::endl;
    }

    std::cerr << "[DEBUG] tpl_char_ empty=" << tpl_char_.empty() << ", size=" << tpl_char_.cols << "x" << tpl_char_.rows << std::endl;
    std::cerr << "[DEBUG] tpl_event_ empty=" << tpl_event_.empty() << ", size=" << tpl_event_.cols << "x" << tpl_event_.rows << std::endl;
    std::cerr << "[DEBUG] tpl_normal_ empty=" << tpl_normal_.empty() << ", size=" << tpl_normal_.cols << "x" << tpl_normal_.rows << std::endl;
    std::cerr << "[DEBUG] tpl_road_ empty=" << tpl_road_.empty() << ", size=" << tpl_road_.cols << "x" << tpl_road_.rows << std::endl;
    std::cerr << "[DEBUG] tpl_road_bgr_ empty=" << tpl_road_bgr_.empty() << ", size=" << tpl_road_bgr_.cols << "x" << tpl_road_bgr_.rows << std::endl;

    find_nodes(image, grid_data, tpl_char_, NodeType::CHAR);
    find_nodes(image, grid_data, tpl_event_, NodeType::EVENT);
    find_nodes(image, grid_data, tpl_normal_, NodeType::NORMAL);

    std::cerr << "[DEBUG] Grid after node detection:" << std::endl;
    for (int row = 0; row < 3; row++) {
        std::cerr << "[DEBUG]   row " << row << ": ";
        for (int col = 0; col < 3; col++) {
            if (grid_data[col][row].type == NodeType::EMPTY) {
                std::cerr << "EMPTY ";
            } else if (grid_data[col][row].type == NodeType::CHAR) {
                std::cerr << "CHAR(" << grid_data[col][row].pos.first << "," << grid_data[col][row].pos.second << ") ";
            } else if (grid_data[col][row].type == NodeType::EVENT) {
                std::cerr << "EVENT(" << grid_data[col][row].pos.first << "," << grid_data[col][row].pos.second << ") ";
            } else if (grid_data[col][row].type == NodeType::NORMAL) {
                std::cerr << "NORMAL(" << grid_data[col][row].pos.first << "," << grid_data[col][row].pos.second << ") ";
            }
        }
        std::cerr << std::endl;
    }

    GameGraph graph;
    for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 3; row++) {
            if (grid_data[col][row].type != NodeType::EMPTY) {
                graph.add_node(col, row, grid_data[col][row].type);
            }
        }
    }

    std::cerr << "[DEBUG] Graph has " << graph.nodes().size() << " nodes" << std::endl;

    int line_checks = 0;
    int line_hits = 0;
    for (int col = 0; col < 2; col++) {
        for (int row_curr = 0; row_curr < 3; row_curr++) {
            if (grid_data[col][row_curr].type == NodeType::EMPTY) continue;

            for (int row_next = 0; row_next < 3; row_next++) {
                if (grid_data[col + 1][row_next].type == NodeType::EMPTY) continue;

                line_checks++;
                bool has_line = is_line_existing(image,
                                     grid_data[col][row_curr].pos,
                                     grid_data[col + 1][row_next].pos,
                                     row_curr, row_next);
                if (has_line) {
                    line_hits++;
                    graph.add_edge(col, row_curr, col + 1, row_next);
                }
            }
        }
    }

    std::cerr << "[DEBUG] Line checks: " << line_checks << ", hits: " << line_hits << std::endl;

    results = plan_routes(graph);
    std::cerr << "[DEBUG] Found " << results.size() << " routes" << std::endl;
    return results;
}
