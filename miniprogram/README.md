# 小票账本（微信小程序）

拍小票 / 手动记账 / 月度统计 / 连续打卡。

## 打开方式

1. 安装 [微信开发者工具](https://developers.weixin.qq.com/miniprogram/dev/devtools/download.html)
2. 导入本目录：`miniprogram/`
3. AppID 可用测试号；`project.config.json` 里当前是 `touristappid`
4. 编译预览即可（数据存本地 Storage，无需后端）

## MVP 能力

| 功能 | 说明 |
|------|------|
| 拍小票入账 | 拍照/相册 → 解析 → 确认页改金额 → 保存 |
| 手动记一笔 | 金额 / 商户 / 分类 / 日期 |
| 今日与连续打卡 | 首页今日合计 + 连续记账天数 |
| 明细 | 按日分组；长按删除 |
| 统计 | 按月合计 + 分类占比 |
| **运动轨迹（次要）** | 不占 Tab；首页入口进入；步行/跑步/骑行 GPS 折线与历史 |

## 信息架构

- Tab：**今日 / 明细 / 统计**（记账主线）
- 运动：首页「去运动」进入二级页，避免与账本抢一级导航

## 运动轨迹说明

- 使用 `wx.startLocationUpdate` + `onLocationChange` 前台持续定位（GCJ-02）
- 切到后台会自动暂停，回到前台可继续（真后台轨迹需微信额外资质）
- 隐私：`app.json` 已声明 `getLocation` / `onLocationChange` / `startLocationUpdate`
- 轨迹保存在本地 Storage；过长轨迹会稀疏采样以防爆仓

## 小票解析说明

默认 **演示解析**（本地规则，可从文件名猜金额），**必须在确认页核对**。

接真 OCR：

1. 开通微信云开发，创建云函数 `parseReceipt`（见 `cloudfunctions/parseReceipt`）
2. 在云函数里调用腾讯云 OCR / 其它识别服务
3. `app.js` 里设 `globalData.preferCloudOcr = true`
4. 取消注释 `wx.cloud.init({ env: '你的环境ID' })`

## 目录

```
miniprogram/
  pages/          # 今日 / 明细 / 统计；二级：记一笔 / 拍小票 / 确认 / 运动
  utils/          # storage / ocr / format / geo / workout
  cloudfunctions/ # OCR 云函数骨架
  assets/         # tab 图标
```

## 第二版候选

- 附近午餐收藏 + 今日随机
- 导出 CSV
- 多人账本
- 运动后台持续定位（需开通）
- 运动配速曲线 / 卡路里估算
