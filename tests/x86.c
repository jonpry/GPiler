#include <stdio.h>
#include <stdlib.h>

void mapit(unsigned idx, unsigned *out, unsigned* in);
void echo(unsigned i);

int main(){
	unsigned in=2,out=0;
//	echo(2);
	mapit(0,&out,&in);
	printf("%d %d\n", in, out);
}
