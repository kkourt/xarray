#include "sl.hh"

using namespace sl;

int main(int argc, const char *argv[])
{
	std::uniform_real_distribution<double> u(0,1);
	std::cout << SLR_PureNode::size() << std::endl;
	std::cout << SLA_Node<char>::size() << std::endl;
	std::cout << SLA_Node<char>::size(1) << std::endl << std::endl;

	SLA<char> sla(10,.5);
	sla.verify();

	char buff[4096];
	for (unsigned i=0; i<sizeof(buff); i++) {
		buff[i] = 'a' + (i % ('z' - 'a' + 1));
	}

	sla_copyto_rand(&sla, buff, sizeof(buff), 10, 100);
	sla.verify();
	sla.print();

	for (size_t idx=0; idx < sizeof(buff); idx++) {
		size_t off;
		SLA_Node<char> *n = sla.find(idx, &off);
		char c1 = n->ch_ptr[off];
		char c2 = buff[idx];
		printf("[idx:%3lu] %c--%c\n", idx, c1, c2);
		if (c1 != c2) {
			fprintf(stderr, "FAIL!\n");
			abort();
		}
	}
	printf("SUCC!\n");

	return 0;
}
