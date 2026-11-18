#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>

/* Host-only PNG pipeline input. Firmware calls rr_render() directly. */
int main(int argc,char **argv) {
    if(argc!=4){fprintf(stderr,"usage: render_host MODE MAP OUTPUT.ppm\n");return 2;}
    int mode=atoi(argv[1]),map=atoi(argv[2]);
    if(mode<0||mode>1||map<0||map>2)return 2;
    uint16_t *frame=calloc(360*360,sizeof *frame);if(!frame)return 1;
    rr_state state=rr_reference_state(mode,map);rr_render(frame,&state);
    FILE *out=fopen(argv[3],"wb");if(!out){free(frame);return 1;}
    fprintf(out,"P6\n360 360\n255\n");
    for(int i=0;i<360*360;i++) {
        uint16_t v=frame[i];unsigned char p[3]={(unsigned char)(((v>>11)&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};
        if(fwrite(p,1,3,out)!=3){fclose(out);free(frame);return 1;}
    }
    int failed=fclose(out);free(frame);return failed?1:0;
}
