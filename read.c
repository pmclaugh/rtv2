#include "rt.h"

typedef union	s_union16
{
	uint8_t			bytes[2];
	short			value;
	unsigned short	u_value;
}				t_union16;

typedef union	s_union32
{
	uint8_t			bytes[4];
	int				value;
	unsigned int	u_value;
	float			f_value;
}				t_union32;

typedef union	s_union64
{
	uint8_t			bytes[8];
	double			value;
}				t_union64;

char			read_char(FILE *fp, const int file_endian, const int machine_endian)
{
	char	value = 0;

	fread(&value, 1, 1, fp);
	return value;
}

unsigned char	read_uchar(FILE *fp, const int file_endian, const int machine_endian)
{
	unsigned char	value = 0;

	fread(&value, 1, 1, fp);
	return value;
}

short			read_short(FILE *fp, const int file_endian, const int machine_endian)
{
	t_union16 u;

	bzero(u.bytes, 2);
	fread(u.bytes, 1, 2, fp);
	if (file_endian != machine_endian)
	{
		uint8_t temp = u.bytes[0];
		u.bytes[0] = u.bytes[1];
		u.bytes[1] = temp;
	}
	return u.value;
}

unsigned short	read_ushort(FILE *fp, const int file_endian, const int machine_endian)
{
	t_union16 u;

	bzero(u.bytes, 2);
	fread(u.bytes, 1, 2, fp);
	if (file_endian != machine_endian)
	{
		uint8_t temp = u.bytes[0];
		u.bytes[0] = u.bytes[1];
		u.bytes[1] = temp;
	}
	return u.u_value;
}

int				read_int(FILE *fp, const int file_endian, const int machine_endian)
{
	t_union32 u;

	bzero(u.bytes, 4);
	fread(u.bytes, 1, 4, fp);
	if (file_endian != machine_endian)
	{
		uint8_t temp = u.bytes[0];
		u.bytes[0] = u.bytes[3];
		u.bytes[3] = temp;
		temp = u.bytes[1];
		u.bytes[1] = u.bytes[2];
		u.bytes[2] = temp;
	}
	return u.value;
}

unsigned int	read_uint(FILE *fp, const int file_endian, const int machine_endian)
{
	t_union32 u;

	bzero(u.bytes, 4);
	fread(u.bytes, 1, 4, fp);
	if (file_endian != machine_endian)
	{
		uint8_t temp = u.bytes[0];
		u.bytes[0] = u.bytes[3];
		u.bytes[3] = temp;
		temp = u.bytes[1];
		u.bytes[1] = u.bytes[2];
		u.bytes[2] = temp;
	}
	return u.u_value;
}

float			read_float(FILE *fp, const int file_endian, const int machine_endian)
{
	t_union32 u;

	bzero(u.bytes, 4);
	fread(u.bytes, 1, 4, fp);
	if (file_endian != machine_endian)
	{
		uint8_t temp = u.bytes[0];
		u.bytes[0] = u.bytes[3];
		u.bytes[3] = temp;
		temp = u.bytes[1];
		u.bytes[1] = u.bytes[2];
		u.bytes[2] = temp;
	}
	return u.f_value;
}

double			read_double(FILE *fp, const int file_endian, const int machine_endian)
{
	t_union64 u;

	bzero(u.bytes, 8);
	fread(u.bytes, 1, 8, fp);
	if (file_endian != machine_endian)
	{
		uint8_t temp = u.bytes[0];
		u.bytes[0] = u.bytes[7];
		u.bytes[7] = temp;
		temp = u.bytes[1];
		u.bytes[1] = u.bytes[6];
		u.bytes[6] = temp;
		temp = u.bytes[2];
		u.bytes[2] = u.bytes[5];
		u.bytes[5] = temp;
		temp = u.bytes[3];
		u.bytes[3] = u.bytes[4];
		u.bytes[4] = temp;
	}
	return u.value;
}
