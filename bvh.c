#include "rt.h"

#define SPATIAL_ENABLE 0
#define VERBOSE 0

Split **new_allocate_splits(AABB *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	float total = span.x + span.y + span.z;
	int x_splits = (int)ceil((float)SPLIT_TEST_NUM * span.x / total);
	int y_splits = (int)((float)SPLIT_TEST_NUM * span.y / total);
	int z_splits = SPLIT_TEST_NUM - x_splits - y_splits;
	// printf("%d %d %d\n", x_splits, y_splits, z_splits);

	Split **splits = calloc(SPLIT_TEST_NUM, sizeof(Split));
	int s = 0;
	for (int i = 0; i < x_splits; i++, s++)
		splits[s] = new_split(box, X_AXIS, (float)(i + 1)/(float)(x_splits + 1));
	for (int i = 0; i < y_splits; i++, s++)
		splits[s] = new_split(box, Y_AXIS, (float)(i + 1) /(float)(y_splits + 1));
	for (int i = 0; i < z_splits; i++, s++)
		splits[s] = new_split(box, Z_AXIS, (float)(i + 1) / (float)(z_splits + 1));

	return splits;
}

Split *best_spatial_split(AABB *box)
{
	Split **spatials = new_allocate_splits(box);

	//for each member, "add" to each split (dont actually make copies)
	int count = 0;
	for (int i = 0; i < SPLIT_TEST_NUM; i++)
	{
		for (AABB *b = box->members; b != NULL; b = b->next)
			if (box_in_box(b, spatials[i]->left) && box_in_box(b, spatials[i]->right) && !all_in(b, spatials[i]->right) && !all_in(b, spatials[i]->left))
			{
				AABB *lclip = dupe_box(b);
				AABB *rclip = dupe_box(b);

				clip_box(lclip, spatials[i]->left);
				clip_box(rclip, spatials[i]->right);

				flex_box(spatials[i]->left_flex, lclip);
				flex_box(spatials[i]->right_flex, rclip);

				spatials[i]->left_count++;
				spatials[i]->right_count++;
				spatials[i]->both_count++;
				
				free(lclip);
				free(rclip);
			}
			else if (all_in(b, spatials[i]->left))
			{
				flex_box(spatials[i]->left_flex, b);
				spatials[i]->left_count++;
			}
			else if (all_in(b, spatials[i]->right))
			{
				flex_box(spatials[i]->right_flex, b);
				spatials[i]->right_count++;
			}
			else
			{
				printf("member not in any box (shouldnt happen) (spatial)\n");
				print_box(b);
				print_split(spatials[i]);
				//getchar();
			}
	}
	return pick_best(spatials, box);
}

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

// float SA(AABB *box)
// {
// 	cl_float3 span = vec_sub(box->max, box->min);
// 	span = (cl_float3){fabs(span.x), fabs(span.y), fabs(span.z)};
// 	return 2 * span.x * span.y + 2 * span.y * span.z + 2 * span.x * span.z;
// }

// float SAH(Split *split, AABB *parent)
// {
// 	return (SA(split->left_flex) * split->left_count + SA(split->right_flex) * split->right_count) / SA(parent);
// }

Split *scan_tallies(AABB *ltr, AABB *rtl, int count, AABB *parent, enum axis ax)
{
	//find best object split on this axis
	float best_SAH = FLT_MAX;
	int best_ind = -1;

	for (int i = 0; i < count - 1; i++)
	{
		float this_SAH = (SA(&ltr[i]) * (i + 1) + SA(&rtl[i + 1]) * (count - i)) / SA(parent);
		if (this_SAH < best_SAH)
		{
			best_ind = i;
			best_SAH = this_SAH;
		}
	}

	Split *best = calloc(1, sizeof(Split));
	best->ax = ax;
	best->left = dupe_box(&ltr[best_ind]);
	best->left_count = best_ind + 1;
	best->right = dupe_box(&rtl[best_ind + 1]);
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

int spatial_wins;
int object_wins;

float root_SA;

void partition(AABB *box)
{
	if (VERBOSE)
		printf("trying to partition a box with member count %d\n", box->member_count);

	Split *object = fast_object_split(box);
	Split *spatial = NULL;

	if (SPATIAL_ENABLE && (!object || SA_overlap(object) / root_SA > ALPHA))
	{
		spatial = best_spatial_split(box);
		if (VERBOSE)
			printf("SAH obj %.2f SAH spatial %.2f\n", SAH(object, box), SAH(spatial, box));
	}

	if (spatial == NULL && object == NULL)
	{
		printf("splits failed, bailing out!\n");
		return;
	}
	else if (spatial == NULL || (object != NULL && SAH(object, box) < SAH(spatial, box)))
	{
		if (VERBOSE)
			printf("CHOSE OBJECT\n");
		object_wins++;
		AABB **members = calloc(box->member_count, sizeof(AABB *));
		AABB *b = box->members;
		for (int i = 0; b; b = b->next, i++)
			members[i] = b;

		pointer_axis_sort(members, box->member_count, object->ax);

		box->left = dupe_box(object->left);
		box->right = dupe_box(object->right);
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
	else
	{
		if (VERBOSE)
			printf("CHOSE SPATIAL\n");
		spatial_wins++;
		box->left = dupe_box(spatial->left_flex);
		box->right = dupe_box(spatial->right_flex);

		for (AABB *b = box->members; b != NULL;)
		{
			AABB *tmp = b->next;
			if (box_in_box(b, spatial->left) && box_in_box(b, spatial->right) && !all_in(b, spatial->left) && !all_in(b, spatial->right))
			{
				AABB *lclip = dupe_box(b);
				AABB *rclip = dupe_box(b);

				clip_box(lclip, spatial->left);
				clip_box(rclip, spatial->right);

				push(&box->left->members, lclip);
				box->left->member_count++;
				push(&box->right->members, rclip);
				box->right->member_count++;

				free(b);
			}
			else if (all_in(b, spatial->left))
			{
				push(&box->left->members, b);
				box->left->member_count++;
			}
			else if (all_in(b, spatial->right))
			{
				push(&box->right->members, b);
				box->right->member_count++;
			}
			else
			{
				printf("\n\nnot in any final box, real problem (spatial)\n");
				print_box(b);
				print_split(spatial);
				print_box(box);
				getchar();
			}
			b = tmp;
		}
	}
	if (object)
		free_split(object);
	if (spatial)
		free_split(spatial);

	if (VERBOSE)
		printf("the split resulted in: L:%d, R:%d\n", box->left->member_count, box->right->member_count);
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

	printf("faces are in boxes, %d\n", fcount);

	AABB *root_box = box_from_boxes(boxes);

	root_SA = SA(root_box);

	printf("root box made\n");
	print_vec(root_box->min);
	print_vec(root_box->max);

	AABB *stack = root_box;
	int count = 1;
	int ref_count = 0;
	spatial_wins = 0;
	object_wins = 0;
	while (stack)
	{
		AABB *box = pop(&stack);
		partition(box);
		if (box->left)
		{
			box->left->parent = box;
			box->right->parent = box;
			count += 2;

			if (box->left->member_count > LEAF_THRESHOLD)
				push(&stack, box->left);
			else
				ref_count += box->left->member_count;

			if (box->right->member_count > LEAF_THRESHOLD)
				push(&stack, box->right);
			else
				ref_count += box->right->member_count;
		}
		else
		{
			printf("chose not to split this box\n");
			print_box(box);
		}
	}
	printf("done. %d boxes", count);
	printf("%d member references vs %d starting\n", ref_count, root_box->member_count);
	printf("pick rates: spatial %.2f object %.2f\n", 100.0f * (float)spatial_wins / (float)(spatial_wins + object_wins), 100.0f * (float)object_wins / (float)(spatial_wins + object_wins));
	
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