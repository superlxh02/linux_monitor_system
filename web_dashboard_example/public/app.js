// Imports from global scope (CDN)
const {
    createTheme, ThemeProvider, CssBaseline, AppBar, Toolbar, Typography, Container, Grid, Paper, Box,
    Card, CardContent, Chip, Table, TableBody, TableCell, TableContainer, TableHead, TableRow,
    IconButton, LinearProgress, Button, Dialog, DialogTitle, DialogContent, DialogActions,
    List, ListItem, ListItemIcon, ListItemText, Divider, Drawer, Checkbox, FormControlLabel, FormGroup,
    MenuItem, Select, TextField
} = MaterialUI;

const {
    LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, AreaChart, Area
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
                        <Tooltip contentStyle={{ backgroundColor: "rgba(20, 30, 40, 0.9)", border: "1px solid #333" }} />
                        <Legend wrapperStyle={{ paddingTop: 10 }} />
                        {servers.map((s, i) => {
                            if (!selectedNodes[s.server_name]) return null;
                            // In fixed mode, force the color to always be primary
                            const color = fixedMode ? theme.palette.primary.main : colors[i % colors.length];
                            
                            const lines = [];
                            if (selectedMetrics.cpu) lines.push(<Line key={`l_${s.server_name}_cpu`} type="monotone" dataKey={`${s.server_name}_cpu`} stroke={color} name={`${s.server_name} CPU`} dot={false} strokeWidth={2} />);
                            if (selectedMetrics.mem) lines.push(<Line key={`l_${s.server_name}_mem`} type="monotone" dataKey={`${s.server_name}_mem`} stroke={color}  name={`${s.server_name} MEM`} dot={false} strokeWidth={2} />);
                            if (selectedMetrics.disk) lines.push(<Line key={`l_${s.server_name}_disk`} type="monotone" dataKey={`${s.server_name}_disk`} stroke={color}  name={`${s.server_name} DISK`} dot={false} strokeWidth={2} />);
                            if (selectedMetrics.net) lines.push(<Line key={`l_${s.server_name}_net`} type="monotone" dataKey={`${s.server_name}_net`} stroke={color} strokeWidth={2} name={`${s.server_name} NET`} dot={false} />);
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
                                当前数值 {sort==="asc"?"↑":"↓"}
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
                                            <Chip size="small" label={`Load: ${s.load_avg_1?.toFixed(2)}`} />
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

const formatCellValue = (value) => {
    if (value === null || value === undefined) return "-";
    if (typeof value === "number") return Number.isInteger(value) ? value : value.toFixed(3);
    if (typeof value === "object") {
        if (value.seconds !== undefined) {
            return new Date(Number(value.seconds) * 1000).toLocaleString("zh-CN", { hour12: false });
        }
        return JSON.stringify(value);
    }
    return String(value);
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
                                    <TableCell key={c}>{c}</TableCell>
                                ))}
                            </TableRow>
                        </TableHead>
                        <TableBody>
                            {rows.map((row, idx) => (
                                <TableRow key={idx} hover>
                                    <TableCell>{idx + 1}</TableCell>
                                    {columns.map((c) => (
                                        <TableCell key={`${idx}_${c}`} sx={{ fontFamily: c.includes("rate") || c.includes("percent") ? "monospace" : "inherit" }}>
                                            {formatCellValue(row[c])}
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
    const [histories, setHistories] = React.useState({});
    const [queryEndpointData, setQueryEndpointData] = React.useState({ endpoints: [] });
    const [selectedQueryServer, setSelectedQueryServer] = React.useState("");
    const [queryHours, setQueryHours] = React.useState(24);
    const [trendIntervalSeconds, setTrendIntervalSeconds] = React.useState(300);
    const [queryRows, setQueryRows] = React.useState({ rank: [], trend: [], anomaly: [], net_detail: [], disk_detail: [], mem_detail: [], softirq_detail: [] });
    const [queryLoading, setQueryLoading] = React.useState({});
    const [queryError, setQueryError] = React.useState({});
    const [anomalyThresholds, setAnomalyThresholds] = React.useState({
        cpu_threshold: 80,
        mem_threshold: 90,
        disk_threshold: 85,
        change_rate_threshold: 0.5
    });

    React.useEffect(() => {
        // Init fetch
        axios.get("/api/overview").then(res => setData(res.data)).catch(console.error);
        axios.get("/api/query-endpoints").then(res => setQueryEndpointData(res.data)).catch(console.error);

        const socket = io();
        socket.on("overview_update", (newData) => {
            setData(newData);
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
                const res = await axios.get("/api/rank", { params: { page: 1, page_size: 50, order: "DESC" } });
                setQueryRows(prev => ({ ...prev, rank: res.data.servers || [] }));
                return;
            }

            if (targetView === "trend") {
                const res = await axios.get(`/api/trend/${encodeURIComponent(selectedQueryServer)}`, {
                    params: { hours: queryHours, interval_seconds: trendIntervalSeconds }
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
    }, [selectedQueryServer, queryHours, trendIntervalSeconds, anomalyThresholds]);

    const handleAnomalyThresholdChange = (key, value) => {
        const nextValue = Number.isFinite(value) ? value : 0;
        setAnomalyThresholds(prev => ({ ...prev, [key]: nextValue }));
    };

    React.useEffect(() => {
        const queryViews = ["rank", "trend", "anomaly", "net_detail", "disk_detail", "mem_detail", "softirq_detail"];
        if (queryViews.includes(view)) {
            loadQueryViewData(view);
        }
    }, [view, loadQueryViewData]);

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
            cpu: { key: "cpu_percent", icon: "⚡", getValue: s => s.cpu_percent, format: v => `${v.toFixed(1)}%` },
            mem: { key: "mem_used_percent", icon: "💾", getValue: s => s.mem_used_percent, format: v => `${v.toFixed(1)}%` },
            disk: { key: "disk_util_percent", icon: "💿", getValue: s => s.disk_util_percent, format: v => `${v.toFixed(1)}%` },
            net: { key: "net", icon: "🌐", getValue: s => (s.rcv_rate + s.send_rate)/1024, format: v => `${v.toFixed(1)} MB/s`, normalize: v => Math.min(v/10 * 100, 100) } // Mock normalization
        };

        const m = metrics[view];
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
