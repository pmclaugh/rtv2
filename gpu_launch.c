#include "rt.h"

typedef struct s_gpu_mat
{
	cl_float3 Ka;
	cl_float3 Kd;
	cl_float3 Ks;
	cl_float3 Ke;
	cl_int tex_ind;
	cl_int tex_w;
	cl_int tex_h;
}				gpu_mat;


# ifndef DEVICE
# define DEVICE CL_DEVICE_TYPE_DEFAULT
# endif

cl_float3 tfloat_to_cl(cl_float3 v)
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
	char *source = calloc(sizeof(char),30000); // real kernel is about 8k characters, whatever
	read(fd, source, 30000);
	close(fd);
	return source;
}

cl_float3 *gpu_render(Scene *s, t_camera cam)
{

	static cl_context context;
	static cl_command_queue commands;
	static cl_program p;
	static cl_mem d_scene;
	static cl_mem d_output;
	static cl_mem d_seeds;
	static cl_mem d_mats;
	static cl_mem d_boxes;
	static cl_mem d_const_boxes;
	static cl_mem d_tex;
	static int first = 1;

	cl_float3 *h_output = calloc(xdim * ydim, sizeof(cl_float3));

	if (first)
	{
		printf("doing the heavy stuff\n");
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
	    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);

	    // Create a command queue
	    commands = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err);
	    //printf("setup context\n");


	    char *source = load_cl_file("new_kernel.cl");

	    p = clCreateProgramWithSource(context, 1, (const char **) &source, NULL, &err);

	    // Build the program
	    err = clBuildProgram(p, 0, NULL, NULL, NULL, NULL);
	    if (err != CL_SUCCESS)
	    {
	        size_t len;
	        char buffer[8192];

	        //printf("Error: Failed to build program executable!\n%s\n", err_code(err));
	        clGetProgramBuildInfo(p, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
	        printf("%s\n", buffer);
	        free(source);
	        exit(0);
	    }
	    free(source);
	    //printf("made kernel\n");

		//set up memory areas
		cl_uint *h_seeds = calloc(xdim * ydim * 2, sizeof(cl_uint));

		for (int i = 0; i < xdim * ydim * 2; i++)
			h_seeds[i] = rand();

		printf("about to do textures\n");
		cl_int tex_size = 0;
		for (int i = 0; i < s->mat_count; i++)
		{
			if (s->materials[i].map_Kd)
				tex_size += s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3;
			printf("%d\n", tex_size);
		}

		printf("tex size? %d\n", tex_size);
		cl_uchar *h_tex = calloc(sizeof(cl_uchar), tex_size);
		tex_size = 0;

		gpu_mat *simple_mats = calloc(s->mat_count, sizeof(gpu_mat));
		for (int i = 0; i < s->mat_count; i++)
		{
			if (s->materials[i].map_Kd)
			{
				memcpy(&h_tex[tex_size], s->materials[i].map_Kd->pixels, s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3);
				simple_mats[i] = (gpu_mat){
					s->materials[i].Ka,
					s->materials[i].Kd,
					s->materials[i].Ks,
					s->materials[i].Ke,
					tex_size,
					s->materials[i].map_Kd->width,
					s->materials[i].map_Kd->height
				};
				tex_size += s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3;
			}
			else
			{
				printf("tex for material %d is null!\n", i);
				simple_mats[i] = (gpu_mat){
					s->materials[i].Ka,
					s->materials[i].Kd,
					s->materials[i].Ks,
					s->materials[i].Ke,
					0,
					0,
					0,
				};
			}
		}

		printf("flattened textures, %d bytes\n", tex_size);

		d_scene = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(Face) * s->face_count, NULL, NULL);
		d_mats = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(gpu_mat) * s->mat_count, NULL, NULL);
		d_output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_float3) * xdim * ydim, NULL, NULL);
		d_seeds = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_uint) * xdim * ydim * 2, NULL, NULL);
		d_boxes = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(Box) * s->box_count, NULL, NULL);
		d_const_boxes = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(C_Box) * s->c_box_count, NULL, NULL);
		d_tex = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_uchar) * tex_size, NULL, NULL);

		clEnqueueWriteBuffer(commands, d_scene, CL_TRUE, 0, sizeof(Face) * s->face_count, s->faces, 0, NULL, NULL);
		clEnqueueWriteBuffer(commands, d_mats, CL_TRUE, 0, sizeof(gpu_mat) * s->mat_count, simple_mats, 0, NULL, NULL);
		clEnqueueWriteBuffer(commands, d_seeds, CL_TRUE, 0, sizeof(cl_uint) * xdim * ydim * 2, h_seeds, 0, NULL, NULL);
		clEnqueueWriteBuffer(commands, d_boxes, CL_TRUE, 0, sizeof(Box) * s->box_count, s->boxes, 0, NULL, NULL);
		clEnqueueWriteBuffer(commands, d_const_boxes, CL_TRUE, 0, sizeof(C_Box) * s->c_box_count, s->c_boxes, 0, NULL, NULL);
		clEnqueueWriteBuffer(commands, d_output, CL_TRUE, 0, sizeof(cl_float3) * xdim * ydim, h_output, 0, NULL, NULL);
		clEnqueueWriteBuffer(commands, d_tex, CL_TRUE, 0, sizeof(cl_uchar) * tex_size, h_tex, 0, NULL, NULL);

		clRetainContext(context);
		clRetainCommandQueue(commands);
		clRetainProgram(p);
		clRetainMemObject(d_scene);
		clRetainMemObject(d_output);
		clRetainMemObject(d_seeds);
		clRetainMemObject(d_tex);
		first = 0;
	}

	// Create the compute kernel from the program (this can't be retained for some reason)
	cl_kernel k = clCreateKernel(p, "render_kernel", NULL);
	int i_xdim = xdim;
	int i_ydim = ydim;
	size_t resolution = xdim * ydim;
	size_t groupsize = 256;
	cl_int obj_count = s->c_box_count;

	cl_uint total_samples = 1;
	cl_float3 gpu_cam_origin = tfloat_to_cl(cam.origin);
	cl_float3 gpu_cam_focus = tfloat_to_cl(cam.focus);
	cl_float3 gpu_cam_dx = tfloat_to_cl(cam.d_x);
	cl_float3 gpu_cam_dy = tfloat_to_cl(cam.d_y);


	clSetKernelArg(k, 0, sizeof(cl_mem), &d_scene);
	clSetKernelArg(k, 1, sizeof(cl_mem), &d_mats);
	clSetKernelArg(k, 2, sizeof(cl_mem), &d_boxes);
	clSetKernelArg(k, 3, sizeof(cl_float3), &gpu_cam_origin);
	clSetKernelArg(k, 4, sizeof(cl_float3), &gpu_cam_focus);
	clSetKernelArg(k, 5, sizeof(cl_float3), &gpu_cam_dx);
	clSetKernelArg(k, 6, sizeof(cl_float3), &gpu_cam_dy);
	clSetKernelArg(k, 7, sizeof(cl_int), &i_xdim);
	clSetKernelArg(k, 8, sizeof(cl_mem), &d_output);
	clSetKernelArg(k, 9, sizeof(cl_mem), &d_seeds);
	clSetKernelArg(k, 10, sizeof(cl_uint), &total_samples);
	clSetKernelArg(k, 11, sizeof(cl_mem), &d_const_boxes);
	clSetKernelArg(k, 12, sizeof(cl_uint), &obj_count);
	clSetKernelArg(k, 13, sizeof(cl_mem), &d_tex);

	printf("about to fire\n");
	//pull the trigger

	// cl_event render;
	// cl_int err = clEnqueueNDRangeKernel(commands, k, 1, 0, &resolution, &groupsize, 0, NULL, &render);
	// clFinish(commands);
	// clEnqueueReadBuffer(commands, d_output, CL_TRUE, 0, sizeof(cl_float3) * xdim * ydim, h_output, 0, NULL, NULL);

	// cl_ulong start, end;
	// clGetEventProfilingInfo(render, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
	// clGetEventProfilingInfo(render, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);

	// printf("kernel took %.3f seconds\n", (float)(end - start) / 1000000000.0f);
	return h_output;
}