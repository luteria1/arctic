#include <math.h>
#include <stdio.h>
int logScaleBins(int size, int binNum, int *cutoffs){
	int tmp[binNum];
	
	//create scale
	for (int i=0;i<binNum;i++){
		tmp[i] = pow((double) size,((double) (i+1)/binNum));
	}
	
	//How many values in each bin
	cutoffs[0]=tmp[0];
	for (int i=1;i<binNum;i++){
		cutoffs[i]=tmp[i]-tmp[i-1];
	}

	//shift scale across by shifter;
	int shift=20;
	cutoffs[binNum-1]-=shift;
	for (int i=0;i<binNum-1;i++){
		cutoffs[i]+=20;
	}

	return 1;
}

