const api = require('../../utils/api');

const FPS_VALUES = [5, 10, 15, 20];
const FPS_LABELS = ['5 fps', '10 fps', '15 fps', '20 fps'];

Page({
    data: {
        host: '',
        viewState: 'connecting', // connecting | ready | error
        connOk: false,
        connText: '未连接',
        frameUrl: '',
        fpsText: '0.0',
        errMsg: '',

        recOn: false,
        recTimeText: '00:00',
        recFrames: 0,
        fpsIndex: 1,
        fpsLabels: FPS_LABELS,
        fpsLabel: FPS_LABELS[1],

        apText: '-',
        staText: '-',
        rssiText: '-',
        heapText: '-',
        sdText: '-',
        upText: '-',
        fsText: '-',
    },

    onLoad() {
        this.frameCount = 0;
        this.failCount = 0;
        this.liveOn = false;
        this.grabbing = false;
        this.setData({ host: api.getHost() });
    },

    onShow() {
        this.liveOn = true;
        this.failCount = 0;
        this.grabbing = false;
        this.setData({
            host: api.getHost(),
            viewState: this.data.viewState === 'ready' ? 'ready' : 'connecting',
        });
        this.pump(0);
        this.startTimers();
    },

    onHide() {
        this.shutdown();
    },

    onUnload() {
        this.shutdown();
    },

    shutdown() {
        this.liveOn = false;
        this.grabbing = false;
        clearTimeout(this.frameTimer);
        clearInterval(this.pollTimer);
        clearInterval(this.fpsTimer);
    },

    /* ---------------- 取帧循环 ---------------- */

    /**
     * 用 wx.request 取 JPEG 再转 base64 喂给 <image>。
     * 不用 <image src="http://192.168.x.x/capture"> 直连：
     * 真机上该组件走微信自己的图片加载通道，对 ESP32 这种短连接 HTTP 服务
     * 经常一次都加载不出来（状态接口能通、画面全黑就是典型症状）。
     */
    pump(delay) {
        clearTimeout(this.frameTimer);
        const wait = typeof delay === 'number' ? delay : 20;
        this.frameTimer = setTimeout(() => this.grab(), wait);
    },

    async grab() {
        if (!this.liveOn || this.data.recOn || this.grabbing) return;
        this.grabbing = true;
        try {
            const buf = await api.fetchFrame(this.data.host, 6000);
            this.grabbing = false;
            if (!this.liveOn || this.data.recOn) return;

            this.frameCount += 1;
            this.failCount = 0;

            const patch = { frameUrl: api.frameDataUrl(buf) };
            if (this.data.viewState !== 'ready' || !this.data.connOk) {
                patch.viewState = 'ready';
                patch.connOk = true;
                patch.connText = '已连接';
                patch.errMsg = '';
            }
            this.setData(patch);
            // ESP32 的 WebServer 不支持 keep-alive，每帧都是一次短连接，
            // 间隔太短会堆积 TIME_WAIT 把 lwIP 连接池占满，这里留出空隙
            this.pump(90);
        } catch (err) {
            this.grabbing = false;
            this.failCount += 1;
            // 单次失败不切换状态，避免网络抖动导致画面闪黑
            if (this.failCount >= 2 && this.data.viewState !== 'error') {
                this.setData({
                    viewState: 'error',
                    connOk: false,
                    connText: '未连接',
                    errMsg: (err && err.message) || '取帧失败',
                });
            }
            this.pump(800);
        }
    },

    /** 仅用于诊断：base64 帧渲染完成 */
    onFrameLoad() {},

    /** 仅用于诊断：渲染层解码失败，不改变连接状态 */
    onFrameError(event) {
        console.warn('[frame-render]', event && event.detail);
    },

    retry() {
        this.failCount = 0;
        this.grabbing = false;
        this.setData({ viewState: 'connecting', frameUrl: '', errMsg: '' });
        this.pump(0);
        this.poll();
    },

    /** 连不上时直接弹框改地址，省得去找顶部输入框 */
    editHost() {
        wx.showModal({
            title: '设备地址',
            editable: true,
            placeholderText: '例如 192.168.4.1',
            content: api.getHost(),
            success: (res) => {
                if (!res.confirm) return;
                const host = api.normalizeHost(res.content);
                if (!host) return;
                api.setHost(host);
                this.failCount = 0;
                this.grabbing = false;
                this.setData({ host: host, frameUrl: '', viewState: 'connecting', errMsg: '' });
                this.pump(0);
                this.poll();
            },
        });
    },

    /* ---------------- 设备地址 ---------------- */

    onHostInput(event) {
        this.setData({ host: event.detail.value });
    },

    onHostConfirm(event) {
        const raw = (event && event.detail && event.detail.value) || this.data.host;
        const host = api.normalizeHost(raw);
        if (!host) {
            this.setData({ host: api.getHost() });
            return;
        }
        if (host === api.getHost()) {
            this.setData({ host: host });
            return;
        }
        api.setHost(host);
        this.failCount = 0;
        this.grabbing = false;
        this.setData({ host: host, frameUrl: '', viewState: 'connecting', errMsg: '' });
        this.pump(0);
        this.poll();
    },

    /* ---------------- 状态轮询 ---------------- */

    startTimers() {
        clearInterval(this.pollTimer);
        clearInterval(this.fpsTimer);
        this.frameCount = 0;
        this.poll();
        this.pollTimer = setInterval(() => this.poll(), 1500);
        this.fpsTimer = setInterval(() => {
            this.setData({ fpsText: this.frameCount.toFixed(1) });
            this.frameCount = 0;
        }, 1000);
    },

    async poll() {
        try {
            const s = await api.request('/status');
            const wasRec = this.data.recOn;
            const nowRec = !!s.rec;

            this.setData({
                connOk: true,
                connText: '已连接',
                apText: s.ap + ' · ' + s.apip,
                staText: s.staip ? s.staid + ' · ' + s.staip : '未连接',
                rssiText: s.staip ? s.rssi + ' dBm' : '-',
                heapText: Math.round(s.heap / 1024) + ' KB',
                sdText: s.sd ? '就绪 · ' + s.sdfree + ' MB 可用' : '未检测到',
                upText: this.fmtUptime(s.up),
                fsText: s.fs,
                recOn: nowRec,
                recTimeText: api.fmtDuration(s.recsec),
                recFrames: s.recf,
            });

            if (nowRec !== wasRec) {
                if (nowRec) {
                    this.setData({ frameUrl: '' });
                } else {
                    this.setData({ viewState: 'connecting' });
                    this.pump(200);
                }
            }
        } catch (err) {
            if (this.data.connOk) {
                this.setData({ connOk: false, connText: '未连接' });
            }
        }
    },

    fmtUptime(sec) {
        const s = Math.floor(Number(sec) || 0);
        if (s < 60) return s + ' 秒';
        const m = Math.floor(s / 60);
        if (m < 60) return m + ' 分 ' + (s % 60) + ' 秒';
        return Math.floor(m / 60) + ' 时 ' + (m % 60) + ' 分';
    },

    /* ---------------- 抓拍 / 存卡 ---------------- */

    async capture() {
        if (!this.data.connOk) {
            wx.showToast({ title: '请先连接设备', icon: 'none' });
            return;
        }
        wx.showLoading({ title: '抓拍中', mask: true });
        try {
            const buf = await api.fetchFrame(this.data.host, 8000);
            const filePath = await api.frameToFile(buf);
            wx.hideLoading();
            this.saveToAlbum(filePath, '已存到相册');
        } catch (err) {
            wx.hideLoading();
            wx.showToast({ title: '抓拍失败', icon: 'none' });
        }
    },

    saveToAlbum(filePath, okTitle) {
        wx.saveImageToPhotosAlbum({
            filePath: filePath,
            success: () => wx.showToast({ title: okTitle || '已保存', icon: 'success' }),
            fail: (err) => {
                const msg = (err && err.errMsg) || '';
                if (msg.indexOf('auth deny') >= 0 || msg.indexOf('authorize') >= 0) {
                    wx.showModal({
                        title: '需要相册权限',
                        content: '保存图片需要相册权限，请在设置中开启',
                        confirmText: '去设置',
                        success: (r) => {
                            if (r.confirm) wx.openSetting();
                        },
                    });
                } else {
                    wx.showToast({ title: '保存失败', icon: 'none' });
                }
            },
        });
    },

    async saveSd() {
        if (!this.data.connOk) {
            wx.showToast({ title: '请先连接设备', icon: 'none' });
            return;
        }
        wx.showLoading({ title: '写入中', mask: true });
        try {
            const j = await api.request('/save');
            wx.hideLoading();
            wx.showToast({
                title: j && j.ok ? '已存储到设备' : (j && j.msg) || '保存失败',
                icon: 'none',
            });
        } catch (err) {
            wx.hideLoading();
            wx.showToast({ title: '设备无响应', icon: 'none' });
        }
    },

    /* ---------------- 录屏 ---------------- */

    onFpsChange(event) {
        const index = Number(event.detail.value);
        this.setData({ fpsIndex: index, fpsLabel: FPS_LABELS[index] });
    },

    toggleRec() {
        if (this.data.recOn) {
            this.stopRec();
        } else {
            this.startRec();
        }
    },

    async startRec() {
        if (!this.data.connOk) {
            wx.showToast({ title: '请先连接设备', icon: 'none' });
            return;
        }
        try {
            const j = await api.request('/api/rec/start', { fps: FPS_VALUES[this.data.fpsIndex] });
            if (!j || !j.ok) {
                wx.showToast({ title: (j && j.msg) || '无法开始录制', icon: 'none' });
                return;
            }
            this.setData({
                recOn: true,
                recFrames: 0,
                recTimeText: '00:00',
                frameUrl: '',
            });
            wx.showToast({ title: '开始录制', icon: 'success' });
        } catch (err) {
            wx.showToast({ title: '设备无响应', icon: 'none' });
        }
    },

    async stopRec() {
        wx.showLoading({ title: '正在保存', mask: true });
        try {
            const j = await api.request('/api/rec/stop', null, { timeout: 25000 });
            wx.hideLoading();
            this.setData({ recOn: false, viewState: 'connecting' });
            this.pump(300);

            if (!j || !j.ok) {
                wx.showToast({ title: (j && j.msg) || '停止失败', icon: 'none' });
                return;
            }
            wx.showModal({
                title: '录制完成',
                content: j.name + ' · ' + api.fmtSize(j.size) + ' · ' + j.frames + ' 帧\n保存到手机？',
                confirmText: '保存',
                cancelText: '稍后',
                success: (r) => {
                    if (r.confirm) this.saveRecFile(j.name);
                },
            });
        } catch (err) {
            wx.hideLoading();
            this.setData({ recOn: false, viewState: 'connecting' });
            this.pump(300);
            wx.showToast({ title: '停止失败', icon: 'none' });
        }
    },

    saveRecFile(path) {
        wx.showLoading({ title: '下载中', mask: true });
        wx.downloadFile({
            url: api.fileUrl(path),
            success: (res) => {
                wx.hideLoading();
                if (res.statusCode !== 200) {
                    wx.showToast({ title: '下载失败', icon: 'none' });
                    return;
                }
                const name = path.split('/').pop();
                if (!wx.shareFileMessage) {
                    wx.showToast({ title: '当前微信版本不支持文件转发', icon: 'none' });
                    return;
                }
                wx.shareFileMessage({
                    filePath: res.tempFilePath,
                    fileName: name,
                    fail: () => {
                        wx.showToast({ title: '已下载，可转发到聊天保存', icon: 'none' });
                    },
                });
            },
            fail: () => {
                wx.hideLoading();
                wx.showToast({ title: '下载失败', icon: 'none' });
            },
        });
    },
});
