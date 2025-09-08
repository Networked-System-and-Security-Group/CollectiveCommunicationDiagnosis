import networkx as nx
import matplotlib.pyplot as plt
import json
import os
from collections import defaultdict

WINDOW_SIZE = 10_000_000
OUTPUT_DIR = "graph_telemetry"

switch_map = {}

f = open("../topology.txt", 'r')
lines = f.readlines()
f.close()

node_num = int(lines[0].split()[0])
link_num = int(lines[0].split()[2])

for idx in range(2, link_num+2):
    line = lines[idx].split()
    src = line[0]
    dst = line[1]
    if src not in switch_map:
        switch_map[src] = []
    if dst not in switch_map:
        switch_map[dst] = []
    switch_map[src].append(dst)
    switch_map[dst].append(src)

for key in switch_map.keys():
    switch_map[key] = {str(idx+1): switch_map[key][idx] for idx in range(len(switch_map[key]))}

with open('telemetry.json') as f:
    data = json.load(f)

window_graphs = defaultdict(lambda: nx.DiGraph())

sw_window_start = {}

for sw_id, sw_data in data.items():
    for ts_str, ts_info in sw_data.items():
        timestamp = int(ts_str)
        window_start = (timestamp // WINDOW_SIZE) * WINDOW_SIZE

        if sw_window_start.get(sw_id) == window_start:
            continue
        sw_window_start[sw_id] = window_start
        
        G = window_graphs[window_start]
        G.graph.setdefault('sw_list', set()).add(sw_id)

        if ts_info["type"] == 'pfc_trace':
            if ts_info.get("inport") is None:
                continue
            last_switch = switch_map[sw_id][ts_info["inport"]]
            last_port = ""
            for port in switch_map[last_switch].keys():
                if switch_map[last_switch][port] == sw_id:
                    last_port = port
                    break
            # print(last_switch)
            p_in_node = f"SW{last_switch}P{last_port}"
            # G.add_node(p_in_node, node_type="P", color="royalblue", size=800, timestamp=timestamp)
            G.add_node(p_in_node, node_type="P", color="royalblue", size=800)
        
        p_data = ts_info["epoch_now"]
        for p_id, p_info in p_data.items():

            if p_id == "p2p_weight":

                for p_p2p_id, p_p2p_weight in p_info.items():

                    edge_attr = {
                        'weight': p_p2p_weight,
                        'label': f"P2P:{p_p2p_weight:.2f}",
                        'color': 'blue'
                    }
                    p_node = f"SW{sw_id}P{p_p2p_id}"
                    G.add_node(p_node, node_type="P", color="royalblue", size=800, timestamp=timestamp)
                    G.add_edge(p_node, p_in_node, **edge_attr)

                continue

            p_node = f"SW{sw_id}P{p_id}"
            
            G.add_node(p_node, node_type="P", color="royalblue", size=800, timestamp=timestamp)

            def process_edges(weight_dict, direction):
                for f_link, weight in weight_dict.items():
                    if abs(weight) < 1e-6:  
                        continue
                    f_node = f"F_{f_link}"
                    
                    G.add_node(f_node, node_type="F", color="limegreen", size=600, timestamp=timestamp) 
                    # G.add_node(f_node, node_type="F", color="limegreen", size=600) 
                    
                    edge_attr = {
                        'weight': weight,
                        'label': f"{direction}:{weight:.1f}",
                        'color': 'crimson' if direction == 'P2F' else 'darkorange'
                    }
                    if direction == 'P2F':
                        G.add_edge(p_node, f_node, **edge_attr)
                    else:
                        G.add_edge(f_node, p_node, **edge_attr)
            
            if p_info.get("p2f_weight") is not None:
                process_edges(p_info["p2f_weight"], 'P2F')
            if p_info.get("f2p_weight") is not None:
                process_edges(p_info["f2p_weight"], 'F2P')


plt.style.use('ggplot')
config = {
    "edge_font_size": 8,
    "p_node_font": {'font_size': 10, 'font_weight': 'bold'},
    "f_node_font": {'font_size': 8 }
}

def delete_files(directory):
    file_list = os.listdir(directory)
    for file in file_list:
        file_path = os.path.join(directory, file)
        if os.path.isfile(file_path):
            os.remove(file_path)

delete_files(OUTPUT_DIR)

for window_start, G in window_graphs.items():
    plt.figure(figsize=(20, 12))
    
    node_types = nx.get_node_attributes(G, 'node_type')
    # pos = nx.spring_layout(G, seed=32)
    pos = nx.random_layout(G, seed=32)
    
    nx.draw_networkx_nodes(
        G, pos,
        nodelist=[n for n, t in node_types.items() if t == "P"],
        node_color='royalblue',
        node_size=800,
        edgecolors='navy',
        linewidths=1.5
    )
    nx.draw_networkx_nodes(
        G, pos,
        nodelist=[n for n, t in node_types.items() if t == "F"],
        node_color='limegreen',
        node_size=600,
        edgecolors='darkgreen',
        node_shape='s'
    )
    
    edge_colors = [e['color'] for _, _, e in G.edges(data=True)]
    nx.draw_networkx_edges(
        G, pos,
        edge_color=edge_colors,
        arrowstyle='-|>',
        arrowsize=20,
        width=1.2
    )
    
    edge_labels = {(u, v): d['label'] for u, v, d in G.edges(data=True)}
    nx.draw_networkx_edge_labels(G, pos, label_pos=0.3, edge_labels=edge_labels, font_size=config["edge_font_size"])
    
    p_labels = {n: n for n in G.nodes if n.startswith('SW')}
    f_labels = {n: n.split('_')[1] for n in G.nodes if n.startswith('F_')}
    nx.draw_networkx_labels(G, pos, labels=p_labels, **config["p_node_font"])
    nx.draw_networkx_labels(G, pos, labels=f_labels, **config["f_node_font"])
    
    plt.legend(
        handles=[
            plt.Line2D([0], [0], marker='o', color='w', label='P Node', 
                      markerfacecolor='royalblue', markersize=12),
            plt.Line2D([0], [0], marker='s', color='w', label='F Node',
                      markerfacecolor='limegreen', markersize=10)
        ],
        loc='upper right'
    )
    
    window_end = window_start + WINDOW_SIZE
    title = f"Global Network ({window_start//1e6}-{window_end//1e6}M)\nSWs: {', '.join(G.graph['sw_list'])}"
    plt.title(title, fontsize=14, pad=20)
    
    output_path = os.path.join(OUTPUT_DIR, f"window_{window_start}-{window_end}.png")
    plt.savefig(output_path, bbox_inches='tight', dpi=150)
    plt.close()

print(f"save to {OUTPUT_DIR}")