#include "rt.h"

typedef struct s_gpu_object
{
	cl_int shape;
	cl_int material;
	cl_float3 v0;
	cl_float3 v1;
	cl_float3 v2;
	cl_float3 color;
	cl_float3 emission;
}				gpu_object;

#define GPU_SPHERE 1
#define GPU_TRIANGLE 2

#define GPU_MAT_DIFFUSE 1
#define GPU_MAT_SPECULAR 2
#define GPU_MAT_REFRACTIVE 3
#define GPU_MAT_NULL 4

# ifndef DEVICE
# define DEVICE CL_DEVICE_TYPE_DEFAULT
# endif

cl_float3 tfloat_to_cl(t_float3 v)
{
	return (cl_float3){v.x, v.y, v.z};
}

void print_clf3(cl_float3 v)
{
	printf("%f %f %f\n", v.x, v.y, v.z);
}

#include <fcntl.h>
char *load_cl_file(char *file)
{
	int fd = open(file, O_RDONLY);
	char *source = malloc(8192); // real kernel is about 5k characters, whatever
	read(fd, source, 8192);
	close(fd);
	return source;
}

cl_float3 *gpu_render(t_scene *scene, t_camera cam)
{
	cl_uint numPlatforms;
    int err;

    // Find number of platforms
    err = clGetPlatformIDs(0, NULL, &numPlatforms);

    // Get all platforms
    cl_platform_id Platform[numPlatforms];
    err = clGetPlatformIDs(numPlatforms, Platform, NULL);

    // Secure a GPU
    cl_device_id device_id;
    for (int i = 0; i < numPlatforms; i++)
    {
        err = clGetDeviceIDs(Platform[i], DEVICE, 1, &device_id, NULL);
        if (err == CL_SUCCESS)
        {
            break;
        }
    }

    // Create a compute context
    cl_context context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);

    // Create a command queue
    cl_command_queue commands = clCreateCommandQueue(context, device_id, 0, &err);
    printf("setup context\n");


    cl_kernel k;
    cl_program p;

    char *source = load_cl_file("kernel.cl");

    p = clCreateProgramWithSource(context, 1, (const char **) &source, NULL, &err);

    // Build the program
    err = clBuildProgram(p, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        printf("bad compile, use tool to check\n");
        exit(0);
    }

    // Create the compute kernel from the program
    k = clCreateKernel(p, "render_kernel", &err);
    clReleaseProgram(p);
    printf("made kernel\n");


	//consolidate scene->objects to an array (this is a hack to avoid changing lots of other stuff, will rework)
	t_object *obj = scene->objects;
	int obj_count = 0;
	while (obj && ++obj_count)
		obj = obj->next;

	gpu_object * gpu_scene = malloc(sizeof(gpu_object) * obj_count);
	obj = scene->objects;
	for (int i = 0; i < obj_count; i++)
	{
		gpu_scene[i].v0 = (cl_float3){obj->position.x, obj->position.y, obj->position.z};
		gpu_scene[i].v1 = (cl_float3){obj->normal.x, obj->normal.y, obj->normal.z};
		gpu_scene[i].v2 = (cl_float3){obj->corner.x, obj->corner.y, obj->corner.z};
		gpu_scene[i].color = (cl_float3){obj->color.x, obj->color.y, obj->color.z};
		gpu_scene[i].emission = (cl_float3){obj->emission, obj->emission, obj->emission};
		
		if (obj->shape == SPHERE)
			gpu_scene[i].shape = GPU_SPHERE;
		else if (obj->shape == TRIANGLE)
			gpu_scene[i].shape = GPU_TRIANGLE;

		if (obj->material == MAT_DIFFUSE)
			gpu_scene[i].material = GPU_MAT_DIFFUSE;
		else if (obj->material == MAT_SPECULAR)
			gpu_scene[i].material = GPU_MAT_SPECULAR;
		obj = obj->next;
	}

	printf("obj count is %d\n", obj_count);

	printf("scene translated\n");
	//transform cam stuff to simple vectors
	cl_float3 gpu_cam_origin = tfloat_to_cl(cam.origin);
	cl_float3 gpu_cam_focus = tfloat_to_cl(cam.focus);
	cl_float3 gpu_cam_dx = tfloat_to_cl(cam.d_x);
	cl_float3 gpu_cam_dy = tfloat_to_cl(cam.d_y);

	print_clf3(gpu_cam_origin);
	print_clf3(gpu_cam_focus);
	print_clf3(gpu_cam_dx);
	print_clf3(gpu_cam_dy);

	printf("translated cam\n");
	//set up memory areas
	cl_float3 *h_output = calloc(xdim * ydim, sizeof(cl_float3));

	cl_mem d_scene;
	cl_mem d_output;

	d_scene = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(gpu_object) * obj_count, NULL, NULL);
	d_output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_float3) * xdim * ydim, NULL, NULL);
	clEnqueueWriteBuffer(commands, d_scene, CL_TRUE, 0, sizeof(gpu_object) * obj_count, gpu_scene, 0, NULL, NULL);

	printf("copied scene to gpu\n");

	int i_xdim = xdim;
	int i_ydim = ydim;
	size_t resolution = xdim * ydim;
	size_t groupsize = 256;

	//DEBUGGING
	//gpu_cam_origin = (cl_float3){0.5f, 0.5f, 0.5f};
	//END DEBUG

	clSetKernelArg(k, 0, sizeof(cl_mem), &d_scene);
	printf("scene arg\n");
	clSetKernelArg(k, 1, sizeof(cl_float3), &gpu_cam_origin);
	clSetKernelArg(k, 2, sizeof(cl_float3), &gpu_cam_focus);
	clSetKernelArg(k, 3, sizeof(cl_float3), &gpu_cam_dx);
	clSetKernelArg(k, 4, sizeof(cl_float3), &gpu_cam_dy);
	printf("cam args\n");
	clSetKernelArg(k, 5, sizeof(cl_int), &i_xdim);
	clSetKernelArg(k, 6, sizeof(cl_int), &i_ydim);
	clSetKernelArg(k, 7, sizeof(cl_int), &obj_count);
	printf("dim args\n");
	clSetKernelArg(k, 8, sizeof(cl_mem), &d_output);
	printf("output arg\n");

	printf("about to fire\n");
	//pull the trigger
	clEnqueueNDRangeKernel(commands, k, 1, 0, &resolution, &groupsize, 0, NULL, NULL);
	clFinish(commands);
	clEnqueueReadBuffer(commands, d_output, CL_TRUE, 0, sizeof(cl_float3) * xdim * ydim, h_output, 0, NULL, NULL);

	for (int i = 0; i < 10; i++)
		print_clf3(h_output[i]);


	return h_output;
}