const ocr = require('../../utils/ocr')

Page({
  data: {
    imagePath: '',
    parsing: false
  },

  chooseCamera() {
    this.pick(['camera'])
  },

  chooseAlbum() {
    this.pick(['album'])
  },

  pick(sourceType) {
    wx.chooseMedia({
      count: 1,
      mediaType: ['image'],
      sourceType,
      success: (res) => {
        const file = res.tempFiles && res.tempFiles[0]
        if (!file) return
        this.setData({ imagePath: file.tempFilePath })
      }
    })
  },

  async onParse() {
    if (!this.data.imagePath || this.data.parsing) return
    this.setData({ parsing: true })
    try {
      const app = getApp()
      const preferCloud = !!(app.globalData && app.globalData.preferCloudOcr)
      const result = await ocr.parseReceipt(this.data.imagePath, preferCloud)
      const payload = encodeURIComponent(
        JSON.stringify({
          imagePath: this.data.imagePath,
          amount: result.amount,
          merchant: result.merchant,
          date: result.date,
          category: result.category,
          rawText: result.rawText,
          mode: result.mode,
          confidence: result.confidence
        })
      )
      wx.navigateTo({ url: `/pages/confirm/confirm?data=${payload}` })
    } catch (e) {
      wx.showToast({ title: '解析失败', icon: 'none' })
    } finally {
      this.setData({ parsing: false })
    }
  }
})
