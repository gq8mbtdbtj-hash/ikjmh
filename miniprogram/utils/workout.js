/** 运动记录本地存储 */

const KEY = 'receipt_ledger_workouts_v1'

function ensureReady() {
  const raw = wx.getStorageSync(KEY)
  if (!raw || typeof raw !== 'object') {
    wx.setStorageSync(KEY, { workouts: [] })
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
  return `w_${Date.now()}_${Math.floor(Math.random() * 10000)}`
}

function addWorkout(workout) {
  const data = load()
  const item = {
    id: uid(),
    createdAt: Date.now(),
    type: workout.type || 'walk',
    startedAt: workout.startedAt,
    endedAt: workout.endedAt,
    durationMs: workout.durationMs || 0,
    distanceM: workout.distanceM || 0,
    points: workout.points || [],
    note: workout.note || ''
  }
  data.workouts = [item, ...(data.workouts || [])]
  // 限制点数过大：超过 2000 点做稀疏采样已在页面侧处理；这里再兜底截断
  if (item.points.length > 3000) {
    item.points = item.points.filter((_, i) => i % 2 === 0).slice(0, 3000)
  }
  save(data)
  return item
}

function listWorkouts() {
  return load().workouts || []
}

function getWorkout(id) {
  return listWorkouts().find((w) => w.id === id) || null
}

function deleteWorkout(id) {
  const data = load()
  data.workouts = (data.workouts || []).filter((w) => w.id !== id)
  save(data)
}

function todayDistanceM(todayStr) {
  return listWorkouts()
    .filter((w) => {
      const d = new Date(w.startedAt)
      const y = d.getFullYear()
      const m = `${d.getMonth() + 1}`.padStart(2, '0')
      const day = `${d.getDate()}`.padStart(2, '0')
      return `${y}-${m}-${day}` === todayStr
    })
    .reduce((s, w) => s + (Number(w.distanceM) || 0), 0)
}

module.exports = {
  ensureReady,
  addWorkout,
  listWorkouts,
  getWorkout,
  deleteWorkout,
  todayDistanceM
}
