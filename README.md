# smh
Shuffle-based predicate matcher and all-round branch free swiss army chainsaw

The SMH predicate matcher is a short sequence that allows matching of multiple predicates with very high throughput and low latency. The types of predicates that it supports are surprisingly varied: the full version of smh can detect ranges, bytes, negated bytes and negated ranges and match several boolean combinations of these predicates at once.

It can be used in a simple fashion as an anchored pattern / prefix / suffix matcher.

The Hyperscan project has a similar function buried in its 'lookaround' code. This presentation of the ideas is, as usual, meant to be more self-contained and accessible than the Hyperscan version but anyone looking to get at this kind of functionality in a complete, robust and supported library is encouraged to use Hyperscan: https://github.com/intel/hyperscan
