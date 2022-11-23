#include <syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
 #include <stdlib.h>
#include <dlfcn.h>

int init_data = 30;
int non_init_data;
long ret = 0;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

void* our_func()
{
	pthread_mutex_lock( &mutex1 );
	int local_data = 30;
 	int* heapAddress = malloc(sizeof(int));
	void *fHandle;
	void *func;
	fHandle = dlopen("/lib/x86_64-linux-gnu/libc.so.6",RTLD_LAZY);
	printf("libc base: %lx\n",*(unsigned long*)fHandle);
	if (!fHandle) {
		printf ("%s\n", dlerror());
		exit(1);
	}
	func = dlsym(fHandle, "printf");
	static __thread int tls = 0;
	if(dlerror() != NULL){
		printf("%s\n", dlerror());
		exit(1);
	}
	size_t* va[7] = {
		(size_t*)&init_data,
		(size_t*)&non_init_data,
		(size_t*)&our_func,
		(size_t*)&local_data,
		(size_t*)func,
		(size_t*)heapAddress,
		(size_t*)&tls
	};
	size_t* pa[7];
	ret = syscall(333, va, pa, 7);
	printf("\nPid of the Thread is = %p, \nPid of the process is = %d\nAddresses info:", (size_t*)pthread_self(), getpid());
	char printArray [7][20] = {"Data", "BSS", "Code", "Stack", 
				   "Library", "heap", "thread"};
	for(int i=0;i<7;i++){
		printf("\n %d) %8s (va/pa) = %16p / %20p", i+1, printArray[i], va[i], pa[i]);
	}
	tls++;
	printf("\n\n");
	dlclose(fHandle);
	pthread_mutex_unlock( &mutex1 );
	pthread_exit(NULL);	

}

int main(){

	int local_data = 30;
	int* heapAddress = malloc(sizeof(int));
	char test = '\n';
	printf("start! %c\n", test);
	void* fHandle;
	void* func;
	fHandle = dlopen("/lib/x86_64-linux-gnu/libc.so.6",RTLD_LAZY);
	printf("libc base: %lx\n",*(size_t*)fHandle);
	if (!fHandle) {
		printf ("%s\n", dlerror());
		exit(1);
	}
	func = dlsym(fHandle, "printf");
	static __thread int tls = 0;
	if(dlerror() != NULL){
		printf("%s\n", dlerror());
		exit(1);
	}
	size_t* va[7] = {
		(size_t*)&init_data,
		(size_t*)&non_init_data,
		(size_t*)&our_func,
		(size_t*)&local_data,
		(size_t*)func,
		(size_t*)heapAddress,
		(size_t*)&tls
	};
	size_t* pa[7];
	ret = syscall(333, va, pa, 7);
	printf("Main thread\n");
	printf("\nPid of the Thread is = %p, \nPid of the process is = %d\nAddresses info:", (size_t*)pthread_self(), getpid());
	char printArray [7][20] = {"Data", "BSS", "Code", "Stack", 
				   "Library", "heap", "thread"};
	for(int i=0;i<7;i++){
		printf("\n %d) %8s (va/pa) = %16p / %20p", i+1, printArray[i], va[i], pa[i]);
	}
	tls++;
	printf("\n\n");
	pthread_t threads[3];
	dlclose(fHandle);
	for (int i = 0; i < 2; i++) {
		pthread_create(&threads[i], NULL, our_func, NULL);
	}
	sleep(2);
	printf("\n");
	for(int i=0;i<3;i++){
		long size = (long)&threads[i+1]-(long)&threads[i];
		printf("threads[%d]:\nsize:%d\nstart address:%p\nend address:%p\n\n", i, (int)size, &threads[i], (size_t*)((long)&threads[i]+size-1));
	}
	for (int i = 0; i < 2; i++) {
		pthread_join(threads[i], NULL);
	}

	if(ret > 0) printf("error!!!QQQQQQQ");

	return 0;
}
