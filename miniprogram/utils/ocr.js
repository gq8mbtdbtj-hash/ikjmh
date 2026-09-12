/**
 * 小票解析：
 * - mock：演示规则（可从文件名猜金额）+ 占位商户，需用户确认
 * - cloud：调用云函数 parseReceipt（需配置云开发）
 */

const storage = require('./storage')

function guessAmountFromName(path) {
  if (!path) return null
  const m = String(path).match(/(\d+)(?:[._-](\d{1,2}))?/)
  if (!m) return null
  const major = Number(m[1])
  if (!Number.isFinite(major) || major <= 0 || major > 99999) return null
  const minor = m[2] != null ? m[2].padEnd(2, '0') : '00'
  return Number(`${major}.${minor}`)
}

function mockParse(filePath) {
  const amount = guessAmountFromName(filePath)
  return {
    ok: true,
    mode: 'mock',
    amount: amount != null ? amount : 32.5,
    merchant: amount != null ? '识别商家（请核对）' : '演示便利店',
    date: storage.todayStr(),
    category: 'food',
    rawText: '【演示解析】未接 OCR Key 时使用本地规则；请核对金额与商户后保存。',
    confidence: amount != null ? 0.55 : 0.35
  }
}

function cloudParse(filePath) {
  return new Promise((resolve) => {
    if (!wx.cloud) {
      resolve(Object.assign(mockParse(filePath), { mode: 'mock' }))
      return
    }
    wx.cloud.uploadFile({
      cloudPath: `receipts/${Date.now()}.jpg`,
      filePath,
      success: (up) => {
        wx.cloud
          .callFunction({
            name: 'parseReceipt',
            data: { fileID: up.fileID }
          })
          .then((res) => {
            const data = (res && res.result) || {}
            if (!data.ok) {
              resolve(Object.assign(mockParse(filePath), { mode: 'cloud-fallback' }))
              return
            }
            resolve({
              ok: true,
              mode: 'cloud',
              amount: Number(data.amount) || 0,
              merchant: data.merchant || '',
              date: data.date || storage.todayStr(),
              category: data.category || 'other',
              rawText: data.rawText || '',
              confidence: data.confidence != null ? data.confidence : 0.8
            })
          })
          .catch(() => resolve(Object.assign(mockParse(filePath), { mode: 'cloud-fallback' })))
      },
      fail: () => resolve(mockParse(filePath))
    })
  })
}

async function parseReceipt(filePath, preferCloud) {
  if (preferCloud) return cloudParse(filePath)
  return mockParse(filePath)
}

module.exports = {
  parseReceipt,
  mockParse
}
