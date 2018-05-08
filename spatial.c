#include "rt.h"
#include <limits.h>

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
	cl_float3 span;
}				Bin_set;

Bin new_bin(AABB *box, int axis, int index, int count, cl_float3 span)
{
	//represent a new split using the "bin/bag paradigm"
	Bin bin;
	bin.bin_min = box->min;
	bin.bin_max = box->max;
	bin.bag_min = INF;
	bin.bag_max = NEG_INF;
	bin.enter = 0;
	bin.exit = 0;

	//I hope this works the way I think it does.
	((float *)&bin.bin_min)[axis] = ((float *)&box->min)[axis] + ((float *)&span)[axis] * (float)(index) / (float)(count + 1);
	((float *)&bin.bin_max)[axis] = ((float *)&box->min)[axis] + ((float *)&span)[axis] * (float)(index + 1) / (float)(count + 1);

	return bin;
}

Bin_set *allocate_bins(AABB *box)
{
	Bin_set *set = calloc(1, sizeof(Bin_set));
	set->span = vec_sub(box->max, box->min);
	float total = set->span.x + set->span.y + set->span.z;

	set->counts[0] = (int)ceil((float)SPLIT_TEST_NUM * set->span.x / total);
	set->counts[1] = (int)((float)SPLIT_TEST_NUM * set->span.y / total);
	set->counts[2] = SPLIT_TEST_NUM - set->counts[0] - set->counts[1];

	//careful with counting, 3 splits yeilds 4 bins
	set->bins = calloc(SPLIT_TEST_NUM + 3, sizeof(Bin));
	for (int axis = 0; axis < 3; axis++)
		for (int j = 0; j <= set->counts[axis]; j++)
			set->bins[axis][j] = new_bin(box, axis, j, set->counts[axis], set->span);

	return set;
}

void flex(Bin *bin, AABB *member)
{
	bin->bag_min.x = fmin(bin->bag_min.x, member->min.x);
	bin->bag_min.y = fmin(bin->bag_min.y, member->min.y);
	bin->bag_min.z = fmin(bin->bag_min.z, member->min.z);

	bin->bag_max.x = fmax(bin->bag_max.x, member->max.x);
	bin->bag_max.y = fmax(bin->bag_max.y, member->max.y);
	bin->bag_max.z = fmax(bin->bag_max.z, member->max.z);
}

AABB *clip(AABB *member, Bin *bin, int axis)
{
	AABB *clipped = dupe_box(member);

	//first, clipped = union(member, bin). only need to modify relevant axis (we know it fit on the other two)
	((float *)&clipped->min)[axis] = fmax(((float *)&clipped->min)[axis], ((float *)&bin->bin_min)[axis]);
	((float *)&clipped->max)[axis] = fmin(((float *)&clipped->max)[axis], ((float *)&bin->bin_max)[axis]);

	//now we need to see if we can shrink it more
	float left = ((float *)&clipped->min)[axis];
	float right = ((float *)&clipped->max)[axis];
	//for each edge, solve for [axis] == left, [axis] == right, add to "point cloud"
	cl_float3 pt_cloud[6];
	bzero(&pt_cloud, sizeof(cl_float3) * 6);
	int pt_count = 0;
	for (int vert = 0; vert < 3; vert++)
		if (point_in_box(member->f->verts[vert], clipped))
			pt_cloud[pt_count++] = member->f->verts[vert];
	for (int edge = 0; edge < 3; edge++)
	{
		float target = left - ((float *)&member->f->verts[edge])[axis];
		float t = target / ((float *)&member->f->edges[edge])[axis];

		if (t == t && t > 0.0f && t < member->f->edge_t[axis]) //might need more checks
			pt_cloud[pt_count++] = vec_add(member->f->verts[edge], vec_scale(member->f->edges[edge], t));

		target = right - ((float *)&member->f->verts[edge])[axis];
		t = target / ((float *)&member->f->edges[edge])[axis];

		if (t == t && t > 0.0f && t < member->f->edge_t[axis])
			pt_cloud[pt_count++] = vec_add(member->f->verts[edge], vec_scale(member->f->edges[edge], t));
	}

	AABB *fit = box_from_points(pt_cloud, pt_count);

	//now clipped should become union(clipped, fit)
	((float *)&clipped->min)[axis] = fmax(((float *)&clipped->min)[axis], ((float *)&fit->min)[axis]);
	((float *)&clipped->max)[axis] = fmin(((float *)&clipped->max)[axis], ((float *)&fit->max)[axis]);

	free(fit);
	return clipped;
}

void add_face(AABB *member, Bin_set *set, int axis, AABB *parent)
{
	int leftmost = INT_MAX;
	int rightmost = INT_MIN;
	float ratio;

	ratio = (((float *)&member->min)[axis] -  ((float *)&parent->min)[axis]) / ((float *)&set->span)[axis];
	leftmost = (int)floor(ratio * ((float *)&set->counts)[axis]);
	
	ratio = (((float *)&member->max)[axis] -  ((float *)&parent->min)[axis]) / ((float *)&set->span)[axis];
	rightmost = (int)floor(ratio * ((float *)&set->counts)[axis]);

	if (rightmost == set->counts[axis])
		rightmost--;

	set->bins[axis][leftmost].enter++;
	set->bins[axis][rightmost].exit++;

	if (leftmost == rightmost)
		flex(&set->bins[axis][leftmost], member);
	else
		for (int bin = leftmost; bin <= rightmost; bin++)
		{
			AABB *clipped = clip(member, &set->bins[axis][bin], axis);
			flex(&set->bins[axis][bin], clipped);
			free(clipped);
		}
}

Split *fast_spatial_split(AABB *box)
{
	
	// allocate split_count splits along axes in proportion
	Bin_set *set = allocate_bins(box);


	for (int axis = 0; axis < 3; axis++)
		for (AABB *member = box->members; member; member = member->next)
			add_face(member, set, axis, box);
	
}