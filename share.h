#include<pthread.h>
#include<fftw3.h>

#define BUFFER_SIZE 512;
struct audio_data {
	double *arctic;
	int arctic_buffer_size;
	unsigned int rate,channels;
	int format;
	char *source;
	int terminate;
	int samplesCounter;
	pthread_mutex_t lock;
};

struct main_data {
	double in_l,in_r;
	fftw_complex out_l,out_r;



};
