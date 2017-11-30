#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
# define quit_key 12
#endif

int main(int ac, char **av)
{
	srand(time(NULL));

	Face *bunny = ply_import("objects/ply/bunny.ply");
	int count;
	bunny = object_flatten(bunny, &count);
	int box_count;
	tree_box *bunny_BVH = build_sbvh(bunny, count, &box_count);
}