/** 地理与运动展示工具 */

const R = 6371000 // meters

function toRad(d) {
  return (d * Math.PI) / 180
}

/** 两点球面距离（米），坐标为 GCJ-02 / WGS 近似 */
function haversine(a, b) {
  if (!a || !b) return 0
  const dLat = toRad(b.latitude - a.latitude)
  const dLng = toRad(b.longitude - a.longitude)
  const lat1 = toRad(a.latitude)
  const lat2 = toRad(b.latitude)
  const h =
    Math.sin(dLat / 2) * Math.sin(dLat / 2) +
    Math.cos(lat1) * Math.cos(lat2) * Math.sin(dLng / 2) * Math.sin(dLng / 2)
  return 2 * R * Math.asin(Math.min(1, Math.sqrt(h)))
}

function pathDistance(points) {
  let sum = 0
  for (let i = 1; i < points.length; i++) {
    sum += haversine(points[i - 1], points[i])
  }
  return sum
}

function formatDistance(meters) {
  const m = Number(meters) || 0
  if (m < 1000) return `${Math.round(m)} m`
  return `${(m / 1000).toFixed(2)} km`
}

function formatDuration(ms) {
  const sec = Math.max(0, Math.floor((Number(ms) || 0) / 1000))
  const h = Math.floor(sec / 3600)
  const m = Math.floor((sec % 3600) / 60)
  const s = sec % 60
  if (h > 0) {
    return `${h}:${`${m}`.padStart(2, '0')}:${`${s}`.padStart(2, '0')}`
  }
  return `${`${m}`.padStart(2, '0')}:${`${s}`.padStart(2, '0')}`
}

/** 配速 min/km */
function formatPace(meters, ms) {
  const km = (Number(meters) || 0) / 1000
  if (km < 0.01) return '--:--'
  const minPerKm = (Number(ms) || 0) / 60000 / km
  if (!Number.isFinite(minPerKm) || minPerKm <= 0 || minPerKm > 99) return '--:--'
  const whole = Math.floor(minPerKm)
  const sec = Math.round((minPerKm - whole) * 60)
  return `${whole}:${`${sec}`.padStart(2, '0')}`
}

const SPORT_TYPES = [
  { id: 'walk', name: '步行' },
  { id: 'run', name: '跑步' },
  { id: 'ride', name: '骑行' }
]

module.exports = {
  haversine,
  pathDistance,
  formatDistance,
  formatDuration,
  formatPace,
  SPORT_TYPES
}
