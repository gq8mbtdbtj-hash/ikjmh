const storage = require('../../utils/storage')

Page({
  data: {
    amount: '',
    merchant: '',
    category: 'food',
    date: '',
    note: '',
    categories: []
  },

  onLoad() {
    const app = getApp()
    this.setData({
      categories: app.globalData.categories,
      date: storage.todayStr()
    })
  },

  onAmount(e) {
    this.setData({ amount: e.detail.value })
  },
  onMerchant(e) {
    this.setData({ merchant: e.detail.value })
  },
  onNote(e) {
    this.setData({ note: e.detail.value })
  },
  onDate(e) {
    this.setData({ date: e.detail.value })
  },
  onCat(e) {
    this.setData({ category: e.currentTarget.dataset.id })
  },

  onSave() {
    const amount = Number(this.data.amount)
    if (!amount || amount <= 0) {
      wx.showToast({ title: '请填写金额', icon: 'none' })
      return
    }
    storage.addRecord({
      amount,
      merchant: this.data.merchant,
      category: this.data.category,
      date: this.data.date,
      note: this.data.note,
      source: 'manual'
    })
    wx.showToast({ title: '已记账', icon: 'success' })
    setTimeout(() => wx.navigateBack({ delta: 1 }), 400)
  }
})
