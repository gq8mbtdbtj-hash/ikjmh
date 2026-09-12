const storage = require('../../utils/storage')
const format = require('../../utils/format')

Page({
  data: {
    todayAmountText: '¥0.00',
    streak: 0,
    streakTip: '',
    recent: []
  },

  onShow() {
    this.refresh()
  },

  refresh() {
    const app = getApp()
    const today = storage.todayStr()
    const sum = storage.sumByDate(today)
    const streak = storage.getStreak()
    const recent = storage
      .listRecords()
      .slice(0, 5)
      .map((r) => ({
        ...r,
        amountText: format.displayAmount(r.amount),
        categoryName: format.categoryName(r.category, app.globalData.categories)
      }))

    let streakTip = ''
    if (!streak.count) streakTip = '记第一笔，开启连续'
    else if (streak.lastDate !== today) streakTip = '今天还没记，别断签'

    this.setData({
      todayAmountText: format.displayAmount(sum),
      streak: streak.count || 0,
      streakTip,
      recent
    })
  },

  goScan() {
    wx.navigateTo({ url: '/pages/scan/scan' })
  },

  goAdd() {
    wx.navigateTo({ url: '/pages/add/add' })
  },

  goRecords() {
    wx.switchTab({ url: '/pages/records/records' })
  }
})
