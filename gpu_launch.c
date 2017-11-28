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

cl_float3 *gpu_render(Scene *s, t_camera cam, int xdim, int ydim)
{

	static cl_context *contexts;
	static cl_command_queue *commands;
	static cl_program *programs;
	static cl_mem *d_scene;
	static cl_mem **d_output;
	static cl_mem **d_seeds;
	static cl_mem *d_mats;
	static cl_mem *d_boxes;
	static cl_mem *d_const_boxes;
	static cl_mem *d_tex;
	static int first = 1;
	static cl_uint numPlatforms;
	static cl_uint numDevices;
	static cl_platform_id *Platform;

	if (first)
	{
		printf("doing the heavy stuff\n");
	    int err;

	    // Find number of platforms
	    err = clGetPlatformIDs(0, NULL, &numPlatforms);

	    // Get all platforms
	    Platform = calloc(numPlatforms, sizeof(cl_platform_id));
	    err = clGetPlatformIDs(numPlatforms, Platform, NULL);

	    //count devices
	    numDevices = 0;
	    for (int i = 0; i < numPlatforms; i++)
	    {
	    	cl_uint d;
	        err = clGetDeviceIDs(Platform[i], CL_DEVICE_TYPE_ALL, 0, NULL, &d);
	        if (err == CL_SUCCESS)
	            numDevices += d;
	    }

	    printf("%u devices, %u platforms\n", numDevices, numPlatforms);

	    //get ids for devices and create (platforms) compute contexts, (devices) command queues
	    cl_device_id device_ids[numDevices];
	    cl_uint offset = 0;
	    contexts = calloc(numPlatforms, sizeof(cl_context));
	    commands = calloc(numDevices, sizeof(cl_command_queue));
	    for (int i = 0; i < numPlatforms; i++)
	    {
	    	cl_uint d;
	    	clGetDeviceIDs(Platform[i], CL_DEVICE_TYPE_GPU, numDevices, &device_ids[offset], &d);
	    	contexts[i] = clCreateContext(0, d, &device_ids[offset], NULL, NULL, &err);
	    	for (int j = 0; j < d; j++)
	    		commands[offset + j] = clCreateCommandQueue(contexts[i], device_ids[offset + j], CL_QUEUE_PROFILING_ENABLE, &err);
	    	offset += d;
	    }
	    printf("made contexts and CQs\n");

	    char *source = load_cl_file("new_kernel.cl");


	    //create (platforms) programs and build them
	    programs = calloc(numPlatforms, sizeof(cl_program));
	    for (int i = 0; i < numPlatforms; i++)
	    {
	    	programs[i] = clCreateProgramWithSource(contexts[i], 1, (const char **) &source, NULL, &err);
	    	err = clBuildProgram(programs[i], 0, NULL, NULL, NULL, NULL);
	    	if (err != CL_SUCCESS)
	    	{
	    	 	printf("bad compile\n");
	    	 	exit(0);
	    	}
		    free(source);
	    }

	    printf("built programs\n");

	    //seeds for random number generation (2 per thread per device)
		cl_uint *h_seeds = calloc(xdim * ydim * 2 * numDevices, sizeof(cl_uint));
		for (int i = 0; i < xdim * ydim * 2 * numDevices; i++)
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

		offset = 0;
		d_scene = calloc(numPlatforms, sizeof(cl_mem));
		d_mats = calloc(numPlatforms, sizeof(cl_mem));
		d_output = calloc(numPlatforms, sizeof(cl_mem *));
		d_seeds = calloc(numPlatforms, sizeof(cl_mem *));
		d_boxes = calloc(numPlatforms, sizeof(cl_mem));
		d_const_boxes = calloc(numPlatforms, sizeof(cl_mem));
		d_tex = calloc(numPlatforms, sizeof(cl_mem));
		for (int i = 0; i < numPlatforms; i++)
		{
			cl_uint d;
			clGetDeviceIDs(Platform[i], CL_DEVICE_TYPE_ALL, 0, NULL, &d);

			d_output[i] = calloc(d, sizeof(cl_mem));
			d_seeds[i] = calloc(d, sizeof(cl_mem));
			d_scene[i] = clCreateBuffer(contexts[i], CL_MEM_READ_ONLY, sizeof(Face) * s->face_count, NULL, NULL);
			d_mats[i] = clCreateBuffer(contexts[i], CL_MEM_READ_ONLY, sizeof(gpu_mat) * s->mat_count, NULL, NULL);
			d_boxes[i] = clCreateBuffer(contexts[i], CL_MEM_READ_ONLY, sizeof(Box) * s->box_count, NULL, NULL);
			d_const_boxes[i] = clCreateBuffer(contexts[i], CL_MEM_READ_ONLY, sizeof(C_Box) * s->c_box_count, NULL, NULL);
			d_tex[i] = clCreateBuffer(contexts[i], CL_MEM_READ_ONLY, sizeof(cl_uchar) * tex_size, NULL, NULL);
			
			for (int j = 0; j < d; j++)
			{
				d_output[i][j] = clCreateBuffer(contexts[i], CL_MEM_WRITE_ONLY, sizeof(cl_float3) * xdim * ydim, NULL, NULL);
				d_seeds[i][j] = clCreateBuffer(contexts[i], CL_MEM_READ_ONLY, sizeof(cl_uint) * xdim * ydim * 2, NULL, NULL);
				clEnqueueWriteBuffer(commands[offset + j], d_scene[i], CL_TRUE, 0, sizeof(Face) * s->face_count, s->faces, 0, NULL, NULL);
				clEnqueueWriteBuffer(commands[offset + j], d_mats[i], CL_TRUE, 0, sizeof(gpu_mat) * s->mat_count, simple_mats, 0, NULL, NULL);
				clEnqueueWriteBuffer(commands[offset + j], d_seeds[i][j], CL_TRUE, 0, sizeof(cl_uint) * xdim * ydim * 2, &h_seeds[(offset + j) * xdim * ydim * 2], 0, NULL, NULL);
				clEnqueueWriteBuffer(commands[offset + j], d_boxes[i], CL_TRUE, 0, sizeof(Box) * s->box_count, s->boxes, 0, NULL, NULL);
				clEnqueueWriteBuffer(commands[offset + j], d_const_boxes[i], CL_TRUE, 0, sizeof(C_Box) * s->c_box_count, s->c_boxes, 0, NULL, NULL);
				clEnqueueWriteBuffer(commands[offset + j], d_tex[i], CL_TRUE, 0, sizeof(cl_uchar) * tex_size, h_tex, 0, NULL, NULL);
			}
			offset += d;
		}
		first = 0;
	}

	// Create the compute kernel from the program (this can't be retained for some reason)
	cl_kernel *kernels = calloc(numPlatforms, sizeof(cl_kernel));
	for (int i = 0; i < numPlatforms; i++)
		kernels[i] = clCreateKernel(programs[i], "render_kernel", NULL);

	size_t resolution = xdim * ydim;
	size_t groupsize = 256;
	cl_int obj_count = s->c_box_count;

	cl_uint samples_per_device = 10;

	for (int i = 0; i < numPlatforms; i++)
	{
		clSetKernelArg(kernels[i], 0, sizeof(cl_mem), &d_scene[i]);
		clSetKernelArg(kernels[i], 1, sizeof(cl_mem), &d_mats[i]);
		clSetKernelArg(kernels[i], 2, sizeof(cl_mem), &d_boxes[i]);
		clSetKernelArg(kernels[i], 3, sizeof(cl_float3), &cam.origin);
		clSetKernelArg(kernels[i], 4, sizeof(cl_float3), &cam.focus);
		clSetKernelArg(kernels[i], 5, sizeof(cl_float3), &cam.d_x);
		clSetKernelArg(kernels[i], 6, sizeof(cl_float3), &cam.d_y);
		clSetKernelArg(kernels[i], 7, sizeof(cl_int), &xdim);
		// clSetKernelArg(kernels[i], 8, sizeof(cl_mem), &d_output[i]);
		// clSetKernelArg(kernels[i], 9, sizeof(cl_mem), &d_seeds[i]);
		clSetKernelArg(kernels[i], 10, sizeof(cl_uint), &samples_per_device);
		clSetKernelArg(kernels[i], 11, sizeof(cl_mem), &d_const_boxes[i]);
		clSetKernelArg(kernels[i], 12, sizeof(cl_uint), &obj_count);
		clSetKernelArg(kernels[i], 13, sizeof(cl_mem), &d_tex[i]);
	}

	printf("about to fire\n");
	//pull the trigger

	cl_event render[numDevices];

	cl_float3 **outputs = calloc(numDevices, sizeof(cl_float3 *));
	cl_uint offset = 0;
	for (int i = 0; i < numPlatforms; i++)
	{
		cl_uint d;
		clGetDeviceIDs(Platform[i], CL_DEVICE_TYPE_ALL, 0, NULL, &d);
		for (int j = 0; j < d; j++)
		{
			clSetKernelArg(kernels[i], 8, sizeof(cl_mem), &d_output[i][j]);
			clSetKernelArg(kernels[i], 9, sizeof(cl_mem), &d_seeds[i][j]);
			outputs[offset + j] = calloc(xdim * ydim, sizeof(cl_float3));
			cl_int err = clEnqueueNDRangeKernel(commands[offset + j], kernels[i], 1, 0, &resolution, &groupsize, 0, NULL, &render[offset + j]);
			clEnqueueReadBuffer(commands[offset + j], d_output[i][j], CL_FALSE, 0, sizeof(cl_float3) * xdim * ydim, outputs[offset + j], 1, &render[offset + j], NULL);
		}
		offset += d;
	}
	printf("all enqueued\n");
	for (int i = 0; i < numDevices; i++)
	{
		clFinish(commands[i]);
		cl_ulong start, end;
		clGetEventProfilingInfo(render[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
		clGetEventProfilingInfo(render[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
		printf("kernel %d took %.3f seconds\n", i, (float)(end - start) / 1000000000.0f);
	}
	printf("done?\n");
	cl_float3 *output_sum = calloc(resolution, sizeof(cl_float3));
	for (int i = 0; i < numDevices; i++)
	{
		for (int j = 0; j < resolution; j++)
			output_sum[j] = vec_add(output_sum[j], outputs[i][j]);
		free(outputs[i]);
	}

	for (int i = 0; i < resolution; i++)
		output_sum[i] = vec_scale(output_sum[i], 1.0f / (float)(samples_per_device * numDevices));

	free(kernels);
	return output_sum;
}