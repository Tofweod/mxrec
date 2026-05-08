#include "u8string.h"
#include <stdio.h>

int main(int argc, char **argv)
{
	u8s s1 = u8snew("hello");
	u8s s2 = u8snew("helloa");

	printf("%d\n", u8scmp(s1, s2));

	u8s s3 = u8snew("éèêë 你好 wot");
	printf("%s\n", s3);

	printf("%zu, %zu\n", u8slen(s3), u8sblen(s3));

	u8s ps = u8schr(s3, u8cpdecode("你"));
	printf("%s\n", ps);

	u8snormalize(&s3, U8S_NFD);
	printf("%s\n", s3);

	s3 = u8scatu8s(s3, s1);
	// UB
	//s3 = u8scatu8s(s3, s3);
	printf("%s\n", s3);

	char *argv1[] = {s1, s2, s3};
	u8s argv2[] = {s1, s2, s3};
	u8s s4 = u8sjoin(argv1, sizeof(argv1) / sizeof(argv1[0]), "#");
	u8s s5 = u8sjoinu8s(argv2, sizeof(argv1) / sizeof(argv1[0]), "#", 1);

	printf("%s\n",s4);
	printf("%s\n",s5);

	u8s s6 = u8snew(NULL);
	printf("%zu\n",u8sblen(s6));

	u8sfree(s1);
	u8sfree(s2);
	u8sfree(s3);
	u8sfree(s4);
	u8sfree(s5);
	u8sfree(s6);


	return 0;
}
