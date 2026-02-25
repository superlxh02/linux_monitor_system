// Imports from global scope (CDN)
const {
    createTheme, ThemeProvider, CssBaseline, AppBar, Toolbar, Typography, Container, Grid, Paper, Box,
    Card, CardContent, Chip, Table, TableBody, TableCell, TableContainer, TableHead, TableRow,
    IconButton, LinearProgress, Button, Dialog, DialogTitle, DialogContent, DialogActions,
    List, ListItem, ListItemIcon, ListItemText, Divider, Drawer, Checkbox, FormControlLabel, FormGroup,
    MenuItem, Select, TextField
} = MaterialUI;

const {
    LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, AreaChart, Area, ReferenceLine
} = Recharts;

// --- Theme ---
const theme = createTheme({
    palette: {
        mode: "dark",
        primary: { main: "#2196f3" },
        secondary: { main: "#f50057" },
        background: { default: "#0a1929", paper: "#132f4c" },
        success: { main: "#00e676" },
        warning: { main: "#ffea00" },
        error: { main: "#ff1744" },
    },
    components: {
        MuiCard: { styleOverrides: { root: { backgroundImage: "none", backgroundColor: "#1e1e1e", borderRadius: 12 } } },
        MuiPaper: { styleOverrides: { root: { backgroundImage: "none", backgroundColor: "#132f4c" } } }
    }
});

const UNIT_MAP = {
    score: "分",
    cpu_percent: "%",
    usr_percent: "%",
    system_percent: "%",
    nice_percent: "%",
    idle_percent: "%",
    io_wait_percent: "%",
    irq_percent: "%",
    soft_irq_percent: "%",
    mem_used_percent: "%",
    disk_util_percent: "%",
    mem_total: "GB",
    mem_free: "GB",
    mem_avail: "GB",
    rcv_rate: "kB/s",
    send_rate: "kB/s",
    rcv_bytes_rate: "B/s",
    snd_bytes_rate: "B/s",
    rcv_packets_rate: "包/s",
    snd_packets_rate: "包/s",
    err_in: "次",
    err_out: "次",
    drop_in: "次",
    drop_out: "次",
    read_bytes_per_sec: "B/s",
    write_bytes_per_sec: "B/s",
    read_iops: "次/s",
    write_iops: "次/s",
    avg_read_latency_ms: "ms",
    avg_write_latency_ms: "ms",
    util_percent: "%",
    total: "GB",
    free: "GB",
    avail: "GB",
    buffers: "GB",
    cached: "GB",
    swap_cached: "GB",
    active: "GB",
    inactive: "GB",
    active_anon: "GB",
    inactive_anon: "GB",
    active_file: "GB",
    inactive_file: "GB",
    dirty: "GB",
    writeback: "GB",
    anon_pages: "GB",
    mapped: "GB",
    hi: "次",
    timer: "次",
    net_tx: "次",
    net_rx: "次",
    block: "次",
    irq_poll: "次",
    tasklet: "次",
    sched: "次",
    hrtimer: "次",
    rcu: "次"
};

const FIELD_LABEL_MAP = {
    server_name: "节点名称",
    timestamp: "采集时间",
    score: "综合评分",
    status: "在线状态",
    cpu_percent: "CPU总使用率",
    usr_percent: "用户态占比",
    system_percent: "内核态占比",
    nice_percent: "Nice占比",
    idle_percent: "空闲占比",
    io_wait_percent: "IO等待占比",
    irq_percent: "硬中断占比",
    soft_irq_percent: "软中断占比",
    load_avg_1: "1分钟负载",
    load_avg_3: "3分钟负载",
    load_avg_15: "15分钟负载",
    mem_used_percent: "内存使用率",
    mem_total: "总内存",
    mem_free: "空闲内存",
    mem_avail: "可用内存",
    disk_util_percent: "磁盘利用率",
    send_rate: "发送速率",
    rcv_rate: "接收速率",
    cpu_percent_rate: "CPU变化率",
    mem_used_percent_rate: "内存变化率",
    disk_util_percent_rate: "磁盘变化率",
    load_avg_1_rate: "负载变化率",
    send_rate_rate: "发送速率变化",
    rcv_rate_rate: "接收速率变化",
    anomaly_type: "异常类型",
    severity: "严重级别",
    value: "异常值",
    threshold: "阈值",
    metric_name: "异常指标",
    net_name: "网卡",
    err_in: "接收错误数",
    err_out: "发送错误数",
    drop_in: "接收丢包数",
    drop_out: "发送丢包数",
    rcv_bytes_rate: "接收字节速率",
    snd_bytes_rate: "发送字节速率",
    rcv_packets_rate: "接收包速率",
    snd_packets_rate: "发送包速率",
    disk_name: "磁盘设备",
    read_bytes_per_sec: "读吞吐",
    write_bytes_per_sec: "写吞吐",
    read_iops: "读IOPS",
    write_iops: "写IOPS",
    avg_read_latency_ms: "平均读时延",
    avg_write_latency_ms: "平均写时延",
    total: "总内存",
    free: "空闲内存",
    avail: "可用内存",
    buffers: "Buffers",
    cached: "缓存",
    active: "活跃内存",
    inactive: "非活跃内存",
    dirty: "脏页",
    cpu_name: "CPU核心",
    hi: "HI软中断",
    timer: "TIMER软中断",
    net_tx: "NET_TX软中断",
    net_rx: "NET_RX软中断",
    block: "BLOCK软中断",
    sched: "SCHED软中断"
};

const getUnitByField = (field) => UNIT_MAP[field] || "";
const getFieldLabel = (field) => FIELD_LABEL_MAP[field] || field;

const getMetricUnitByDataKey = (dataKey = "") => {
    if (dataKey.endsWith("_cpu") || dataKey.endsWith("_mem") || dataKey.endsWith("_disk")) return "%";
    if (dataKey.endsWith("_net")) return "MB/s";
    return "";
};

const SCORING_PROFILES = [
    { value: "BALANCED", label: "通用均衡" },
    { value: "HIGH_CONCURRENCY", label: "高并发" },
    { value: "IO_INTENSIVE", label: "IO密集" },
    { value: "MEMORY_SENSITIVE", label: "内存敏感" }
];

const getScoringWeights = (profile) => {
    switch (profile) {
        case "HIGH_CONCURRENCY":
            return { cpu: 0.45, mem: 0.25, load: 0.15, disk: 0.10, net: 0.05, loadCoef: 1.2, maxBandwidth: 125000000 };
        case "IO_INTENSIVE":
            return { cpu: 0.20, mem: 0.15, load: 0.20, disk: 0.35, net: 0.10, loadCoef: 2.0, maxBandwidth: 125000000 };
        case "MEMORY_SENSITIVE":
            return { cpu: 0.20, mem: 0.45, load: 0.15, disk: 0.10, net: 0.10, loadCoef: 1.5, maxBandwidth: 125000000 };
        case "BALANCED":
        default:
            return { cpu: 0.35, mem: 0.30, load: 0.15, disk: 0.15, net: 0.05, loadCoef: 1.5, maxBandwidth: 125000000 };
    }
};

const clamp01 = (v) => Math.max(0, Math.min(1, v));

const calculateScenarioScore = (server, profile) => {
    const w = getScoringWeights(profile);
    const cpu = Number(server.cpu_percent) || 0;
    const mem = Number(server.mem_used_percent) || 0;
    const load = Number(server.load_avg_1) || 0;
    const disk = Number(server.disk_util_percent) || 0;
    const sendBytes = (Number(server.send_rate) || 0) * 1024;
    const recvBytes = (Number(server.rcv_rate) || 0) * 1024;

    const cpuScore = clamp01(1 - cpu / 100);
    const memScore = clamp01(1 - mem / 100);
    const loadScore = clamp01(1 - load / (4 * w.loadCoef));
    const diskScore = clamp01(1 - disk / 100);
    const recvScore = clamp01(1 - recvBytes / w.maxBandwidth);
    const sendScore = clamp01(1 - sendBytes / w.maxBandwidth);
    const netScore = (recvScore + sendScore) / 2;

    const score = 100 * (cpuScore * w.cpu + memScore * w.mem + loadScore * w.load + diskScore * w.disk + netScore * w.net);
    return Math.max(0, Math.min(100, score));
};

const applyScoringProfileToOverview = (overviewData, profile) => {
    const servers = (overviewData?.servers || []).map((s) => ({ ...s, score: calculateScenarioScore(s, profile) }));
    const sortedByScore = [...servers].sort((a, b) => (b.score || 0) - (a.score || 0));
    const total = servers.length;
    const online = servers.filter((s) => s.status === "0" || s.status === "ONLINE" || s.status === 0).length;
    const sum = servers.reduce((acc, s) => acc + (Number(s.score) || 0), 0);
    const max = sortedByScore[0]?.score || 0;
    const min = sortedByScore[sortedByScore.length - 1]?.score || 0;
    const cluster_stats = {
        ...(overviewData?.cluster_stats || {}),
        total_servers: total,
        online_servers: online,
        offline_servers: Math.max(0, total - online),
        avg_score: total > 0 ? sum / total : 0,
        max_score: max,
        min_score: min,
        best_server: sortedByScore[0]?.server_name || "",
        worst_server: sortedByScore[sortedByScore.length - 1]?.server_name || ""
    };
    return { ...overviewData, servers, cluster_stats, scoring_profile: profile };
};

// --- Dashboard Components ---

const TopStats = ({ clusterStats }) => (
    <Grid container spacing={3} sx={{ height: "100%" }}>
        {/* Cluster Health */}
        <Grid item xs={12} md={6} sx={{ height: "100%" }}>
            <Card sx={{ height: "100%", display: "flex", flexDirection: "column", justifyContent: "center", p: 4, background: "linear-gradient(135deg, rgba(33, 150, 243, 0.1) 0%, rgba(0,0,0,0) 100%)", border: "1px solid rgba(33, 150, 243, 0.3)" }}>
                <Box display="flex" alignItems="center" mb={2}>
                    <Box sx={{ mr: 2, color: "primary.main", fontSize: 40 }}>❤️</Box>
                    <Typography variant="h5" color="text.secondary">集群健康度</Typography>
                </Box>
                <Box display="flex" alignItems="baseline">
                    <Typography variant="h1" fontWeight="bold" sx={{ mr: 2 }}>{clusterStats.avg_score?.toFixed(1) || 100}</Typography>
                    <Typography variant="h6" color="text.secondary">/ 100</Typography>
                </Box>
                <LinearProgress 
                    variant="determinate" 
                    value={clusterStats.avg_score || 100} 
                    color={clusterStats.avg_score > 80 ? "primary" : "warning"}
                    sx={{ width: "100%", mt: 3, height: 12, borderRadius: 6 }} 
                />
            </Card>
        </Grid>
        
        {/* Total Nodes */}
        <Grid item xs={12} md={6} sx={{ height: "100%" }}>
            <Card sx={{ height: "100%", display: "flex", flexDirection: "column", justifyContent: "center", p: 4, background: "linear-gradient(135deg, rgba(0, 230, 118, 0.1) 0%, rgba(0,0,0,0) 100%)", border: "1px solid rgba(0, 230, 118, 0.3)" }}>
                <Box display="flex" alignItems="center" mb={2}>
                    <Box sx={{ mr: 2, color: "success.main", fontSize: 40 }}>🖥️</Box>
                    <Typography variant="h5" color="text.secondary">总节点数监控</Typography>
                </Box>
                <Box display="flex" alignItems="baseline">
                    <Typography variant="h1" fontWeight="bold" sx={{ mr: 2 }}>
                        {clusterStats.total_servers || 0}
                    </Typography>
                    <Typography variant="h5" color="success.light">个节点</Typography>
                </Box>
                <Box mt={2}>
                     <Chip label={`${clusterStats.online_servers || 0} 在线`} color="success" sx={{ mr: 1, fontWeight: "bold" }} />
                     <Chip label={`${(clusterStats.total_servers || 0) - (clusterStats.online_servers || 0)} 离线`} color="error" variant="outlined" />
                </Box>
            </Card>
        </Grid>
    </Grid>
);

const ChartSection = ({ servers, nodeHistories, initialNode, initialMetric, onBack, fixedMode }) => {
    // If fixedMode is true, user cannot change selection
    
    const [selectedMetrics, setSelectedMetrics] = React.useState(() => ({
        cpu: initialMetric ? initialMetric === "cpu" : true,
        mem: initialMetric ? initialMetric === "mem" : false,
        disk: initialMetric ? initialMetric === "disk" : false,
        net: initialMetric ? initialMetric === "net" : false
    }));

    const [selectedNodes, setSelectedNodes] = React.useState(() => {
        const initial = {};
        if (servers.length > 0) {
            if (initialNode) {
                servers.forEach(s => initial[s.server_name] = s.server_name === initialNode);
            } else {
                servers.forEach(s => initial[s.server_name] = true);
            }
        }
        return initial;
    });

    const initRef = React.useRef(false);

    React.useEffect(() => {
        if (!initRef.current && servers.length > 0 && Object.keys(selectedNodes).length === 0) {
            const initial = {};
            if (initialNode) {
                 servers.forEach(s => initial[s.server_name] = s.server_name === initialNode);
            } else {
                 servers.forEach(s => initial[s.server_name] = true);
            }
            setSelectedNodes(initial);
            initRef.current = true;
        }
    }, [servers, initialNode]);

    const handleMetricChange = (m) => {
        if (fixedMode) return;
        setSelectedMetrics(prev => ({ ...prev, [m]: !prev[m] }));
    };
    const handleNodeChange = (n) => {
         if (fixedMode) return;
         setSelectedNodes(prev => ({ ...prev, [n]: !prev[n] }));
    };
    const selectAllNodes = (checked) => {
        if (fixedMode) return;
        const next = {};
        servers.forEach(s => next[s.server_name] = checked);
        setSelectedNodes(next);
    };

    // Construct Chart Data
    const chartData = React.useMemo(() => {
        if (!servers.length) return [];
        let maxLen = 0;
        let baseNode = "";
        servers.forEach(s => {
            const h = nodeHistories[s.server_name] || [];
            if (h.length > maxLen) { maxLen = h.length; baseNode = s.server_name; }
        });
        
        if (!baseNode) return [];
        const baseHistory = nodeHistories[baseNode] || [];
        
        return baseHistory.map((point, idx) => {
            const row = { time: point.time };
            servers.forEach(s => {
                const sHist = nodeHistories[s.server_name];
                if (sHist) {
                    const sIdx = sHist.length - (baseHistory.length - idx);
                    if (sIdx >= 0) {
                        const sPoint = sHist[sIdx];
                        if (selectedMetrics.cpu) row[`${s.server_name}_cpu`] = sPoint.cpu;
                        if (selectedMetrics.mem) row[`${s.server_name}_mem`] = sPoint.mem;
                        if (selectedMetrics.disk) row[`${s.server_name}_disk`] = sPoint.disk;
                        if (selectedMetrics.net) row[`${s.server_name}_net`] = (sPoint.net_in + sPoint.net_out) / 1024; 
                    }
                }
            });
            return row;
        });
    }, [servers, nodeHistories, selectedMetrics]);

    const colors = ["#2196f3", "#f50057", "#00e676", "#ffea00", "#ff9100", "#d500f9", "#00b0ff", "#651fff"];
    const tooltipFormatter = (value, name, item) => {
        const unit = getMetricUnitByDataKey(item?.dataKey || "");
        if (typeof value === "number") {
            return [`${value.toFixed(2)}${unit ? ` ${unit}` : ""}`, name];
        }
        return [value, name];
    };

    return (
        <Card sx={{ height: "100%", display: "flex", flexDirection: "column", p: 0, overflow: "hidden" }}>
             {/* Toolbar */}
            <Box sx={{ p: 2, borderBottom: "1px solid rgba(255,255,255,0.05)", bgcolor: "rgba(0,0,0,0.2)" }}>
                <Box display="flex" alignItems="center" mb={1}>
                    {onBack && (
                        <Button 
                            onClick={onBack} 
                            startIcon={<span>🔙</span>} 
                            sx={{ mr: 2, color: "white", borderColor: "rgba(255,255,255,0.3)" }} 
                            variant="outlined" 
                            size="small"
                        >
                            返回
                        </Button>
                    )}
                    <Typography variant="subtitle2" color="text.secondary">
                        {fixedMode ? `节点 ${initialNode} - ${initialMetric.toUpperCase()} 历史趋势` : "自定义趋势分析"}
                    </Typography>
                </Box>

                {!fixedMode && (
                    <Grid container spacing={2} alignItems="center">
                        <Grid item xs={12} lg={5}>
                            <Typography variant="caption" color="text.secondary" gutterBottom>1. 监控指标</Typography>
                            <FormGroup row>
                                <FormControlLabel control={<Checkbox checked={selectedMetrics.cpu} onChange={() => handleMetricChange("cpu")} size="small" />} label="CPU" />
                                <FormControlLabel control={<Checkbox checked={selectedMetrics.mem} onChange={() => handleMetricChange("mem")} size="small" />} label="内存" />
                                <FormControlLabel control={<Checkbox checked={selectedMetrics.disk} onChange={() => handleMetricChange("disk")} size="small" />} label="磁盘" />
                                <FormControlLabel control={<Checkbox checked={selectedMetrics.net} onChange={() => handleMetricChange("net")} size="small" />} label="网络" />
                            </FormGroup>
                        </Grid>
                        <Grid item xs={12} lg={7}>
                            <Box display="flex" justifyContent="space-between" alignItems="center">
                                <Typography variant="caption" color="text.secondary">2. 节点选择</Typography>
                                <Box>
                                    <Button size="small" onClick={() => selectAllNodes(true)}>全选</Button>
                                    <Button size="small" onClick={() => selectAllNodes(false)}>全不选</Button>
                                </Box>
                            </Box>
                            <Box sx={{ display: "flex", flexWrap: "wrap", gap: 1, maxHeight: 60, overflowY: "auto" }}>
                                {servers.map((s) => (
                                    <Chip 
                                        key={s.server_name}
                                        label={s.server_name}
                                        onClick={() => handleNodeChange(s.server_name)}
                                        color={selectedNodes[s.server_name] ? "primary" : "default"}
                                        variant={selectedNodes[s.server_name] ? "filled" : "outlined"}
                                        size="small"
                                        icon={selectedNodes[s.server_name] ? <span>✔️</span> : undefined}
                                    />
                                ))}
                            </Box>
                        </Grid>
                    </Grid>
                )}
            </Box>

            {/* Chart Area */}
            <Box sx={{ flexGrow: 1, p: 2, bgcolor: "#0e1621" }}>
                <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={chartData}>
                        <CartesianGrid strokeDasharray="3 3" stroke="#2c3e50" />
                        <XAxis dataKey="time" stroke="#546e7a" />
                        <YAxis stroke="#546e7a" />
                        <Tooltip formatter={tooltipFormatter} contentStyle={{ backgroundColor: "rgba(20, 30, 40, 0.9)", border: "1px solid #333" }} />
                        <Legend wrapperStyle={{ paddingTop: 10 }} />
                        {servers.map((s, i) => {
                            if (!selectedNodes[s.server_name]) return null;
                            // In fixed mode, force the color to always be primary
                            const color = fixedMode ? theme.palette.primary.main : colors[i % colors.length];
                            
                            const lines = [];
                            if (selectedMetrics.cpu) lines.push(<Line key={`l_${s.server_name}_cpu`} type="monotone" dataKey={`${s.server_name}_cpu`} stroke={color} name={`${s.server_name} CPU(%)`} dot={false} strokeWidth={2} />);
                            if (selectedMetrics.mem) lines.push(<Line key={`l_${s.server_name}_mem`} type="monotone" dataKey={`${s.server_name}_mem`} stroke={color}  name={`${s.server_name} MEM(%)`} dot={false} strokeWidth={2} />);
                            if (selectedMetrics.disk) lines.push(<Line key={`l_${s.server_name}_disk`} type="monotone" dataKey={`${s.server_name}_disk`} stroke={color}  name={`${s.server_name} DISK(%)`} dot={false} strokeWidth={2} />);
                            if (selectedMetrics.net) lines.push(<Line key={`l_${s.server_name}_net`} type="monotone" dataKey={`${s.server_name}_net`} stroke={color} strokeWidth={2} name={`${s.server_name} NET(MB/s)`} dot={false} />);
                            return lines;
                        })}
                    </LineChart>
                </ResponsiveContainer>
            </Box>
        </Card>
    );
};

// --- Detail Page ---

const DetailView = ({ title, metric, servers, onNodeClick }) => {
    // Sort logic
    const [sort, setSort] = React.useState("desc");
    
    const sorted = React.useMemo(() => {
        return [...servers].sort((a, b) => {
            const valA = metric.getValue(a);
            const valB = metric.getValue(b);
            return sort === "asc" ? valA - valB : valB - valA;
        });
    }, [servers, metric, sort]);

    return (
        <Box sx={{ p: 4, height: "100%", overflowY: "auto" }}>
            <Box display="flex" justifyContent="space-between" alignItems="center" mb={4}>
                <Typography variant="h4" fontWeight="bold">
                    <span style={{ marginRight: 10 }}>{metric.icon}</span> {title} 监控详情
                </Typography>
                <Button variant="outlined" startIcon={<span>🔃</span>}>刷新数据</Button>
            </Box>

            <TableContainer component={Paper} sx={{ mb: 4 }}>
                <Table>
                    <TableHead sx={{ bgcolor: "background.default" }}>
                         <TableRow>
                            <TableCell>#</TableCell>
                            <TableCell>节点名称 (点击查看图表)</TableCell>
                            <TableCell>状态</TableCell>
                            <TableCell align="right" onClick={() => setSort(s => s==="asc"?"desc":"asc")} style={{cursor:"pointer"}}>
                                当前数值{metric.unit ? ` (${metric.unit})` : ""} {sort==="asc"?"↑":"↓"}
                            </TableCell>
                            <TableCell>趋势可视化</TableCell>
                            <TableCell>其他相关指标</TableCell>
                        </TableRow>
                    </TableHead>
                    <TableBody>
                        {sorted.map((s, idx) => {
                            const val = metric.getValue(s);
                            const displayVal = metric.format ? metric.format(val) : val.toFixed(1);
                            
                            return (
                                <TableRow 
                                    key={s.server_name} 
                                    hover 
                                    onClick={() => onNodeClick(s.server_name)}
                                    sx={{ cursor: "pointer" }}
                                >
                                    <TableCell>{idx + 1}</TableCell>
                                    <TableCell component="th" scope="row" sx={{ fontWeight: "bold", textDecoration: "underline", color: "primary.main" }}>
                                        {s.server_name}
                                    </TableCell>
                                    <TableCell>
                                        <Chip 
                                            size="small" 
                                            label={s.status === "0" || s.status === "ONLINE" ? "在线" : "离线"} 
                                            color={s.status === "0" || s.status === "ONLINE" ? "success" : "default"} 
                                            variant="outlined"
                                        />
                                    </TableCell>
                                    <TableCell align="right" sx={{ fontSize: "1.1rem", fontFamily: "monospace", color: "primary.main" }}>
                                        {displayVal}
                                    </TableCell>
                                    <TableCell sx={{ width: "25%" }}>
                                        <LinearProgress 
                                            variant="determinate" 
                                            value={Math.min( metric.normalize ? metric.normalize(val) : val, 100)} 
                                            color={val > 80 ? "error" : val > 50 ? "warning" : "success"}
                                            sx={{ height: 8, borderRadius: 4 }}
                                        />
                                    </TableCell>
                                    <TableCell>
                                        <Box display="flex" gap={1}>
                                            <Chip size="small" label={`Load(1m): ${s.load_avg_1?.toFixed(2)}`} />
                                            {metric.key !== "mem_used_percent" && <Chip size="small" label={`Mem: ${s.mem_used_percent?.toFixed(0)}%`} />}
                                        </Box>
                                    </TableCell>
                                </TableRow>
                            );
                        })}
                    </TableBody>
                </Table>
            </TableContainer>
        </Box>
    );
};

const formatCellValue = (value, unit = "") => {
    if (value === null || value === undefined) return "-";
    if (typeof value === "number") {
        const formatted = Number.isInteger(value) ? String(value) : value.toFixed(3);
        return unit ? `${formatted} ${unit}` : formatted;
    }
    if (typeof value === "object") {
        if (value.seconds !== undefined) {
            return new Date(Number(value.seconds) * 1000).toLocaleString("zh-CN", { hour12: false });
        }
        return JSON.stringify(value);
    }
    return unit ? `${String(value)} ${unit}` : String(value);
};

const QueryTableView = ({ title, subtitle, rows, loading, error, emptyText }) => {
    const columns = React.useMemo(() => {
        if (!rows || rows.length === 0) return [];
        return Object.keys(rows[0]);
    }, [rows]);

    return (
        <Box sx={{ p: 4, height: "100%", overflowY: "auto" }}>
            <Typography variant="h4" fontWeight="bold" mb={1}>{title}</Typography>
            {subtitle ? <Typography variant="body2" color="text.secondary" mb={2}>{subtitle}</Typography> : null}

            {loading && <LinearProgress sx={{ mb: 2 }} />}
            {error ? <Typography color="error" mb={2}>{error}</Typography> : null}

            {!loading && (!rows || rows.length === 0) ? (
                <Paper sx={{ p: 3 }}>
                    <Typography color="text.secondary">{emptyText || "暂无数据"}</Typography>
                </Paper>
            ) : (
                <TableContainer component={Paper}>
                    <Table size="small">
                        <TableHead sx={{ bgcolor: "background.default" }}>
                            <TableRow>
                                <TableCell>#</TableCell>
                                {columns.map((c) => (
                                    <TableCell key={c}>{getUnitByField(c) ? `${getFieldLabel(c)} (${getUnitByField(c)})` : getFieldLabel(c)}</TableCell>
                                ))}
                            </TableRow>
                        </TableHead>
                        <TableBody>
                            {rows.map((row, idx) => (
                                <TableRow key={idx} hover>
                                    <TableCell>{idx + 1}</TableCell>
                                    {columns.map((c) => (
                                        <TableCell key={`${idx}_${c}`} sx={{ fontFamily: c.includes("rate") || c.includes("percent") ? "monospace" : "inherit" }}>
                                            {formatCellValue(row[c], getUnitByField(c))}
                                        </TableCell>
                                    ))}
                                </TableRow>
                            ))}
                        </TableBody>
                    </Table>
                </TableContainer>
            )}
        </Box>
    );
};

const QueryControlBar = ({
    servers,
    selectedServer,
    onServerChange,
    scoringProfile,
    onScoringProfileChange,
    hours,
    onHoursChange,
    intervalSeconds,
    onIntervalChange,
    showInterval,
    showAnomalyThresholds,
    anomalyThresholds,
    onAnomalyThresholdChange,
    onRefresh
}) => (
    <Paper sx={{ p: 2, mb: 2, display: "flex", flexWrap: "wrap", gap: 2, alignItems: "center" }}>
        <Typography variant="body2" color="text.secondary">节点</Typography>
        <Select
            size="small"
            value={selectedServer}
            onChange={(e) => onServerChange(e.target.value)}
            sx={{ minWidth: 220 }}
        >
            {servers.map((s) => (
                <MenuItem key={s.server_name} value={s.server_name}>{s.server_name}</MenuItem>
            ))}
        </Select>

        <Typography variant="body2" color="text.secondary">评分体系</Typography>
        <Select
            size="small"
            value={scoringProfile}
            onChange={(e) => onScoringProfileChange(e.target.value)}
            sx={{ minWidth: 180 }}
        >
            {SCORING_PROFILES.map((p) => (
                <MenuItem key={p.value} value={p.value}>{p.label}</MenuItem>
            ))}
        </Select>

        <Typography variant="body2" color="text.secondary">时间窗口</Typography>
        <Select size="small" value={hours} onChange={(e) => onHoursChange(Number(e.target.value))}>
            <MenuItem value={1}>1 小时</MenuItem>
            <MenuItem value={6}>6 小时</MenuItem>
            <MenuItem value={24}>24 小时</MenuItem>
            <MenuItem value={72}>72 小时</MenuItem>
        </Select>

        {showInterval ? (
            <>
                <Typography variant="body2" color="text.secondary">聚合间隔</Typography>
                <Select size="small" value={intervalSeconds} onChange={(e) => onIntervalChange(Number(e.target.value))}>
                    <MenuItem value={60}>60 秒</MenuItem>
                    <MenuItem value={300}>300 秒</MenuItem>
                    <MenuItem value={600}>600 秒</MenuItem>
                    <MenuItem value={1800}>1800 秒</MenuItem>
                </Select>
            </>
        ) : null}

        {showAnomalyThresholds ? (
            <>
                <TextField
                    size="small"
                    type="number"
                    label="CPU阈值(%)"
                    value={anomalyThresholds.cpu_threshold}
                    onChange={(e) => onAnomalyThresholdChange("cpu_threshold", Number(e.target.value))}
                    inputProps={{ min: 1, max: 100, step: 1 }}
                    sx={{ width: 130 }}
                />
                <TextField
                    size="small"
                    type="number"
                    label="内存阈值(%)"
                    value={anomalyThresholds.mem_threshold}
                    onChange={(e) => onAnomalyThresholdChange("mem_threshold", Number(e.target.value))}
                    inputProps={{ min: 1, max: 100, step: 1 }}
                    sx={{ width: 130 }}
                />
                <TextField
                    size="small"
                    type="number"
                    label="磁盘阈值(%)"
                    value={anomalyThresholds.disk_threshold}
                    onChange={(e) => onAnomalyThresholdChange("disk_threshold", Number(e.target.value))}
                    inputProps={{ min: 1, max: 100, step: 1 }}
                    sx={{ width: 130 }}
                />
                <TextField
                    size="small"
                    type="number"
                    label="变化率阈值"
                    value={anomalyThresholds.change_rate_threshold}
                    onChange={(e) => onAnomalyThresholdChange("change_rate_threshold", Number(e.target.value))}
                    inputProps={{ min: 0.01, step: 0.01 }}
                    sx={{ width: 140 }}
                />
            </>
        ) : null}

        <Button variant="outlined" onClick={onRefresh}>刷新</Button>
    </Paper>
);

const formatTsLabel = (timestamp) => {
    if (!timestamp || typeof timestamp !== "object") return "-";
    const seconds = Number(timestamp.seconds || 0);
    if (!Number.isFinite(seconds) || seconds <= 0) return "-";
    return new Date(seconds * 1000).toLocaleString("zh-CN", { hour12: false });
};

const aggregateFlowRows = (records, mode) => {
    const map = new Map();
    records.forEach((row) => {
        const ts = Number(row?.timestamp?.seconds || 0);
        if (!ts) return;
        const key = String(ts);
        const prev = map.get(key) || { ts, input: 0, output: 0 };
        if (mode === "net") {
            prev.input += Number(row.rcv_bytes_rate || 0);
            prev.output += Number(row.snd_bytes_rate || 0);
        } else {
            prev.input += Number(row.read_bytes_per_sec || 0);
            prev.output += Number(row.write_bytes_per_sec || 0);
        }
        map.set(key, prev);
    });

    return [...map.values()]
        .sort((a, b) => a.ts - b.ts)
        .map((r) => {
            const inputMB = r.input / 1024 / 1024;
            const outputMB = r.output / 1024 / 1024;
            return {
                time: new Date(r.ts * 1000).toLocaleTimeString("zh-CN", { hour12: false }),
                timestampLabel: new Date(r.ts * 1000).toLocaleString("zh-CN", { hour12: false }),
                input: inputMB,
                output: outputMB,
                total: inputMB + outputMB
            };
        });
};

const percentile = (values, p) => {
    if (!values || values.length === 0) return 0;
    const sorted = [...values].sort((a, b) => a - b);
    const idx = Math.min(sorted.length - 1, Math.max(0, Math.floor((sorted.length - 1) * p)));
    return Number(sorted[idx] || 0);
};

const FlowTripleMonitorView = ({
    mode,
    selectedServer,
    rows,
    loading,
    error
}) => {
    const [alertsOnly, setAlertsOnly] = React.useState(false);
    const aggregated = React.useMemo(() => aggregateFlowRows(rows || [], mode), [rows, mode]);

    const thresholds = React.useMemo(() => {
        const build = (key) => {
            const vals = aggregated.map((r) => Number(r[key] || 0));
            return {
                warn: percentile(vals, 0.90),
                critical: percentile(vals, 0.98)
            };
        };
        return {
            input: build("input"),
            output: build("output"),
            total: build("total")
        };
    }, [aggregated]);

    const title = mode === "net" ? "🌐 网络流量监控详情" : "💿 磁盘IO监控详情";
    const subtitle = mode === "net"
        ? "展示输入流量、输出流量、总流量（聚合所有网卡）"
        : "展示读流量、写流量、总流量（聚合所有磁盘）";

    const sections = [
        { key: "input", title: mode === "net" ? "输入流量趋势" : "读流量趋势", color: "#2196f3" },
        { key: "output", title: mode === "net" ? "输出流量趋势" : "写流量趋势", color: "#f50057" },
        { key: "total", title: "总流量趋势", color: "#00e676" }
    ];

    const getSeverity = React.useCallback((sectionKey, value) => {
        const t = thresholds[sectionKey] || { warn: 0, critical: 0 };
        if (value >= t.critical && t.critical > 0) return "critical";
        if (value >= t.warn && t.warn > 0) return "warning";
        return "normal";
    }, [thresholds]);

    return (
        <Box sx={{ p: 4, height: "100%", overflowY: "auto" }}>
            <Box display="flex" justifyContent="space-between" alignItems="center" mb={1}>
                <Typography variant="h4" fontWeight="bold">{title}</Typography>
                <FormControlLabel
                    control={<Checkbox checked={alertsOnly} onChange={(e) => setAlertsOnly(e.target.checked)} size="small" />}
                    label="仅看告警"
                />
            </Box>
            <Typography variant="body2" color="text.secondary" mb={2}>{selectedServer || "-"} ｜ {subtitle}</Typography>

            {loading ? <LinearProgress sx={{ mb: 2 }} /> : null}
            {error ? <Typography color="error" mb={2}>{error}</Typography> : null}

            {!loading && aggregated.length === 0 ? (
                <Paper sx={{ p: 3 }}>
                    <Typography color="text.secondary">当前时间窗口暂无可展示的流量数据</Typography>
                </Paper>
            ) : (
                <Box sx={{ display: "grid", gap: 3 }}>
                    {sections.map((section) => (
                        <Card key={section.key} sx={{ p: 2 }}>
                            {(() => {
                                const latestRows = [...aggregated].reverse().slice(0, 20);
                                const criticalCount = latestRows.filter((r) => getSeverity(section.key, Number(r[section.key] || 0)) === "critical").length;
                                const warningCount = latestRows.filter((r) => getSeverity(section.key, Number(r[section.key] || 0)) === "warning").length;
                                return (
                            <Box display="flex" justifyContent="space-between" alignItems="center" mb={1.5}>
                                <Typography variant="h6">{section.title} (MB/s)</Typography>
                                <Box display="flex" gap={1}>
                                    <Chip size="small" color="error" variant="outlined" label={`严重: ${criticalCount}`} />
                                    <Chip size="small" color="warning" variant="outlined" label={`预警: ${warningCount}`} />
                                    <Chip size="small" color="warning" label={`P90: ${thresholds[section.key].warn.toFixed(3)}`} />
                                    <Chip size="small" color="error" label={`P98: ${thresholds[section.key].critical.toFixed(3)}`} />
                                </Box>
                            </Box>
                                );
                            })()}
                            <Box sx={{ height: 260, mb: 2 }}>
                                <ResponsiveContainer width="100%" height="100%">
                                    <LineChart data={aggregated}>
                                        <CartesianGrid strokeDasharray="3 3" stroke="#2c3e50" />
                                        <XAxis dataKey="time" stroke="#546e7a" />
                                        <YAxis stroke="#546e7a" />
                                        <Tooltip formatter={(v) => [`${Number(v || 0).toFixed(3)} MB/s`, section.title]} contentStyle={{ backgroundColor: "rgba(20, 30, 40, 0.9)", border: "1px solid #333" }} />
                                        <Legend />
                                        <ReferenceLine y={thresholds[section.key].warn} stroke="#ffea00" strokeDasharray="5 5" label={{ value: "P90", position: "right", fill: "#ffea00" }} />
                                        <ReferenceLine y={thresholds[section.key].critical} stroke="#ff1744" strokeDasharray="5 5" label={{ value: "P98", position: "right", fill: "#ff1744" }} />
                                        <Line type="monotone" dataKey={section.key} stroke={section.color} dot={false} strokeWidth={2} name={section.title} />
                                    </LineChart>
                                </ResponsiveContainer>
                            </Box>

                            <TableContainer component={Paper}>
                                <Table size="small">
                                    <TableHead sx={{ bgcolor: "background.default" }}>
                                        <TableRow>
                                            <TableCell>#</TableCell>
                                            <TableCell>时间</TableCell>
                                            <TableCell align="right">{section.title} (MB/s)</TableCell>
                                        </TableRow>
                                    </TableHead>
                                    <TableBody>
                                        {[...aggregated].reverse().slice(0, 20).filter((row) => {
                                            if (!alertsOnly) return true;
                                            const value = Number(row[section.key] || 0);
                                            return getSeverity(section.key, value) !== "normal";
                                        }).map((row, idx) => {
                                            const value = Number(row[section.key] || 0);
                                            const severity = getSeverity(section.key, value);
                                            const isCritical = severity === "critical";
                                            const isWarning = severity === "warning";
                                            return (
                                            <TableRow
                                                key={`${section.key}_${row.timestampLabel}_${idx}`}
                                                hover
                                                sx={isCritical
                                                    ? { backgroundColor: "rgba(255, 23, 68, 0.12)" }
                                                    : isWarning
                                                        ? { backgroundColor: "rgba(255, 234, 0, 0.10)" }
                                                        : undefined}
                                            >
                                                <TableCell>{idx + 1}</TableCell>
                                                <TableCell>{row.timestampLabel}</TableCell>
                                                <TableCell align="right" sx={{ fontFamily: "monospace", color: isCritical ? "error.main" : (isWarning ? "warning.main" : "text.primary") }}>
                                                    {value.toFixed(3)}
                                                </TableCell>
                                            </TableRow>
                                        )})}
                                    </TableBody>
                                </Table>
                            </TableContainer>
                        </Card>
                    ))}
                </Box>
            )}
        </Box>
    );
};

const CpuCoreBarsView = ({
    servers,
    selectedServer,
    onServerChange,
    scoringProfile,
    onScoringProfileChange,
    hours,
    onHoursChange,
    onRefresh,
    cpuCoreRows,
    cpuCoreLoading,
    cpuCoreError,
    nodeHistories
}) => {
    const formatCpuLabel = React.useCallback((cpuName = "") => {
        const text = String(cpuName).trim().toLowerCase();
        const match = text.match(/^cpu(\d+)$/);
        if (match) {
            return `CPU ${match[1]}`;
        }
        if (text === "cpu") {
            return "CPU总览";
        }
        return String(cpuName).toUpperCase();
    }, []);

    const selectedServerInfo = React.useMemo(
        () => servers.find((s) => s.server_name === selectedServer) || null,
        [servers, selectedServer]
    );

    const coreRows = React.useMemo(() => {
        const rows = (cpuCoreRows || []).filter((r) => r.server_name === selectedServer);
        const cpuOrder = (name = "") => {
            const match = name.match(/^cpu(\d+)$/i);
            if (!match) return Number.MAX_SAFE_INTEGER;
            return Number(match[1]);
        };
        return rows.sort((a, b) => {
            const da = cpuOrder(a.cpu_name);
            const db = cpuOrder(b.cpu_name);
            if (da !== db) return da - db;
            return String(a.cpu_name).localeCompare(String(b.cpu_name));
        });
    }, [cpuCoreRows, selectedServer]);

    const historyData = React.useMemo(() => {
        const history = nodeHistories[selectedServer] || [];
        return history.map((p) => ({ time: p.time, cpu: p.cpu }));
    }, [nodeHistories, selectedServer]);

    return (
        <Box sx={{ p: 4, height: "100%", overflowY: "auto" }}>
            <Typography variant="h4" fontWeight="bold" mb={1}>⚡ CPU 负载监控详情</Typography>
            <Typography variant="body2" color="text.secondary" mb={2}>
                上方展示每个 CPU 核心当前负载（柱状），下方展示节点总 CPU 负载曲线
            </Typography>

            <QueryControlBar
                servers={servers}
                selectedServer={selectedServer}
                onServerChange={onServerChange}
                scoringProfile={scoringProfile}
                onScoringProfileChange={onScoringProfileChange}
                hours={hours}
                onHoursChange={onHoursChange}
                intervalSeconds={300}
                onIntervalChange={() => {}}
                showInterval={false}
                showAnomalyThresholds={false}
                anomalyThresholds={{}}
                onAnomalyThresholdChange={() => {}}
                onRefresh={onRefresh}
            />

            <Card sx={{ p: 3, mb: 3 }}>
                <Box display="flex" justifyContent="space-between" alignItems="center" mb={2}>
                    <Typography variant="h6">{selectedServer || "-"} 核心负载</Typography>
                    <Chip
                        size="small"
                        color="primary"
                        label={`总CPU: ${selectedServerInfo ? selectedServerInfo.cpu_percent.toFixed(1) : "0.0"}%`}
                    />
                </Box>

                {cpuCoreLoading && coreRows.length === 0 ? <LinearProgress sx={{ mb: 2 }} /> : null}
                {cpuCoreError ? <Typography color="error" mb={2}>{cpuCoreError}</Typography> : null}

                {!cpuCoreLoading && coreRows.length === 0 ? (
                    <Typography color="text.secondary">当前节点在该时间窗口没有 CPU 核心明细数据</Typography>
                ) : (
                    <Box sx={{ display: "grid", gridTemplateColumns: { xs: "1fr", md: "1fr 1fr" }, gap: 1.5 }}>
                        {coreRows.map((row, idx) => {
                            const percent = Math.max(0, Math.min(100, Number(row.cpu_percent) || 0));
                            const color = percent > 80 ? "error" : percent > 50 ? "warning" : "success";
                            return (
                                <Box key={`${row.server_name}_${row.cpu_name}_${idx}`} sx={{ display: "flex", alignItems: "center", gap: 1.5 }}>
                                    <Typography sx={{ width: 52, fontFamily: "monospace", color: "text.secondary" }}>
                                        {formatCpuLabel(row.cpu_name)}
                                    </Typography>
                                    <Box sx={{ flexGrow: 1 }}>
                                        <LinearProgress
                                            variant="determinate"
                                            value={percent}
                                            color={color}
                                            sx={{ height: 10, borderRadius: 5 }}
                                        />
                                    </Box>
                                    <Typography sx={{ width: 56, textAlign: "right", fontFamily: "monospace", color: "primary.main" }}>
                                        {percent.toFixed(1)}%
                                    </Typography>
                                </Box>
                            );
                        })}
                    </Box>
                )}
            </Card>

            <Card sx={{ p: 3, mb: 3 }}>
                <Typography variant="h6" mb={1.5}>每核关键指标矩阵（性能分析重点）</Typography>
                <Typography variant="body2" color="text.secondary" mb={2}>
                    每个核心展示总使用率、用户态、内核态、IO等待、软中断、空闲占比，便于快速定位瓶颈类型。
                </Typography>

                {cpuCoreLoading && coreRows.length === 0 ? <LinearProgress sx={{ mb: 2 }} /> : null}
                {!cpuCoreLoading && coreRows.length === 0 ? (
                    <Typography color="text.secondary">当前没有可用的核心状态数据</Typography>
                ) : (
                    <TableContainer component={Paper}>
                        <Table size="small">
                            <TableHead sx={{ bgcolor: "background.default" }}>
                                <TableRow>
                                    <TableCell>CPU核心</TableCell>
                                    <TableCell align="right">总使用率(%)</TableCell>
                                    <TableCell align="right">用户态(%)</TableCell>
                                    <TableCell align="right">内核态(%)</TableCell>
                                    <TableCell align="right">IO等待(%)</TableCell>
                                    <TableCell align="right">软中断(%)</TableCell>
                                    <TableCell align="right">空闲(%)</TableCell>
                                </TableRow>
                            </TableHead>
                            <TableBody>
                                {coreRows.map((row, idx) => (
                                    <TableRow key={`${row.server_name}_${row.cpu_name}_metric_${idx}`} hover>
                                        <TableCell sx={{ fontFamily: "monospace" }}>{formatCpuLabel(row.cpu_name)}</TableCell>
                                        <TableCell align="right" sx={{ color: "primary.main", fontFamily: "monospace" }}>{Number(row.cpu_percent || 0).toFixed(1)}</TableCell>
                                        <TableCell align="right" sx={{ fontFamily: "monospace" }}>{Number(row.usr_percent || 0).toFixed(1)}</TableCell>
                                        <TableCell align="right" sx={{ fontFamily: "monospace" }}>{Number(row.system_percent || 0).toFixed(1)}</TableCell>
                                        <TableCell align="right" sx={{ fontFamily: "monospace" }}>{Number(row.io_wait_percent || 0).toFixed(1)}</TableCell>
                                        <TableCell align="right" sx={{ fontFamily: "monospace" }}>{Number(row.soft_irq_percent || 0).toFixed(1)}</TableCell>
                                        <TableCell align="right" sx={{ color: "success.main", fontFamily: "monospace" }}>{Number(row.idle_percent || 0).toFixed(1)}</TableCell>
                                    </TableRow>
                                ))}
                            </TableBody>
                        </Table>
                    </TableContainer>
                )}
            </Card>

            <Card sx={{ p: 2 }}>
                <Typography variant="h6" mb={1.5}>总CPU负载曲线</Typography>
                <Box sx={{ height: 320 }}>
                    <ResponsiveContainer width="100%" height="100%">
                        <AreaChart data={historyData}>
                            <CartesianGrid strokeDasharray="3 3" stroke="#2c3e50" />
                            <XAxis dataKey="time" stroke="#546e7a" />
                            <YAxis stroke="#546e7a" domain={[0, 100]} />
                            <Tooltip formatter={(v) => [`${Number(v || 0).toFixed(2)} %`, "CPU"]} contentStyle={{ backgroundColor: "rgba(20, 30, 40, 0.9)", border: "1px solid #333" }} />
                            <Legend />
                            <Area type="monotone" dataKey="cpu" name={`${selectedServer || "节点"} 总CPU(%)`} stroke="#2196f3" fill="#2196f3" fillOpacity={0.25} />
                        </AreaChart>
                    </ResponsiveContainer>
                </Box>
            </Card>
        </Box>
    );
};

const EndpointView = ({ endpointData }) => {
    const endpoints = endpointData?.endpoints || [];

    return (
        <Box sx={{ p: 4, height: "100%", overflowY: "auto" }}>
            <Typography variant="h4" fontWeight="bold" mb={1}>🔌 查询端口总览</Typography>
            <Typography variant="body2" color="text.secondary" mb={3}>
                Manager 地址：{endpointData?.manager_address || "-"} ｜ Dashboard 地址：{endpointData?.dashboard_address || "-"}
            </Typography>

            <TableContainer component={Paper}>
                <Table>
                    <TableHead sx={{ bgcolor: "background.default" }}>
                        <TableRow>
                            <TableCell>#</TableCell>
                            <TableCell>类型</TableCell>
                            <TableCell>方法</TableCell>
                            <TableCell>查询端口 / 接口</TableCell>
                            <TableCell>目标地址</TableCell>
                            <TableCell>说明</TableCell>
                        </TableRow>
                    </TableHead>
                    <TableBody>
                        {endpoints.map((ep, idx) => (
                            <TableRow key={`${ep.type}_${ep.path}_${idx}`} hover>
                                <TableCell>{idx + 1}</TableCell>
                                <TableCell>
                                    <Chip
                                        size="small"
                                        label={ep.type}
                                        color={ep.type === "gRPC" ? "secondary" : ep.type === "HTTP" ? "primary" : "success"}
                                        variant="outlined"
                                    />
                                </TableCell>
                                <TableCell sx={{ fontFamily: "monospace" }}>{ep.method}</TableCell>
                                <TableCell sx={{ fontFamily: "monospace", color: "primary.main" }}>{ep.path}</TableCell>
                                <TableCell sx={{ fontFamily: "monospace" }}>{ep.target}</TableCell>
                                <TableCell>{ep.description}</TableCell>
                            </TableRow>
                        ))}
                    </TableBody>
                </Table>
            </TableContainer>
        </Box>
    );
};

// --- Main App Shell ---

const AppShell = () => {
    const [view, setView] = React.useState("dashboard"); // dashboard | endpoints | rank | trend | anomaly | net_detail | disk_detail | mem_detail | softirq_detail | cpu | mem | disk | net
    const [subViewNode, setSubViewNode] = React.useState(null); // If set, shows chart for this node inside the current view
    
    const [data, setData] = React.useState({ servers: [], cluster_stats: {} });
    const [scoringProfile, setScoringProfile] = React.useState("BALANCED");
    const [histories, setHistories] = React.useState({});
    const [queryEndpointData, setQueryEndpointData] = React.useState({ endpoints: [] });
    const [selectedQueryServer, setSelectedQueryServer] = React.useState("");
    const [queryHours, setQueryHours] = React.useState(24);
    const [trendIntervalSeconds, setTrendIntervalSeconds] = React.useState(300);
    const [queryRows, setQueryRows] = React.useState({ rank: [], trend: [], anomaly: [], net_detail: [], disk_detail: [], mem_detail: [], softirq_detail: [] });
    const [queryLoading, setQueryLoading] = React.useState({});
    const [queryError, setQueryError] = React.useState({});
    const [cpuCoreRows, setCpuCoreRows] = React.useState([]);
    const [cpuCoreLoading, setCpuCoreLoading] = React.useState(false);
    const [cpuCoreError, setCpuCoreError] = React.useState("");
    const [anomalyThresholds, setAnomalyThresholds] = React.useState({
        cpu_threshold: 80,
        mem_threshold: 90,
        disk_threshold: 85,
        change_rate_threshold: 0.5
    });
    const scoringProfileRef = React.useRef("BALANCED");

    React.useEffect(() => {
        scoringProfileRef.current = scoringProfile;
    }, [scoringProfile]);

    React.useEffect(() => {
        // Init fetch
        axios.get("/api/overview", { params: { scoring_profile: scoringProfileRef.current } })
            .then(res => setData(applyScoringProfileToOverview(res.data, scoringProfileRef.current)))
            .catch(console.error);
        axios.get("/api/query-endpoints").then(res => setQueryEndpointData(res.data)).catch(console.error);

        const socket = io();
        socket.on("overview_update", (newData) => {
            const activeProfile = scoringProfileRef.current;
            setData(applyScoringProfileToOverview(newData, activeProfile));
            // Append history
            const now = new Date().toLocaleTimeString("zh-CN", {hour12: false});
            setHistories(prev => {
                const next = { ...prev };
                newData.servers.forEach(s => {
                    const h = next[s.server_name] || [];
                    const p = {
                        time: now,
                        cpu: s.cpu_percent,
                        mem: s.mem_used_percent,
                        disk: s.disk_util_percent,
                        net_in: s.rcv_rate,
                        net_out: s.send_rate
                    };
                    // Keep last 60 points
                    next[s.server_name] = [...h, p].slice(-60);
                });
                return next;
            });
        });
        return () => socket.disconnect();
    }, []);

    React.useEffect(() => {
        axios.get("/api/overview", { params: { scoring_profile: scoringProfile } })
            .then(res => setData(applyScoringProfileToOverview(res.data, scoringProfile)))
            .catch(console.error);
    }, [scoringProfile]);

    React.useEffect(() => {
        if (!selectedQueryServer && data.servers.length > 0) {
            setSelectedQueryServer(data.servers[0].server_name);
        }
    }, [data.servers, selectedQueryServer]);

    const loadQueryViewData = React.useCallback(async (targetView) => {
        const needServerViews = ["trend", "net_detail", "disk_detail", "mem_detail", "softirq_detail"];
        if (needServerViews.includes(targetView) && !selectedQueryServer) return;

        setQueryLoading(prev => ({ ...prev, [targetView]: true }));
        setQueryError(prev => ({ ...prev, [targetView]: "" }));

        try {
            if (targetView === "rank") {
                const res = await axios.get("/api/rank", {
                    params: { page: 1, page_size: 50, order: "DESC", scoring_profile: scoringProfile }
                });
                setQueryRows(prev => ({ ...prev, rank: res.data.servers || [] }));
                return;
            }

            if (targetView === "trend") {
                const res = await axios.get(`/api/trend/${encodeURIComponent(selectedQueryServer)}`, {
                    params: {
                        hours: queryHours,
                        interval_seconds: trendIntervalSeconds,
                        scoring_profile: scoringProfile
                    }
                });
                setQueryRows(prev => ({ ...prev, trend: res.data.records || [] }));
                return;
            }

            if (targetView === "anomaly") {
                const res = await axios.get("/api/anomaly", {
                    params: {
                        server_name: selectedQueryServer,
                        hours: queryHours,
                        cpu_threshold: anomalyThresholds.cpu_threshold,
                        mem_threshold: anomalyThresholds.mem_threshold,
                        disk_threshold: anomalyThresholds.disk_threshold,
                        change_rate_threshold: anomalyThresholds.change_rate_threshold,
                        page: 1,
                        page_size: 100
                    }
                });
                setQueryRows(prev => ({ ...prev, anomaly: res.data.anomalies || [] }));
                return;
            }

            const endpointMap = {
                net_detail: "/api/net-detail",
                disk_detail: "/api/disk-detail",
                mem_detail: "/api/mem-detail",
                softirq_detail: "/api/softirq-detail"
            };

            if (endpointMap[targetView]) {
                const res = await axios.get(endpointMap[targetView], {
                    params: {
                        server_name: selectedQueryServer,
                        hours: queryHours,
                        page: 1,
                        page_size: 100
                    }
                });
                setQueryRows(prev => ({ ...prev, [targetView]: res.data.records || [] }));
            }
        } catch (err) {
            setQueryError(prev => ({ ...prev, [targetView]: err?.response?.data?.error || err.message || "查询失败" }));
        } finally {
            setQueryLoading(prev => ({ ...prev, [targetView]: false }));
        }
    }, [selectedQueryServer, queryHours, trendIntervalSeconds, anomalyThresholds, scoringProfile]);

    const handleAnomalyThresholdChange = (key, value) => {
        const nextValue = Number.isFinite(value) ? value : 0;
        setAnomalyThresholds(prev => ({ ...prev, [key]: nextValue }));
    };

    const loadCpuCoreDetails = React.useCallback(async (options = {}) => {
        const { silent = false } = options;
        if (!silent) {
            setCpuCoreLoading(true);
        }
        setCpuCoreError("");
        try {
            const res = await axios.get("/api/cpu-core-detail", {
                params: {
                    server_name: selectedQueryServer,
                    hours: queryHours
                }
            });
            setCpuCoreRows(res.data.records || []);
        } catch (err) {
            setCpuCoreError(err?.response?.data?.error || err.message || "CPU核心负载查询失败");
        } finally {
            if (!silent) {
                setCpuCoreLoading(false);
            }
        }
    }, [queryHours, selectedQueryServer]);

    React.useEffect(() => {
        const queryViews = ["rank", "trend", "anomaly", "net_detail", "disk_detail", "mem_detail", "softirq_detail"];
        if (queryViews.includes(view)) {
            loadQueryViewData(view);
        }
    }, [view, loadQueryViewData]);

    React.useEffect(() => {
        if (view === "net") {
            loadQueryViewData("net_detail");
        } else if (view === "disk") {
            loadQueryViewData("disk_detail");
        }
    }, [view, selectedQueryServer, queryHours, loadQueryViewData]);

    React.useEffect(() => {
        if (view === "cpu") {
            loadCpuCoreDetails();
        }
    }, [view, loadCpuCoreDetails]);

    React.useEffect(() => {
        if (view !== "cpu") {
            return;
        }

        const timer = setInterval(() => {
            loadCpuCoreDetails({ silent: true });
        }, 2000);

        return () => clearInterval(timer);
    }, [view, loadCpuCoreDetails]);

    const sidebarItems = [
        { id: "dashboard", label: "总览看板", icon: "📊" },
        { id: "endpoints", label: "查询端口", icon: "🔌" },
        { divider: true },
        { id: "header_query", label: "查询功能项" },
        { id: "rank", label: "评分排行", icon: "🏆" },
        { id: "trend", label: "趋势聚合", icon: "📈" },
        { id: "anomaly", label: "异常查询", icon: "🚨" },
        { id: "net_detail", label: "网络详情", icon: "🛰️" },
        { id: "disk_detail", label: "磁盘详情", icon: "📀" },
        { id: "mem_detail", label: "内存详情", icon: "🧠" },
        { id: "softirq_detail", label: "软中断详情", icon: "🧩" },
        { divider: true },
        { id: "header", label: "细分监控项" },
        { id: "cpu", label: "CPU 负载", icon: "⚡" },
        { id: "mem", label: "内存使用", icon: "💾" },
        { id: "disk", label: "磁盘 IO", icon: "💿" },
        { id: "net", label: "网络流量", icon: "🌐" },
    ];

    const handleViewChange = (newView) => {
        setView(newView);
        setSubViewNode(null); // Reset sub-view on main nav change
    };

    const handleNodeClick = (nodeName) => {
        setSubViewNode(nodeName);
    };

    const renderContent = () => {
        // If we are drill-down into a node chart
        if (subViewNode) {
             return (
                <Box sx={{ height: "100%", p: 3 }}>
                    <ChartSection 
                        servers={data.servers} 
                        nodeHistories={histories} 
                        initialNode={subViewNode}
                        initialMetric={view === "dashboard" ? "cpu" : view} // pass current view type as initial metric
                        onBack={() => setSubViewNode(null)}
                        fixedMode={true}
                    />
                </Box>
             );
        }

        if (view === "dashboard") {
            return (
                <Box sx={{ height: "100%", display: "flex", flexDirection: "column", p: 3, gap: 3 }}>
                    <Paper sx={{ p: 2, display: "flex", alignItems: "center", gap: 2 }}>
                        <Typography variant="body2" color="text.secondary">评分体系</Typography>
                        <Select
                            size="small"
                            value={scoringProfile}
                            onChange={(e) => setScoringProfile(e.target.value)}
                            sx={{ minWidth: 180 }}
                        >
                            {SCORING_PROFILES.map((p) => (
                                <MenuItem key={p.value} value={p.value}>{p.label}</MenuItem>
                            ))}
                        </Select>
                        <Typography variant="caption" color="text.secondary">
                            当前总览分数按所选业务场景重算
                        </Typography>
                    </Paper>
                    <Box sx={{ height: "30%", minHeight: 200 }}>
                        <TopStats clusterStats={data.cluster_stats} />
                    </Box>
                    <Box sx={{ flexGrow: 1, minHeight: 400 }}>
                        <ChartSection servers={data.servers} nodeHistories={histories} />
                    </Box>
                </Box>
            );
        }

        if (view === "endpoints") {
            return <EndpointView endpointData={queryEndpointData} />;
        }

        if (["rank", "trend", "anomaly", "net_detail", "disk_detail", "mem_detail", "softirq_detail"].includes(view)) {
            const queryTitleMap = {
                rank: "🏆 评分排行查询",
                trend: "📈 趋势聚合查询",
                anomaly: "🚨 异常记录查询",
                net_detail: "🛰️ 网络详情查询",
                disk_detail: "📀 磁盘详情查询",
                mem_detail: "🧠 内存详情查询",
                softirq_detail: "🧩 软中断详情查询"
            };

            return (
                <Box sx={{ p: 4, height: "100%", overflowY: "auto" }}>
                    <Typography variant="h4" fontWeight="bold" mb={1}>{queryTitleMap[view]}</Typography>
                    <Typography variant="body2" color="text.secondary" mb={2}>
                        当前视图调用后端查询接口并展示原始查询结果
                    </Typography>

                    <QueryControlBar
                        servers={data.servers}
                        selectedServer={selectedQueryServer}
                        onServerChange={setSelectedQueryServer}
                        scoringProfile={scoringProfile}
                        onScoringProfileChange={setScoringProfile}
                        hours={queryHours}
                        onHoursChange={setQueryHours}
                        intervalSeconds={trendIntervalSeconds}
                        onIntervalChange={setTrendIntervalSeconds}
                        showInterval={view === "trend"}
                        showAnomalyThresholds={view === "anomaly"}
                        anomalyThresholds={anomalyThresholds}
                        onAnomalyThresholdChange={handleAnomalyThresholdChange}
                        onRefresh={() => loadQueryViewData(view)}
                    />

                    <QueryTableView
                        title=""
                        rows={queryRows[view] || []}
                        loading={!!queryLoading[view]}
                        error={queryError[view]}
                        emptyText="该条件下暂无返回记录"
                    />
                </Box>
            );
        }

        // Detail Views
        const metrics = {
            cpu: { key: "cpu_percent", icon: "⚡", unit: "%", getValue: s => s.cpu_percent, format: v => `${v.toFixed(1)}%` },
            mem: { key: "mem_used_percent", icon: "💾", unit: "%", getValue: s => s.mem_used_percent, format: v => `${v.toFixed(1)}%` },
            disk: { key: "disk_util_percent", icon: "💿", unit: "%", getValue: s => s.disk_util_percent, format: v => `${v.toFixed(1)}%` },
            net: { key: "net", icon: "🌐", unit: "MB/s", getValue: s => (s.rcv_rate + s.send_rate)/1024, format: v => `${v.toFixed(1)} MB/s`, normalize: v => Math.min(v/10 * 100, 100) } // Mock normalization
        };

        const m = metrics[view];
        if (m && view === "cpu") {
            return (
                <CpuCoreBarsView
                    servers={data.servers}
                    selectedServer={selectedQueryServer}
                    onServerChange={setSelectedQueryServer}
                    scoringProfile={scoringProfile}
                    onScoringProfileChange={setScoringProfile}
                    hours={queryHours}
                    onHoursChange={setQueryHours}
                    onRefresh={loadCpuCoreDetails}
                    cpuCoreRows={cpuCoreRows}
                    cpuCoreLoading={cpuCoreLoading}
                    cpuCoreError={cpuCoreError}
                    nodeHistories={histories}
                />
            );
        }

        if (view === "net") {
            return (
                <Box sx={{ p: 4, height: "100%", overflowY: "auto" }}>
                    <QueryControlBar
                        servers={data.servers}
                        selectedServer={selectedQueryServer}
                        onServerChange={setSelectedQueryServer}
                        scoringProfile={scoringProfile}
                        onScoringProfileChange={setScoringProfile}
                        hours={queryHours}
                        onHoursChange={setQueryHours}
                        intervalSeconds={trendIntervalSeconds}
                        onIntervalChange={setTrendIntervalSeconds}
                        showInterval={false}
                        showAnomalyThresholds={false}
                        anomalyThresholds={anomalyThresholds}
                        onAnomalyThresholdChange={handleAnomalyThresholdChange}
                        onRefresh={() => loadQueryViewData("net_detail")}
                    />
                    <FlowTripleMonitorView
                        mode="net"
                        selectedServer={selectedQueryServer}
                        rows={queryRows.net_detail || []}
                        loading={!!queryLoading.net_detail}
                        error={queryError.net_detail}
                    />
                </Box>
            );
        }

        if (view === "disk") {
            return (
                <Box sx={{ p: 4, height: "100%", overflowY: "auto" }}>
                    <QueryControlBar
                        servers={data.servers}
                        selectedServer={selectedQueryServer}
                        onServerChange={setSelectedQueryServer}
                        scoringProfile={scoringProfile}
                        onScoringProfileChange={setScoringProfile}
                        hours={queryHours}
                        onHoursChange={setQueryHours}
                        intervalSeconds={trendIntervalSeconds}
                        onIntervalChange={setTrendIntervalSeconds}
                        showInterval={false}
                        showAnomalyThresholds={false}
                        anomalyThresholds={anomalyThresholds}
                        onAnomalyThresholdChange={handleAnomalyThresholdChange}
                        onRefresh={() => loadQueryViewData("disk_detail")}
                    />
                    <FlowTripleMonitorView
                        mode="disk"
                        selectedServer={selectedQueryServer}
                        rows={queryRows.disk_detail || []}
                        loading={!!queryLoading.disk_detail}
                        error={queryError.disk_detail}
                    />
                </Box>
            );
        }

        if (m) {
            return (
                <DetailView 
                    title={sidebarItems.find(x => x.id === view).label} 
                    metric={m} 
                    servers={data.servers} 
                    onNodeClick={handleNodeClick}
                />
            );
        }
        return null;
    };

    return (
        <ThemeProvider theme={theme}>
            <CssBaseline />
            <Box sx={{ display: "flex", height: "100vh", overflow: "hidden" }}>
                <Drawer
                     variant="permanent"
                     sx={{
                         width: 250, flexShrink: 0,
                         [`& .MuiDrawer-paper`]: { width: 250, boxSizing: "border-box", borderRight: "1px solid rgba(255,255,255,0.05)", bgcolor: "#05101a" },
                     }}
                >
                    <Box sx={{ p: 3, display: "flex", alignItems: "center", borderBottom: "1px solid rgba(255,255,255,0.05)" }}>
                        <Typography variant="h5" color="primary" fontWeight="900" letterSpacing={1}>🛡️ LMS</Typography>
                    </Box>
                    <List sx={{ pt: 2 }}>
                        {sidebarItems.map((item, i) => {
                            if (item.divider) return <Divider key={i} sx={{ my: 2, borderColor: "rgba(255,255,255,0.1)" }} />;
                            if (item.id === "header" || item.id === "header_query") return <Typography key={i} variant="caption" sx={{ px: 3, color: "text.secondary", mt: 1, display: "block" }}>{item.label}</Typography>;
                            
                            return (
                                <ListItem 
                                    button 
                                    key={item.id} 
                                    selected={view === item.id && !subViewNode} 
                                    onClick={() => handleViewChange(item.id)}
                                    sx={{ 
                                        mx: 1, my: 0.5, borderRadius: 2, 
                                        "&.Mui-selected": { bgcolor: "primary.main", color: "white", "&:hover": { bgcolor: "primary.dark" } },
                                        "&.Mui-selected .MuiListItemIcon-root": { color: "white" }
                                    }}
                                >
                                    <ListItemIcon sx={{ minWidth: 40, color: "text.secondary" }}>{item.icon}</ListItemIcon>
                                    <ListItemText primary={item.label} />
                                </ListItem>
                            );
                        })}
                    </List>
                </Drawer>
                <Box component="main" sx={{ flexGrow: 1, height: "100vh", overflow: "hidden", bgcolor: "#0a1929" }}>
                     {renderContent()}
                </Box>
            </Box>
        </ThemeProvider>
    );
};

const root = ReactDOM.createRoot(document.getElementById("root"));
root.render(<AppShell /> );
