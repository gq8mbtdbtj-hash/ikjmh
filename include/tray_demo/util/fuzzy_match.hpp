#pragma once

/**
 * @file fuzzy_match.hpp
 * @brief 简易模糊匹配（子序列 + 子串），供 demo 列表面板过滤。
 *
 * @customize 可换成更重的评分算法；接口保持 FuzzyMatch / FuzzyScore。
 */

#include <string>

namespace tray_demo {

/**
 * @brief 查询为空则匹配；否则要求 query 字符按序出现在 text 中（大小写不敏感 ASCII）。
 * 同时：若 text 包含 query 子串则直接匹配（利于中文关键词）。
 */
bool FuzzyMatch(const std::string& text, const std::string& query);

/**
 * @brief 匹配分数，越大越好；不匹配返回负数。
 */
int FuzzyScore(const std::string& text, const std::string& query);

}  // namespace tray_demo
