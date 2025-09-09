import json
import bisect

SWITCH_LIST = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
CC_PORT = 10000
DEMO_CLIENT = true
START_TIME_OFFSET = 2000  # in microseconds

cc_f_waitfor = {}
cc_f_timestamp = 0
cc_f_swid = ''

def sim_pkt_queue(flows, pktnums, queuedepths):
    flownum = len(flows)
    if flownum == 0:
        return
    ptrs = [0] * flownum
    pkt_list = []
    pkt_waitfor = []
    degree = {}

    for flow_id in range(flownum):
        degree[flows[flow_id]] = 0
        pkt_waitfor.append([])
        for flow_id1 in range(flownum):
            pkt_waitfor[flow_id].append(0)
    
    while True:
        rela_ptrs = [ ptr/pktnum for ptr, pktnum in zip(ptrs, pktnums)]
        if(min(rela_ptrs) >= 1):
            break
        max_idx = rela_ptrs.index(min(rela_ptrs))
        ptrs[max_idx] += 1
        pkt_list.append(max_idx)

    for pkt_id in range(len(pkt_list)):
        queuedepth = queuedepths[pkt_list[pkt_id]]
        for i in range(1, queuedepth + 1):
            if pkt_id - i < 0:
                break
            pkt_waitfor[pkt_list[pkt_id]][pkt_list[pkt_id - i]] += 1

    for flow_idx in range(flownum):
        for flow_idx1 in range(flownum):
            # pkt_waitfor[flow_idx][flow_idx1] = round(pkt_waitfor[flow_idx][flow_idx1] / pktnums[flow_idx], 1)
            pkt_waitfor[flow_idx][flow_idx1] = round(pkt_waitfor[flow_idx][flow_idx1], 1)
    pkt_waitfor = [dict(zip(flows, pkt_waitfor[flow_idx])) for flow_idx in range(flownum)]
    pkt_waitfor = dict(zip(flows, pkt_waitfor))

    for flow in flows:
        for flow1 in flows:

            if flow == flow1:
                continue
            
            degree[flow] += pkt_waitfor[flow][flow1]

            if flow.endswith(f'{CC_PORT}'):
                global cc_f_waitfor, cc_f_timestamp
                if cc_f_waitfor.get(cc_f_timestamp) is None:
                    cc_f_waitfor[cc_f_timestamp] = {}
                if cc_f_waitfor[cc_f_timestamp].get(cc_f_swid) is None:
                    cc_f_waitfor[cc_f_timestamp][cc_f_swid] = {}
                if cc_f_waitfor[cc_f_timestamp][cc_f_swid].get(flow) is None:
                    cc_f_waitfor[cc_f_timestamp][cc_f_swid][flow] = {}
                cc_f_waitfor[cc_f_timestamp][cc_f_swid][flow][flow1] = pkt_waitfor[flow][flow1]

    return degree

def parse_telemetry(switch_dict, switch_list):
    for swicth_id in switch_list:
        # print('Parsing switch', swicth_id)
        f = open("telemetry_"+str(swicth_id)+".txt", 'r')
        lines = f.readlines()
        f.close()

        switch_dict[str(swicth_id)] = {}
        line_idx = 0
        while line_idx < len(lines):
            line = lines[line_idx]
            
            if line.startswith("time"):
                time = line.split()[1]
                switch_dict[str(swicth_id)][time] = {"epoch_now":{},"epoch_last":{}}
                porttelemetry = {"epoch_now":{},"epoch_last":{}}
                trafficmeter = {}
                inport = -1
                teleflag = False
                polling = False
                port = "0"
                epoch = "epoch_now"

            elif line.startswith("end"):
                if inport != -1:
                    switch_dict[str(swicth_id)][time]["inport"] = inport
                for epoch in ["epoch_now", "epoch_last"]:
                    if switch_dict[str(swicth_id)][time][epoch] != {} and polling == False:
                        for key in porttelemetry[epoch].keys():
                            if sum(trafficmeter.values()) == 0:
                                porttelemetry[epoch][key] = 0
                            else:
                                porttelemetry[epoch][key] = float(trafficmeter[key]) / sum(trafficmeter.values())
                        switch_dict[str(swicth_id)][time][epoch]["p2p_weight"] = porttelemetry[epoch]
                
            elif line.startswith("polling"):
                switch_dict[str(swicth_id)][time]["type"] = "flow_trace"
                polling = True
            elif line.startswith("signal"):
                switch_dict[str(swicth_id)][time]["type"] = "pfc_trace"
                polling = False
            elif line.startswith("epoch"):
                epoch = "epoch_"+line.split()[1]
            elif teleflag and line == '\n':
                teleflag = False
                global cc_f_timestamp, cc_f_swid
                cc_f_timestamp = time
                cc_f_swid = f'SW{swicth_id}P{port}'
                degrees = sim_pkt_queue(flows, pktnums, queuedepths)
                if degrees is not None:
                    switch_dict[str(swicth_id)][time][epoch][port]["f2p_weight"] = degrees

                for flow, pn in zip(flows, pktnums):
                    switch_dict[str(swicth_id)][time][epoch][port]["p2f_weight"][flow] = float(pn) / sum(pktnums) * qdepth 
            elif line.startswith("flow telemetry"):
                flows = []
                pktnums = []
                queuedepths = []
                switch_dict[str(swicth_id)][time][epoch][port]["p2f_weight"] = {}
                port = line.split()[-1]
                line_idx += 1
                teleflag = True
            elif line.startswith("port telemetry"):
                port = line[:-1].split()[-1]
                pktnum = int(lines[line_idx+2].split()[2])
                qdepth = int(lines[line_idx+2].split()[0])
                paused = int(lines[line_idx+2].split()[1])
                switch_dict[str(swicth_id)][time][epoch][port] = {}
                switch_dict[str(swicth_id)][time][epoch][port]["paused_pkt"] = paused
                if pktnum == 0:
                    porttelemetry[epoch][line[:-1].split()[-1]] = 0
                else:
                    porttelemetry[epoch][line[:-1].split()[-1]] = qdepth / pktnum
            elif line.startswith("traffic meter form port"):
                port = line[:-1].split()[-1]
                trafficmeter[port] = int(lines[line_idx+2][:-1])
                inport = line[:-1].split()[4]
            elif teleflag:
                flow = line.split()[1]+"->"+line.split()[2]+"\n"+line.split()[4]
                pktnum = int(line.split()[8])
                paused = int(line.split()[10])
                flows.append(flow)
                pktnums.append(pktnum)
                # switch_dict[str(swicth_id)][time][epoch][port]["p2f_weight"][flow] = paused
                if (int(line.split()[8])-int(line.split()[10])) == 0:
                    queuedepths.append(0)
                else:
                    queuedepths.append(int(int(line.split()[9]) / (int(line.split()[8])-int(line.split()[10]))))

            line_idx += 1

def ip_to_node_id(ip: str) -> int:
    return int(ip[2:6], 16)


def rating():
    import demo_telemetry
    import networkx as nx
    graphs = demo_telemetry.window_graphs

    def parse_network_graph(graph):

        H = nx.DiGraph()
        
        for node, attr in graph.nodes(data=True):
            if graph.degree()[node] == 0:
                continue
            H.add_node(node, **attr)

        f_nodes = [n for n, attr in H.nodes(data=True) if attr.get('node_type') == 'F']
        p_nodes = [n for n, attr in H.nodes(data=True) if attr.get('node_type') == 'P']

        ccf_nodes = []
        nor_nodes = []
        for node in f_nodes:
            if node.endswith(f'{CC_PORT}'):
                ccf_nodes.append(node)
            else:
                nor_nodes.append(node)
        
        for u, v, attr in graph.edges(data=True):
            if (u in p_nodes and v in nor_nodes) or (u in p_nodes and v in p_nodes) or (u in ccf_nodes and v in p_nodes):
                H.add_edge(v, u, **attr)

        # print(ccf_nodes)

        topo_order = list(nx.topological_sort(H))
        rating = {node : {} for node in p_nodes}
        ccf_rating = {node : (H.nodes[node]['timestamp'], {}) for node in ccf_nodes}

        for node in topo_order:
            
            successors = list(H.successors(node))
            
            for succ in successors:
                edge_data = H.get_edge_data(node, succ)
                edge_weight = edge_data.get('weight')

                if node in nor_nodes:
                    if rating[succ].get(node) is None:
                        rating[succ][node] = 0
                    rating[succ][node] = rating[succ][node] + edge_weight
                elif node in p_nodes:
                    if succ in p_nodes:
                        for f in rating[node].keys():
                            if rating[succ].get(f) is None:
                                rating[succ][f] = 0
                            rating[succ][f] = rating[succ][f] + rating[node][f] * edge_weight
                    elif succ in ccf_nodes:
                        for f in rating[node].keys():
                            if ccf_rating[succ][1].get(f) is None:
                                ccf_rating[succ][1][f] = 0
                            if H.get_edge_data(f, node) is None:
                                ccf_rating[succ][1][f] = ccf_rating[succ][1][f] + rating[node][f]
                            else:
                                ccf_rating[succ][1][f] = ccf_rating[succ][1][f] + rating[node][f] - H.get_edge_data(f, node).get('weight') + cc_f_waitfor[f"{H.nodes[node]['timestamp']}"][node][succ.split('_')[1]][f.split('_')[1]]

        return ccf_rating
    
    # ccf_rating = parse_network_graph(graphs[list(graphs.keys())[0]])
    # print('CCF Rating:', json.dumps(ccf_rating))

    ccf_ratings = []

    f = open('result.txt', 'a+')
    f.truncate(0)

    for window, G in sorted(graphs.items()):
        ccf_rating = parse_network_graph(G)
        ccf_ratings.append(ccf_rating)

        with open('result.txt', 'a+') as f:
            f.write(f'Graph window_{window}:\n{json.dumps(ccf_rating, indent=4)}\n\n')

    f.close()

    FS_rating = {}

    if DEMO_CLIENT:
        import demo_client
        flow_graph = demo_client.demo()
        critical_path = nx.dag_longest_path(flow_graph)

        time_sum = []
        fs = []
        for i in range(len(critical_path)-1):
            if flow_graph.get_edge_data(critical_path[i], critical_path[i+1]).get('weight') == 0:
                continue
            if critical_path[i+1].endswith('_begin'):
                fs.append(critical_path[i+1].split('_')[0])
            elif critical_path[i+1].endswith('_end'):
                s = critical_path[i+1].split('_')[0]
                fff, ffs = s.split('S')
                ffs = str(int(ffs) + 1)
                fs.append(fff + 'S' + ffs)
            else:
                assert critical_path[i+1] == 'All Begin'
                fs.append(critical_path[i].split('_')[0])
            time_sum.append(flow_graph.get_edge_data(critical_path[i], critical_path[i+1]).get('weight'))

        time_sum.append(0)
        time_sum = list(reversed(time_sum))
        for i in range(len(time_sum)-1):
            time_sum[i+1] += time_sum[i]

        fs = list(reversed(fs))

        weight = []
        for i in range(1, len(time_sum)):
            weight.append((time_sum[i] - time_sum[i-1]) / time_sum[-1])

        # print('Critical Path:', fs)
        # print('Time Sum:', time_sum)
        # print('Weight:', weight)

        for ccf_rating in ccf_ratings:
            for ccf, (timestamp, rating) in ccf_rating.items():
                timestamp = timestamp / 1000000 - START_TIME_OFFSET
                if timestamp >= time_sum[-1]:
                    continue
                index = bisect.bisect(time_sum, timestamp)

                ccf_str, _ = ccf.split('\n')
                ccf_src = ip_to_node_id(ccf_str[2:10])
                fs_f = fs[index-1]
                fs_f = int(fs_f[1:fs_f.index('S')])
                if ccf_src != fs_f:
                    continue

                for fl, ra in rating.items():
                    if FS_rating.get(fl) is None:
                        FS_rating[fl] = 0
                    FS_rating[fl] += ra * weight[index-1]
        
        # print(f'Rating: {json.dumps(FS_rating, indent=4)}')
        generate_rating_table(FS_rating)
        with open('result.txt', 'a+') as f:
            f.write(f'Rating:\n{json.dumps(FS_rating, indent=4)}\n\n')

    return FS_rating

def generate_rating_table(data):

    table_data = []
    for key, value in data.items():
        addr_part, port = key.split('\n')
        src_dst = addr_part.split('->')
        src = src_dst[0][2:] 
        dst = src_dst[1]
        src_num = int(src[2:6], 16)
        dst_num = int(dst[2:6], 16)
        table_data.append([src_num, dst_num, port, round(value, 2)])
    
    table_data.sort(key=lambda x: x[3], reverse=True)
    
    print("+--------+--------+--------+----------+")
    print("| src_id | dest_id |  port  |   rating   |")
    print("+--------+--------+--------+----------+")
    for row in table_data:
        print(f"|{row[0]:^8}|{row[1]:^8}|{row[2]:^8}|{row[3]:^10.2f}|")
    print("+--------+--------+--------+----------+")

def main():
    switch_list = SWITCH_LIST
    switch_dict = {}

    parse_telemetry(switch_dict, switch_list)

    for switch in switch_dict.keys():
        time_list = list(switch_dict[switch].keys())
        for time_idx in range(len(time_list) - 1):
            if int(time_list[time_idx + 1]) - int(time_list[time_idx]) < 50000:
                switch_dict[switch].pop(time_list[time_idx+1])

    with open('telemetry.json', 'w') as f:
        json.dump(switch_dict, f)
    
    # print('waitfor data:', json.dumps(cc_f_waitfor, indent=4))

    rating()

if __name__ == "__main__":
    main()