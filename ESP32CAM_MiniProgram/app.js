App({
    globalData: {
        appName: 'ESP32-CAM 监控',
        joinedAt: 0,
    },

    onLaunch() {
        this.globalData.joinedAt = Date.now();
    },
});
