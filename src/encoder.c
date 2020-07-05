#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"

#define max(a,b) ((a < b) ? b : a)
#define min(a,b) ((a < b) ? a : b)
#define square(a) (a*a)
#define rref(x) (row[x] & MASK)
#define diff(a,b) ((a & 1) == (b & 1) ? (a - b) : ((a & MASK) - (b & MASK)))


typedef char bool;

//a struct that contains information about an active region
struct region {
  bool is_active;
  int l;
  unsigned char* output;
  unsigned char color;
};

long avg_area_num = 0;
long avg_area_denom = 0;
long num_bad_regions = 0;

//the bitmask to apply to all values. The last bit must be unset, but it is possible to get better compression (and a more restricted color space) by making a smaller mask.
unsigned char MASK = 0xfe;

//checks that there is no active edge to the left of the given position
bool no_active_edge(int x, unsigned char this_color, struct region* regions) {
  for(int xo = 1; xo <= x; xo++) {
    if (regions[x-xo].is_active && (regions[x-xo].l == xo - 1) && (regions[x-xo].color == this_color)) {
      return 0;
    }
  }
  return 1;
}

//encodes one component of one row.
unsigned char* encode_row(struct region* regions, int rowlen, unsigned char* output, unsigned char* row, unsigned char* nextrow) {
  for(int x = 0; x < rowlen; x++) { //first we have to get rid of the low bit of every byte
    row[x] &= MASK;
  }
  if (nextrow != 0) {
    for(int x = 0; x < rowlen; x++) { //first we have to get rid of the low bit of every byte
      nextrow[x] &= MASK;
    }
  }
  for(int x = 0; x < rowlen;) {
    if (regions[x].is_active) { //if this region is active, we need to see if it continues here or not
      int i = x;
      int imax = min(rowlen,x+regions[x].l+2);
      for(;i < imax; i++) {
        if(row[i] != regions[x].color) {
          break;
        }
      }
      if (i == imax) {
        regions[x].l++;
        if(regions[i - 1].is_active && (i != x)) {
          fprintf(stderr,"nonzero-length region at %d, last color was %x, this color = %x, region color = %x, l = %d\n",x,(x > 0 ? row[x - 1] : 0),row[x],regions[i].color,regions[i].l);
          regions[i - 1].is_active = 0;
          *(regions[i - 1].output) = regions[i - 1].l | 0x80;
          avg_area_denom += square(regions[i - 1].l + 1)/2;
          avg_area_num += 2;
          if(regions[i - 1].l == 0) {
            //fprintf(stderr,"region %d was length zero when it died\n",x);
            num_bad_regions++;
          }
        }
        if(regions[x].l == 127) {
          regions[x].is_active = 0;
          *(regions[x].output) = 0xff;
          avg_area_denom += square(128)/2;
          avg_area_num += 2;
        }
        x = i;
      } else {
        fprintf(stderr,"nonzero-length region at %d, last color was %x, this color = %x, region color = %x, l = %d\n",x,(x > 0 ? row[x - 1] : 0),row[x],regions[x].color,regions[x].l);
        regions[x].is_active = 0;
        *(regions[x].output) = regions[x].l | 0x80;
        avg_area_denom += square(regions[x].l + 1)/2;
        avg_area_num += 2;
        if(regions[x].l == 0) {
          //fprintf(stderr,"region %d was length zero when it died\n",x);
          num_bad_regions++;
        }
        if((nextrow != 0) && (nextrow[x] == row[x]) && (nextrow[x < rowlen - 1 ? x + 1 : x] == row[x]) && no_active_edge(x,row[x],regions)) {
          //fprintf(stderr,"new region %d, color %d, color (x,y+1) = %d, color (x+1,y+1) = %d\n",x,row[x],nextrow[x],nextrow[x < rowlen - 1 ? x + 1 : x]);
          regions[x].is_active = 1;
          regions[x].output = output+1;
          *output = (row[x] - (x > 0 ? row[x-1] : 0)) | 0x1;
          *(output + 1) = 0;
          regions[x].l = 0;
          regions[x].color = row[x];
          output = output + 2;
        } else {
          *output = row[x] - (x > 0 ? row[x-1] : 0);
          fprintf(stderr,"zero-length region at %d, last color was %x, this color = %x, offset = %d\n",x,(x > 0 ? row[x - 1] : 0),row[x],*output);
          output = output + 1;
          avg_area_num ++;
          avg_area_denom ++;
        }
        x++;
      }
    } else {
      if ((nextrow != 0) && (nextrow[x] == row[x]) && (nextrow[x < rowlen - 1 ? x + 1 : x] == row[x]) && no_active_edge(x,row[x],regions)) {
        //fprintf(stderr,"new region %d, color %d, color (x,y+1) = %d, color (x+1,y+1) = %d\n",x,row[x],nextrow[x],nextrow[x < rowlen - 1 ? x + 1 : x]);
        regions[x].is_active = 1;
        regions[x].output = output+1;
        *output = (row[x] - (x > 0 ? row[x-1] : 0)) | 0x1;
        *(output + 1) = 0;
        regions[x].l = 0;
        regions[x].color = row[x];
        output = output + 2;
      } else {
        *output = row[x] - (x > 0 ? row[x-1] : 0);
        fprintf(stderr,"zero-length region at %d, last color was %x, this color = %x, offset = %d\n",x,(x > 0 ? row[x - 1] : 0),row[x],*output);
        output = output + 1;
        avg_area_num ++;
        avg_area_denom ++;
      }
      x++;
    }
  }
  return output;
}

//extract the red components
void extract_red(unsigned char* row_buf, unsigned char* row_ptr, int row_len) {
  for(int i = 0; i < row_len; i++) {
    row_ptr[i] = row_buf[3*i];
  }
}

//extract the green components
void extract_green(unsigned char* row_buf, unsigned char* row_ptr, int row_len) {
  for(int i = 0; i < row_len; i++) {
    row_ptr[i] = row_buf[3*i + 1];
  }
}

//extract the blue components
void extract_blue(unsigned char* row_buf, unsigned char* row_ptr, int row_len) {
  for(int i = 0; i < row_len; i++) {
    row_ptr[i] = row_buf[3*i + 2];
  }
}

//reads from a file and outputs to a file, calling encode_row repeatedly and feeding the results into a gzip file
//reads and writes only one row at a time, so it can work on streams and can easily be adapted to situations where that is key
void encode_stream(FILE* in, gzFile out, int rowlen) {
  struct region* red_regions = calloc(rowlen,sizeof(struct region));
  struct region* green_regions = calloc(rowlen,sizeof(struct region));
  struct region* blue_regions = calloc(rowlen,sizeof(struct region));
  unsigned char* output_buffer = calloc(rowlen*256*6,1); //an adversarial example keeps the output buffer locked for an entire 128 rows, so we need quite a lot of memory to guarantee no segfaults.
  unsigned char* output_cursor = output_buffer;
  int output_begin = 0;
  int output_len = 0;
  int output_end = rowlen*256*6;
  unsigned char* this_row = calloc(rowlen,1);
  unsigned char* next_row = calloc(rowlen,1);
  unsigned char* this_row_buf = calloc(rowlen*3,1);
  unsigned char* next_row_buf = calloc(rowlen*3,1);
  fprintf(stderr,"output buffer is size %d\n",rowlen*256*6);
  //read the first row into next_row to prime the whole process
  fread(next_row_buf,1,3*rowlen,in);
  int y = 0;
  while(feof(in) == 0) {
    memcpy(this_row_buf,next_row_buf,3*rowlen);
    fread(next_row_buf,1,3*rowlen,in);
    //encode the different components
    extract_red(this_row_buf,this_row,rowlen);
    extract_red(next_row_buf,next_row,rowlen);
    output_cursor = encode_row(red_regions,rowlen,output_cursor,this_row,next_row);
    extract_green(this_row_buf,this_row,rowlen);
    extract_green(next_row_buf,next_row,rowlen);
    output_cursor = encode_row(green_regions,rowlen,output_cursor,this_row,next_row);
    extract_blue(this_row_buf,this_row,rowlen);
    extract_blue(next_row_buf,next_row,rowlen);
    output_cursor = encode_row(blue_regions,rowlen,output_cursor,this_row,next_row);
    //if we can, input some data.
    fprintf(stderr,"after reading row %d, cursor is %d above buffer\n",y,output_cursor - output_buffer);
    while(output_begin + output_len < output_end) {
      if((output_buffer[output_begin + output_len] & 0x1) && (output_buffer[output_begin + output_len + 1] & 0x80)) {
        output_buffer[output_begin + output_len + 1] &= 0x7f;
        output_len += 2;
      } else if ((output_buffer[output_begin + output_len] & 0x1) == 0) {
        output_len ++;
      } else {
        break;
      }
    }
    fprintf(stderr,"write of size %d\n",output_len);
    if(output_len > 0) {
      gzwrite(out,output_buffer + output_begin,output_len);
      output_begin += output_len;
      output_len = 0;
      //reset output_begin to 0 if we have just finished reading the region left over after resetting the cursor
      if(output_begin == output_end) {
        fprintf(stderr,"resetting output_begin\n");
        output_begin = 0;
        output_end = rowlen*256*6;
      //reset the cursor to 0 so that there is always rowlen*128 space after the cursor
      //have to make sure this doesn't happen twice in a row, so we restrict to the case when cursor > output_buffer + output_begin
      } else if((output_cursor - output_buffer > output_begin) && (output_begin > rowlen*128*6)) {
        fprintf(stderr,"resetting cursor\n");
        output_end = output_cursor - output_buffer;
        output_cursor = output_buffer;
      }
      fprintf(stderr,"cursor at %d above buffer, output_begin at %d\n",output_cursor - output_buffer, output_begin);
    }
    y++;
  }
  //final row
  extract_red(next_row_buf,next_row,rowlen);
  output_cursor = encode_row(red_regions,rowlen,output_cursor,next_row,0);
  extract_green(next_row_buf,next_row,rowlen);
  output_cursor = encode_row(green_regions,rowlen,output_cursor,next_row,0);
  extract_blue(next_row_buf,next_row,rowlen);
  output_cursor = encode_row(blue_regions,rowlen,output_cursor,next_row,0);
  //finalize all regions that didn't get finalized
  for(int i = 0; i < rowlen; i++) {
    if(red_regions[i].is_active) {
      *(red_regions[i].output) = red_regions[i].l;
    }
    if(green_regions[i].is_active) {
      *(green_regions[i].output) = green_regions[i].l;
    }
    if(blue_regions[i].is_active) {
      *(blue_regions[i].output) = blue_regions[i].l;
    }
  }
  //now 100% of our output should be writeable
  output_len = output_cursor - (output_buffer + output_begin);
  fprintf(stderr,"final write of size %d\n",output_len);
  gzwrite(out,output_buffer + output_begin,output_len);
  gzclose(out);
  free(red_regions);
  free(green_regions);
  free(blue_regions);
  free(output_buffer);
  free(this_row);
  free(next_row);
  free(this_row_buf);
}

int main(int argc, char** argv) {
  FILE* in = stdin;
  gzFile out = gzdopen(fileno(stdout),"w");
  int rowlen = 0;
  if(argc < 2) {
    return 1;
  } else if (argc < 4) {
    rowlen = atoi(argv[1]);
  } else {
    rowlen = atoi(argv[1]);
    in = fopen(argv[2],"r");
    gzclose(out);
    out = gzopen(argv[3],"w");
  }
  gzputc(out,rowlen & 0xff); //output rowlen in little-endian
  gzputc(out,(rowlen >> 8) & 0xff);
  gzputc(out,(rowlen >> 16) & 0xff);
  gzputc(out,rowlen >> 24);
  encode_stream(in,out,rowlen);
  fclose(in);
  fprintf(stderr,"average bytes per pixel before compression: %f\n",(1.0*avg_area_num)/avg_area_denom);
  fprintf(stderr,"number of bad regions: %d\n",num_bad_regions);
}
