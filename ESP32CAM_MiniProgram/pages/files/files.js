const api = require('../../utils/api');

function buildCrumbs(path) {
    const crumbs = [{ name: 'SD 卡', path: '/' }];
    const parts = String(path || '/').split('/').filter(Boolean);
    let acc = '';
    parts.forEach(function (part) {
        acc += '/' + part;
        crumbs.push({ name: part, path: acc });
    });
    return crumbs;
}

function extOf(name) {
    const parts = String(name).split('.');
    if (parts.length < 2) return '';
    return parts.pop().toUpperCase().slice(0, 4);
}

Page({
    data: {
        path: '/',
        crumbs: [{ name: 'SD 卡', path: '/' }],
        items: [],
        loading: true,
        empty: false,
        usedText: '-',
        totalText: '-',
        percent: 0,
        uploadPct: -1,
    },

    onShow() {
        this.load(this.data.path || '/');
    },

    async load(path) {
        const target = path || '/';
        this.setData({ loading: true });
        try {
            const j = await api.request('/api/list', { path: target });
            if (!j || !j.ok) {
                this.setData({ loading: false, items: [], empty: true });
                wx.showToast({ title: (j && j.msg) || '读取失败', icon: 'none' });
                return;
            }
            const basePath = j.path === '/' ? '' : j.path;
            const items = (j.items || []).map(function (it) {
                return {
                    n: it.n,
                    d: !!it.d,
                    full: basePath + '/' + it.n,
                    tag: it.d ? 'DIR' : extOf(it.n) || 'FILE',
                    sub: it.d ? '文件夹' : (extOf(it.n) || '文件') + ' · ' + api.fmtSize(it.s),
                };
            });
            const total = Number(j.total) || 0;
            const used = Number(j.used) || 0;
            this.setData({
                path: j.path || target,
                crumbs: buildCrumbs(j.path || target),
                items: items,
                loading: false,
                empty: items.length === 0,
                usedText: api.fmtSize(used * 1048576),
                totalText: api.fmtSize(total * 1048576),
                percent: total > 0 ? Math.min(100, Math.round((used / total) * 100)) : 0,
            });
        } catch (err) {
            this.setData({ loading: false });
            wx.showToast({ title: '设备无响应', icon: 'none' });
        }
    },

    gotoCrumb(event) {
        this.load(event.currentTarget.dataset.path);
    },

    refresh() {
        this.load(this.data.path);
    },

    onItemTap(event) {
        const item = this.data.items[Number(event.currentTarget.dataset.index)];
        if (!item) return;
        if (item.d) {
            this.load(item.full);
            return;
        }
        wx.showActionSheet({
            itemList: ['下载到手机', '重命名', '删除'],
            success: (res) => {
                if (res.tapIndex === 0) this.download(item);
                if (res.tapIndex === 1) this.renameItem(item);
                if (res.tapIndex === 2) this.removeItem(item);
            },
        });
    },

    download(item) {
        const ext = extOf(item.n).toLowerCase();
        wx.showLoading({ title: '下载中', mask: true });
        wx.downloadFile({
            url: api.fileUrl(item.full),
            success: (res) => {
                wx.hideLoading();
                if (res.statusCode !== 200) {
                    wx.showToast({ title: '下载失败', icon: 'none' });
                    return;
                }
                if (ext === 'jpg' || ext === 'jpeg') {
                    wx.saveImageToPhotosAlbum({
                        filePath: res.tempFilePath,
                        success: () => wx.showToast({ title: '已存到相册', icon: 'success' }),
                        fail: () => wx.showToast({ title: '保存失败', icon: 'none' }),
                    });
                } else {
                    this.shareFile(res.tempFilePath, item.n);
                }
            },
            fail: () => {
                wx.hideLoading();
                wx.showToast({ title: '下载失败', icon: 'none' });
            },
        });
    },

    shareFile(filePath, name) {
        if (!wx.shareFileMessage) {
            wx.showToast({ title: '当前微信版本不支持文件转发', icon: 'none' });
            return;
        }
        wx.shareFileMessage({
            filePath: filePath,
            fileName: name,
            fail: () => wx.showToast({ title: '可转发到聊天保存', icon: 'none' }),
        });
    },

    renameItem(item) {
        wx.showModal({
            title: '重命名',
            editable: true,
            placeholderText: '输入新名称',
            content: item.n,
            success: async (res) => {
                if (!res.confirm) return;
                const next = String(res.content || '').trim();
                if (!next || next === item.n) return;
                try {
                    const j = await api.request('/api/rename', { path: item.full, name: next });
                    wx.showToast({
                        title: j && j.ok ? '已重命名' : (j && j.msg) || '失败',
                        icon: 'none',
                    });
                    this.load(this.data.path);
                } catch (err) {
                    wx.showToast({ title: '失败', icon: 'none' });
                }
            },
        });
    },

    removeItem(item) {
        wx.showModal({
            title: '删除文件',
            content: '确定删除 ' + item.n + ' ？删除后不可恢复。',
            confirmText: '删除',
            confirmColor: '#ff4d4f',
            success: async (res) => {
                if (!res.confirm) return;
                try {
                    const j = await api.request('/api/delete', { path: item.full });
                    wx.showToast({
                        title: j && j.ok ? '已删除' : (j && j.msg) || '删除失败',
                        icon: 'none',
                    });
                    this.load(this.data.path);
                } catch (err) {
                    wx.showToast({ title: '删除失败', icon: 'none' });
                }
            },
        });
    },

    makeDir() {
        wx.showModal({
            title: '新建文件夹',
            editable: true,
            placeholderText: '文件夹名称',
            success: async (res) => {
                if (!res.confirm) return;
                const name = String(res.content || '').trim();
                if (!name) return;
                const full = (this.data.path === '/' ? '' : this.data.path) + '/' + name;
                try {
                    const j = await api.request('/api/mkdir', { path: full });
                    wx.showToast({
                        title: j && j.ok ? '已创建' : (j && j.msg) || '失败',
                        icon: 'none',
                    });
                    this.load(this.data.path);
                } catch (err) {
                    wx.showToast({ title: '失败', icon: 'none' });
                }
            },
        });
    },

    upload() {
        wx.chooseMessageFile({
            count: 1,
            type: 'file',
            success: (res) => {
                const file = res.tempFiles && res.tempFiles[0];
                if (!file) return;
                this.setData({ uploadPct: 0 });
                const task = wx.uploadFile({
                    url: api.base() + '/api/upload?dir=' + encodeURIComponent(this.data.path),
                    filePath: file.path,
                    name: 'file',
                    formData: {},
                    success: (r) => {
                        this.setData({ uploadPct: -1 });
                        let ok = false;
                        try {
                            ok = !!JSON.parse(r.data).ok;
                        } catch (e) {
                            ok = false;
                        }
                        wx.showToast({ title: ok ? '上传完成' : '上传失败', icon: 'none' });
                        this.load(this.data.path);
                    },
                    fail: () => {
                        this.setData({ uploadPct: -1 });
                        wx.showToast({ title: '上传失败', icon: 'none' });
                    },
                });
                task.onProgressUpdate((p) => {
                    this.setData({ uploadPct: p.progress });
                });
            },
        });
    },

    wipeAll() {
        wx.showModal({
            title: '清空 SD 卡',
            content: '将删除卡内全部文件，此操作不可恢复。确定继续？',
            confirmText: '继续',
            confirmColor: '#ff4d4f',
            success: (r) => {
                if (!r.confirm) return;
                wx.showModal({
                    title: '再次确认',
                    content: '确定要清空整个 SD 卡吗？',
                    confirmText: '确认清空',
                    confirmColor: '#ff4d4f',
                    success: async (r2) => {
                        if (!r2.confirm) return;
                        wx.showLoading({ title: '清空中', mask: true });
                        try {
                            const j = await api.request('/api/wipe', { confirm: 'YES' }, { timeout: 30000 });
                            wx.hideLoading();
                            wx.showToast({
                                title: j && j.ok ? '已清空' : (j && j.msg) || '失败',
                                icon: 'none',
                            });
                            this.load('/');
                        } catch (err) {
                            wx.hideLoading();
                            wx.showToast({ title: '失败', icon: 'none' });
                        }
                    },
                });
            },
        });
    },
});
