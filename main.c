#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
# define quit_key 12
#endif

int main(int ac, char **av)
{
	srand(time(NULL));

	tree_box *bunnies = calloc(12, sizeof(tree_box));
	for (int i = 0; i < 12; i++)
	{
		Face *bunny = ply_import("objects/ply/bunny.ply");
		int count;
		translate(bunny, (cl_float3){(float) i * 2, 0, 0});
		bunny = object_flatten(bunny, &count);
		int box_count;
		tree_box *bun_bvh =  build_sbvh(bunny, count, &box_count);
		bunnies[i] = *bun_bvh; //this is technically bad but w/e testing
	}
	int mbcount;
	meta_box *mb = meta_bvh(bunnies, 12, &mbcount);
}
