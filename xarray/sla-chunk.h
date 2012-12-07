#ifndef SLA_H__
#define SLA_H__

// SLA: skip-list array

struct sla_node;

/**
 * sla_fwrd: forward pointer for each level in an sla node
 * @node: next node for this level
 * @cnt:  number of items between the previous node and this for this level
 */
struct sla_fwrd {
	struct sla_node *node;
	unsigned int     cnt;
};
typedef struct sla_fwrd sla_fwrd_t;

/**
 * sla_node: node for skip list array
 * @chunk:       data buffer
 * @chunk_size:  size of data buffer
 * @forward:    forward pointers
 */
struct sla_node {
	void       *chunk;
	size_t     chunk_size;
	sla_fwrd_t forward[];
};
typedef struct sla_node sla_node_t;


/**
 *  SLA_NODE_NEXT: next node for a particular level
 *  SLA_NODE_CNT:  distance between this node and the previous one in items
 *
 * Note that the number of items included in each node is:
 * SLA_NODE_NITEMS(node, 0), which should be <= from ->chunk_size
 */
#define SLA_NODE_NEXT(n, lvl) (n)->forward[lvl].node
#define SLA_NODE_CNT(n,  lvl) (n)->forward[lvl].cnt
#define SLA_NODE_NITEMS(n)    SLA_NODE_CNT(n, 0)

static inline sla_node_t *
sla_node_next(sla_node_t *n, unsigned level)
{
	return SLA_NODE_NEXT(n,level);
}

/**
 * sla: skiplist array structure
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
struct sla {
	unsigned     max_level;
	unsigned     cur_level;
	size_t       total_size;
	float        p;
	sla_node_t   *tail;
	sla_node_t   *head;
	unsigned int seed;
};
typedef struct sla sla_t;

/**
 *  SLA_HEAD_NODE:  first node for a particular level
 *  SLA_TAIL_NODE:  last node for a particular level
 *  SLA_TAIL_CNT:   number of items between the last node of this level
 *                  and the end of the list
 *
 * Note that ->cnt in head is unused
 */
#define SLA_HEAD_NODE(sla, lvl) (sla)->head->forward[lvl].node
#define SLA_TAIL_NODE(sla, lvl) (sla)->tail->forward[lvl].node
#define SLA_TAIL_CNT(sla, lvl)  (sla)->tail->forward[lvl].cnt

/**
 * sla iterators:
 *   sla_for_each: iterate all nodes (level zero)
 *   sla_for_each_safe: allows deletion
 */

#define sla_for_each(sla, lvl, node) \
	for (node = SLA_HEAD_NODE(sla, lvl); \
	     node != (sla)->tail; \
	     node = sla_node_next(node, lvl))

#define sla_for_each_safe(sla, lvl, node, next) \
	for (node = SLA_HEAD_NODE(sla, lvl), next = sla_node_next(node,lvl); \
	     node != (sla)->tail; \
	     node = next, next = sla_node_next(next,lvl))

/**
 * size of a structure containing an sla_node based on its level
 */
#define sla_node_struct_size(s, level) (sizeof(s) + sizeof(sla_fwrd_t [level]))

/**
 * sla_node_size(): sized needed based on the node's leve
 */
static inline size_t
sla_node_size(unsigned level)
{
	return sla_node_struct_size(sla_node_t, level);
}

/**
 * return a random level for given sla
 */
unsigned sla_rand_level(sla_t *sla);

/**
 * sla_node_init: initialize an sla node
 *  @lvl:        level of the node -- normally returned from sla_rand_level()
 *  @chunk:      pointer to the buffer of the node
 *  @chunk_size: size of the buffer pointed by chunk
 *  @node_size:  size of valid bytes in the buffer
 */
void
sla_node_init(sla_node_t *node, unsigned lvl,
              void *chunk, size_t chunk_size,
              size_t node_size);
/**
 * allocate and initialize an sla node
 */
sla_node_t *
sla_node_alloc(sla_t *sla, void *chunk, size_t chunk_size, unsigned *lvl_ret);

sla_node_t *
do_sla_node_alloc(unsigned lvl, void *chunk, size_t chunk_size);

/**
 * sla_init: initialize an sla
 *  this will allocate memory (head and tail), call sla_destroy() to release it.
 */
void sla_init(sla_t *sla, unsigned int max_level, float p);

void sla_init_seed(sla_t *sla, unsigned int max_level, float p, int seed);

/**
 * sla_destroy: release memory allocated in sla_init() (and only that)
 *  all other objects should be released by the code that allocated them
 */
void sla_destroy(sla_t *sla);

void sla_verify(sla_t *sla);
void sla_print(sla_t  *sla);

/* sla_find(): find the node responsible for offset.
 *  - return the node that contains the key
 *  - the offset of the value in this node is placed on offset */
sla_node_t *
sla_find(sla_t *sla, size_t offset, size_t *chunk_off);

/**
 * sla_append_node(): append a node to the end of the sla
 * @sla to append the node
 * @node to append: should be initialized, but it can either be empty or contain data
 * @node_lvl: level of the node
 */
void sla_append_node(sla_t *sla, sla_node_t *node, int node_lvl);

/**
 * sla_pop_node_head(): decapitate sla
 * returns head node or NULL
 * @lvl_ret: if not NULL, it will contain the level of the node
 */
sla_node_t *sla_pop_head(sla_t *sla, unsigned *lvl_ret);

/**
 * sla_add_node_head(): add a node the the head of an sla
 */
void sla_add_node_head(sla_t *sla, sla_node_t *node, int node_lvl);

/* append buffer @buff of size @len to tail node */
size_t sla_append_tailnode(sla_t *sla, char *buff, size_t len);

/**
 * return a pointer that can fit @len bytes
 *   @len initially contains the size requested by the user
 *   @len finally contains teh size that the user is allowed to write
 * if @len == 0, function returns NULL
 */
char *
sla_append_tailnode__(sla_t *sla, size_t *len);

void sla_copyto(sla_t *sla, char *src, size_t len, size_t alloc_grain);
void sla_copyto_rand(sla_t *sla, char *src, size_t len,
                     size_t alloc_grain_min, size_t alloc_grain_max);

void
sla_split_coarse(sla_t *sla, sla_t *sla1, sla_t *sla2, size_t offset);

void sla_concat(sla_t *sla1, sla_t *sla2);


static inline int
sla_node_full(sla_node_t *n)
{
	return n->chunk_size == SLA_NODE_NITEMS(n);
}

static inline int
sla_tailnode_full(sla_t *sla)
{
	return sla_node_full(SLA_TAIL_NODE(sla, 0));
}

/**
 * pop data from the tail node
 *  @len initially contas the user-requested @len to be popped
 *  @len finally contains the size actual popped
 * returns a pointer to data popped
 */
char *sla_pop_tailnode(sla_t *sla, size_t *len);

void sla_print_chars(sla_t *sla);


void
sla_setptr(sla_t *sla, size_t idx, sla_fwrd_t ptr[]);
sla_node_t *
sla_ptr_find(sla_t *sla, sla_fwrd_t ptr[], size_t key, size_t *chunk_off);
void
sla_ptr_setptr(sla_t *sla, const sla_fwrd_t ptr_in[], size_t key,
               sla_fwrd_t ptr_out[]);
sla_node_t *
sla_ptr_nextchunk(sla_t *sla, sla_fwrd_t ptr[], size_t *node_key);

int
sla_ptr_equal(sla_fwrd_t ptr1[], sla_fwrd_t ptr2[], unsigned cur_level);

void
sla_ptr_print(sla_fwrd_t ptr[], unsigned cur_level);

#endif /* SLA_H__ */
