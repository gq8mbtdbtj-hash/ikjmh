#include "tray_demo/util/fuzzy_match.hpp"

#include <algorithm>
#include <cctype>

namespace tray_demo {
namespace {

std::string ToLowerAscii(std::string s) {
  for (std::size_t i = 0; i < s.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 128) {
      s[i] = static_cast<char>(std::tolower(c));
    }
  }
  return s;
}

bool ContainsIgnoreCase(const std::string& hay, const std::string& needle) {
  if (needle.empty()) {
    return true;
  }
  const std::string h = ToLowerAscii(hay);
  const std::string n = ToLowerAscii(needle);
  return h.find(n) != std::string::npos;
}

}  // namespace

bool FuzzyMatch(const std::string& text, const std::string& query) {
  return FuzzyScore(text, query) >= 0;
}

int FuzzyScore(const std::string& text, const std::string& query) {
  if (query.empty()) {
    return 0;
  }
  if (ContainsIgnoreCase(text, query)) {
    return 1000 + static_cast<int>(query.size()) * 10;
  }

  const std::string t = ToLowerAscii(text);
  const std::string q = ToLowerAscii(query);
  std::size_t ti = 0;
  int score = 0;
  int streak = 0;
  for (std::size_t qi = 0; qi < q.size(); ++qi) {
    bool found = false;
    while (ti < t.size()) {
      if (t[ti] == q[qi]) {
        ++streak;
        score += 2 + streak;
        ++ti;
        found = true;
        break;
      }
      streak = 0;
      ++ti;
    }
    if (!found) {
      return -1;
    }
  }
  return score;
}

}  // namespace tray_demo
