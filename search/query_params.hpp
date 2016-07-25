#pragma once

#include "base/string_utils.hpp"

#include "std/cstdint.hpp"
#include "std/unordered_set.hpp"
#include "std/vector.hpp"

namespace search
{
struct QueryParams
{
  using TString = strings::UniString;
  using TSynonymsVector = vector<TString>;
  using TLangsSet = unordered_set<int8_t>;

  vector<TSynonymsVector> m_tokens;
  TSynonymsVector m_prefixTokens;
  vector<bool> m_isCategorySynonym;

  TLangsSet m_langs;
  int m_scale;

  QueryParams();

  void Clear();

  /// @param[in] eraseInds Sorted vector of token's indexes.
  void EraseTokens(vector<size_t> & eraseInds);

  void ProcessAddressTokens();

  inline bool IsEmpty() const { return (m_tokens.empty() && m_prefixTokens.empty()); }
  inline bool CanSuggest() const { return (m_tokens.empty() && !m_prefixTokens.empty()); }
  inline bool IsLangExist(int8_t l) const { return (m_langs.count(l) > 0); }

  TSynonymsVector const & GetTokens(size_t i) const;
  TSynonymsVector & GetTokens(size_t i);
  inline size_t GetNumTokens() const
  {
    return m_prefixTokens.empty() ? m_tokens.size() : m_tokens.size() + 1;
  }

  /// @return true if all tokens in [start, end) range has number synonym.
  bool IsNumberTokens(size_t start, size_t end) const;

private:
  template <class ToDo>
  void ForEachToken(ToDo && toDo);
};

string DebugPrint(search::QueryParams const & params);
}  // namespace search
