#include "rt.h"

#define VERBOSE 0

// typedef struct s_split
// {
// 	AABB *left;
// 	AABB *left_flex;
// 	AABB *right;
// 	AABB *right_flex;
// 	int left_count;
// 	int right_count;
// 	int both_count;

// 	enum axis ax;
// 	float ratio;
// }				Split;

AABB sum_box(AABB a, AABB b)
{
	AABB ret;
	bzero(&ret, sizeof(AABB));
	ret.min.x = fmin(a.min.x, b.min.x);
	ret.min.y = fmin(a.min.y, b.min.y);
	ret.min.z = fmin(a.min.z, b.min.z);

	ret.max.x = fmax(a.max.x, b.max.x);
	ret.max.y = fmax(a.max.y, b.max.y);
	ret.max.z = fmax(a.max.z, b.max.z);
	return ret;
}

#define Ct 10.0f
#define Ci 1.0f

float SA(AABB *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	span = (cl_float3){fabs(span.x), fabs(span.y), fabs(span.z)};
	return 2 * span.x * span.y + 2 * span.y * span.z + 2 * span.x * span.z;
}

float SAH(Split *split, AABB *parent)
{
	return Ct + Ci * (SA(split->left_flex) * split->left_count + SA(split->right_flex) * split->right_count) / SA(parent);
}

Split *scan_tallies(AABB *ltr, AABB *rtl, int count, AABB *parent, enum axis ax)
{
	//find best object split on this axis
	float best_SAH = FLT_MAX;
	int best_ind = -1;

	for (int i = 0; i < count - 1; i++)
	{
		float this_SAH = Ct + Ci * (SA(&ltr[i]) * (i + 1) + SA(&rtl[i + 1]) * (count - i)) / SA(parent);
		if (this_SAH < best_SAH)
		{
			best_ind = i;
			best_SAH = this_SAH;
		}
	}

	Split *best = calloc(1, sizeof(Split));
	best->ax = ax;
	best->left_flex = dupe_box(&ltr[best_ind]);
	best->left_count = best_ind + 1;
	best->right_flex = dupe_box(&rtl[best_ind + 1]);
	best->right_count = count - best_ind;
	best->ratio = best_SAH; //I am going to rename this variable but it's fine for now
	return best;
}

Split *fast_object_split(AABB *box)
{
	//put members into an array for easier sorting
	int count = box->member_count;
	AABB *members = calloc(count, sizeof(AABB));
	AABB *b = box->members;
	for (int i = 0; b; b = b->next, i++)
		members[i] = *b;

	AABB *tallies_ltr = calloc(count, sizeof(AABB));
	AABB *tallies_rtl = calloc(count, sizeof(AABB));
	Split *best_split = NULL;

	//X axis
	fast_axis_sort(members, count, X_AXIS);
	memcpy(tallies_ltr, members, count * sizeof(AABB));
	memcpy(tallies_rtl, members, count * sizeof(AABB));
	for (int i = 1; i < count; i++)
		tallies_ltr[i] = sum_box(tallies_ltr[i], tallies_ltr[i - 1]);
	for (int i = count - 2; i >= 0; i--)
		tallies_rtl[i] = sum_box(tallies_rtl[i], tallies_rtl[i + 1]);
	best_split = scan_tallies(tallies_ltr, tallies_rtl, count, box, X_AXIS);
	//Y axis
	fast_axis_sort(members, count, Y_AXIS);
	memcpy(tallies_ltr, members, count * sizeof(AABB));
	memcpy(tallies_rtl, members, count * sizeof(AABB));
	for (int i = 1; i < count; i ++)
		tallies_ltr[i] = sum_box(tallies_ltr[i], tallies_ltr[i - 1]);
	for (int i = count - 2; i >= 0; i--)
		tallies_rtl[i] = sum_box(tallies_rtl[i], tallies_rtl[i + 1]);
	Split *this_split = scan_tallies(tallies_ltr, tallies_rtl, count, box, Y_AXIS);
	if (this_split->ratio < best_split->ratio)
	{
		free(best_split);
		best_split = this_split;
	}
	else
		free(this_split);

	//Z axis
	fast_axis_sort(members, count, Z_AXIS);
	memcpy(tallies_ltr, members, count * sizeof(AABB));
	memcpy(tallies_rtl, members, count * sizeof(AABB));
	for (int i = 1; i < count; i ++)
		tallies_ltr[i] = sum_box(tallies_ltr[i], tallies_ltr[i - 1]);
	for (int i = count - 2; i >= 0; i--)
		tallies_rtl[i] = sum_box(tallies_rtl[i], tallies_rtl[i + 1]);
	this_split = scan_tallies(tallies_ltr, tallies_rtl, count, box, Z_AXIS);
	if (this_split->ratio < best_split->ratio)
	{
		free(best_split);
		best_split = this_split;
	}
	else
		free(this_split);

	free(tallies_ltr);
	free(tallies_rtl);
	return best_split;
}



void partition(AABB *box)
{
	Split *object = fast_object_split(box); //figure out the best way to split the box

	if (SAH(object, box) < Ci * box->member_count) //is that split worth it?
	{
		AABB **members = calloc(box->member_count, sizeof(AABB *));
		AABB *b = box->members;
		for (int i = 0; b; b = b->next, i++)
			members[i] = b;

		pointer_axis_sort(members, box->member_count, object->ax);

		box->left = dupe_box(object->left_flex);
		box->right = dupe_box(object->right_flex);
		for (int i = 0; i < box->member_count; i++)
		{
			if (i < object->left_count)
			{
				push(&box->left->members, members[i]);
				box->left->member_count++;
			}
			else
			{
				push(&box->right->members, members[i]);
				box->right->member_count++;
			}
		}
		free(members);
	}
	
	if (object)
		free_split(object);
}

void sbvh(t_env *env)
{
	//LL is best for this bvh. don't want to rearrange import for now, will do later
	Face *faces = NULL;
	for (int i = 0; i < env->scene->face_count; i++)
	{
		Face *f = calloc(1, sizeof(Face));
		memcpy(f, &env->scene->faces[i], sizeof(Face));
		f->next = faces;
		faces = f;
	}
	free(env->scene->faces);

	//put all faces in AABBs
	AABB *boxes = NULL;
	int fcount = 0;
	for (Face *f = faces; f; f = f->next)
	{
		push(&boxes, box_from_face(f));
		fcount++;
	}

	AABB *root_box = box_from_boxes(boxes);

	AABB *stack = root_box;
	int count = 1;
	int ref_count = 0;

	while (stack)
	{
		AABB *box = pop(&stack);
		partition(box);
		if (box->left)
		{
			box->left->parent = box;
			box->right->parent = box;
			count += 2;

			push(&stack, box->left);
			push(&stack, box->right);				
		}
		else
			ref_count += box->member_count;
	}
	printf("done. %d boxes", count);
	printf("%d member references vs %d starting\n", ref_count, root_box->member_count);

	env->scene->bins = root_box;
	env->scene->bin_count = count;
	env->scene->face_count = ref_count;
}































///////FLATTENING SECTION//////////

void flatten_faces(Scene *scene)
{
	//make array of Faces that's ref_count big
	Face *faces = calloc(scene->face_count, sizeof(Face));
	int face_ind = 0;
	//traverse tree, finding leaf nodes
	scene->bins->next = NULL;
	AABB *stack = scene->bins;
	AABB *box = NULL;
	//populate face array with faces in leaf nodes
	while (stack)
	{
		box = pop(&stack);
		if (box->left) // boxes always have 0 or 2 children never 1
		{
			push(&stack, box->left);
			push(&stack, box->right);
		}
		else
		{
			box->start_ind = face_ind;
			for (AABB *node = box->members; node; node = node->next)
				memcpy(&faces[face_ind++], node->f, sizeof(Face));
		}
	}
	scene->faces = faces;
}

gpu_bin bin_from_box(AABB *box)
{
	gpu_bin bin;

	bin.minx = box->min.x;
	bin.miny = box->min.y;
	bin.minz = box->min.z;

	bin.maxx = box->max.x;
	bin.maxy = box->max.y;
	bin.maxz = box->max.z;

	if (box->left)
	{
		bin.lind = box->left->flat_ind;
		bin.rind = box->right->flat_ind;
	}
	else
	{
		bin.lind = -3 * box->start_ind;
		bin.rind = -3 * box->member_count;
	}

	return bin;
}

gpu_bin *flatten_bvh(Scene *scene)
{
	gpu_bin *bins = calloc(scene->bin_count, sizeof(gpu_bin));
	int bin_ind = 0;
	int boost_ind = 0;

	//do a "dummy" traversal and fill in flat_ind for each AABB

	AABB *queue_head = scene->bins;
	AABB *queue_tail = scene->bins;

	while (queue_head)
	{
		queue_head->flat_ind = bin_ind++;

		if (queue_head->left)
		{
			queue_head->left->next = queue_head->right;
			queue_tail->next = queue_head->left;
			queue_tail = queue_head->right;
			queue_tail->next = NULL;
		}

		queue_head = queue_head->next;
	}

	//second pass to actually populate the gpu_bins
	queue_head = scene->bins;
	queue_tail = scene->bins;

	while (queue_head)
	{
		bins[queue_head->flat_ind] = bin_from_box(queue_head);
		if (queue_head->left)
		{
			queue_head->left->next = queue_head->right;
			queue_tail->next = queue_head->left;
			queue_tail = queue_head->right;
			queue_tail->next = NULL;
		}

		queue_head = queue_head->next;
	}

	return bins;
}