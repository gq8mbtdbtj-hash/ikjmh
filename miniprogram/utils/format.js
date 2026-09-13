function yuan(n) {
  const v = Number(n) || 0
  return v.toFixed(2)
}

function displayAmount(n) {
  return `¥${yuan(n)}`
}

function categoryName(id, categories) {
  const hit = (categories || []).find((c) => c.id === id)
  return hit ? hit.name : '其他'
}

module.exports = {
  yuan,
  displayAmount,
  categoryName
}
