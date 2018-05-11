#include "rt.h"
#include <limits.h>

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

	//careful with counting, n splits yeilds n+1 bins
	set->bins = calloc(3, sizeof(Bin *));
	for (int axis = 0; axis < 3; axis++)
	{
		set->bins[axis] = calloc(set->counts[axis] + 1, sizeof(Bin));
		for (int j = 0; j <= set->counts[axis]; j++)
			set->bins[axis][j] = new_bin(box, axis, j, set->counts[axis], set->span);
	}

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

	//first, clipped = intersection(member, bin). only need to modify relevant axis (we know it's <= on the other axes)
	((float *)&clipped->min)[axis] = fmax(((float *)&clipped->min)[axis], ((float *)&bin->bin_min)[axis]);
	((float *)&clipped->max)[axis] = fmin(((float *)&clipped->max)[axis], ((float *)&bin->bin_max)[axis]);

	//now we need to see if we can shrink it more
	float left = ((float *)&clipped->min)[axis];
	float right = ((float *)&clipped->max)[axis];
	
	//we will assemble a "point cloud" that describes the portion of the (whole) triangle in this range on the given axis
	cl_float3 pt_cloud[6];
	bzero(&pt_cloud, sizeof(cl_float3) * 6);
	int pt_count = 0;

	//check for vertices inside the range
	for (int vert = 0; vert < 3; vert++)
		if (((float *)&member->f->verts[vert])[axis] >= left && ((float *)&member->f->verts[vert])[axis] <= right)
			pt_cloud[pt_count++] = member->f->verts[vert];

	//for each edge, solve for t such that [axis] == left, [axis] == right, add to "point cloud"
	for (int edge = 0; edge < 3; edge++)
	{
		int old_pt_count = pt_count;
		float target = left - ((float *)&member->f->verts[edge])[axis];
		float t = target / ((float *)&member->f->edges[edge])[axis];

		if (t == t && t > 0.0f && t < member->f->edge_t[edge])
			pt_cloud[pt_count++] = vec_add(member->f->verts[edge], vec_scale(member->f->edges[edge], t));

		target = right - ((float *)&member->f->verts[edge])[axis];
		t = target / ((float *)&member->f->edges[edge])[axis];

		if (t == t && t > 0.0f && t < member->f->edge_t[edge])
			pt_cloud[pt_count++] = vec_add(member->f->verts[edge], vec_scale(member->f->edges[edge], t));
	}

	//error checking
	if (pt_count > 6)
	{
		printf("too many points!\n");
		getchar();
	}
	if (pt_count <= 2)
	{
		printf("not enough points!!\n");
		print_face(member->f);
		printf("bounded by\n");
		print_vec(member->min);
		print_vec(member->max);
		printf("had only %d hits in\n", pt_count);
		print_vec(clipped->min);
		print_vec(clipped->max);
		printf("bin was (axis %d)\n", axis);
		print_vec(bin->bin_min);
		print_vec(bin->bin_max);
		getchar();
	}

	AABB *fit = box_from_points(pt_cloud, pt_count);

	//now clipped should become intersection(clipped, fit)
	for (int axis = 0; axis < 3; axis++)
	{
		((float *)&clipped->min)[axis] = fmax(((float *)&clipped->min)[axis], ((float *)&fit->min)[axis]);
		((float *)&clipped->max)[axis] = fmin(((float *)&clipped->max)[axis], ((float *)&fit->max)[axis]);
	}

	free(fit);
	return clipped;
}

int point_in_range_L(cl_float3 point, cl_float3 min, cl_float3 max, int last, int axis)
{
	if (last)
	{
		if (min.x <= point.x && point.x <= max.x)
			if (min.y <= point.y  && point.y <= max.y)
				if (min.z <= point.z && point.z <= max.z)
					return 1;
		return 0;
	}
	else
	{
		if (min.x <= point.x && (point.x < max.x || (point.x == max.x && axis != 0)))
			if (min.y <= point.y  && (point.y < max.y || (point.y == max.y && axis != 1)))
				if (min.z <= point.z && (point.z < max.z || (point.z == max.z && axis != 2)))
					return 1;
		return 0;
	}
}

int point_in_range_R(cl_float3 point, cl_float3 min, cl_float3 max, int first, int axis)
{
	if (first)
	{
		if (min.x <= point.x && point.x <= max.x)
			if (min.y <= point.y  && point.y <= max.y)
				if (min.z <= point.z && point.z <= max.z)
					return 1;
		return 0;
	}
	else
	{
		if ((min.x < point.x || (point.x == min.x && axis != 0)) && point.x <= max.x)
			if ((min.y < point.y || (point.y == min.y && axis != 1)) && point.y <= max.y)
				if ((min.z < point.z || (point.z == min.z && axis != 2)) && point.z <= max.z)
					return 1;
		return 0;
	}
}

void add_face(AABB *member, Bin_set *set, int axis, AABB *parent)
{
	//default values, should always get overwritten
	int leftmost = 0;
	int rightmost = set->counts[axis];

	for (int i = 0; i <= set->counts[axis]; i++)
	{
		if (point_in_range_L(member->min, set->bins[axis][i].bin_min, set->bins[axis][i].bin_max, i == set->counts[axis] ? 1 : 0, axis))
			leftmost = i;
		if (point_in_range_R(member->max, set->bins[axis][i].bin_min, set->bins[axis][i].bin_max, i == 0 ? 1 : 0, axis))
			rightmost = i;
	}

	set->bins[axis][leftmost].enter++;
	set->bins[axis][rightmost].exit++;

	if (leftmost == rightmost)
		flex(&set->bins[axis][leftmost], member);
	else
	{
		for (int bin = leftmost; bin <= rightmost; bin++)
		{
			AABB *clipped = clip(member, &set->bins[axis][bin], axis);
			flex(&set->bins[axis][bin], clipped);
			free(clipped);
		}
	}
}

Bin sum_bin(Bin a, Bin b)
{
	Bin ret;
	bzero(&ret, sizeof(Bin));
	ret.bag_min.x = fmin(a.bag_min.x, b.bag_min.x);
	ret.bag_min.y = fmin(a.bag_min.y, b.bag_min.y);
	ret.bag_min.z = fmin(a.bag_min.z, b.bag_min.z);

	ret.bag_max.x = fmax(a.bag_max.x, b.bag_max.x);
	ret.bag_max.y = fmax(a.bag_max.y, b.bag_max.y);
	ret.bag_max.z = fmax(a.bag_max.z, b.bag_max.z);

	ret.enter = a.enter + b.enter;
	ret.exit = a.exit + b.exit;
	return ret;
}

float bin_SA(Bin bin)
{
	cl_float3 span = vec_sub(bin.bag_max, bin.bag_min);
	span = (cl_float3){fabs(span.x), fabs(span.y), fabs(span.z)};
	return 2.0f * span.x * span.y + 2.0f * span.y * span.z + 2.0f * span.x * span.z;
}

float bin_SAH(Bin a, Bin b, AABB *parent)
{
	return (bin_SA(a) * (float)a.enter + bin_SA(b) * (float)b.exit) / SA(parent);
}

AABB *box_from_range(cl_float3 min, cl_float3 max)
{
	AABB *box = empty_box();
	box->min = min;
	box->max = max;
	return box;
}

Split *find_best_split(Bin_set *set, AABB *parent)
{
	Bin **ltr = calloc(3, sizeof(Bin *));
	Bin **rtl = calloc(3, sizeof(Bin *));

	//sum everything up
	for (int axis = 0; axis < 3; axis++)
	{
		ltr[axis] = calloc(set->counts[axis] + 1, sizeof(Bin));
		rtl[axis] = calloc(set->counts[axis] + 1, sizeof(Bin));

		ltr[axis][0] = set->bins[axis][0];
		for (int i = 1; i <= set->counts[axis]; i++)
			ltr[axis][i] = sum_bin(ltr[axis][i - 1], set->bins[axis][i]);

		rtl[axis][set->counts[axis]] = set->bins[axis][set->counts[axis]];
		for (int i = set->counts[axis] - 1; i >= 0; i--)
			rtl[axis][i] = sum_bin(rtl[axis][i + 1], set->bins[axis][i]);
	}

	//find the best split
	int best_axis = -1;
	int best_index = -1;
	float best_sah = FLT_MAX;
	for (int axis = 0; axis < 3; axis++)
	{
		for (int i = 0; i < set->counts[axis]; i++)
		{
			float this_sah = bin_SAH(ltr[axis][i], rtl[axis][i + 1], parent);
			if (this_sah < best_sah)
			{
				best_axis = axis;
				best_index = i;
				best_sah = this_sah;
			}
		}
	}

	//turn that into a proper Split
	Split *winner = calloc(1, sizeof(Split));
	winner->left = box_from_range(set->bins[best_axis][0].bin_min, set->bins[best_axis][best_index].bin_max);
	winner->right = box_from_range(set->bins[best_axis][best_index + 1].bin_min, set->bins[best_axis][set->counts[best_axis]].bin_max);

	winner->left_flex = box_from_range(ltr[best_axis][best_index].bag_min, ltr[best_axis][best_index].bag_max);
	winner->right_flex = box_from_range(rtl[best_axis][best_index + 1].bag_min, rtl[best_axis][best_index + 1].bag_max);

	winner->left_count = ltr[best_axis][best_index].enter;
	winner->right_count = rtl[best_axis][best_index + 1].exit;
	winner->both_count = winner->left_count + winner->right_count - parent->member_count;

	winner->ax = X_AXIS + best_axis; //awkward, fix this

	//clean up
	for (int axis = 0; axis < 3; axis++)
	{
		free(ltr[axis]);
		free(rtl[axis]);
		free(set->bins[axis]);
	}
	free(ltr);
	free(rtl);
	free(set->bins);
	free(set);

	return winner;
}

void print_set(Bin_set *set)
{
	printf("======print_set=======\n");
	for (int axis = 0; axis < 3; axis++)
	{
		printf("axis %d\n", axis);
		for (int i = 0; i <= set->counts[axis]; i++)
		{
			printf("bin, bag %d\n", i);
			print_vec(set->bins[axis][i].bin_min);
			print_vec(set->bins[axis][i].bin_max);
			print_vec(set->bins[axis][i].bag_min);
			print_vec(set->bins[axis][i].bag_max);
			printf("enter %d exit %d\n", set->bins[axis][i].enter, set->bins[axis][i].exit);
		}
	}
}

Split *fast_spatial_split(AABB *box)
{
	// allocate split_count splits along axes in proportion
	Bin_set *set = allocate_bins(box);

	//add the faces to the bins
	for (int axis = 0; axis < 3; axis++)
		for (AABB *member = box->members; member; member = member->next)
			add_face(member, set, axis, box);

	//pick the best one
	Split *best = find_best_split(set, box);

	return best;
}