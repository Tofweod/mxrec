#include "u8string.h"
#include <stdio.h>

int main(int argc, char **argv)
{
	u8s s1 = u8snew("hello");
	u8s s2 = u8snew("helloa");

	printf("%d\n",u8scmp(s1,s2));


	u8s s3 = u8snew("éèêë 你好 "); 
	printf("%s\n",s3);

	printf("%zu, %zu\n",u8slen(s3),u8sblen(s3));

	u8s ps = u8schr(s3,u8cpdecode("你"));
	printf("%s\n",ps);


	u8snormalize(&s3,U8S_NFD);
	printf("%s\n",s3);

	u8sfree(s1);
	u8sfree(s2);
	u8sfree(s3);

	return 0;
}
