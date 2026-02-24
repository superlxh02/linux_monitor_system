const express = require('express');
const path = require('path');
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const cors = require('cors');
const http = require('http');
const { Server } = require('socket.io');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

const PORT = 3000;
const MANAGER_ADDRESS = '127.0.0.1:50051'; // Adjust if manager runs elsewhere

const QUERY_ENDPOINTS = [
    // gRPC QueryService APIs
    { type: 'gRPC', method: 'RPC', path: 'QueryPerformance', target: MANAGER_ADDRESS, description: '时间段内性能数据（分页）' },
    { type: 'gRPC', method: 'RPC', path: 'QueryTrend', target: MANAGER_ADDRESS, description: '变化率趋势查询（可聚合）' },
    { type: 'gRPC', method: 'RPC', path: 'QueryAnomaly', target: MANAGER_ADDRESS, description: '异常数据查询（阈值过滤）' },
    { type: 'gRPC', method: 'RPC', path: 'QueryScoreRank', target: MANAGER_ADDRESS, description: '评分排序查询' },
    { type: 'gRPC', method: 'RPC', path: 'QueryLatestScore', target: MANAGER_ADDRESS, description: '最新评分查询' },
    { type: 'gRPC', method: 'RPC', path: 'QueryNetDetail', target: MANAGER_ADDRESS, description: '网络详情查询（分页）' },
    { type: 'gRPC', method: 'RPC', path: 'QueryDiskDetail', target: MANAGER_ADDRESS, description: '磁盘详情查询（分页）' },
    { type: 'gRPC', method: 'RPC', path: 'QueryMemDetail', target: MANAGER_ADDRESS, description: '内存详情查询（分页）' },
    { type: 'gRPC', method: 'RPC', path: 'QuerySoftIrqDetail', target: MANAGER_ADDRESS, description: '软中断详情查询（分页）' },

    // Exposed BFF HTTP APIs
    { type: 'HTTP', method: 'GET', path: '/api/overview', target: `0.0.0.0:${PORT}`, description: '总览数据（映射 QueryLatestScore）' },
    { type: 'HTTP', method: 'GET', path: '/api/performance/:serverName?hours=1', target: `0.0.0.0:${PORT}`, description: '历史性能（映射 QueryPerformance）' },
    { type: 'HTTP', method: 'GET', path: '/api/rank', target: `0.0.0.0:${PORT}`, description: '评分排行（映射 QueryScoreRank）' },
    { type: 'HTTP', method: 'GET', path: '/api/anomaly', target: `0.0.0.0:${PORT}`, description: '异常查询（映射 QueryAnomaly）' },
    { type: 'HTTP', method: 'GET', path: '/api/trend/:serverName', target: `0.0.0.0:${PORT}`, description: '趋势聚合（映射 QueryTrend）' },
    { type: 'HTTP', method: 'GET', path: '/api/net-detail', target: `0.0.0.0:${PORT}`, description: '网络详情（映射 QueryNetDetail）' },
    { type: 'HTTP', method: 'GET', path: '/api/disk-detail', target: `0.0.0.0:${PORT}`, description: '磁盘详情（映射 QueryDiskDetail）' },
    { type: 'HTTP', method: 'GET', path: '/api/mem-detail', target: `0.0.0.0:${PORT}`, description: '内存详情（映射 QueryMemDetail）' },
    { type: 'HTTP', method: 'GET', path: '/api/softirq-detail', target: `0.0.0.0:${PORT}`, description: '软中断详情（映射 QuerySoftIrqDetail）' },
    { type: 'HTTP', method: 'GET', path: '/api/query-endpoints', target: `0.0.0.0:${PORT}`, description: '查询端口清单' },

    // Real-time push channel
    { type: 'Socket.IO', method: 'EVENT', path: 'overview_update', target: `0.0.0.0:${PORT}`, description: '2 秒推送一次总览更新' }
];

// Middleware
app.use(cors());
app.use(express.static(path.join(__dirname, 'public')));
app.use(express.json());

// gRPC Setup
const PROTO_PATH = path.join(__dirname, '../proto/query_api.proto');
const packageDefinition = protoLoader.loadSync(PROTO_PATH, {
    keepCase: true,
    longs: String,
    enums: String,
    defaults: true,
    oneofs: true,
    includeDirs: [path.join(__dirname, '../proto')]
});

const protoDescriptor = grpc.loadPackageDefinition(packageDefinition);
const queryService = new protoDescriptor.monitor.proto.QueryService(
    MANAGER_ADDRESS,
    grpc.credentials.createInsecure()
);

// Helper for gRPC calls using Promises
const promisifyGrpc = (method, params = {}) => {
    return new Promise((resolve, reject) => {
        queryService[method](params, (err, response) => {
            if (err) {
                console.error(`gRPC Error [${method}]:`, err);
                reject(err);
            } else {
                resolve(response);
            }
        });
    });
};

// API Endpoints

// Get Latest Scores (Overview)
app.get('/api/overview', async (req, res) => {
    try {
        const response = await promisifyGrpc('QueryLatestScore', {});
        res.json(response);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Get Performance History (Charts)
app.get('/api/performance/:serverName', async (req, res) => {
    const { serverName } = req.params;
    const { hours = 1 } = req.query;
    
    // Calculate time range
    const endTime = new Date();
    const startTime = new Date(endTime.getTime() - hours * 60 * 60 * 1000);

    const request = {
        server_name: serverName,
        time_range: {
            start_time: { seconds: Math.floor(startTime.getTime() / 1000), nanos: 0 },
            end_time: { seconds: Math.floor(endTime.getTime() / 1000), nanos: 0 }
        },
        pagination: { page: 1, page_size: 1000 } // Fetch plenty for charts
    };

    try {
        const response = await promisifyGrpc('QueryPerformance', request);
        res.json(response);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Get Top Ranked Servers
app.get('/api/rank', async (req, res) => {
    try {
        const order = (req.query.order || 'DESC').toUpperCase();
        const page = parseInt(req.query.page, 10) || 1;
        const page_size = parseInt(req.query.page_size, 10) || 20;
        const response = await promisifyGrpc('QueryScoreRank', {
            order: order === 'ASC' ? 1 : 0,
            pagination: { page, page_size }
        });
        res.json(response);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// 异常数据（超阈值）
app.get('/api/anomaly', async (req, res) => {
    try {
        const serverName = req.query.server_name || '';
        const hours = parseFloat(req.query.hours, 10) || 24;
        const cpu_threshold = parseFloat(req.query.cpu_threshold, 10) || 80;
        const mem_threshold = parseFloat(req.query.mem_threshold, 10) || 90;
        const disk_threshold = parseFloat(req.query.disk_threshold, 10) || 85;
        const change_rate_threshold = parseFloat(req.query.change_rate_threshold, 10) || 0.5;
        const page = parseInt(req.query.page, 10) || 1;
        const page_size = parseInt(req.query.page_size, 10) || 50;
        const endTime = new Date();
        const startTime = new Date(endTime.getTime() - hours * 60 * 60 * 1000);
        const request = {
            server_name: serverName,
            time_range: {
                start_time: { seconds: Math.floor(startTime.getTime() / 1000), nanos: 0 },
                end_time: { seconds: Math.floor(endTime.getTime() / 1000), nanos: 0 }
            },
            cpu_threshold,
            mem_threshold,
            disk_threshold,
            change_rate_threshold,
            pagination: { page, page_size }
        };
        const response = await promisifyGrpc('QueryAnomaly', request);
        res.json(response);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// 趋势聚合
app.get('/api/trend/:serverName', async (req, res) => {
    try {
        const serverName = req.params.serverName;
        const hours = parseFloat(req.query.hours, 10) || 1;
        const interval_seconds = parseInt(req.query.interval_seconds, 10) || 300;
        const endTime = new Date();
        const startTime = new Date(endTime.getTime() - hours * 60 * 60 * 1000);
        const request = {
            server_name: serverName,
            time_range: {
                start_time: { seconds: Math.floor(startTime.getTime() / 1000), nanos: 0 },
                end_time: { seconds: Math.floor(endTime.getTime() / 1000), nanos: 0 }
            },
            interval_seconds
        };
        const response = await promisifyGrpc('QueryTrend', request);
        res.json(response);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// 详细数据通用参数：server_name, hours, page, page_size
function detailTimeRange(req) {
    const serverName = req.query.server_name || '';
    const hours = parseFloat(req.query.hours, 10) || 24;
    const page = parseInt(req.query.page, 10) || 1;
    const page_size = parseInt(req.query.page_size, 10) || 50;
    const endTime = new Date();
    const startTime = new Date(endTime.getTime() - hours * 60 * 60 * 1000);
    return {
        server_name: serverName,
        time_range: {
            start_time: { seconds: Math.floor(startTime.getTime() / 1000), nanos: 0 },
            end_time: { seconds: Math.floor(endTime.getTime() / 1000), nanos: 0 }
        },
        pagination: { page, page_size }
    };
}

app.get('/api/net-detail', async (req, res) => {
    try {
        const request = detailTimeRange(req);
        const response = await promisifyGrpc('QueryNetDetail', request);
        res.json(response);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/disk-detail', async (req, res) => {
    try {
        const request = detailTimeRange(req);
        const response = await promisifyGrpc('QueryDiskDetail', request);
        res.json(response);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/mem-detail', async (req, res) => {
    try {
        const request = detailTimeRange(req);
        const response = await promisifyGrpc('QueryMemDetail', request);
        res.json(response);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/softirq-detail', async (req, res) => {
    try {
        const request = detailTimeRange(req);
        const response = await promisifyGrpc('QuerySoftIrqDetail', request);
        res.json(response);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Get all exposed query endpoints
app.get('/api/query-endpoints', (req, res) => {
    res.json({
        manager_address: MANAGER_ADDRESS,
        dashboard_address: `0.0.0.0:${PORT}`,
        endpoints: QUERY_ENDPOINTS
    });
});

// Socket.io for real-time pushing (Polling manager every 2s)
setInterval(async () => {
    try {
        const response = await promisifyGrpc('QueryLatestScore', {});
        io.emit('overview_update', response);
    } catch (err) {
        // Suppress logs for polling errors to avoid spam if manager is down
    }
}, 2000);

// Default route
app.get('*', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`Dashboard server running at http://0.0.0.0:${PORT}`);
    console.log(`Connecting to gRPC Manager at ${MANAGER_ADDRESS}`);
});
