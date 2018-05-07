#include "rt.h"

void	save_img(cl_float3 *image)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *outfile;		/* target file */
	JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
	int row_stride;		/* physical row width in image buffer */

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	char *filename = "../Desktop/image.jpeg";
	if ((outfile = fopen(filename, "wb")) == NULL)
	{
		fprintf(stderr, "can't open %s\n", filename);
		exit(1);
	}
	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = DIM_PT; 	/* image width and height, in pixels */
	cinfo.image_height = DIM_PT;
	cinfo.input_components = 3;		/* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
	jpeg_set_defaults(&cinfo);
//	jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

	jpeg_start_compress(&cinfo, TRUE);

	row_stride = cinfo.image_width * 3;	/* JSAMPLEs per row in image_buffer */

	unsigned char *img_data = calloc((DIM_PT * DIM_PT) * 3, sizeof(unsigned char));
	for (int y = 0; y < DIM_PT; y++)
		for (int x = 0; x < DIM_PT; x++)
		{
			img_data[(x * 3) + (y * DIM_PT * 3)] = image[x + (y * DIM_PT)].x * 255.0f;
			img_data[1 + (x * 3) + (y * DIM_PT * 3)] = image[x + (y * DIM_PT)].y * 255.0f;
			img_data[2 + (x * 3) + (y * DIM_PT * 3)] = image[x + (y * DIM_PT)].z * 255.0f;
		}
	while (cinfo.next_scanline < cinfo.image_height)
	{
/* jpeg_write_scanlines expects an array of pointers to scanlines.
* Here the array is only one element long, but you could pass
* more than one scanline at a time if that's more convenient.
*/
		row_pointer[0] = &img_data[cinfo.next_scanline * row_stride];
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
	jpeg_finish_compress(&cinfo);
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);
	free(img_data);
}