import networkx as nx
import matplotlib.pyplot as plt
import json
import os
from collections import defaultdict

WINDOW_SIZE = 10_000_000
OUTPUT_DIR = "graph_telemetry"

with open('telemetry.json') as f:
    data = json.load(f)

window_graphs = defaultdict(lambda: nx.DiGraph())

for sw_id, sw_data in data.items():
    for ts_str, ts_info in sw_data.items():
        timestamp = int(ts_str)
        window_start = (timestamp // WINDOW_SIZE) * WINDOW_SIZE
        
        G = window_graphs[window_start]
        G.graph.setdefault('sw_list', set()).add(sw_id)

        if ts_info["type"] == 'pfc_trace':
            continue
        
        p_data = ts_info["epoch_now"]
        for p_id, p_info in p_data.items():
            p_node = f"SW{sw_id}P{p_id}"
            
            G.add_node(p_node, node_type="P", color="royalblue", size=800)
            
            def process_edges(weight_dict, direction):
                for f_link, weight in weight_dict.items():
                    if abs(weight) < 1e-6:  
                        continue
                    f_node = f"F_{f_link}"
                    
                    G.add_node(f_node, node_type="F", color="limegreen", size=600)
                    
                    edge_attr = {
                        'weight': weight,
                        'label': f"{direction}:{weight:.1f}",
                        'color': 'crimson' if direction == 'P2F' else 'darkorange'
                    }
                    if direction == 'P2F':
                        G.add_edge(p_node, f_node, **edge_attr)
                    else:
                        G.add_edge(f_node, p_node, **edge_attr)
            
            process_edges(p_info["p2f_weight"], 'P2F')
            process_edges(p_info["f2p_weight"], 'F2P')

plt.style.use('ggplot')
config = {
    "edge_font_size": 8,
    "p_node_font": {'font_size': 10, 'font_weight': 'bold'},
    "f_node_font": {'font_size': 8 }
}


for window_start, G in window_graphs.items():
    plt.figure(figsize=(20, 12))
    
    node_types = nx.get_node_attributes(G, 'node_type')
    pos = nx.spring_layout(G, k=0.8, iterations=50, seed=42)
    
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
    nx.draw_networkx_edge_labels(G, pos, edge_labels=edge_labels, font_size=config["edge_font_size"])
    
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