const storage = require('./utils/storage')

App({
  onLaunch() {
    storage.ensureReady()
    // 开通云开发后取消注释：
    // if (wx.cloud) {
    //   wx.cloud.init({ env: 'your-env-id', traceUser: true })
    // }
  },
  globalData: {
    categories: [
      { id: 'food', name: '餐饮' },
      { id: 'transit', name: '交通' },
      { id: 'shop', name: '购物' },
      { id: 'daily', name: '日用' },
      { id: 'fun', name: '娱乐' },
      { id: 'other', name: '其他' }
    ],
    preferCloudOcr: false
  }
})
