#include "globals.h"
#include "route_recognition.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;

static RouteRecognition g_recognition;

const std::vector<std::pair<int, int>> RouteRecognition::columns_x_ = {
    {350, 600}, {950, 1200}, {1450, 1700}};

const std::vector<std::pair<int, int>> RouteRecognition::rows_y_ = {
    {200, 400}, {450, 650}, {700, 900}};

MaaBool RouteRecognitionCallback(
    MaaContext *context,
    MaaTaskId task_id,
    const char *node_name,
    const char *custom_recognition_name,
    const char *custom_recognition_param,
    const MaaImageBuffer *image,
    const MaaRect *roi,
    void *trans_arg,
    /* out */ MaaRect *out_box,
    /* out */ MaaStringBuffer *out_detail)
{
    cv::Mat mat(MaaImageBufferHeight(image), MaaImageBufferWidth(image), MaaImageBufferType(image), MaaImageBufferGetRawData(image));

    if (!g_recognition.load_templates(resource_path))
    {
        ERROR("资源路径错误");
        return false;
    }

    std::vector<RouteInfo> routes = g_recognition.analyze(mat);

    if (!routes.empty())
    {
        MaaRect box = {0, 0, 0, 0};
        *out_box = box;
        return true;
    }

    return false;
}

void GameGraph::add_node(int col, int row, NodeType type)
{
    std::string node_id = std::to_string(col) + "_" + std::to_string(row);
    nodes_[node_id] = type;
    if (adjacency_list_.find(node_id) == adjacency_list_.end())
    {
        adjacency_list_[node_id] = {};
    }
}

void GameGraph::add_edge(int from_col, int from_row, int to_col, int to_row)
{
    std::string from_id = std::to_string(from_col) + "_" + std::to_string(from_row);
    std::string to_id = std::to_string(to_col) + "_" + std::to_string(to_row);
    auto it = adjacency_list_.find(from_id);
    if (it != adjacency_list_.end())
    {
        auto &edges = it->second;
        if (std::find(edges.begin(), edges.end(), to_id) == edges.end())
        {
            edges.push_back(to_id);
        }
    }
}

NodeType GameGraph::get_type(const std::string &node_id) const
{
    auto it = nodes_.find(node_id);
    if (it != nodes_.end())
    {
        return it->second;
    }
    return NodeType::EMPTY;
}

RouteRecognition::RouteRecognition() {}

static cv::Mat imread_unicode(const std::filesystem::path &p, int flags)
{
    std::ifstream file(p, std::ios::binary);
    if (!file.is_open())
    {
        // std::cerr << "Error: [imread_unicode] 无法打开文件: " << p << std::endl;
        ERROR("[imread_unicode] 无法打开文件: " + p.string());
        return {};
    }
    const std::vector<uchar> buffer(std::istreambuf_iterator<char>(file), {});
    file.close();
    if (buffer.empty())
    {
        // std::cerr << "Error: [imread_unicode] 文件为空: " << p << std::endl;
        ERROR("[imread_unicode] 文件为空: " + p.string());
        return {};
    }
    try
    {
        cv::Mat img = cv::imdecode(buffer, flags);
        if (img.empty())
        {
            // std::cerr << "Error: [imread_unicode] cv::imdecode 解码失败 (图像为空): " << p << std::endl;
            ERROR("[imread_unicode] cv::imdecode 解码失败 (图像为空): " + p.string());
        }
        return img;
    }
    catch (const cv::Exception &ex)
    {
        // std::cerr << "Error: [imread_unicode] cv::imdecode 失败: " << ex.what() << std::endl;
        ERROR("[imread_unicode] cv::imdecode 失败: " + std::string(ex.what()));
        return {};
    }
}

bool RouteRecognition::load_templates(const std::filesystem::path &base_path)
{
    if (m_initialized)
    {
        return true;
    }

    auto load_if_fail = [&](const cv::Mat &mat, const std::string &name)
    {
        if (mat.empty())
        {
            // std::cerr << "Failed to load: " << name << std::endl;
            ERROR("Failed to load: " + name);
            return false;
        }
        return true;
    };

    std::filesystem::path char_path = base_path / "image" / L"连结印记.png";
    std::filesystem::path event_path = base_path / "image" / L"EVENT.png";
    std::filesystem::path normal_path = base_path / "image" / L"战斗-普通.png";
    std::filesystem::path road_path = base_path / "image" / L"道路.png";

    tpl_char_ = imread_unicode(char_path, cv::IMREAD_COLOR);
    tpl_event_ = imread_unicode(event_path, cv::IMREAD_COLOR);
    tpl_normal_ = imread_unicode(normal_path, cv::IMREAD_COLOR);
    cv::Mat tpl_road_raw = imread_unicode(road_path, cv::IMREAD_UNCHANGED);

    if (!load_if_fail(tpl_char_, "连结印记.png"))
        return false;
    if (!load_if_fail(tpl_event_, "EVENT.png"))
        return false;
    if (!load_if_fail(tpl_normal_, "战斗-普通.png"))
        return false;
    if (!load_if_fail(tpl_road_raw, "道路.png"))
        return false;

    cv::cvtColor(tpl_road_raw, tpl_road_bgr_, cv::COLOR_BGRA2BGR);
    std::vector<cv::Mat> channels;
    cv::split(tpl_road_raw, channels);
    tpl_road_alpha_ = channels[3];

    int fixed_width = 325;
    cv::Mat resized_bgr = tpl_road_bgr_.clone();
    cv::resize(tpl_road_bgr_, resized_bgr, cv::Size(fixed_width, tpl_road_bgr_.rows), 0, 0, cv::INTER_LINEAR);
    cv::Mat resized_alpha;
    cv::resize(tpl_road_alpha_, resized_alpha, cv::Size(fixed_width, tpl_road_alpha_.rows), 0, 0, cv::INTER_NEAREST);

    auto rotate_tpl = [&](double angle, cv::Mat &out_bgr, cv::Mat &out_alpha)
    {
        cv::Point2f center(resized_bgr.cols / 2.0f, resized_bgr.rows / 2.0f);
        cv::Mat rot_matrix = cv::getRotationMatrix2D(center, angle, 1.0);

        double cos_val = std::abs(rot_matrix.at<double>(0, 0));
        double sin_val = std::abs(rot_matrix.at<double>(0, 1));
        int new_w = static_cast<int>(resized_bgr.rows * sin_val + resized_bgr.cols * cos_val);
        int new_h = static_cast<int>(resized_bgr.rows * cos_val + resized_bgr.cols * sin_val);

        rot_matrix.at<double>(0, 2) += (new_w / 2.0) - center.x;
        rot_matrix.at<double>(1, 2) += (new_h / 2.0) - center.y;

        cv::warpAffine(resized_bgr, out_bgr, rot_matrix, cv::Size(new_w, new_h), cv::INTER_LINEAR);
        cv::warpAffine(resized_alpha, out_alpha, rot_matrix, cv::Size(new_w, new_h), cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(0));
    };

    rotate_tpl(-31.0, tpl_road_rotated_bgr_down_, tpl_road_rotated_alpha_down_);
    rotate_tpl(31.0, tpl_road_rotated_bgr_up_, tpl_road_rotated_alpha_up_);

    m_initialized = true;
    return true;
}

void RouteRecognition::find_nodes(const cv::Mat &img, std::vector<std::vector<Node>> &grid_data,
                                  const cv::Mat &template_img, NodeType type, float threshold)
{
    if (template_img.empty())
    {
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

    for (const auto &pt : locations)
    {
        int center_x = pt.x + w / 2;
        int center_y = pt.y + h / 2;

        int matched_col = -1;
        int matched_row = -1;

        for (int col_idx = 0; col_idx < static_cast<int>(columns_x_.size()); col_idx++)
        {
            if (center_x >= columns_x_[col_idx].first && center_x <= columns_x_[col_idx].second)
            {
                matched_col = col_idx;
                break;
            }
        }

        for (int row_idx = 0; row_idx < static_cast<int>(rows_y_.size()); row_idx++)
        {
            if (center_y >= rows_y_[row_idx].first && center_y <= rows_y_[row_idx].second)
            {
                matched_row = row_idx;
                break;
            }
        }

        if (matched_col >= 0 && matched_row >= 0)
        {
            if (grid_data[matched_col][matched_row].type == NodeType::EMPTY)
            {
                grid_data[matched_col][matched_row].type = type;
                grid_data[matched_col][matched_row].pos = {center_x, center_y};
            }
        }
    }
}

bool RouteRecognition::is_line_existing(const cv::Mat &img,
                                        const std::pair<int, int> &pos_a,
                                        const std::pair<int, int> &pos_b,
                                        int row_curr, int row_next,
                                        float threshold)
{
    if (pos_a.first == 0 && pos_a.second == 0 && pos_b.first == 0 && pos_b.second == 0)
    {
        return false;
    }

    if (tpl_road_bgr_.empty())
    {
        return false;
    }

    int x1 = pos_a.first;
    int y1 = pos_a.second;
    int x2 = pos_b.first;
    int y2 = pos_b.second;
    int dy = y2 - y1;

    bool is_horizontal = (row_curr == row_next);

    const cv::Mat *current_tpl = &tpl_road_bgr_;
    const cv::Mat *current_mask = &tpl_road_alpha_;

    if (!is_horizontal)
    {
        if (dy > 0)
        {
            current_tpl = &tpl_road_rotated_bgr_down_;
            current_mask = &tpl_road_rotated_alpha_down_;
        }
        else
        {
            current_tpl = &tpl_road_rotated_bgr_up_;
            current_mask = &tpl_road_rotated_alpha_up_;
        }
    }

    int mid_x = (x1 + x2) / 2;
    int mid_y = (y1 + y2) / 2;

    int roi_y_start = std::max(0, mid_y - 140);
    int roi_y_end = std::min(img.rows, mid_y + 140);
    int roi_x_start = std::max(0, mid_x - 190);
    int roi_x_end = std::min(img.cols, mid_x + 190);

    cv::Mat roi = img(cv::Rect(roi_x_start, roi_y_start, roi_x_end - roi_x_start, roi_y_end - roi_y_start));

    if (roi.rows < current_tpl->rows || roi.cols < current_tpl->cols)
    {
        return false;
    }

    double max_val;
    cv::Mat res;
    cv::matchTemplate(roi, *current_tpl, res, cv::TM_CCOEFF_NORMED, *current_mask);
    cv::minMaxLoc(res, nullptr, &max_val, nullptr, nullptr);

    return max_val > threshold;
}

std::vector<std::vector<std::string>> RouteRecognition::find_all_paths(
    const GameGraph &graph,
    const std::string &current_node,
    NodeType end_type,
    std::vector<std::string> &current_path)
{

    current_path.push_back(current_node);

    NodeType current_type = graph.get_type(current_node);

    if (current_type == end_type && current_path.size() > 1)
    {
        return {current_path};
    }

    auto it = graph.adjacency_list().find(current_node);
    if (it == graph.adjacency_list().end() || it->second.empty())
    {
        current_path.pop_back();
        return {};
    }

    std::vector<std::vector<std::string>> all_paths;
    for (const auto &next_node : it->second)
    {
        bool visited = std::find(current_path.begin(), current_path.end(), next_node) != current_path.end();
        if (!visited)
        {
            std::vector<std::vector<std::string>> sub_paths = find_all_paths(graph, next_node, end_type, current_path);
            all_paths.insert(all_paths.end(), sub_paths.begin(), sub_paths.end());
        }
    }
    current_path.pop_back();

    return all_paths;
}

std::vector<RouteInfo> RouteRecognition::plan_routes(const GameGraph &graph)
{
    std::vector<RouteInfo> results;

    auto nodes = graph.nodes();
    std::vector<std::string> start_nodes;
    for (const auto &[id, type] : nodes)
    {
        if (type == NodeType::CHAR)
        {
            start_nodes.push_back(id);
        }
    }

    if (start_nodes.empty())
    {
        return results;
    }

    for (const auto &start_node : start_nodes)
    {
        std::vector<std::string> path;
        std::vector<std::vector<std::string>> valid_paths = find_all_paths(graph, start_node, NodeType::CHAR, path);

        for (const auto &route : valid_paths)
        {
            RouteInfo info;
            info.node_path = route;

            for (const auto &node_id : route)
            {
                NodeType t = graph.get_type(node_id);
                static const char *type_names[] = {"EMPTY", "CHAR", "EVENT", "NORMAL"};
                info.type_chain.push_back(type_names[static_cast<int>(t)]);

                if (t == NodeType::CHAR)
                    info.chars_count++;
                else if (t == NodeType::EVENT)
                    info.events_count++;
                else if (t == NodeType::NORMAL)
                    info.normals_count++;
            }

            results.push_back(info);
        }
    }

    return results;
}

std::vector<RouteInfo> RouteRecognition::analyze(const cv::Mat &image)
{
    if (image.empty())
    {
        return {};
    }

    std::vector<RouteInfo> results;

    std::vector<std::vector<Node>> grid_data(3, std::vector<Node>(3));

    find_nodes(image, grid_data, tpl_char_, NodeType::CHAR, 0.95);
    find_nodes(image, grid_data, tpl_event_, NodeType::EVENT, 0.95);
    find_nodes(image, grid_data, tpl_normal_, NodeType::NORMAL, 0.95);

    GameGraph graph;
    for (int col = 0; col < 3; col++)
    {
        for (int row = 0; row < 3; row++)
        {
            if (grid_data[col][row].type != NodeType::EMPTY)
            {
                graph.add_node(col, row, grid_data[col][row].type);
            }
        }
    }

    static const char *type_names[] = {"EMPTY", "CHAR", "EVENT", "NORMAL"};
    for (int row = 0; row < 3; row++)
    {
        // std::cout << "Row " << row << ": ";
        INFO_INLINE("Row " + std::to_string(row) + ": ");
        for (int col = 0; col < 3; col++)
        {
            // std::cout << type_names[static_cast<int>(grid_data[col][row].type)];
            INFO_INLINE(type_names[static_cast<int>(grid_data[col][row].type)]);
            if (col < 2)
                INFO_INLINE(" ");
        }
        INFO("");
    }

    for (int col = 0; col < 2; col++)
    {
        for (int row_curr = 0; row_curr < 3; row_curr++)
        {
            if (grid_data[col][row_curr].type == NodeType::EMPTY)
                continue;

            for (int row_next = 0; row_next < 3; row_next++)
            {
                if (grid_data[col + 1][row_next].type == NodeType::EMPTY)
                    continue;

                bool has_line = is_line_existing(image,
                                                 grid_data[col][row_curr].pos,
                                                 grid_data[col + 1][row_next].pos,
                                                 row_curr, row_next);
                if (has_line)
                {
                    graph.add_edge(col, row_curr, col + 1, row_next);
                }
            }
        }
    }

    results = plan_routes(graph);
    // std::cout << "Routes: " << results.size() << std::endl;
    INFO("Routes:" + std::to_string(results.size()));
    return results;
}
