const geo = require('../../utils/geo')
const workout = require('../../utils/workout')

const MIN_POINT_GAP_M = 3
const MAX_POINTS = 2500

Page({
  data: {
    types: geo.SPORT_TYPES,
    sportType: 'walk',
    tracking: false,
    paused: false,
    latitude: 39.9042,
    longitude: 116.4074,
    scale: 16,
    polyline: [],
    markers: [],
    distanceText: '0 m',
    durationText: '00:00',
    paceText: '--:--',
    locError: ''
  },

  _points: [],
  _distanceM: 0,
  _startedAt: 0,
  _elapsedMs: 0,
  _tickTimer: null,
  _locHandler: null,

  onLoad() {
    workout.ensureReady()
    this._locateOnce()
  },

  onUnload() {
    this._teardownLocation()
    this._clearTick()
  },

  onHide() {
    if (this.data.tracking) {
      this.onPause()
      wx.showToast({ title: '已暂停（回前台可继续）', icon: 'none' })
    }
  },

  onType(e) {
    if (this.data.tracking || this.data.paused) return
    this.setData({ sportType: e.currentTarget.dataset.id })
  },

  goHistory() {
    wx.navigateTo({ url: '/pages/sport-history/sport-history' })
  },

  _locateOnce() {
    wx.getLocation({
      type: 'gcj02',
      isHighAccuracy: true,
      success: (res) => {
        this.setData({
          latitude: res.latitude,
          longitude: res.longitude,
          locError: ''
        })
      },
      fail: () => {
        this.setData({ locError: '定位失败，请允许位置权限后重试' })
      }
    })
  },

  _ensureAuth() {
    return new Promise((resolve) => {
      wx.getSetting({
        success: (res) => {
          if (res.authSetting['scope.userLocation']) {
            resolve(true)
            return
          }
          wx.authorize({
            scope: 'scope.userLocation',
            success: () => resolve(true),
            fail: () => {
              wx.showModal({
                title: '需要位置权限',
                content: '记录运动轨迹需要定位权限',
                confirmText: '去设置',
                success: (r) => {
                  if (r.confirm) wx.openSetting({})
                }
              })
              resolve(false)
            }
          })
        },
        fail: () => resolve(false)
      })
    })
  },

  async onStart() {
    const ok = await this._ensureAuth()
    if (!ok) return

    this._points = []
    this._distanceM = 0
    this._elapsedMs = 0
    this._startedAt = Date.now()
    this.setData({
      tracking: true,
      paused: false,
      polyline: [],
      markers: [],
      distanceText: '0 m',
      durationText: '00:00',
      paceText: '--:--',
      locError: ''
    })
    this._startTick()
    this._startLocation()
  },

  onPause() {
    if (!this.data.tracking) return
    this._elapsedMs += Date.now() - this._startedAt
    this._teardownLocation()
    this._clearTick()
    this.setData({ tracking: false, paused: true })
    this._refreshMetrics()
  },

  async onResume() {
    if (!this.data.paused) return
    const ok = await this._ensureAuth()
    if (!ok) return
    this._startedAt = Date.now()
    this.setData({ tracking: true, paused: false, locError: '' })
    this._startTick()
    this._startLocation()
  },

  onStop() {
    if (!this.data.tracking && !this.data.paused) return
    if (this.data.tracking) {
      this._elapsedMs += Date.now() - this._startedAt
    }
    this._teardownLocation()
    this._clearTick()

    const points = this._points.slice()
    const distanceM = this._distanceM
    const durationMs = this._elapsedMs
    this.setData({ tracking: false, paused: false })
    this._refreshMetrics()

    if (points.length < 2 || distanceM < 10) {
      wx.showModal({
        title: '轨迹太短',
        content: '未达到可保存的最短距离，是否丢弃？',
        confirmText: '丢弃',
        success: (res) => {
          if (res.confirm) this._resetSession()
          else this.setData({ paused: true })
        }
      })
      return
    }

    const item = workout.addWorkout({
      type: this.data.sportType,
      startedAt: Date.now() - durationMs,
      endedAt: Date.now(),
      durationMs,
      distanceM,
      points
    })
    wx.showToast({ title: '已保存', icon: 'success' })
    this._resetSession()
    setTimeout(() => {
      wx.navigateTo({ url: `/pages/sport-detail/sport-detail?id=${item.id}` })
    }, 400)
  },

  _resetSession() {
    this._points = []
    this._distanceM = 0
    this._elapsedMs = 0
    this._startedAt = 0
    this.setData({
      tracking: false,
      paused: false,
      polyline: [],
      markers: [],
      distanceText: '0 m',
      durationText: '00:00',
      paceText: '--:--'
    })
  },

  _startTick() {
    this._clearTick()
    this._tickTimer = setInterval(() => this._refreshMetrics(), 1000)
  },

  _clearTick() {
    if (this._tickTimer) {
      clearInterval(this._tickTimer)
      this._tickTimer = null
    }
  },

  _refreshMetrics() {
    const durationMs =
      this._elapsedMs + (this.data.tracking ? Date.now() - this._startedAt : 0)
    this.setData({
      distanceText: geo.formatDistance(this._distanceM),
      durationText: geo.formatDuration(durationMs),
      paceText: geo.formatPace(this._distanceM, durationMs)
    })
  },

  _startLocation() {
    this._teardownLocation()
    this._locHandler = (res) => this._onLocation(res)
    wx.onLocationChange(this._locHandler)
    wx.startLocationUpdate({
      type: 'gcj02',
      success: () => {},
      fail: () => {
        this.setData({ locError: '无法开启持续定位，请检查权限' })
        this.onPause()
      }
    })
  },

  _teardownLocation() {
    if (this._locHandler) {
      try {
        wx.offLocationChange(this._locHandler)
      } catch (e) {}
      this._locHandler = null
    }
    try {
      wx.stopLocationUpdate({})
    } catch (e) {}
  },

  _onLocation(res) {
    if (!this.data.tracking) return
    if (res.latitude == null || res.longitude == null) return

    const point = {
      latitude: res.latitude,
      longitude: res.longitude,
      altitude: res.altitude || 0,
      speed: res.speed || 0,
      accuracy: res.accuracy || 0,
      t: Date.now()
    }

    if (point.accuracy && point.accuracy > 80) return

    const last = this._points[this._points.length - 1]
    if (last) {
      const gap = geo.haversine(last, point)
      if (gap < MIN_POINT_GAP_M) return
      if (gap > 80 && point.accuracy > 40) return
      this._distanceM += gap
    }

    this._points.push(point)
    if (this._points.length > MAX_POINTS) {
      this._points = this._points.filter((_, i) => i % 2 === 0)
    }

    const line = this._points.map((p) => ({
      latitude: p.latitude,
      longitude: p.longitude
    }))
    const start = line[0]
    const end = line[line.length - 1]
    this.setData({
      latitude: point.latitude,
      longitude: point.longitude,
      polyline: [
        {
          points: line,
          color: '#0f766e',
          width: 6,
          arrowLine: true
        }
      ],
      markers: [
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
          title: '今'
        }
      ]
    })
    this._refreshMetrics()
  }
})
