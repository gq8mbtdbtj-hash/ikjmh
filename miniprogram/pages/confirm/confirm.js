const storage = require('../../utils/storage')

Page({
  data: {
    imagePath: '',
    amount: '',
    merchant: '',
    category: 'food',
    date: '',
    note: '',
    rawText: '',
    modeLabel: '演示',
    categories: []
  },

  onLoad(query) {
    const app = getApp()
    let parsed = {}
    try {
      parsed = JSON.parse(decodeURIComponent(query.data || '{}'))
    } catch (e) {
      parsed = {}
    }
    const mode = parsed.mode || 'mock'
    let modeLabel = '演示解析'
    if (mode === 'cloud') modeLabel = '云 OCR'
    if (mode === 'cloud-fallback') modeLabel = '云失败·回退演示'

    this.setData({
      categories: app.globalData.categories,
      imagePath: parsed.imagePath || '',
      amount: parsed.amount != null ? String(parsed.amount) : '',
      merchant: parsed.merchant || '',
      category: parsed.category || 'food',
      date: parsed.date || storage.todayStr(),
      rawText: parsed.rawText || '',
      modeLabel
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

  async onSave() {
    const amount = Number(this.data.amount)
    if (!amount || amount <= 0) {
      wx.showToast({ title: '请核对金额', icon: 'none' })
      return
    }
    const imagePath = await storage.persistImage(this.data.imagePath)
    storage.addRecord({
      amount,
      merchant: this.data.merchant,
      category: this.data.category,
      date: this.data.date,
      note: this.data.note,
      imagePath,
      source: 'receipt'
    })
    wx.showToast({ title: '已入账', icon: 'success' })
    setTimeout(() => {
      wx.switchTab({ url: '/pages/index/index' })
    }, 400)
  }
})
