/**
 * 设备通信层
 * 所有请求都指向 ESP32-CAM 的 80 端口（网页/控制/文件管理）
 * 视频流走 81 端口，小程序里不用它，改为轮询 /capture 取帧
 */

const HOST_KEY = 'cam_host';
// 默认填 ESP32 自发热点的地址；走路由器 STA 时改成路由器分配给你的那块板子的 IP
const DEFAULT_HOST = '192.168.4.1';

/** 去掉协议头、路径、空白，只保留 host[:port] */
function normalizeHost(raw) {
    return String(raw || '')
        .trim()
        .replace(/^https?:\/\//i, '')
        .replace(/\/.*$/, '')
        .replace(/\s/g, '');
}

function getHost() {
    return wx.getStorageSync(HOST_KEY) || DEFAULT_HOST;
}

function setHost(raw) {
    const host = normalizeHost(raw);
    if (host) {
        wx.setStorageSync(HOST_KEY, host);
        return host;
    }
    return getHost();
}

function base(host) {
    return 'http://' + (host || getHost());
}

function buildQuery(params) {
    if (!params) return '';
    const parts = [];
    Object.keys(params).forEach(function (key) {
        const value = params[key];
        if (value === undefined || value === null || value === '') return;
        parts.push(encodeURIComponent(key) + '=' + encodeURIComponent(value));
    });
    return parts.length ? '?' + parts.join('&') : '';
}

/**
 * 统一请求封装
 * @param {string} path  形如 '/status'
 * @param {object} params 查询参数
 * @param {object} options { method, timeout, host }
 */
function request(path, params, options) {
    const opt = options || {};
    return new Promise(function (resolve, reject) {
        wx.request({
            url: base(opt.host) + path + buildQuery(params),
            method: opt.method || 'GET',
            timeout: opt.timeout || 8000,
            header: { 'Cache-Control': 'no-cache' },
            dataType: 'json',
            success: function (res) {
                if (res.statusCode >= 200 && res.statusCode < 300) {
                    resolve(res.data);
                } else {
                    reject(new Error('HTTP ' + res.statusCode));
                }
            },
            fail: function (err) {
                reject(new Error((err && err.errMsg) || '网络请求失败'));
            },
        });
    });
}

/**
 * 实时预览取帧地址（加时间戳防止缓存）
 * 仅用于调试 / 兜底：真机上 <image> 直连 http 局域网地址不可靠，
 * 正常预览走 fetchFrame() 的 wx.request 通道。
 */
function frameUrl() {
    return base() + '/capture?t=' + Date.now() + Math.floor(Math.random() * 1000);
}

/**
 * 取一帧原始 JPEG（ArrayBuffer）
 * 走 wx.request，与 /status 同一条通道，不受 <image> 组件加载策略影响。
 */
function fetchFrame(host, timeout) {
    return new Promise(function (resolve, reject) {
        wx.request({
            url: base(host) + '/capture?t=' + Date.now(),
            method: 'GET',
            responseType: 'arraybuffer',
            timeout: timeout || 6000,
            header: { 'Cache-Control': 'no-cache' },
            success: function (res) {
                const buf = res.data;
                if (res.statusCode !== 200) {
                    reject(new Error('HTTP ' + res.statusCode));
                    return;
                }
                if (!buf || typeof buf === 'string') {
                    reject(new Error('响应类型异常'));
                    return;
                }
                const bytes = new Uint8Array(buf);
                if (bytes.length < 4) {
                    reject(new Error('空帧'));
                    return;
                }
                // JPEG 必须以 FFD8 开头，否则说明返回的不是图像
                if (bytes[0] !== 0xff || bytes[1] !== 0xd8) {
                    reject(new Error('非 JPEG 数据'));
                    return;
                }
                resolve(buf);
            },
            fail: function (err) {
                reject(new Error((err && err.errMsg) || '取帧失败'));
            },
        });
    });
}

/** ArrayBuffer → base64 文本（兼容没有 wx.arrayBufferToBase64 的基础库） */
function toBase64(buf) {
    if (typeof wx.arrayBufferToBase64 === 'function') {
        return wx.arrayBufferToBase64(buf);
    }
    const bytes = new Uint8Array(buf);
    const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
    let out = '';
    let i = 0;
    for (; i + 2 < bytes.length; i += 3) {
        const n = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];
        out += chars[(n >> 18) & 63] + chars[(n >> 12) & 63] + chars[(n >> 6) & 63] + chars[n & 63];
    }
    const rest = bytes.length - i;
    if (rest === 1) {
        const n = bytes[i] << 16;
        out += chars[(n >> 18) & 63] + chars[(n >> 12) & 63] + '==';
    } else if (rest === 2) {
        const n = (bytes[i] << 16) | (bytes[i + 1] << 8);
        out += chars[(n >> 18) & 63] + chars[(n >> 12) & 63] + chars[(n >> 6) & 63] + '=';
    }
    return out;
}

/** ArrayBuffer → data:image/jpeg;base64,... */
function frameDataUrl(buf) {
    return 'data:image/jpeg;base64,' + toBase64(buf);
}

/** 把一帧写到本地临时文件，返回文件路径（用于抓拍 / 存相册） */
function frameToFile(buf) {
    return new Promise(function (resolve, reject) {
        const filePath = wx.env.USER_DATA_PATH + '/cap_' + Date.now() + '.jpg';
        wx.getFileSystemManager().writeFile({
            filePath: filePath,
            data: toBase64(buf),
            encoding: 'base64',
            success: function () {
                resolve(filePath);
            },
            fail: function (err) {
                reject(new Error((err && err.errMsg) || '写文件失败'));
            },
        });
    });
}

/** SD 卡文件下载地址 */
function fileUrl(filePath) {
    return base() + '/download?path=' + encodeURIComponent(filePath);
}

/** 字节数格式化 */
function fmtSize(bytes) {
    const b = Number(bytes) || 0;
    if (b < 1024) return b + ' B';
    if (b < 1048576) return (b / 1024).toFixed(1) + ' KB';
    if (b < 1073741824) return (b / 1048576).toFixed(1) + ' MB';
    return (b / 1073741824).toFixed(2) + ' GB';
}

/** 秒数格式化为 mm:ss */
function fmtDuration(sec) {
    const s = Math.max(0, Math.floor(Number(sec) || 0));
    const m = Math.floor(s / 60);
    const r = s % 60;
    return (m < 10 ? '0' : '') + m + ':' + (r < 10 ? '0' : '') + r;
}

module.exports = {
    normalizeHost: normalizeHost,
    getHost: getHost,
    setHost: setHost,
    base: base,
    request: request,
    frameUrl: frameUrl,
    fetchFrame: fetchFrame,
    frameDataUrl: frameDataUrl,
    frameToFile: frameToFile,
    fileUrl: fileUrl,
    fmtSize: fmtSize,
    fmtDuration: fmtDuration,
};
