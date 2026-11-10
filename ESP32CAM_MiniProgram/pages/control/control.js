const api = require('../../utils/api');

// 必须与设备端 FS_TABLE 的 name 完全一致，设备按字符串匹配分辨率
const FS_VALUES = ['QQVGA', 'CIF', 'VGA', 'SVGA', 'XGA', 'HD', 'SXGA', 'UXGA'];
const FS_LABELS = [
    'QQVGA · 160×120',
    'CIF · 400×296',
    'VGA · 640×480',
    'SVGA · 800×600',
    'XGA · 1024×768',
    'HD · 1280×720',
    'SXGA · 1280×1024',
    'UXGA · 1600×1200',
];

Page({
    data: {
        fsLabels: FS_LABELS,
        fsIndex: 3,
        fsLabel: FS_LABELS[3],

        quality: 12,
        qualityText: '12',
        brightness: 0,
        contrast: 0,

        hmirror: false,
        vflip: false,
        flash: false,

        recOn: false,
        online: false,
    },

    onShow() {
        this.busyUntil = 0;
        this.syncStatus();
        this.statusTimer = setInterval(() => this.syncStatus(true), 3000);
    },

    onHide() {
        clearInterval(this.statusTimer);
    },

    onUnload() {
        clearInterval(this.statusTimer);
    },

    /** 拖动滑条期间不要被轮询回填打断 */
    markBusy() {
        this.busyUntil = Date.now() + 1500;
    },

    /**
     * 以设备为准拉取全部控制项。
     * 以前这里会用本地 storage 覆盖，导致网页端改过的值在小程序里看不到；
     * 现在设备返回的真实传感器值优先，storage 只在设备离线时兜底。
     */
    async syncStatus(silent) {
        if (Date.now() < (this.busyUntil || 0)) return;
        try {
            const s = await api.request('/status');
            const index = FS_VALUES.indexOf(s.fs);
            const patch = {
                online: true,
                recOn: !!s.rec,
                quality: Number(s.quality),
                qualityText: String(s.quality),
            };
            if (index >= 0) {
                patch.fsIndex = index;
                patch.fsLabel = FS_LABELS[index];
            }
            if (s.bri !== undefined) patch.brightness = Number(s.bri);
            if (s.con !== undefined) patch.contrast = Number(s.con);
            if (s.hm !== undefined) patch.hmirror = !!Number(s.hm);
            if (s.vf !== undefined) patch.vflip = !!Number(s.vf);
            if (s.led !== undefined) patch.flash = !!Number(s.led);
            this.setData(patch);
        } catch (err) {
            if (!silent) {
                this.setData({ online: false });
                const saved = wx.getStorageSync('cam_ctrl');
                if (saved && typeof saved === 'object') {
                    this.setData({
                        brightness: Number(saved.brightness) || 0,
                        contrast: Number(saved.contrast) || 0,
                        hmirror: !!saved.hmirror,
                        vflip: !!saved.vflip,
                        flash: !!saved.flash,
                    });
                }
            }
        }
    },

    persist() {
        wx.setStorageSync('cam_ctrl', {
            brightness: this.data.brightness,
            contrast: this.data.contrast,
            hmirror: this.data.hmirror,
            vflip: this.data.vflip,
            flash: this.data.flash,
        });
    },

    async send(params, okTip) {
        try {
            await api.request('/control', params);
            if (okTip) wx.showToast({ title: okTip, icon: 'none' });
            return true;
        } catch (err) {
            wx.showToast({ title: '设置失败，检查连接', icon: 'none' });
            return false;
        }
    },

    /* ---------- 分辨率 ---------- */
    onFsChange(event) {
        this.markBusy();
        if (this.data.recOn) {
            wx.showToast({ title: '录制中不能改分辨率', icon: 'none' });
            return;
        }
        const index = Number(event.detail.value);
        this.setData({ fsIndex: index, fsLabel: FS_LABELS[index] });
        this.send({ fs: FS_VALUES[index] });
    },

    /* ---------- 画质 ---------- */
    onQualityChanging(event) {
        this.markBusy();
        this.setData({ qualityText: String(event.detail.value) });
    },
    onQualityChange(event) {
        this.markBusy();
        const value = Number(event.detail.value);
        this.setData({ quality: value, qualityText: String(value) });
        this.send({ quality: value });
    },

    /* ---------- 亮度 / 对比度 ---------- */
    onBrightChanging(event) {
        this.markBusy();
        this.setData({ brightness: Number(event.detail.value) });
    },
    onBrightChange(event) {
        this.markBusy();
        const value = Number(event.detail.value);
        this.setData({ brightness: value });
        this.persist();
        this.send({ brightness: value });
    },
    onContrastChanging(event) {
        this.markBusy();
        this.setData({ contrast: Number(event.detail.value) });
    },
    onContrastChange(event) {
        this.markBusy();
        const value = Number(event.detail.value);
        this.setData({ contrast: value });
        this.persist();
        this.send({ contrast: value });
    },

    /* ---------- 镜像 / 翻转 / 补光 ---------- */
    onMirror(event) {
        this.markBusy();
        const on = event.detail.value;
        this.setData({ hmirror: on });
        this.persist();
        this.send({ hmirror: on ? 1 : 0 });
    },
    onVflip(event) {
        this.markBusy();
        const on = event.detail.value;
        this.setData({ vflip: on });
        this.persist();
        this.send({ vflip: on ? 1 : 0 });
    },
    onFlash(event) {
        this.markBusy();
        const on = event.detail.value;
        this.setData({ flash: on });
        this.persist();
        this.send({ flash: on ? 1 : 0 }, on ? '补光灯已开' : '补光灯已关');
    },

    /* ---------- 恢复默认 ---------- */
    async resetAll() {
        this.setData({
            quality: 12,
            qualityText: '12',
            brightness: 0,
            contrast: 0,
            hmirror: false,
            vflip: false,
            flash: false,
        });
        this.persist();
        await this.send({
            quality: 12,
            brightness: 0,
            contrast: 0,
            hmirror: 0,
            vflip: 0,
            flash: 0,
        }, '已恢复默认');
    },

    gotoMonitor() {
        wx.switchTab({ url: '/pages/index/index' });
    },
});
