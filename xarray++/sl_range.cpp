#ifndef XARR_SL_RANGE_HPP__
#define XARR_SL_RANGE_HPP__

#include <iostream>
#include <cstddef> // size_t
#include <random>
#include <functional>

extern "C" {
	#include "misc.h"
};

#include "chunk.hpp"

namespace sl {

// SlrFwrd: forward pointer for each level in an slr node
// @node: next node for this level
// @cnt:  number of items between the previous node and this for this level
template <typename NodeT>
struct SLR_Fwrd {
	NodeT     *node;
	size_t     cnt;
};

// SLRNode: node for skip list range
// @forward:    forward pointers
// @lvl:        level of node (see Note below)
//
// Note: @lvl isn't necessary for the algorithms, but it simplifies them
// significantly. On the initial implementation, I wanted to avoid the extra
// space, so the current code does not need a ->lvl. However, the space waste is
// too small (actually it is none since malloc_usable_size(node) returns the
// same value whether ->lvl is included or not), and it might be a good idea to
// include it in the structure. Currently, we are using it only to index the
// free lists, and the #if is a reminder to change the slr algorithms to use it.
//
// C++ Note: apparently, defining the base class as a template with the derived
// class as argument is a common template pattern:
// http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
template <typename NodeT>
struct SLR_Node {
	#if defined(SLR_MM_FREELISTS)
	int             lvl;
	#endif

	// Flexible array members are not a part of C++. They seem to work with gcc
	// and clang, so I'm using them.
	typedef SLR_Fwrd<NodeT> fwrd_type;
	fwrd_type forward[];

	static size_t size(unsigned int lvls = 0) {
		return sizeof(NodeT) + lvls*sizeof(typename NodeT::fwrd_type);
	}

	size_t &fw_cnt(unsigned int lvl) {
		return forward[lvl].cnt;
	}

	NodeT * &fw_node(unsigned int lvl) {
		return forward[lvl].node;
	}

	size_t &nitems(void) {
		return fw_cnt(0);
	}
};

// SLR_PureNode: range skip list node w/o payload: mostly for testing
struct SLR_PureNode: SLR_Node<SLR_PureNode> {};

// SLA_Node: skip list array node
template <typename T>
struct SLA_Node: Chunk<T>, SLR_Node<SLA_Node<T>> {};

// Note that SLR_Node should always be at the last on the list of base classes,
// so that ->forward ends up on the end of the object
// (maybe there is a trick to check this in all instantiations?)
static_assert(offsetof(SLR_PureNode, forward) == sizeof(SLR_PureNode),
	          "forward pointer must be at the end");
// apparently, offsetof is not allowed for non-POD data, and gcc produces a
// warning.
#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(offsetof(SLA_Node<char>, forward) == sizeof(SLA_Node<char>),
	          "forward pointer must be at the end");
#if !defined(__clang__)
#pragma GCC diagnostic pop
#endif

/**
 * SLR: skiplist range datastructure
 * @max_level:  maximum possible level. This is just a trick to avoid doing
 *              reallocs when allocating memory (e.g., for the update path). It
 *              should be possible to remove it.
 * @cur_level:  current maximum level
 * @head:       dummy node that has pointers to the first node for each level
 * @tail:       dummy node that has pointers to the last node for each level
 *              So, despite it's name, ->forward for this particular node
 *              contains backwards pointers. It is used to perform fast
 *              concatenation.
 * @total_size: total size of array
 */
template <typename NodeT>
class SLR {
public:
	size_t       total_size;
	unsigned int max_level;
	unsigned int cur_level;
	float        p;

	NodeT        *head, *tail;

	NodeT * &tail_node(unsigned int i) { return tail->forward[i].node; }
	NodeT * &head_node(unsigned int i) { return head->forward[i].node; }
	size_t &tail_cnt(unsigned int i)   { return tail->forward[i].cnt;  }
	// note: head cnt is not used

	// random engine generator for randomized node levels
	std::default_random_engine rnd_eng;
	std::uniform_real_distribution<float> rnd_dist;

	/*
	 * simple allocator functions using malloc
	 */
	NodeT *alloc_node(unsigned int lvl) {
		NodeT *ret;
		size_t size = NodeT::size(lvl);
		ret = (NodeT *)xmalloc(size);
		return ret;
	}

	NodeT *alloc_node(void) {
		unsigned int lvl = get_rand_lvl();
		return alloc_node(lvl);

	}

	void init_node(NodeT *node, unsigned int lvl, size_t node_size) {
		for (unsigned int i=0; i<lvl; i++) {
			node->forward[i].cnt = node_size;
		}
	}

	NodeT *new_node(unsigned int lvl, size_t node_size) {
		NodeT *ret = alloc_node(lvl);
		init_node(ret, lvl, node_size);
		return ret;
	}

	void free_node(NodeT *node) {
		free(node);
	}

	SLR(unsigned int max_level_, float p_):
		max_level(max_level_),
		p(p_),
		rnd_dist(0,1)
	{
		head = new_node(max_level + 1, 0);
		tail = new_node(max_level + 1, 0);
		reset();
	};

	void reset(void) {
		cur_level  = 1;
		total_size = 0;

		for (unsigned int i=0; i<cur_level; i++) {
			head_node(i) = tail;
			tail_node(i) = head;
			tail_cnt(i)  = 0;
		}
	}

	float randf(void) {
		return rnd_eng(rnd_dist);
	}

	unsigned int get_rand_lvl(void) {
		unsigned int lvl;
		for (lvl=1; (lvl < max_level) && (p > randf()); lvl++)
			;
		return lvl;
	}
};

// SLA : Skip List Array
template <typename T>
using SLA = SLR<SLA_Node<T>>;

};

using namespace sl;

int main(int argc, const char *argv[])
{
	std::uniform_real_distribution<double> u(0,1);
	std::cout << SLR_PureNode::size() << std::endl;
	std::cout << SLA_Node<char>::size() << std::endl;
	std::cout << SLA_Node<char>::size(1) << std::endl;

	SLA<char> sla(10,.5);
	return 0;
}

#endif /* XARR_SL_RANGE_HPP__ */
