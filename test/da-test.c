#include "assert.h"
#include "xmalloc.h"
#include "da.h"
#include <stdio.h>

int main(){
	int *arr = NULL;

	da_init(arr,sizeof(arr[0]));

	da_append(arr,1);
	da_append(arr,2);
	da_append(arr,3);
	da_append(arr,4);
	da_append(arr,3);
	da_append(arr,2);
	da_append(arr,1);

	printf("%lu\n",da_len(arr));

	for(size_t i = 0; i < da_len(arr);++i) {
		printf("%d\n",arr[i]);
	}

	da_free(arr);
	return 0;
}
