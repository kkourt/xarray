#include <string.h>
#include <stddef.h>
#include <assert.h>
#include "sla-chunk.h"


int main(int argc, const char *argv[])
{
	char buff[1024];
	sla_t sla;
	size_t alloc_chunk = sizeof(buff)/8;
	size_t pop_size = alloc_chunk*2;
	char *ret;

	sla_init(&sla, 5, 0.5);
	memset(buff, 'a', sizeof(buff));
	sla_copyto(&sla, buff, sizeof(buff), alloc_chunk);
	sla_print(&sla);
	ret = sla_pop_tailnode(&sla, &pop_size);
	assert(ret);
	sla_print(&sla);
	return 0;
}
