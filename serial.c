#include <dirent.h> 
#include <stdio.h> 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1048576 // 1MB
#define MAX_THREADS 20

/*
Class
*/
//Zemaphores
typedef struct __Zem_t{
	int value;
	pthread_cond_t cond;
	pthread_mutex_t lock;
}Zem_t;

void Zem_init(Zem_t *s, int value){
	s->value = value;
	int rc = pthread_cond_init(&s->cond, NULL);
	assert (rc == 0);
	rc = pthread_mutex_init(&s->lock, NULL);
	assert (rc == 0);
}

void Zem_wait(Zem_t *s){
	pthread_mutex_lock(&s->lock);
	while(s->value <= 0){pthread_cond_wait(&s->cond, &s->lock);}
	s->value--;
	pthread_mutex_unlock(&s->lock);
}

void Zem_post(Zem_t *s){
	pthread_mutex_lock(&s->lock);
	s->value++;
	pthread_cond_signal(&s->cond);
	pthread_mutex_unlock(&s->lock);
}

//Read-Write Lock using Zemaphore
typedef struct _rwlock_t{
	Zem_t lock; //Basic Lock
	Zem_t writelock; //Used to allow one writer or many readers
	int readers; //count the readers in critical section
}rwlock_t;

void rwlock_init(rwlock_t *rw){
	rw->readers = 0;
	Zem_init(&rw->lock, 1);
	Zem_init(&rw->writelock, 1);
}

void rwlock_aquire_readlock(rwlock_t *rw){
	Zem_wait(&rw->lock);
	rw->readers++;
	if(rw->readers == 1){Zem_wait(&rw->writelock);}
	Zem_post(&rw->lock);
}

void rwlock_release_readlock(rwlock_t *rw){
	Zem_wait(&rw->lock);
	rw->readers--;
	if(rw->readers == 0){Zem_post(&rw->writelock);}
	Zem_post(&rw->lock);
}

void rwlock_acquire_writelock(rwlock_t *rw){
	Zem_wait(&rw->writelock);
}

void rwlock_release_writelock(rwlock_t *rw){
	Zem_post(&rw->writelock);
}

/*
Functions
*/
int cmp(const void *a, const void *b) {
	return strcmp(*(char **) a, *(char **) b);
}

/*
Shared Variables
*/
DIR *d;
struct dirent *dir;
char **files = NULL;
int nfiles = 0;

rwlock_t rw;
int thread_count = 0;

/*
Threads
*/
void *thread_function(void *arg){
	int *thread_arg = (int*) arg;
	
	pthread_exit(NULL);
}



/*
Main Function
*/
int main(int argc, char **argv) {
	// time computation header
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	// end of time computation header

	// do not modify the main function before this point!
	assert(argc == 2);

	//Initialize Read-Write Lock
	rwlock_init(&rw);

	int thread_count = 0; //The counter that store the number of the thread

	d = opendir(argv[1]);
	if(d == NULL) {
		printf("An error has occurred\n");
		return 0;
	}

	// create sorted list of PPM files
	while ((dir = readdir(d)) != NULL) {
		files = realloc(files, (nfiles+1)*sizeof(char *));
		assert(files != NULL);

		int len = strlen(dir->d_name);
		if(dir->d_name[len-4] == '.' && dir->d_name[len-3] == 'p' && dir->d_name[len-2] == 'p' && dir->d_name[len-1] == 'm') {
			files[nfiles] = strdup(dir->d_name);
			assert(files[nfiles] != NULL);

			nfiles++;
		}
	}
	closedir(d);
	qsort(files, nfiles, sizeof(char *), cmp);

	// create a single zipped package with all PPM files in lexicographical order
	int total_in = 0, total_out = 0;
	FILE *f_out = fopen("video.vzip", "w");
	assert(f_out != NULL);

///////////////////////////
	pthread_t threads[MAX_THREADS];
	int num_active_threads = 0;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

	for(int i=0; i < nfiles; i++) {
		pthread_mutex_lock(&mutex);
		while (num_active_threads >= MAX_THREADS){
			pthread_cond_wait(&cond, &mutex);
		}
		num_active_threads++;
		pthread_mutex_unlock(&mutex);
/////////////////////////
		int len = strlen(argv[1])+strlen(files[i])+2;
		char *full_path = malloc(len*sizeof(char));
		assert(full_path != NULL);
		strcpy(full_path, argv[1]);
		strcat(full_path, "/");
		strcat(full_path, files[i]);

		unsigned char buffer_in[BUFFER_SIZE];
		unsigned char buffer_out[BUFFER_SIZE];

		// load file
		FILE *f_in = fopen(full_path, "r");
		assert(f_in != NULL);
		int nbytes = fread(buffer_in, sizeof(unsigned char), BUFFER_SIZE, f_in);
		fclose(f_in);
		total_in += nbytes;

		// zip file
		z_stream strm;
		int ret = deflateInit(&strm, 9);
		assert(ret == Z_OK);
		strm.avail_in = nbytes;
		strm.next_in = buffer_in;
		strm.avail_out = BUFFER_SIZE;
		strm.next_out = buffer_out;

		ret = deflate(&strm, Z_FINISH);
		assert(ret == Z_STREAM_END);

		// dump zipped file
		int nbytes_zipped = BUFFER_SIZE-strm.avail_out;
		fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
		fwrite(buffer_out, sizeof(unsigned char), nbytes_zipped, f_out);
		total_out += nbytes_zipped;

		free(full_path);
	}
	fclose(f_out);

	printf("Compression rate: %.2lf%%\n", 100.0*(total_in-total_out)/total_in);

	// release list of files
	for(int i=0; i < nfiles; i++)
		free(files[i]);
	free(files);

	//wait for threads to finish
	for (int i = 0; i < thread_count; i++){
		pthread_join(threads[i], NULL);
	}

	// do not modify the main function after this point!

	// time computation footer
	clock_gettime(CLOCK_MONOTONIC, &end);
	printf("Time: %.2f seconds\n", ((double)end.tv_sec+1.0e-9*end.tv_nsec)-((double)start.tv_sec+1.0e-9*start.tv_nsec));
	// end of time computation footer

	return 0;
}
