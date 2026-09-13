const workout = require('../../utils/workout')
const geo = require('../../utils/geo')

function typeName(id) {
  const hit = geo.SPORT_TYPES.find((t) => t.id === id)
  return hit ? hit.name : '运动'
}

function timeText(ts) {
  const d = new Date(ts)
  const y = d.getFullYear()
  const m = `${d.getMonth() + 1}`.padStart(2, '0')
  const day = `${d.getDate()}`.padStart(2, '0')
  const hh = `${d.getHours()}`.padStart(2, '0')
  const mm = `${d.getMinutes()}`.padStart(2, '0')
  return `${y}-${m}-${day} ${hh}:${mm}`
}

Page({
  data: {
    item: null,
    typeName: '',
    timeText: '',
    distanceText: '',
    durationText: '',
    paceText: '',
    pointCount: 0,
    latitude: 39.9,
    longitude: 116.4,
    scale: 15,
    polyline: [],
    markers: [],
    includePoints: []
  },

  onLoad(query) {
    const item = workout.getWorkout(query.id)
    if (!item) {
      this.setData({ item: null })
      return
    }
    const points = (item.points || []).map((p) => ({
      latitude: p.latitude,
      longitude: p.longitude
    }))
    const start = points[0]
    const end = points[points.length - 1] || start
    this.setData({
      item,
      typeName: typeName(item.type),
      timeText: timeText(item.startedAt),
      distanceText: geo.formatDistance(item.distanceM),
      durationText: geo.formatDuration(item.durationMs),
      paceText: geo.formatPace(item.distanceM, item.durationMs),
      pointCount: points.length,
      latitude: start ? start.latitude : 39.9,
      longitude: start ? start.longitude : 116.4,
      includePoints: points,
      polyline: points.length
        ? [
            {
              points,
              color: '#0f766e',
              width: 6,
              arrowLine: true
            }
          ]
        : [],
      markers: start
        ? [
            {
              id: 1,
              latitude: start.latitude,
              longitude: start.longitude,
              width: 24,
              height: 24,
              title: '起'
            },
            {
              id: 2,
              latitude: end.latitude,
              longitude: end.longitude,
              width: 24,
              height: 24,
              title: '终'
            }
          ]
        : []
    })
  },

  onDelete() {
    const id = this.data.item && this.data.item.id
    if (!id) return
    wx.showModal({
      title: '删除这次运动？',
      success: (res) => {
        if (!res.confirm) return
        workout.deleteWorkout(id)
        wx.navigateBack({ delta: 1 })
      }
    })
  }
})
