const storage = require('../../utils/storage')
const format = require('../../utils/format')

Page({
  data: {
    month: '',
    totalText: '¥0.00',
    count: 0,
    cats: [],
    streak: 0
  },

  onShow() {
    const month = storage.monthKey(storage.todayStr())
    this.setData({ month })
    this.refresh(month)
  },

  onMonth(e) {
    const month = e.detail.value
    this.setData({ month })
    this.refresh(month)
  },

  refresh(month) {
    const app = getApp()
    const stats = storage.statsForMonth(month)
    const total = stats.total || 0
    const cats = app.globalData.categories
      .map((c) => {
        const amount = stats.byCat[c.id] || 0
        return {
          id: c.id,
          name: c.name,
          amount,
          amountText: format.displayAmount(amount),
          pct: total > 0 ? Math.round((amount / total) * 100) : 0
        }
      })
      .filter((c) => c.amount > 0)
      .sort((a, b) => b.amount - a.amount)

    this.setData({
      totalText: format.displayAmount(total),
      count: stats.count,
      cats,
      streak: storage.getStreak().count || 0
    })
  }
})
