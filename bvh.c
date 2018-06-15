#include "rt.h"

#define VERBOSE 0
#define SPATIAL_ENABLE 1

#define ALPHA 0.0001f

#define Ci 1.0f
#define Ct 10.0f

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
		float this_SAH = (SA(&ltr[i]) * (i + 1) + SA(&rtl[i + 1]) * (count - i - 1)) / SA(parent);
		if (this_SAH > FLT_MAX)
		{
			printf("this_SAH: %f\n", this_SAH);
			printf("left SA %f right SA %f\n", SA(&ltr[i]), SA(&rtl[i + 1]));
			print_vec(ltr[i].min);
			print_vec(ltr[i].max);
			//getchar();
		}
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
	best->right_count = count - best_ind - 1;
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
	
	for (int i = 1; i < count; i++)
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

	for (int i = 1; i < count; i++)
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

	free(members);
	free(tallies_ltr);
	free(tallies_rtl);
	return best_split;
}

float root_SA;

int box_contains_box(AABB *big, AABB *little)
{
	if (point_in_box(little->min, big) && point_in_box(little->max, big))
		return 1;
	else
		return 0;
}

void partition(AABB *box)
{
	Split *object = fast_object_split(box);
	Split *spatial = NULL;

	if(((SA_overlap(object) / root_SA) > ALPHA) && SPATIAL_ENABLE)
		spatial = fast_spatial_split(box);

	float object_sah = SAH(object, box);
	float spatial_sah = spatial ? SAH(spatial, box) : FLT_MAX;

	float best_sah;
	Split *chosen;
	if (object_sah <= spatial_sah)
	{
		best_sah = object_sah;
		chosen = object;
	}
	else
	{
		best_sah = spatial_sah;
		chosen = spatial;
	}

	if (best_sah < Ci * box->member_count) //make sure it's worth splitting at all (dynamic termination)
	{
		if (chosen == object)
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
		else
		{
			box->left = dupe_box(spatial->left_flex);
			box->right = dupe_box(spatial->right_flex);

			// printf("we settled on this split:\n");
			// print_split(spatial);

			//I built the clip function to work on Bins not AABBs so do a quick conversion
			Bin left, right;
			left.bin_min = spatial->left->min;
			left.bin_max = spatial->left->max;
			right.bin_min = spatial->right->min;
			right.bin_max = spatial->right->max;

			int axis = spatial->ax - X_AXIS; //awkward, fix

			//now distribute the members and re-clip on the chosen bounds if needed.
			//seems a little inefficient but it's not that expensive and saves the hassle of tracking fragments from before.
			for (AABB *b = box->members; b;)
			{
				AABB *tmp = b->next;

				if (point_in_box(b->max, spatial->left))
				{
					push(&box->left->members, b);
					box->left->member_count++;
				}
				else if (point_in_box(b->min, spatial->right))
				{
					push(&box->right->members, b);
					box->right->member_count++;
				}
				else
				{
					AABB *lclip, *rclip;
					
					lclip = clip(b, &left, axis);
					rclip = clip(b, &right, axis);
					
					lclip->f = b->f;
					rclip->f = b->f;

					push(&box->left->members, lclip);
					push(&box->right->members, rclip);
					
					box->left->member_count++;
					box->right->member_count++;

					free(b);
				}

				b = tmp;
			}
		}
	//verify assumptions
	
	if (chosen->left_count != box->left->member_count)
	{
		printf("left count %d, expected %d\n", box->left->member_count, spatial->left_count);
		//getchar();
	}
	if (chosen->right_count != box->right->member_count)
	{
		printf("right count %d, expected %d\n", box->right->member_count, spatial->right_count);
		//getchar();
	}

	for (AABB *b = box->left->members; b; b = b->next)
		if (!box_contains_box(box->left, b))
		{
			printf("member out of bounds!!!\n");
			print_box(b);
			printf("is not in (left)\n");
			print_box(box->left);
			printf("assuming error is small, flexing\n");
			flex_box(box->left, b);
			printf("new box\n");
			print_box(box->left);
		}
	for (AABB *b = box->right->members; b; b = b->next)
		if (!box_contains_box(box->right, b))
		{
			printf("member out of bounds!!!\n");
			print_box(b);
			printf("is not in (right)\n");
			print_box(box->right);
			printf("assuming error is small, flexing\n");
			flex_box(box->right, b);
			printf("new box\n");
			print_box(box->right);
		}
	}

	if (object)
		free_split(object);
	if (spatial)
		free_split(spatial);
}

void sbvh(t_env *env)
{
	printf("start bvh process\n");
	clock_t start, end;
	start = clock();

	//some preprocessing
	Face *faces = NULL;
	for (int i = 0; i < env->scene->face_count; i++)
	{
		Face *f = calloc(1, sizeof(Face));
		memcpy(f, &env->scene->faces[i], sizeof(Face));
		f->next = faces;

		for (int i = 0; i < 3; i++)
		{
			f->edges[i] = unit_vec(vec_sub(f->verts[(i + 1) % 3], f->verts[i]));
			f->edge_t[i] = vec_mag(vec_sub(f->verts[(i + 1) % 3], f->verts[i]));
		}

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

	root_SA = SA(root_box);

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
	printf("done. %d boxes\n", count);
	printf("%d member references vs %d starting\n", ref_count, root_box->member_count);

	env->scene->bins = root_box;
	env->scene->bin_count = count;
	env->scene->face_count = ref_count;
	end = clock();
	printf("took %.4f seconds\n", (float)(end - start) / CLOCKS_PER_SEC);
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