/**
 * 云函数骨架：上传的小票 fileID → OCR → 结构化字段
 *
 * 部署后请在微信开发者工具上传并部署。
 * 未实现真实 OCR 时返回 ok:false，客户端会回退演示解析。
 */
const cloud = require('wx-server-sdk')
cloud.init({ env: cloud.DYNAMIC_CURRENT_ENV })

exports.main = async (event) => {
  const fileID = event.fileID || event.fileId
  if (!fileID) {
    return { ok: false, error: 'missing fileID' }
  }

  // TODO: 下载 fileID → 调用腾讯云通用印刷体 OCR
  // 解析 amount / merchant / date 后返回：
  // return {
  //   ok: true,
  //   amount: 32.5,
  //   merchant: '某某超市',
  //   date: '2026-09-12',
  //   category: 'food',
  //   rawText: '...',
  //   confidence: 0.9
  // }

  return {
    ok: false,
    error: 'OCR not configured',
    hint: 'Wire Tencent Cloud OCR here, then set preferCloudOcr=true in app.js'
  }
}
