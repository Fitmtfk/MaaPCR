from maa.agent.agent_server import AgentServer
from maa.custom_recognition import CustomRecognition
from maa.context import Context

from logger import logger


import cv2
import numpy as np
import os
import json
from pathlib import Path

PI_RESOURCE = os.environ.get("PI_RESOURCE") or '{}'
logger.set_log_dir(Path() / "debug" / "route_Recognition.log","debug")
base_path = Path((json.loads(PI_RESOURCE).get("path") or ["./resource"])[0])
logger.info("资源路径："+str(base_path.resolve()))

tpl_char = cv2.imdecode(np.fromfile(base_path / "image" / "连结印记.png", dtype=np.uint8),cv2.IMREAD_COLOR)
tpl_event = cv2.imdecode(np.fromfile(base_path / "image" / "EVENT.png", dtype=np.uint8),cv2.IMREAD_COLOR)
tpl_normal = cv2.imdecode(np.fromfile(base_path / "image" / "战斗-普通.png", dtype=np.uint8),cv2.IMREAD_COLOR)
tpl_road = cv2.imdecode(np.fromfile(base_path / "image" / "道路.png" , dtype=np.uint8),cv2.IMREAD_UNCHANGED)

COLUMNS_X = [
    (380, 500),
    (900, 1050),
    (1400, 1550)
]
ROWS_Y = [
    (200, 380),
    (500, 650),
    (750, 920)
]

def find_nodes(img, grid_data, template, type_name, threshold=0.75):
    if template is None:
        logger.error(f"❌ 错误：未能成功加载模板图片 -> {type_name}")
        return
    h, w = template.shape[:2]
    # 执行模板匹配
    res = cv2.matchTemplate(img, template, cv2.TM_CCOEFF_NORMED)
    loc = np.where(res >= threshold)
    for pt in zip(*loc[::-1]):  # pt 是匹配到的左上角坐标 (x, y)
        center_x = pt[0] + w // 2
        center_y = pt[1] + h // 2
        matched_col = -1
        matched_row = -1
        # 匹配所属列
        for col_idx, (min_x, max_x) in enumerate(COLUMNS_X):
            if min_x <= center_x <= max_x:
                matched_col = col_idx
                break
        # 匹配所属行
        for row_idx, (min_y, max_y) in enumerate(ROWS_Y):
            if min_y <= center_y <= max_y:
                matched_row = row_idx
                break
        # 如果成功定位到网格，且该格子目前没有被占用，则写入数据
        if matched_col != -1 and matched_row != -1:
            if grid_data[matched_col][matched_row]["type"] == "EMPTY":
                grid_data[matched_col][matched_row] = {
                    "type": type_name,
                    "pos": (center_x, center_y)
                }

def is_line_existing(img, pos_A, pos_B, row_curr, row_next, threshold=0.82):
    if pos_A is None or pos_B is None:
        return False
        
    if tpl_road is None:
        raise ValueError("图片解码失败")
    tpl_bgr = tpl_road[:, :, :3]
    tpl_alpha = tpl_road[:, :, 3]

    x1, y1 = int(pos_A[0]), int(pos_A[1])
    x2, y2 = int(pos_B[0]), int(pos_B[1])
    dy = y2 - y1
    
    is_horizontal = (row_curr == row_next)
    
    if is_horizontal:
        #【水平线分支】: 
        current_tpl = tpl_bgr
        current_mask = tpl_alpha
    else:
        #【斜线分支】: 拉伸至固定 325 像素，并顺/逆时针旋转 31°
        fixed_width = 325 
        orig_h = tpl_bgr.shape[0]
        tpl_bgr_resized = cv2.resize(tpl_bgr, (fixed_width, orig_h), interpolation=cv2.INTER_LINEAR)
        tpl_alpha_resized = cv2.resize(tpl_alpha, (fixed_width, orig_h), interpolation=cv2.INTER_LINEAR)
        
        fixed_angle = -31.0 if dy > 0 else 31.0
        
        (h, w) = tpl_bgr_resized.shape[:2]
        cx, cy = w // 2, h // 2
        M = cv2.getRotationMatrix2D((cx, cy), fixed_angle, 1.0)
        cos, sin = np.abs(M[0, 0]), np.abs(M[0, 1])
        nW = int((h * sin) + (w * cos))
        nH = int((h * cos) + (w * sin))
        M[0, 2] += (nW / 2) - cx
        M[1, 2] += (nH / 2) - cy
        
        current_tpl = cv2.warpAffine(tpl_bgr_resized, M, (nW, nH), flags=cv2.INTER_LINEAR)
        current_mask = cv2.warpAffine(tpl_alpha_resized, M, (nW, nH), flags=cv2.INTER_NEAREST, borderValue=0)
    
    # 切出安全区
    mid_x, mid_y = (x1 + x2) // 2, (y1 + y2) // 2
    roi = img[max(0, mid_y-140):min(img.shape[0], mid_y+140), 
              max(0, mid_x-190):min(img.shape[1], mid_x+190)]
              
    if roi.shape[0] < current_tpl.shape[0] or roi.shape[1] < current_tpl.shape[1]:
        return False 
        
    res = cv2.matchTemplate(roi, current_tpl, cv2.TM_CCOEFF_NORMED, mask=current_mask)
    _, max_val, _, _ = cv2.minMaxLoc(res)
    
    return max_val > threshold

class GameGraph:
    def __init__(self):
        # 存储节点属性，格式：{"0_0": "CHAR", "0_1": "NORMAL", ...}
        self.nodes = {}
        # 存储邻接表（拓扑通路），格式：{"0_0": ["1_0", "1_1"], ...}
        self.adjacency_list = {}
    
    def add_node(self, col, row, node_type):
        """添加节点及其类型"""
        node_id = f"{col}_{row}"
        self.nodes[node_id] = node_type
        if node_id not in self.adjacency_list:
            self.adjacency_list[node_id] = []
            
    def add_edge(self, from_col, from_row, to_col, to_row):
        """添加一条有向边（只能从左往右）"""
        from_id = f"{from_col}_{from_row}"
        to_id = f"{to_col}_{to_row}"
        if from_id in self.adjacency_list:
            if to_id not in self.adjacency_list[from_id]:
                self.adjacency_list[from_id].append(to_id)
                
    def get_type(self, node_id):
        """获取某个坐标的节点类型"""
        return self.nodes.get(node_id, "UNKNOWN")

    def show_graph(self):
        """直观打印当前图的完整结构"""
        logger.info("=== 📊 关卡数据图拓扑矩阵 ===")
        for node_id, edges in self.adjacency_list.items():
            node_type = self.get_type(node_id)
            edge_strs = [f"{target}({self.get_type(target)})" for target in edges]
            logger.info(f"📍 节点 {node_id} [{node_type}] ──> 可达: {edge_strs}")

def find_all_graph_paths(graph, current_node, end_type, current_path=[]):
    """
    基于 GameGraph 对象的拓扑寻路核心（递归回溯）
    """
    current_path = current_path + [current_node]
    current_type = graph.get_type(current_node)
    
    # 🎯 终点判定：如果当前节点类型正好是我们要找的终点类型，且路径长度大于 1（防止原地打转）
    if current_type == end_type and len(current_path) > 1:
        return [current_path]
        
    # 如果当前节点没有后续连线了，说明此路不通
    neighbors = graph.adjacency_list.get(current_node, [])
    if not neighbors:
        return []
        
    paths = []
    # 严格从左往右遍历邻居
    for next_node in neighbors:
        if next_node not in current_path:
            # 递归探索
            new_paths = find_all_graph_paths(graph, next_node, end_type, current_path)
            for p in new_paths:
                paths.append(p)
    return paths

def plan_routes_by_type(graph, start_type="CHAR", end_type="CHAR"):
    """
    高级路由规划：从指定的 start_type 到 end_type 的所有最优通路
    """
    # 1. 自动在图中检索出所有符合条件的起点节点
    start_nodes = [node_id for node_id, t_type in graph.nodes.items() if t_type == start_type]
    
    if not start_nodes:
        logger.info(f"⚠️  图中未发现类型为 [{start_type}] 的起点节点。")
        return False
        
    logger.info(f"🚀 [网格扫描] 锁定起点节点: {start_nodes}")
    logger.info(f"🎯 [目标锁定] 终点类型: [{end_type}]\n")
    
    total_routes_found = 0
    
    # 2. 从每一个潜在起点出发进行路径推演
    for s_node in start_nodes:
        valid_paths = find_all_graph_paths(graph, s_node, end_type)
        
        if not valid_paths:
            continue
            
        for path in valid_paths:
            total_routes_found += 1
            # 3. 统计这条路线的节点类型链
            type_chain = [graph.get_type(n) for n in path]
            
            # 4. 智能化计算旅途成分（比如包含多少个事件，多少个普通格）
            events_count = type_chain.count("EVENT")
            normals_count = type_chain.count("NORMAL")
            chars_count = type_chain.count("CHAR")
            
            logger.info(f"🛣️  【可行方案 {total_routes_found}】")
            logger.info(f"  🧭 节点走位: {' ──> '.join(path)}")
            logger.info(f"  🎨 属性步进: {' ──> '.join([f'{n}({graph.get_type(n)})' for n in path])}")
            logger.info(f"  📊 旅途成分: CHAR × {chars_count} | EVENT × {events_count} | NORMAL × {normals_count}")
            logger.info("-" * 65)
            
    if total_routes_found == 0:
        logger.info(f"🧱 规划失败：全图未发现任何能从 [{start_type}] 走到 [{end_type}] 的合规有向通路。")
        return False
    else:
        return True

@AgentServer.custom_recognition("route_recognition")
class route_recognition(CustomRecognition):

    def analyze(
        self,
        context: Context,
        argv: CustomRecognition.AnalyzeArg,
    ) -> CustomRecognition.AnalyzeResult:

        img = argv.image
        
        # 初始化 3x3 网格数据结构
        grid_data = [[{"type": "EMPTY", "pos": None} for _ in range(3)] for _ in range(3)]
        
        # 3. 依次识别三种节点（对应你的新变量名）
        find_nodes(img,grid_data,tpl_char, "CHAR")
        find_nodes(img,grid_data,tpl_event, "EVENT")
        find_nodes(img,grid_data,tpl_normal, "NORMAL")
        logger.info("--- 节点数字化结果 ---")
        for c in range(3):
            for r in range(3):
                node = grid_data[c][r]
                logger.info(f"列 {c} 行 {r} -> 类型: {node['type']}, 核心点击坐标: {node['pos']}")
        # 实例化数据图
        level_graph = GameGraph()

        # 先把所有存在的节点和它们的类型灌进去
        for col in range(3): # 遍历所有 3 列
            for row in range(3):
                node = grid_data[col][row]
                if node is None: continue
                level_graph.add_node(col, row, node.get("type"))

        # 遍历连线，灌入边（Edge）关系
        for col in range(2): # 前两列向后连线
            for row_curr in range(3):
                node_start = grid_data[col][row_curr]
                if node_start is None: continue

                for row_next in range(3):
                    node_end = grid_data[col+1][row_next]
                    if node_end is None: continue

                    if is_line_existing(img, node_start["pos"], node_end["pos"], row_curr, row_next):
                        level_graph.add_edge(col, row_curr, col+1, row_next)

        level_graph.show_graph()

        if plan_routes_by_type(level_graph, start_type="CHAR", end_type="CHAR"):
            return CustomRecognition.AnalyzeResult(
            box=(0,0,0,0), detail={"详情":"路线优秀"}
        )
        else:
            return CustomRecognition.AnalyzeResult(
                box=None, detail={"详情":"路线不佳"}
            )
