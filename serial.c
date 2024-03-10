#include <dirent.h> 
#include <stdio.h> 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1048576 // 1MB
#define MAX_THREAD 20

int cmp(const void *a, const void *b) {
	return strcmp(*(char **) a, *(char **) b);
}

//concurrent queue is used to enqueue and dequeue filenames, and the concurrent hashmap is used to track processed files
//each thread processes file from the queue
//checks if the file has been processed using the hashmap

//node strycture for the concurrent queue
typedef struct __node_t{
	int value;
	struct __node_t *next;
}node_t;

//concurrent queue structure
typedef struct __queue_t{
	node_t *head;
	node_t *tail;
	pthread_mutex_t head_lock, tail_lock;
}queue_t;

//initialize the concurrent queue
void Queue_Init(queue_t *q){
	node_t *tmp = malloc(sizeof(node_t));
	tmp->next = NULL;
	q->head = q->tail = tmp;
	pthread_mutex_init(&q->head_lock, NULL);
	pthread_mutex_init(&q->tail_lock, NULL);
}

void Queue_Enqueue(queue_t *q, int value){
	node_t *tmp = malloc(sizeof(node_t));
	assert(tmp != NULL);
	tmp->value = value;
	tmp->next = NULL;

	pthread_mutex_lock(&q->tail_lock);
	q->tail->next = tmp;
	q->tail = tmp;
	pthread_mutex_unlock(&q->tail_lock);
}

void Queue_Dequeue(queue_t *q, int *value){
	pthread_mutex_lock(&q->head_lock);
	node_t *tmp = q->head;
	node_t *new_head = tmp->next;
	if (new_head == NULL){
		pthread_mutex_unlock(&q->head_lock);
		return;  //queue was empty
	}
	*value = new_head->value;
	q->head = new_head;
	pthread_mutex_unlock(&q->head_lock);
	free(tmp);
}

//list structure for the hash table
typedef struct __list_t{
	int key;
	struct __list_t *next;
	pthread_mutex_t lock;
}list_t;

//hash table structure
typedef struct{
	list_t lists[MAX_THREAD];
}hash_t;

//initialize hash table
void Hash_Init(hash_t *H){
	int i;
	for (i = 0; i < MAX_THREAD; i++){
		H->lists[i].next = NULL;
		pthread_mutex_init(&H->lists[i].lock, NULL);
	}
}

int Hash_Insert(hash_t *H, int key){
	list_t *new = malloc(sizeof(list_t));
	if (new == NULL){
		perror("malloc");
		return -1;
	}
	new->key = key;
	new->next = NULL;
	//lock critical section
	pthread_mutex_lock(&H->lists[key%MAX_THREAD].lock);
	new->next = H->lists[key%MAX_THREAD].next;
	H->lists[key%MAX_THREAD].next = new;
	pthread_mutex_unlock(&H->lists[key%MAX_THREAD].lock);
	return 0;	//success
}

int Hash_Lookup(hash_t *H, int key){
	pthread_mutex_lock(&H->lists[key % MAX_THREAD].lock);
	list_t *curr = H->lists[key % MAX_THREAD].next;
	while (curr != NULL){
		if (curr->key == key){
			pthread_mutex_unlock(&H->lists[key%MAX_THREAD].lock);
			return 0;
		}
		curr = curr->next;
	}
	pthread_mutex_unlock(&H->lists[key % MAX_THREAD].lock);
	return -1;	//not found  
}

void* process_files (void* arg){
	queue_t* queue = (queue_t*)arg;
	hash_t* hashMap = (hash_t*)arg;

	while (1){
		int tmp;
		Queue_Dequeue(queue, &tmp);
		if (tmp == -1){
			break;
		}

		pthread_mutex_lock(&hashMap->lists[tmp % MAX_THREAD].lock);
		Hash_Insert(hashMap, tmp);
		pthread_mutex_unlock(&hashMap->lists[tmp % MAX_THREAD].lock);
	}
	return NULL;
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
	if(d == NULL) {
		printf("An error has occurred\n");
		return 0;
	}

	queue_t queue;
	Queue_Init(&queue);

	hash_t hashMap;
	Hash_Init(&hashMap);

	for (int i = 0; i < MAX_THREAD; i++){
		Queue_Enqueue(&queue, i);
	}

	//create threads
	pthread_t threads[MAX_THREAD];
	for (int i = 0; i < MAX_THREAD; i++){
		pthread_create(&threads[i], NULL, process_files, &queue);
	}

	//wait for threads to finish
	for (int i = 0; i < MAX_THREAD; i++){
		pthread_join(threads[i], NULL);
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
