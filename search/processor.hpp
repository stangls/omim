#pragma once
#include "search/geocoder.hpp"
#include "search/keyword_lang_matcher.hpp"
#include "search/mode.hpp"
#include "search/pre_ranker.hpp"
#include "search/ranker.hpp"
#include "search/rank_table_cache.hpp"
#include "search/reverse_geocoder.hpp"
#include "search/search_trie.hpp"
#include "search/suggest.hpp"
#include "search/token_slice.hpp"

#include "indexer/ftypes_matcher.hpp"
#include "indexer/index.hpp"
#include "indexer/rank_table.hpp"
#include "indexer/string_slice.hpp"

#include "geometry/rect2d.hpp"

#include "base/buffer_vector.hpp"
#include "base/cancellable.hpp"
#include "base/limited_priority_queue.hpp"
#include "base/string_utils.hpp"

#include "std/function.hpp"
#include "std/map.hpp"
#include "std/string.hpp"
#include "std/unordered_set.hpp"
#include "std/unique_ptr.hpp"
#include "std/vector.hpp"

#define FIND_LOCALITY_TEST

#ifdef FIND_LOCALITY_TEST
#include "search/locality_finder.hpp"
#endif

class FeatureType;
class CategoriesHolder;

namespace coding
{
class CompressedBitVector;
}

namespace storage
{
class CountryInfoGetter;
}

namespace search
{
struct Locality;
struct Region;
struct QueryParams;
class ReverseGeocoder;

class Geocoder;
class Ranker;
// todo(@m) Merge with Ranker.
class PreResult2Maker;

class FeatureLoader;
class BestNameFinder;
class DoFindLocality;
class HouseCompFactory;

class Processor : public my::Cancellable
{
public:
  // Maximum result candidates count for each viewport/criteria.
  static size_t const kPreResultsCount = 200;

  static double const kMinViewportRadiusM;
  static double const kMaxViewportRadiusM;

  Processor(Index & index, CategoriesHolder const & categories, vector<Suggest> const & suggests,
            storage::CountryInfoGetter const & infoGetter);

  inline void SupportOldFormat(bool b) { m_supportOldFormat = b; }

  void Init(bool viewportSearch);

  /// @param[in]  forceUpdate Pass true (default) to recache feature's ids even
  /// if viewport is a part of the old cached rect.
  void SetViewport(m2::RectD const & viewport, bool forceUpdate);
  void SetPreferredLocale(string const & locale);
  void SetInputLocale(string const & locale);
  void SetQuery(string const & query);
  // TODO (@y): this function must be removed.
  void SetRankPivot(m2::PointD const & pivot);
  inline void SetMode(Mode mode) { m_mode = mode; }
  inline void SetSearchInWorld(bool b) { m_worldSearch = b; }
  inline void SetSuggestsEnabled(bool enabled) { m_suggestsEnabled = enabled; }
  inline void SetPosition(m2::PointD const & position) { m_position = position; }

  inline string const & GetPivotRegion() const { return m_region; }
  inline m2::PointD const & GetPosition() const { return m_position; }

  /// Suggestions language code, not the same as we use in mwm data
  int8_t m_inputLocaleCode, m_currentLocaleCode;

  inline bool IsEmptyQuery() const { return (m_prefix.empty() && m_tokens.empty()); }

  /// @name Various search functions.
  //@{
  void Search(Results & results, size_t limit);
  void SearchViewportPoints(Results & results);

  // Tries to generate a (lat, lon) result from |m_query|.
  void SearchCoordinates(Results & res) const;
  //@}

  struct CancelException
  {
  };

  void InitParams(QueryParams & params);

  void ClearCaches();

protected:
  enum ViewportID
  {
    DEFAULT_V = -1,
    CURRENT_V = 0,
    LOCALITY_V = 1,
    COUNT_V = 2  // Should always be the last
  };

  friend string DebugPrint(ViewportID viewportId);

  friend class BestNameFinder;
  friend class DoFindLocality;
  friend class FeatureLoader;
  friend class HouseCompFactory;
  friend class PreResult2Maker;
  friend class Ranker;

  int GetCategoryLocales(int8_t(&arr)[3]) const;

  void ForEachCategoryType(
      StringSliceBase const & slice,
      function<void(size_t /* tokenId */, uint32_t /* typeId */)> const & fn) const;

  void ProcessEmojiIfNeeded(
      strings::UniString const & token, size_t index,
      function<void(size_t /* tokenId */, uint32_t /* typeId */)> const & fn) const;

  using TMWMVector = vector<shared_ptr<MwmInfo>>;
  using TOffsetsVector = map<MwmSet::MwmId, vector<uint32_t>>;
  using TFHeader = feature::DataHeader;

  m2::PointD GetPivotPoint() const;
  m2::RectD GetPivotRect() const;

  void SetViewportByIndex(m2::RectD const & viewport, size_t idx, bool forceUpdate);
  void ClearCache(size_t ind);

  void RemoveStringPrefix(string const & str, string & res) const;
  void GetSuggestion(string const & name, string & suggest) const;

  void ProcessSuggestions(vector<IndexedValue> & vec, Results & res) const;

  void SuggestStrings(Results & res);
  void MatchForSuggestionsImpl(strings::UniString const & token, int8_t locale,
                               string const & prolog, Results & res);

  void GetBestMatchName(FeatureType const & f, string & name) const;

  Result MakeResult(PreResult2 const & r) const;
  void MakeResultHighlight(Result & res) const;

  Index & m_index;
  CategoriesHolder const & m_categories;
  vector<Suggest> const & m_suggests;
  storage::CountryInfoGetter const & m_infoGetter;

  string m_region;
  string m_query;
  buffer_vector<strings::UniString, 32> m_tokens;
  strings::UniString m_prefix;
  set<uint32_t> m_prefferedTypes;

#ifdef FIND_LOCALITY_TEST
  mutable LocalityFinder m_locality;
#endif

  m2::RectD m_viewport[COUNT_V];
  m2::PointD m_pivot;
  m2::PointD m_position;
  Mode m_mode;
  bool m_worldSearch;
  bool m_suggestsEnabled;

  /// @name Get ranking params.
  //@{
  /// @return Rect for viewport-distance calculation.
  m2::RectD const & GetViewport(ViewportID vID = DEFAULT_V) const;
  //@}

  void SetLanguage(int id, int8_t lang);
  int8_t GetLanguage(int id) const;

  KeywordLangMatcher m_keywordsScorer;

  bool m_supportOldFormat;

protected:
  bool m_viewportSearch;

  PreRanker m_preRanker;
  Ranker m_ranker;
  Geocoder m_geocoder;
  ReverseGeocoder const m_reverseGeocoder;
};
}  // namespace search
