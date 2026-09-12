const storage = require('../../utils/storage')
const format = require('../../utils/format')

Page({
  data: {
    count: 0,
    groups: []
  },

  onShow() {
    this.refresh()
  },

  refresh() {
    const app = getApp()
    const records = storage.listRecords()
    const map = {}
    records.forEach((r) => {
      if (!map[r.date]) map[r.date] = []
      map[r.date].push({
        ...r,
        amountText: format.displayAmount(r.amount),
        categoryName: format.categoryName(r.category, app.globalData.categories),
        sourceLabel: r.source === 'receipt' ? '小票' : '手动'
      })
    })
    const groups = Object.keys(map)
      .sort((a, b) => (a < b ? 1 : -1))
      .map((date) => {
        const items = map[date]
        const sum = items.reduce((s, r) => s + (Number(r.amount) || 0), 0)
        return { date, items, sumText: format.displayAmount(sum) }
      })
    this.setData({ groups, count: records.length })
  },

  onDelete(e) {
    const id = e.currentTarget.dataset.id
    wx.showModal({
      title: '删除这笔？',
      success: (res) => {
        if (!res.confirm) return
        storage.deleteRecord(id)
        this.refresh()
      }
    })
  }
})
