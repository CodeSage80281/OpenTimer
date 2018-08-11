#include <ot/timer/timer.hpp>

namespace ot {
  
// Constructor
PfxtNode::PfxtNode(float s, size_t f, size_t t, const Arc* a, const PfxtNode* p) :
  slack  {s},
  from   {f},
  to     {t},
  arc    {a},
  parent {p} {
}

// ------------------------------------------------------------------------------------------------

// Constructor
PfxtCache::PfxtCache(const SfxtCache& sfxt) : _sfxt {sfxt} {
}

// Move constructor
PfxtCache::PfxtCache(PfxtCache&& pfxt) : 
  _sfxt  {pfxt._sfxt},
  _comp  {pfxt._comp},
  _paths {std::move(pfxt._paths)},
  _nodes {std::move(pfxt._nodes)} {
}

// Procedure: _push
void PfxtCache::_push(float s, size_t f, size_t t, const Arc* a, const PfxtNode* p) {
  _nodes.emplace_back(std::make_unique<PfxtNode>(s, f, t, a, p));
  std::push_heap(_nodes.begin(), _nodes.end(), _comp);
}

// Procedure: _pop
PfxtNode* PfxtCache::_pop() {
  if(_nodes.empty()) {
    return nullptr;
  }
  std::pop_heap(_nodes.begin(), _nodes.end(), _comp);
  _paths.push_back(std::move(_nodes.back()));
  _nodes.pop_back();
  return _paths.back().get();
}

// Function: _top
PfxtNode* PfxtCache::_top() const {
  return _nodes.empty() ? nullptr : _nodes.front().get();
}

// ------------------------------------------------------------------------------------------------

// Function: _pfxt_cacheS
// Construct a prefix tree from a given suffix tree.
PfxtCache Timer::_pfxt_cache(const SfxtCache& sfxt) const {

  PfxtCache pfxt(sfxt);

  assert(sfxt.slack());

  // Generate the path prefix from each startpoint. 
  for(const auto& [k, v] : sfxt._srcs) {
    if(!v) {
      continue;
    }
    else if(auto s = *sfxt.__dist[k] + *v; s < 0.0f) {
      pfxt._push(s, sfxt._S, k, nullptr, nullptr);
    }
  }

  return pfxt;
}

// Procedure: _spur
// Spur the path and expands the search space. The procedure iteratively scan the present
// critical path and performs spur operation along the path to generate other candidates.
void Timer::_spur(PfxtCache& pfxt, size_t K, PathHeap& heap) {

  for(size_t k=0; k<K; ++k) {
    // no more path to spur
    if(pfxt._nodes.empty()) {
      break;
    }
    // rank a new path
    else {

      auto node = pfxt._pop();
      
      // If the maximum among the minimum is smaller than the current minimum,
      // there is no need to do more.
      if(heap.num_paths() >= K && heap._top()->slack <= node->slack) {
        break;
      }
      
      // push the path to the heap and maintain the top-k
      auto path = std::make_unique<Path>(pfxt._sfxt._el, node->slack);

      heap._insert(std::move(path));
      heap._fit(K);

      // expand the search space
      _spur(pfxt, *node);
    }
  }
}

// Procedure: _spur
void Timer::_spur(PfxtCache& pfxt, const PfxtNode& pfx) {
  
  auto el = pfxt._sfxt._el;
  auto T  = pfxt._sfxt._T;
  auto u  = pfx.to;

  while(u != T) {

    assert(pfxt._sfxt.__link[u]);

    auto [upin, urf] = _decode_pin(u);

    for(auto arc : upin->_fanout) {
      
      // skip if the edge belongs to the suffix tree
      if(*pfxt._sfxt.__link[u] == arc->_idx) {
        continue;
      }

      FOR_EACH_RF_IF(vrf, arc->_delay[el][urf][vrf]) {

        auto v = _encode_pin(arc->_to, vrf);

        // skip if the edge goes outside the sfxt
        if(!pfxt._sfxt.__dist[v]) {
          continue;
        }

        auto w = (el == EARLY) ? *arc->_delay[el][urf][vrf] : -(*arc->_delay[el][urf][vrf]);
        auto s = *pfxt._sfxt.__dist[v] + w - *pfxt._sfxt.__dist[u] + pfx.slack;

        if(s < 0.0f) {
          pfxt._push(s, u, v, arc, &pfx);
        }
      }
    }
    u = *pfxt._sfxt.__tree[u];
  }
}


};  // end of namespace ot. -----------------------------------------------------------------------

