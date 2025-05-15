import networkx as nx
import matplotlib.pyplot as plt

class FlowRecord:

    def __init__(self, sid, did, send_step, receive_step, prev, sport, dport, size, start_time, fct, standalone_fct):
        self.sid = sid
        self.did = did
        self.send_step = send_step
        self.receive_step = receive_step
        self.prev = prev
        self.sport = sport
        self.dport = dport
        self.size = size
        self.start_time = start_time // 1000000
        self.fct = fct // 1000000
        self.standalone_fct = standalone_fct // 1000000

def remove_zero_in_degree_nodes(G, exclude_nodes={"All End"}):
    while True:
        nodes_to_remove = [
            n for n, d in G.in_degree()
            if d == 0 and n not in exclude_nodes
        ]
        if not nodes_to_remove:
            break
        G.remove_nodes_from(nodes_to_remove)
    return G

def parse_traffic_records(filename):

    records = []
    with open(filename, 'r') as f:
        for line_num, line in enumerate(f, 1):
            elements = line.strip().split()
            if not elements:
                continue

            if elements[0] == "###":
                continue
            
            try:
                converted = [int(e) for e in elements]
                
                record = FlowRecord(*converted)
                records.append(record)
            except (IndexError, ValueError) as e:
                print(f"Error in line {line_num}: {str(e)}")
    return records

num_j = 8
bias = 11
gap_i = 200
gap_j = 10

def build_graph(records):
    G = nx.DiGraph()

    n = records[-1].send_step
    
    prev_nodes = ["All Begin"] * len(records)
    wait = {}

    G.add_node("All Begin", label=f"All Begin", pos=[0, (num_j+1) / 2.0 * gap_j])
    G.add_node("All End", label=f"All End", pos=[gap_i*2 * n, (num_j+1) / 2.0 * gap_j])
    
    for record in records:
        if record.send_step != n:
            node = f"F{record.sid}S{record.send_step}_end"
            G.add_node(node, label=node, pos=[gap_i*2 * record.send_step, (record.sid-bias) * gap_j])
        else:
            node = "All End"
        if wait.get(record.sid) is None:
            G.add_edge(node, prev_nodes[record.sid], weight=record.fct, label=f"{record.fct} ms")
        else:
            node_begin = f"F{record.sid}S{record.send_step}_begin"
            G.add_node(node_begin, label=node_begin, pos=[gap_i*2 * record.send_step - gap_i, (record.sid-bias) * gap_j])
            # wait_time = record.start_time - wait[record.sid][1]
            # G.add_edge(node_begin, wait[record.sid][0], weight=wait_time, label=f"{wait_time} ms") 
            G.add_edge(node_begin, wait[record.sid][0], weight=0, label=f"w") 
            G.add_edge(node, node_begin, weight=record.fct, label=f"{record.fct} ms")

        if record.send_step == record.receive_step:
            wait[record.sid] = (f"F{record.prev}S{record.receive_step}_end", record.start_time + record.fct)
        else:
            wait[record.sid] = None
        prev_nodes[record.sid] = node

    remove_zero_in_degree_nodes(G)
    
    return G

def visualize_graph(G):

    critical_path = nx.dag_longest_path(G)
    print("Critical Path:", critical_path)
    for i in range(len(critical_path)-1):
        print(f"{critical_path[i]} -> {critical_path[i+1]} : {G.get_edge_data(critical_path[i], critical_path[i+1]).get('weight')}")
    print(nx.dag_longest_path_length(G))

    plt.figure(figsize=(100, 50))
    
    node_labels = {
        n: G.nodes[n].get('label')  
        for n in G.nodes
    }

    pos = {
        n: G.nodes[n].get('pos')  
        for n in G.nodes
    }
    
    edge_labels = {}
    for u, v, d in G.edges(data=True):
        edge_labels[(u, v)] = d.get('label')
    
    nx.draw_networkx_nodes(G, pos, node_color='lightblue', node_size=6000)
    nx.draw_networkx_edges(G, pos, edgelist=G.edges(), arrowstyle='-|>', arrowsize=20, width=2)
    nx.draw_networkx_labels(G, pos, labels=node_labels)
    nx.draw_networkx_edge_labels(G, pos, edge_labels=edge_labels, font_size=20)
    
    plt.savefig("demo_client.png")

if __name__ == "__main__":
    filename = "../out/fct.txt"  # 按完成时间排序
    flow_data = parse_traffic_records(filename)

    flow_graph = build_graph(flow_data)
    
    visualize_graph(flow_graph)
    