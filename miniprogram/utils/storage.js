const KEY = 'receipt_ledger_v1'

function ensureReady() {
  const raw = wx.getStorageSync(KEY)
  if (!raw || typeof raw !== 'object') {
    wx.setStorageSync(KEY, { records: [], streak: { lastDate: '', count: 0 } })
  }
}

function load() {
  ensureReady()
  return wx.getStorageSync(KEY)
}

function save(data) {
  wx.setStorageSync(KEY, data)
}

function uid() {
  return `r_${Date.now()}_${Math.floor(Math.random() * 10000)}`
}

function todayStr(d = new Date()) {
  const y = d.getFullYear()
  const m = `${d.getMonth() + 1}`.padStart(2, '0')
  const day = `${d.getDate()}`.padStart(2, '0')
  return `${y}-${m}-${day}`
}

function bumpStreak(data, dateStr) {
  const s = data.streak || { lastDate: '', count: 0 }
  if (s.lastDate === dateStr) {
    data.streak = s
    return data
  }
  const yesterday = new Date(`${dateStr}T12:00:00`)
  yesterday.setDate(yesterday.getDate() - 1)
  const yStr = todayStr(yesterday)
  if (s.lastDate === yStr) {
    s.count = (s.count || 0) + 1
  } else {
    s.count = 1
  }
  s.lastDate = dateStr
  data.streak = s
  return data
}

function addRecord(record) {
  const data = load()
  const item = {
    id: uid(),
    createdAt: Date.now(),
    date: record.date || todayStr(),
    amount: Number(record.amount) || 0,
    merchant: (record.merchant || '').trim() || '未命名商家',
    category: record.category || 'other',
    note: record.note || '',
    imagePath: record.imagePath || '',
    source: record.source || 'manual'
  }
  data.records = [item, ...(data.records || [])]
  bumpStreak(data, item.date)
  save(data)
  return item
}

function listRecords() {
  return load().records || []
}

function deleteRecord(id) {
  const data = load()
  data.records = (data.records || []).filter((r) => r.id !== id)
  save(data)
}

function getStreak() {
  return load().streak || { lastDate: '', count: 0 }
}

function sumByDate(dateStr) {
  return listRecords()
    .filter((r) => r.date === dateStr)
    .reduce((s, r) => s + (Number(r.amount) || 0), 0)
}

function monthKey(dateStr) {
  return (dateStr || '').slice(0, 7)
}

function statsForMonth(ym) {
  const rows = listRecords().filter((r) => monthKey(r.date) === ym)
  const total = rows.reduce((s, r) => s + (Number(r.amount) || 0), 0)
  const byCat = {}
  rows.forEach((r) => {
    byCat[r.category] = (byCat[r.category] || 0) + (Number(r.amount) || 0)
  })
  return { total, count: rows.length, byCat, rows }
}

function persistImage(tempPath) {
  return new Promise((resolve) => {
    if (!tempPath) {
      resolve('')
      return
    }
    const fs = wx.getFileSystemManager()
    const dest = `${wx.env.USER_DATA_PATH}/receipt_${Date.now()}.jpg`
    fs.saveFile({
      tempFilePath: tempPath,
      filePath: dest,
      success: () => resolve(dest),
      fail: () => resolve(tempPath)
    })
  })
}

module.exports = {
  ensureReady,
  addRecord,
  listRecords,
  deleteRecord,
  getStreak,
  sumByDate,
  statsForMonth,
  todayStr,
  monthKey,
  persistImage
}
