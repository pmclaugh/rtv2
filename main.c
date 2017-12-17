#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
# define quit_key 12
#endif

int main(int ac, char **av)
{
	srand(time(NULL));
	// Face *bunny = ply_import("objects/ply/bunny.ply");
	// int count;
	// bunny = object_flatten(bunny, &count);
	// int box_count;
	// tree_box *bun_bvh =  super_bvh(bunny, count, &box_count);
	

	Scene *sponza = scene_from_obj("objects/sponza/", "sponza.obj");
	int bin_count;
	tree_box *sponza_bvh = super_bvh(sponza->faces, sponza->face_count, &bin_count);
	sponza->bins = sponza_bvh;
	sponza->bin_count = bin_count;

	flatten_bvh(sponza->bins, bin_count);

	//printf("%lu\n", sizeof(gpu_bin));
}
