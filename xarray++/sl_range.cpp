#ifndef XARR_SL_RANGE_HPP__
#define XARR_SL_RANGE_HPP__

#include <iostream>
#include <cstddef>   // size_t
#include <cstring>   //memcpy
#include <random>
#include <utility>   // std::forward
#include <algorithm> // std::max
#include <iterator>

extern "C" {
	#include "misc.h"
};

template <typename T>
struct Chunk {
	T       *ch_ptr;
	size_t   ch_cnt; // number of Ts starting from @ch_ptr
};

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
class SLR_Node {
public:
	#if defined(SLR_MM_FREELISTS)
	int             lvls;
	#endif

	// Flexible array members are not a part of C++. They seem to work with gcc
	// and clang, so I'm using them.
	typedef SLR_Fwrd<NodeT> fwrd_type;
	fwrd_type forward[];

	/*
	SLR_Node(unsigned int lvls, size_t node_cnt) {
		init(lvls, node_cnt);
	}
	*/

	static size_t size(unsigned int lvls = 0) {
		return sizeof(NodeT) + lvls*sizeof(typename NodeT::fwrd_type);
	}

	void init(unsigned int lvls, size_t node_cnt) {
		for (unsigned int i=0; i<lvls; i++) {
			forward[i].cnt = node_cnt;
		}
	}

	// Accessors
	size_t &cnt(unsigned int lvl) {
		return forward[lvl].cnt;
	}

	NodeT * &next_node(unsigned int lvl) {
		return forward[lvl].node;
	}

	size_t &ncnt(void) {
		return cnt(0);
	}
};

// SLR_PureNode: range skip list node w/o payload: mostly for testing
class SLR_PureNode: public SLR_Node<SLR_PureNode> {};

// SLA_Node: skip list array node
template <typename T>
class SLA_Node: public Chunk<T>, public SLR_Node<SLA_Node<T>> {
public:
	void init(unsigned int lvls, size_t cnt_, T *chunk_=NULL, size_t chunk_size_=0)
	{
		SLR_Node<SLA_Node<T>>::init(lvls, cnt_);
		Chunk<T>::ch_ptr = chunk_;
		Chunk<T>::ch_cnt = chunk_size_;
	}
};

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

template <typename NodeT>
class SLR_NodeAlloc_Malloc {
public:

	static NodeT *alloc_node(unsigned int lvls) {
		NodeT *ret;
		size_t size = NodeT::size(lvls);
		ret = (NodeT *)xmalloc(size);
		return ret;
	}

	static void free_node(NodeT *node) {
		free(node);
	}

	template <typename ...Args>
	static void init_node(NodeT *node,
	                       unsigned int lvls, size_t node_cnt,
	                       Args&&... params) {
		node->init(lvls, node_cnt, std::forward<Args>(params)...);
	}

	template <typename ...Args>
	static NodeT *new_node(unsigned int lvls, size_t node_cnt, Args&&...args) {
		NodeT *ret = alloc_node(lvls);
		init_node(ret, lvls, node_cnt, std::forward<Args>(args)...);
		return ret;
	}
};

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
 * @total_cnt: total size of array
 */
template <typename NodeT,
          template <typename, typename ...> class NodeAllocator = SLR_NodeAlloc_Malloc>
class SLR : NodeAllocator<NodeT> {
public:
	size_t       total_cnt;
	unsigned int max_level;
	unsigned int cur_level;
	float        p;

	NodeT        *head, *tail;

	typedef NodeT node_type;
	typedef typename NodeT::fwrd_type fwrd_type;

	NodeT * &tail_node(unsigned int i) { return tail->forward[i].node; }
	NodeT * &head_node(unsigned int i) { return head->forward[i].node; }
	size_t &tail_cnt(unsigned int i)   { return tail->forward[i].cnt;  }
	// note: head cnt is not used

	// random engine generator for randomized node levels
	std::default_random_engine rnd_eng;
	std::uniform_real_distribution<float> rnd_dist;

	using NodeAllocator<NodeT>::new_node;
	using NodeAllocator<NodeT>::free_node;

	void reset(void) {
		cur_level  = 1;
		total_cnt = 0;

		for (unsigned int i=0; i<cur_level; i++) {
			head_node(i) = tail;
			tail_node(i) = head;
			tail_cnt(i)  = 0;
		}
	}

	SLR(unsigned int max_level_, float p_):
		NodeAllocator<NodeT>(),
		max_level(max_level_),
		p(p_),
		rnd_dist(0,1)
	{
		head = new_node(max_level + 1, 0);
		tail = new_node(max_level + 1, 0);
		reset();
	};

	~SLR(void) {
		free_node(head);
		free_node(tail);
	}

	unsigned int get_rand_lvl(void) {
		unsigned int lvl;
		for (lvl=1; (lvl < max_level) && (p > rnd_dist(rnd_eng)); lvl++)
			;
		return lvl;
	}

	void verify() {
		#if defined(NDEBUG)
		return;
		#endif

		for (unsigned int lvl = 0; lvl < cur_level; lvl++) {
			size_t cnt = 0;

			for (iterator i = begin(lvl); i != end();  ++i) {
				cnt += i->cnt(lvl);
				if (cnt > total_cnt) {
					std::cerr << "cnt (" << cnt << ")" << " is larger than total_cnt (" << total_cnt << ")" << std::endl;
					abort();
				}
			}
			cnt += tail->cnt(lvl);
			if (cnt != total_cnt) {
				std::cerr << "cnt (" << cnt << ")" << " differs from total_cnt (" << total_cnt << ")" << std::endl;
				abort();
			}
		}
	}

	void print() {
		printf("sla: %p total_size: %lu cur_level: %u\n", this, total_cnt, cur_level);

		for (int lvl = cur_level - 1; lvl >= 0; lvl--) {
			unsigned long cnt __attribute__((unused));
			cnt = 0;

			printf("L%-4d    ", lvl);
			printf("|---------");

			NodeT *x = head;
			while ((x = x->next_node(0)) != head->next_node(lvl)) {
				printf("----------------");
				//assert(cnt++ < total_cnt);
			}
			printf(">");

			for (iterator it = begin(lvl); it != end(); ++it) {
				printf("(%zu     )----", it->cnt(lvl));
				NodeT *z = it.node();
				while ((z = z->next_node(0)) != it->next_node(lvl)) {
					printf("----------------");
					//assert(cnt++ < total_cnt);
				}
				printf(">");
			}
			printf("| %zu\n", tail->cnt(lvl));
		}

		#if 0
		sla_node_t *n;
		printf("    (%9p)", sla->head);
		sla_for_each(sla, 0, n) {
			printf("     (%9p)", n);
		}
		printf("     (%9p)", sla->tail);
		printf("\n");
		#endif
	}

	// SLR iterator
	class iterator : public std::iterator<std::forward_iterator_tag, NodeT> {
	private:
		NodeT *n__;
		unsigned int lvl;
	public:
		iterator(NodeT *node_, unsigned int lvl_ = 0): n__(node_), lvl(lvl_) { }

		bool operator==(const iterator &i) { return (n__ == i.n__); }
		bool operator!=(const iterator &i) { return (n__ != i.n__); }
		void operator++() { n__ = n__->next_node(lvl); }
		NodeT& operator*() { return *n__; };
		NodeT *operator->() { return n__; };
		NodeT *node() { return n__; };
	};

	iterator begin(unsigned int lvl_ = 0) {
		return iterator(head->next_node(lvl_), lvl_);
	}
	iterator end() {
		return iterator(tail, 0);
	};

	// set the update path for a specific key
	void traverse(size_t key, typename NodeT::fwrd_type update[]) {
		int lvl      = cur_level - 1;
		NodeT *n     = head;
		size_t idx   = 0;

		assert(key < total_cnt);
		do {
			for (;;) {
				NodeT *next = n->next_node(lvl);
				size_t next_idx = idx + next->cnt(lvl);

				/*  we need to go down a level */
				if (key < next_idx)
					break;

				/* tail adds up to the number of nodes. If we've reached
				 * tail then it should be that next_idx > key  */
				assert(next != tail);

				n = next;
				idx = next_idx;
			}
			update[lvl].node = n;
			update[lvl].cnt  = idx;
		} while(--lvl >= 0);
	}

	// add a  node at the front
	void add_node_head(NodeT *node, int node_lvl) {

		if (cur_level < node_lvl) {
			for (unsigned lvl = cur_level; lvl < node_lvl; lvl++) {
				head_node(lvl) = node;
				node->next_node(lvl) = tail;
				tail_node(lvl) = node;
				tail_cnt(lvl)  = total_cnt;
			}
			cur_level = node_lvl;
		}

		unsigned lvl;
		for (lvl=0; lvl<node_lvl; lvl++) {
			assert(head_node(lvl) != tail);
			node->next_node(lvl) = head_node(lvl);
			head_node(lvl) = node;
		};

		for (   ; lvl< cur_level; lvl++) {
			NodeT *h = head_node(lvl);
			assert(h != tail); // not an empty level
			h->node_cnt(lvl) += node->ncnt();
		}

		total_cnt += node->ncnt();
	}

	// decapitate slr
	NodeT *pop_head(unsigned int *lvl_ret) {

		NodeT *ret = head_node(0);
		if (ret == tail) // slr empty
			return NULL;

		// find node level first
		// (we could do it on the loops below, but it gets complicated)
		if (lvl_ret) {
			unsigned lvl = 0;
			while (head_node(++lvl) == ret)
				;
			*lvl_ret = lvl;
		}

		if (ret->next_node(0) == tail) { // ret is the only node in the slr
			assert(ret->ncnt(ret) == total_cnt);
			reset();
			return ret;
		}

		assert(ret->ncnt() < total_cnt);
		unsigned lvl;
		for (lvl = 0; lvl < cur_level; lvl++) { // iterate node levels
			if (head_node(lvl) != ret)
				break;
			NodeT *nxt = ret->next_node(lvl);
			head_node(lvl) = nxt;
			if (nxt == tail) {
				assert(lvl != 0);
				// level is empty, update current level
				cur_level = lvl;
				break;
			}
		}

		for ( ;lvl<cur_level; lvl++) { // itereate remaining levels
			size_t ncnt = ret->cnt(lvl);
			assert(head_node(lvl)->cnt(lvl) >= ncnt);
			head_node(lvl)->node_cnt(lvl) -= ncnt;
		}

		total_cnt -= ret->ncnt();
		return ret;
	}

	// append a node to the end
	void append_node(NodeT *node, int node_lvl) {
		// ugly trick that lazily sets up un-initialized levels
		if (cur_level < node_lvl) {
			for (unsigned lvl = cur_level; lvl < node_lvl; lvl++) {
				head_node(lvl) = node;
				node->next_node(lvl) = tail;
				tail_node(lvl) = node;
				node->cnt(lvl) += total_cnt;
				tail_cnt(lvl) = 0;
			}
			unsigned int new_level = node_lvl;
			node_lvl = cur_level; // set levels for next iteration
			cur_level = new_level;
		}

		unsigned lvl;
		for (lvl=0; lvl<node_lvl; lvl++) {
			/* update links */
			NodeT *last_i = tail_node(lvl);
			assert(last_i->next_node(lvl) == tail);
			last_i->next_node(lvl) = node;
			node->next_node(lvl) = tail;
			tail_node(lvl) = node;

			/* for each level of the node we increase its count by the count of
			 * the tail -- i.e., the number of items between this node and the
			 * previous one */
			node->cnt(lvl) += tail_cnt(lvl);
			/* there is nothing between the tail and the last nodef or this level:
			 * tail's count is zero */
			tail_cnt(lvl) = 0;
		}

		/* for the remaining levels -- the tail count should be increased by the
		 * node's count */
		for ( ; lvl<cur_level; lvl++) {
			tail_cnt(lvl) += node->ncnt();
		}

		total_cnt += node->ncnt();
	}

	/**
	 * pop the tail node
	 *  This is ugly and has performance issues. Hopefully it won't be called
	 *  often
	 *  Alternative options:
	 *   - don't deallocate nodes => not easy: how would you do append?
	 *   - double link list
	 */
	NodeT *
	sla_pop_tail()
	{
		verify();
		NodeT *n = tail_node(0);
		NodeT *h = head_node(0);
		size_t nlen = n->ncnt();

		if (h == n) {
			//only one node in the list
			reset();
			return n;
		}

		/**
		 * ugly part: for all @n's levels -- i.e., i levels such for which:
		 *  SLA_TAIL_NODE(sla, i) == SLA_TAIL_NODE(sla, 0)
		 * We need to find the node before n, which will become the new tail
		 *
		 * To do that, we start from @r, which is the first tail node we can
		 * find (as we go up the levels) that is not @n. We then, do a linear
		 * search for @n starting from @r for all @n's levels. If @n has
		 * @cur_level leafs, we start with the head node.
		 */

		/* find @r and @n_levels -- i.e n's number of levels */
		NodeT *r = NULL;
		unsigned int n_levels=1;
		for (unsigned lvl=1; lvl<cur_level; lvl++) {
			NodeT *x = tail_node(lvl);
			if (x != n) {
				r = x;
				break;
			}
			n_levels++;
		}

		if (r == NULL) {
			unsigned int lvl;
			for (lvl=n_levels-1; lvl<n_levels; lvl--) { // backwards
				r = head_node(lvl);
				if (r != n)
					break;
			}

			if (n_levels == cur_level) {
				cur_level = lvl + 1;
				n_levels = cur_level;
			}
		}
		assert(r != n && r != NULL);

		/**
		 * go over all @n's levels and fix pointers and counts.
		 * Update @r as we find nodes that are closer to @n.
		 */
		for (unsigned lvl = n_levels - 1; lvl<n_levels; lvl--) { // backwards
			// find @n's previous node for this level.
			NodeT *x;
			for (x = r; x != n; x = x->next_node(lvl)) {
				assert(x != tail);
				assert(x != NULL);
				r = x;
			}

			assert(r != n);
			assert(r->next_node(lvl) == n);
			assert(tail_node(lvl) == n);
			assert(n->next_node(lvl) == tail);
			assert(tail_cnt(lvl) == 0);
			assert(nlen <= n->cnt(lvl));

			// fix tail count: there might be other nodes with smaller
			// levels than the one we currently are. We need to update tail
			// count to the total count of these nodes. We find this value
			// by substracting @n's elelemts for its count on this level
			tail_cnt(lvl) = n->node_cnt(lvl) - nlen;
			// fix pointers: we need to fix tail pointers and node pointers
			tail_node(lvl) = r;
			r->next_node(lvl) = tail;
		}

		/**
		 * for the remaining levels, we only need to fix tail count
		 */
		for (unsigned lvl=n_levels; lvl<cur_level; lvl++) {
			assert(tail_node(lvl) != n);
			assert(tail_cnt(lvl) >= nlen);
			tail_cnt(lvl) -= nlen;
		}

		total_cnt -= nlen;
		verify();
		return n;
	}
};

/**
 * sla_split: split a skip-list array on node level
 *  @sla:    initial sla -- if sla1 is NULL, on return contains first half
 *  @sla1:   uninitialized sla -- if not NULL, on return contains first half
 *  @sla2:   uninitialized sla -- on return contains the second half
 *  @offset: offset to do the splitting
 *  item at @offset will end up on the first node of slr2
 */
template <typename SLR>
void slr_split(SLR *slr, SLR *slr1, SLR *slr2, size_t offset)
{
	assert(offset > 0);
	assert(offset < slr->total_cnt);

	typename SLR::fwrd_type update[slr->max_level];
	slr->traverse(offset, update);

	// initialize slr2
	new (slr2) SLR(slr->max_level, slr->p);

	/* current levels will (possibly) need re-adjustment */
	unsigned int slr1_cur_level=1, slr2_cur_level=1;
	assert(slr->cur_level > 0);

	/* to compute the current levels, we start from the top levels*/
	for (int lvl=slr->cur_level - 1; lvl >= 0; lvl--) {
		/* slr1_n: the last node for slr1 for this level
		 * slr2_n: the first node for slr2 for this level
		 * dcnt:   cnt between slr1 and split point for this level */
		typename SLR::node_type *slr1_n = update[lvl].node;
		typename SLR::node_type *slr2_n = slr1_n->next_node(lvl);
		size_t dcnt = update[0].cnt - update[lvl].cnt;

		// update slr2 head node
		slr2->head_node(lvl) = slr2_n;
		assert(slr2_n->cnt(lvl) > dcnt);
		slr2_n->cnt(lvl) -= dcnt;

		// The slr2 tail node for this level is the tail node of slr unless this
		// node belongs to slr1, which means that this level is empty for slr2
		typename SLR::node_type *ti = slr->tail_node(lvl);
		if (ti != slr1_n) {
			ti->next_node(lvl) = slr2->tail;
			slr2->tail_node(lvl) = ti;
			slr2->tail_cnt(lvl)  = slr->tail_cnt(lvl);
			// check if this is the first non-empty level from the top
			if (slr2_cur_level == 1)
				slr2_cur_level = lvl + 1;
		}

		// update slr1 tail and slr1 current level
		slr1_n->next_node(lvl) = slr->tail;
		slr->tail_node(lvl) = slr1_n;
		slr->tail_cnt(lvl) = dcnt;
		if (slr1_cur_level == 1 && slr->head_node(lvl) != slr->tail) {
			slr1_cur_level = lvl + 1;
		}
	}

	if (slr1 == NULL)
		slr1 = slr;
	else
		new (slr1) SLR(slr->max_level, slr->p);

	assert((slr1_cur_level <= slr1->cur_level) && slr1_cur_level);
	assert(slr2_cur_level);
	slr1->cur_level = slr1_cur_level;
	slr2->cur_level = slr2_cur_level;

	slr2->total_cnt = slr1->total_cnt - update[0].cnt;
	slr1->total_cnt = update[0].cnt;

	return;
}

/*  sla1 will contain the concatenated sla
 *  sla2 will be empty
 */
template <typename SLR>
void slr_concat(SLR *slr1, SLR *slr2)
{
	assert(slr1->max_level == slr2->max_level &&
	       "FIXME: realloc head and tail pointers");

	unsigned int new_level = std::max(slr1->cur_level, slr2->cur_level);

	for (unsigned int lvl = 0; lvl < new_level; lvl++) {
		if (lvl >= slr1->cur_level ) {       /* invalid level for slr1 */
			typename SLR::node_type *slr2_fst = slr2->head_node(lvl);
			slr1->head_node(lvl) = slr2_fst;
			slr2_fst->cnt(lvl) += slr1->total_cnt;
		} else if (lvl >= slr2->cur_level) { /* invalid level for slr2 */
			typename SLR::node_type *slr1_lst = slr1->tail_node(lvl);
			slr1_lst->next_node(lvl) = slr2->tail;
			slr2->tail_node(lvl) = slr1_lst;
			slr2->tail_cnt(lvl) = slr2->total_cnt + slr1->tail_cnt(lvl);
		} else {                            /* valid level for both */
			typename SLR::node_type *slr1_lst = slr1->tail_node(lvl);
			typename SLR::node_type *slr2_fst = slr2->head_node(lvl);
			slr1_lst->next_node(lvl) = slr2_fst;
			slr2_fst->node_cnt(lvl) += slr1->tail_cnt(lvl);
		}
	}

	slr1->cur_level = new_level;
	SLR::node_type *slr1 = slr1->tail;
	slr1->tail = slr2->tail;
	slr1->total_cnt += slr2->total_cnt;

	/* leave slr2 in a consistent state */
	slr2->tail = slr1;
	slr2->reset();
}

template <typename T>
using SLA_Base = SLR<SLA_Node<T>>;

// SLA : Skip List Array
template <typename T>
class SLA : public SLA_Base<T> {
public:

	using NodeT = SLA_Node<T>;
	using SLR_T = SLR<NodeT>;
	using SLR_T::total_cnt;
	using SLR_T::head;
	using SLR_T::tail;
	using SLR_T::tail_node;
	using SLR_T::tail_cnt;
	using SLR_T::cur_level;

	typedef T elemT;

	static T *chunk_alloc(size_t size) {
		return (T *)xmalloc(sizeof(T)*size);
	}

	static void chunk_free(T *chunk) {
		free(chunk);
	}


	SLA(unsigned int max_level_, float p_): SLR_T(max_level_, p_) { };

	NodeT *
	sla_do_find(NodeT *node, unsigned int lvl, size_t idx, size_t key,
	            size_t *chunk_off) {
		size_t next_e, next_s;
		for(;;) { /* iterate all levels */
			for (;;) {
				/* next node covers the range: [next_s, next_e) */
				NodeT *next = node->next_node(lvl);
				next_e = idx + next->cnt(lvl);
				next_s = next_e - next->ncnt();

				/* key is before next node, go down a level */
				if (key < next_s)
					break;

				/* sanity check */
				if (next->ch_ptr == NULL) {
					fprintf(stderr, "something's wrong: next:%p lvl:%d key:%zu\n", next, lvl, key);
					abort();
				}

				node = next;

				/* key is in [next_s, next_e), return node */
				if (key < next_e)
					goto end;

				/* key is after next node, continue iteration */
				idx = next_e;
			}

			if (lvl-- == 0)
				break;
		}
		assert(0 && "We didn't found a node -- something's wrong");
	end:
		if (chunk_off)
			*chunk_off = key - next_s;
		return node;
	}

	/* find the node responsible for key.
	 *  - return the node that contains the key
	 *  - the offset of the value in this node is placed on offset */
	NodeT *
	find(size_t key, size_t *chunk_off)
	{
		if (key >= total_cnt)
			return NULL;

		return do_find(head, cur_level -1, 0, key, chunk_off);
	}

	/**
	 * return a pointer that can fit @len bytes
	 *   @len initially contains the size requested by the user
	 *   @len finally contains teh size that the user is allowed to write
	 * if @len == 0, function returns NULL
	 */
	T *
	append_tailnode__(size_t &len)
	{
		NodeT *n = tail_node(0);
		size_t nlen = n->ncnt();
		if (n == head || nlen == n->ch_cnt) {
			len = 0;
			return NULL;
		}

		assert(nlen < n->ch_cnt);
		size_t clen = std::min(n->ch_cnt - nlen, len);

		for (unsigned lvl=0; lvl < cur_level; lvl++) {
			if (tail_node(lvl) == n)
				n->cnt(lvl) += clen;
			else
				tail_cnt(lvl) += clen;
		}

		total_cnt += clen;
		len = clen;
		return n->ch_ptr + nlen;
	}

	NodeT *new_node(T *buff, size_t blen, unsigned int &lvls) {
		lvls = SLR_T::get_rand_lvl();
		NodeT *node = SLR_T::new_node(lvls, 0, buff, blen);
		return node;
	}

	/* append only to the tail node. Return the amount of data copied */
	size_t
	append_tailnode(T *buff, size_t len)
	{
		size_t clen = len;
		T *dst = append_tailnode__(clen);

		if (dst != NULL) {
			assert(clen > 0);
			memcpy(dst, buff, clen*sizeof(T));
		} else
			assert(clen == 0);

		return clen;
	}

	static bool node_full(NodeT *n) {
		return n->ch_cnt == n->ncnt();
	}

	bool tailnode_full() {
		return node_full(tail_node(0));
	}
};

template<typename SLA>
void sla_concat(SLA *sla1, SLA *sla2)
{
	// first we try to copy items if we can.
	// We do it only if sla2 has a single node for now, because there seems
	// to be a bug when we copy and sla2 has additional nodes
	typename SLA::node_type *sla1_tail = sla1->tail_node(0);
	typename SLA::node_type *sla2_head = sla2->head_node(0);
	size_t sla1_tcnt = sla1_tail->ncnt();
	size_t sla2_hcnt = sla2_head->ncnt();
	if (sla2->total_size == sla2_hcnt
	    && sla1_tcnt + sla2_hcnt <= sla1_tail->chunk_size) {
		sla1->verify();
		sla2->verify();
		size_t cpsize = sla2_hcnt;
		typename SLA::node_type *n = sla2->pop_head(NULL);
		assert(n == sla2_head);
		assert(n != NULL);
		assert(n->ch_ptr != NULL);
		typename SLA::elemT *dst = append_tailnode__(sla1, &cpsize);
		assert(cpsize == sla2_hcnt);
		memcpy(dst, n->chunk, cpsize*sizeof(SLA::elemT));
		SLA::chunk_free(n->ch_ptr);
		SLA::free_node(n);
		assert(sla1_tail->ncnt() == sla1_tcnt + sla2_hcnt);
		sla1->verify();
		sla2->verify();
		assert(sla2->total_size == 0);
		return;
	}

	return slr_concat(sla1, sla2);
}

template<typename SLA>
void sla_copyto(SLA *sla, typename SLA::elemT *src, size_t len,
                size_t alloc_grain)
{
	size_t clen;

	clen = sla->append_tailnode(src, len);
	src += clen;
	len -= clen;

	while (len > 0) {
		assert(sla_tailnode_full(sla));
		typename SLA::elemT *buff = SLA::chunk_alloc(alloc_grain);
		unsigned lvl;
		typename SLA::NodeT *node = sla->new_node(buff, alloc_grain, &lvl);
		sla->append_node(node, lvl);
		clen = sla->append_tailnode(src, len);
		assert(clen > 0);
		src += clen;
		len -= clen;
	}
}

// this is for testing: uses random-sized chunks
template<typename SLA>
void sla_copyto_rand(SLA *sla, typename SLA::elemT *src, size_t len,
                size_t alloc_grain_min, size_t alloc_grain_max)
{
	size_t clen;

	std::default_random_engine g;
	std::uniform_int_distribution<size_t> d(alloc_grain_min, alloc_grain_max);
	auto rand_grain = std::bind(d, g);

	clen = sla->append_tailnode(src, len);
	src += clen;
	len -= clen;

	while (len > 0) {
		assert(sla->tailnode_full());
		size_t alloc_grain = rand_grain();
		typename SLA::elemT *buff = SLA::chunk_alloc(alloc_grain);
		unsigned lvl;
		typename SLA::NodeT *node = sla->new_node(buff, alloc_grain, lvl);
		sla->append_node(node, lvl);
		clen = sla->append_tailnode(src, len);
		assert(clen > 0);
		src += clen;
		len -= clen;
	}
}


}; // end namespace sl

using namespace sl;

int main(int argc, const char *argv[])
{
	std::uniform_real_distribution<double> u(0,1);
	std::cout << SLR_PureNode::size() << std::endl;
	std::cout << SLA_Node<char>::size() << std::endl;
	std::cout << SLA_Node<char>::size(1) << std::endl << std::endl;

	SLA<char> sla(10,.5);
	sla.verify();
	sla.print();

	char buff[1024];
	for (unsigned i=0; i<sizeof(buff); i++) {
		buff[i] = 'a' + (i % ('z' - 'a' + 1));
	}

	sla_copyto_rand(&sla, buff, sizeof(buff), 10, 100);
	sla.print();
	//sla_print_chars(&sla);
	//sla_print(&sla); printf("\n");

	#if 0
	for (size_t idx=0; idx < sizeof(buff); idx++) {
		size_t off;
		sla_node_t *n = sla_find(&sla, idx, &off);
		char c1 = ((char *)n->chunk)[off];
		char c2 = buff[idx];
		printf("[idx:%3lu] %c--%c\n", idx, c1, c2);
		if (c1 != c2) {
			fprintf(stderr, "FAIL!\n");
			abort();
		}
	}
	#endif

	return 0;
}

#endif /* XARR_SL_RANGE_HPP__ */
