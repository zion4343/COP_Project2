#include <dirent.h> 
#include <stdio.h> 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>
#define BUFFER_SIZE 1048576

typedef struct {
	char *file_path;	//Full path to the image file
	int index;
}compression_task_t;

//original function (keeping it just in case)
// void *compress_image(void *arg){
// 	compression_task_t *task = (compression_task_t *)arg;
// 	pthread_exit(NULL);
// }

//this function might be unnecessary
void *compress_image(void *arg) {
	compression_task_t *task = (compression_task_t *)arg;

    // Ensure 'task' and 'task->file_path' are valid
    if (task == NULL || task->file_path == NULL) {
        fprintf(stderr, "Invalid task or file path.\n");
        pthread_exit(NULL);
    }
	
    unsigned char buffer_in[BUFFER_SIZE];
    unsigned char buffer_out[BUFFER_SIZE];
    int total_in = 0, total_out = 0;

    // load file
    FILE *f_in = fopen(task->file_path, "r");
    if (!f_in) {
        perror("Failed to open input file");
        pthread_exit(NULL);
    }
    int nbytes = fread(buffer_in, sizeof(unsigned char), BUFFER_SIZE, f_in);
    fclose(f_in);
    total_in += nbytes;

    // zip file
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int ret = deflateInit(&strm, Z_BEST_COMPRESSION);
    if (ret != Z_OK) {
        printf("Failed to initialize compression for file %s\n", task->file_path);
        pthread_exit(NULL);
    }
    strm.avail_in = nbytes;
    strm.next_in = buffer_in;
    strm.avail_out = BUFFER_SIZE;
    strm.next_out = buffer_out;

    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        printf("Failed to compress file %s\n", task->file_path);
        deflateEnd(&strm);
        pthread_exit(NULL);
    }
    deflateEnd(&strm);

    int nbytes_zipped = BUFFER_SIZE - strm.avail_out;
    total_out += nbytes_zipped;

    // print out compression info here
    // may need to output the compressed data in a thread-safe way?
    printf("Thread %p compressed %s: %d -> %d bytes\n", (void*)pthread_self(), task->file_path, total_in, total_out);

    // Clean up
    free(task->file_path);
    free(task);

    pthread_exit(NULL);
}

#define BUFFER_SIZE 1048576 // 1MB
#define MAX_THREADS 19		//Max worker threads, leaving one for the main thread

int cmp(const void *a, const void *b) {
	return strcmp(*(char **) a, *(char **) b);
}

int main(int argc, char **argv) {
	// time computation header
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	// end of time computation header

	// do not modify the main function before this point!

	assert(argc == 2);
	DIR *d;
	struct dirent *dir;
	char **files = NULL;
	int nfiles = 0;

	d = opendir(argv[1]);
	if (d == NULL) {
		printf("An error has occurred\n");
		return 0;
	}

	// create sorted list of PPM files
	while ((dir = readdir(d)) != NULL) {
		if (dir->d_name[0] == '.') continue; // Skip hidden files and directories (like . and ..)
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

	pthread_t threads[MAX_THREADS];
    compression_task_t **tasks = malloc(nfiles * sizeof(compression_task_t*));
	if (!tasks) {
    	perror("Failed to allocate memory for tasks");
    	exit(1);
	}

	int thread_count = 0;

	for (int i = 0; i < nfiles; i++) {
    	tasks[i] = malloc(sizeof(compression_task_t));
		tasks[i]->file_path = malloc(strlen(argv[1]) + strlen(files[i]) + 2); // +1 for '/' and +1 for '\0'
    	sprintf(tasks[i]->file_path, "%s/%s", argv[1], files[i]);
    	tasks[i]->index = i;

    	if (pthread_create(&threads[thread_count], NULL, compress_image, (void *)tasks[i])) {
        	perror("Failed to create thread");
			//not sure if we need to handle errors here
    	}

    	thread_count++;
    	if (thread_count >= MAX_THREADS || i == nfiles - 1) {
        	// Wait for threads to finish
       		for (int j = 0; j < thread_count; j++) {
        	    pthread_join(threads[j], NULL);
       		}
        	thread_count = 0; // Reset thread count
    	}
	}

	// create a single zipped package with all PPM files in lexicographical order
	int total_in = 0, total_out = 0;
	FILE *f_out = fopen("video.vzip", "wb");
	assert(f_out != NULL);
	for(int i=0; i < nfiles; i++) {
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

	// do not modify the main function after this point!

	// time computation footer
	clock_gettime(CLOCK_MONOTONIC, &end);
	printf("Time: %.2f seconds\n", ((double)end.tv_sec+1.0e-9*end.tv_nsec)-((double)start.tv_sec+1.0e-9*start.tv_nsec));
	// end of time computation footer

	return 0;
}
