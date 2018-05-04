#include "rt.h"



typedef struct s_bin
{
	cl_float3 bin_min;
	cl_float3 bin_max;
	cl_float3 bag_min;
	cl_float3 bag_max;
	int enter;
	int exit;
}				Bin;

typedef struct s_bin_set
{
	Bin **bins;
	int counts[3];
}				Bin_set;

Bin *new_bin(AABB *box, enum axis a, int index, int count)
{
	//make a new split using the "bin/bag paradigm"
	cl_float3 span = vec_sub(box->max, box->min);
	Bin *bin = calloc(1, sizeof(Bin));

	if (a == X_AXIS)
	{
		split->left->max.x = split->left->min.x + span.x * ratio;
		split->right->min.x = split->left->max.x;
	}
	if (a == Y_AXIS)
	{
		split->left->max.y = split->left->min.y + span.y * ratio;
		split->right->min.y = split->left->max.y;
	}
	if (a == Z_AXIS)
	{
		split->left->max.z = split->left->min.z + span.z * ratio;
		split->right->min.z = split->left->max.z;
	}
	return split;
}

Bin_set *allocate_bins(AABB *box)
{
	Bin_set *set = calloc(1, sizeof(Bin_set));
	cl_float3 span = vec_sub(box->max, box->min);
	float total = span.x + span.y + span.z;

	set->counts[0] = (int)ceil((float)SPLIT_TEST_NUM * span.x / total);
	set->counts[1] = (int)((float)SPLIT_TEST_NUM * span.y / total);
	set->counts[2] = SPLIT_TEST_NUM - x_splits - y_splits;

	set->bins = calloc(SPLIT_TEST_NUM, sizeof(Bin));
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < set->counts[i]; j++)
			set->bins[i][j] = new_bin(box, X_AXIS + i, j, set->counts[i]);

	return splits;
}

Split *fast_spatial_split(AABB *box)
{
	
	// allocate split_count splits along axes in proportion
	Split **allocate_splits(box);
	// for each axis:
	for (enum axis ax = X_AXIS; ax <= Z_AXIS; ax++)
	{

	}
	// 	make n + 1 bins for n splits - bin/bag paradigm
	// 	for each face:
	// 		increment entry count for bin containing leftmost vertex
	// 		increment exit count for bin containing rightmost vertex
	// 		grow each "bag" by clip(face, bin)
	// 		seems faster/easier to reclip once plane is picked than to connect all fragments
	// 	for each bin:
	// 		ltr/rtl scan, compare SAH values like in object split
	// 		pick a split
	// pick best overall from 3 axes
	// return to partition function where if spatial wins we do fast re-clip
	// *ie lowest on that axis
	
}