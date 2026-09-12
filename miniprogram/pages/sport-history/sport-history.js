const workout = require('../../utils/workout')
const geo = require('../../utils/geo')

function typeName(id) {
  const hit = geo.SPORT_TYPES.find((t) => t.id === id)
  return hit ? hit.name : '运动'
}

function timeText(ts) {
  const d = new Date(ts)
  const m = `${d.getMonth() + 1}`.padStart(2, '0')
  const day = `${d.getDate()}`.padStart(2, '0')
  const hh = `${d.getHours()}`.padStart(2, '0')
  const mm = `${d.getMinutes()}`.padStart(2, '0')
  return `${m}-${day} ${hh}:${mm}`
}

Page({
  data: {
    count: 0,
    list: []
  },

  onShow() {
    this.refresh()
  },

  refresh() {
    const list = workout.listWorkouts().map((w) => ({
      ...w,
      typeName: typeName(w.type),
      distanceText: geo.formatDistance(w.distanceM),
      durationText: geo.formatDuration(w.durationMs),
      paceText: geo.formatPace(w.distanceM, w.durationMs),
      timeText: timeText(w.startedAt)
    }))
    this.setData({ list, count: list.length })
  },

  onOpen(e) {
    wx.navigateTo({
      url: `/pages/sport-detail/sport-detail?id=${e.currentTarget.dataset.id}`
    })
  },

  onDelete(e) {
    const id = e.currentTarget.dataset.id
    wx.showModal({
      title: '删除这次运动？',
      success: (res) => {
        if (!res.confirm) return
        workout.deleteWorkout(id)
        this.refresh()
      }
    })
  }
})
